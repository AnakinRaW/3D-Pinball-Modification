#include "controls.h"
#include "core/clock.h"
#include "logging/logger.h"

constexpr uint8_t PIN = 5;               // pin-assignment.md: Toggle switch
constexpr uint8_t CLOSED_POSITION = 1;   // controls design: the contact closes in position 1
constexpr uint8_t OPEN_POSITION = 0;     // controls design: the contact opens in position 0
// A check of the pin on every driver tick reports a position that has settled at most one tick
// after QUIET_US.
constexpr uint32_t CHECK_US = DriverTick::PERIOD_US;

/// The driver that has begun and holds the pin's interrupt, nullptr before.
static ToggleSwitch* holder = nullptr;

/// Gets the position the pin shows. The switch closes the pin to ground, and the pull-up holds it
/// HIGH while the contact is open.
static uint8_t readPosition() {
    return digitalReadFast(PIN) == LOW ? CLOSED_POSITION : OPEN_POSITION;
}

bool ToggleSwitch::begin() {
    if (begun) {
        return begunOk;
    }
    begun = true;
    if (holder != nullptr) {
        logWarning("controls", "another toggle switch driver holds the pin");
        return false;
    }
    if (!queue.attach(storage, QUEUE_DEPTH, out)) {
        logWarning("controls", "the event queue takes no further queue");
        return false;
    }
    // R1 on the motherboard leads the pin to the switch, and nothing else pulls it, so the Teensy's
    // pull-up holds it HIGH while the contact is open.
    pinMode(PIN, INPUT_PULLUP);
    // The position at the start counts as reported, so a switch that rests gives no event.
    reported = readPosition();
    reportedUs = nowUs();
    if (!ticker.attach([this] { check(); }, CHECK_US)) {
        logWarning("controls", "the driver tick has no free slot");
        return false;
    }
    holder = this;
    begunOk = true;
    attachInterrupt(digitalPinToInterrupt(PIN), onEdge, CHANGE);
    return true;
}

uint8_t ToggleSwitch::position() const {
    return begunOk ? readPosition() : 0;
}

void ToggleSwitch::onEdge() {
    holder->edge();
}

void ToggleSwitch::edge() {
    const uint32_t now = nowUs();
    if (settling) {
        dropped = true;
        lastEdgeUs = now;
        return;
    }
    // The level is read rather than taken from the edge's direction. A bounce faster than the
    // interrupt's response gives fewer calls than edges, and each call reads the level that holds.
    const uint8_t moved = readPosition();
    if (moved == reported) {
        return;
    }
    report(now, moved);
}

void ToggleSwitch::check() {
    // The tick closes the window, so an edge long after a report never meets a window that the
    // wrap of nowUs() has opened again.
    if (!settling || !elapsedUs(reportedUs, QUIET_US)) {
        return;
    }
    settling = false;
    if (!dropped) {
        return;
    }
    dropped = false;
    // The window dropped an edge. Where the switch has come to rest in another position than the
    // one reported, no further edge will come, so the check publishes that position.
    const uint8_t moved = readPosition();
    if (moved != reported) {
        report(lastEdgeUs, moved);
    }
}

void ToggleSwitch::report(uint32_t edgeUs, uint8_t moved) {
    reported = moved;
    reportedUs = nowUs();
    settling = true;
    out->publish<DriverEventType::TOGGLE_SWITCHED>(edgeUs, SWITCH, moved);
}
