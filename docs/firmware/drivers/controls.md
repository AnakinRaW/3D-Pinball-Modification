# Controls driver

The driver watches the toggle switch that [`docs/parts/controls/design.md`](../../parts/controls/design.md) describes. It publishes an event whenever the switch is moved, and the game logic can ask for the switch's position at any time.

## Position

The driver reports the position printed on the switch, `1` or `0`. A low pin means the contact is closed, which the [controls design](../../parts/controls/design.md#toggle-switch) puts in position 1.

## Driver events

| Field | Content |
|---|---|
| Type | `DriverEventType::ToggleSwitched` |
| Source | The switch, always `0` in this build |
| Timestamp | `nowUs()` in the handler, the moment the contact moved |
| Payload | The new position, `1` or `0` |

The driver runs on a pin interrupt on both edges and publishes the event to the event queue. It publishes only a position that differs from the one it reported last. Edges within 50 ms of a report are bounce and are dropped.

`position()` reads the pin itself, so it gives the switch's current position.

## Device faults

The driver notes no fault.

## Driver

```cpp
class ToggleSwitch : public Driver {
public:
    // attaches the queue, sets the pin as an input with the Teensy's pull-up and puts both
    // edges on the pin interrupt; always true
    bool begin() override;

    // the position printed on the switch, read from the pin
    uint8_t position() const;
};
```
