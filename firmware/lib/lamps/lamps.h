// The machine's lamps in fixed groups, with the effects the game's modes run on them. Each lamp
// and each strip holds up to five effects at once and shows the one of the highest priority.
// Every frame period, update() works out each lamp's color and hands the frame to the lighting
// driver.
//
// The config names each group's chain and its LEDs: the two perimeters, the ceiling, and the
// playfield's lower, middle and upper section. A playfield lamp takes one LED or several in a row.
// A strip's hidden LEDs stay dark, and its lamps are the LEDs between them, every one on the
// ceiling and on the perimeters as the driver's profile steps them.
//
// Design: docs/firmware/components/lamps.md.
#pragma once
#include <Arduino.h>
#include <new>
#include <type_traits>
#include <clock.h>
#include <debug-log.h>
#include <layer.h>
#include <lighting.h>
#include <priority-list.h>

// a color of a lamp, 8 bits for each of red, green and blue
struct Color {
    static constexpr uint8_t WHOLE_PERCENT = 100;

    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    // shows the effect below instead of a color, as Color::NONE does
    bool below = false;

    // Gets the color at percent of its brightness. A percent above 100 counts as 100.
    constexpr Color dim(uint8_t percent) const {
        const uint32_t p = percent > WHOLE_PERCENT ? WHOLE_PERCENT : percent;
        return {uint8_t(r * p / WHOLE_PERCENT), uint8_t(g * p / WHOLE_PERCENT),
                uint8_t(b * p / WHOLE_PERCENT), below};
    }

    static const Color BLACK, WHITE, RED, GREEN, BLUE, AMBER, NONE;
};

inline constexpr Color Color::BLACK{0, 0, 0, false};
inline constexpr Color Color::WHITE{255, 255, 255, false};
inline constexpr Color Color::RED{255, 0, 0, false};
inline constexpr Color Color::GREEN{0, 255, 0, false};
inline constexpr Color Color::BLUE{0, 0, 255, false};
inline constexpr Color Color::AMBER{255, 120, 0, false};
inline constexpr Color Color::NONE{0, 0, 0, true};

// what picks a palette's color: the time, the lamp's place, or the effect's value
enum class By : uint8_t { TIME, POSITION, VALUE };

// What an effect paints with: a single color, or the colors of one of FastLED's palettes, such as
// RainbowColors_p. A single Color is passed as it is.
struct Paint {
    Color color;
    const TProgmemRGBPalette16* palette = nullptr;   // in flash, as FastLED keeps its palettes
    By by = By::VALUE;

    constexpr Paint() = default;
    constexpr Paint(Color c) : color(c) {}
    constexpr Paint(const TProgmemRGBPalette16& colors, By by) : palette(&colors), by(by) {}
};

// the patterns run() moves along the lamps
enum class Pattern : uint8_t { CHASE, ASTEROID, WIPE };
// UP runs toward the higher lamp numbers
enum class Direction : uint8_t { UP, DOWN };
// what a blink shows while off: the effect below, or a dark lamp
enum class Off : uint8_t { THROUGH, DARK };

// where a lamp lies among the lamps an effect covers, for an effect that moves along them
struct Spot {
    uint16_t index;   // the lamp's place, 0 for the first
    uint16_t count;   // how many lamps the effect covers
    uint32_t
        mmQ;   // a strip lamp's position along its strip in quarter millimeters, 0 on the playfield
    uint32_t lengthQ;   // the position of the strip's last lamp, 0 on the playfield
    uint32_t pitchQ;    // the spacing of the perimeters' lamps, which a run's head covers, 0 on the
                        // playfield
};

// What every effect is. An entry of a lamp keeps a copy of the effect, so an effect holds plain
// values only and fits Lamps::EFFECT_BYTES.
class Effect {
public:
    // Gets 0 for the first color, 255 for the second, or a value between, ms after the effect
    // began.
    virtual uint8_t value(uint32_t ms, const Spot& at) const = 0;

protected:
    ~Effect() = default;
};

// lights steadily in the second color
class Steady : public Effect {
public:
    uint8_t value(uint32_t ms, const Spot& at) const override;
};

// lights in the second color for onMs, then in the first for offMs, over and over
class Blink : public Effect {
public:
    Blink(uint16_t onMs, uint16_t offMs) : onMs(onMs), offMs(offMs) {}
    uint8_t value(uint32_t ms, const Spot& at) const override;

private:
    uint16_t onMs;
    uint16_t offMs;
};

// fades from the first color to the second and back once per period
class Pulse : public Effect {
public:
    explicit Pulse(uint16_t periodMs) : periodMs(periodMs) {}
    uint8_t value(uint32_t ms, const Spot& at) const override;

private:
    uint16_t periodMs;
};

// fades from the first color to the second within ms and stays there
class Fade : public Effect {
public:
    explicit Fade(uint16_t ms) : fadeMs(ms) {}
    uint8_t value(uint32_t ms, const Spot& at) const override;

private:
    uint16_t fadeMs;
};

// Moves a pattern in the second color over the first. step is the ms from lamp to lamp on the
// playfield and the mm a second along a strip.
class Run : public Effect {
public:
    Run(Pattern pattern, Direction direction, uint16_t step)
        : pattern(pattern), direction(direction), step(step) {}
    uint8_t value(uint32_t ms, const Spot& at) const override;

private:
    Pattern pattern;
    Direction direction;
    uint16_t step;
};

class Lamps;
class Lamp;

// some lamps of the playfield, to which effects go with one key and priority
class LampRange {
public:
    LampRange() = default;

    // Gets one lamp of the range. A lamp past its end is a lamp of no lamps.
    Lamp operator[](uint16_t i) const;
    // Gets a part of the range. A part past its end is cut at the end.
    LampRange sub(uint16_t first, uint16_t count) const;
    // Gets how many lamps the range holds.
    uint16_t size() const { return count; }

    // Starts an effect that mixes from into to, a built-in one or one a game writes.
    template <typename E> LampRange& play(const E& effect, Paint from, Paint to);

    LampRange& set(Paint c);
    LampRange& off();
    LampRange& blink(Paint c, uint16_t onMs, uint16_t offMs, Off off = Off::THROUGH);
    LampRange& alternate(Paint a, Paint b, uint16_t aMs, uint16_t bMs);
    LampRange& pulse(Paint a, Paint b, uint16_t periodMs);
    // Fades each lamp from the color it shows now to c within ms.
    LampRange& fade(Paint c, uint16_t ms);
    // Moves the pattern in c over back by one lamp every stepMs.
    LampRange& run(Pattern p, Direction d, Paint c, uint16_t stepMs, Paint back = Color::NONE);
    // Removes the range's effects ms from now.
    void lasting(uint16_t ms);
    // Removes the effects the range's mode set, so the ones below show.
    void clear();

private:
    friend class Lamps;
    LampRange(Lamps* lamps, uint16_t first, uint16_t count, uint8_t key, uint8_t priority)
        : lamps(lamps), first(first), count(count), key(key), priority(priority) {}

    Lamps* lamps = nullptr;
    uint16_t first = 0;   // the range's first lamp in the playfield
    uint16_t count = 0;
    uint8_t key = 0;
    uint8_t priority = 0;
};

// a single lamp of the playfield, to which effects go with one key and priority
class Lamp {
public:
    Lamp() = default;

    template <typename E> Lamp& play(const E& effect, Paint from, Paint to) {
        one.play(effect, from, to);
        return *this;
    }

    Lamp& set(Paint c) {
        one.set(c);
        return *this;
    }
    Lamp& off() {
        one.off();
        return *this;
    }
    Lamp& blink(Paint c, uint16_t onMs, uint16_t offMs, Off off = Off::THROUGH) {
        one.blink(c, onMs, offMs, off);
        return *this;
    }
    Lamp& alternate(Paint a, Paint b, uint16_t aMs, uint16_t bMs) {
        one.alternate(a, b, aMs, bMs);
        return *this;
    }
    Lamp& pulse(Paint a, Paint b, uint16_t periodMs) {
        one.pulse(a, b, periodMs);
        return *this;
    }
    Lamp& fade(Paint c, uint16_t ms) {
        one.fade(c, ms);
        return *this;
    }
    void lasting(uint16_t ms) { one.lasting(ms); }
    void clear() { one.clear(); }

private:
    friend class LampRange;
    explicit Lamp(LampRange one) : one(one) {}

    LampRange one;   // the lamp as a range of one
};

inline Lamp LampRange::operator[](uint16_t i) const {
    return Lamp(sub(i, 1));
}

// a strip of lamps that an effect covers as a whole: a perimeter, the ceiling, or a path through
// several of them
class Strip {
public:
    Strip() = default;

    // Gets how many lamps the strip holds.
    uint16_t size() const;

    template <typename E> Strip& play(const E& effect, Paint from, Paint to);

    Strip& set(Paint c);
    Strip& off();
    Strip& blink(Paint c, uint16_t onMs, uint16_t offMs, Off off = Off::THROUGH);
    Strip& alternate(Paint a, Paint b, uint16_t aMs, uint16_t bMs);
    Strip& pulse(Paint a, Paint b, uint16_t periodMs);
    // Fades the strip from the color its first lamp shows now to c within ms.
    Strip& fade(Paint c, uint16_t ms);
    // Moves the pattern in c over back along the strip at mmPerS millimeters a second.
    Strip& run(Pattern p, Direction d, Paint c, uint16_t mmPerS, Paint back = Color::NONE);
    void lasting(uint16_t ms);
    void clear();

private:
    friend class Lamps;
    Strip(Lamps* lamps, uint8_t strip, uint8_t key, uint8_t priority)
        : lamps(lamps), strip(strip), key(key), priority(priority) {}

    Lamps* lamps = nullptr;
    uint8_t strip = 0;   // which of the lamps component's strips
    uint8_t key = 0;
    uint8_t priority = 0;
};

// the lamp groups, to which effects go with one key and priority
struct LampGroups {
    Strip perimeterLeft;
    Strip perimeterRight;
    Strip ceiling;
    // both perimeters as one: up the left, across the top, down the right
    Strip perimeter;
    // the perimeters and the ceiling as one: up the left, across the ceiling, down the right
    Strip ambient;
    LampRange playfield;
};

// the machine's lamps, each lamp and each strip with up to five effects at once
class Lamps {
public:
    // lamps.md, Overlapping effects: one effect for each mode, five on a lamp or a strip
    static constexpr uint8_t MAX_EFFECTS = 5;
    // the lighting model's t_frame, lamps.md, Frames
    static constexpr uint32_t FRAME_US = 10000;
    // room for the largest built-in effect, Run; an effect a game writes has to fit it as well
    static constexpr size_t EFFECT_BYTES = 16;

    // the groups the config places on the chains, in the order of the playfield's lamps
    enum class Group : uint8_t {
        PERIMETER_LEFT,
        PERIMETER_RIGHT,
        CEILING,
        PLAYFIELD_LOWER,
        PLAYFIELD_MIDDLE,
        PLAYFIELD_UPPER
    };
    static constexpr uint8_t GROUPS = 6;
    static constexpr uint8_t NO_CHAIN = 0xFF;   // a group the config leaves out

    // the config's lamps.<group>, which has no default: the group's chain and LEDs, and on the
    // playfield the LEDs of each lamp. A group without a chain holds no lamps
    struct GroupSettings {
        uint8_t chain = NO_CHAIN;          // 0 to 5 for chains A to F
        uint16_t leds = 0;                 // the LEDs on that chain
        const uint16_t* lamps = nullptr;   // playfield only: each lamp's LEDs, read until begin()
        uint16_t lampCount = 0;            // how many counts lamps holds
        const uint16_t* gaps = nullptr;    // hidden LEDs, first and count in pairs
        uint16_t gapCount = 0;             // how many pairs gaps holds
    };
    struct Settings {
        GroupSettings groups[GROUPS];

        GroupSettings& of(Group g) { return groups[uint8_t(g)]; }
    };

    // Writes the LEDs of each chain that the groups take into leds, for the lighting driver's
    // configure(). Returns false when two groups name one chain or a group a chain past F; the
    // first group on a chain keeps it.
    static bool chainLeds(const Settings& settings, uint16_t (&leds)[Lighting::CHAINS]);

    explicit Lamps(Lighting& lighting) : lighting(lighting) {}

    // Takes the groups. The main task calls it before begin().
    void configure(const Settings& given) { settings = given; }

    // Places the groups on their chains with the LEDs the lighting driver holds there, and takes
    // the room for the effects and the frames from the heap. A group on a chain an earlier group
    // took holds no lamps. The main task calls it once, after the driver's begin(). Returns false
    // when the heap has no room.
    bool begin();

    // Gets the groups to which the effects of a layer go.
    LampGroups groups(Layer layer);

    // Removes every effect of a layer from every lamp and strip, so the ones below show.
    void remove(Layer layer);
    // Gets how many effects a lamp or strip with five effects has refused.
    uint32_t refused() const { return refusedCount; }

    // Removes every effect from every lamp and strip and sets both brightnesses to 1. The host
    // calls it between two games.
    void clear();

    // Sets how bright the playfield lights and the ambient lights are, each from 0 to 1 of the
    // most the driver's profile allows. A value outside counts as the nearer end.
    void brightness(float playfield, float ambient);

    // Works out every effect and hands the driver a frame once a frame period has passed since
    // the last one. Gets whether it handed one. The main loop calls it in every pass.
    bool update(uint32_t now);

private:
    friend class LampRange;
    friend class Strip;

    // the groups' places in groupChain and groupLeds
    static constexpr uint8_t GROUP_LEFT = uint8_t(Group::PERIMETER_LEFT);
    static constexpr uint8_t GROUP_RIGHT = uint8_t(Group::PERIMETER_RIGHT);
    static constexpr uint8_t GROUP_CEILING = uint8_t(Group::CEILING);
    static constexpr uint8_t FIRST_SECTION = uint8_t(Group::PLAYFIELD_LOWER);

    enum StripId : uint8_t { LEFT, RIGHT, CEILING, PERIMETER, AMBIENT, STRIPS };

    // one effect of a lamp or strip: who set it, how high it lies, when it began and ends, its
    // two paints and its copy of the effect
    struct Entry {
        uint8_t key = 0;
        uint8_t priority = 0;
        bool ends = false;    // whether lasting() set an end
        uint16_t index = 0;   // a playfield lamp's place in the range of the call
        uint16_t count = 1;   // how many lamps that range held
        uint32_t seq = 0;     // the order of the calls, so the later of equal priorities shows
        uint32_t startMs = 0;
        uint32_t endMs = 0;
        Paint from;
        Paint to;
        const Effect* effect = nullptr;   // the copy in bytes
        alignas(Effect) uint8_t bytes[EFFECT_BYTES] = {};

        bool above(const Entry& other) const;
        friend uint8_t priorityOf(const Entry& e) { return e.priority; }
    };
    using Effects = PriorityList<Entry, MAX_EFFECTS>;

    // the LEDs a playfield lamp lights: its chain, its first LED there and how many follow
    struct Span {
        uint8_t chain;
        uint16_t led;
        uint16_t leds;
    };

    // Walks the playfield's sections lamp by lamp, as the settings cut them into spans, and
    // writes the spans into spans when it is given. Gets how many lamps the walk found.
    uint16_t walk(Span* spans) const;

    // where a strip's lamp lies: its LED in the ambient frame, and its position along the strip
    struct Place {
        uint16_t led;
        uint32_t q;
    };

    uint8_t step() const;
    uint32_t pitchQ() const;
    uint16_t sideLamps(uint8_t group) const;
    bool hidden(uint8_t group, uint16_t led) const;
    uint16_t stripSize(uint8_t strip) const;
    Place locate(uint8_t strip, uint16_t i) const;
    uint32_t stripLength(uint8_t strip) const;

    // Advances the component's millisecond clock to now and gets it. The clock counts on across
    // the wrap of nowUs().
    uint32_t clockAt(uint32_t now);

    // Takes a new entry for key at its priority in place of the key's own entry. Gets nullptr
    // when the lamp holds five effects of other keys, which counts as refused.
    Entry* claim(Effects& effects, uint8_t key, uint8_t priority);

    // Gives a new entry the paints and a copy of effect. Gets nullptr when the lamp refused it.
    template <typename E>
    Entry* start(Effects& effects, uint8_t key, uint8_t priority, Paint from, Paint to,
                 const E& effect);

    void expire(uint32_t ms);
    void paintPlayfield(uint32_t ms);
    void paintStrips(uint32_t ms);
    void write();

    static CRGB paint(const Paint& p, CRGB below, uint8_t value, uint8_t place, uint32_t elapsedMs);
    static CRGB mix(const Entry& e, uint32_t ms, const Spot& at, CRGB below);

    Lighting& lighting;
    Settings settings;
    uint8_t groupChain[GROUPS] = {NO_CHAIN, NO_CHAIN, NO_CHAIN, NO_CHAIN, NO_CHAIN, NO_CHAIN};
    uint16_t groupLeds[GROUPS] = {};   // the driver's LEDs on each group's chain at begin()
    // from the heap in begin(): each strip group's LEDs outside its gaps, in the chain's order
    uint16_t* visible[FIRST_SECTION] = {};
    uint16_t visibleCount[FIRST_SECTION] = {};
    uint16_t playfieldLamps = 0;

    // from the heap in begin(): the playfield's effects, and the colors of the last frame before
    // the brightness, which fade() starts from
    Effects* playfieldEffects = nullptr;
    CRGB* playfieldFrame = nullptr;
    Span* spans = nullptr;          // the LEDs of each playfield lamp
    CRGB* ambientFrame = nullptr;   // by LED: the left perimeter, the right, then the ceiling
    Effects stripEffects[STRIPS];
    uint32_t seq = 0;
    uint32_t refusedCount = 0;
    bool clockStarted = false;
    uint32_t clockUs = 0;   // the nowUs() at which clockMs last counted a millisecond
    uint32_t clockMs = 0;
    bool framed = false;
    uint32_t shownUs = 0;   // the update() of the last frame
    float playfieldShare = 1;
    float ambientShare = 1;
};

template <typename E>
Lamps::Entry* Lamps::start(Effects& effects, uint8_t key, uint8_t priority, Paint from, Paint to,
                           const E& effect) {
    static_assert(std::is_base_of<Effect, E>::value, "an effect derives from Effect");
    static_assert(sizeof(E) <= EFFECT_BYTES && alignof(E) <= alignof(Effect),
                  "an effect has to fit Lamps::EFFECT_BYTES");
    static_assert(std::is_trivially_destructible<E>::value, "an effect holds plain values only");
    Entry* e = claim(effects, key, priority);
    if (e == nullptr) {
        return nullptr;
    }
    e->from = from;
    e->to = to;
    // The entry stays in its slot until it is removed, so the effect is copied into it once.
    e->effect = new (e->bytes) E(effect);
    return e;
}

template <typename E> LampRange& LampRange::play(const E& effect, Paint from, Paint to) {
    uint16_t refusals = 0;
    for (uint16_t i = 0; i < count; ++i) {
        Lamps::Entry* e =
            lamps->start(lamps->playfieldEffects[first + i], key, priority, from, to, effect);
        if (e == nullptr) {
            ++refusals;
            continue;
        }
        e->index = i;
        e->count = count;
    }
    if (refusals > 0) {
        DEBUG_LOG("Lamps: %u lamps hold five effects and refuse one of key %u\n", refusals, key);
    }
    return *this;
}

template <typename E> Strip& Strip::play(const E& effect, Paint from, Paint to) {
    if (lamps->start(lamps->stripEffects[strip], key, priority, from, to, effect) == nullptr) {
        DEBUG_LOG("Lamps: strip %u holds five effects and refuses one of key %u\n", strip, key);
    }
    return *this;
}
