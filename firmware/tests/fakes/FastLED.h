// What the lamps take from FastLED, for the tests that run on the computer. FastLED's stub
// platform does not build under PlatformIO's native platform, so this stands in for the colors,
// the palettes and the curves, each computed as FastLED's lib8tion and colorutils compute them.
#pragma once
#include <cstdint>

using fract8 = uint8_t;

/// Scales @p i like lib8tion's scale8 with FASTLED_SCALE8_FIXED: 255 keeps a value whole.
///
/// @param i      The value to scale.
inline uint8_t scale8(uint8_t i, fract8 scale) {
    return uint8_t((uint16_t(i) * (1 + uint16_t(scale))) >> 8);
}

/// Blends @p a and @p b like lib8tion's blend8: @p amountOfB 0 gives @p a, 255 gives @p b.
///
/// @param a          The first value.
/// @param b          The second value.
/// @param amountOfB  How much of @p b the result holds.
inline uint8_t blend8(uint8_t a, uint8_t b, uint8_t amountOfB) {
    uint16_t partial = uint16_t((a << 8) | b);
    partial = uint16_t(partial + b * amountOfB);
    partial = uint16_t(partial - a * amountOfB);
    return uint8_t(partial >> 8);
}

inline uint8_t triwave8(uint8_t in) {
    if (in & 0x80) {
        in = uint8_t(255 - in);
    }
    return uint8_t(in << 1);
}

inline uint8_t ease8InOutCubic(fract8 i) {
    const uint8_t ii = scale8(i, i);
    const uint8_t iii = scale8(ii, i);
    const uint16_t r1 = uint16_t(3 * uint16_t(ii) - 2 * uint16_t(iii));
    return (r1 & 0x100) ? 255 : uint8_t(r1);
}

inline uint8_t cubicwave8(uint8_t in) {
    return ease8InOutCubic(triwave8(in));
}

struct CRGB {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;

    CRGB() = default;
    constexpr CRGB(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}
    constexpr explicit CRGB(uint32_t code)
        : r(uint8_t(code >> 16)), g(uint8_t(code >> 8)), b(uint8_t(code)) {}

    CRGB& nscale8(uint8_t scale) {
        r = scale8(r, scale);
        g = scale8(g, scale);
        b = scale8(b, scale);
        return *this;
    }

    bool operator==(const CRGB& o) const { return r == o.r && g == o.g && b == o.b; }
    bool operator!=(const CRGB& o) const { return !(*this == o); }
};

inline CRGB blend(const CRGB& p1, const CRGB& p2, fract8 amountOfP2) {
    return CRGB(blend8(p1.r, p2.r, amountOfP2), blend8(p1.g, p2.g, amountOfP2),
                blend8(p1.b, p2.b, amountOfP2));
}

typedef uint32_t TProgmemRGBPalette16[16];

/// Gets a palette color like colorutils' ColorFromPalette at full brightness with LINEARBLEND.
///
/// The upper four bits of @p index pick an entry, the lower four blend it into the next one, and
/// the last entry blends into the first.
///
/// @param index  The position in the palette.
inline CRGB ColorFromPalette(const TProgmemRGBPalette16& pal, uint8_t index) {
    const uint8_t hi4 = index >> 4;
    const uint8_t lo4 = index & 0x0F;
    CRGB entry(pal[hi4]);
    if (lo4 == 0) {
        return entry;
    }
    const CRGB next(pal[hi4 == 15 ? 0 : hi4 + 1]);
    const uint8_t f2 = uint8_t(lo4 << 4);
    const uint8_t f1 = uint8_t(255 - f2);
    return CRGB(uint8_t(scale8(entry.r, f1) + scale8(next.r, f2)),
                uint8_t(scale8(entry.g, f1) + scale8(next.g, f2)),
                uint8_t(scale8(entry.b, f1) + scale8(next.b, f2)));
}
