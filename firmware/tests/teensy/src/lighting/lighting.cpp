// Tests on the bench whether the lighting driver lights the LEDs within its current limits.
//
//   profiles   begin() picks the 6 A profile for a 6 A adapter and the 3 A profile otherwise
//   begin      without configure(), the driver starts on the 3 A profile and no chain
//   counts     lighting.leds sets each chain's LEDs, dark, within its bound, once
//   colors     the LEDs show pure colors and a rainbow, undimmed (asks at the bench)
//   limits     the LEDs step down to the profile's ambient brightness (asks at the bench)
//   cap        a cap the LEDs exceed dims them by the amount the program computes
//   layout-3a  the whole light layout at the 3 A limits stays under the 3 A cap, in memory
//   layout-6a  the same at 6 A
//
// Wiring: three WS2812B on chain A behind the lighting board's level shifter, and the Teensy's
// ground joined to the lighting board's. Chains B to F stay empty. A bench test lights the three
// LEDs and asks what they showed. The LEDs hold the asked state until the answer comes or the
// question times out.

#include <Arduino.h>
// FastLED 3.10.9's fl/gfx/colorutils.h turns warnings off twice and back on once, so without
// the push and pop -Wunused-parameter and -Wreturn-type stay off for all code after it.
#pragma GCC diagnostic push
#include <FastLED.h>
#pragma GCC diagnostic pop
#include "drivers/lighting.h"
#include "test-harness.h"

constexpr uint16_t BENCH_LEDS = 3;   // the three LEDs on chain A at the bench

// a cap that the three LEDs at full white exceed, so the driver has to dim them
constexpr uint32_t BENCH_CAP_MA = 40;
// design.md, Light layout: the playfield action lights, which have no chain yet
constexpr uint16_t ACTION_LIGHTS = 20;
// the chains of the layout tests: the driver's six and the action lights as a seventh
constexpr uint8_t LAYOUT_CHAINS = Lighting::CHAINS + 1;
// design.md, Light layout: the LEDs planned on chains A to F
constexpr uint16_t LAYOUT_LEDS[Lighting::CHAINS] = {79, 79, 26, 21, 12, 7};
constexpr uint16_t LAYOUT_ROOM = 79;      // the longest of them, a perimeter
constexpr uint8_t PERIMETER_CHAINS = 2;   // A and B
constexpr uint8_t AMBIENT_CHAINS = 3;     // A to C, the perimeters and the ceiling
constexpr uint32_t HOLD_MS = 2000;
// counts: a chain asked for more LEDs than the bound allows
constexpr uint16_t ABOVE_BOUND_LEDS = Lighting::MAX_CHAIN_LEDS + 1;
constexpr uint32_t ASK_TIMEOUT_MS = 60000;
constexpr uint8_t HUE_STEP = 256 / BENCH_LEDS;   // three colors spread over the color wheel
// the longest each test takes: its holds and its question with room to spare
constexpr uint32_t TEENSY_LIMIT_S = 10;
constexpr uint32_t ASK_LIMIT_S = 90;

// one driver without chains for each config begin() is tried with in profiles
struct ProfileCase {
    bool set;
    int32_t adapterA;
    bool sixA;
};
constexpr ProfileCase PROFILE_CASES[] = {
    {false, 0, false}, {true, 2, false}, {true, 3, false},
    {true, 5, false},  {true, 6, true},  {true, 7, true},
};
constexpr uint8_t PROFILE_CASE_COUNT = sizeof(PROFILE_CASES) / sizeof(PROFILE_CASES[0]);
static Lighting probes[PROFILE_CASE_COUNT];

static Lighting lighting;
static Lighting fresh;     // begin: never configured
static Lighting counted;   // counts: configured twice
static CRGB layout[LAYOUT_CHAINS][LAYOUT_ROOM];

// Asks a person and waits for y or n. The LEDs keep what they show meanwhile.
static void ask(const char* question) {
    switch (test::ask(question, ASK_TIMEOUT_MS)) {
    case test::Answer::YES:
        test::pass();
        break;
    case test::Answer::NO:
        test::fail("n", "y");
        break;
    case test::Answer::NONE:
        test::skip("no answer");
        break;
    }
}

// Starts the bench driver with the bench's LEDs on chain A. Every call adds chain A to FastLED
// again, since another program's driver may have taken its pin since. Reports a failure and
// returns false where begin() fails.
static bool begun() {
    Lighting::Settings bench;
    bench.leds[0] = BENCH_LEDS;
    lighting.configure(bench);
    if (!lighting.begin()) {
        test::fail("begin() false", "begin() true");
        return false;
    }
    return true;
}

static bool sameProfile(const Profile& a, const Profile& b) {
    return a.perimeterStep == b.perimeterStep && a.ambientMax == b.ambientMax &&
           a.playfieldMax == b.playfieldMax && a.capMa == b.capMa;
}

static void fillBench(const CRGB& color) {
    fill_solid(lighting.chain(0), BENCH_LEDS, color);
}

// Shows a frame and holds it. Reports a failure and returns false when show() refused it or the
// cap dimmed it.
static bool showUnlimited(const char* what) {
    if (!lighting.show()) {
        test::fail("show() false", "show() true");
        return false;
    }
    if (lighting.brightness() != 255) {
        char seen[64];
        snprintf(seen, sizeof seen, "%s dimmed to %u", what, lighting.brightness());
        test::fail(seen, "no dimming");
        return false;
    }
    delay(HOLD_MS);
    return true;
}

// What FastLED's power model charges for a set of chains at full brightness, split as
// calculate_max_brightness_for_power_mW() splits it: every LED's dark draw, which no brightness
// changes, and the part the brightness scales.
struct Demand {
    uint32_t fixedMw;
    uint32_t scaledMw;
};

static Demand demandOf(CRGB* const* chains, const uint16_t* counts, uint8_t n) {
    Demand demand = {0, 0};
    const uint32_t darkMw = FastLED.getPowerModel().dark_mW;
    for (uint8_t i = 0; i < n; ++i) {
        const fl::span<const CRGB> leds(chains[i], counts[i]);
        const uint32_t unscaledMw = calculate_unscaled_power_mW(leds);
        const uint32_t chainDarkMw = darkMw * counts[i];
        demand.fixedMw += chainDarkMw;
        demand.scaledMw += unscaledMw > chainDarkMw ? unscaledMw - chainDarkMw : 0;
    }
    return demand;
}

static uint32_t demandMw(const Demand& demand, uint8_t brightness) {
    return demand.fixedMw + scale_power_for_brightness(demand.scaledMw, brightness);
}

// The brightness a demand gets under a cap, as power_mgt.cpp.hpp works it out with FastLED's
// linear power response.
static uint8_t limitedBrightness(const Demand& demand, uint32_t capMw) {
    if (demandMw(demand, 255) <= capMw) {
        return 255;
    }
    if (demand.scaledMw == 0 || capMw <= demand.fixedMw) {
        return 0;
    }
    uint8_t brightness = uint8_t(power_ratio_of_255(capMw - demand.fixedMw, demand.scaledMw));
    while (brightness > 0 && demandMw(demand, brightness) > capMw) {
        --brightness;
    }
    return brightness;
}

// profiles: begin() selects the 6 A profile for an adapter of 6 A and the 3 A profile for
// anything less, the default and values outside the range included.
static void testProfiles() {
    for (uint8_t i = 0; i < PROFILE_CASE_COUNT; ++i) {
        const ProfileCase& pc = PROFILE_CASES[i];
        if (pc.set) {
            Lighting::Settings settings;
            settings.adapterA = uint8_t(pc.adapterA);
            probes[i].configure(settings);
        }
        char seen[64];
        if (!probes[i].begin()) {
            snprintf(seen, sizeof seen, "begin() false at %ld A", pc.adapterA);
            test::fail(seen, "begin() true");
            return;
        }
        const Profile& wanted = pc.sixA ? Lighting::PROFILE_6A : Lighting::PROFILE_3A;
        if (!sameProfile(probes[i].profile(), wanted)) {
            if (pc.set) {
                snprintf(seen, sizeof seen, "cap %lu mA at %ld A", probes[i].profile().capMa,
                         pc.adapterA);
            } else {
                snprintf(seen, sizeof seen, "cap %lu mA without the key",
                         probes[i].profile().capMa);
            }
            test::fail(seen, pc.sixA ? "the 6 A profile" : "the 3 A profile");
            return;
        }
    }
    test::pass();
}

// begin: without configure(), the driver starts on the 3 A profile with no chain, hands FastLED
// the strip's power model and refuses to show.
static void testBegin() {
    if (!fresh.begin()) {
        test::fail("begin() false", "begin() true");
        return;
    }
    if (!sameProfile(fresh.profile(), Lighting::PROFILE_3A)) {
        test::fail("another profile", "the 3 A profile");
        return;
    }
    const PowerModelRGB model = FastLED.getPowerModel();
    if (model.red_mW != Lighting::RED_MW || model.green_mW != Lighting::GREEN_MW ||
        model.blue_mW != Lighting::BLUE_MW || model.dark_mW != Lighting::DARK_MW) {
        char seen[64];
        snprintf(seen, sizeof seen, "power model %u %u %u %u mW", model.red_mW, model.green_mW,
                 model.blue_mW, model.dark_mW);
        test::fail(seen, "28 31 31 3 mW");
        return;
    }
    for (uint8_t c = 0; c < Lighting::CHAINS; ++c) {
        if (fresh.leds(c) != 0 || fresh.chain(c) != nullptr) {
            char seen[48];
            snprintf(seen, sizeof seen, "chain %u with %u LEDs", c, fresh.leds(c));
            test::fail(seen, "no LEDs on any chain");
            return;
        }
    }
    if (fresh.chain(Lighting::CHAINS) != nullptr || fresh.leds(Lighting::CHAINS) != 0) {
        test::fail("a chain past F", "nullptr and no LEDs");
        return;
    }
    if (fresh.show()) {
        test::fail("show() true without LEDs", "show() false");
        return;
    }
    test::pass();
}

// counts: chain A configured above the bound holds the bound's LEDs, all dark, and the other
// chains hold none. A second configure() and begin() keep the chains of the first. Nothing is
// shown.
static void testCounts() {
    Lighting::Settings settings;
    settings.leds[0] = ABOVE_BOUND_LEDS;
    counted.configure(settings);
    if (!counted.begin()) {
        test::fail("begin() false", "begin() true");
        return;
    }
    char seen[64];
    char wanted[48];
    if (counted.leds(0) != Lighting::MAX_CHAIN_LEDS || counted.chain(0) == nullptr) {
        snprintf(seen, sizeof seen, "%u LEDs on chain A", counted.leds(0));
        snprintf(wanted, sizeof wanted, "the bound's %u", Lighting::MAX_CHAIN_LEDS);
        test::fail(seen, wanted);
        return;
    }
    for (uint16_t i = 0; i < counted.leds(0); ++i) {
        if (counted.chain(0)[i] != CRGB(CRGB::Black)) {
            snprintf(seen, sizeof seen, "LED %u not dark", i);
            test::fail(seen, "every LED dark");
            return;
        }
    }
    for (uint8_t c = 1; c < Lighting::CHAINS; ++c) {
        if (counted.leds(c) != 0 || counted.chain(c) != nullptr) {
            snprintf(seen, sizeof seen, "chain %u with %u LEDs", c, counted.leds(c));
            test::fail(seen, "no LEDs");
            return;
        }
    }
    settings.leds[0] = BENCH_LEDS;
    counted.configure(settings);
    if (!counted.begin() || counted.leds(0) != Lighting::MAX_CHAIN_LEDS) {
        snprintf(seen, sizeof seen, "%u LEDs after the second begin()", counted.leds(0));
        snprintf(wanted, sizeof wanted, "the first begin()'s %u", Lighting::MAX_CHAIN_LEDS);
        test::fail(seen, wanted);
        return;
    }
    test::pass();
}

// colors: the three LEDs show red, green, blue and white at full value, then three colors of the
// rainbow, and the cap dims none of them.
static void testColors() {
    if (!begun()) {
        return;
    }
    const CRGB colors[] = {CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::White};
    const char* names[] = {"red", "green", "blue", "white"};
    for (uint8_t i = 0; i < 4; ++i) {
        fillBench(colors[i]);
        if (!showUnlimited(names[i])) {
            return;
        }
    }
    fill_rainbow(lighting.chain(0), BENCH_LEDS, 0, HUE_STEP);
    lighting.show();
    ask("Did all three LEDs show red, green, blue and white in turn, and do they now show "
        "three different colors of the rainbow?");
}

// limits: white at the profile's playfield brightness, then at its ambient brightness, then the
// perimeter pattern of the profile at the ambient brightness.
static void testLimits() {
    if (!begun()) {
        return;
    }
    const Profile& p = lighting.profile();
    fillBench(CRGB(CRGB::White).nscale8(p.playfieldMax));
    if (!showUnlimited("the playfield brightness")) {
        return;
    }
    fillBench(CRGB(CRGB::White).nscale8(p.ambientMax));
    if (!showUnlimited("the ambient brightness")) {
        return;
    }
    CRGB* leds = lighting.chain(0);
    for (uint16_t i = 0; i < BENCH_LEDS; ++i) {
        leds[i] = i % p.perimeterStep == 0 ? CRGB(CRGB::White).nscale8(p.ambientMax) : CRGB::Black;
    }
    lighting.show();
    Serial.printf("the profile: playfield %u, ambient %u of 255, perimeter step %u\n",
                  p.playfieldMax, p.ambientMax, p.perimeterStep);
    ask("Did the three white LEDs step down to under half their brightness, and do now only "
        "the first and the third light?");
}

// cap: under the profile's cap the three LEDs at full white keep their brightness. Under a cap
// they exceed, the driver's brightness is the one the program's sum gives, and the LEDs' draw at
// it stays inside that cap. Nothing is shown.
static void testCap() {
    if (!begun()) {
        return;
    }
    char seen[80];
    char wanted[48];
    CRGB white[BENCH_LEDS];
    fill_solid(white, BENCH_LEDS, CRGB::White);
    const uint8_t underProfile =
        Lighting::brightnessUnder(white, BENCH_LEDS, lighting.profile().capMa);
    if (underProfile != 255) {
        snprintf(seen, sizeof seen, "brightness %u under %lu mA", underProfile,
                 lighting.profile().capMa);
        test::fail(seen, "255");
        return;
    }
    const uint32_t capMw = BENCH_CAP_MA * Lighting::VOLTS;
    CRGB* const chains[] = {white};
    const uint16_t counts[] = {BENCH_LEDS};
    const Demand demand = demandOf(chains, counts, 1);
    const uint8_t sum = limitedBrightness(demand, capMw);
    const uint8_t live = Lighting::brightnessUnder(white, BENCH_LEDS, BENCH_CAP_MA);
    Serial.printf("the three LEDs under %lu mA: brightness %u, %lu mW of %lu mW\n", BENCH_CAP_MA,
                  live, demandMw(demand, live), capMw);
    if (live == 255) {
        test::fail("no dimming", "dimming");
        return;
    }
    if (live != sum) {
        snprintf(seen, sizeof seen, "brightness %u", live);
        snprintf(wanted, sizeof wanted, "%u from the program's sum", sum);
        test::fail(seen, wanted);
        return;
    }
    if (demandMw(demand, live) > capMw) {
        snprintf(seen, sizeof seen, "%lu mW", demandMw(demand, live));
        snprintf(wanted, sizeof wanted, "at most %lu mW", capMw);
        test::fail(seen, wanted);
        return;
    }
    test::pass();
}

// layout: the whole light layout at full white within one profile's limits, with the action
// lights at the playfield brightness, stays under the profile's cap in FastLED's sum, so the
// driver dims nothing. The action lights have no chain yet, so the test sums the layout with
// FastLED's public power functions.
static void testLayout(const Profile& p) {
    uint16_t counts[LAYOUT_CHAINS];
    CRGB* chains[LAYOUT_CHAINS];
    for (uint8_t c = 0; c < LAYOUT_CHAINS; ++c) {
        counts[c] = c < Lighting::CHAINS ? LAYOUT_LEDS[c] : ACTION_LIGHTS;
        chains[c] = layout[c];
        for (uint16_t i = 0; i < counts[c]; ++i) {
            const bool ambient = c < AMBIENT_CHAINS;
            const bool lit = c >= PERIMETER_CHAINS || i % p.perimeterStep == 0;
            const uint8_t brightness = ambient ? p.ambientMax : p.playfieldMax;
            layout[c][i] = lit ? CRGB(CRGB::White).nscale8(brightness) : CRGB::Black;
        }
    }
    const Demand demand = demandOf(chains, counts, LAYOUT_CHAINS);
    const uint32_t capMw = p.capMa * Lighting::VOLTS;
    const uint32_t fullMw = demandMw(demand, 255);
    Serial.printf("the layout: %lu mA at 5 V, of which %lu mA fixed, under a cap of %lu mA\n",
                  fullMw / Lighting::VOLTS, demand.fixedMw / Lighting::VOLTS, p.capMa);
    if (fullMw > capMw) {
        char seen[64];
        char wanted[32];
        snprintf(seen, sizeof seen, "%lu mA, dimmed to %u", fullMw / Lighting::VOLTS,
                 limitedBrightness(demand, capMw));
        snprintf(wanted, sizeof wanted, "at most %lu mA", p.capMa);
        test::fail(seen, wanted);
        return;
    }
    test::pass();
}

static void testLayout3a() {
    testLayout(Lighting::PROFILE_3A);
}

static void testLayout6a() {
    testLayout(Lighting::PROFILE_6A);
}

static const Test TESTS[] = {
    {"lighting.profiles", RunsOn::TEENSY, TEENSY_LIMIT_S, TEENSY_LIMIT_S, testProfiles, nullptr},
    {"lighting.begin", RunsOn::TEENSY, TEENSY_LIMIT_S, TEENSY_LIMIT_S, testBegin, nullptr},
    {"lighting.counts", RunsOn::TEENSY, TEENSY_LIMIT_S, TEENSY_LIMIT_S, testCounts, nullptr},
    {"lighting.colors", RunsOn::BENCH, ASK_LIMIT_S, ASK_LIMIT_S, testColors, nullptr},
    {"lighting.limits", RunsOn::BENCH, ASK_LIMIT_S, ASK_LIMIT_S, testLimits, nullptr},
    {"lighting.cap", RunsOn::TEENSY, TEENSY_LIMIT_S, TEENSY_LIMIT_S, testCap, nullptr},
    {"lighting.layout-3a", RunsOn::TEENSY, TEENSY_LIMIT_S, TEENSY_LIMIT_S, testLayout3a, nullptr},
    {"lighting.layout-6a", RunsOn::TEENSY, TEENSY_LIMIT_S, TEENSY_LIMIT_S, testLayout6a, nullptr},
};

extern const Program LIGHTING = {TESTS, sizeof TESTS / sizeof TESTS[0]};
