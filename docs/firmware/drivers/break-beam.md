# Break beam driver

The driver watches the break beam sensor at the ball drain and publishes an event for every ball that passes it.

## Ball Sensing

The driver reports the ball at the moment the IR beam breaks. It runs on a pin interrupt on both edges of the receiver's output and publishes the event from the handler. It reports a break only when the beam had been clear for at least 10 ms before it, so the flicker at the beam's edge as a ball enters or leaves counts as one crossing.

## Driver events

| Field | Content |
|---|---|
| Type | `DriverEventType::BALL_DRAINED` |
| Source | The gate's index, always `0` in this build |
| Timestamp | `nowUs()` in the handler, the moment the beam went |
| Payload | None |

## Limitations

The break beam cannot tell apart two balls touching each other. This most likely counts as one event.

## Device faults

The beam counts as failed once it has stayed broken for 5 s, which no passing ball does. A dead emitter, a broken cable, a receiver knocked out of line or a ball stuck in the gate cause it.

## Driver

[`firmware/src/drivers/break-beam.h`](../../../firmware/src/drivers/break-beam.h) holds the interface.
