#include "break-beam.h"
#include "core/clock.h"
#include "logging/debug-log.h"

constexpr uint8_t PIN = 31;   // pin-assignment.md: Receiver output of the ball drain gate

/// The driver that has begun and holds the pin's interrupt, nullptr before.
static BreakBeamDriver* holder = nullptr;

bool BreakBeamDriver::begin() {
    if (begun) {
        return begunOk;
    }
    begun = true;
    if (holder != nullptr) {
        DEBUG_LOG("break-beam: another break beam driver holds the pin\n");
        return false;
    }
    if (!queue.attach(storage, QUEUE_DEPTH, out)) {
        DEBUG_LOG("break-beam: the event queue takes no further queue\n");
        return false;
    }
    // R3 on the motherboard pulls the pin up. The Teensy's pull-up next to it would keep a lit
    // receiver from pulling the pin LOW, so INPUT leaves it off.
    pinMode(PIN, INPUT);
    // A beam clear at the start counts as clear long enough, and a beam broken at the start
    // counts toward a fault from now on.
    const uint32_t now = nowUs();
    clearedUs = now - DEAD_US;
    brokenUs = now;
    broken = digitalReadFast(PIN) == HIGH;
    holder = this;
    begunOk = true;
    // attachInterrupt() also turns the pad's hysteresis on, which keeps the receiver's slow edge
    // from firing the interrupt more than once.
    attachInterrupt(digitalPinToInterrupt(PIN), onEdge, CHANGE);
    return true;
}

bool BreakBeamDriver::blocked() const {
    // The receiver conducts while it is lit, and R3 pulls the pin HIGH once a ball darkens it.
    return begunOk && digitalReadFast(PIN) == HIGH;
}

Fault BreakBeamDriver::failed() const {
    if (!blocked() || !elapsedMs(brokenUs, FAIL_MS)) {
        return {};
    }
    return {1, 0};
}

void BreakBeamDriver::onEdge() {
    holder->edge();
}

void BreakBeamDriver::edge() {
    // The level is read rather than taken from the edge's direction. Two edges closer together
    // than the interrupt's response give one call or two, each reading the level that holds.
    const uint32_t now = nowUs();
    if (digitalReadFast(PIN) != HIGH) {
        clearedUs = now;
        broken = false;
        return;
    }
    // A break read twice was clear for less than the interrupt's response between the readings,
    // which belongs to the same crossing.
    if (broken) {
        return;
    }
    broken = true;
    brokenUs = now;
    if (!elapsedUs(clearedUs, DEAD_US)) {
        return;
    }
    out->publish<DriverEventType::BALL_DRAINED>(now, GATE, NoPayload{});
}
