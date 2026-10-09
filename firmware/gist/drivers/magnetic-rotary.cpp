// what the AS5600 said about the magnet at begin()
struct MagnetState {
    bool    detected;    // STATUS MD: a magnet is there
    bool    tooWeak;     // STATUS ML: the gain is at its maximum, the magnet is too far away
    bool    tooStrong;   // STATUS MH: the gain is at its minimum, the magnet is too close
    uint8_t agc;         // the gain the AS5600 settled on, mid-range at the right gap
};

class MagneticRotaryDriver : public Driver {
public:
    // attaches the queue, sets up Wire, reads STATUS, AGC and the first angle, points
    // the AS5600 at RAW ANGLE, attaches its read to the driver tick and registers with
    // the device monitor; false when the AS5600 does not answer or either of the two is full
    bool begin() override {
        out_ = &events.attach(queue_, kQueueDepth);
        Wire.begin();                                        // the pins, their pull-ups and 100 kHz

        // reads n bytes from the register reg, high byte first; -1 when the AS5600 does not answer
        auto readRegister = [](uint8_t reg, uint8_t n) -> int32_t {
            Wire.beginTransmission(kAddress);
            Wire.write(reg);
            if (Wire.endTransmission() != 0 || Wire.requestFrom(kAddress, n) != n) return -1;
            int32_t value = 0;
            while (Wire.available()) value = (value << 8) | Wire.read();
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
        lastGood_       = nowUs();                        // the first working read

        if (!driverTick.attach([this] { tick(); }, DriverTick::kPeriodUs)) return false;   // tick() every 5 ms from here on
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
    static constexpr int32_t  kStill    = 4;      // ticks without a degree of movement
    static constexpr uint32_t kFailMs   = 1000;   // 1 s without a working read, and the sensor has failed
    static constexpr uint8_t  kSda      = 18;   // pin-assignment.md: SDA to the rotary sensors
    static constexpr uint8_t  kScl      = 19;   // pin-assignment.md: SCL to the rotary sensors
    static constexpr uint32_t kErrors   = LPI2C_MSR_NDF | LPI2C_MSR_ALF | LPI2C_MSR_FEF | LPI2C_MSR_PLTF;

    // the driver tick: collect the read the last tick started, start the next
    void tick() {
        const uint32_t msr = LPI2C1_MSR;
        if (msr & kErrors) {                                 // no answer, lost bus, FIFO error, line low
            lastError_ = msr & kErrors;
            if (msr & LPI2C_MSR_PLTF) recover();
            LPI2C1_MCR |= LPI2C_MCR_RTF | LPI2C_MCR_RRF;     // drop the read
            LPI2C1_MSR  = 0x00007F00;                        // clear every flag, as Wire does
        } else if (msr & LPI2C_MSR_MBF) {
            return;                                          // the last read is still on the bus
        } else if (((LPI2C1_MFSR >> 16) & 0x07) >= 2) {      // the two bytes of RAW ANGLE
            const uint32_t high = LPI2C1_MRDR & 0x0F;
            const uint32_t low  = LPI2C1_MRDR & 0xFF;
            const int32_t  raw  = (high << 8) | low;
            lastGood_ = nowUs();                          // the note failed() reads
            const int32_t  step = (raw - last_ + kSteps + kSteps / 2) % kSteps - kSteps / 2;
            last_      = raw;
            position_ += (uint32_t)step;                   // wraps without harm
            const int32_t moved = (int32_t)(position_ - published_);
            if (moved >= kDeadband || moved <= -kDeadband) {
                if (out_->publish(DriverEvent{nowUs(), DriverEventType::RotorMoved, 0, position_})) {
                    published_ = position_;                 // a full queue: the next tick tries again
                    moving_    = true;
                    still_     = 0;
                }
            } else if (moving_ && ++still_ >= kStill) {     // the rod stands still
                if (out_->publish(DriverEvent{nowUs(), DriverEventType::RotorStopped, 0, position_})) {
                    published_ = position_;
                    moving_    = false;
                }
            }
        }
        LPI2C1_MTDR = LPI2C_MTDR_CMD_START | (kAddress << 1) | 1;   // the address, to read
        LPI2C1_MTDR = LPI2C_MTDR_CMD_RECEIVE | (2 - 1);             // two bytes
        LPI2C1_MTDR = LPI2C_MTDR_CMD_STOP;
    }

    // clocks SCL until the device lets go of SDA, as Wire's force_clock() does
    static void recover() {
        for (const uint8_t pin : {kSda, kScl}) {
            *portConfigRegister(pin) = 5 | 0x10;             // GPIO, released high
            *portSetRegister(pin)    = digitalPinToBitMask(pin);
            *portModeRegister(pin)  |= digitalPinToBitMask(pin);
        }
        delayMicroseconds(10);
        for (int i = 0; i < 9 && !(digitalReadFast(kSda) && digitalReadFast(kScl)); i++) {
            digitalWriteFast(kScl, LOW);
            delayMicroseconds(5);
            digitalWriteFast(kScl, HIGH);
            delayMicroseconds(5);
        }
        for (const uint8_t pin : {kSda, kScl}) *portConfigRegister(pin) = 3 | 0x10;   // back to LPI2C1
    }

    DriverEvent                  queue_[kQueueDepth];
    EventQueue::Producer*        out_       = nullptr;
    int32_t                      last_      = 0;       // the first angle, set by begin()
    volatile uint32_t            position_  = 0;
    uint32_t                     published_ = 0;       // the position the last event carried
    bool                         moving_    = false;   // a RotorMoved has come since the last RotorStopped
    int32_t                      still_     = 0;       // ticks since the last RotorMoved
    volatile uint32_t            lastGood_ = 0;      // when a read last worked
    volatile uint32_t            lastError_  = 0;      // the error flags of the last failed read
    MagnetState                  magnet_{};
};
