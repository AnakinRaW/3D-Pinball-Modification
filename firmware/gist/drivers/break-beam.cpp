class BreakBeamDriver : public Driver {
public:
    // attaches the queue, registers with the device monitor and arms the pin interrupt;
    // false when the device monitor is full, and the driver then does nothing
    bool begin() override {
        out_  = &events.attach(queue_, kQueueDepth);
        self_ = this;
        if (!deviceMonitor.watch(Device::BreakBeam, *this)) return false;
        pinMode(kPin, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(kPin), [] { self_->edge(); }, CHANGE);
        return true;
    }

    // the state the game logic may query between events
    bool blocked() const { return digitalReadFast(kPin) == LOW; }

    // gets which parts have failed: bit 0 once the beam has stayed broken for kFailMs
    Fault failed() const override { return {blocked() && elapsedMs(brokenAt_, kFailMs), 0}; }

private:
    static constexpr uint8_t  kPin    = 31;      // pin-assignment.md: Receiver output of the ball drain gate
    static constexpr uint8_t  kSource = 0;       // the one gate this build has
    static constexpr uint32_t kDeadUs = 10000;   // 10 ms, a beam clear for less is still the same crossing
    static constexpr uint32_t kFailMs = 5000;    // 5 s, a beam broken this long has failed

    void edge() {
        const uint32_t now = nowUs();
        if (!blocked()) {                         // the beam cleared
            clearedAt_ = now;
            return;
        }
        brokenAt_ = nowUs();                   // the note failed() reads
        if (!elapsedUs(clearedAt_, kDeadUs)) return;   // clear too briefly: still the same crossing
        out_->publish(DriverEvent{now, DriverEventType::BallDrained, kSource, 0});
    }

    static BreakBeamDriver* self_;
    DriverEvent             queue_[kQueueDepth];
    EventQueue::Producer*   out_        = nullptr;
    volatile uint32_t       clearedAt_  = 0;     // when the beam last cleared
    volatile uint32_t       brokenAt_ = 0;     // when the beam last broke
};
