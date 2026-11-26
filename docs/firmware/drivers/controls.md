# Controls driver

The driver watches the toggle switch that [`docs/parts/controls/design.md`](../../parts/controls/design.md) describes. It publishes an event whenever the switch is moved, and the game logic can ask for the switch's position at any time.

## Position

The driver reports the position printed on the switch, `1` or `0`. A low pin means the contact is closed, which the [controls design](../../parts/controls/design.md#toggle-switch) puts in position 1.

## Driver events

| Field | Content |
|---|---|
| Type | `DriverEventType::TOGGLE_SWITCHED` |
| Source | The switch, always `0` in this build |
| Timestamp | `nowUs()` at the edge that moved the contact |
| Payload | The new position, `1` or `0` |

The driver runs on a pin interrupt on both edges and publishes the event to the event queue. It publishes only a position that differs from the one it reported last. After every report, the driver ignores the pin for 50 ms, since the contact bounces.

`position()` reads the pin itself, so it gives the switch's current position.

## Device faults

The driver notes no fault.

## Driver

[`firmware/src/drivers/toggle-switch.h`](../../../firmware/src/drivers/toggle-switch.h) holds the interface.
