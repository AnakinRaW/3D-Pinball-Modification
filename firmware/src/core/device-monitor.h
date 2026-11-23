// Watches the drivers for failed parts and tells the main loop when a part fails or works again.
// Every 100 ms the driver tick lets it ask each registered driver which parts of its device have
// failed. For every part whose answer changed, it publishes DEVICE_FAILED or DEVICE_RECOVERED into
// the event queue. deviceMonitor is the firmware's one monitor.
//
// Design: docs/firmware/error-handling.md, Device faults.
#pragma once
#include <Arduino.h>
#include "drivers/driver-base.h"
#include "core/event-queue.h"

/// The devices of the machine. A device of one part has only part 0.
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

/// The source of a fault event packs the device into the upper four bits and the part into the
/// lower four, so a device has at most 16 parts.
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

/// Gets a device's name in words, such as "SD card", for the log.
///
/// @return  "device" for a value past the last device.
const char* deviceName(Device d);

/// Writes the log line of a fault event: a warning for DEVICE_FAILED with the device, the part and
/// the error code, and a line of the level INFO for DEVICE_RECOVERED.
///
/// @param e  A driver event of any type. One of another type writes nothing.
/// @return   Whether @p e was DEVICE_FAILED or DEVICE_RECOVERED.
/// @note     The host calls it from the main loop for every driver event.
bool logFault(const DriverEvent& e);

/// Records the failed parts of every device and the error code each device last failed with.
class FailedDevices {
public:
    /// Marks a part as failed and keeps the error code it failed with.
    ///
    /// @param part  The part of the device.
    /// @param code  The error code the part failed with.
    void set(Device d, uint8_t part, uint32_t code) {
        parts[size_t(d)] |= PartMask(1u << part);
        codes[size_t(d)] = code;
    }

    /// Marks a part as working again.
    ///
    /// @param part  The part of the device.
    void clear(Device d, uint8_t part) { parts[size_t(d)] &= PartMask(~(1u << part)); }

    /// Gets whether a part has failed.
    ///
    /// @param part  The part of the device.
    bool isFailed(Device d, uint8_t part) const { return (parts[size_t(d)] & (1u << part)) != 0; }

    /// Gets whether any part of a device has failed.
    ///
    bool anyFailed(Device d) const { return parts[size_t(d)] != 0; }

    /// Gets the error code a device last failed with.
    ///
    uint32_t code(Device d) const { return codes[size_t(d)]; }

    /// Calls @p fn(device, part) for every failed part, device by device.
    ///
    /// @param fn  Receives the device and the part.
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
    /// model/firmware/figures.py, n_monitor_slots: one slot for each device, and a device that
    /// reports nothing leaves its slot empty
    static constexpr uint8_t SLOTS = uint8_t(Device::COUNT);
    /// model/firmware/figures.py, n_queue: eight parts failing at once with room to spare
    static constexpr uint32_t DEPTH = 32;
    static constexpr uint32_t CHECK_US = 100000;   // model/firmware/figures.py, t_monitor

    /// Creates a monitor whose queue takes its events from @p storage.
    ///
    /// @param queue    The event queue that watch() attaches the monitor's queue to on its first
    ///                 call.
    /// @param tick     The driver tick that watch() attaches the check to on its first call.
    /// @param storage  The events of the monitor's queue.
    DeviceMonitor(EventQueue& queue, DriverTick& tick, DriverEvent (&storage)[DEPTH])
        : queue(queue), tick(tick), storage(storage) {}

    /// Registers a driver, whose failed() every check asks.
    ///
    /// The first call attaches the monitor's queue to the event queue and its check to the driver
    /// tick.
    ///
    /// @param device  The device the driver belongs to.
    /// @param driver  The driver whose failed() every check asks.
    /// @return        False when every slot is taken or the driver tick refuses the check.
    /// @note          Before the main loop only.
    bool watch(Device device, const Driver& driver);

private:
    struct Slot {
        Device device;
        const Driver* driver;
        PartMask reported;   // the failed parts as last published
    };

    /// Publishes every part whose bit changed.
    ///
    /// A full queue leaves the part as it was reported, so the next check tries again.
    ///
    /// @note  The driver tick calls it, every CHECK_US.
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
