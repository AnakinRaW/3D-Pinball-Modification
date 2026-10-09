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
    static constexpr uint8_t  kPin     = 32;     // pin-assignment.md: Toggle switch
    static constexpr uint8_t  kClosed  = 1;      // the position that closes the contact
    static constexpr uint32_t kQuietUs = 50000;  // edges this soon after a report are bounce

    void edge() {
        const uint32_t now = nowUs();
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
