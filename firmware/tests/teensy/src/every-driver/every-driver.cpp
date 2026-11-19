// Tests on the Teensy two promises every driver makes, with the servo and the lighting driver.
//
//   begin-fails  a driver whose begin() failed answers every later call at once, with no data
//   at-once      show() returns at once while the LEDs still receive their frame, and refuses
//                a second frame until the first has gone out
//
// Wiring: none. LEDs on chain A may stay connected, they stay dark. begin-fails takes every slot of
// the driver tick and keeps them, so it relies on the restart before the program.

#include <Arduino.h>
// FastLED 3.10.9's fl/gfx/colorutils.h turns warnings off twice and back on once, so without
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
constexpr uint32_t SETTLE_MS = 10;    // longer than a frame, so no transfer is pending
constexpr uint32_t AT_ONCE_US = 50;   // what a call that only sets a value may take
constexpr uint32_t LIMIT_S = 10;      // the longest each test takes, with room to spare

// design.md, Light layout: a perimeter, the longest chain planned, which at-once sends dark
constexpr uint16_t LONGEST_LEDS = 79;

static ServoDriver servo;
static Lighting lighting;
static bool lightingBegun = false;

// Starts the lighting with the longest chain on chain A.
static bool beginLighting() {
    Lighting::Settings settings;
    settings.leds[0] = LONGEST_LEDS;
    lighting.configure(settings);
    return lighting.begin();
}

// begin-fails: with every slot of the driver tick taken, the servo driver's begin() returns false
// and every later call answers at once with nothing. The lighting driver needs no slot, so its
// begin() still returns true.
static void testBeginFails() {
    char seen[64];
    uint8_t taken = 0;
    while (driverTick.attach([] {}, DriverTick::PERIOD_US)) {
        ++taken;
    }
    if (taken != DriverTick::SLOTS) {
        snprintf(seen, sizeof seen, "%u slots taken", taken);
        test::fail(seen, "every slot");
        return;
    }
    if (servo.begin()) {
        test::fail("servo begin() true", "false");
        return;
    }
    const uint32_t t0Us = micros();
    servo.moveTo(ServoDriver::Position::END);
    const uint32_t moveUs = micros() - t0Us;
    if (moveUs > AT_ONCE_US) {
        snprintf(seen, sizeof seen, "servo moveTo() took %lu us", moveUs);
        test::fail(seen, "an answer at once");
        return;
    }
    if (servo.position() != ServoDriver::Position::START || servo.travelMs() != 0 ||
        servo.failed().parts != 0) {
        test::fail("servo answers with data", "empty data");
        return;
    }
    lightingBegun = beginLighting();
    if (!lightingBegun) {
        test::fail("lighting begin() false", "true, it needs no slot");
        return;
    }
    test::pass();
}

// at-once: show() returns while the frame of the longest chain still goes out. A second show()
// right after it returns false at once, and one after the frame and its latch returns true.
static void testAtOnce() {
    if (!lightingBegun) {
        lightingBegun = beginLighting();
        if (!lightingBegun) {
            test::fail("lighting begin() false", "true");
            return;
        }
    }
    const uint16_t longest = lighting.leds(0);
    fill_solid(lighting.chain(0), longest, CRGB::Black);
    const uint32_t frameUs = longest * BITS_PER_LED * BIT_NS / 1000;
    delay(SETTLE_MS);
    uint32_t t0Us = micros();
    const bool first = lighting.show();
    const uint32_t firstUs = micros() - t0Us;
    t0Us = micros();
    const bool second = lighting.show();
    const uint32_t secondUs = micros() - t0Us;
    Serial.printf("show(): %lu us, the next one %lu us, a frame of %u LEDs %lu us\n", firstUs,
                  secondUs, longest, frameUs);
    char seen[48];
    char wanted[48];
    if (!first || second) {
        snprintf(seen, sizeof seen, "show() %d, the next one %d", first, second);
        test::fail(seen, "true, then false");
        return;
    }
    if (firstUs >= frameUs || secondUs >= AT_ONCE_US) {
        snprintf(seen, sizeof seen, "show() took %lu us, the next one %lu us", firstUs, secondUs);
        snprintf(wanted, sizeof wanted, "less than %lu us and %lu us", frameUs, AT_ONCE_US);
        test::fail(seen, wanted);
        return;
    }
    delay(SETTLE_MS);
    if (!lighting.show()) {
        test::fail("show() false after the frame", "true");
        return;
    }
    test::pass();
}

static const Test TESTS[] = {
    {"every-driver.begin-fails", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testBeginFails, nullptr},
    {"every-driver.at-once", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testAtOnce, nullptr},
};

extern const Program EVERY_DRIVER = {TESTS, sizeof TESTS / sizeof TESTS[0]};
