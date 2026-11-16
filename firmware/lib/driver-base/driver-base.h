// What every driver shares, as docs/firmware/driver-design.md and docs/firmware/error-handling.md
// design it: the Driver interface, the Fault a driver reports and the driver tick.
//
// driverTick is the one DriverTick every driver attaches to.
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
    // Sets the driver up. Returns false when that fails.
    virtual bool begin() = 0;

    // Gets which parts of the driver's device have failed. The default reports none, for a
    // driver that notes no fault.
    virtual Fault failed() const { return {}; }
};

// One IntervalTimer, shared by every driver that needs a regular period.
class DriverTick {
public:
    using Callback = teensy::inplace_function<void(void), 16>;   // a lambda capturing this fits

    static constexpr uint32_t PERIOD_US = 5000;   // driver-design.md: the tick's period
    static constexpr uint8_t SLOTS = 8;           // the callbacks it runs at most

    // Runs fn every periodUs. Before the main loop only, and the first call starts the timer.
    // Returns false when every slot is taken, periodUs is no multiple of PERIOD_US or no
    // IntervalTimer is free.
    bool attach(Callback fn, uint32_t periodUs);

private:
    static constexpr uint8_t PRIORITY = 96;   // general-design.md: the driver tick's row

    struct Slot {
        Callback fn;
        uint32_t every;
        uint32_t left;
    };

    // The IntervalTimer's interrupt: counts every slot down and runs the ones that are due.
    void tick();

    IntervalTimer timer;
    Slot slots[SLOTS];
    volatile uint8_t count = 0;
};

extern DriverTick driverTick;
