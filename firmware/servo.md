# Servo driver

The driver sets the angle of the servo that [`docs/parts/servo/design.md`](../docs/parts/servo/design.md) describes. It utilizes [PWMServo](https://github.com/PaulStoffregen/PWMServo).

## Servo positions

The servo is used in the ball drain as part of the ball separator. It moves a small seesaw. It therefore has two well-known positions. The start position and the end position put the seesaw at its two extremes.

The driver knows the mechanism's two end positions, and `moveTo()` takes the servo to one of them. Their angles, in whole degrees, are the constants `kStart` and `kEnd`. They are set once on the assembled machine.

## Driver events

The servo has no feedback line, so the driver cannot tell when a move has ended, and it publishes no event. `position()` gives the end position last moved to. A component that must know the servo has arrived waits a fixed travel time instead or uses other detection methods that imply the servo moved.

## PWMServo

PWMServo sets the timer of pin [24](../docs/pin-assignment.md "Servo signal"), FlexPWM1.2, to a frame of 20 ms, and the timer repeats the pulse in every frame. The driver therefore needs no interrupt, and `moveTo()` returns at once.

`attach()` takes the SER0049's pulse range, 500 µs for 0° and 2500 µs for 180°.

## Start-up

PWMServo has the issue that its first move puts out an overlong pulse of up to 20 ms. The driver prevents that in `begin()` and then moves the servo to its start position. If the timer has not taken the fix after two frames, `begin()` resets the Teensy.

## Device faults

The driver notes no fault as the servo has no feedback line and thus cannot see a move that fails.

## Driver Code

```cpp
#include <PWMServo.h>

class ServoDriver : public Driver {
public:
    enum class Position : uint8_t { Start, End };

    // attaches the pin with the SER0049's range, 500 µs to 2500 µs, parks the pulse and
    // moves the servo to its start position; resets the Teensy if the timer has not
    // taken the parked pulse after two frames
    bool begin() override {
        servo_.attach(kPin, 500, 2500);
        IMXRT_FLEXPWM1.MCTRL |= FLEXPWM_MCTRL_CLDOK(kMask);
        IMXRT_FLEXPWM1.SM[kSub].VAL0 = IMXRT_FLEXPWM1.SM[kSub].VAL1 + 1;   // beyond the frame
        IMXRT_FLEXPWM1.MCTRL |= FLEXPWM_MCTRL_LDOK(kMask);
        elapsedMillis waited;                                              // counts milliseconds up from 0
        while (IMXRT_FLEXPWM1.MCTRL & FLEXPWM_MCTRL_LDOK(kMask)) {         // until the timer takes it
            if (waited > kLoadTimeoutMs) SCB_AIRCR = 0x05FA0004;              // hard reset
        }
        ready_ = true;
        moveTo(Position::Start);
        return true;
    }

    // moves the servo to one of its two end positions
    void moveTo(Position target) {
        if (!ready_) return;
        servo_.write(target == Position::Start ? kStart : kEnd);
        position_ = target;
    }

    // gets the end position last moved to
    Position position() const { return position_; }

private:
    static constexpr uint8_t  kPin   = 24;        // pin-assignment.md: Servo signal
    static constexpr uint8_t  kSub   = 2;         // FlexPWM1's submodule 2, behind the pin
    static constexpr uint16_t kMask  = 1 << kSub;
    static constexpr uint32_t kLoadTimeoutMs = 40;  // two frames of 20 ms
    static constexpr uint8_t  kStart = TODO;      // the start position in degrees, set on the machine
    static constexpr uint8_t  kEnd   = TODO;      // the end position in degrees, set on the machine

    PWMServo servo_;
    bool     ready_    = false;
    Position position_ = Position::Start;
};
```
