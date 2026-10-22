// what the AS5600 said about the magnet at begin()
struct MagnetState {
    bool    detected;    // STATUS MD: a magnet is there
    bool    tooWeak;     // STATUS ML: the gain is at its maximum, the magnet is too far away
    bool    tooStrong;   // STATUS MH: the gain is at its minimum, the magnet is too close
    uint8_t agc;         // the gain the AS5600 settled on, mid-range at the right gap
};

class MagneticRotaryDriver : public Driver {
public:
    // attaches the queue and the transfers, reads STATUS, AGC and the first angle, points
    // the AS5600 at RAW ANGLE, attaches its read to the driver tick and registers with
    // the device monitor; false when the AS5600 does not answer or either of the two is full
    bool begin() override {
        out_ = &events.attach(queue_, kQueueDepth);
        io::wireBus.attach(setup_);
        io::wireBus.attach(read_);

        // reads n bytes from the register reg, high byte first; -1 when the AS5600 does not answer
        auto readRegister = [this](uint8_t reg, uint8_t n) -> int32_t {
            reg_ = reg;
            setup_.inCount = n;
            if (io::wireBus.requestAndWait(setup_) != 0) return -1;
            int32_t value = 0;
            for (uint8_t i = 0; i < n; i++) value = (value << 8) | bytes_[i];
            return value;
        };
        const int32_t status = readRegister(kStatus, 1);
        const int32_t agc    = readRegister(kAgc, 1);
        const int32_t raw    = readRegister(kRawAngle, 2);   // read last, so the pointer stays on RAW ANGLE
        if (status < 0 || agc < 0 || raw < 0) return false;

        magnet_.detected  = status & 0x20;                   // MD
        magnet_.tooWeak   = status & 0x10;                   // ML
        magnet_.tooStrong = status & 0x08;                   // MH
        magnet_.agc       = agc;
        last_             = raw & 0x0FFF;                    // the 12 bits of RAW ANGLE
        lastGood_         = nowUs();                         // the first working read

        if (!driverTick.attach([this] { io::wireBus.request(read_); }, DriverTick::kPeriodUs)) return false;   // a read every 5 ms from here on
        return deviceMonitor.watch(Device::RotarySensor, *this);
    }

    // steps since begin(), 4096 to a turn
    int32_t position() const { return (int32_t)position_; }

    // what STATUS and AGC said at begin(): magnet found, too weak, too strong
    MagnetState magnet() const { return magnet_; }

    // gets which parts have failed, bit 0 once no read has worked for kFailMs, with the
    // controller's error flags of the last failed read as the error code
    Fault failed() const override { return {elapsedMs(lastGood_, kFailMs), lastError_}; }

private:
    static constexpr uint8_t  kAddress  = 0x36;
    static constexpr uint8_t  kStatus   = 0x0B;
    static constexpr uint8_t  kAgc      = 0x1A;
    static constexpr uint8_t  kRawAngle = 0x0C;
    static constexpr int32_t  kSteps    = 4096;
    static constexpr int32_t  kDeadband = 12;
    static constexpr int32_t  kStill    = 4;      // reads, one each tick, without a degree of movement
    static constexpr uint32_t kFailMs   = 1000;   // 1 s without a working read, and the sensor has failed

    // the end of a read, in the bus's interrupt: adds the step since the last reading and publishes
    void done(uint32_t errors) {
        if (errors) {                                        // no answer, lost bus, FIFO error, line low
            lastError_ = errors;                             // the next tick requests a new read
            return;
        }
        const int32_t raw  = ((angle_[0] & 0x0F) << 8) | angle_[1];
        lastGood_ = nowUs();                                 // the note failed() reads
        const int32_t step = (raw - last_ + kSteps + kSteps / 2) % kSteps - kSteps / 2;
        last_      = raw;
        position_ += (uint32_t)step;                         // wraps without harm
        const int32_t moved = (int32_t)(position_ - published_);
        if (moved >= kDeadband || moved <= -kDeadband) {
            if (out_->publish(DriverEvent{nowUs(), DriverEventType::RotorMoved, 0, position_})) {
                published_ = position_;                       // a full queue: the next read tries again
                moving_    = true;
                still_     = 0;
            }
        } else if (moving_ && ++still_ >= kStill) {           // the rod stands still
            if (out_->publish(DriverEvent{nowUs(), DriverEventType::RotorStopped, 0, position_})) {
                published_ = position_;
                moving_    = false;
            }
        }
    }

    DriverEvent           queue_[kQueueDepth];
    EventQueue::Producer* out_       = nullptr;
    uint8_t               reg_       = 0;                    // the register setup_ points the AS5600 at
    uint8_t               bytes_[2]  = {};                   // what setup_ reads, RAW ANGLE's two bytes at most
    uint8_t               angle_[2]  = {};                   // RAW ANGLE, high byte first
    io::I2cTransfer       setup_{kAddress, &reg_, 1, bytes_, 1, {}};
    io::I2cTransfer       read_{kAddress, nullptr, 0, angle_, 2, [this](uint32_t e) { done(e); }};   // the AS5600 keeps its pointer on RAW ANGLE
    int32_t               last_      = 0;                    // the first angle, set by begin()
    volatile uint32_t     position_  = 0;
    uint32_t              published_ = 0;                    // the position the last event carried
    bool                  moving_    = false;                // a RotorMoved has come since the last RotorStopped
    int32_t               still_     = 0;                    // reads since the last RotorMoved
    volatile uint32_t     lastGood_  = 0;                    // when a read last worked
    volatile uint32_t     lastError_ = 0;                    // the error flags of the last failed read
    MagnetState           magnet_{};
};
