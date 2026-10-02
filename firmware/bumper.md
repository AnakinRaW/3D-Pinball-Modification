# Bumper driver

The driver watches the three top bumper contacts, fires the coil of whichever one a ball reached, and publishes the hit. A fourth channel, used for a scoop, drives a solenoid without a contact, which the game logic fires.

## Sensing and firing

The machine knows which top bumper to pull, when the ball closed circuit between the conductive foil and the specific shell. This pulls the channel's input to HIGH. The driver runs on a pin interrupt on both edges, raises the trigger pin from the handler when the line goes high, and publishes the event from there.

Firing the top bumpers belongs in the handler because a bumper has to answer the ball quickly and cannot wait for game logic to pick up the event. The event gets published to the event queue on pulling the solenoid.

All solenoids can also be fired by the game logic manually.

The game logic can only request a pull with `fire()`, which returns false while the coil is still on or cooling down. It cannot hold a coil on or release it directly.

At start-up `begin()` pulls every coil once, as the stock machine does at power-on. It pulls them one after another, so the supply carries a single coil at a time.

## Driver events

| Field | Content |
|---|---|
| Type | `DriverEventType::BumperHit` |
| Source | The bumper, `0` to `2` |
| Timestamp | `micros()` in the handler, the moment the contact closed |
| Payload | None |

## Solenoid protection

A rising edge on a sense line pulls its coil once, even when the contact stays closed. A released coil can fire again only after the cool-down, and a top bumper's contact has to have been open for that long as well.

| Rule | Value |
|---|---|
| Duration of a solenoid pull | ~50 ms |
| Duration of the cool-down phase after a solenoid pull | ~10 ms |

When a coil switches on, the driver notes the time. Every 5 ms the [driver tick](driver-design.md#driver-intervaltimer) checks how long each coil has been on and switches it off after 45 ms, so a pull lasts 45 to 50 ms. The tick also notes when it switched the coil off, and the cool-down counts from that time. 

The driver always writes the time first and switches the coil second. Whoever looks at a coil then finds the time that belongs to its state.

If the firmware itself fails, the [watchdog](error-handling.md#the-watchdog) resets the Teensy. The main loop feeds it only while `overdue()` reports that no coil has been on for longer than 51 ms.

At a firmware reset, the Teensy no longer drives the trigger pins, and the pull-down resistors on the board switch every coil off.

## Device faults

A top bumper counts as failed once its contact has stayed closed for 2 s. This might be caused by a ball resting against a shell or a sense line touching the foil. 

The closing edge notes the time. [`failed()`](error-handling.md#device-faults) then checks for each top bumper whether its contact is still closed 2 s later.

## The driver

```cpp
class BumperDriver : public Driver {
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
        if (!deviceMonitor.watch(Device::Bumpers, *this)) return false;
        ready_ = true;

        // every coil pulls once, one after another, before any contact is armed
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

    // the game logic's own path, and the only one the fourth coil has. The pin
    // interrupts start pulls as well, so every interrupt, the IR driver's
    // included, waits the few instructions start() takes
    bool fire(uint8_t coil) {
        noInterrupts();
        const bool started = start(coil, micros());
        interrupts();
        return started;
    }

    // true once a coil has run more than kLateUs past its pull-in; the main loop
    // feeds the watchdog only while this is false
    bool overdue() const {
        for (uint8_t c = 0; c < kCoils; ++c) {
            if (pulling(c) && elapsedUs(since_[c], kOnUs + kLateUs)) return true;
        }
        return false;
    }

    // the state the game logic may query between events
    bool ballOn(uint8_t bumper) const {
        return digitalReadFast(kSense[bumper]) == HIGH;
    }

    // gets which top bumpers have failed, one bit each: a contact closed for kFailMs; no error code
    Fault failed() const override {
        PartMask parts = 0;
        for (uint8_t b = 0; b < kSenses; ++b) {
            if (ballOn(b) && elapsedMs(closedAtMs_[b], kFailMs)) parts |= 1u << b;
        }
        return {parts, 0};
    }

private:
    static constexpr uint8_t  kCoils    = 4;
    static constexpr uint8_t  kSenses   = 3;
    static constexpr uint8_t  kTrigger[kCoils] = {32, 34, 35, 0};  // pin-assignment.md: Bumper trigger 1 to 4
    static constexpr uint8_t  kSense[kSenses]  = {1, 14, 15};      // pin-assignment.md: Bumper sense 1 to 3
    static constexpr uint32_t kOnUs     = 50000;  // the longest pull-in, docs/parts/bumper
    static constexpr uint32_t kReleaseUs = kOnUs - DriverTick::kPeriodUs;   // 45 ms, from here the tick ends a pull
    static constexpr uint32_t kCoolUs   = 10000;  // the cool-down, and how long a contact stays open
    static constexpr uint32_t kLateUs   = 1000;   // past the pull-in, a coil counts as overdue
    static constexpr uint32_t kFailMs   = 2000;   // 2 s, a contact closed this long has failed

    // both edges of a sense line arrive here; a closing edge fires only once the
    // contact has been open for the cool-down, so neither bounce nor a contact
    // held closed fires again
    void edge(uint8_t bumper) {
        const uint32_t now = micros();
        if (digitalReadFast(kSense[bumper]) == LOW) {
            opened_[bumper] = now;                // the contact just let go
            return;
        }
        closedAtMs_[bumper] = millis();           // the note failed() reads
        if (!elapsedUs(opened_[bumper], kCoolUs) || !start(bumper, now)) return;
        out_->publish(DriverEvent{now, DriverEventType::BumperHit, bumper, 0});
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
            if (pulling(c) && elapsedUs(since_[c], kReleaseUs)) stop(c, micros());
        }
    }

    static BumperDriver*  self_;
    DriverEvent           queue_[kQueueDepth];
    EventQueue::Producer* out_ = nullptr;
    volatile bool         ready_ = false;
    volatile uint32_t     since_[kCoils]       = {};
    volatile uint32_t     released_[kCoils]    = {};
    volatile uint32_t     opened_[kSenses]     = {};
    volatile uint32_t     closedAtMs_[kSenses] = {};   // when the contact last closed
};
```
