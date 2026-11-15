// What ServoDriver needs from the rest of the firmware. Driver, Fault and DriverTick are those of
// firmware/gist/driver-design.cpp and firmware/gist/error-handling.cpp. The device monitor and the
// config are stand-ins: the monitor accepts every driver, and the config holds the keys a test
// sets and returns the default for every other.
#pragma once
#include <Arduino.h>
#include <IntervalTimer.h>

using PartMask = uint16_t;

struct Fault {
    PartMask parts;   // one bit per failed part
    uint32_t code;    // the driver's error code, 0 for a driver without error codes
};

class Driver {
public:
    // sets the driver up; false when that fails
    virtual bool begin() = 0;

    // gets which parts of the driver's device have failed; the default reports none, for a
    // driver that notes no fault
    virtual Fault failed() const { return {}; }
};

// one IntervalTimer, shared by every driver that needs a regular period
class DriverTick {
public:
    using Callback = teensy::inplace_function<void(void), 16>;   // a lambda capturing this fits

    static constexpr uint32_t kPeriodUs = 5000;   // tick period

    // runs fn every periodUs; before the main loop only, and the first call starts the timer. False
    // when every slot is taken, periodUs is no multiple of kPeriodUs or no IntervalTimer is free
    bool attach(Callback fn, uint32_t periodUs) {
        const uint32_t every = periodUs / kPeriodUs;
        if (count_ == kSlots || every == 0 || periodUs % kPeriodUs != 0) {
            return false;
        }
        if (count_ == 0) {
            timer_.priority(kPriority);
            if (!timer_.begin([this] { tick(); }, kPeriodUs)) {
                return false;
            }
        }
        noInterrupts();   // the tick must not see a half-written slot
        slots_[count_] = Slot{fn, every, every};
        count_ = count_ + 1;
        interrupts();
        return true;
    }

private:
    static constexpr uint8_t kSlots = 8;
    static constexpr uint8_t kPriority = 96;   // the driver tick's row in general-design.md

    struct Slot {
        Callback fn;
        uint32_t every;
        uint32_t left;
    };

    // the IntervalTimer: counts every slot down and runs the ones that are due
    void tick() {
        for (uint8_t i = 0; i < count_; ++i) {
            if (--slots_[i].left != 0) {
                continue;
            }
            slots_[i].left = slots_[i].every;
            slots_[i].fn();
        }
    }

    IntervalTimer timer_;
    Slot slots_[kSlots];
    volatile uint8_t count_ = 0;
};

extern DriverTick driverTick;

// stand-in for the device monitor
enum class Device : uint8_t { Servo };
struct DeviceMonitor {
    bool watch(Device, const Driver&) { return true; }
};
extern DeviceMonitor deviceMonitor;

// stand-in for the machine config: a key a test has set reads as its value inside the key's
// range, and an absent key reads as its default
enum class ConfigKey : uint8_t { kServoStart, kServoEnd, kServoSpeed };
struct Config {
    static constexpr uint8_t KEYS = 3;

    void set(ConfigKey key, int32_t value) {
        values[uint8_t(key)] = value;
        present[uint8_t(key)] = true;
    }

    void clear() {
        for (bool& p : present) {
            p = false;
        }
    }

    int32_t get(ConfigKey key, int32_t fallback, int32_t lowest, int32_t highest) const {
        if (!present[uint8_t(key)]) {
            return fallback;
        }
        const int32_t v = values[uint8_t(key)];
        return v < lowest ? lowest : v > highest ? highest : v;
    }

    int32_t values[KEYS] = {};
    bool present[KEYS] = {};
};
extern Config config;
