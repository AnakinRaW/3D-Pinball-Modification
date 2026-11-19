#include "lamp-effects.h"

constexpr uint8_t FULL = 255;   // the value of an effect's second color
constexpr uint32_t Q_PER_MM = 4;
constexpr uint32_t MS_PER_S = 1000;
// an asteroid's tail is a quarter of the round, and at least two lamps long
constexpr uint32_t TAIL_SHARE = 4;
constexpr uint32_t TAIL_LEAST_LAMPS = 2;

uint8_t Steady::value(uint32_t, const Spot&) const {
    return FULL;
}

uint8_t Blink::value(uint32_t ms, const Spot&) const {
    const uint32_t periodMs = uint32_t(onMs) + offMs;
    if (periodMs == 0) {
        return 0;
    }
    return ms % periodMs < onMs ? FULL : 0;
}

// FastLED's cubicwave8 rises from 0 to 255 over the first half of its 256 steps and falls back
// over the second, eased at both ends.
uint8_t Pulse::value(uint32_t ms, const Spot&) const {
    if (periodMs == 0) {
        return 0;
    }
    const uint32_t phase = (ms % periodMs) * (FULL + 1) / periodMs;
    return cubicwave8(uint8_t(phase));
}

// FastLED's ease8InOutCubic eases the fade at both ends, as the pulse does.
uint8_t Fade::value(uint32_t ms, const Spot&) const {
    if (ms >= fadeMs) {
        return FULL;
    }
    return ease8InOutCubic(uint8_t(ms * FULL / fadeMs));
}

// The head moves along the lamps' positions: one position a lamp on the playfield, a quarter
// millimeter on a strip. A lamp lights once the head has reached it.
uint8_t Run::value(uint32_t ms, const Spot& at) const {
    const bool up = direction == Direction::UP;
    uint64_t travelled = 0;   // how far the head has gone since the effect began
    uint32_t x = 0;           // where the lamp lies from the start of the run
    uint32_t pitch = 0;       // how far the head covers a lamp
    uint32_t round = 0;       // how far the head goes before it starts over
    if (at.pitchQ == 0) {
        travelled = step == 0 ? 0 : ms / step;
        x = up ? at.index : uint32_t(at.count - 1 - at.index);
        pitch = 1;
        round = at.count;
    } else {
        travelled = uint64_t(ms) * step * Q_PER_MM / MS_PER_S;
        x = up ? at.mmQ : at.lengthQ - at.mmQ;
        pitch = at.pitchQ;
        round = at.lengthQ + at.pitchQ;
    }
    if (travelled < x) {
        return 0;
    }
    const uint32_t behind = uint32_t((travelled - x) % round);   // how far the head is past it
    switch (pattern) {
    case Pattern::CHASE:
        return behind < pitch ? FULL : 0;
    case Pattern::ASTEROID: {
        const uint32_t least = TAIL_LEAST_LAMPS * pitch;
        const uint32_t tail = round / TAIL_SHARE > least ? round / TAIL_SHARE : least;
        return behind < tail ? uint8_t(FULL - behind * FULL / tail) : 0;
    }
    case Pattern::WIPE:
        return FULL;
    }
    return 0;
}
