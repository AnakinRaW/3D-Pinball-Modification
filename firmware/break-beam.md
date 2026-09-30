# Break beam driver

The driver watches the break beam sensor at the ball drain and publishes an event for every ball that passes it.

## Ball Sensing

The driver reports the ball at the moment the IR beam breaks. It runs on a pin interrupt on the falling edge of the receiver's output and publishes the event from the handler. Falling edges within 10 ms of a published event are discarded because they are most likely caused flickering when the ball enters.

## The event

| Field | Content |
|---|---|
| Type | `DriverEventType::BallDrained` |
| Source | The gate's index, always `0` in this build |
| Timestamp | `micros()` in the handler, the moment the beam went |
| Payload | None |

## Limitations

**How many balls one interruption holds.** Two balls touching each other break the beam once and arrive as one event. Telling them apart would need the ball's speed, and nothing at the gate measures it.

## The driver

```cpp
class BreakBeamDriver {
public:
    void begin(EventQueue::Producer& out) {
        out_ = &out;
        self_ = this;
        pinMode(kPin, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(kPin), trampoline, FALLING);
    }

    // the state the game logic may query between events
    bool blocked() const { return digitalReadFast(kPin) == LOW; }

private:
    static constexpr uint8_t  kPin    = 40;      // pin-assignment.md: Receiver output of the ball drain gate
    static constexpr uint8_t  kSource = 0;       // the one gate this build has
    static constexpr uint32_t kDeadUs = 10000;   // 10 ms, edges inside it are one crossing

    // attachInterrupt takes a plain function pointer, so a static one hands
    // the edge to the instance
    static void trampoline() { self_->edge(); }

    void edge() {
        const uint32_t now = micros();
        if (now - last_ < kDeadUs) return;       // still the crossing just published
        last_ = now;
        out_->publish(DriverEvent{now, DriverEventType::BallDrained, kSource, 0});
    }

    static BreakBeamDriver* self_;
    EventQueue::Producer*   out_  = nullptr;
    volatile uint32_t       last_ = 0;
};
```