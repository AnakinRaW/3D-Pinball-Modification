# Error handling

The firmware handles errors on two levels. A device is the hardware a driver works, such as a sensor, the solenoids or the SD card. When a device stops working, its driver notices it, the fault is reported, and the machine goes on running without that device. When the firmware itself hangs, or would harm the hardware by keeping a coil on, the watchdog restarts the Teensy.

## Device faults

The device monitor is a firmware component that attaches to the [driver tick](driver-design.md#driver-intervaltimer) with a period of 100 ms and reports every fault. Each driver notes when its device parts last worked and counts the errors it notices. A part counts as failed once it has been out of order for longer than the driver allows, or has failed as often as the driver allows. A driver that counts time uses [`elapsedMs()`](general-design.md#time-measurement) to measure for how long the device part is inoperative.

Drivers register to the monitor in their `begin()` method. On every tick the device monitor calls `Fault failed()` of every registered driver. The driver returns a `Fault`, which holds a bitmask with one bit per failed part and the driver's error code. The bits count from the LSB, and the error code is 0 for a driver without error codes.

For every part whose bit error status, the monitor publishes `DeviceFailed` or `DeviceRecovered` into its event queue. The event's source packs the device and the part. The device is an entry of [`Device`](driver-design.md#devices). The payload of `DeviceFailed` is the driver's error code, and `DeviceRecovered` has no payload. When the queue is full, the part keeps the state it was last reported in, and the next check tries again.

```cpp
// the source of a fault event: the device in the upper four bits and the part in the lower
// four, so a device has at most 16 parts
constexpr uint8_t faultSource(Device d, uint8_t part);
constexpr Device  faultDevice(uint8_t source);
constexpr uint8_t faultPart(uint8_t source);

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
    void set(Device d, uint8_t part, uint32_t code);

    void     clear(Device d, uint8_t part);           // marks a part as working again
    bool     isFailed(Device d, uint8_t part) const;  // gets whether a part has failed
    bool     anyFailed(Device d) const;               // gets whether any part of a device has failed
    uint32_t code(Device d) const;                    // gets the error code a device last failed with

    // calls fn(device, part) for every failed part, device by device
    template <typename Fn> void forEach(Fn fn) const;
};

// monitors the devices of the registered drivers and reports every part that fails or recovers
class DeviceMonitor {
public:
    // registers a driver, whose failed() every check asks; before the main loop only, and the first call
    // attaches the queue and the check to the driver tick. False when every slot is taken
    bool watch(Device device, const Driver& driver);
};

extern DeviceMonitor deviceMonitor;
```

## Watchdog

The watchdog is a hardware timer in the i.MX RT that resets the Teensy unless the firmware feeds it within a specified time. It keeps counting when the firmware has crashed, so it also catches a failure that stops every line of code. The watchdog is to be used to guard against major malfunctions which are not recoverable, critical to the system or would harm hardware.

The watchdog time can be only as long as the shortest guard it has to ensure.

The main loop feeds it once per pass, and only while every condition it should guard holds. Feeding the watchdog belongs in the main loop because interrupts keep running while the loop hangs, and a watchdog fed from one would never fire.

The watchdog starts first. Until the main loop runs, FreeRTOS's idle task feeds it under the same conditions.

The following components are guarded by the watchdog:

| Component | Condition for feeding | Shut-off Time Constraint |
|---|---|---|
| Solenoid Driver | no coil has been on for longer than 51 ms, which `overdue()` checks, see [`solenoid.md`](drivers/solenoid.md) | ≤ 5 s |

```cpp
#include "Watchdog_t4.h"         // the WDT_T4 library, github.com/tonton81/WDT_T4

// the i.MX RT's WDOG1, which restarts the Teensy unless it is fed within 2 s
class Watchdog {
public:
    // starts the watchdog and its warning; the main task calls it first, right before the
    // solenoids' begin(), so it guards the coils from their first pull
    void begin();

    // feeds the watchdog while every guard holds, and notes the cause where one fails; at the end
    // of every pass of the main loop, and from FreeRTOS's idle task until the main loop runs
    void check();

    // writes the report of the last restart into the log; the main task, right after the logger's begin()
    void report(Logger& logger);
};

extern Watchdog watchdog;
```

### Restart causes

After every restart other than a power-up, the log on the card states its cause. PJRC's `CrashReport` tells which reset it was, such as the watchdog or a crash, and where a crash happened. Before the watchdog restarts the Teensy, the firmware notes in `CrashReport` why it went unfed.

| Cause | Note |
|---|---|
| A coil stays on too long | the coils |
| The firmware hangs | the address of the running code |

Right after the logger's `begin()`, the main task writes the report into the log.
