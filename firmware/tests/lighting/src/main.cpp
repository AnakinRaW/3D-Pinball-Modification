// Tests on the bench whether the lighting driver lights the LEDs within its current limits.
//
//   profiles   begin() picks the 6 A profile for a 6 A adapter and the 3 A profile otherwise
//   begin      without configure(), the driver starts on the 3 A profile
//   colors     the LEDs show pure colors and a rainbow, undimmed (asks at the bench)
//   limits     the LEDs step down to the profile's ambient brightness (asks at the bench)
//   cap        a cap the LEDs exceed dims them by the amount the program computes (asks too)
//   layout-3a  the whole light layout at the 3 A limits stays under the 3 A cap, in memory
//   layout-6a  the same at 6 A
//
// Wiring: three WS2812B on chain A behind the lighting board's level shifter, and the Teensy's
// ground joined to the lighting board's. Chains B to F stay empty. A question waits for y or n.
// Run: open the serial monitor, and the program starts. Each test prints RUN and then PASS, FAIL
// or SKIP, and the last line reads END <passed> <failed> <skipped>. The tests are rows of
// docs/firmware/testing.md.

#include <Arduino.h>
// FastLED 3.10.9's fl/gfx/colorutils.h turns warnings off twice and back on once, so without
// the push and pop -Wunused-parameter and -Wreturn-type stay off for all code after it.
#pragma GCC diagnostic push
#include <FastLED.h>
#pragma GCC diagnostic pop
#include <lighting.h>

constexpr uint16_t BENCH_LEDS = 3;   // the three LEDs on chain A at the bench
using BenchLighting = LightingChains<BENCH_LEDS, 0, 0, 0, 0, 0>;
using NoChains = LightingChains<0, 0, 0, 0, 0, 0>;

// power_mgt.cpp.hpp of FastLED 3.10.9: gMCU_mW, the 25 mA at 5 V its limiter charges for the
// processor in every frame
constexpr uint32_t FASTLED_MCU_MW = 125;
// a cap that the three LEDs at full white exceed, so the limiter has to dim them
constexpr uint32_t BENCH_CAP_MA = 60;
// design.md, Light layout: the playfield action lights, which have no chain yet
constexpr uint16_t ACTION_LIGHTS = 20;
// the chains of the layout tests: the driver's six and the action lights as a seventh
constexpr uint8_t LAYOUT_CHAINS = Lighting::CHAINS + 1;
constexpr uint16_t LAYOUT_ROOM = 79;      // the longest of them, a perimeter
constexpr uint8_t PERIMETER_CHAINS = 2;   // A and B
constexpr uint8_t AMBIENT_CHAINS = 3;     // A to C, the perimeters and the ceiling
constexpr uint32_t HOLD_MS = 2000;
constexpr uint32_t ASK_TIMEOUT_MS = 60000;
constexpr uint32_t RAINBOW_STEP_MS = 20;
constexpr uint8_t HUE_STEP = 256 / BENCH_LEDS;   // three colors spread over the color wheel

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
static NoChains probes[PROFILE_CASE_COUNT];

static BenchLighting lighting;
static CRGB layout[LAYOUT_CHAINS][LAYOUT_ROOM];

static uint32_t passed = 0;
static uint32_t failed = 0;
static uint32_t skipped = 0;

static void run(const char* test) {
    Serial.printf("RUN lighting.%s\n", test);
}

static void pass(const char* test) {
    Serial.printf("PASS lighting.%s\n", test);
    ++passed;
}

static void skip(const char* test, const char* reason) {
    Serial.printf("SKIP lighting.%s %s\n", test, reason);
    ++skipped;
}

static void fail(const char* test, const char* seen, const char* wanted) {
    Serial.printf("FAIL lighting.%s %s expected %s\n", test, seen, wanted);
    ++failed;
}

// Asks a person and waits for y or n. The LEDs keep what they show meanwhile.
static void ask(const char* test, const char* question) {
    while (Serial.available()) {
        Serial.read();
    }
    Serial.printf("ASK lighting.%s %s\n", test, question);
    const uint32_t t0 = millis();
    while (millis() - t0 < ASK_TIMEOUT_MS) {
        const int c = Serial.read();
        if (c == 'y') {
            pass(test);
            return;
        }
        if (c == 'n') {
            fail(test, "n", "y");
            return;
        }
    }
    skip(test, "no answer");
}

static bool sameProfile(const Profile& a, const Profile& b) {
    return a.perimeterStep == b.perimeterStep && a.ambientMax == b.ambientMax &&
           a.playfieldMax == b.playfieldMax && a.capMa == b.capMa;
}

static void fillBench(const CRGB& color) {
    fill_solid(lighting.chain(0), BENCH_LEDS, color);
}

// Shows a frame and holds it. Reports a failure and returns false when the limiter dimmed it.
static bool showUnlimited(const char* test, const char* what) {
    lighting.show();
    if (FastLED.isPowerLimited()) {
        char seen[64];
        snprintf(seen, sizeof seen, "%s dimmed to %u", what, FastLED.getLastShowBrightness());
        fail(test, seen, "no dimming");
        return false;
    }
    delay(HOLD_MS);
    return true;
}

// What FastLED's limiter charges for a set of chains at full brightness, split as it splits it:
// the part no brightness changes, which is the processor, every LED's dark draw and the reserve
// for dithering, and the part the brightness scales.
struct Demand {
    uint32_t fixedMw;
    uint32_t scaledMw;
};

static Demand demandOf(CRGB* const* chains, const uint16_t* counts, uint8_t n) {
    Demand demand = {FASTLED_MCU_MW, 0};
    const uint32_t darkMw = FastLED.getPowerModel().dark_mW;
    for (uint8_t i = 0; i < n; ++i) {
        const fl::span<const CRGB> leds(chains[i], counts[i]);
        const uint32_t unscaledMw = calculate_unscaled_power_mW(leds);
        const uint32_t chainDarkMw = darkMw * counts[i];
        demand.fixedMw += chainDarkMw + dither_reserve_mW(leds);
        demand.scaledMw += unscaledMw > chainDarkMw ? unscaledMw - chainDarkMw : 0;
    }
    return demand;
}

static uint32_t demandMw(const Demand& demand, uint8_t brightness) {
    return demand.fixedMw + scale_power_for_brightness(demand.scaledMw, brightness);
}

// The brightness the limiter gives a demand under a cap, as power_mgt.cpp.hpp works it out with
// FastLED's linear power response.
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
    const char* test = "profiles";
    run(test);
    for (uint8_t i = 0; i < PROFILE_CASE_COUNT; ++i) {
        const ProfileCase& pc = PROFILE_CASES[i];
        if (pc.set) {
            probes[i].configure({uint8_t(pc.adapterA)});
        }
        char seen[64];
        if (!probes[i].begin()) {
            snprintf(seen, sizeof seen, "begin() false at %ld A", pc.adapterA);
            fail(test, seen, "begin() true");
            return;
        }
        const Profile& wanted = pc.sixA ? NoChains::PROFILE_6A : NoChains::PROFILE_3A;
        if (!sameProfile(probes[i].profile(), wanted)) {
            if (pc.set) {
                snprintf(seen, sizeof seen, "cap %lu mA at %ld A", probes[i].profile().capMa,
                         pc.adapterA);
            } else {
                snprintf(seen, sizeof seen, "cap %lu mA without the key",
                         probes[i].profile().capMa);
            }
            fail(test, seen, pc.sixA ? "the 6 A profile" : "the 3 A profile");
            return;
        }
    }
    pass(test);
}

// begin: without configure(), the bench driver starts on the 3 A profile and hands
// FastLED the strip's power model.
static void testBegin() {
    const char* test = "begin";
    run(test);
    if (!lighting.begin()) {
        fail(test, "begin() false", "begin() true");
        return;
    }
    if (!sameProfile(lighting.profile(), BenchLighting::PROFILE_3A)) {
        fail(test, "another profile", "the 3 A profile");
        return;
    }
    const PowerModelRGB model = FastLED.getPowerModel();
    if (model.red_mW != BenchLighting::RED_MW || model.green_mW != BenchLighting::GREEN_MW ||
        model.blue_mW != BenchLighting::BLUE_MW || model.dark_mW != BenchLighting::DARK_MW) {
        char seen[64];
        snprintf(seen, sizeof seen, "power model %u %u %u %u mW", model.red_mW, model.green_mW,
                 model.blue_mW, model.dark_mW);
        fail(test, seen, "28 31 31 3 mW");
        return;
    }
    if (lighting.chain(BenchLighting::CHAINS) != nullptr) {
        fail(test, "a chain past F", "nullptr");
        return;
    }
    pass(test);
}

// colors: the three LEDs show red, green, blue and white at full value, then three colors of the
// rainbow, and the limiter dims none of them under the profile's cap.
static void testColors() {
    const char* test = "colors";
    run(test);
    const CRGB colors[] = {CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::White};
    const char* names[] = {"red", "green", "blue", "white"};
    for (uint8_t i = 0; i < 4; ++i) {
        fillBench(colors[i]);
        if (!showUnlimited(test, names[i])) {
            return;
        }
    }
    fill_rainbow(lighting.chain(0), BENCH_LEDS, 0, HUE_STEP);
    lighting.show();
    ask(test, "Did all three LEDs show red, green, blue and white in turn, and do they now show "
              "three different colors of the rainbow?");
}

// limits: white at the profile's playfield brightness, then at its ambient brightness, then the
// perimeter pattern of the profile at the ambient brightness.
static void testLimits() {
    const char* test = "limits";
    run(test);
    const Profile& p = lighting.profile();
    fillBench(CRGB(CRGB::White).nscale8(p.playfieldMax));
    if (!showUnlimited(test, "the playfield brightness")) {
        return;
    }
    fillBench(CRGB(CRGB::White).nscale8(p.ambientMax));
    if (!showUnlimited(test, "the ambient brightness")) {
        return;
    }
    CRGB* leds = lighting.chain(0);
    for (uint16_t i = 0; i < BENCH_LEDS; ++i) {
        leds[i] = i % p.perimeterStep == 0 ? CRGB(CRGB::White).nscale8(p.ambientMax) : CRGB::Black;
    }
    lighting.show();
    Serial.printf("the profile: playfield %u, ambient %u of 255, perimeter step %u\n",
                  p.playfieldMax, p.ambientMax, p.perimeterStep);
    ask(test, "Did the three white LEDs step down to under half their brightness, and do now only "
              "the first and the third light?");
}

// cap: under the profile's cap the three LEDs at full white stay undimmed. Under a cap they
// exceed, the limiter dims them to the brightness the program's sum gives and keeps their
// estimate inside that cap.
static void testCap() {
    const char* test = "cap";
    run(test);
    char seen[80];
    char wanted[48];
    fillBench(CRGB::White);
    lighting.show();
    if (FastLED.isPowerLimited() || FastLED.getLastShowBrightness() != 255) {
        snprintf(seen, sizeof seen, "brightness %u under %lu mA", FastLED.getLastShowBrightness(),
                 lighting.profile().capMa);
        fail(test, seen, "255");
        return;
    }
    delay(HOLD_MS);

    const uint32_t capMw = BENCH_CAP_MA * BenchLighting::VOLTS;
    CRGB* const chains[] = {lighting.chain(0)};
    const uint16_t counts[] = {BENCH_LEDS};
    const uint8_t sum = limitedBrightness(demandOf(chains, counts, 1), capMw);
    FastLED.setMaxPowerInVoltsAndMilliamps(BenchLighting::VOLTS, BENCH_CAP_MA);
    lighting.show();
    const uint8_t live = FastLED.getLastShowBrightness();
    const uint32_t estimateMw = FastLED.getEstimatedPowerInMilliWatts(true) + FASTLED_MCU_MW;
    Serial.printf("the three LEDs under %lu mA: brightness %u, estimate %lu mW of %lu mW\n",
                  BENCH_CAP_MA, live, estimateMw, capMw);
    delay(HOLD_MS);
    FastLED.setMaxPowerInVoltsAndMilliamps(BenchLighting::VOLTS, lighting.profile().capMa);
    lighting.show();
    if (!FastLED.isPowerLimited() && live == 255) {
        fail(test, "no dimming", "dimming");
        return;
    }
    if (live != sum) {
        snprintf(seen, sizeof seen, "brightness %u", live);
        snprintf(wanted, sizeof wanted, "%u from the program's sum", sum);
        fail(test, seen, wanted);
        return;
    }
    if (estimateMw > capMw) {
        snprintf(seen, sizeof seen, "estimate %lu mW", estimateMw);
        snprintf(wanted, sizeof wanted, "at most %lu mW", capMw);
        fail(test, seen, wanted);
        return;
    }
    ask(test, "Did the three white LEDs dim to about two thirds for two seconds and then come back "
              "to full?");
}

// layout: the whole light layout at full white within one profile's limits, with the action
// lights at the playfield brightness, stays under the profile's cap in FastLED's sum, so the
// limiter dims nothing. The limiter itself only sees the chains added to FastLED, so the test
// repeats its sum with FastLED's public power functions.
static void testLayout(const char* test, const Profile& p) {
    run(test);
    uint16_t counts[LAYOUT_CHAINS];
    CRGB* chains[LAYOUT_CHAINS];
    for (uint8_t c = 0; c < LAYOUT_CHAINS; ++c) {
        counts[c] = c < Lighting::CHAINS ? Lighting::CHAIN_LEDS[c] : ACTION_LIGHTS;
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
        fail(test, seen, wanted);
        return;
    }
    pass(test);
}

void setup() {
    Serial.begin(115200);
    while (!Serial) {
    }
    delay(200);

    testProfiles();
    testBegin();
    testColors();
    testLimits();
    testCap();
    testLayout("layout-3a", Lighting::PROFILE_3A);
    testLayout("layout-6a", Lighting::PROFILE_6A);
    Serial.printf("END %lu %lu %lu\n", passed, failed, skipped);
}

// After the run, a rainbow drifts along the three LEDs at the ambient brightness.
void loop() {
    static uint8_t hue = 0;
    static uint32_t lastMs = 0;
    if (millis() - lastMs < RAINBOW_STEP_MS) {
        return;
    }
    lastMs = millis();
    fill_rainbow(lighting.chain(0), BENCH_LEDS, hue, HUE_STEP);
    for (uint16_t i = 0; i < BENCH_LEDS; ++i) {
        lighting.chain(0)[i].nscale8(lighting.profile().ambientMax);
    }
    lighting.show();
    hue = hue + 1;
}
