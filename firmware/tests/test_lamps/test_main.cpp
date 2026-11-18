// Tests on the computer whether the lamps show what their effects give, on a fake lighting driver
// whose chains the tests read. The tests set the time through the fake GPT2 counter.
//
//   effects    every effect shows its color at every moment, run() moves its pattern one lamp
//              per step in its direction, and a brightness outside 0 to 1 takes the nearer end
//   overlap    removing the modes laid over a blink shows the effect below each time, and the
//              blink goes on in its old rhythm
//   six-modes  a lamp and a strip refuse and count a sixth effect and keep their five
//   groups     each group holds the lamps the chains and the profile give, a range cuts a part
//              past its end, and the ambient path starts at the left perimeter's first lamp
//   separate   two playfield lamps hold different effects, and clearing one leaves the other
//   spans      a section's lamp counts give a playfield lamp several LEDs, cut at its chain's end
//   chains     a group lies on the chain the config names, and a chain two groups name goes to
//              the first
//   gaps       a strip's hidden LEDs stay dark, and a run steps over them at an even pace
//   playfield-gaps  a playfield section's hidden LEDs stay dark and belong to no lamp, and a gap
//              ends the lamp it meets
//
// Run: pio test -e computer, which runs in CI as well.

#include "components/lamps.h"
#include <unity.h>

constexpr uint32_t US_PER_MS = 1000;
constexpr uint32_t TEST_GAP_US = 1000000;   // between two tests, so their frames never meet
constexpr uint8_t PLAYFIELD_CHAIN = 3;      // chain D, which holds the first playfield lamps
constexpr uint8_t LEFT_CHAIN = 0;           // chain A, the left perimeter
constexpr uint8_t RIGHT_CHAIN = 1;          // chain B, the right perimeter
constexpr uint8_t CEILING_CHAIN = 2;
constexpr uint8_t AMBIENT_SCALE_3A = 102;   // the 3 A profile's ambientMax at the brightness 1
constexpr uint8_t RANGE = 4;                // the lamps a range run covers
constexpr uint16_t STEP_MS = 20;            // a range run's ms per lamp
// a strip run at 125 mm/s crosses a 3 A perimeter lamp, 12.5 mm, in 100 ms
constexpr uint16_t STRIP_MM_PER_S = 125;
constexpr uint32_t STRIP_LAMP_MS = 100;
// the chains the tests configure: 79 LEDs on chain A give 40 lamps on the 3 A profile, and every
// other chain holds a few
constexpr uint16_t TEST_LEDS[Lighting::CHAINS] = {79, 9, 6, 8, 2, 3};
constexpr uint16_t LEFT_LAMPS_3A = 40;

// a palette whose sixteen entries differ, so every index gives its own color
const TProgmemRGBPalette16 TEST_PALETTE = {
    0x100000, 0x200000, 0x300000, 0x400000, 0x500000, 0x600000, 0x700000, 0x800000,
    0x000010, 0x000020, 0x000030, 0x000040, 0x000050, 0x000060, 0x000070, 0x000080,
};

// an effect a game writes: the first color for 50 ms, then the second
class Late : public Effect {
public:
    uint8_t value(uint32_t ms, const Spot&) const override { return ms < 50 ? 0 : 255; }
};

static Lighting lighting;
static Lamps lamps(lighting);
// spans: the playfield LEDs of TEST_LEDS cut into lamps. The lower section's 8 LEDs take 2, 1, 3
// and the 2 left of the 4 asked for. The middle section's 2 take 1 for the 0 asked for and 1, and
// its third count finds no LED. The upper section's 3 LEDs are one lamp each, with no counts.
const uint16_t SPAN_LOWER[] = {2, 1, 3, 4};
const uint16_t SPAN_MIDDLE[] = {0, 1, 5};
constexpr uint16_t SPAN_LAMPS = 9;
static Lamps spanned(lighting);

// chains: the playfield's lower section on chain A with a lamp of 1 LED and one of 3, the ceiling
// on a chain past F and the upper section on chain A, which the lower section takes first
constexpr uint8_t CHAIN_A = 0;
constexpr uint8_t PAST_F = 7;
constexpr uint16_t BENCH_LEDS = 4;
const uint16_t BENCH_LAMPS[] = {1, 3};
static Lighting bench;
static Lamps benchLamps(bench);

// gaps: the right perimeter on chain B, 12 LEDs, with LEDs 2 to 4 and LED 8 hidden
constexpr uint8_t CHAIN_B = 1;
constexpr uint16_t GAP_LEDS = 12;
const uint16_t GAPS[] = {2, 3, 8, 1};
const uint16_t VISIBLE[] = {0, 1, 5, 6, 7, 9, 10, 11};
constexpr uint16_t VISIBLE_COUNT = sizeof VISIBLE / sizeof VISIBLE[0];
// a run at 125 mm/s crosses a 6 A lamp, 6.25 mm, in 50 ms
constexpr uint32_t GAP_LAMP_MS = 50;
static Lighting gapLighting;
static Lamps gapLamps(gapLighting);
// playfield-gaps: the lower section on chain D, 10 LEDs with LED 2 and LEDs 5 and 6 hidden, cut
// into lamps of 2, 1 and 2 LEDs. The third lamp meets the gap after one LED, and LEDs 7 to 9 are
// one lamp each.
constexpr uint8_t CHAIN_D = 3;
constexpr uint16_t PF_GAP_LEDS = 10;
const uint16_t PF_GAP_COUNTS[] = {2, 1, 2};
const uint16_t PF_GAPS[] = {2, 1, 5, 2};
constexpr uint16_t PF_GAP_LAMPS = 6;
constexpr uint8_t DARK = 0xFF;   // an LED no lamp takes
const uint8_t PF_LAMP_OF[PF_GAP_LEDS] = {0, 0, DARK, 1, 2, DARK, DARK, 3, 4, 5};
static Lighting pfGapLighting;
static Lamps pfGapLamps(pfGapLighting);
static uint32_t startUs = 0;

void setUp() {
    lamps.clear();
    lighting.current = Lighting::PROFILE_3A;
    startUs = GPT2_CNT + TEST_GAP_US;
    GPT2_CNT = startUs;
}

void tearDown() {}

// Sets the time to ms after the test's start.
static void at(uint32_t ms) {
    GPT2_CNT = startUs + ms * US_PER_MS;
}

// Hands on the frame at ms after the test's start. Frames lie at least a frame period apart.
static void frame(uint32_t ms) {
    at(ms);
    TEST_ASSERT_TRUE_MESSAGE(lamps.update(GPT2_CNT), "no frame");
}

static CRGB crgb(Color c) {
    return CRGB(c.r, c.g, c.b);
}

static CRGB scaled(CRGB c, uint8_t scale) {
    return c.nscale8(scale);
}

static CRGB playfieldLamp(uint16_t i) {
    return lighting.chain(PLAYFIELD_CHAIN)[i];
}

// a lamp of the left perimeter on the 3 A profile, which lights every second LED
static CRGB leftLamp(uint16_t i) {
    return lighting.chain(LEFT_CHAIN)[2 * i];
}

static void expect(const CRGB& wanted, const CRGB& seen, const char* what) {
    char message[96];
    snprintf(message, sizeof message, "%s: %02x%02x%02x, expected %02x%02x%02x", what, seen.r,
             seen.g, seen.b, wanted.r, wanted.g, wanted.b);
    TEST_ASSERT_TRUE_MESSAGE(wanted == seen, message);
}

static void expectLamp(uint16_t lamp, const CRGB& wanted, const char* what) {
    expect(wanted, playfieldLamp(lamp), what);
}

// effects: each built-in effect, a palette of each kind, the brightness, lasting() and an
// effect of the test's own on the first playfield lamps, and runs on a range and a strip.
static void testEffects() {
    Layer mode = {0, 1};
    LampGroups g = lamps.groups(mode);
    Lamp lamp = g.playfield[0];

    lamp.set(Color::RED);
    frame(0);
    expectLamp(0, crgb(Color::RED), "set");
    lamp.off();
    frame(10);
    expectLamp(0, CRGB(0, 0, 0), "off");

    at(100);
    lamp.blink(Color::GREEN, 100, 50, Off::DARK);
    const uint32_t blinkAt[] = {100, 190, 200, 240, 250, 350};
    const bool blinkOn[] = {true, true, false, false, true, false};
    for (uint8_t i = 0; i < 6; ++i) {
        frame(blinkAt[i]);
        expectLamp(0, blinkOn[i] ? crgb(Color::GREEN) : CRGB(0, 0, 0), "blink");
    }

    at(400);
    lamp.alternate(Color::RED, Color::BLUE, 30, 70);
    frame(400);
    expectLamp(0, crgb(Color::RED), "alternate a");
    frame(430);
    expectLamp(0, crgb(Color::BLUE), "alternate b");
    frame(500);
    expectLamp(0, crgb(Color::RED), "alternate a again");

    at(600);
    lamp.pulse(Color::BLACK, Color::WHITE, 200);
    frame(600);
    expectLamp(0, CRGB(0, 0, 0), "pulse start");
    frame(650);
    expectLamp(0, blend(CRGB(0, 0, 0), crgb(Color::WHITE), cubicwave8(64)), "pulse rising");
    frame(700);
    expectLamp(0, crgb(Color::WHITE), "pulse half");
    frame(800);
    expectLamp(0, CRGB(0, 0, 0), "pulse period");

    at(900);
    lamp.set(Color::BLUE);
    frame(900);
    at(910);
    lamp.fade(Color::RED, 100);
    frame(910);
    expectLamp(0, crgb(Color::BLUE), "fade start");
    frame(960);
    expectLamp(0, blend(crgb(Color::BLUE), crgb(Color::RED), ease8InOutCubic(127)), "fade half");
    frame(1010);
    expectLamp(0, crgb(Color::RED), "fade end");
    frame(1300);
    expectLamp(0, crgb(Color::RED), "fade stays");

    expect(CRGB(76, 0, 0), crgb(Color::RED.dim(30)), "dim");

    at(1400);
    LampRange range = g.playfield.sub(0, RANGE);
    range.set(Paint(TEST_PALETTE, By::POSITION));
    frame(1400);
    for (uint8_t i = 0; i < RANGE; ++i) {
        expectLamp(i, ColorFromPalette(TEST_PALETTE, uint8_t(i * 255 / (RANGE - 1))),
                   "palette by position");
    }
    at(1500);
    lamp.set(Paint(TEST_PALETTE, By::TIME));
    frame(1600);
    expectLamp(0, ColorFromPalette(TEST_PALETTE, 10), "palette by time");
    at(1700);
    lamp.pulse(Color::BLACK, Paint(TEST_PALETTE, By::VALUE), 200);
    frame(1800);
    expectLamp(0, ColorFromPalette(TEST_PALETTE, 255), "palette by value");

    at(1900);
    lamp.set(Color::WHITE);
    g.perimeterLeft.set(Color::WHITE);
    lamps.brightness(0.5f, 0.25f);
    frame(1900);
    expectLamp(0, scaled(crgb(Color::WHITE), 128), "playfield brightness");
    // a quarter of the 3 A profile's ambient brightness, rounded as lamps rounds it
    expect(scaled(crgb(Color::WHITE), uint8_t(AMBIENT_SCALE_3A / 4.0f + 0.5f)), leftLamp(0),
           "ambient brightness");
    lamps.brightness(2, -1);
    frame(1910);
    expectLamp(0, crgb(Color::WHITE), "playfield brightness above 1");
    expect(CRGB(0, 0, 0), leftLamp(0), "ambient brightness below 0");
    lamps.brightness(1, 1);
    g.perimeterLeft.clear();

    at(2000);
    lamp.set(Color::RED).lasting(50);
    frame(2040);
    expectLamp(0, crgb(Color::RED), "before lasting ends");
    frame(2050);
    expectLamp(0, CRGB(0, 0, 0), "after lasting ends");

    at(2100);
    lamp.play(Late(), Color::BLACK, Color::GREEN);
    frame(2140);
    expectLamp(0, CRGB(0, 0, 0), "own effect, first color");
    frame(2150);
    expectLamp(0, crgb(Color::GREEN), "own effect, second color");

    // runs over four lamps: one step is STEP_MS
    const Pattern patterns[] = {Pattern::CHASE, Pattern::WIPE};
    const Direction directions[] = {Direction::UP, Direction::DOWN};
    uint32_t t = 2200;
    for (Pattern p : patterns) {
        for (Direction d : directions) {
            at(t);
            range.run(p, d, Color::RED, STEP_MS, Color::BLACK);
            for (uint8_t k = 0; k < 2 * RANGE; ++k) {
                frame(t + k * STEP_MS);
                for (uint8_t i = 0; i < RANGE; ++i) {
                    const uint8_t from = d == Direction::UP ? i : uint8_t(RANGE - 1 - i);
                    const bool lit = p == Pattern::CHASE ? from == k % RANGE : from <= k;
                    expectLamp(i, lit ? crgb(Color::RED) : CRGB(0, 0, 0),
                               p == Pattern::CHASE ? "chase" : "wipe");
                }
            }
            t += 2 * RANGE * STEP_MS + STEP_MS;
        }
    }
    at(t);
    range.run(Pattern::ASTEROID, Direction::UP, Color::RED, STEP_MS, Color::BLACK);
    frame(t + 3 * STEP_MS);
    expectLamp(3, crgb(Color::RED), "asteroid head");
    expectLamp(2, blend(CRGB(0, 0, 0), crgb(Color::RED), 128), "asteroid tail");
    expectLamp(1, CRGB(0, 0, 0), "asteroid behind its tail");
    range.clear();
    t += 10 * STEP_MS;

    // a run along the left perimeter: one lamp per STRIP_LAMP_MS, up and down
    const uint16_t side = g.perimeterLeft.size();
    TEST_ASSERT_EQUAL_UINT16(LEFT_LAMPS_3A, side);
    for (Direction d : directions) {
        at(t);
        g.perimeterLeft.run(Pattern::CHASE, d, Color::RED, STRIP_MM_PER_S, Color::BLACK);
        for (uint16_t k = 0; k < side; k += 7) {
            frame(t + k * STRIP_LAMP_MS + STRIP_LAMP_MS / 2);
            for (uint16_t i = 0; i < side; ++i) {
                const uint16_t from = d == Direction::UP ? i : uint16_t(side - 1 - i);
                const CRGB wanted = from == k ? crgb(Color::RED) : CRGB(0, 0, 0);
                expect(scaled(wanted, AMBIENT_SCALE_3A), leftLamp(i), "strip chase");
            }
        }
        t += side * STRIP_LAMP_MS + STRIP_LAMP_MS;
    }
}

// overlap: a blink of the lowest mode, two modes over it, removed one after the other, on a
// playfield lamp and on the ambient lights over the left perimeter's blink.
static void testOverlap() {
    Layer low = {1, 1};
    Layer middle = {2, 2};
    Layer high = {3, 3};
    Lamp lamp = lamps.groups(low).playfield[0];
    lamp.blink(Color::GREEN, 100, 100, Off::DARK);
    lamps.groups(low).perimeterLeft.blink(Color::GREEN, 100, 100, Off::DARK);
    at(30);
    lamps.groups(middle).playfield[0].set(Color::RED);
    lamps.groups(middle).ambient.set(Color::RED);
    at(60);
    lamps.groups(high).playfield[0].blink(Color::BLUE, 20, 20);
    lamps.groups(high).ambient.blink(Color::BLUE, 20, 20);
    frame(60);
    expectLamp(0, crgb(Color::BLUE), "high on");
    expect(scaled(crgb(Color::BLUE), AMBIENT_SCALE_3A), leftLamp(0), "high on, strip");
    frame(80);
    expectLamp(0, crgb(Color::RED), "high off shows middle");
    expect(scaled(crgb(Color::RED), AMBIENT_SCALE_3A), leftLamp(0), "middle, strip");

    at(90);
    lamps.remove(high);
    frame(90);
    expectLamp(0, crgb(Color::RED), "middle after high left");
    lamps.remove(middle);
    frame(150);
    expectLamp(0, CRGB(0, 0, 0), "blink off in its rhythm");
    expect(CRGB(0, 0, 0), leftLamp(0), "strip blink off");
    frame(200);
    expectLamp(0, crgb(Color::GREEN), "blink on in its rhythm");
    expect(scaled(crgb(Color::GREEN), AMBIENT_SCALE_3A), leftLamp(0), "strip blink on");

    // equal priorities: the later call shows, and clear() shows the one below again
    Layer same = {4, 1};
    at(210);
    lamps.groups(same).playfield[0].set(Color::WHITE);
    frame(210);
    expectLamp(0, crgb(Color::WHITE), "later of equal priorities");
    lamps.groups(same).playfield[0].clear();
    frame(220);
    expectLamp(0, crgb(Color::GREEN), "clear shows the blink");
}

// six-modes: a playfield lamp and the ceiling, set for six modes.
static void testSixModes() {
    const uint32_t before = lamps.refused();
    Layer modes[] = {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 6}};
    const Color colors[] = {Color::RED,   Color::GREEN, Color::BLUE,
                            Color::WHITE, Color::AMBER, Color::RED.dim(50)};
    for (uint8_t m = 0; m < 6; ++m) {
        lamps.groups(modes[m]).playfield[0].set(colors[m]);
        lamps.groups(modes[m]).ceiling.set(colors[m]);
    }
    TEST_ASSERT_EQUAL_UINT32(before + 2, lamps.refused());
    uint32_t t = 0;
    for (int8_t m = 4; m >= 0; --m) {
        frame(t);
        expectLamp(0, crgb(colors[m]), "the top of the five");
        expect(scaled(crgb(colors[m]), AMBIENT_SCALE_3A), lighting.chain(CEILING_CHAIN)[0],
               "the top of the five, ceiling");
        lamps.remove(modes[m]);
        t += 10;
    }
    frame(t);
    expectLamp(0, CRGB(0, 0, 0), "nothing of the sixth");
    expect(CRGB(0, 0, 0), lighting.chain(CEILING_CHAIN)[0], "nothing of the sixth, ceiling");
}

// groups: the sizes of every group on both profiles, with the chains of TEST_LEDS, and the parts
// of a range at and past its end.
static void testGroups() {
    Layer mode = {0, 1};
    const uint16_t ceiling = TEST_LEDS[2];
    const uint16_t playfield = TEST_LEDS[3] + TEST_LEDS[4] + TEST_LEDS[5];
    const Profile profiles[] = {Lighting::PROFILE_3A, Lighting::PROFILE_6A};
    for (const Profile& p : profiles) {
        lighting.current = p;
        const uint16_t left = (TEST_LEDS[0] + p.perimeterStep - 1) / p.perimeterStep;
        const uint16_t right = (TEST_LEDS[1] + p.perimeterStep - 1) / p.perimeterStep;
        LampGroups g = lamps.groups(mode);
        TEST_ASSERT_EQUAL_UINT16(left, g.perimeterLeft.size());
        TEST_ASSERT_EQUAL_UINT16(right, g.perimeterRight.size());
        TEST_ASSERT_EQUAL_UINT16(ceiling, g.ceiling.size());
        TEST_ASSERT_EQUAL_UINT16(left + right, g.perimeter.size());
        TEST_ASSERT_EQUAL_UINT16(left + ceiling + right, g.ambient.size());
        TEST_ASSERT_EQUAL_UINT16(playfield, g.playfield.size());
    }
    LampGroups g = lamps.groups(mode);
    TEST_ASSERT_EQUAL_UINT16(3, g.playfield.sub(playfield - 3, 10).size());
    TEST_ASSERT_EQUAL_UINT16(0, g.playfield.sub(playfield, 1).size());
    g.playfield[playfield].set(Color::RED);
    frame(0);
    for (uint16_t i = 0; i < TEST_LEDS[3]; ++i) {
        expectLamp(i, CRGB(0, 0, 0), "a lamp past the playfield's end lights none");
    }

    // the ambient path runs up the left, across the ceiling and down the right
    lighting.current = Lighting::PROFILE_6A;
    g.ambient.run(Pattern::WIPE, Direction::UP, Color::BLUE, STEP_MS, Color::BLACK);
    frame(10);
    expect(scaled(crgb(Color::BLUE), Lighting::PROFILE_6A.ambientMax), leftLamp(0),
           "the path's first lamp is the left perimeter's first");
    expect(CRGB(0, 0, 0), lighting.chain(RIGHT_CHAIN)[0], "the right perimeter's bottom dark");
}

// separate: two playfield lamps of one mode with different effects, then one of them cleared.
static void testSeparate() {
    Layer mode = {0, 1};
    LampGroups g = lamps.groups(mode);
    g.playfield[0].blink(Color::GREEN, 100, 100, Off::DARK);
    g.playfield[1].set(Color::RED);
    frame(0);
    expectLamp(0, crgb(Color::GREEN), "lamp 0 blinks");
    expectLamp(1, crgb(Color::RED), "lamp 1 holds its own color");
    expectLamp(2, CRGB(0, 0, 0), "lamp 2 holds nothing");
    at(50);
    g.playfield[1].clear();
    frame(100);
    expectLamp(0, CRGB(0, 0, 0), "lamp 0 blinks off in its rhythm");
    expectLamp(1, CRGB(0, 0, 0), "lamp 1 cleared");
    frame(200);
    expectLamp(0, crgb(Color::GREEN), "lamp 0 blinks on after the other's clear");
}

// spans: each lamp of spanned lights every LED of its span and no other.
static void testSpans() {
    Layer mode = {0, 1};
    LampGroups g = spanned.groups(mode);
    TEST_ASSERT_EQUAL_UINT16(SPAN_LAMPS, g.playfield.size());
    const Color colors[SPAN_LAMPS] = {
        Color::RED,           Color::GREEN,        Color::BLUE,
        Color::WHITE,         Color::AMBER,        Color::RED.dim(50),
        Color::GREEN.dim(50), Color::BLUE.dim(50), Color::WHITE.dim(50)};
    for (uint16_t i = 0; i < SPAN_LAMPS; ++i) {
        g.playfield[i].set(colors[i]);
    }
    at(0);
    TEST_ASSERT_TRUE(spanned.update(GPT2_CNT));
    // the lamp of each LED on chains D, E and F, as SPAN_COUNTS cuts them
    const uint8_t chainD[] = {0, 0, 1, 2, 2, 2, 3, 3};
    const uint8_t chainE[] = {4, 5};
    const uint8_t chainF[] = {6, 7, 8};
    const uint8_t* lampOf[] = {chainD, chainE, chainF};
    for (uint8_t c = 0; c < 3; ++c) {
        for (uint16_t i = 0; i < TEST_LEDS[PLAYFIELD_CHAIN + c]; ++i) {
            expect(crgb(colors[lampOf[c][i]]), lighting.chain(PLAYFIELD_CHAIN + c)[i],
                   "each LED shows its lamp's color");
        }
    }
}

// Hands a Lamps' groups to its lighting driver and starts both. Gets what chainLeds() said.
static bool start(Lighting& driver, Lamps& lamps, const Lamps::Settings& groups) {
    Lighting::Settings settings;
    const bool placed = Lamps::chainLeds(groups, settings.leds);
    driver.configure(settings);
    lamps.configure(groups);
    driver.begin();
    lamps.begin();
    return placed;
}

// chains: the groups land on the chains the settings give, and the second group on chain A and
// the group past F hold no lamps.
static void testChains() {
    Layer mode = {0, 1};
    LampGroups g = benchLamps.groups(mode);
    TEST_ASSERT_EQUAL_UINT16(2, g.playfield.size());
    TEST_ASSERT_EQUAL_UINT16(0, g.ceiling.size());
    TEST_ASSERT_EQUAL_UINT16(0, g.perimeterLeft.size());
    TEST_ASSERT_EQUAL_UINT16(BENCH_LEDS, bench.leds(CHAIN_A));
    g.playfield[0].set(Color::RED);
    g.playfield[1].set(Color::BLUE);
    at(0);
    TEST_ASSERT_TRUE(benchLamps.update(GPT2_CNT));
    expect(crgb(Color::RED), bench.chain(CHAIN_A)[0], "the lower section's first lamp on chain A");
    for (uint16_t i = 1; i < BENCH_LEDS; ++i) {
        expect(crgb(Color::BLUE), bench.chain(CHAIN_A)[i], "its second lamp of 3 LEDs");
    }
}

// gaps: a chase along the right perimeter lights the visible LEDs one after the other, one per
// GAP_LAMP_MS, and never a hidden one; on the 3 A profile every second visible LED is a lamp.
static void testGaps() {
    Layer mode = {0, 1};
    gapLighting.current = Lighting::PROFILE_6A;
    LampGroups g = gapLamps.groups(mode);
    TEST_ASSERT_EQUAL_UINT16(VISIBLE_COUNT, g.perimeterRight.size());
    at(0);
    g.perimeterRight.run(Pattern::CHASE, Direction::UP, Color::RED, STRIP_MM_PER_S, Color::BLACK);
    const uint8_t scale = Lighting::PROFILE_6A.ambientMax;
    for (uint16_t k = 0; k < VISIBLE_COUNT; ++k) {
        at(k * GAP_LAMP_MS + GAP_LAMP_MS / 2);
        TEST_ASSERT_TRUE(gapLamps.update(GPT2_CNT));
        for (uint16_t led = 0; led < GAP_LEDS; ++led) {
            const bool lit = led == VISIBLE[k];
            expect(scaled(lit ? crgb(Color::RED) : CRGB(0, 0, 0), scale),
                   gapLighting.chain(CHAIN_B)[led], "the chase on the visible LEDs");
        }
    }
    gapLighting.current = Lighting::PROFILE_3A;
    TEST_ASSERT_EQUAL_UINT16((VISIBLE_COUNT + 1) / 2, g.perimeterRight.size());
}

// playfield-gaps: hidden LEDs of a playfield section belong to no lamp and stay dark, a lamp
// starts after a gap, and a gap cuts the lamp it meets.
static void testPlayfieldGaps() {
    Layer mode = {0, 1};
    LampGroups g = pfGapLamps.groups(mode);
    TEST_ASSERT_EQUAL_UINT16(PF_GAP_LAMPS, g.playfield.size());
    const Color colors[PF_GAP_LAMPS] = {Color::RED,   Color::GREEN, Color::BLUE,
                                        Color::WHITE, Color::AMBER, Color::RED.dim(50)};
    for (uint16_t i = 0; i < PF_GAP_LAMPS; ++i) {
        g.playfield[i].set(colors[i]);
    }
    at(0);
    TEST_ASSERT_TRUE(pfGapLamps.update(GPT2_CNT));
    for (uint16_t led = 0; led < PF_GAP_LEDS; ++led) {
        const CRGB wanted = PF_LAMP_OF[led] == DARK ? CRGB(0, 0, 0) : crgb(colors[PF_LAMP_OF[led]]);
        expect(wanted, pfGapLighting.chain(CHAIN_D)[led],
               "each LED shows its lamp, a hidden one dark");
    }
}

int main() {
    Lamps::Settings layout;
    for (uint8_t c = 0; c < Lighting::CHAINS; ++c) {
        layout.groups[c].chain = c;
        layout.groups[c].leds = TEST_LEDS[c];
    }
    Lamps::Settings spans = layout;
    spans.of(Lamps::Group::PLAYFIELD_LOWER).lamps = SPAN_LOWER;
    spans.of(Lamps::Group::PLAYFIELD_LOWER).lampCount = sizeof SPAN_LOWER / sizeof SPAN_LOWER[0];
    spans.of(Lamps::Group::PLAYFIELD_MIDDLE).lamps = SPAN_MIDDLE;
    spans.of(Lamps::Group::PLAYFIELD_MIDDLE).lampCount = sizeof SPAN_MIDDLE / sizeof SPAN_MIDDLE[0];
    Lamps::Settings benchGroups;
    benchGroups.of(Lamps::Group::CEILING).chain = PAST_F;
    benchGroups.of(Lamps::Group::PLAYFIELD_LOWER).chain = CHAIN_A;
    benchGroups.of(Lamps::Group::PLAYFIELD_LOWER).leds = BENCH_LEDS;
    benchGroups.of(Lamps::Group::PLAYFIELD_LOWER).lamps = BENCH_LAMPS;
    benchGroups.of(Lamps::Group::PLAYFIELD_LOWER).lampCount = 2;
    benchGroups.of(Lamps::Group::PLAYFIELD_UPPER).chain = CHAIN_A;
    benchGroups.of(Lamps::Group::PLAYFIELD_UPPER).leds = BENCH_LEDS;
    Lamps::Settings gapGroups;
    gapGroups.of(Lamps::Group::PERIMETER_RIGHT).chain = CHAIN_B;
    gapGroups.of(Lamps::Group::PERIMETER_RIGHT).leds = GAP_LEDS;
    gapGroups.of(Lamps::Group::PERIMETER_RIGHT).gaps = GAPS;
    gapGroups.of(Lamps::Group::PERIMETER_RIGHT).gapCount = 2;
    Lamps::Settings pfGapGroups;
    pfGapGroups.of(Lamps::Group::PLAYFIELD_LOWER).chain = CHAIN_D;
    pfGapGroups.of(Lamps::Group::PLAYFIELD_LOWER).leds = PF_GAP_LEDS;
    pfGapGroups.of(Lamps::Group::PLAYFIELD_LOWER).lamps = PF_GAP_COUNTS;
    pfGapGroups.of(Lamps::Group::PLAYFIELD_LOWER).lampCount = 3;
    pfGapGroups.of(Lamps::Group::PLAYFIELD_LOWER).gaps = PF_GAPS;
    pfGapGroups.of(Lamps::Group::PLAYFIELD_LOWER).gapCount = 2;
    // the layout and the spans place every group, the bench's groups two of them
    if (!start(lighting, lamps, layout) || !start(lighting, spanned, spans) ||
        start(bench, benchLamps, benchGroups) || !start(gapLighting, gapLamps, gapGroups) ||
        !start(pfGapLighting, pfGapLamps, pfGapGroups)) {
        return 1;
    }
    UNITY_BEGIN();
    RUN_TEST(testEffects);
    RUN_TEST(testOverlap);
    RUN_TEST(testSixModes);
    RUN_TEST(testGroups);
    RUN_TEST(testSeparate);
    RUN_TEST(testSpans);
    RUN_TEST(testChains);
    RUN_TEST(testGaps);
    RUN_TEST(testPlayfieldGaps);
    return UNITY_END();
}
