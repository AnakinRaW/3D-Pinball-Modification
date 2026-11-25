// Reports the position of the machine's toggle switch. An interrupt on the switch's pin publishes
// TOGGLE_SWITCHED the moment the switch is flipped, and the game can read the position at any time.
//
// The contact's bounce gives one event. A switch flipped back right after a flip still ends on an
// event with the position it rests in.
//
// Design: docs/firmware/drivers/controls.md.
#pragma once
#include <Arduino.h>
#include "core/event-queue.h"
#include "drivers/driver-base.h"

class ToggleSwitch : public Driver {
public:
    /// model/parts/controls/figures.py, t_bounce: an edge this soon after a report is bounce
    static constexpr uint32_t QUIET_US = 50000;
    static constexpr uint8_t SWITCH = 0;   // the source of TOGGLE_SWITCHED, the build's one switch

    /// Creates a driver that publishes into @p queue.
    ///
    /// @param queue   The event queue that begin() attaches TOGGLE_SWITCHED to.
    /// @param ticker  The driver tick that begin() attaches the check of a settled position to.
    ToggleSwitch(EventQueue& queue, DriverTick& ticker) : queue(queue), ticker(ticker) {}

    /// Sets the switch's pin up as an input with the Teensy's pull-up, and publishes
    /// TOGGLE_SWITCHED from then on.
    ///
    /// An event carries the position the switch moved to, and only a position that differs from
    /// the one last reported. An edge within QUIET_US of a report is bounce and publishes nothing.
    /// Once QUIET_US has passed, a position that differs from the last report is published with
    /// the time of the last edge. A second call returns what the first returned.
    ///
    /// @return  False when the driver tick has no free slot, when the event queue takes no further
    ///          queue, or when another toggle switch driver has begun, which holds the pin's
    ///          interrupt.
    /// @note    The main task calls it before the main loop. The pin interrupt has to run at the
    ///          driver tick's priority, so the two never publish at once.
    bool begin() override;

    /// Gets the position printed on the switch, 1 or 0, read from the pin at the call.
    ///
    /// @return  0 before begin() and after a failed begin().
    /// @note    Any task or interrupt may call it.
    uint8_t position() const;

private:
    /// The switch reports at most once every QUIET_US, and the main loop reads the queue far more
    /// often. Two events, a power of two, leave room for one the main loop has not read yet.
    static constexpr uint32_t QUEUE_DEPTH = 2;

    /// Takes an edge of the switch's pin and publishes a new position outside the bounce window.
    ///
    /// @note  Runs in the pin interrupt.
    void edge();

    /// Ends the bounce window once QUIET_US has passed, and publishes a position that differs
    /// from the last report where the window dropped an edge.
    ///
    /// @note  Runs on the driver tick.
    void check();

    /// Publishes @p moved as the new position and opens the bounce window.
    ///
    /// @param edgeUs  The time of the edge that moved the switch.
    /// @param moved   The position the switch moved to.
    void report(uint32_t edgeUs, uint8_t moved);

    /// Hands an edge to the driver that holds the pin's interrupt.
    ///
    /// @note  The pin interrupt calls it.
    static void onEdge();

    EventQueue& queue;
    DriverTick& ticker;
    DriverEvent storage[QUEUE_DEPTH] = {};
    EventQueue::Producer* out = nullptr;
    bool begun = false;
    bool begunOk = false;
    volatile uint8_t reported = 0;      // the position last published, or read at begin()
    volatile uint32_t reportedUs = 0;   // nowUs() at the last report
    volatile uint32_t lastEdgeUs = 0;   // nowUs() at the last edge the window dropped
    volatile bool settling = false;     // the bounce window after the last report is open
    volatile bool dropped = false;      // the open window has dropped an edge
};
