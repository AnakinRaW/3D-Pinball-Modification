#include <PWMServo.h>

class ServoDriver : public Driver {
public:
    enum class Position : uint8_t { Start, End };

    // registers with the device monitor, attaches the pin with the SER0049's range, 500 µs to
    // 2500 µs, parks the pulse and, once the config sets both angles, moves the servo to its start
    // position; false when the device monitor is full or the timer has not taken the parked pulse
    // after two frames
    bool begin() override {
        if (!deviceMonitor.watch(Device::Servo, *this)) return false;    // before the wait, so a timer that never takes the pulse is reported
        servo_.attach(kPin, 500, 2500);
        IMXRT_FLEXPWM1.MCTRL |= FLEXPWM_MCTRL_CLDOK(kMask);
        IMXRT_FLEXPWM1.SM[kSub].VAL0 = IMXRT_FLEXPWM1.SM[kSub].VAL1 + 1;   // beyond the frame
        IMXRT_FLEXPWM1.MCTRL |= FLEXPWM_MCTRL_LDOK(kMask);
        elapsedMillis waited;                                              // counts milliseconds up from 0
        while (IMXRT_FLEXPWM1.MCTRL & FLEXPWM_MCTRL_LDOK(kMask)) {         // until the timer takes it
            if (waited > kLoadTimeoutMs) { stuck_ = true; return false; }     // the pin stays low, so the servo gets no pulse
        }
        const auto start = config.get(ConfigKey::kServoStart, 0, kAngleMax);
        const auto end   = config.get(ConfigKey::kServoEnd, 0, kAngleMax);
        ready_ = start && end;                                // an unset angle leaves the pulse parked
        if (ready_) startDeg_ = uint8_t(*start), endDeg_ = uint8_t(*end);
        moveTo(Position::Start);
        return true;
    }

    // moves the servo to one of its two end positions; nothing while an angle is unset
    void moveTo(Position target) {
        if (!ready_) return;
        servo_.write(target == Position::Start ? startDeg_ : endDeg_);
        position_ = target;
    }

    // gets the end position last moved to
    Position position() const { return position_; }

    // gets the servo as failed once its timer has not taken the parked pulse in begin()
    Fault failed() const override { return {stuck_, 0}; }

private:
    static constexpr uint8_t  kPin   = 0;         // pin-assignment.md: Servo signal
    static constexpr uint8_t  kSub   = 1;         // FlexPWM1's submodule 1, behind the pin
    static constexpr uint16_t kMask  = 1 << kSub;
    static constexpr uint32_t kLoadTimeoutMs = 40;  // two frames of 20 ms
    static constexpr uint8_t  kAngleMax = 180;      // the SER0049's travel in degrees

    PWMServo servo_;
    bool     ready_    = false;   // the timer took the parked pulse and the config sets both angles
    uint8_t  startDeg_ = 0;       // servo.start_deg from the config
    uint8_t  endDeg_   = 0;       // servo.end_deg from the config
    bool     stuck_    = false;   // the timer did not take the parked pulse in begin()
    Position position_ = Position::Start;
};
