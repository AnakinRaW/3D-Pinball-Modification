// Reports every ball that leaves play through the drain. An interrupt on the receiver's pin
// publishes BALL_DRAINED the moment a ball breaks the IR beam across the drain, so no ball passes
// unseen however busy the main loop is.
//
// The driver also reports the beam failed once it has stayed broken far longer than a passing
// ball keeps it broken.
//
// Design: docs/firmware/drivers/break-beam.md.
#pragma once
#include <Arduino.h>
#include "core/event-queue.h"
#include "drivers/driver-base.h"

class BreakBeamDriver : public Driver {
public:
    /// model/parts/break-beam/figures.py, t_dead: a beam clear for less than this between two
    /// breaks still belongs to the same crossing
    static constexpr uint32_t DEAD_US = 10000;
    /// model/parts/break-beam/figures.py, t_blocked_fail: a beam broken this long has failed
    static constexpr uint32_t FAIL_MS = 5000;
    /// drain.md: the machine has two balls, which drain at most once each between two passes of
    /// the main loop. The queue holds twice that, a power of two
    static constexpr uint32_t QUEUE_DEPTH = 4;
    static constexpr uint8_t GATE = 0;   // the source of BALL_DRAINED, the build's one gate

    /// Creates a driver that publishes into @p queue.
    ///
    /// @param queue  The event queue that begin() attaches BALL_DRAINED to.
    explicit BreakBeamDriver(EventQueue& queue) : queue(queue) {}

    /// Sets the receiver's pin up as an input with hysteresis and without the Teensy's pull-up,
    /// and publishes BALL_DRAINED from then on.
    ///
    /// A break counts as a ball only after the beam has been clear for DEAD_US, so the flicker at
    /// the beam's edge as a ball enters or leaves gives one event. A second call returns what the
    /// first returned.
    ///
    /// @return  False when the event queue takes no further queue, or when another break beam
    ///          driver has begun, which holds the pin's interrupt.
    /// @note    The main task calls it before the main loop.
    bool begin() override;

    /// Gets whether something blocks the beam, read from the pin at the call.
    ///
    /// @return  False before begin() and after a failed begin().
    /// @note    Any task or interrupt may call it.
    bool blocked() const;

    /// Gets part 0 as failed while the beam has stayed broken for FAIL_MS, counted from the
    /// break or from begin(), whichever came later.
    ///
    /// The part works again once the beam is clear. The code is always 0.
    Fault failed() const override;

private:
    /// Takes an edge of the receiver's output and publishes BALL_DRAINED for a new crossing.
    ///
    /// @note  Runs in the pin interrupt.
    void edge();

    /// Hands an edge to the driver that holds the pin's interrupt.
    ///
    /// @note  The pin interrupt calls it.
    static void onEdge();

    EventQueue& queue;
    DriverEvent storage[QUEUE_DEPTH] = {};
    EventQueue::Producer* out = nullptr;
    bool begun = false;
    bool begunOk = false;
    volatile uint32_t clearedUs = 0;   // nowUs() when the beam last cleared
    volatile uint32_t brokenUs = 0;    // nowUs() when the beam last broke
    volatile bool broken = false;      // the level the interrupt last read was HIGH
};
