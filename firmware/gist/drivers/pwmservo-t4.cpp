#include <Arduino.h>

// PWMServoT4, a library in this repository that copies PaulStoffregen/PWMServo for the Teensy 4
// alone. A new pulse width takes effect at the start of the next 20 ms frame. For the servo's pin,
// the core sets the width to the whole frame at boot, so PWMServo's first write() puts out a pulse
// of up to 20 ms. This attach() sets the timer to no pulse first and waits until the timer has
// taken it, so the first pulse is the one the first write() names. The header is PWMServoT4.h, so
// the compiler cannot include the PWMServo that Teensyduino bundles in its place.
// write() and everything else are the original's, unchanged.

// the core's pwm.c: each pin's timer, with the FlexPWM module in the high and the submodule in the
// low nibble of module
extern const struct pwm_pin_info_struct { uint8_t type, module, channel, muxval; } pwm_pin_info[];

class PWMServoT4 {
public:
    // sets the pin's timer to the 50 Hz frame, drives the pin low as an output, takes the pulse
    // range, sets no pulse and waits until the timer takes that setting; false for a pin without
    // FlexPWM or when the timer has not taken the no-pulse setting after two frames, and the
    // pin then stays low, so the servo gets no pulse
    bool attach(uint8_t pin, uint16_t minUs, uint16_t maxUs) {
        if (pin >= NUM_DIGITAL_PINS || pwm_pin_info[pin].type != 1) return false;   // 1: FlexPWM
        analogWriteFrequency(pin, 50);
        pinMode(pin, OUTPUT);
        pin_ = pin, min16_ = minUs >> 4, max16_ = maxUs >> 4;            // as the original keeps them

        IMXRT_FLEXPWM_t& pwm = *kModules[pwm_pin_info[pin].module >> 4];
        const uint8_t  sub  = pwm_pin_info[pin].module & 0x03;
        const uint16_t mask = 1 << sub;
        pwm.MCTRL |= FLEXPWM_MCTRL_CLDOK(mask);
        pwm.SM[sub].VAL0 = pwm.SM[sub].VAL1 + 1;                           // starts past the frame's end: no pulse
        pwm.MCTRL |= FLEXPWM_MCTRL_LDOK(mask);
        elapsedMillis waited;                                              // counts milliseconds up from 0
        while (pwm.MCTRL & FLEXPWM_MCTRL_LDOK(mask)) {                     // until the timer takes it
            if (waited > kLoadTimeoutMs) return false;
        }
        return true;
    }

    // write(angle) as in the original: the angle, 0 to 180, as a pulse between the two ends of
    // the range, at 12-bit resolution of the frame
    void write(int angle);

private:
    static constexpr uint32_t kLoadTimeoutMs = 40;  // two frames of 20 ms
    static inline IMXRT_FLEXPWM_t* const kModules[] = {&IMXRT_FLEXPWM1, &IMXRT_FLEXPWM2,
                                                       &IMXRT_FLEXPWM3, &IMXRT_FLEXPWM4};

    uint8_t  pin_   = 0;
    uint16_t min16_ = 0;    // the pulse for 0°, in 16 µs
    uint16_t max16_ = 0;    // the pulse for 180°, in 16 µs
};
