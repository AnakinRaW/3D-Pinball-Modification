# Servo driver

The driver sets the angle of the servo that [`docs/parts/servo/design.md`](../../parts/servo/design.md) describes. It uses PWMServoT4, a library in this repository that copies [PWMServo](https://github.com/PaulStoffregen/PWMServo) for the Teensy 4 alone. [`firmware/gist/drivers/pwmservo-t4.cpp`](../../../firmware/gist/drivers/pwmservo-t4.cpp) holds it.

A timer repeats the servo pulse in every frame of 20 ms. A new pulse width written to the timer takes effect at the start of the next frame. Until then, the timer keeps the width it already holds. For the servo's pin, the Teensy core sets that width to the whole frame at boot. PWMServo's first `write()` connects the pin to the timer at once, so the servo first gets a pulse of up to 20 ms and drives to its stop. PWMServoT4's `attach()` therefore sets the timer to no pulse first and waits until the timer has taken that setting. The first pulse the servo gets is then the one the first `write()` names.

The library's header is called `PWMServoT4.h`. The compiler therefore cannot include the PWMServo that Teensyduino bundles in its place.

## Servo positions

The servo is used in the ball drain as part of the ball separator. It moves a small seesaw. It therefore has two well-known positions. The start position and the end position put the seesaw at its two extremes.

The driver knows the mechanism's two end positions, and `moveTo()` takes the servo to one of them at the speed the [config](#config) sets. Their angles stand in the config too.

## Config

The two angles stand in the [machine config](../components/config.md) in whole degrees, the speed in degrees per second:

| Key | Default | Range |
|---|---|---|
| `servo.start_deg` | 0 | 0 to 60 |
| `servo.end_deg` | 40 | 0 to 60 |
| `servo.speed_deg_s` | 50 | 10 to 200 |


## Driver events

The servo has no feedback line, so the driver cannot tell when a move has ended, and it publishes no event. `position()` gives the end position last moved to. A component that must know the servo has arrived waits a fixed travel time instead or uses other detection methods that imply the servo moved.

## PWMServo

The timer of pin [0](../../pin-assignment.md "Servo signal") is FlexPWM1.1. PWMServo sets it to a frame of 20 ms, and the timer repeats the pulse in every frame. Once per frame, the driver tick moves the pulse toward the target by what the speed covers in a frame. At the default, that is one degree per frame. A slower speed gathers over several frames until a whole degree is due. `moveTo()` only sets the target and returns at once.

`attach()` takes the SER0049's pulse range, 500 µs for 0° and 2500 µs for 180°.

## Start-up

If the timer has not taken the no-pulse setting after two frames, `attach()` returns false, and so does `begin()`. The servo then gets no pulse.

After power-up, the servo's position is unknown, and its first move runs at full speed whatever the pulse. The first pulse therefore points at the middle between the two end positions. The driver holds it there for 500 ms and then moves the servo to its start position at the config's speed. The SER0049's sheet states no speed. A servo that is still moving when the hold ends meets the moving pulse early and follows it from there.

## Device faults

The servo counts as failed when its timer has not taken the no-pulse setting in `attach()`. A move that fails stays unseen, since the servo has no feedback line.

## Driver Code

```cpp
class ServoDriver : public Driver {
public:
    enum class Position : uint8_t { Start, End };

    // registers with the device monitor, attaches its step to the driver tick, attaches the pin
    // with the SER0049's range, 500 µs to 2500 µs, sets no pulse, sends the servo at full speed
    // to the middle between its end positions and from there slowly to its start position; false
    // when the device monitor or the driver tick is full or the timer has not taken the no-pulse
    // setting after two frames
    bool begin() override;

    // moves the servo at the config's speed to one of its two end positions; nothing once begin()
    // has failed
    void moveTo(Position target);

    // gets the end position last moved to
    Position position() const;

    // gets how long a move from one end position to the other takes at the config's speed, in ms
    uint32_t travelMs() const;

    // gets the servo as failed once its timer has not taken the no-pulse setting in attach()
    Fault failed() const override;
};
```
