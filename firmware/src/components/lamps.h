// The machine's lamps in fixed groups, with the effects the game's modes run on them. Each lamp
// and each strip holds up to five effects at once and shows the one of the highest priority.
// Every frame period, update() works out each lamp's color and hands the frame to the lighting
// driver.
//
// The config names each group's chain and its LEDs: the two perimeters, the ceiling, and the
// playfield's lower, middle and upper section. A playfield lamp takes one LED or several in a row.
// Hidden LEDs stay dark and belong to no lamp. A strip's lamps are the LEDs between them, every
// one on the ceiling and on the perimeters as the driver's profile steps them. A run on a
// perimeter moves by height, which the config gives for a few of its LEDs, so the same run on
// both perimeters stays level. It keeps its pace behind hidden LEDs. A run on a path jumps over a
// hidden stretch without a pause. The colors and the effects stand in lamp-effects.h.
//
// Design: docs/firmware/components/lamps.md.
#pragma once
#include <Arduino.h>
#include <new>
#include <type_traits>
#include "core/clock.h"
#include "logging/debug-log.h"
#include "components/lamp-effects.h"
#include "game/layer.h"
#include "drivers/lighting.h"
#include "core/priority-list.h"

class Lamps;
class Lamp;

/// The built-in effects set, off, blink, alternate and pulse, which every group of lamps can start.
/// LampRange, Lamp and Strip derive from this class, so the five are written once for all three.
/// Group names the class that derives from it, and each effect starts through that class's play().
template <typename Group> class EffectCalls {
public:
    /// Lights steadily in @p c.
    ///
    /// @param c  The paint the group lights in.
    Group& set(Paint c) { return self().play(Steady(), c, c); }
    /// Lights dark.
    Group& off() { return set(Color::BLACK); }
    /// Lights in @p c for @p onMs, then shows @p off for @p offMs, over and over.
    ///
    /// @param c      The paint the group lights in.
    /// @param onMs   How long the group lights in @p c, in ms.
    /// @param offMs  How long the group shows @p off, in ms.
    /// @param off    What the blink shows while off: the effect below, or a dark lamp.
    Group& blink(Paint c, uint16_t onMs, uint16_t offMs, Off off = Off::THROUGH) {
        return self().play(Blink(onMs, offMs), off == Off::DARK ? Color::BLACK : Color::NONE, c);
    }
    /// Lights in @p a for @p aMs, then in @p b for @p bMs, over and over.
    ///
    /// @param a    The first paint.
    /// @param b    The second paint.
    /// @param aMs  How long the group lights in @p a, in ms.
    /// @param bMs  How long the group lights in @p b, in ms.
    Group& alternate(Paint a, Paint b, uint16_t aMs, uint16_t bMs) {
        return self().play(Blink(aMs, bMs), b, a);
    }
    /// Fades from @p a to @p b and back once per @p periodMs.
    ///
    /// @param a         The first paint.
    /// @param b         The second paint.
    /// @param periodMs  The time of one fade there and back, in ms.
    Group& pulse(Paint a, Paint b, uint16_t periodMs) { return self().play(Pulse(periodMs), a, b); }

private:
    Group& self() { return static_cast<Group&>(*this); }
};

/// Some lamps of the playfield, to which the effects of one layer go
class LampRange : public EffectCalls<LampRange> {
public:
    LampRange() = default;

    /// Gets one lamp of the range.
    ///
    /// A lamp past its end is a lamp of no lamps.
    ///
    /// @param i  The lamp's place in the range.
    Lamp operator[](uint16_t i) const;
    /// Gets a part of the range.
    ///
    /// A part past its end is cut at the end.
    ///
    /// @param first  The part's first lamp in the range.
    /// @param count  How many lamps the part holds.
    LampRange sub(uint16_t first, uint16_t count) const;
    /// Gets how many lamps the range holds.
    uint16_t size() const { return count; }

    /// Starts an effect that mixes @p from into @p to, a built-in one or one a game writes.
    ///
    /// @param effect  The effect to start.
    /// @param from    The first paint of the effect.
    /// @param to      The second paint of the effect.
    template <typename E> LampRange& play(const E& effect, Paint from, Paint to);

    /// Fades each lamp from the color it shows now to @p c within @p ms.
    ///
    /// @param c   The paint to fade to.
    /// @param ms  How long the fade takes, in ms.
    LampRange& fade(Paint c, uint16_t ms);
    /// Moves the pattern in @p c over @p back by one lamp every @p stepMs.
    ///
    /// @param p       The pattern to move.
    /// @param d       The direction it moves in.
    /// @param c       The paint of the pattern.
    /// @param stepMs  The time from lamp to lamp, in ms.
    /// @param back    The paint the pattern moves over.
    LampRange& run(Pattern p, Direction d, Paint c, uint16_t stepMs, Paint back = Color::NONE);
    /// Dates the range's effects back to @p startMs, so they run on as if they had begun then.
    ///
    /// A blink, a pulse or a run shows at once what it would show at this point of its rhythm. A
    /// fade goes on from the color it began with now, as far along as the time since @p startMs.
    /// The end that lasting() sets stays where it is.
    ///
    /// @param startMs  A millis() value. One after now counts as now.
    LampRange& since(uint32_t startMs);
    /// Removes the range's effects @p ms from now.
    ///
    /// @param ms  The time until they go, in ms.
    void lasting(uint16_t ms);
    /// Removes the effects the range's layer set, so the ones below show.
    void clear();

private:
    friend class Lamps;
    LampRange(Lamps* lamps, uint16_t first, uint16_t count, Layer layer)
        : lamps(lamps), first(first), count(count), layer(layer) {}

    Lamps* lamps = nullptr;
    uint16_t first = 0;   // the range's first lamp in the playfield
    uint16_t count = 0;
    Layer layer = {};
};

/// A single lamp of the playfield, to which the effects of one layer go
class Lamp : public EffectCalls<Lamp> {
public:
    Lamp() = default;

    template <typename E> Lamp& play(const E& effect, Paint from, Paint to) {
        one.play(effect, from, to);
        return *this;
    }

    /// Fades the lamp from the color it shows now to @p c within @p ms.
    ///
    /// @param c   The paint to fade to.
    /// @param ms  How long the fade takes, in ms.
    Lamp& fade(Paint c, uint16_t ms) {
        one.fade(c, ms);
        return *this;
    }
    /// Dates the lamp's effects back to @p startMs, as LampRange::since() does.
    ///
    /// @param startMs  A millis() value. One after now counts as now.
    Lamp& since(uint32_t startMs) {
        one.since(startMs);
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

/// A strip of lamps that an effect covers as a whole: a perimeter, the ceiling, or a path through
/// several of them
class Strip : public EffectCalls<Strip> {
public:
    Strip() = default;

    /// Gets how many lamps the strip holds.
    uint16_t size() const;
    /// Gets the strip with a run that jumps over a hidden stretch without a pause.
    ///
    /// Without it, a run on a perimeter moves by height and disappears behind hidden LEDs while it
    /// passes them. A run on a path or on the ceiling always jumps.
    Strip skipGaps() const;

    /// Starts an effect that mixes @p from into @p to, a built-in one or one a game writes.
    ///
    /// @param effect  The effect to start.
    /// @param from    The first paint of the effect.
    /// @param to      The second paint of the effect.
    template <typename E> Strip& play(const E& effect, Paint from, Paint to);

    /// Fades the strip from the color its first lamp shows now to @p c within @p ms.
    ///
    /// @param c   The paint to fade to.
    /// @param ms  How long the fade takes, in ms.
    Strip& fade(Paint c, uint16_t ms);
    /// Moves the pattern in @p c over @p back along the strip at @p mmPerS millimeters a second.
    ///
    /// @param p       The pattern to move.
    /// @param d       The direction it moves in.
    /// @param c       The paint of the pattern.
    /// @param mmPerS  The speed along the strip, in millimeters a second.
    /// @param back    The paint the pattern moves over.
    Strip& run(Pattern p, Direction d, Paint c, uint16_t mmPerS, Paint back = Color::NONE);
    /// Dates the strip's effect back to @p startMs, as LampRange::since() does.
    ///
    /// @param startMs  A millis() value. One after now counts as now.
    Strip& since(uint32_t startMs);
    void lasting(uint16_t ms);
    void clear();

private:
    friend class Lamps;
    Strip(Lamps* lamps, uint8_t strip, Layer layer) : lamps(lamps), strip(strip), layer(layer) {}

    Lamps* lamps = nullptr;
    uint8_t strip = 0;   // which of the lamps component's strips
    Layer layer = {};
    bool inRow = false;   // whether skipGaps() counts the lamps in a row
};

/// The lamp groups, to which the effects of one layer go
struct LampGroups {
    Strip perimeterLeft;
    Strip perimeterRight;
    Strip ceiling;
    /// Both perimeters as one: up the left, across the top, down the right
    Strip perimeter;
    /// The perimeters and the ceiling as one: up the left, across the ceiling, down the right
    Strip ambient;
    LampRange playfield;
};

/// The machine's lamps, each lamp and each strip with up to five effects at once
class Lamps {
public:
    /// The lighting model's t_frame, lamps.md, Frames
    static constexpr uint32_t FRAME_US = 10000;
    /// Room for the largest built-in effect, Run; an effect a game writes has to fit it as well
    static constexpr size_t EFFECT_BYTES = 16;

    /// The groups the config places on the chains, in the order of the playfield's lamps
    enum class Group : uint8_t {
        PERIMETER_LEFT,
        PERIMETER_RIGHT,
        CEILING,
        PLAYFIELD_LOWER,
        PLAYFIELD_MIDDLE,
        PLAYFIELD_UPPER
    };
    static constexpr uint8_t GROUPS = 6;

    /// The config's lamps.<group>, which has no default: the group's chain and LEDs, and on the
    /// playfield the LEDs of each lamp. A group without a chain holds no lamps
    struct GroupSettings {
        uint8_t chain = NO_CHAIN;          // 0 to 5 for chains A to F
        uint16_t leds = 0;                 // the LEDs on that chain
        const uint16_t* lamps = nullptr;   // playfield only: each lamp's LEDs, read until begin()
        uint16_t lampCount = 0;            // how many counts lamps holds
        const uint16_t* gaps = nullptr;    // hidden LEDs, first and count in pairs
        uint16_t gapCount = 0;             // how many pairs gaps holds
        /// Perimeters only: an LED and its height in mm above the playfield's lower end, in pairs
        /// in the order of the LEDs, read until begin()
        const uint16_t* heights = nullptr;
        uint16_t heightCount = 0;   // how many pairs heights holds
    };
    struct Settings {
        GroupSettings groups[GROUPS];

        GroupSettings& of(Group g) { return groups[uint8_t(g)]; }
    };

    /// Writes the LEDs of each chain that the groups take into @p leds, for the lighting driver's
    /// configure().
    ///
    /// The first group on a chain keeps it.
    ///
    /// @param settings  The settings that place the groups on the chains.
    /// @param leds      Takes the LEDs of each chain that the groups take.
    /// @return          False when two groups name one chain or a group a chain past F.
    static bool chainLeds(const Settings& settings, uint16_t (&leds)[Lighting::CHAINS]);

    explicit Lamps(Lighting& lighting) : lighting(lighting) {}

    /// Takes the groups.
    ///
    /// @param given  The settings of the groups.
    /// @note         The main task calls it before begin().
    void configure(const Settings& given) { settings = given; }

    /// Places the groups on their chains with the LEDs the lighting driver holds there, and takes
    /// the room for the effects and the frames from the heap.
    ///
    /// A group on a chain an earlier group took holds no lamps.
    ///
    /// @return  False when the heap has no room.
    /// @note    The main task calls it once, after the driver's begin().
    bool begin();

    /// Gets the groups to which the effects of a layer go.
    ///
    /// @param layer  The layer whose effects go to the groups.
    LampGroups groups(Layer layer);

    /// Removes every effect of a layer from every lamp and strip, so the ones below show.
    ///
    /// @param layer  The layer whose effects go.
    void remove(Layer layer);
    /// Gets how many effects a lamp or strip with five effects has refused.
    uint32_t refused() const { return refusedCount; }

    /// Removes every effect from every lamp and strip and sets both brightnesses to 1.
    ///
    /// @note  The host calls it between two games.
    void clear();

    /// Sets how bright the playfield lights and the ambient lights are, each from 0 to 1 of the
    /// most the driver's profile allows.
    ///
    /// A value outside counts as the nearer end.
    ///
    /// @param playfield  How bright the playfield lights are.
    /// @param ambient    How bright the ambient lights are.
    void brightness(float playfield, float ambient);

    /// Works out every effect and hands the driver a frame once a frame period has passed since
    /// the last one.
    ///
    /// @param now  The time of this pass, as nowUs() gives it.
    /// @return     Whether it handed a frame.
    /// @note       The main loop calls it in every pass with nowUs(). The effects take their time
    ///             from millis().
    bool update(uint32_t now);

private:
    /// lamps.md, Overlapping effects: one effect for each mode, five on a lamp or a strip
    static constexpr uint8_t MAX_EFFECTS = 5;
    static constexpr uint8_t NO_CHAIN = 0xFF;   // a group the config leaves out

    friend class LampRange;
    friend class Strip;

    /// The groups' places in groupChain and groupLeds
    static constexpr uint8_t GROUP_LEFT = uint8_t(Group::PERIMETER_LEFT);
    static constexpr uint8_t GROUP_RIGHT = uint8_t(Group::PERIMETER_RIGHT);
    static constexpr uint8_t GROUP_CEILING = uint8_t(Group::CEILING);
    static constexpr uint8_t FIRST_SECTION = uint8_t(Group::PLAYFIELD_LOWER);

    enum StripId : uint8_t { LEFT, RIGHT, CEILING, PERIMETER, AMBIENT, STRIPS };

    /// One effect of a lamp or strip: who set it, how high it lies, when it began and ends, its
    /// two paints and its copy of the effect
    struct Entry {
        uint8_t key = 0;
        uint8_t priority = 0;
        bool ends = false;    // whether lasting() set an end
        uint16_t index = 0;   // a playfield lamp's place in the range of the call
        uint16_t count = 1;   // how many lamps that range held
        bool inRow = false;   // a strip's lamps counted in a row, as skipGaps() asks
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

    /// The LEDs a playfield lamp lights: its chain, its first LED there and how many follow
    struct Span {
        uint8_t chain;
        uint16_t led;
        uint16_t leds;
    };

    /// Walks the playfield's sections lamp by lamp, as the settings cut them into spans.
    ///
    /// It writes the spans into @p spans when it is given.
    ///
    /// @param spans  Takes the spans when it is not nullptr.
    /// @return       How many lamps the walk found.
    uint16_t walk(Span* spans) const;

    /// Where a strip's lamp lies: its LED in the ambient frame, and its position along the strip
    struct Place {
        uint16_t led;
        uint32_t q;
    };

    uint8_t step() const;
    uint32_t pitchQ() const;
    uint16_t sideLamps(uint8_t group) const;
    bool hidden(uint8_t group, uint16_t led) const;
    void placeHeights(uint8_t group);
    uint32_t topQ(uint8_t group) const;
    uint16_t stripSize(uint8_t strip) const;
    bool byLed(uint8_t strip, bool inRow) const;
    Place locate(uint8_t strip, uint16_t i, bool inRow) const;
    uint32_t stripLength(uint8_t strip, bool inRow) const;

    /// Takes a new entry for @p key at its priority in place of the key's own entry.
    ///
    /// @param effects   The effects of the lamp or strip.
    /// @param key       Who sets the effect.
    /// @param priority  How high the effect lies.
    /// @return          nullptr when the lamp holds five effects of other keys, which counts as
    ///                  refused.
    Entry* claim(Effects& effects, uint8_t key, uint8_t priority);

    /// Gives a new entry the paints and a copy of @p effect.
    ///
    /// @param effects   The effects of the lamp or strip.
    /// @param key       Who sets the effect.
    /// @param priority  How high the effect lies.
    /// @param from      The first paint of the effect.
    /// @param to        The second paint of the effect.
    /// @param effect    The effect to copy.
    /// @return          nullptr when the lamp refused it.
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
    /// From the heap in begin(): each strip group's LEDs outside its gaps, in the chain's order
    uint16_t* visible[FIRST_SECTION] = {};
    uint16_t visibleCount[FIRST_SECTION] = {};
    /// From the heap in begin(): the height of each perimeter LED in quarter millimeters
    uint32_t* heightQ[GROUP_CEILING] = {};
    uint16_t playfieldLamps = 0;

    /// From the heap in begin(): the playfield's effects, and the colors of the last frame before
    /// the brightness, which fade() starts from
    Effects* playfieldEffects = nullptr;
    CRGB* playfieldFrame = nullptr;
    Span* spans = nullptr;          // the LEDs of each playfield lamp
    CRGB* ambientFrame = nullptr;   // by LED: the left perimeter, the right, then the ceiling
    Effects stripEffects[STRIPS];
    uint32_t seq = 0;
    uint32_t refusedCount = 0;
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
    DEBUG_LOG("lamps: key %u starts an effect on lamps %u to %u\n", layer.key, first,
              first + count - 1);
    uint16_t refusals = 0;
    for (uint16_t i = 0; i < count; ++i) {
        Lamps::Entry* e = lamps->start(lamps->playfieldEffects[first + i], layer.key,
                                       layer.priority, from, to, effect);
        if (e == nullptr) {
            ++refusals;
            continue;
        }
        e->index = i;
        e->count = count;
    }
    if (refusals > 0) {
        DEBUG_LOG("Lamps: %u lamps hold five effects and refuse one of key %u\n", refusals,
                  layer.key);
    }
    return *this;
}

template <typename E> Strip& Strip::play(const E& effect, Paint from, Paint to) {
    DEBUG_LOG("lamps: key %u starts an effect on strip %u\n", layer.key, strip);
    Lamps::Entry* e =
        lamps->start(lamps->stripEffects[strip], layer.key, layer.priority, from, to, effect);
    if (e == nullptr) {
        DEBUG_LOG("Lamps: strip %u holds five effects and refuses one of key %u\n", strip,
                  layer.key);
        return *this;
    }
    e->inRow = inRow;
    return *this;
}
