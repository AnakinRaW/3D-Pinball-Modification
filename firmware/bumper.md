# Bumper driver

The driver watches the three bumper contacts, fires the coil of whichever one a ball reached, and publishes the hit. A fourth channel drives a solenoid that has no contact of its own and is asked for by the game logic. The board these pins reach is described in [`docs/parts/bumper/design.md`](../docs/parts/bumper/design.md).

## Sensing and firing

The conductive foil sits at 3.3 V, so a ball bridging foil and shell pulls that channel's sense line high. The driver runs on a pin interrupt on both edges, raises the trigger pin from the handler when the line goes high, and publishes the event from there.

Firing belongs in the handler because a bumper has to answer the ball, not the loop. Publishing follows the rule in [`general-design.md`](general-design.md): what cannot wait stays in the subsystem, and the game logic hears about it through the queue.

## What bounds a pull-in

| Rule | Value |
|---|---|
| Pull-in the driver commands | 50 ms |
| How long the contact has to stay open before that channel fires again | 10 ms |

**A channel fires once per closed contact, however long it stays closed.** The rising edge fires the coil and disarms that channel. Only a contact that opens again, and stays open for 10 ms, arms it for another pull-in. A shell held against the foil therefore pulls in once and then does nothing, which is the one thing the driver has to guarantee.

**The pull-in ends in the driver's own interrupt.** An `IntervalTimer` started in `begin()` ticks every millisecond, drops the trigger pin of a coil that has run its time, and arms a channel whose contact has been open long enough. Nothing in the main loop is called for any of it, and a loop that stops running leaves no coil energised.

A pull-in therefore lasts the commanded 50 ms plus at most one tick, so 51 ms. A solenoid needs nothing better than that.

**A one-shot armed at each pull-in was rejected.** It would end the pull-in to the microsecond, but arming it means calling `IntervalTimer::begin()` from the sense interrupt, which allocates a hardware channel and is not documented as safe there. A tick that runs whatever happens costs one interrupt per millisecond and needs nothing undocumented.

**Nothing arbitrates between channels.** One or two balls on a playfield decide by themselves how many bumpers can be struck at once, so a lock in firmware would add a rule the machine already keeps, and it would cost a hit its score whenever two coils happened to coincide.

**The event goes out as the coil pulls.** The handler raises the trigger pin first and publishes immediately after, in that order because the kick is what the ball is waiting for and the queue is not.

## The fourth channel

The fourth trigger drives a solenoid with no sense line. The game logic calls `fire()` for it, which gives that coil the same single pull-in. Nothing arms or disarms it: a call while it is still energised is refused, and once it has released the next call fires it again.

## What the hardware depends on

**The watchdog is fed from the main loop and never from an interrupt.** A reset puts every Teensy pin back to high impedance and the gate pull-downs on the board drop every coil. A watchdog fed from an interrupt survives a dead loop and would never fire.

With the pull-in ending in an interrupt, a stopped loop no longer strands a coil on its own. What the watchdog covers is a fault that takes the interrupts down with it, and the restart a hung game needs either way.

**The lighting budget is lowered while any coil is energised.** The machine's supply does not carry the lighting ceiling and a coil at the same time, which [`docs/parts/lighting/design.md`](../docs/parts/lighting/design.md) sets out.

## The event

| Field | Content |
|---|---|
| Type | `EventType::BumperHit` |
| Source | The bumper, `0` to `2` |
| Timestamp | `micros()` in the handler, the moment the contact closed |
| Payload | None |

## Limitations

**The 50 ms rests on no coil datasheet.** The manufacturer of the solenoid is unknown, and the figure comes from what the stock machine does.

## The driver

```cpp
class BumperDriver {
public:
    void begin(EventQueue::Producer& out) {
        out_  = &out;
        self_ = this;
        for (uint8_t c = 0; c < kCoils; ++c) {
            pinMode(kTrigger[c], OUTPUT);
            digitalWriteFast(kTrigger[c], LOW);
        }
        for (uint8_t c = 0; c < kSenses; ++c) {
            pinMode(kSense[c], INPUT);
            attachInterrupt(digitalPinToInterrupt(kSense[c]), kEdge[c], CHANGE);
        }
        timer_.begin(tick, kTickUs);              // the driver's own time base
    }

    // the game logic's own path, and the only one the fourth coil has
    bool fire(uint8_t coil) { return pull(coil, micros()); }

    // the state the game logic may query between events
    bool ballOn(uint8_t bumper) const {
        return digitalReadFast(kSense[bumper]) == HIGH;
    }

private:
    static constexpr uint8_t  kCoils  = 4;
    static constexpr uint8_t  kSenses = 3;
    static constexpr uint8_t  kTrigger[kCoils] = {32, 34, 35, 0};
    static constexpr uint8_t  kSense[kSenses]  = {1, 14, 15};
    static constexpr uint32_t kOnUs   = 50000;    // the pull-in, docs/parts/bumper
    static constexpr uint32_t kOpenUs = 10000;    // the contact has to stay open
    static constexpr uint32_t kTickUs = 1000;     // what ends a pull-in

    // attachInterrupt and IntervalTimer take a plain function pointer, so a
    // static one per channel hands the edge to the instance
    static void edge0() { self_->edge(0); }
    static void edge1() { self_->edge(1); }
    static void edge2() { self_->edge(2); }
    static constexpr void (*kEdge[kSenses])() = {edge0, edge1, edge2};
    static void tick() { self_->sweep(); }

    // both edges of a sense line arrive here
    void edge(uint8_t bumper) {
        const uint32_t now = micros();
        if (digitalReadFast(kSense[bumper]) == LOW) {
            opened_[bumper] = now;                // the contact just let go
            return;
        }
        if (!armed_[bumper]) return;              // still the contact that fired
        armed_[bumper] = false;
        pull(bumper, now);
        out_->publish(PinballEvent{now, EventType::BumperHit, bumper, 0});
    }

    // the driver's own interrupt: end a pull-in that has run, and arm a channel
    // whose contact has been open long enough
    void sweep() {
        const uint32_t now = micros();
        for (uint8_t c = 0; c < kCoils; ++c) {
            if (live_[c] && now - since_[c] >= kOnUs) {
                digitalWriteFast(kTrigger[c], LOW);
                live_[c] = false;
            }
        }
        for (uint8_t c = 0; c < kSenses; ++c) {
            if (!armed_[c] && digitalReadFast(kSense[c]) == LOW
                          && now - opened_[c] >= kOpenUs) {
                armed_[c] = true;
            }
        }
    }

    bool pull(uint8_t coil, uint32_t now) {
        if (live_[coil]) return false;
        live_[coil]  = true;
        since_[coil] = now;
        digitalWriteFast(kTrigger[coil], HIGH);
        return true;
    }

    IntervalTimer         timer_;
    static BumperDriver*  self_;
    EventQueue::Producer* out_ = nullptr;
    volatile bool         live_[kCoils]    = {};
    volatile uint32_t     since_[kCoils]   = {};
    volatile bool         armed_[kSenses]  = {true, true, true};
    volatile uint32_t     opened_[kSenses] = {};
};
```

Each channel owns its own state, so the sense interrupts never contend with one another and nothing here runs with interrupts disabled. `fire()` is the one path the main loop uses, and calling it on a channel that a sense edge fires at the same moment leaves that coil energised for a fraction of a millisecond longer than commanded.
