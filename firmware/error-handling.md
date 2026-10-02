# Error handling

The firmware handles errors on two levels. A device is the hardware a driver works, such as a sensor, the bumper contacts or the SD card. When a device stops working, its driver notices it, the fault is reported, and the machine goes on running without that device. When the firmware itself hangs, or would harm the hardware by keeping a coil on, the watchdog restarts the Teensy.

## Device faults

The device monitor is a firmware component that attaches to the [driver tick](driver-design.md#driver-intervaltimer) with a period of 100 ms and reports every fault. Each driver notes when its device parts last worked and counts the errors it notices. A part counts as failed once it has been out of order for longer than the driver allows. The driver uses [`elapsedMs()`](general-design.md#time-measurement) to measure for how long the device part is inoperative.

Drivers register to the monitor in their `begin()` method. On every tick the device monitor calls `Fault failed()` of every registered driver. The driver returns a `Fault` with a bitmask of one bit per failed part, counted from LSB, and its error code, 0 for a driver without error codes. 

For every changed bit, that is for every device part, the monitor publishes `DeviceFailed` or `DeviceRecovered` into its event queue. The source packs the device, an entry of [`Device`](driver-design.md#devices), and the part. The payload of `DeviceFailed` is the driver's error code, and `DeviceRecovered` has no payload. 

```cpp
// the source of a fault event: the device in the upper four bits and the part in the lower
// four, so a device has at most 16 parts
constexpr uint8_t faultSource(Device d, uint8_t part) { return (uint8_t)((uint8_t)d << 4 | part); }
constexpr Device  faultDevice(uint8_t source)         { return (Device)(source >> 4); }
constexpr uint8_t faultPart(uint8_t source)           { return source & 0x0F; }

// the parts of one device as a set, one bit per part, bit n for part n, started from LSB
using PartMask = uint16_t;

// what a driver's failed() returns
struct Fault {
    PartMask parts;   // one bit per failed part
    uint32_t code;    // the driver's error code, 0 for a driver without error codes
};

// records the failed parts of every device and the error code each device last failed with
class FailedDevices {
public:
    // marks a part as failed and keeps the error code it failed with
    void set(Device d, uint8_t part, uint32_t code) { parts_[(size_t)d] |= 1u << part; codes_[(size_t)d] = code; }

    void     clear(Device d, uint8_t part)          { parts_[(size_t)d] &= ~(1u << part); }      // marks a part as working again
    bool     isFailed(Device d, uint8_t part) const { return parts_[(size_t)d] & (1u << part); } // gets whether a part has failed
    bool     anyFailed(Device d) const              { return parts_[(size_t)d] != 0; }           // gets whether any part of a device has failed
    uint32_t code(Device d) const                   { return codes_[(size_t)d]; }                // gets the error code a device last failed with

    // calls fn(device, part) for every failed part, device by device
    template <typename Fn> void forEach(Fn fn) const {
        for (size_t d = 0; d < (size_t)Device::Count; ++d)
            for (PartMask left = parts_[d]; left != 0; left &= left - 1)   // drops the lowest set bit each round
                fn((Device)d, (uint8_t)__builtin_ctz(left));                // the number of that bit is the part
    }

private:
    PartMask parts_[(size_t)Device::Count] = {};   // one bit per part
    uint32_t codes_[(size_t)Device::Count] = {};   // the error code of each device's last failure
};

// monitors the devices of the registered drivers and reports every part that fails or recovers
class DeviceMonitor {
public:
    // registers a driver, whose failed() every check asks; setup() only, and the first call
    // attaches the queue and the check to the driver tick. False when every slot is taken
    bool watch(Device device, const Driver& driver) {
        if (count_ == kSlots) return false;
        if (count_ == 0) {
            out_ = &events.attach(queue_, kDepth);
            if (!driverTick.attach([this] { check(); }, kCheckUs)) return false;
        }
        noInterrupts();                               // the check must not see a half-written slot
        slots_[count_] = Slot{device, &driver, 0};
        count_ = count_ + 1;
        interrupts();
        return true;
    }

private:
    static constexpr uint8_t  kSlots   = 8;
    static constexpr size_t   kDepth   = 32;          // a converter that stops answering fails eight parts at once
    static constexpr uint32_t kCheckUs = 100000;      // 100 ms from one check to the next

    struct Slot { Device device; const Driver* driver; PartMask reported; };

    // the driver tick, every kCheckUs: publishes every part whose bit changed; a full
    // queue leaves the part as reported, so the next check tries again
    void check() {
        for (uint8_t i = 0; i < count_; ++i) {
            Slot&       s = slots_[i];
            const Fault f = s.driver->failed();
            for (PartMask left = f.parts ^ s.reported; left != 0; left &= left - 1) {
                const uint8_t  part = __builtin_ctz(left);   // the lowest part that changed
                const PartMask bit  = 1u << part;
                const auto     type = (s.reported & bit) ? DriverEventType::DeviceRecovered
                                                         : DriverEventType::DeviceFailed;
                const uint32_t code = type == DriverEventType::DeviceFailed ? f.code : 0;
                if (out_->publish(DriverEvent{micros(), type, faultSource(s.device, part), code})) s.reported ^= bit;
            }
        }
    }

    DriverEvent           queue_[kDepth];
    EventQueue::Producer* out_ = nullptr;
    Slot                  slots_[kSlots];
    volatile uint8_t      count_ = 0;
};

extern DeviceMonitor deviceMonitor;
```

## The watchdog

The watchdog is a hardware timer in the i.MX RT that resets the Teensy unless the firmware feeds it within a specified time. It keeps counting when the firmware has crashed, so it also catches a failure that stops every line of code. The watchdog is to be used to guard against major malfunctions which are not recoverable, critical to the system or would harm hardware.

The watchdog time can be only as long as the shortest guard it has to ensure.

The main loop feeds it once per pass, and only while every condition it should guard holds. Feeding the watchdog belongs in the main loop because interrupts keep running while the loop hangs, and a watchdog fed from one would never fire.

The following components are guarded by the watchdog:

| Component | Condition for feeding | Shut-off Time Constraint |
|---|---|---|
| Bumper Driver | no coil has been on for longer than 51 ms, which `overdue()` checks, see [`bumper.md`](drivers/bumper.md) | ≤ 5 s |

```cpp
#include "Watchdog_t4.h"         // the WDT_T4 library, github.com/tonton81/WDT_T4

WDT_T4<WDT1> watchdog;           // the i.MX RT's WDOG1

void setup() {
    // ... every driver's begin() first, so a slow start-up cannot reset the Teensy
    WDT_timings_t config;
    config.timeout = 2;          // in seconds
    watchdog.begin(config);
}

void loop() {
    // ... the rest of the pass
    if (!bumpers.overdue()) watchdog.feed();   // each guarded component adds its condition
}
```
