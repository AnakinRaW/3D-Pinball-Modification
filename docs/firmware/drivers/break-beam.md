# Break beam driver

The driver watches the break beam sensor at the ball drain and publishes an event for every ball that passes it.

## Ball Sensing

The driver reports the ball at the moment the IR beam breaks. It runs on a pin interrupt on both edges of the receiver's output and publishes the event from the handler. It reports a break only when the beam had been clear for at least 10 ms before it, so the flicker at the beam's edge as a ball enters or leaves counts as one crossing.

## Driver events

| Field | Content |
|---|---|
| Type | `DriverEventType::BallDrained` |
| Source | The gate's index, always `0` in this build |
| Timestamp | `nowUs()` in the handler, the moment the beam went |
| Payload | None |

## Limitations

The break beam cannot tell apart two balls touching each. This most likely counts as one event.

## Device faults

The beam counts as failed once it has stayed broken for 5 s, which no passing ball does. A dead emitter, a receiver knocked out of line or a ball stuck in the gate cause it.

## Driver

```cpp
class BreakBeamDriver : public Driver {
public:
    // attaches the queue, registers with the device monitor, sets the pin as an input with the
    // Teensy's pull-up and arms the pin interrupt; false when the device monitor is full, and
    // the driver then does nothing
    bool begin() override;

    // the state the game logic may query between events
    bool blocked() const;

    // gets which parts have failed: bit 0 once the beam has stayed broken for 5 s
    Fault failed() const override;
};
```
