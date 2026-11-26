// Tests on the Teensy whether the PSRAM on the Teensy's underside is found and keeps every byte
// written to it.
//
//   found     Teensyduino found a chip of the size the PSRAM design names
//   card      background.img read from the card into the PSRAM, on a 32-byte step and off it
//   patterns  fixed patterns and pseudo-random sequences written over the whole PSRAM read back
//
// Wiring: the Teensy with its PSRAM chip, and a card in its socket for card, which writes
// background.img once if it is missing. patterns erases whatever the PSRAM holds. The debug
// output's buffer lies there, so patterns sends every waiting debug line first. Every pattern
// takes about half a second, so a quick run writes them all as a soak run does.

#include <Arduino.h>
#include "drivers/storage.h"
#include "logging/debug-log.h"
#include "test-harness.h"
#include "test-media.h"

// docs/parts/psram/design.md, Chip: the chip's size
constexpr uint32_t PSRAM_MB = 8;
constexpr uint32_t BYTES_PER_MB = 1024 * 1024;
constexpr uint32_t LIMIT_S = 30;   // the whole test takes about 8 s

// every bit at 0 and at 1, alternating bits, pairs, nibbles, bytes and half words
constexpr uint32_t PATTERNS[] = {0x00000000, 0xFFFFFFFF, 0x55555555, 0xAAAAAAAA,
                                 0x33333333, 0xCCCCCCCC, 0x0F0F0F0F, 0xF0F0F0F0,
                                 0x00FF00FF, 0xFF00FF00, 0x0000FFFF, 0xFFFF0000};
// the first values of the pseudo-random sequences, none of them 0, where xorshift stays at 0
constexpr uint32_t SEEDS[] = {0x2C9277B5, 0x8E3A51D7, 0x13579BDF, 0x7F4A7C15};
constexpr uint32_t PATTERN_COUNT = sizeof PATTERNS / sizeof PATTERNS[0];
constexpr uint32_t SEED_COUNT = sizeof SEEDS / sizeof SEEDS[0];

// the linker script's EXTMEM section, which starts at the beginning of the PSRAM
extern "C" uint32_t _extram_start;
extern "C" uint32_t _extram_end;
constexpr uint32_t DRAIN_MS = 5000;   // the longest the runner takes to read the waiting lines
constexpr uint32_t IDLE_MS = 3;       // ms without a sent byte that end the drain
// startup.c of the Teensy core: the PSRAM's size in MB, 0 without a chip
extern "C" uint8_t external_psram_size;

// background.img: the picture's header and its pixels
constexpr uint32_t PICTURE_BYTES = media::HEADER_BYTES + media::FULL_BYTES;
// the cache's line, which a read into the PSRAM fills straight when the buffer starts on it
constexpr uint32_t LINE_BYTES = 32;
// one byte past a line's start, so the read takes the storage driver's copy
constexpr uint32_t OFF_LINE = 1;

// room for the picture on a line's start and one byte past it
EXTMEM alignas(LINE_BYTES) static uint8_t picture[PICTURE_BYTES + LINE_BYTES];

// Gets byte i of background.img as media::writePictures() writes it.
static uint8_t backgroundByte(uint32_t i) {
    if (i < media::HEADER_BYTES) {
        return media::headerByte(i, media::RGB565_SWAPPED, media::W, media::H);
    }
    const uint32_t p = (i - media::HEADER_BYTES) / 2;
    const uint16_t c = media::backgroundAt(p % media::W, p / media::W);
    return (i - media::HEADER_BYTES) % 2 == 0 ? uint8_t(c >> 8) : uint8_t(c & 0xFF);
}

// Reads background.img into the PSRAM at an offset from a line's start and compares every byte.
static bool readPicture(uint32_t offset) {
    char path[FILE_PATH_BYTES];
    media::pathOf("background.img", path);
    uint8_t* const into = picture + offset;
    memset(into, 0, PICTURE_BYTES);
    FileRequest r;
    storage.open(path).read(0, into, PICTURE_BYTES, r);
    char seen[64];
    char wanted[64];
    if (!r.wait() || r.bytes() != PICTURE_BYTES) {
        snprintf(seen, sizeof seen, "%u bytes at offset %lu", unsigned(r.bytes()), offset);
        snprintf(wanted, sizeof wanted, "%lu bytes", PICTURE_BYTES);
        test::fail(seen, wanted);
        return false;
    }
    for (uint32_t i = 0; i < PICTURE_BYTES; ++i) {
        if (into[i] != backgroundByte(i)) {
            snprintf(seen, sizeof seen, "0x%02X at byte %lu, offset %lu", into[i], i, offset);
            snprintf(wanted, sizeof wanted, "0x%02X", backgroundByte(i));
            test::fail(seen, wanted);
            return false;
        }
    }
    return true;
}

static uint32_t* words() {
    return &_extram_start;
}

static uint32_t wordCount() {
    return external_psram_size * BYTES_PER_MB / sizeof(uint32_t);
}

// Sends every waiting debug line, so no line waits in the buffer the patterns erase.
static void drainDebugOutput() {
    uint32_t idleMs = 0;
    const uint32_t startMs = millis();
    while (idleMs < IDLE_MS && millis() - startMs < DRAIN_MS) {
        idleMs = sendDebugLog() == 0 ? idleMs + 1 : 0;
        delay(1);
    }
}

// Marsaglia's xorshift32: a sequence that every value fixes, so the check can rebuild it
static uint32_t next(uint32_t x) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

// Writes back what the cache holds and drops it, so the reads that follow come from the chip.
static void flushCache() {
    arm_dcache_flush_delete(words(), wordCount() * sizeof(uint32_t));
}

static bool mismatch(uint32_t i, uint32_t seen, uint32_t wanted) {
    char seenText[64];
    char wantedText[16];
    snprintf(seenText, sizeof seenText, "0x%08lX at 0x%08lX", seen,
             reinterpret_cast<uint32_t>(words() + i));
    snprintf(wantedText, sizeof wantedText, "0x%08lX", wanted);
    test::fail(seenText, wantedText);
    return false;
}

static bool fixedPattern(uint32_t pattern) {
    uint32_t* const w = words();
    const uint32_t n = wordCount();
    for (uint32_t i = 0; i < n; ++i) {
        w[i] = pattern;
    }
    flushCache();
    for (uint32_t i = 0; i < n; ++i) {
        if (w[i] != pattern) {
            return mismatch(i, w[i], pattern);
        }
    }
    return true;
}

static bool sequence(uint32_t seed) {
    uint32_t* const w = words();
    const uint32_t n = wordCount();
    uint32_t x = seed;
    for (uint32_t i = 0; i < n; ++i) {
        w[i] = x;
        x = next(x);
    }
    flushCache();
    x = seed;
    for (uint32_t i = 0; i < n; ++i) {
        if (w[i] != x) {
            return mismatch(i, w[i], x);
        }
        x = next(x);
    }
    return true;
}

static void testFound() {
    if (external_psram_size != PSRAM_MB) {
        char seen[24];
        char wanted[24];
        snprintf(seen, sizeof seen, "%u MB", external_psram_size);
        snprintf(wanted, sizeof wanted, "%lu MB", PSRAM_MB);
        test::fail(seen, wanted);
        return;
    }
    test::pass();
}

static void testCard() {
    if (external_psram_size == 0) {
        test::skip("no PSRAM chip found");
        return;
    }
    if (!storage.begin()) {
        test::skip("no card in the socket");
        return;
    }
    if (!media::writePictures()) {
        test::fail("writing background.img failed", "it reaches the card");
        return;
    }
    if (readPicture(0) && readPicture(OFF_LINE)) {
        test::pass();
    }
}

static void testPatterns() {
    if (external_psram_size == 0) {
        test::skip("no PSRAM chip found");
        return;
    }
    drainDebugOutput();
    const uint32_t t0Ms = millis();
    for (uint32_t k = 0; k < PATTERN_COUNT; ++k) {
        if (!fixedPattern(PATTERNS[k])) {
            return;
        }
    }
    for (uint32_t k = 0; k < SEED_COUNT; ++k) {
        if (!sequence(SEEDS[k])) {
            return;
        }
    }
    Serial.printf("%lu patterns and %lu sequences over %u MB in %lu ms\n", PATTERN_COUNT,
                  SEED_COUNT, external_psram_size, millis() - t0Ms);
    test::pass();
}

static const Test TESTS[] = {
    {"psram.found", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testFound, nullptr},
    {"psram.card", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testCard, nullptr},
    {"psram.patterns", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testPatterns, nullptr},
};

extern const Program PSRAM = {TESTS, sizeof TESTS / sizeof TESTS[0]};
