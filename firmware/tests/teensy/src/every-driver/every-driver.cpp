// Tests on the Teensy the rules every driver follows, on every driver of firmware/src/drivers. Each
// driver stands in the register below with a function for each row, and a row runs every driver's
// function in turn. A driver that has no case for a row is named in the run's output.
//
//   before-begin  every call before begin() answers at once with false or empty data
//   out-of-range  a setting past its range behaves as the nearer end of the range
//   at-once       a call that starts a long transfer or a move returns while it still runs
//   begin-fails   a driver whose begin() failed answers every later call at once, with no data
//
// begin-fails takes every slot of the driver tick and keeps them, so it runs last and relies on the
// restart before the program.
//
// Wiring: none. The lighting runs on chain F, which has no LEDs at the bench, and sends dark
// frames. Chain A and its LEDs stay untouched. A servo on the servo pin stays at its start
// position.

#include <Arduino.h>
#include <stdarg.h>
// FastLED's fl/gfx/colorutils.h turns warnings off twice and back on once, so without
// the push and pop -Wunused-parameter and -Wreturn-type stay off for all code after it.
#pragma GCC diagnostic push
#include <FastLED.h>
#pragma GCC diagnostic pop
#include "drivers/lighting.h"
#include "drivers/servo-driver.h"
#include "drivers/ws2812-color-encoder.h"
#include "test-harness.h"

// the frame: a WS2812B takes 24 bits, and the lighting driver sends a bit as one symbol
constexpr uint32_t BITS_PER_LED = ws2812::BITS_PER_LED;
constexpr uint32_t BIT_NS = ws2812::SYMBOL_BITS * ws2812::SYMBOL_NS;
constexpr uint32_t NS_PER_US = 1000;
constexpr uint32_t SETTLE_MS = 10;    // longer than a frame, so no transfer is pending
constexpr uint32_t AT_ONCE_US = 50;   // what a call that only sets a value may take
constexpr uint32_t LIMIT_S = 10;      // the longest each test takes, with room to spare
constexpr size_t TEXT = 96;           // the room for what a test saw or wanted

// the chain the lighting runs on, F, which has no LEDs at the bench
constexpr uint8_t BENCH_CHAIN = Lighting::CHAINS - 1;
constexpr uint16_t TOO_MANY_LEDS = 1000;   // more LEDs than a chain takes
// adapter ratings below and above the two ratings
constexpr uint8_t ADAPTER_BELOW_A = Lighting::ADAPTER_MIN_A - 1;
constexpr uint8_t ADAPTER_ABOVE_A = Lighting::ADAPTER_MAX_A + 3;
// an angle past ServoDriver::ANGLE_LIMIT_DEG, and speeds below and above its range
constexpr uint8_t TOO_FAR_DEG = 255;
constexpr uint32_t TOO_SLOW_DEG_S = 0;
constexpr uint32_t TOO_FAST_DEG_S = 5000;

// What a driver's case saw and what it wanted, where it failed.
struct Verdict {
    char seen[TEXT] = "";
    char wanted[TEXT] = "";

    // Notes what the case wanted.
    __attribute__((format(printf, 2, 3))) Verdict& want(const char* format, ...) {
        va_list args;
        va_start(args, format);
        vsnprintf(wanted, sizeof wanted, format, args);
        va_end(args);
        return *this;
    }

    // Notes what the case saw. Returns false, which the case returns as its failure.
    __attribute__((format(printf, 2, 3))) bool saw(const char* format, ...) {
        va_list args;
        va_start(args, format);
        vsnprintf(seen, sizeof seen, format, args);
        va_end(args);
        return false;
    }
};

// One driver of the register: its name and a function for each row, nullptr where the row has no
// case for it. A function returns true where the driver keeps the rule.
struct DriverCase {
    const char* name;
    bool (*beforeBegin)(Verdict&);
    bool (*outOfRange)(Verdict&);
    bool (*atOnce)(Verdict&);
    bool (*beginFails)(Verdict&);
};

// Gets how long fn takes, in microseconds.
template <typename F> static uint32_t timedUs(F fn) {
    const uint32_t t0Us = micros();
    fn();
    return micros() - t0Us;
}

// Gets whether a call that took us microseconds answered at once.
static bool atOnce(uint32_t us, const char* call, Verdict& v) {
    return us <= AT_ONCE_US || v.want("%lu us at most", AT_ONCE_US).saw("%s took %lu us", call, us);
}

// ---- the lighting driver

static Lighting lightingFresh;   // before-begin: never begun
static Lighting lighting;        // out-of-range and at-once: the bench chain past its bound
static bool lightingBegun = false;

// an adapter rating past the range and the cap of the profile it counts as
struct AdapterCase {
    uint8_t adapterA;
    uint32_t capMa;
};
static const AdapterCase ADAPTERS[] = {
    {ADAPTER_BELOW_A, Lighting::PROFILE_3A.capMa},
    {ADAPTER_ABOVE_A, Lighting::PROFILE_6A.capMa},
};

// Configures the lighting with an adapter rating and too many LEDs on the bench chain, and begins
// it. A later begin() keeps the chains of the first.
static bool beginLighting(uint8_t adapterA) {
    Lighting::Settings settings;
    settings.adapterA = adapterA;
    settings.leds[BENCH_CHAIN] = TOO_MANY_LEDS;
    lighting.configure(settings);
    lightingBegun = lighting.begin();
    return lightingBegun;
}

static bool lightingBeforeBegin(Verdict& v) {
    bool shown = true;
    const uint32_t us = timedUs([&] { shown = lightingFresh.show(); });
    if (shown || lightingFresh.chain(0) != nullptr || lightingFresh.leds(0) != 0) {
        return v.want("show() false and no chain").saw("show() true or a chain with LEDs");
    }
    return atOnce(us, "show()", v);
}

static bool lightingOutOfRange(Verdict& v) {
    for (const AdapterCase& a : ADAPTERS) {
        if (!beginLighting(a.adapterA)) {
            return v.want("begin() true").saw("%u A: begin() false", a.adapterA);
        }
        if (lighting.profile().capMa != a.capMa ||
            lighting.leds(BENCH_CHAIN) != Lighting::MAX_CHAIN_LEDS) {
            return v.want("cap %lu mA, %u LEDs", a.capMa, Lighting::MAX_CHAIN_LEDS)
                .saw("%u A: cap %lu mA, %u LEDs", a.adapterA, lighting.profile().capMa,
                     lighting.leds(BENCH_CHAIN));
        }
    }
    return true;
}

// show() returns while the frame of the longest chain still goes out. A second show() right after
// it returns false at once, and one after the frame and its latch returns true.
static bool lightingAtOnce(Verdict& v) {
    if (!lightingBegun && !beginLighting(Lighting::ADAPTER_MIN_A)) {
        return v.want("begin() true").saw("begin() false");
    }
    const uint16_t longest = lighting.leds(BENCH_CHAIN);
    fill_solid(lighting.chain(BENCH_CHAIN), longest, CRGB::Black);
    const uint32_t frameUs = longest * BITS_PER_LED * BIT_NS / NS_PER_US;
    delay(SETTLE_MS);
    bool first = false;
    bool second = true;
    const uint32_t firstUs = timedUs([&] { first = lighting.show(); });
    const uint32_t secondUs = timedUs([&] { second = lighting.show(); });
    Serial.printf("show(): %lu us, the next one %lu us, a frame of %u LEDs %lu us\n", firstUs,
                  secondUs, longest, frameUs);
    if (!first || second) {
        return v.want("true, then false").saw("show() %d, the next one %d", first, second);
    }
    if (firstUs >= frameUs) {
        return v.want("less than the frame's %lu us", frameUs).saw("show() took %lu us", firstUs);
    }
    if (!atOnce(secondUs, "the second show()", v)) {
        return false;
    }
    delay(SETTLE_MS);
    return lighting.show() || v.want("true").saw("show() false after the frame");
}

// ---- the servo driver

static ServoDriver servoFresh;     // before-begin: never begun
static ServoDriver servoStarved;   // begin-fails: begins with the driver tick full
static ServoDriver servoMoving;    // at-once

// out-of-range: settings past a range, each beside a twin at the nearer end
struct ServoPair {
    ServoDriver::Settings past;
    ServoDriver::Settings nearer;
};
static const ServoPair SERVO_PAIRS[] = {
    {{ServoDriver::START_DEG, TOO_FAR_DEG, TOO_FAST_DEG_S},
     {ServoDriver::START_DEG, ServoDriver::ANGLE_LIMIT_DEG, ServoDriver::SPEED_MAX_DEG_S}},
    {{ServoDriver::START_DEG, ServoDriver::ANGLE_LIMIT_DEG, TOO_SLOW_DEG_S},
     {ServoDriver::START_DEG, ServoDriver::ANGLE_LIMIT_DEG, ServoDriver::SPEED_MIN_DEG_S}},
};
constexpr size_t SERVO_PAIR_COUNT = sizeof SERVO_PAIRS / sizeof SERVO_PAIRS[0];
static ServoDriver servoPast[SERVO_PAIR_COUNT];
static ServoDriver servoNearer[SERVO_PAIR_COUNT];

// Gets whether a servo driver answers every call at once with empty data: no move, the start
// position, no travel and no failed part.
static bool servoEmpty(ServoDriver& servo, Verdict& v) {
    const uint32_t us = timedUs([&] { servo.moveTo(ServoDriver::Position::END); });
    if (servo.position() != ServoDriver::Position::START || servo.travelMs() != 0 ||
        servo.failed().parts != 0) {
        return v.want("empty data").saw("a position, a travel or a failed part");
    }
    return atOnce(us, "moveTo()", v);
}

static bool servoBeforeBegin(Verdict& v) {
    return servoEmpty(servoFresh, v);
}

static bool servoOutOfRange(Verdict& v) {
    for (size_t i = 0; i < SERVO_PAIR_COUNT; ++i) {
        const ServoPair& p = SERVO_PAIRS[i];
        servoPast[i].configure(p.past);
        servoNearer[i].configure(p.nearer);
        if (!servoPast[i].begin() || !servoNearer[i].begin()) {
            return v.want("begin() true").saw("begin() false");
        }
        if (servoPast[i].travelMs() != servoNearer[i].travelMs()) {
            return v.want("%lu ms, as at the nearer end", servoNearer[i].travelMs())
                .saw("%u to %u deg at %lu deg/s: %lu ms", p.past.startDeg, p.past.endDeg,
                     p.past.speedDegS, servoPast[i].travelMs());
        }
    }
    return true;
}

// moveTo() returns at once and leaves the move to the driver tick. Both ends lie at the start
// position, so a servo on the pin does not move.
static bool servoAtOnce(Verdict& v) {
    servoMoving.configure(
        {ServoDriver::START_DEG, ServoDriver::START_DEG, ServoDriver::SPEED_DEG_S});
    if (!servoMoving.begin()) {
        return v.want("begin() true").saw("begin() false");
    }
    return atOnce(timedUs([] { servoMoving.moveTo(ServoDriver::Position::END); }), "moveTo()", v);
}

// With every slot of the driver tick taken, begin() returns false and every later call answers at
// once with nothing.
static bool servoBeginFails(Verdict& v) {
    if (servoStarved.begin()) {
        return v.want("false").saw("begin() true with the driver tick full");
    }
    return servoEmpty(servoStarved, v);
}

// ---- the register and the rows

static const DriverCase DRIVERS[] = {
    // begin() fails only when the heap has no room for the colors, which no setting reaches
    {"lighting", lightingBeforeBegin, lightingOutOfRange, lightingAtOnce, nullptr},
    {"servo", servoBeforeBegin, servoOutOfRange, servoAtOnce, servoBeginFails},
};

// Runs a row's case of every driver and reports the first failure, or PASS.
template <bool (*DriverCase::*CASE)(Verdict&)> static void row() {
    for (const DriverCase& d : DRIVERS) {
        if (d.*CASE == nullptr) {
            Serial.printf("%s: no case for this row\n", d.name);
            continue;
        }
        Verdict v;
        if (!(d.*CASE)(v)) {
            char seen[TEXT + 16];
            snprintf(seen, sizeof seen, "%s: %s", d.name, v.seen);
            test::fail(seen, v.wanted);
            return;
        }
    }
    test::pass();
}

static void testBeginFails() {
    uint8_t taken = 0;
    while (driverTick.attach([] {}, DriverTick::PERIOD_US)) {
        ++taken;
    }
    Serial.printf("the driver tick had %u free slots\n", taken);
    row<&DriverCase::beginFails>();
}

static const Test TESTS[] = {
    {"every-driver.before-begin", RunsOn::TEENSY, LIMIT_S, LIMIT_S, row<&DriverCase::beforeBegin>,
     nullptr},
    {"every-driver.out-of-range", RunsOn::TEENSY, LIMIT_S, LIMIT_S, row<&DriverCase::outOfRange>,
     nullptr},
    {"every-driver.at-once", RunsOn::TEENSY, LIMIT_S, LIMIT_S, row<&DriverCase::atOnce>, nullptr},
    {"every-driver.begin-fails", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testBeginFails, nullptr},
};

extern const Program EVERY_DRIVER = {TESTS, sizeof TESTS / sizeof TESTS[0]};
