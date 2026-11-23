// Reads where a finger touches the screen of the Adafruit 2090 and reports a touch as it comes
// down, moves, rests and leaves. The reads run on the I²C bus in the background, so no caller ever
// waits for the touch controller.
//
// The driver also tells whether the machine has its display module, and reports the touch
// controller failed once it stops answering.
//
// Design: docs/firmware/drivers/touch.md.
#pragma once
#include <Arduino.h>
#include "core/event-queue.h"
#include "drivers/driver-base.h"
#include "io/i2c-bus.h"

/// Where a finger touches the screen, in the picture's pixels, 0 at the top left.
struct Finger {
    bool down;   // a finger touches the screen
    uint16_t x;
    uint16_t y;
};

class Touch : public Driver {
public:
    static constexpr uint32_t TICK_US = 20000;    // design.md: the touch driver's tick
    static constexpr uint16_t SLOP_PX = 10;       // design.md: the pixels a resting finger may move
    static constexpr uint32_t HOLD_MS = 1000;     // design.md: the rest before TouchHeld
    static constexpr uint32_t FAIL_MS = 100;      // design.md: no working read, and it has failed
    static constexpr uint32_t QUEUE_DEPTH = 8;    // the touch events the queue holds
    static constexpr uint16_t RAW_LEFT = 327;     // touch.raw_left's default, measured
    static constexpr uint16_t RAW_RIGHT = 1;      // touch.raw_right's default, measured
    static constexpr uint16_t RAW_TOP = 3;        // touch.raw_top's default, measured
    static constexpr uint16_t RAW_BOTTOM = 254;   // touch.raw_bottom's default, measured
    static constexpr uint16_t RAW_MAX = 4095;     // FT6x06 register map: a reading has 12 bits
    /// The least difference between the readings at two opposite edges. A smaller one cannot
    /// come from a touch layer over the whole picture, so that axis keeps its defaults.
    static constexpr uint16_t SPAN_MIN = 100;

    /// The config's touch.raw_left, touch.raw_right, touch.raw_top and touch.raw_bottom: the
    /// controller's readings at the picture's four edges, each with its default.
    ///
    /// The panel's y reads rawLeft at the picture's left edge and rawRight at its right edge. The
    /// panel's x reads rawTop at the top edge and rawBottom at the bottom edge.
    struct Settings {
        uint16_t rawLeft = RAW_LEFT;
        uint16_t rawRight = RAW_RIGHT;
        uint16_t rawTop = RAW_TOP;
        uint16_t rawBottom = RAW_BOTTOM;
    };

    /// Creates a driver that reads the touch controller on @p bus.
    ///
    /// @param bus     The I²C bus of the touch controller.
    /// @param queue   The event queue that begin() attaches the driver's events to.
    /// @param ticker  The driver tick that begin() attaches the reads to.
    Touch(io::I2cBus& bus, EventQueue& queue, DriverTick& ticker);

    /// Takes the readings at the picture's edges, which map the controller's positions onto the
    /// picture.
    ///
    /// A reading above RAW_MAX counts as RAW_MAX. An axis whose two readings lie less than
    /// SPAN_MIN apart keeps its defaults. Without a call, the driver takes the defaults.
    ///
    /// @param settings  The four readings.
    /// @note            The main task calls it before begin().
    void configure(const Settings& settings);

    /// Gets the readings configure() took, held inside their ranges.
    Settings settings() const { return held; }

    /// Finds out whether the touch controller answers, and starts reading it every TICK_US.
    ///
    /// It waits until 580 ms after power-up, when the controller first answers, and then for
    /// one transfer on the bus. A controller that does not answer leaves present() false, which
    /// still counts as success. A second call returns what the first returned.
    ///
    /// @return  False when the driver tick is full.
    bool begin() override;

    /// Gets whether the machine has its display module, which begin() finds out from the touch
    /// controller.
    bool present() const { return found; }

    /// Gets where a finger touches the screen, as the last working read saw it.
    ///
    /// A finger counts as gone once the controller has brought no read for FAIL_MS.
    Finger finger() const;

    /// Gets part 0 as failed once the controller that answered at begin() has brought no read for
    /// FAIL_MS.
    ///
    /// The code holds the LPI2C error flags of the last failed read.
    Fault failed() const override;

private:
    /// Ends a read in the bus's interrupt: turns its five bytes into the touch events.
    ///
    /// @param errors  The controller's error flags, 0 when the read worked.
    void done(uint32_t errors);

    /// Publishes the events of one working read.
    ///
    /// @param down  Whether a finger touches the screen.
    /// @param at    Where, in the picture's pixels.
    void report(bool down, TouchPoint at);

    static constexpr uint8_t READ_BYTES = 5;     // TD_STATUS and the first touch point
    static constexpr uint8_t TD_STATUS = 0x02;   // FT6x06 register map: the first register read
    static constexpr uint8_t ADDRESS = 0x38;     // design.md: the CST026

    io::I2cBus& bus;
    Settings held;   // what configure() took, held inside the ranges
    EventQueue& queue;
    DriverTick& ticker;
    uint8_t bytes[READ_BYTES] = {};
    io::I2cTransfer probe;   // the address alone, which tells whether the controller answers
    io::I2cTransfer read;    // TD_STATUS and the first touch point
    DriverEvent storage[QUEUE_DEPTH] = {};
    EventQueue::Producer* out = nullptr;

    bool begun = false;
    bool begunOk = false;
    volatile bool found = false;
    /// The finger as the last working read saw it: x in bits 0 to 15, y in bits 16 to 30 and
    /// whether it is down in bit 31, so the main loop reads it in one access.
    volatile uint32_t packed = 0;
    volatile uint32_t lastGoodUs = 0;   // nowUs() at the end of the last working read
    volatile uint32_t lastErrors = 0;   // the error flags of the last failed read

    // what the bus's interrupt alone keeps about the touch on the screen
    TouchPoint downAt = {};     // where the finger came down
    bool failing = false;       // the last read failed
    TouchPoint reported = {};   // the position last published
    uint32_t downUs = 0;        // nowUs() when it came down
    bool resting = false;       // it has stayed within SLOP_PX of where it came down
};
