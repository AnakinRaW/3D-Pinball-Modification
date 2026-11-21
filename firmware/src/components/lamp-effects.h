// The colors a lamp shows and the effects that change them over time.
//
// Every effect works with two colors and decides, moment by moment, how much of each the lamp
// shows. A value of 0 shows the first color, 255 the second, and a value between mixes the two.
// A blink jumps between 0 and 255, and a fade glides from 0 to 255. A color is a single Color or
// one of FastLED's palettes.
//
// Design: docs/firmware/components/lamps.md, Effects.
#pragma once
#include <Arduino.h>
// FastLED's fl/gfx/colorutils.h turns warnings off twice and back on once, so without
// the push and pop -Wunused-parameter and -Wreturn-type stay off for all code after it.
#pragma GCC diagnostic push
#include <FastLED.h>
#pragma GCC diagnostic pop

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

// Where a lamp lies among the lamps an effect covers, for an effect that moves along them. The
// positions count quarter millimeters along a strip and stay 0 on the playfield.
struct Spot {
    uint16_t index;     // the lamp's place, 0 for the first
    uint16_t count;     // how many lamps the effect covers
    uint32_t mmQ;       // a strip lamp's position along its strip
    uint32_t lengthQ;   // the position of the strip's last lamp
    uint32_t pitchQ;    // the spacing of the perimeters' lamps, which a run's head covers
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
