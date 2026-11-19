// The device monitor of docs/firmware/error-handling.md. Every 100 ms on the driver tick it asks
// each registered driver which parts of its device have failed, and publishes DEVICE_FAILED or
// DEVICE_RECOVERED into its queue of the event queue for every part whose answer changed.
//
// Device names the devices of docs/firmware/driver-design.md. deviceMonitor is the one
// DeviceMonitor of the firmware, on events and driverTick.
#pragma once
#include <Arduino.h>
#include <driver-base.h>
#include <event-queue.h>

// the devices of the machine; a device of one part has only part 0
enum class Device : uint8_t {
    ROTARY_SENSOR,
    BREAK_BEAM,
    SOLENOIDS,
    IR_SENSING,
    SD_CARD,
    DISPLAY_MODULE,   // DISPLAY is a macro of the Teensy core
    TOUCH,
    SERVO,
    AMPLIFIER,
    LED_CHAINS,
    TOGGLE_SWITCH,
    CONTACTS,
    COUNT,   // the number of devices
};

// The source of a fault event packs the device into the upper four bits and the part into the
// lower four, so a device has at most 16 parts.
constexpr uint8_t PART_BITS = 4;
constexpr uint8_t PART_MASK = (1u << PART_BITS) - 1;
static_assert(uint8_t(Device::COUNT) <= (1u << PART_BITS), "every device fits the upper four bits");
static_assert(sizeof(PartMask) * 8 == (1u << PART_BITS), "a PartMask holds every part of a device");

constexpr uint8_t faultSource(Device d, uint8_t part) {
    return uint8_t(uint8_t(d) << PART_BITS | (part & PART_MASK));
}
constexpr Device faultDevice(uint8_t source) {
    return Device(source >> PART_BITS);
}
constexpr uint8_t faultPart(uint8_t source) {
    return source & PART_MASK;
}

// records the failed parts of every device and the error code each device last failed with
class FailedDevices {
public:
    // Marks a part as failed and keeps the error code it failed with.
    void set(Device d, uint8_t part, uint32_t code) {
        parts[size_t(d)] |= PartMask(1u << part);
        codes[size_t(d)] = code;
    }

    // Marks a part as working again.
    void clear(Device d, uint8_t part) { parts[size_t(d)] &= PartMask(~(1u << part)); }

    // Gets whether a part has failed.
    bool isFailed(Device d, uint8_t part) const { return (parts[size_t(d)] & (1u << part)) != 0; }

    // Gets whether any part of a device has failed.
    bool anyFailed(Device d) const { return parts[size_t(d)] != 0; }

    // Gets the error code a device last failed with.
    uint32_t code(Device d) const { return codes[size_t(d)]; }

    // Calls fn(device, part) for every failed part, device by device.
    template <typename Fn> void forEach(Fn fn) const {
        for (size_t d = 0; d < size_t(Device::COUNT); ++d) {
            // Each round drops the lowest set bit, whose number is the part.
            for (uint32_t left = parts[d]; left != 0; left &= left - 1) {
                fn(Device(d), uint8_t(__builtin_ctz(left)));
            }
        }
    }

private:
    PartMask parts[size_t(Device::COUNT)] = {};
    uint32_t codes[size_t(Device::COUNT)] = {};
};

class DeviceMonitor {
public:
    // model/firmware/figures.py, n_monitor_slots: one slot for each device, and a device that
    // reports nothing leaves its slot empty
    static constexpr uint8_t SLOTS = uint8_t(Device::COUNT);
    // model/firmware/figures.py, n_queue: eight parts failing at once with room to spare
    static constexpr uint32_t DEPTH = 32;
    static constexpr uint32_t CHECK_US = 100000;   // model/firmware/figures.py, t_monitor

    // Takes the queue's events from storage, which watch() attaches to queue on its first call.
    DeviceMonitor(EventQueue& queue, DriverTick& tick, DriverEvent (&storage)[DEPTH])
        : queue(queue), tick(tick), storage(storage) {}

    // Registers a driver, whose failed() every check asks. Before the main loop only. The first
    // call attaches the monitor's queue to the event queue and its check to the driver tick.
    // Returns false when every slot is taken or the driver tick refuses the check.
    bool watch(Device device, const Driver& driver);

private:
    struct Slot {
        Device device;
        const Driver* driver;
        PartMask reported;   // the failed parts as last published
    };

    // The driver tick, every CHECK_US: publishes every part whose bit changed. A full queue
    // leaves the part as it was reported, so the next check tries again.
    void check();

    EventQueue& queue;
    DriverTick& tick;
    DriverEvent* const storage;
    EventQueue::Producer* out = nullptr;
    bool checking = false;   // the driver tick took the check
    Slot slots[SLOTS] = {};
    volatile uint8_t count = 0;
};

extern DeviceMonitor deviceMonitor;
