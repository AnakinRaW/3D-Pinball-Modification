// the interface every driver implements
class Driver {
public:
    // sets the driver up; false when that fails
    virtual bool begin() = 0;

    // gets which parts of the driver's device have failed; the default reports none, for a
    // driver that notes no fault
    virtual Fault failed() const { return {}; }
};

// one IntervalTimer, shared by every driver that needs a regular period
class DriverTick {
public:
    using Callback = teensy::inplace_function<void(void), 16>;   // a lambda capturing this fits

    static constexpr uint32_t kPeriodUs = 5000;    // tick period

    // runs fn every periodUs; before the main loop only, and the first call starts the timer. False
    // when every slot is taken, periodUs is no multiple of kPeriodUs or no IntervalTimer is free
    bool attach(Callback fn, uint32_t periodUs) {
        const uint32_t every = periodUs / kPeriodUs;
        if (count_ == kSlots || every == 0 || periodUs % kPeriodUs != 0) return false;
        if (count_ == 0) {
            timer_.priority(kPriority);
            if (!timer_.begin([this] { tick(); }, kPeriodUs)) return false;
        }
        noInterrupts();                            // the tick must not see a half-written slot
        slots_[count_] = Slot{fn, every, every};
        count_ = count_ + 1;
        interrupts();
        return true;
    }

private:
    static constexpr uint8_t kSlots    = 8;
    static constexpr uint8_t kPriority = 96;       // the driver tick's row in general-design.md

    struct Slot { Callback fn; uint32_t every; uint32_t left; };

    // the IntervalTimer: counts every slot down and runs the ones that are due
    void tick() {
        for (uint8_t i = 0; i < count_; ++i) {
            if (--slots_[i].left != 0) continue;
            slots_[i].left = slots_[i].every;
            slots_[i].fn();
        }
    }

    IntervalTimer    timer_;
    Slot             slots_[kSlots];
    volatile uint8_t count_ = 0;
};

extern DriverTick driverTick;   // the one driver tick, defined in the main file
