// PWMServoT4, this repository's copy of PJRC's PWMServo for the Teensy 4.1 alone.
//
// A FlexPWM timer repeats the servo pulse in every frame of 20 ms, and a new pulse width takes
// effect at the start of the next frame. For a PWM pin, the Teensy core sets that width to the
// whole frame at boot. PWMServo's first write() connects the pin to the timer at once, so the
// servo first gets a pulse of up to 20 ms. attach() here sets the timer to no pulse first and
// waits until the timer has taken it, so the first pulse is the one the first write() names.
// write() computes the pulse in µs and rounds it to the nearest step of the frame.
#pragma once
#include <Arduino.h>

class PWMServoT4 {
public:
    static constexpr uint8_t MAX_ANGLE_DEG = 180;   // the angle write() maps to maxUs

    // Sets the pin's timer to the 20 ms frame, drives the pin low, takes the pulse range, sets no
    // pulse and waits until the timer has taken that setting. Runs at start-up only, since it
    // waits up to two frames. Returns false for a pin that no FlexPWM channel X drives, for a range
    // whose low end is not below its high end or whose high end exceeds the frame, and when the
    // timer has not taken the setting after two frames. The pin then gets no pulse.
    bool attach(uint8_t pin, uint16_t minUs, uint16_t maxUs);

    // Sets the pulse for an angle between the two ends of the range, at the nearest of the 4096
    // steps of the frame. An angle above MAX_ANGLE_DEG counts as MAX_ANGLE_DEG. Does nothing until
    // attach() has succeeded.
    void write(uint8_t angleDeg);

private:
    static constexpr uint8_t NOT_ATTACHED = 0xFF;

    uint8_t pin = NOT_ATTACHED;
    uint16_t lowUs = 0;    // the pulse for 0 degrees
    uint16_t highUs = 0;   // the pulse for MAX_ANGLE_DEG
};
