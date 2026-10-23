#include <new>          // placement new, which puts an effect into its entry
#include <jled_base.h>  // JLed 4.15, https://github.com/jandelgado/jled; only its curves

// a color of a lamp, 8 bits for each of red, green and blue
struct Color {
    uint8_t r = 0, g = 0, b = 0;
    bool    below = false;         // shows the effect below instead of a color, as Color::none does

    // gets the color at percent of its brightness, 0 to 100
    constexpr Color dim(uint8_t percent) const {
        return {uint8_t(r * percent / 100), uint8_t(g * percent / 100), uint8_t(b * percent / 100), below};
    }

    static const Color black, white, red, green, blue, amber, none;
};

inline constexpr Color Color::black{0, 0, 0};
inline constexpr Color Color::white{255, 255, 255};
inline constexpr Color Color::red{255, 0, 0};
inline constexpr Color Color::green{0, 255, 0};
inline constexpr Color Color::blue{0, 0, 255};
inline constexpr Color Color::amber{255, 120, 0};
inline constexpr Color Color::none{0, 0, 0, true};

enum class By : uint8_t { Time, Position, Value };   // what picks a palette's color: the time, the lamp's place, or the effect's value

// what an effect paints with: a single color, Color::none, or the colors of a palette; a single Color is passed as it is
struct Paint {
    Color                       color;
    const TProgmemRGBPalette16* palette = nullptr;   // one of FastLED's palettes, such as RainbowColors_p, which stay in flash
    By                          by      = By::Value;

    constexpr Paint() = default;
    constexpr Paint(Color c) : color(c) {}
    constexpr Paint(const TProgmemRGBPalette16& p, By b) : palette(&p), by(b) {}
};

enum class Pattern : uint8_t { Chase, Asteroid, Wipe };   // the patterns run() moves along a range
enum class Direction : uint8_t { Up, Down };               // Up runs toward the higher lamp numbers
enum class Off : uint8_t { Through, Dark };                // what a blink shows while off: the effect below, or a dark lamp

// where the strips' lamps lie along their path, in quarters of a millimeter; placeholders until the
// strips are mounted: 160 LEDs to the meter, every second LED lit on a perimeter, one pitch across a gap
inline constexpr uint16_t kPerimeterPitchQ = 50;   // 12.5 mm
inline constexpr uint16_t kCeilingPitchQ   = 25;   // 6.25 mm
inline constexpr uint16_t kGapQ            = 50;   // 12.5 mm between two strips of a path
inline constexpr uint32_t kMsPerQ          = 250;  // ms a speed of one millimeter a second takes for a quarter millimeter

// where a lamp lies among the lamps an effect covers, for an effect that moves along them
struct Spot {
    uint16_t index   = 0;          // the lamp's place, 0 for the first
    uint16_t count   = 1;          // how many lamps the effect covers
    uint32_t mmQ     = 0;          // a strip lamp's position along its strip in quarter millimeters, 0 on the playfield
    uint32_t lengthQ = 0;          // the position of the strip's last lamp, 0 on the playfield
};

// what every effect is: for a lamp at a moment, how far it stands between the effect's two paints;
// an effect holds only plain values, so an entry can keep a copy of it
class Effect {
public:
    // gets 0 for the first paint, 255 for the second, or a value between, ms after the effect began
    virtual uint8_t value(uint32_t ms, const Spot& at) const = 0;
};

inline constexpr size_t kEffectSize = 16;   // room for the largest built-in effect; an effect a game writes has to fit it too

// lights steadily
class Steady : public Effect {
public:
    uint8_t value(uint32_t, const Spot&) const override { return 255; }
};

// lights for onMs, then goes off for offMs, over and over
class Blink : public Effect {
public:
    Blink(uint16_t onMs, uint16_t offMs) : on_(onMs), off_(offMs) {}
    uint8_t value(uint32_t ms, const Spot&) const override {
        const jled::BlinkBrightnessEvaluator e(on_, off_);
        return e.Period() == 0 ? 0 : e.Eval(ms % e.Period());
    }

private:
    uint16_t on_, off_;
};

// fades up and back once per period, along JLed's breathing curve
class Pulse : public Effect {
public:
    explicit Pulse(uint16_t periodMs) : period_(periodMs) {}
    uint8_t value(uint32_t ms, const Spot&) const override {
        const jled::BreatheBrightnessEvaluator e(period_ / 2, 0, period_ - period_ / 2);
        return e.Period() == 0 ? 0 : e.Eval(ms % e.Period());
    }

private:
    uint16_t period_;
};

// fades up within ms and stays there
class Fade : public Effect {
public:
    explicit Fade(uint16_t ms) : ms_(ms) {}
    uint8_t value(uint32_t ms, const Spot&) const override {
        const jled::BreatheBrightnessEvaluator e(ms_, 0, 0);       // JLed's FadeOn
        return ms >= ms_ ? 255 : e.Eval(ms);
    }

private:
    uint16_t ms_;
};

// moves a pattern along the lamps; step is the ms from lamp to lamp, or with perMm a speed in mm a second
class Run : public Effect {
public:
    Run(Pattern p, Direction d, uint16_t step, bool perMm) : pattern_(p), up_(d == Direction::Up), perMm_(perMm), step_(step) {}
    uint8_t value(uint32_t ms, const Spot& at) const override;

private:
    Pattern  pattern_;
    bool     up_, perMm_;
    uint16_t step_;
};

uint8_t Run::value(uint32_t ms, const Spot& at) const {
    uint32_t offset, round, head;          // when the run reaches the lamp, how long a round takes, how long the head lights a lamp
    if (perMm_) {                          // a strip: a lamp's time follows its position, so the pace holds across spacing and gaps
        const uint32_t q = up_ ? at.mmQ : at.lengthQ - at.mmQ;
        offset = q * kMsPerQ / step_;
        round  = (at.lengthQ + kPerimeterPitchQ) * kMsPerQ / step_;
        head   = kPerimeterPitchQ * kMsPerQ / step_;
    } else {                               // a range: one lamp per step
        const uint32_t i = up_ ? at.index : uint32_t(at.count - 1 - at.index);
        offset = i * step_;
        round  = uint32_t(at.count) * step_;
        head   = step_;
    }
    if (ms < offset) return 0;             // the run has not reached the lamp yet
    const uint32_t t = ms - offset;
    switch (pattern_) {
        case Pattern::Chase: {
            if (round <= head) return 255;
            const jled::BlinkBrightnessEvaluator e(uint16_t(head), uint16_t(round - head));
            return e.Eval(t % round);
        }
        case Pattern::Asteroid: {
            const uint16_t tail = uint16_t(round / 4);                 // an asteroid's tail is a quarter of the lamps
            if (tail == 0) return 0;
            const jled::BreatheBrightnessEvaluator e(0, 0, tail);      // JLed's FadeOff
            const uint32_t r = t % round;
            return r < tail ? e.Eval(r) : 0;
        }
        case Pattern::Wipe: return 255;                                // lights up once and stays
    }
    return 0;
}

class Lamps;
class Lamp;

// some lamps of one group, to which effects go with one key and priority
class LampRange {
public:
    LampRange() = default;
    LampRange(Lamps* lamps, uint16_t first, uint16_t count, uint8_t key, uint8_t priority)
        : lamps_(lamps), first_(first), count_(count), key_(key), priority_(priority) {}

    Lamp operator[](uint16_t i) const;   // gets one lamp of the range

    // gets a part of the range; a part past its end is cut at the end
    LampRange sub(uint16_t first, uint16_t count) const {
        if (first >= count_) return {lamps_, first_, 0, key_, priority_};
        return {lamps_, uint16_t(first_ + first), min(count, uint16_t(count_ - first)), key_, priority_};
    }

    uint16_t size() const { return count_; }   // gets how many lamps the range holds

    // starts an effect that mixes from into to, a built-in one or one a game writes
    template <typename E>
    LampRange& play(const E& effect, Paint from, Paint to);

    LampRange& set(Paint c)                                                  { return play(Steady(), c, c); }                 // lights every lamp steadily in c
    LampRange& off()                                                         { return set(Color::black); }                    // turns every lamp dark
    LampRange& blink(Paint c, uint16_t onMs, uint16_t offMs, Off off = Off::Through);   // lights in c for onMs, then goes off for offMs, over and over
    LampRange& alternate(Paint a, Paint b, uint16_t aMs, uint16_t bMs)      { return play(Blink(aMs, bMs), b, a); }          // lights in a for aMs, then in b for bMs, over and over
    LampRange& pulse(Paint a, Paint b, uint16_t periodMs)                   { return play(Pulse(periodMs), a, b); }          // fades from a to b and back once per period
    LampRange& fade(Paint c, uint16_t ms);                                   // fades from the current color to c within ms
    LampRange& run(Pattern p, Direction d, Paint c, uint16_t stepMs, Paint back = Color::none);   // moves the pattern in c over back by one lamp per step
    void       lasting(uint16_t ms);                                         // removes the effects ms from now
    void       clear();                                                      // removes the effects the range's key set, so the ones below show

private:
    Lamps*   lamps_    = nullptr;
    uint16_t first_    = 0;        // the range's first lamp in the lamps component's numbering
    uint16_t count_    = 0;
    uint8_t  key_      = 0;
    uint8_t  priority_ = 0;
};

// a single lamp, to which effects go with one key and priority; it takes every effect but run(), which needs several lamps
class Lamp {
public:
    Lamp() = default;

    template <typename E>
    Lamp& play(const E& effect, Paint from, Paint to) { one_.play(effect, from, to); return *this; }   // starts an effect that mixes from into to

    Lamp& set(Paint c)                                                  { one_.set(c); return *this; }                     // lights steadily in c
    Lamp& off()                                                         { one_.off(); return *this; }                      // turns dark
    Lamp& blink(Paint c, uint16_t onMs, uint16_t offMs, Off off = Off::Through) { one_.blink(c, onMs, offMs, off); return *this; } // lights in c for onMs, then goes off for offMs, over and over
    Lamp& alternate(Paint a, Paint b, uint16_t aMs, uint16_t bMs)      { one_.alternate(a, b, aMs, bMs); return *this; }  // lights in a for aMs, then in b for bMs, over and over
    Lamp& pulse(Paint a, Paint b, uint16_t periodMs)                   { one_.pulse(a, b, periodMs); return *this; }      // fades from a to b and back once per period
    Lamp& fade(Paint c, uint16_t ms)                                   { one_.fade(c, ms); return *this; }                // fades from the current color to c within ms
    void  lasting(uint16_t ms)                                         { one_.lasting(ms); }                              // removes the effect ms from now
    void  clear()                                                      { one_.clear(); }                                  // removes the effect the lamp's key set, so the one below shows

private:
    friend class LampRange;
    explicit Lamp(LampRange one) : one_(one) {}

    LampRange one_;   // the lamp as a range of one
};

inline Lamp LampRange::operator[](uint16_t i) const { return Lamp(sub(i, 1)); }

// a strip of lamps that an effect runs along as a whole: a perimeter, the ceiling, or a path through
// several of them; each lamp has its position along the strip, and each of the strip's entries holds one effect
class Strip {
public:
    Strip() = default;
    Strip(Lamps* lamps, uint8_t strip, uint8_t key, uint8_t priority)
        : lamps_(lamps), strip_(strip), key_(key), priority_(priority) {}

    uint16_t size() const;   // gets how many lamps the strip holds

    // starts an effect that mixes from into to, a built-in one or one a game writes
    template <typename E>
    Strip& play(const E& effect, Paint from, Paint to);

    Strip& set(Paint c)                                                  { return play(Steady(), c, c); }         // lights the strip steadily in c
    Strip& off()                                                         { return set(Color::black); }            // turns the strip dark
    Strip& blink(Paint c, uint16_t onMs, uint16_t offMs, Off off = Off::Through);   // lights in c for onMs, then goes off for offMs, over and over
    Strip& alternate(Paint a, Paint b, uint16_t aMs, uint16_t bMs)      { return play(Blink(aMs, bMs), b, a); }  // lights in a for aMs, then in b for bMs, over and over
    Strip& pulse(Paint a, Paint b, uint16_t periodMs)                   { return play(Pulse(periodMs), a, b); }  // fades from a to b and back once per period
    Strip& fade(Paint c, uint16_t ms);                                   // fades from the color its first lamp shows now to c within ms
    Strip& run(Pattern p, Direction d, Paint c, uint16_t mmPerS, Paint back = Color::none);   // moves the pattern in c over back along the strip at mmPerS millimeters a second
    void   lasting(uint16_t ms);                                         // removes the effect ms from now
    void   clear();                                                      // removes the effect the strip's key set, so the one below shows

private:
    Lamps*  lamps_    = nullptr;
    uint8_t strip_    = 0;         // which of the lamps component's strips
    uint8_t key_      = 0;
    uint8_t priority_ = 0;
};

// the lamp groups, to which effects go with one key and priority
struct LampGroups {
    Strip     perimeterLeft, perimeterRight, ceiling;
    Strip     perimeter;                                     // both perimeters as one: up the left, across the top, down the right
    Strip     ambient;                                       // the perimeters and the ceiling as one: up the left, across the ceiling, down the right
    LampRange playfield;
    LampRange score, ballCount, track, bumpers, rollovers, targets, action;   // the named parts of playfield
    Lamp      drain, scoop;
};

// the machine's lamps, each lamp and each strip with up to kMaxEffects effects at once
class Lamps {
public:
    explicit Lamps(Lighting& lighting);

    // gets the groups to which effects go with key and priority
    LampGroups groups(uint8_t key, uint8_t priority);

    void     remove(uint8_t key);                    // removes every effect with key from every lamp and strip, so the ones below show
    uint32_t refused() const { return refused_; }    // gets how many effects a lamp or strip with kMaxEffects effects has refused
    void     clear();                                // removes every effect from every lamp and strip; the host calls it between two games

    // works out every effect and hands on a frame once kFrameUs has passed; the main loop calls it in every pass
    void update(uint32_t now);

private:
    friend class LampRange;
    friend class Strip;

    // the group sizes, from the light layout of docs/parts/lighting/design.md
    static constexpr uint16_t kScore     = 10;
    static constexpr uint16_t kBallCount = 6;
    static constexpr uint16_t kTrack     = 7;    // the track arrows and the track lights
    static constexpr uint16_t kBumpers   = 4;
    static constexpr uint16_t kDrain     = 1;
    static constexpr uint16_t kRollovers = 3;
    static constexpr uint16_t kScoop     = 1;
    static constexpr uint16_t kTargets   = 4;    // TBD
    static constexpr uint16_t kAction    = 20;   // TDB, the playfield action lights
    static constexpr uint16_t kPlayfield = kScore + kBallCount + kTrack + kBumpers + kDrain + kRollovers
                                         + kScoop + kTargets + kAction;
    static constexpr uint16_t kPerimeter = 40;   // the lit LEDs of a perimeter strip, every second of its 79
    static constexpr uint16_t kCeiling   = 26;
    static constexpr uint8_t  kMaxEffects = 5;    // the component, the game's base mode, two modes over it and a short flash
    static constexpr uint32_t kFrameUs   = 10000;   // 100 frames a second, which leaves a frame's 2.67 ms room
    static constexpr uint32_t kPaletteStepMs = 8;   // a palette by time runs through its 256 colors in 2048 ms

    enum StripId : uint8_t { kLeft, kRight, kCeilingStrip, kBoth, kAmbient, kStrips };

    // one effect of a lamp or strip: who set it, how high it lies, when it came and ends, its two paints and a copy of its effect
    struct Entry {
        uint8_t  key      = 0;
        uint8_t  priority = 0;
        bool     used     = false;
        bool     ends     = false;     // whether lasting() set an end
        uint8_t  index    = 0;         // a playfield lamp's place in the range of the call
        uint8_t  count    = 1;         // how many lamps that range held
        uint32_t seq      = 0;         // the order of the calls, so between equal priorities the later one shows
        uint32_t endMs    = 0;         // nowUs() / 1000 at which lasting() removes the effect
        uint32_t startMs  = 0;         // nowUs() / 1000 when the effect began
        Paint    from, to;
        alignas(Effect) uint8_t effect[kEffectSize];

        const Effect& fx() const { return *reinterpret_cast<const Effect*>(effect); }
        bool above(const Entry& o) const { return priority != o.priority ? priority > o.priority : int32_t(seq - o.seq) > 0; }
        bool over(uint32_t ms) const     { return ends && int32_t(ms - endMs) >= 0; }   // gets whether lasting() has run out
    };

    struct Place { uint8_t chain; uint8_t led; };   // where a lamp sits: its chain, 0 to 5 for A to F, and its LED in the chain
    struct Where { Place place; uint16_t q = 0; };   // where a strip's lamp sits, and its position along the strip in quarter millimeters
    struct Segment { uint8_t strip; bool reversed; }; // a strip of LEDs a path runs through, a reversed one from its far end

    // the playfield's lamps in their chains, in the order of its parts; set when the chains
    // are wired, docs/parts/lighting/design.md
    static const Place kPlayfieldPlaces[kPlayfield];

    uint16_t stripSize(uint8_t strip) const;            // gets how many lamps a strip holds
    Where    locate(uint8_t strip, uint16_t i) const;   // gets where lamp i of a strip sits and its position
    uint16_t stripLength(uint8_t strip) const;          // gets the position of a strip's last lamp
    Color    colorAt(Place p) const;                    // gets the color the LED at p shows now

    // gets the color a paint gives: below for Color::none, and a palette's color at the place, the time
    // since the effect began or the value
    static CRGB paint(const Paint& p, CRGB below, uint8_t value, uint8_t place, uint32_t elapsedMs);

    // gets the color an entry gives at ms over the color below, for a lamp at a spot
    static CRGB mix(const Entry& e, uint32_t ms, const Spot& at, CRGB below);

    // gets the entry for key among a lamp's entries: the key's own entry, else a free one, else none, which counts as refused
    Entry* claim(Entry (&entries)[kMaxEffects], uint8_t key, uint8_t priority);

    // gives an entry with key and priority the paints from and to and a copy of effect
    template <typename E>
    bool start(Entry (&entries)[kMaxEffects], uint8_t key, uint8_t priority, Paint from, Paint to, const E& effect);

    Lighting& lighting_;
    Entry     slots_[kPlayfield][kMaxEffects];
    Entry     strips_[kStrips][kMaxEffects];
    uint32_t  seq_     = 0;            // the last call's number, for Entry::seq
    uint32_t  refused_ = 0;
    uint32_t  shown_   = 0;            // nowUs() of the last frame
};

const Lamps::Place Lamps::kPlayfieldPlaces[Lamps::kPlayfield] = {};

Lamps::Lamps(Lighting& lighting) : lighting_(lighting) {}

LampGroups Lamps::groups(uint8_t key, uint8_t priority) {
    LampGroups g;
    g.perimeterLeft  = {this, kLeft, key, priority};
    g.perimeterRight = {this, kRight, key, priority};
    g.ceiling        = {this, kCeilingStrip, key, priority};
    g.perimeter      = {this, kBoth, key, priority};
    g.ambient        = {this, kAmbient, key, priority};
    g.playfield      = {this, 0, kPlayfield, key, priority};
    g.score     = g.playfield.sub(0, kScore);
    g.ballCount = g.playfield.sub(kScore, kBallCount);
    g.track     = g.playfield.sub(kScore + kBallCount, kTrack);
    g.bumpers   = g.playfield.sub(kScore + kBallCount + kTrack, kBumpers);
    g.drain     = g.playfield[kScore + kBallCount + kTrack + kBumpers];
    g.rollovers = g.playfield.sub(kScore + kBallCount + kTrack + kBumpers + kDrain, kRollovers);
    g.scoop     = g.playfield[kScore + kBallCount + kTrack + kBumpers + kDrain + kRollovers];
    g.targets   = g.playfield.sub(kScore + kBallCount + kTrack + kBumpers + kDrain + kRollovers + kScoop,
                                  kTargets);
    g.action    = g.playfield.sub(kScore + kBallCount + kTrack + kBumpers + kDrain + kRollovers + kScoop
                                  + kTargets, kAction);
    return g;
}

void Lamps::clear() {
    for (auto& lamp : slots_)   for (Entry& e : lamp)  e.used = false;
    for (auto& strip : strips_) for (Entry& e : strip) e.used = false;
}

void Lamps::remove(uint8_t key) {
    for (auto& lamp : slots_)   for (Entry& e : lamp) if (e.key == key) e.used = false;
    for (auto& strip : strips_) for (Entry& e : strip) if (e.key == key) e.used = false;
}

uint16_t Lamps::stripSize(uint8_t strip) const {
    switch (strip) {
        case kCeilingStrip: return kCeiling;
        case kBoth:         return 2 * kPerimeter;
        case kAmbient:      return 2 * kPerimeter + kCeiling;
        default:            return kPerimeter;
    }
}

// the order of each strip's lamps and their positions are set when the strips are mounted; until then
// lamp 0 of a perimeter is its first lit LED, and the lamps lie evenly
Lamps::Where Lamps::locate(uint8_t strip, uint16_t i) const {
    static constexpr Segment both[]    = {{kLeft, false}, {kRight, true}};
    static constexpr Segment ambient[] = {{kLeft, false}, {kCeilingStrip, false}, {kRight, true}};
    switch (strip) {
        case kLeft:         return {{0, uint8_t(2 * i)}, uint16_t(i * kPerimeterPitchQ)};   // chain A, every second LED
        case kRight:        return {{1, uint8_t(2 * i)}, uint16_t(i * kPerimeterPitchQ)};   // chain B, every second LED
        case kCeilingStrip: return {{2, uint8_t(i)}, uint16_t(i * kCeilingPitchQ)};         // chain C
    }
    const Segment* path  = strip == kBoth ? both : ambient;
    const uint8_t  parts = strip == kBoth ? 2 : 3;
    uint16_t start = 0;                                                     // the position where the part begins
    for (uint8_t k = 0; k < parts; ++k) {
        const uint16_t size = stripSize(path[k].strip);
        if (i < size || k == parts - 1) {
            const Where    base = locate(path[k].strip, path[k].reversed ? uint16_t(size - 1 - i) : i);
            const uint16_t q    = path[k].reversed ? uint16_t(stripLength(path[k].strip) - base.q) : base.q;
            return {base.place, uint16_t(start + q)};
        }
        i     -= size;
        start += stripLength(path[k].strip) + kGapQ;
    }
    return {};
}

uint16_t Lamps::stripLength(uint8_t strip) const {
    return locate(strip, uint16_t(stripSize(strip) - 1)).q;
}

Color Lamps::colorAt(Place p) const {
    const CRGB c = lighting_.chain(p.chain)[p.led];
    return {c.r, c.g, c.b};
}

CRGB Lamps::paint(const Paint& p, CRGB below, uint8_t value, uint8_t place, uint32_t elapsedMs) {
    if (p.palette) {
        const uint8_t i = p.by == By::Time     ? uint8_t(elapsedMs / kPaletteStepMs)
                        : p.by == By::Position ? place
                        :                        uint8_t(255 - value);   // by value: a tail runs through the palette from its head
        return ColorFromPalette(*p.palette, i);
    }
    return p.color.below ? below : CRGB(p.color.r, p.color.g, p.color.b);
}

CRGB Lamps::mix(const Entry& e, uint32_t ms, const Spot& at, CRGB below) {
    const uint32_t elapsed = ms - e.startMs;
    const uint8_t  value   = e.fx().value(elapsed, at);
    const uint8_t  place   = at.lengthQ ? uint8_t(at.mmQ * 255 / at.lengthQ)
                           : at.count > 1 ? uint8_t(uint32_t(at.index) * 255 / (at.count - 1)) : 0;
    return blend(paint(e.from, below, value, place, elapsed), paint(e.to, below, value, place, elapsed), value);
}

Lamps::Entry* Lamps::claim(Entry (&entries)[kMaxEffects], uint8_t key, uint8_t priority) {
    Entry* e = nullptr;
    for (Entry& x : entries) {
        if (x.used && x.key == key) { e = &x; break; }   // the key's own entry is replaced
        if (!x.used && !e) e = &x;
    }
    if (!e) { ++refused_; return nullptr; }              // a lamp that holds kMaxEffects effects keeps them
    e->key      = key;
    e->priority = priority;
    e->used     = true;
    e->ends     = false;
    e->index    = 0;
    e->count    = 1;
    e->seq      = ++seq_;
    e->startMs  = nowUs() / 1000;
    return e;
}

template <typename E>
bool Lamps::start(Entry (&entries)[kMaxEffects], uint8_t key, uint8_t priority, Paint from, Paint to, const E& effect) {
    static_assert(sizeof(E) <= kEffectSize && alignof(E) <= alignof(Effect), "an effect has to fit an entry");
    Entry* e = claim(entries, key, priority);
    if (!e) return false;
    e->from = from;
    e->to   = to;
    new (e->effect) E(effect);
    return true;
}

void Lamps::update(uint32_t now) {
    if (now - shown_ < kFrameUs) return;
    shown_ = now;
    const uint32_t ms = now / 1000;

    // the playfield's lamps: each lamp mixes its effects from the lowest priority up
    for (uint16_t lamp = 0; lamp < kPlayfield; ++lamp) {
        const Entry* order[kMaxEffects];
        uint8_t      n = 0;
        for (Entry& e : slots_[lamp]) {
            if (e.used && e.over(ms)) e.used = false;
            if (!e.used) continue;
            uint8_t at = n++;
            while (at > 0 && order[at - 1]->above(e)) { order[at] = order[at - 1]; --at; }
            order[at] = &e;
        }
        CRGB c(0, 0, 0);                                 // a lamp without effects is dark
        for (uint8_t k = 0; k < n; ++k) c = mix(*order[k], ms, Spot{order[k]->index, order[k]->count}, c);
        const Place p = kPlayfieldPlaces[lamp];
        lighting_.chain(p.chain)[p.led] = c;
    }

    // the strips: dark first, then every effect from the lowest up, so the top one paints last
    for (uint8_t strip = kLeft; strip <= kCeilingStrip; ++strip) {   // the three strips of LEDs; the paths share theirs
        for (uint16_t i = 0; i < stripSize(strip); ++i) {
            const Place p = locate(strip, i).place;
            lighting_.chain(p.chain)[p.led] = CRGB(0, 0, 0);
        }
    }
    struct Layer { uint8_t strip; const Entry* e; };
    Layer   order[kStrips * kMaxEffects];
    uint8_t n = 0;
    for (uint8_t strip = 0; strip < kStrips; ++strip) {
        for (Entry& e : strips_[strip]) {
            if (e.used && e.over(ms)) e.used = false;
            if (!e.used) continue;
            uint8_t at = n++;
            while (at > 0 && order[at - 1].e->above(e)) { order[at] = order[at - 1]; --at; }
            order[at] = {strip, &e};
        }
    }
    for (uint8_t k = 0; k < n; ++k) {
        const uint8_t  strip  = order[k].strip;
        const uint16_t size   = stripSize(strip);
        const uint16_t length = stripLength(strip);
        for (uint16_t i = 0; i < size; ++i) {
            const Where at  = locate(strip, i);
            CRGB&       led = lighting_.chain(at.place.chain)[at.place.led];   // holds the color the entries below painted
            led = mix(*order[k].e, ms, Spot{i, size, at.q, length}, led);
        }
    }

    lighting_.show();                                  // returns at once; the frame goes out by DMA
}

template <typename E>
LampRange& LampRange::play(const E& effect, Paint from, Paint to) {
    for (uint16_t i = 0; i < count_; ++i) {
        if (!lamps_->start(lamps_->slots_[first_ + i], key_, priority_, from, to, effect)) continue;
        for (Lamps::Entry& e : lamps_->slots_[first_ + i]) {
            if (e.used && e.key == key_) { e.index = uint8_t(i); e.count = uint8_t(count_); }
        }
    }
    return *this;
}

LampRange& LampRange::blink(Paint c, uint16_t onMs, uint16_t offMs, Off off) {
    return play(Blink(onMs, offMs), off == Off::Dark ? Paint(Color::black) : Paint(Color::none), c);
}

LampRange& LampRange::fade(Paint c, uint16_t ms) {
    for (uint16_t i = 0; i < count_; ++i) {   // each lamp fades from the color it shows now
        const Color from = lamps_->colorAt(Lamps::kPlayfieldPlaces[first_ + i]);
        sub(i, 1).play(Fade(ms), from, c);
        for (Lamps::Entry& e : lamps_->slots_[first_ + i]) {   // keeps the lamp's place in this range, for a palette by position
            if (e.used && e.key == key_) { e.index = uint8_t(i); e.count = uint8_t(count_); }
        }
    }
    return *this;
}

LampRange& LampRange::run(Pattern p, Direction d, Paint c, uint16_t stepMs, Paint back) {
    const uint16_t step = count_ == 0 ? stepMs : min(stepMs, uint16_t(UINT16_MAX / count_));   // JLed times a round in 16 bits
    return play(Run(p, d, step, false), back, c);
}

void LampRange::lasting(uint16_t ms) {
    const uint32_t end = nowUs() / 1000 + ms;
    for (uint16_t i = 0; i < count_; ++i) {
        for (Lamps::Entry& e : lamps_->slots_[first_ + i]) {
            if (e.used && e.key == key_) { e.ends = true; e.endMs = end; }
        }
    }
}

void LampRange::clear() {
    for (uint16_t i = 0; i < count_; ++i) {
        for (Lamps::Entry& e : lamps_->slots_[first_ + i]) if (e.key == key_) e.used = false;
    }
}

uint16_t Strip::size() const { return lamps_->stripSize(strip_); }

template <typename E>
Strip& Strip::play(const E& effect, Paint from, Paint to) {
    lamps_->start(lamps_->strips_[strip_], key_, priority_, from, to, effect);
    return *this;
}

Strip& Strip::blink(Paint c, uint16_t onMs, uint16_t offMs, Off off) {
    return play(Blink(onMs, offMs), off == Off::Dark ? Paint(Color::black) : Paint(Color::none), c);
}

Strip& Strip::fade(Paint c, uint16_t ms) {
    const Color from = lamps_->colorAt(lamps_->locate(strip_, 0).place);
    return play(Fade(ms), from, c);
}

Strip& Strip::run(Pattern p, Direction d, Paint c, uint16_t mmPerS, Paint back) {
    const uint32_t round = (lamps_->stripLength(strip_) + kPerimeterPitchQ) * kMsPerQ;   // a round in ms at 1 mm a second
    const uint32_t least = round / UINT16_MAX + 1;                                     // JLed times a round in 16 bits
    return play(Run(p, d, mmPerS < least ? uint16_t(least) : mmPerS, true), back, c);
}

void Strip::lasting(uint16_t ms) {
    for (Lamps::Entry& e : lamps_->strips_[strip_]) {
        if (e.used && e.key == key_) { e.ends = true; e.endMs = nowUs() / 1000 + ms; }
    }
}

void Strip::clear() {
    for (Lamps::Entry& e : lamps_->strips_[strip_]) if (e.key == key_) e.used = false;
}
