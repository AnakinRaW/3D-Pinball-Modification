#include <PWMServo.h>

class ServoDriver : public Driver {
public:
    enum class Position : uint8_t { Start, End };

    // registers with the device monitor, attaches the pin with the SER0049's range, 500 µs to
    // 2500 µs, parks the pulse and moves the servo to its start position; false when the device
    // monitor is full or the timer has not taken the parked pulse after two frames
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
        ready_ = true;
        moveTo(Position::Start);
        return true;
    }

    // moves the servo to one of its two end positions
    void moveTo(Position target) {
        if (!ready_) return;
        servo_.write(target == Position::Start ? kStart : kEnd);
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
    static constexpr uint8_t  kStart = TODO;      // the start position in degrees, set on the machine
    static constexpr uint8_t  kEnd   = TODO;      // the end position in degrees, set on the machine

    PWMServo servo_;
    bool     ready_    = false;
    bool     stuck_    = false;   // the timer did not take the parked pulse in begin()
    Position position_ = Position::Start;
};
