# Servo driver

The driver sets the angle of the servo that [`docs/parts/servo/design.md`](../../parts/servo/design.md) describes. It uses [PWMServo](https://github.com/PaulStoffregen/PWMServo).

## Servo positions

The servo is used in the ball drain as part of the ball separator. It moves a small seesaw. It therefore has two well-known positions. The start position and the end position put the seesaw at its two extremes.

The driver knows the mechanism's two end positions, and `moveTo()` takes the servo to one of them. Their angles are set once on the assembled machine, in whole degrees.

## Driver events

The servo has no feedback line, so the driver cannot tell when a move has ended, and it publishes no event. `position()` gives the end position last moved to. A component that must know the servo has arrived waits a fixed travel time instead or uses other detection methods that imply the servo moved.

## PWMServo

The timer of pin [0](../../pin-assignment.md "Servo signal") is FlexPWM1.1. PWMServo sets it to a frame of 20 ms, and the timer repeats the pulse in every frame. The driver therefore needs no interrupt, and `moveTo()` returns at once.

`attach()` takes the SER0049's pulse range, 500 µs for 0° and 2500 µs for 180°.

## Start-up

PWMServo has the issue that its first move puts out an overlong pulse of up to 20 ms. The driver prevents that in `begin()` and then moves the servo to its start position. If the timer has not taken the fix after two frames, `begin()` resets the Teensy.

## Device faults

The driver notes no fault as the servo has no feedback line and thus cannot see a move that fails.

## Driver Code

```cpp
class ServoDriver : public Driver {
public:
    enum class Position : uint8_t { Start, End };

    // attaches the pin with the SER0049's range, 500 µs to 2500 µs, parks the pulse and
    // moves the servo to its start position; resets the Teensy if the timer has not
    // taken the parked pulse after two frames
    bool begin() override;

    // moves the servo to one of its two end positions
    void moveTo(Position target);

    // gets the end position last moved to
    Position position() const;
};
```
