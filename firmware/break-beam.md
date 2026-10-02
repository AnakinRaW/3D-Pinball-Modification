# Break beam driver

The driver watches the break beam sensor at the ball drain and publishes an event for every ball that passes it.

## Ball Sensing

The driver reports the ball at the moment the IR beam breaks. It runs on a pin interrupt on the falling edge of the receiver's output and publishes the event from the handler. Falling edges within 10 ms of a published event are discarded because they are most likely caused flickering when the ball enters.

## Driver events

| Field | Content |
|---|---|
| Type | `DriverEventType::BallDrained` |
| Source | The gate's index, always `0` in this build |
| Timestamp | `micros()` in the handler, the moment the beam went |
| Payload | None |

## Limitations

**How many balls one interruption holds.** Two balls touching each other break the beam once and arrive as one event. Telling them apart would need the ball's speed, and nothing at the gate measures it.

## Device faults

The beam counts as failed once it has stayed broken for 5 s, which no passing ball does. A dead emitter, a receiver knocked out of line or a ball stuck in the gate cause it. The falling edge notes the time the beam broke. [`failed()`](error-handling.md#device-faults) then checks whether the beam is still broken 5 s later.

## The driver

```cpp
class BreakBeamDriver : public Driver {
public:
    // attaches the queue, registers with the device monitor and arms the pin interrupt;
    // false when the device monitor is full, and the driver then does nothing
    bool begin() override {
        out_  = &events.attach(queue_, kQueueDepth);
        self_ = this;
        if (!deviceMonitor.watch(Device::BreakBeam, *this)) return false;
        pinMode(kPin, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(kPin), [] { self_->edge(); }, FALLING);
        return true;
    }

    // the state the game logic may query between events
    bool blocked() const { return digitalReadFast(kPin) == LOW; }

    // gets which parts have failed: bit 0 once the beam has stayed broken for kFailMs
    Fault failed() const override { return {blocked() && elapsedMs(brokenAtMs_, kFailMs), 0}; }

private:
    static constexpr uint8_t  kPin    = 40;      // pin-assignment.md: Receiver output of the ball drain gate
    static constexpr uint8_t  kSource = 0;       // the one gate this build has
    static constexpr uint32_t kDeadUs = 10000;   // 10 ms, edges inside it are one crossing
    static constexpr uint32_t kFailMs = 5000;    // 5 s, a beam broken this long has failed

    void edge() {
        const uint32_t now = micros();
        brokenAtMs_ = millis();                   // the note failed() reads
        if (!elapsedUs(last_, kDeadUs)) return;  // still the crossing just published
        last_ = now;
        out_->publish(DriverEvent{now, DriverEventType::BallDrained, kSource, 0});
    }

    static BreakBeamDriver* self_;
    DriverEvent             queue_[kQueueDepth];
    EventQueue::Producer*   out_        = nullptr;
    volatile uint32_t       last_       = 0;
    volatile uint32_t       brokenAtMs_ = 0;     // when the beam last broke
};
```