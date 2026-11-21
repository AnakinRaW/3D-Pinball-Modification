#include "servo-driver.h"
#include "logging/logger.h"
#include "logging/debug-log.h"
#include <inttypes.h>

constexpr uint8_t PIN = 0;                // pin-assignment.md: Servo signal
constexpr uint16_t PULSE_MIN_US = 500;    // servo.md: the SER0049's pulse at 0 degrees
constexpr uint16_t PULSE_MAX_US = 2500;   // servo.md: its pulse at 180 degrees
constexpr uint32_t FRAME_MS = 20;         // the servo's frame, one step per frame
constexpr uint32_t US_PER_MS = 1000;
constexpr uint32_t MDEG_PER_DEG = 1000;   // thousandths of a degree in a degree

/// Holds a value inside its range.
///
/// A value it moves is named in the log.
///
/// @param name     The setting's name.
/// @param lowest   The lowest value of the range.
/// @param highest  The highest value of the range.
static uint32_t held(const char* name, uint32_t value, uint32_t lowest, uint32_t highest) {
    if (value >= lowest && value <= highest) {
        return value;
    }
    logWarning("servo", "%s %" PRIu32 " lies outside %" PRIu32 " to %" PRIu32, name, value, lowest,
               highest);
    return value < lowest ? lowest : highest;
}

void ServoDriver::configure(const Settings& s) {
    settings.startDeg = uint8_t(held("start_deg", s.startDeg, 0, ANGLE_LIMIT_DEG));
    settings.endDeg = uint8_t(held("end_deg", s.endDeg, 0, ANGLE_LIMIT_DEG));
    settings.speedDegS = held("speed_deg_s", s.speedDegS, SPEED_MIN_DEG_S, SPEED_MAX_DEG_S);
}

bool ServoDriver::begin() {
    if (!driverTick.attach([this] { step(); }, FRAME_MS * US_PER_MS)) {
        logWarning("servo", "the driver tick refused its step");
        return false;
    }
    if (!servo.attach(PIN, PULSE_MIN_US, PULSE_MAX_US)) {
        // The pin stays low, so the servo gets no pulse.
        logWarning("servo", "the pulse timer did not start, the servo stays without pulse");
        stuck = true;
        return false;
    }
    startDeg = settings.startDeg;
    endDeg = settings.endDeg;
    stepMdeg = settings.speedDegS * FRAME_MS;

    // The servo has no feedback line, so after power-up its position is unknown, and a servo runs
    // at full speed to whatever its first pulse points at. No ramp can soften that first move,
    // because a ramp needs a starting point. The servo rests at its start position between
    // releases, so a first pulse there leaves it standing in the usual case.
    nowDeg = startDeg;
    targetDeg = startDeg;
    lastTarget = Position::START;
    servo.write(nowDeg);
    ready = true;
    return true;
}

void ServoDriver::moveTo(Position target) {
    if (!ready) {
        DEBUG_LOG("ServoDriver::moveTo: no begin() yet, the servo stays\n");
        return;
    }
    if (target != lastTarget) {
        DEBUG_LOG("servo moves to its %s position\n", target == Position::START ? "start" : "end");
    }
    // step() walks the pulse there.
    targetDeg = target == Position::START ? startDeg : endDeg;
    lastTarget = target;
}

uint32_t ServoDriver::travelMs() const {
    const uint32_t spanDeg = startDeg > endDeg ? startDeg - endDeg : endDeg - startDeg;
    if (stepMdeg == 0) {
        return 0;
    }
    return (spanDeg * MDEG_PER_DEG + stepMdeg - 1) / stepMdeg * FRAME_MS;
}

/// Limits the speed by moving the angle the pulse names a little each frame.
///
/// The servo itself always runs at full speed to the angle its pulse names. A speed below one
/// degree per frame gathers its thousandths of a degree over several frames.
void ServoDriver::step() {
    if (!ready) {
        return;
    }
    const uint8_t toDeg = targetDeg;
    if (nowDeg == toDeg) {
        carryMdeg = 0;
        return;
    }
    carryMdeg += stepMdeg;
    const uint32_t deg = carryMdeg / MDEG_PER_DEG;
    if (deg == 0) {
        return;
    }
    carryMdeg %= MDEG_PER_DEG;
    const uint8_t leftDeg = toDeg > nowDeg ? toDeg - nowDeg : nowDeg - toDeg;
    if (deg >= leftDeg) {
        // The last step stops on the target.
        nowDeg = toDeg;
        carryMdeg = 0;
    } else {
        nowDeg = toDeg > nowDeg ? nowDeg + deg : nowDeg - deg;
    }
    servo.write(nowDeg);
}
