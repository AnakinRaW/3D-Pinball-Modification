class SolenoidDriver : public Driver {
public:
    // attaches the queue, attaches the release to the driver tick and registers with the
    // device monitor; false when the driver tick or the device monitor is full, and the
    // driver then never energises a coil
    bool begin() override {
        out_  = &events.attach(queue_, kQueueDepth);
        self_ = this;
        for (uint8_t c = 0; c < kCoils; ++c) {
            pinMode(kTrigger[c], OUTPUT);
            digitalWriteFast(kTrigger[c], LOW);
        }
        if (!driverTick.attach([this] { release(); }, DriverTick::kPeriodUs)) return false;
        if (!deviceMonitor.watch(Device::Solenoids, *this)) return false;
        ready_ = true;

        // every coil pulls once, one after another, before the contacts are watched; they stay
        // disarmed until arm()
        for (uint8_t c = 0; c < kCoils; ++c) {
            fire(c);
            delayMicroseconds(kOnUs + kCoolUs);
        }

        for (uint8_t c = 0; c < kSenses; ++c) pinMode(kSense[c], INPUT);

        attachInterrupt(digitalPinToInterrupt(kSense[0]), [] { self_->edge(0); }, CHANGE);
        attachInterrupt(digitalPinToInterrupt(kSense[1]), [] { self_->edge(1); }, CHANGE);
        attachInterrupt(digitalPinToInterrupt(kSense[2]), [] { self_->edge(2); }, CHANGE);
        return true;
    }

    // the game logic's own path, and the only one the unsensed solenoid has. The
    // pin interrupts start pulls as well, so every interrupt, the IR driver's
    // included, waits the few instructions start() takes
    bool fire(uint8_t coil) {
        noInterrupts();
        const bool started = start(coil, nowUs());
        interrupts();
        return started;
    }

    // arms the contacts of the sensed solenoids, or disarms them with false; a disarmed contact
    // neither fires its coil nor publishes a hit. Disarmed after begin()
    void arm(bool armed) { armed_ = armed; }

    // gets the coils that have run more than kLateUs past their pull-in, one bit each; the main
    // loop feeds the watchdog only while this is 0
    uint8_t overdue() const {
        uint8_t coils = 0;
        for (uint8_t c = 0; c < kCoils; ++c) {
            if (pulling(c) && elapsedUs(since_[c], kOnUs + kLateUs)) coils |= 1u << c;
        }
        return coils;
    }

    // the state the game logic may query between events
    bool ballOn(uint8_t solenoid) const {
        return digitalReadFast(kSense[solenoid]) == HIGH;
    }

    // gets which sensed solenoids have failed, one bit each: a contact closed for kFailMs; no error code
    Fault failed() const override {
        PartMask parts = 0;
        for (uint8_t s = 0; s < kSenses; ++s) {
            if (ballOn(s) && elapsedMs(closedAt_[s], kFailMs)) parts |= 1u << s;
        }
        return {parts, 0};
    }

private:
    static constexpr uint8_t  kCoils    = 4;
    static constexpr uint8_t  kSenses   = 3;
    static constexpr uint8_t  kTrigger[kCoils] = {40, 41, 30, 37};  // pin-assignment.md: Solenoid trigger 1 to 4
    static constexpr uint8_t  kSense[kSenses]  = {36, 35, 34};      // pin-assignment.md: Solenoid sense 1 to 3
    static constexpr uint32_t kOnUs     = 50000;  // the longest pull-in, docs/parts/solenoid
    static constexpr uint32_t kReleaseUs = kOnUs - DriverTick::kPeriodUs;   // 45 ms, from here the tick ends a pull
    static constexpr uint32_t kCoolUs   = 10000;  // the cool-down, and how long a contact stays open
    static constexpr uint32_t kLateUs   = 1000;   // past the pull-in, a coil counts as overdue
    static constexpr uint32_t kFailMs   = 2000;   // 2 s, a contact closed this long has failed

    // both edges of a sense line arrive here; a closing edge fires only once the
    // contact has been open for the cool-down, so neither bounce nor a contact
    // held closed fires again
    void edge(uint8_t solenoid) {
        const uint32_t now = nowUs();
        if (digitalReadFast(kSense[solenoid]) == LOW) {
            opened_[solenoid] = now;              // the contact just let go
            return;
        }
        closedAt_[solenoid] = nowUs();         // the note failed() reads
        if (!armed_ || !elapsedUs(opened_[solenoid], kCoolUs) || !start(solenoid, now)) return;
        out_->publish(DriverEvent{now, DriverEventType::SolenoidHit, solenoid, 0});
    }

    // gets whether a coil pulls, read back from its trigger pin's output register
    bool pulling(uint8_t coil) const {
        return *portOutputRegister(kTrigger[coil]) & digitalPinToBitMask(kTrigger[coil]);
    }

    // starts a pull; since_ is written before the pin goes high, so the tick never
    // finds a pulling coil with an old start time
    bool start(uint8_t coil, uint32_t now) {
        if (!ready_ || pulling(coil) || !elapsedUs(released_[coil], kCoolUs)) return false;
        since_[coil] = now;
        digitalWriteFast(kTrigger[coil], HIGH);
        return true;
    }

    // ends a pull; released_ is written before the pin goes low, so a start never
    // finds a free coil with an old release time
    void stop(uint8_t coil, uint32_t now) {
        released_[coil] = now;
        digitalWriteFast(kTrigger[coil], LOW);
    }

    // the driver tick, every 5 ms: stops every coil that has pulled for kReleaseUs,
    // so a pull lasts from kReleaseUs up to kOnUs
    void release() {
        for (uint8_t c = 0; c < kCoils; ++c) {
            if (pulling(c) && elapsedUs(since_[c], kReleaseUs)) stop(c, nowUs());
        }
    }

    static SolenoidDriver* self_;
    DriverEvent            queue_[kQueueDepth];
    EventQueue::Producer*  out_ = nullptr;
    volatile bool          ready_ = false;
    volatile bool          armed_ = false;      // whether a closing contact fires its coil
    volatile uint32_t      since_[kCoils]       = {};
    volatile uint32_t      released_[kCoils]    = {};
    volatile uint32_t      opened_[kSenses]     = {};
    volatile uint32_t      closedAt_[kSenses] = {};   // when the contact last closed
};
