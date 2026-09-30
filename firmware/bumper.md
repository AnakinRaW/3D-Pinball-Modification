# Bumper driver

The driver watches the three top bumper contacts, fires the coil of whichever one a ball reached, and publishes the hit. A fourth channel, used for a scoop, drives a solenoid that has no contact of its own and is asked for by the game logic.

## Sensing and firing

The machine knows which top bumper to pull, when the ball closed circuit between the conductive foil and the specific shell. This pulls the channel's input to HIGH. The driver runs on a pin interrupt on both edges, raises the trigger pin from the handler when the line goes high, and publishes the event from there.

Firing the top bumpers belongs in the handler because a bumper has to answer the ball quickly and cannot wait for game logic to pick up the event. The event gets published to the event queue on pulling the solenoid.

All solenoids can also be fired by the game logic manually.

The game logic can only request a pull with `fire()`, which returns false while the coil is still on or cooling down. It cannot hold a coil on or release it directly.

At start-up `begin()` pulls every coil once, as the stock machine does at power-on. It pulls them one after another, so the supply carries a single coil at a time.

## The event

| Field | Content |
|---|---|
| Type | `DriverEventType::BumperHit` |
| Source | The bumper, `0` to `2` |
| Timestamp | `micros()` in the handler, the moment the contact closed |
| Payload | None |

## Solenoid protection

Every pull ends on its own, even when a top bumper's contact stays closed. A released coil can fire again only after the cool-down, and a top bumper's contact has to have been open for that long as well.

| Rule | Value |
|---|---|
| Duration of a solenoid pull | 50 ms |
| Duration of the cool-down phase after a solenoid pull | 10 ms |

One `IntervalTimer`, the release timer, ends every pull. It waits for the oldest running pull, switches off each coil that has had its 50 ms and then waits for the next, so no pull is ever cut short. `begin()` takes the timer once and never gives it back. Between pulls the timer is parked on a long period. If no `IntervalTimer` is free at start-up, `begin()` returns false.

If also the firmware fails an additional [watchdog](general-design.md#the-watchdog) steps in. The main loop feeds it only while `overdue()` reports that no coil has been on for longer than 51 ms.

At a firmware reset, the Teensy no longer drives the trigger pins, and the pull-down resistors on the board switch every coil off.

## The driver

```cpp
class BumperDriver {
public:
    // takes the release timer and keeps it; false when no IntervalTimer is
    // free, and the driver then never energises a coil
    bool begin(EventQueue::Producer& out) {
        out_  = &out;
        self_ = this;
        for (uint8_t c = 0; c < kCoils; ++c) {
            pinMode(kTrigger[c], OUTPUT);
            digitalWriteFast(kTrigger[c], LOW);
        }

        // the release timer and every pin interrupt share one level, so none of
        // them can interrupt another
        timer_.priority(kPriority);
        if (!timer_.begin(expire0, kIdleUs)) return false;
        ready_ = true;

        // every coil pulls once, one after another, before any contact is armed
        for (uint8_t c = 0; c < kCoils; ++c) {
            fire(c);
            delayMicroseconds(kOnUs + kCoolUs);
        }

        NVIC_SET_PRIORITY(IRQ_GPIO6789, kPriority);
        for (uint8_t c = 0; c < kSenses; ++c) {
            pinMode(kSense[c], INPUT);
            attachInterrupt(digitalPinToInterrupt(kSense[c]), kEdge[c], CHANGE);
        }
        return true;
    }

    // the game logic's own path, and the only one the fourth coil has. The
    // release timer and the pin interrupts change what pull() changes, so every
    // interrupt, the IR driver's included, waits the few instructions it takes
    bool fire(uint8_t coil) {
        noInterrupts();
        const bool started = pull(coil, micros());
        interrupts();
        return started;
    }

    // true once a coil has run more than kLateUs past its pull-in; the main loop
    // feeds the watchdog only while this is false
    bool overdue() const {
        const uint32_t now = micros();
        for (uint8_t c = 0; c < kCoils; ++c) {
            if (live_[c] && now - since_[c] > kOnUs + kLateUs) return true;
        }
        return false;
    }

    // the state the game logic may query between events
    bool ballOn(uint8_t bumper) const {
        return digitalReadFast(kSense[bumper]) == HIGH;
    }

private:
    static constexpr uint8_t  kCoils    = 4;
    static constexpr uint8_t  kSenses   = 3;
    static constexpr uint8_t  kTrigger[kCoils] = {32, 34, 35, 0};  // pin-assignment.md: Bumper trigger 1 to 4
    static constexpr uint8_t  kSense[kSenses]  = {1, 14, 15};      // pin-assignment.md: Bumper sense 1 to 3
    static constexpr uint32_t kOnUs     = 50000;  // the pull-in, docs/parts/bumper
    static constexpr uint32_t kCoolUs   = 10000;  // the cool-down, and how long a contact stays open
    static constexpr uint32_t kLateUs   = 1000;   // past the pull-in, a coil counts as overdue
    static constexpr uint32_t kIdleUs   = 100000000;  // parks the release timer between pulls
    static constexpr uint8_t  kPriority = 96;     // general-design.md

    // attachInterrupt and IntervalTimer take a plain function pointer, so a
    // static one per source hands the call to the instance
    static void edge0() { self_->edge(0); }
    static void edge1() { self_->edge(1); }
    static void edge2() { self_->edge(2); }
    static constexpr void (*kEdge[kSenses])() = {edge0, edge1, edge2};
    static void expire0() { self_->expire(); }

    // both edges of a sense line arrive here; a closing edge fires only once the
    // contact has been open for the cool-down, so neither bounce nor a contact
    // held closed fires again
    void edge(uint8_t bumper) {
        const uint32_t now = micros();
        if (digitalReadFast(kSense[bumper]) == LOW) {
            opened_[bumper] = now;                // the contact just let go
            return;
        }
        if (now - opened_[bumper] < kCoolUs || !pull(bumper, now)) return;
        out_->publish(DriverEvent{now, DriverEventType::BumperHit, bumper, 0});
    }

    // starts the pull-in; the release timer is set to it only when no other coil
    // is on, because a coil that started earlier is always due first
    bool pull(uint8_t coil, uint32_t now) {
        if (!ready_ || live_[coil] || now - released_[coil] < kCoolUs) return false;
        if (!timing_) {
            timer_.begin(expire0, kOnUs);         // restarts on the channel it holds
            timing_ = true;
        }
        live_[coil]  = true;
        since_[coil] = now;
        digitalWriteFast(kTrigger[coil], HIGH);
        return true;
    }

    // the release timer: switch off every coil that has been on for kOnUs, then
    // wait for the next coil that is due, or park until the next pull
    void expire() {
        const uint32_t now = micros();
        uint32_t next = 0;                        // what the oldest coil still on has left
        for (uint8_t c = 0; c < kCoils; ++c) {
            if (!live_[c]) continue;
            const uint32_t on = now - since_[c];
            if (on >= kOnUs) {
                digitalWriteFast(kTrigger[c], LOW);
                released_[c] = now;
                live_[c]     = false;
            } else if (next == 0 || kOnUs - on < next) {
                next = kOnUs - on;
            }
        }
        timing_ = next != 0;
        timer_.begin(expire0, timing_ ? next : kIdleUs);  // restarts on the channel it holds
    }

    IntervalTimer         timer_;
    static BumperDriver*  self_;
    EventQueue::Producer* out_ = nullptr;
    volatile bool         ready_  = false;
    volatile bool         timing_ = false;
    volatile bool         live_[kCoils]     = {};
    volatile uint32_t     since_[kCoils]    = {};
    volatile uint32_t     released_[kCoils] = {};
    volatile uint32_t     opened_[kSenses]  = {};
};
```
