// Turns the colors of WS2812B LEDs into the words a FlexIO shifter sends out on a pin.
//
// A WS2812B tells a 0 from a 1 by how long the line stays high. The encoder draws every bit as 13
// steps of 100 ns, called a symbol. A 0 is 3 steps high and 10 low, so 300 ns high. A 1 is 7 steps
// high and 6 low, so 700 ns high. The shifter sends the lowest bit of its word first, so the high
// steps sit in the low bits. Two symbols fill one 32-bit word, so an LED's 24 bits take 12 words.
//
// Design: docs/firmware/drivers/lighting.md.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace ws2812 {

// One step lasts SYMBOL_NS, and a symbol has SYMBOL_BITS steps. The highs lie inside both WS2812B
// datasheets at hand, which differ: T0H 220 to 380 ns and T1H 580 to 1000 ns in the
// WS2812B-2020 V6, T0H 0.4 µs and T1H 0.8 µs, each ±150 ns, in the older WS2812B revision.
constexpr uint32_t SYMBOL_NS = 100;
constexpr uint32_t SYMBOL_BITS = 13;   // 1300 ns a bit
constexpr uint32_t ZERO_HIGHS = 3;     // T0H: 300 ns
constexpr uint32_t ONE_HIGHS = 7;      // T1H: 700 ns
constexpr uint32_t ZERO = (1u << ZERO_HIGHS) - 1;
constexpr uint32_t ONE = (1u << ONE_HIGHS) - 1;
constexpr uint32_t SYMBOLS_PER_WORD = 2;   // 26 of a shifter's 32 bits
constexpr uint32_t WORD_BITS = SYMBOLS_PER_WORD * SYMBOL_BITS;
constexpr uint32_t BITS_PER_LED = 24;   // WS2812B datasheet: 8 bits each of green, red and blue
constexpr uint32_t WORDS_PER_LED = BITS_PER_LED / SYMBOLS_PER_WORD;
constexpr uint32_t BITS_PER_COLOR = 8;
constexpr uint32_t WORDS_PER_COLOR = BITS_PER_COLOR / SYMBOLS_PER_WORD;
constexpr uint32_t PAIRS = 1u << SYMBOLS_PER_WORD;   // the values two LED bits can take

// Gets the word that sends two LED bits. pair holds the two bits as a number from 0 to 3, and its
// upper bit goes out first. Example: pair 0b10 gives the symbol of a 1, then the symbol of a 0.
constexpr uint32_t pairWord(uint8_t pair) {
    uint32_t word = 0;
    for (uint32_t k = 0; k < SYMBOLS_PER_WORD; ++k) {
        const bool one = (pair >> (SYMBOLS_PER_WORD - 1 - k)) & 1;
        word |= (one ? ONE : ZERO) << (SYMBOL_BITS * k);
    }
    return word;
}

// Gets a color value dimmed by a brightness from 0 to 255. Brightness 255 leaves the value as it
// is, and 0 gives 0. The arithmetic is FastLED's scale8(), so the driver dims as FastLED would.
constexpr uint8_t scale(uint8_t value, uint8_t brightness) {
    return uint8_t((uint16_t(value) * (uint16_t(brightness) + 1)) >> BITS_PER_COLOR);
}

// Writes the words of one chain into the buffer.
//   rgb         the chain's colors: three bytes for each LED, red, green, blue, as FastLED's CRGB
//               stores them
//   leds        how many LEDs the chain has
//   brightness  0 to 255, applied to every color
//   words       where the chain's first word goes
//   stride      how many words apart to write them: 1 writes them one after the other, and 6
//               leaves 5 words free between two of them
//   count       how many words to write. Past the last LED they are 0, which keeps the line low.
// A WS2812B takes green first, then red, then blue, each with its most significant bit first.
inline void encodeLane(const uint8_t* rgb, uint16_t leds, uint8_t brightness, uint32_t* words,
                       size_t stride, uint32_t count) {
    constexpr uint8_t RED = 0;
    constexpr uint8_t GREEN = 1;
    constexpr uint8_t BLUE = 2;
    constexpr uint8_t ORDER[] = {GREEN, RED, BLUE};
    constexpr uint8_t COLORS = 3;
    uint32_t w = 0;
    for (uint16_t i = 0; i < leds && w + WORDS_PER_LED <= count; ++i) {
        const uint8_t* led = rgb + size_t(i) * COLORS;
        for (uint8_t c = 0; c < COLORS; ++c) {
            const uint8_t value = scale(led[ORDER[c]], brightness);
            for (uint32_t k = 1; k <= WORDS_PER_COLOR; ++k) {
                const uint8_t pair =
                    (value >> (BITS_PER_COLOR - k * SYMBOLS_PER_WORD)) & (PAIRS - 1);
                words[(w++) * stride] = pairWord(pair);
            }
        }
    }
    for (; w < count; ++w) {
        words[w * stride] = 0;
    }
}

}   // namespace ws2812
