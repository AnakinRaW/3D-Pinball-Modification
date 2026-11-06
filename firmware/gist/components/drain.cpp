// the drain with its break beam, the ball store, the ball separator and the drain lamp
class Drain : public Component {
public:
    // the store's three IR channels, as ir.channels in the config numbers them
    struct Channels { uint8_t front, back, exit; };

    Drain(ServoDriver& servo, IrSensing& ir, Lamps& lamps, Channels channels)
        : servo_(servo), ir_(ir), lamps_(lamps), ch_(channels) {}

    // turns BallDrained into Drained, a store channel's change into BallStored once stored() rises
    // above the counted number, and the exit channel's ball during a release into BallReleased
    bool translate(const DriverEvent& in, PinballEvents& out) override {
        out = {};
        if (in.type == DriverEventType::BallDrained) {
            return emit(out, {in.time, EventType::Drained, 0, 0});
        }
        if (in.type != DriverEventType::Detected && in.type != DriverEventType::Released) return false;
        if (in.source == ch_.front || in.source == ch_.back) {
            if (!counting_) return true;                   // the first update() sets the counted number
            const uint8_t n = stored();
            uint8_t made = 0;
            while (counted_ < n && made < kMaxEvents) {    // one BallStored for each ball more
                events_[made++] = {in.time, EventType::BallStored, 0, ++counted_};
            }
            out = {events_, made};
            return true;
        }
        if (in.source == ch_.exit) {
            if (in.type != DriverEventType::Detected || phase_ != Phase::Opening) return true;   // no release waits for it
            if (counted_ > 0) --counted_;
            close(in.time);
            return emit(out, {in.time, EventType::BallReleased, 0, stored()});
        }
        return false;                                      // another component's channel
    }

    // sets the counted number in the first pass of the main loop, ends a release once the servo is
    // back, and makes ReleaseFailed once exit has reported no ball in time
    bool update(uint32_t now, PinballEvents& out) override {
        out = {};
        if (!counting_) {                                  // start-up, with every coil's first pull, is over
            counted_  = stored();
            counting_ = true;
        }
        if (phase_ == Phase::Closing && elapsedMs(since_, servo_.travelMs())) phase_ = Phase::Idle;
        if (phase_ == Phase::Opening && elapsedMs(since_, servo_.travelMs() + kRollMs)) {
            close(now);
            return emit(out, {now, EventType::ReleaseFailed, 0, 0});
        }
        return false;
    }

    // gets the balls front and back hold at this moment
    uint8_t stored() const {
        return uint8_t(ballOn(ch_.front)) + uint8_t(ballOn(ch_.back));
    }

    // releases one ball: the servo to its end position, and back once exit reports the ball;
    // false while a release runs or the store is empty
    bool release() {
        if (phase_ != Phase::Idle || stored() == 0) return false;
        servo_.moveTo(ServoDriver::Position::End);
        phase_ = Phase::Opening;
        since_ = nowUs();
        return true;
    }

    // gets whether a release runs, the servo's return to its start position included
    bool releasing() const { return phase_ != Phase::Idle; }

    // gets the drain lamp for the key and priority of the game's mode
    Lamp lamp(uint8_t key, uint8_t priority) { return lamps_.groups(key, priority).drain; }

private:
    enum class Phase : uint8_t { Idle, Opening, Closing };

    static constexpr uint32_t kRollMs    = 2000;   // past the servo's travel, for the ball to reach exit
    static constexpr uint8_t  kMaxEvents = 2;      // two balls arriving together

    bool ballOn(uint8_t channel) const {
        uint32_t ms;
        return ir_.getState(channel, ms) == IrSensing::State::Ball;
    }

    void close(uint32_t now) {
        servo_.moveTo(ServoDriver::Position::Start);
        phase_ = Phase::Closing;
        since_ = now;
    }

    bool emit(PinballEvents& out, const PinballEvent& e) {
        events_[0] = e;
        out = {events_, 1};
        return true;
    }

    ServoDriver& servo_;
    IrSensing&   ir_;
    Lamps&       lamps_;
    Channels     ch_;

    Phase        phase_    = Phase::Idle;
    uint32_t     since_    = 0;        // nowUs() when the servo last set off
    uint8_t      counted_  = 0;        // the balls counted into the store, less those released
    bool         counting_ = false;    // the first update() has set counted_
    PinballEvent events_[kMaxEvents];
};
