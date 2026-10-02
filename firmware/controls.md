# Controls driver

The driver watches the toggle switch that [`docs/parts/controls/design.md`](../docs/parts/controls/design.md) describes. It publishes an event whenever the switch is moved, and the game logic can ask for the switch's position at any time.

## Position

The driver reports the position printed on the switch, `1` or `0`. A low pin means the contact is closed, which the [controls design](../docs/parts/controls/design.md#toggle-switch) puts in position 1, and `kClosed` holds that position.

## Driver events

| Field | Content |
|---|---|
| Type | `DriverEventType::ToggleSwitched` |
| Source | The switch, always `0` in this build |
| Timestamp | `micros()` in the handler, the moment the contact moved |
| Payload | The new position, `1` or `0` |

The driver runs on a pin interrupt on both edges and publishes the event to the event queue. It publishes only a position that differs from the one it reported last. Edges within 50 ms of a report are bounce and are dropped.

`position()` reads the pin itself, so it gives the switch's current position.

## Device faults

The driver notes no fault.

## The driver

```cpp
class ToggleSwitch : public Driver {
public:
    // attaches the queue, sets the pin as an input with the Teensy's pull-up and puts both
    // edges on the pin interrupt; always true
    bool begin() override {
        out_  = &events.attach(queue_, kQueueDepth);
        self_ = this;
        pinMode(kPin, INPUT_PULLUP);
        reported_ = position();
        attachInterrupt(digitalPinToInterrupt(kPin), [] { self_->edge(); }, CHANGE);
        return true;
    }

    // the position printed on the switch, read from the pin
    uint8_t position() const {
        return digitalReadFast(kPin) == LOW ? kClosed : 1 - kClosed;
    }

private:
    static constexpr uint8_t  kPin     = 41;     // pin-assignment.md: Toggle switch
    static constexpr uint8_t  kClosed  = 1;      // the position that closes the contact
    static constexpr uint32_t kQuietUs = 50000;  // edges this soon after a report are bounce

    void edge() {
        const uint32_t now = micros();
        const uint8_t  pos = position();
        if (pos == reported_ || !elapsedUs(since_, kQuietUs)) return;
        reported_ = pos;
        since_    = now;
        out_->publish(DriverEvent{now, DriverEventType::ToggleSwitched, 0, pos});
    }

    static ToggleSwitch*  self_;
    DriverEvent           queue_[kQueueDepth];
    EventQueue::Producer* out_      = nullptr;
    volatile uint8_t      reported_ = 0;
    volatile uint32_t     since_    = 0;
};
```
