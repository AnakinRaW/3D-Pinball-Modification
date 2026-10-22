class ContactDriver : public Driver {
public:
    // attaches the queue and the pins E-1 and E-2 to the port expander and registers with the device
    // monitor; false when the port expander refuses the pins or the device monitor is full
    bool begin() override {
        out_ = &events.attach(queue_, kQueueDepth);
        const uint32_t now = nowUs();
        for (uint32_t& t : openedAt_) t = now;               // an open contact counts as open since begin()
        if (!io::expander.attach(kPins, [this](uint8_t changed, uint8_t levels, uint32_t time) {
                change(changed, levels, time);
            })) return false;
        return deviceMonitor.watch(Device::Contacts, *this);
    }

    // gets the pins whose contact is closed, bit 0 for E-1 and bit 1 for E-2
    uint8_t closed() const { return (uint8_t)~io::expander.levels() & kPins; }

    // gets which parts have failed, bit 0 once no read of the port expander has worked for 1 s,
    // with the controller's error flags of the last failed read as the error code
    Fault failed() const override {
        return {elapsedMs(io::expander.lastRead(), kFailMs), io::expander.errors()};
    }

private:
    static constexpr uint8_t  kPins    = 0x03;      // E-1 and E-2, the pins with a contact
    static constexpr uint32_t kQuietUs = 10000;     // 10 ms, how long a contact stays open before its closure counts
    static constexpr uint32_t kFailMs  = 1000;      // 1 s without a working read, and the contacts have failed

    // a change on the pins, in the bus's interrupt: publishes every closure after kQuietUs of open contact
    void change(uint8_t changed, uint8_t levels, uint32_t time) {
        for (uint8_t i = 0; i < io::PortExpander::kPins; i++) {
            const uint8_t bit = 1 << i;
            if (!(changed & bit)) continue;
            if (levels & bit) openedAt_[i] = time;          // the contact opened
            else if (time - openedAt_[i] >= kQuietUs)       // closed after a quiet time, not a bounce
                out_->publish(DriverEvent{time, DriverEventType::ContactClosed, i, 0});
        }
    }

    DriverEvent           queue_[kQueueDepth];
    EventQueue::Producer* out_ = nullptr;
    uint32_t              openedAt_[io::PortExpander::kPins] = {};   // when each contact last opened
};
