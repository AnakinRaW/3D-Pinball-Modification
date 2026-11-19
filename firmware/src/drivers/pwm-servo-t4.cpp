#include "pwm-servo-t4.h"

namespace {

// The pins that a channel X of FlexPWM1 drives, with their submodule, from the table pwm_pin_info
// in the core's pwm.c of Teensyduino 1.62. The core declares that table in no header, so it is
// copied here. attach() takes these pins alone, because the no-pulse setting goes into channel
// X's compare register.
struct XPin {
    uint8_t pin;
    uint8_t submodule;
};
constexpr XPin X_PINS[] = {{0, 1}, {1, 0}, {24, 2}, {25, 3}};

constexpr float FRAME_HZ = 50;   // PWMServo's frame of 20 ms
constexpr uint32_t FRAME_US = 20000;
constexpr uint32_t LOAD_TIMEOUT_MS = 40;   // two frames, while the next reload comes within one
constexpr uint32_t DUTY_BITS = 12;         // PWMServo's resolution of the frame
constexpr uint32_t DUTY_STEPS = 1u << DUTY_BITS;

}   // namespace

bool PWMServoT4::attach(uint8_t pinNumber, uint16_t minUs, uint16_t maxUs) {
    if (minUs >= maxUs || maxUs > FRAME_US) {
        return false;
    }
    const XPin* x = nullptr;
    for (const XPin& candidate : X_PINS) {
        if (candidate.pin == pinNumber) {
            x = &candidate;
        }
    }
    if (x == nullptr) {
        return false;
    }

    analogWriteFrequency(pinNumber, FRAME_HZ);
    digitalWrite(pinNumber, LOW);
    pinMode(pinNumber, OUTPUT);

    // Channel X is high from VAL0 to the frame's end at VAL1, as the core's flexpwmWrite() sets
    // it. A VAL0 past VAL1 is never reached, so the channel puts out no pulse. The buffered
    // registers are written with LDOK clear and take effect at the next reload, which clears LDOK
    // again (i.MX RT1060 Reference Manual, eFlexPWM, MCTRL).
    IMXRT_FLEXPWM_t& pwm = IMXRT_FLEXPWM1;
    const uint16_t mask = 1 << x->submodule;
    pwm.MCTRL |= FLEXPWM_MCTRL_CLDOK(mask);
    pwm.SM[x->submodule].VAL0 = pwm.SM[x->submodule].VAL1 + 1;
    pwm.MCTRL |= FLEXPWM_MCTRL_LDOK(mask);

    // At start-up only: the wait ends at the next reload, at most one frame away.
    elapsedMillis waitedMs;
    while ((pwm.MCTRL & FLEXPWM_MCTRL_LDOK(mask)) != 0) {
        if (waitedMs > LOAD_TIMEOUT_MS) {
            return false;
        }
    }

    lowUs = minUs;
    highUs = maxUs;
    pin = pinNumber;
    return true;
}

void PWMServoT4::write(uint8_t angleDeg) {
    if (pin == NOT_ATTACHED) {
        return;
    }
    const uint32_t angle = angleDeg > MAX_ANGLE_DEG ? MAX_ANGLE_DEG : angleDeg;
    // The pulse in µs times MAX_ANGLE_DEG, so the division by the angle comes last, and from there
    // the nearest step of the frame. PWMServo keeps the range in 16 µs and rounds down twice,
    // which put 0 degrees at 493 µs for a range from 500 µs.
    const uint64_t pulse = uint64_t(lowUs) * MAX_ANGLE_DEG + uint64_t(highUs - lowUs) * angle;
    const uint64_t whole = uint64_t(FRAME_US) * MAX_ANGLE_DEG;
    const uint32_t duty = uint32_t((pulse * DUTY_STEPS + whole / 2) / whole);

    // analogWrite()'s resolution is one setting for every pin, so it is set and put back with
    // interrupts off, as PWMServo does
    noInterrupts();
    const uint32_t oldBits = analogWriteResolution(DUTY_BITS);
    analogWrite(pin, static_cast<int>(duty));   // duty is at most DUTY_STEPS
    analogWriteResolution(oldBits);
    interrupts();
}
