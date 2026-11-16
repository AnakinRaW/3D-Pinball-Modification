// Tests on the Teensy two promises every driver makes, with the servo and the lighting driver.
//
//   begin-fails  a driver whose begin() failed answers every later call at once, with no data
//   at-once      show() returns at once while the LEDs still receive their frame
//
// Wiring: none. LEDs on chain A may stay connected, they stay dark.
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
#include <servo-driver.h>

// lighting design.md, the frame: a WS2812B takes 24 bits, and ObjectFLED sends a bit in 1250 ns
constexpr uint32_t BITS_PER_LED = 24;
constexpr uint32_t BIT_NS = 1250;
constexpr uint32_t SETTLE_MS = 10;    // longer than a frame, so no transfer is pending
constexpr uint32_t AT_ONCE_US = 50;   // what a call that only sets a value may take

static ServoDriver servo;
static Lighting lighting;

static uint32_t passed = 0;
static uint32_t failed = 0;
static uint32_t skipped = 0;

static void run(const char* test) {
    Serial.printf("RUN every-driver.%s\n", test);
}

static void pass(const char* test) {
    Serial.printf("PASS every-driver.%s\n", test);
    ++passed;
}

static void fail(const char* test, const char* seen, const char* wanted) {
    Serial.printf("FAIL every-driver.%s %s expected %s\n", test, seen, wanted);
    ++failed;
}

// begin-fails: with every slot of the driver tick taken, the servo driver's begin() returns false
// and every later call answers at once with nothing. The lighting driver needs no slot, so its
// begin() still returns true.
static void testBeginFails() {
    const char* test = "begin-fails";
    run(test);
    char seen[64];
    uint8_t taken = 0;
    while (driverTick.attach([] {}, DriverTick::PERIOD_US)) {
        ++taken;
    }
    if (taken != DriverTick::SLOTS) {
        snprintf(seen, sizeof seen, "%u slots taken", taken);
        fail(test, seen, "every slot");
        return;
    }
    if (servo.begin()) {
        fail(test, "servo begin() true", "false");
        return;
    }
    const uint32_t t0Us = micros();
    servo.moveTo(ServoDriver::Position::END);
    const uint32_t moveUs = micros() - t0Us;
    if (moveUs > AT_ONCE_US) {
        snprintf(seen, sizeof seen, "servo moveTo() took %lu us", moveUs);
        fail(test, seen, "an answer at once");
        return;
    }
    if (servo.position() != ServoDriver::Position::START || servo.travelMs() != 0 ||
        servo.failed().parts != 0) {
        fail(test, "servo answers with data", "empty data");
        return;
    }
    if (!lighting.begin()) {
        fail(test, "lighting begin() false", "true, it needs no slot");
        return;
    }
    pass(test);
}

// at-once: show() returns while the frame of the longest chain still goes out. A second show()
// right after it waits for that frame.
static void testAtOnce() {
    const char* test = "at-once";
    run(test);
    uint16_t longest = 0;
    for (uint8_t c = 0; c < Lighting::CHAINS; ++c) {
        fill_solid(lighting.chain(c), Lighting::CHAIN_LEDS[c], CRGB::Black);
        longest = Lighting::CHAIN_LEDS[c] > longest ? Lighting::CHAIN_LEDS[c] : longest;
    }
    const uint32_t frameUs = longest * BITS_PER_LED * BIT_NS / 1000;
    delay(SETTLE_MS);
    uint32_t t0Us = micros();
    lighting.show();
    const uint32_t firstUs = micros() - t0Us;
    t0Us = micros();
    lighting.show();
    const uint32_t secondUs = micros() - t0Us;
    Serial.printf("show(): %lu us, the next one %lu us, a frame of %u LEDs %lu us\n", firstUs,
                  secondUs, longest, frameUs);
    if (firstUs >= frameUs) {
        char seen[48];
        char wanted[48];
        snprintf(seen, sizeof seen, "show() took %lu us", firstUs);
        snprintf(wanted, sizeof wanted, "less than the frame's %lu us", frameUs);
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

    testBeginFails();
    testAtOnce();
    Serial.printf("END %lu %lu %lu\n", passed, failed, skipped);
}

void loop() {}
