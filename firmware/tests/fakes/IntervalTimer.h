// A fake of the Teensy core's IntervalTimer for the tests that run on the computer. It notes how
// it was started, and a test runs its callback with fire(), one call for each period.
#pragma once
#include <Arduino.h>
#include <functional>

namespace teensy {
template <typename Signature, size_t Capacity> using inplace_function = std::function<Signature>;
}

class IntervalTimer;

/// What the fake timers have been asked. A test resets it before it starts.
struct FakeTimers {
    bool refuse = false;                // begin() returns false while it is set
    uint32_t begins = 0;                // calls to begin(), refused ones included
    IntervalTimer* running = nullptr;   // the timer begin() last started
    uint32_t periodUs = 0;              // the period it was started with
    uint8_t priority = 0;               // the priority it was given
};
inline FakeTimers fakeTimers;

class IntervalTimer {
public:
    bool begin(std::function<void()> fn, uint32_t periodUs) {
        fakeTimers.begins = fakeTimers.begins + 1;
        if (fakeTimers.refuse) {
            return false;
        }
        callback = fn;
        fakeTimers.running = this;
        fakeTimers.periodUs = periodUs;
        return true;
    }

    void priority(uint8_t n) { fakeTimers.priority = n; }

    /// Runs the callback once, as one period of the hardware timer would.
    void fire() { callback(); }

private:
    std::function<void()> callback;
};
