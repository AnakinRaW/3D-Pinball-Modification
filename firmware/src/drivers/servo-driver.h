// The servo driver moves the SER0049 in the drain between its start and end position at the
// config's speed, one step per frame on the driver tick, with PWMServoT4 from
// drivers/pwm-servo-t4.h.
//
// Driver and the driver tick come from drivers/driver-base.h. The main task hands the angles and
// the speed from the config to configure() before begin().
//
// Design: docs/firmware/drivers/servo.md.
#pragma once
#include <Arduino.h>
#include "drivers/driver-base.h"
#include "drivers/pwm-servo-t4.h"   // this repository's copy of PWMServo

class ServoDriver : public Driver {
public:
    enum class Position : uint8_t { START, END };

    static constexpr uint8_t ANGLE_LIMIT_DEG = 60;    // past it the servo bends the seesaw's bridge
    static constexpr uint8_t START_DEG = 0;           // servo.start_deg's default
    static constexpr uint32_t SPEED_DEG_S = 50;       // servo.speed_deg_s's default
    static constexpr uint32_t SPEED_MIN_DEG_S = 10;   // the slowest servo.speed_deg_s
    static constexpr uint32_t SPEED_MAX_DEG_S = 200;   // the fastest servo.speed_deg_s

    /// The config's servo.start_deg, servo.end_deg and servo.speed_deg_s, each with its default.
    struct Settings {
        uint8_t startDeg = START_DEG;
        uint8_t endDeg = END_DEG;
        uint32_t speedDegS = SPEED_DEG_S;
    };

    /// Takes the two angles and the speed.
    ///
    /// A value outside its range counts as the nearer end. Without a call, begin() takes the
    /// defaults.
    ///
    /// @param settings  The start angle, the end angle and the speed.
    /// @note            The main task calls it before begin().
    void configure(const Settings& settings);

    /// Attaches its step to the driver tick, attaches the pin with the SER0049's range, sets no
    /// pulse and sends the servo to its start position.
    ///
    /// @return  False when the driver tick is full or the timer has not taken the no-pulse
    ///          setting after two frames.
    bool begin() override;

    /// Moves the servo at the config's speed to one of its two end positions.
    ///
    /// Does nothing once begin() has failed.
    ///
    /// @param target  The end position to move to.
    void moveTo(Position target);

    /// Gets the end position last moved to.
    Position position() const { return lastTarget; }

    /// Gets how long a move from one end position to the other takes at the config's speed, in
    /// whole frames, rounded up.
    uint32_t travelMs() const;

    /// Gets the servo as failed once its timer has not taken the no-pulse setting in attach().
    Fault failed() const override { return {stuck, 0}; }

private:
    static constexpr uint8_t END_DEG = 40;   // servo.end_deg's default

    /// Runs on the driver tick once per frame.
    void step();

    PWMServoT4 servo;
    Settings settings;                // what configure() took, held inside the ranges
    volatile bool ready = false;      // the timer took the no-pulse setting
    uint8_t startDeg = 0;             // servo.start_deg from the settings
    uint8_t endDeg = 0;               // servo.end_deg from the settings
    uint8_t nowDeg = 0;               // the angle the pulse names, changed by step() alone
    volatile uint8_t targetDeg = 0;   // the angle step() walks the pulse to
    uint32_t stepMdeg = 0;            // servo.speed_deg_s as thousandths of a degree per frame
    uint32_t carryMdeg = 0;           // thousandths of a degree gathered toward the next step
    bool stuck = false;               // the timer did not take the no-pulse setting in attach()
    Position lastTarget = Position::START;   // the end position last moved to
};
