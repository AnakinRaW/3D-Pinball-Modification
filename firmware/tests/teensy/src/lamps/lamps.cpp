// Tests on the Teensy whether the lamps component hands the lighting driver its frames at the
// frame period without waiting. The rows stand in docs/firmware/testing.md, Lamps.
//
//   frames  effects on every group for a minute: one frame per frame period, and no frame's
//           update() takes as long as the longest chain the config allows takes to go out
//
// Wiring: three WS2812B on chain A, as for the lighting program. The left perimeter is configured
// on chain A with those three LEDs alone, which show the left perimeter's effects at half the
// ambient brightness and go dark at the end.

#include <Arduino.h>
#include <clock.h>
#include <lamps.h>
#include <ws2812-color-encoder.h>
#include "test-harness.h"

// the frame: a WS2812B takes 24 bits, and the lighting driver sends a bit as one symbol
constexpr uint32_t BITS_PER_LED = ws2812::BITS_PER_LED;
constexpr uint32_t BIT_NS = ws2812::SYMBOL_BITS * ws2812::SYMBOL_NS;
constexpr uint32_t NS_PER_US = 1000;
constexpr uint16_t BENCH_LEDS = 3;   // the three LEDs on chain A at the bench
constexpr uint8_t CHAIN_A = 0;       // the left perimeter's chain at the bench
constexpr uint32_t QUICK_MS = 5000;
constexpr uint32_t SOAK_MS = 60000;   // testing.md: a minute
constexpr uint32_t QUICK_LIMIT_S = 15;
constexpr uint32_t SOAK_LIMIT_S = 75;
constexpr float SHARE = 0.5f;   // half the ambient brightness the profile allows
constexpr uint8_t KEY = 0;      // the one mode these effects belong to
constexpr uint8_t PRIORITY = 1;
// the effects' times, chosen so that every effect changes within a few seconds
constexpr uint16_t STEP_MS = 50;
constexpr uint16_t SPEED_MM_PER_S = 200;
constexpr uint16_t PERIOD_MS = 2000;
constexpr uint16_t ON_MS = 300;

static Lighting lighting;
static Lamps lamps(lighting);

// frames: runs the effects for QUICK_MS or SOAK_MS and checks every frame.
static void testFrames() {
    Lamps::Settings groups;
    groups.of(Lamps::Group::PERIMETER_LEFT).chain = CHAIN_A;
    groups.of(Lamps::Group::PERIMETER_LEFT).leds = BENCH_LEDS;
    Lighting::Settings bench;
    if (!Lamps::chainLeds(groups, bench.leds)) {
        test::fail("chainLeds() false", "true");
        return;
    }
    lighting.configure(bench);
    lamps.configure(groups);
    if (!lighting.begin()) {
        test::fail("lighting begin() false", "true");
        return;
    }
    if (!lamps.begin()) {
        test::fail("lamps begin() false", "true");
        return;
    }
    // the longest chain the config allows, whose frame the period has to outlast
    const uint16_t longest = Lighting::MAX_CHAIN_LEDS;
    const uint32_t chainUs = longest * BITS_PER_LED * BIT_NS / NS_PER_US;

    LampGroups g = lamps.groups({KEY, PRIORITY});
    lamps.clear();
    lamps.brightness(SHARE, SHARE);
    g.playfield.run(Pattern::ASTEROID, Direction::UP, Color::AMBER, STEP_MS);
    g.perimeterLeft.run(Pattern::CHASE, Direction::UP, Color::BLUE, SPEED_MM_PER_S, Color::RED);
    g.perimeterRight.pulse(Color::GREEN, Paint(RainbowColors_p, By::VALUE), PERIOD_MS);
    g.ceiling.blink(Color::WHITE, ON_MS, ON_MS, Off::DARK);
    g.perimeter.fade(Color::RED.dim(50), PERIOD_MS);
    g.ambient.alternate(Paint(RainbowColors_p, By::POSITION), Color::NONE, ON_MS, PERIOD_MS);

    const uint32_t runMs = test::durationMs(QUICK_MS, SOAK_MS);
    uint32_t frames = 0;
    uint32_t shortestUs = UINT32_MAX;
    uint32_t widestUs = 0;
    uint32_t longestFrameUs = 0;
    uint32_t longestPassUs = 0;
    uint32_t lastUs = 0;
    const uint32_t t0Us = nowUs();
    while (!elapsedMs(t0Us, runMs)) {
        const uint32_t now = nowUs();
        const bool framed = lamps.update(now);
        const uint32_t tookUs = nowUs() - now;
        if (!framed) {
            longestPassUs = tookUs > longestPassUs ? tookUs : longestPassUs;
            continue;
        }
        if (frames > 0) {
            const uint32_t gapUs = now - lastUs;
            shortestUs = gapUs < shortestUs ? gapUs : shortestUs;
            widestUs = gapUs > widestUs ? gapUs : widestUs;
        }
        longestFrameUs = tookUs > longestFrameUs ? tookUs : longestFrameUs;
        lastUs = now;
        ++frames;
    }
    const uint32_t ranUs = nowUs() - t0Us;
    lamps.clear();
    while (!lamps.update(nowUs())) {
    }

    Serial.printf("frames: %lu in %lu us, %lu to %lu us apart, update() %lu us with a frame and "
                  "%lu us without, a frame of %u LEDs %lu us\n",
                  frames, ranUs, shortestUs, widestUs, longestFrameUs, longestPassUs, longest,
                  chainUs);
    char seen[64];
    char wanted[64];
    const uint32_t expected = ranUs / Lamps::FRAME_US;
    if (frames + 1 < expected || frames > expected + 1) {
        snprintf(seen, sizeof seen, "%lu frames", frames);
        snprintf(wanted, sizeof wanted, "%lu, one per period", expected);
        test::fail(seen, wanted);
        return;
    }
    if (shortestUs < Lamps::FRAME_US || widestUs >= 2 * Lamps::FRAME_US) {
        snprintf(seen, sizeof seen, "frames %lu to %lu us apart", shortestUs, widestUs);
        snprintf(wanted, sizeof wanted, "one period, at least %lu us", Lamps::FRAME_US);
        test::fail(seen, wanted);
        return;
    }
    if (longestFrameUs >= chainUs) {
        snprintf(seen, sizeof seen, "update() took %lu us", longestFrameUs);
        snprintf(wanted, sizeof wanted, "less than the frame's %lu us", chainUs);
        test::fail(seen, wanted);
        return;
    }
    test::pass();
}

static const Test TESTS[] = {
    {"lamps.frames", RunsOn::TEENSY, QUICK_LIMIT_S, SOAK_LIMIT_S, testFrames, nullptr},
};

extern const Program LAMPS = {TESTS, sizeof TESTS / sizeof TESTS[0]};
