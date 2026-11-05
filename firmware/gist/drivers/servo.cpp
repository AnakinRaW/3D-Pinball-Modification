#include <PWMServoT4.h>   // this repository's copy of PWMServo, pwmservo-t4.cpp

class ServoDriver : public Driver {
public:
    enum class Position : uint8_t { Start, End };

    // registers with the device monitor, attaches its step to the driver tick, attaches the pin
    // with the SER0049's range, 500 µs to 2500 µs, sets no pulse, sends the servo at full speed
    // to the middle between its end positions and from there slowly to its start position; false
    // when the device monitor or the driver tick is full or the timer has not taken the no-pulse
    // setting after two frames
    bool begin() override {
        if (!deviceMonitor.watch(Device::Servo, *this)) return false;    // before the wait, so a timer that never takes the pulse is reported
        if (!driverTick.attach([this] { step(); }, kFrameMs * 1000)) return false;
        if (!servo_.attach(kPin, 500, 2500)) { stuck_ = true; return false; }   // the pin stays low, so the servo gets no pulse
        startDeg_ = uint8_t(config.get(ConfigKey::kServoStart, kStartDeg, 0, kAngleLimit));
        endDeg_   = uint8_t(config.get(ConfigKey::kServoEnd, kEndDeg, 0, kAngleLimit));
        stepMdeg_ = uint32_t(config.get(ConfigKey::kServoSpeed, kSpeedDegS, kSpeedMin, kSpeedMax)) * kFrameMs;

        // The servo has no feedback line, so after power-up its position is unknown, and a servo
        // runs at full speed to whatever its first pulse points at. No ramp can soften that first
        // move, because a ramp needs a starting point. Pointing the first pulse at the middle
        // keeps the fast move to half the distance between the end positions at most.
        nowDeg_    = uint8_t((startDeg_ + endDeg_) / 2);
        targetDeg_ = nowDeg_;
        servo_.write(nowDeg_);
        holdFrames_ = kHoldMs / kFrameMs;
        ready_ = true;
        moveTo(Position::Start);
        return true;
    }

    // moves the servo at the config's speed to one of its two end positions; nothing once begin()
    // has failed
    void moveTo(Position target) {
        if (!ready_) return;
        targetDeg_ = target == Position::Start ? startDeg_ : endDeg_;   // step() walks the pulse there
        position_ = target;
    }

    // gets the end position last moved to
    Position position() const { return position_; }

    // gets the servo as failed once its timer has not taken the no-pulse setting in attach()
    Fault failed() const override { return {stuck_, 0}; }

private:
    static constexpr uint8_t  kPin   = 0;         // pin-assignment.md: Servo signal
    static constexpr uint8_t  kAngleLimit = 60;     // past it the servo bends the seesaw's bridge
    static constexpr uint8_t  kStartDeg = 0;        // servo.start_deg's default
    static constexpr uint8_t  kEndDeg   = 40;       // servo.end_deg's default
    static constexpr uint32_t kFrameMs = 20;        // the servo's frame, one step per frame
    static constexpr uint32_t kSpeedDegS = 50;      // servo.speed_deg_s's default, one degree per frame
    static constexpr uint32_t kSpeedMin  = 10;      // the slowest servo.speed_deg_s the config takes
    static constexpr uint32_t kSpeedMax  = 200;     // the fastest servo.speed_deg_s the config takes
    static constexpr uint32_t kHoldMs  = 500;       // how long the first pulse stays at the middle

    // Runs on the driver tick once per frame. The servo itself always runs at full speed to the
    // angle its pulse names, so the speed is limited by moving that angle a little each frame. A
    // speed below one degree per frame gathers its thousandths of a degree over several frames.
    void step() {
        if (!ready_) return;
        // The sheet states no speed, so the hold lets the fast move to the middle end first. A
        // servo still moving when it ends only meets the moving pulse early and follows it.
        if (holdFrames_) { --holdFrames_; return; }
        const uint8_t to = targetDeg_;
        if (nowDeg_ == to) { carryMdeg_ = 0; return; }
        carryMdeg_ += stepMdeg_;
        const uint32_t deg = carryMdeg_ / 1000;
        if (deg == 0) return;
        carryMdeg_ %= 1000;
        const uint8_t left = to > nowDeg_ ? to - nowDeg_ : nowDeg_ - to;
        if (deg >= left) nowDeg_ = to, carryMdeg_ = 0;             // the last step stops on the target
        else nowDeg_ = to > nowDeg_ ? nowDeg_ + deg : nowDeg_ - deg;
        servo_.write(nowDeg_);
    }

    PWMServoT4 servo_;
    volatile bool     ready_      = false;   // the timer took the no-pulse setting
    uint8_t           startDeg_   = 0;       // servo.start_deg from the config
    uint8_t           endDeg_     = 0;       // servo.end_deg from the config
    uint8_t           nowDeg_     = 0;       // the angle the pulse names, changed by step() alone
    volatile uint8_t  targetDeg_  = 0;       // the angle step() walks the pulse to
    uint32_t          stepMdeg_   = 0;       // servo.speed_deg_s as thousandths of a degree per frame
    uint32_t          carryMdeg_  = 0;       // thousandths of a degree gathered toward the next step
    uint32_t          holdFrames_ = 0;       // frames left at the middle after power-up
    bool              stuck_      = false;   // the timer did not take the no-pulse setting in attach()
    Position          position_   = Position::Start;
};
