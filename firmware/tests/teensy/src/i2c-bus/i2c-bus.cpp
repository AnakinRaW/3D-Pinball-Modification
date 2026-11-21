// Tests on the Teensy whether the I²C bus runs its transfers in turn and reports their errors, on
// Wire2 with the display module's touch controller.
//
//   wire2-probe   the address alone gets an answer from the controller and none from an empty
//                 address
//   wire2-turns   two transfers to the controller, one requested every tick and one after each
//                 run of the first, each run once per request and without an error
//   wire2-repeat  requests during a run add exactly one run
//   wire2-held-low  SCL held at GND ends the reads with the line held low as their error, and
//                 the reads work again once SCL is free (a person holds the wire at the bench)
//
// Wiring: the touch controller on Wire2 through the level converter, as pin-assignment.md gives it.

#include <Arduino.h>
#include "drivers/driver-base.h"
#include "io/i2c-bus.h"
#include "test-harness.h"

constexpr uint8_t TOUCH_ADDRESS = 0x38;   // design.md: the CST026
constexpr uint8_t EMPTY_ADDRESS = 0x39;   // no device on Wire2 answers there
constexpr uint8_t TD_STATUS = 0x02;       // FT6x06 register map: the number of touches
constexpr uint8_t DEVICE_MODE = 0x00;     // FT6x06 register map: the first register
constexpr uint8_t TOUCH_BYTES = 5;
/// RM LPI2C, MSR: NDF, no acknowledge
constexpr uint32_t NO_ANSWER = LPI2C_MSR_NDF;
constexpr uint32_t TURNS_QUICK_MS = 3000;
constexpr uint32_t TURNS_SOAK_MS = 60000;
constexpr uint32_t SETTLE_MS = 10;   // longer than any transfer here
constexpr uint32_t REQUESTS = 3;     // wire2-repeat: the requests during one run
/// RM LPI2C, MSR: PLTF, a line held low past the timeout Wire configures
constexpr uint32_t HELD_LOW = LPI2C_MSR_PLTF;
constexpr uint32_t STEP_TIMEOUT_MS = 60000;   // wire2-held-low: the time for each step
constexpr uint32_t WORKING_MS = 500;          // reads without an error that count as working
constexpr uint32_t LIMIT_S = 10;
constexpr uint32_t HELD_LOW_LIMIT_S = 150;
constexpr uint32_t TURNS_SOAK_LIMIT_S = 75;

static uint8_t touchBytes[TOUCH_BYTES];
static uint8_t modeByte;

static volatile uint32_t aRequests = 0;
static volatile uint32_t aRuns = 0;
static volatile uint32_t bRuns = 0;
static volatile uint32_t failures = 0;
static volatile uint32_t lastErrors = 0;
static volatile bool turning = false;
static volatile bool holding = false;      // wire2-held-low: its tick asks for reads
static volatile uint32_t seenErrors = 0;   // every error flag a read ended with
static volatile uint32_t lastOkMs = 0;     // millis() at the end of the last working read

static io::I2cTransfer probeTouch(TOUCH_ADDRESS, nullptr, 0, nullptr, 0, {});
static io::I2cTransfer probeEmpty(EMPTY_ADDRESS, nullptr, 0, nullptr, 0, {});
static io::I2cTransfer readMode(TOUCH_ADDRESS, &DEVICE_MODE, 1, &modeByte, 1, [](uint32_t errors) {
    bRuns = bRuns + 1;
    if (errors != 0) {
        failures = failures + 1;
        lastErrors = errors;
    }
});
static io::I2cTransfer readTouch(TOUCH_ADDRESS, &TD_STATUS, 1, touchBytes, TOUCH_BYTES,
                                 [](uint32_t errors) {
                                     aRuns = aRuns + 1;
                                     if (errors != 0) {
                                         failures = failures + 1;
                                         lastErrors = errors;
                                         seenErrors = seenErrors | errors;
                                     } else {
                                         lastOkMs = millis();
                                     }
                                     if (turning) {
                                         io::wire2Bus.request(readMode);
                                     }
                                 });

static void testProbe() {
    char seen[64];
    const uint32_t answered = io::wire2Bus.requestAndWait(probeTouch);
    const uint32_t empty = io::wire2Bus.requestAndWait(probeEmpty);
    Serial.printf("0x%02x: errors 0x%lx; 0x%02x: errors 0x%lx\n", TOUCH_ADDRESS, answered,
                  EMPTY_ADDRESS, empty);
    if (answered != 0 || (empty & NO_ANSWER) == 0) {
        snprintf(seen, sizeof seen, "0x%lx and 0x%lx", answered, empty);
        test::fail(seen, "0 and the missing answer");
        return;
    }
    test::pass();
}

static void testTurns() {
    char seen[96];
    io::wire2Bus.attach(readTouch);
    io::wire2Bus.attach(readMode);
    failures = 0;
    turning = true;
    if (!driverTick.attach(
            [] {
                if (turning) {
                    aRequests = aRequests + 1;
                    io::wire2Bus.request(readTouch);
                }
            },
            DriverTick::PERIOD_US)) {
        test::fail("the driver tick refused", "attached");
        return;
    }
    delay(test::durationMs(TURNS_QUICK_MS, TURNS_SOAK_MS));
    turning = false;
    delay(SETTLE_MS);
    Serial.printf("%lu requests, %lu runs of the touch read, %lu of the mode read, %lu failed, "
                  "last errors 0x%lx\n",
                  aRequests, aRuns, bRuns, failures, lastErrors);
    // A tick of 5 ms outlasts both reads together, so every request finds the bus free.
    if (failures != 0 || aRuns != aRequests || bRuns + 1 < aRuns || bRuns > aRuns) {
        snprintf(seen, sizeof seen, "%lu requests, %lu and %lu runs, %lu failed", aRequests, aRuns,
                 bRuns, failures);
        test::fail(seen, "one run each per request, none failed");
        return;
    }
    test::pass();
}

static void testRepeat() {
    char seen[64];
    io::wire2Bus.attach(readTouch);
    delay(SETTLE_MS);
    const uint32_t before = aRuns;
    noInterrupts();
    for (uint32_t i = 0; i < REQUESTS; ++i) {
        io::wire2Bus.request(readTouch);
    }
    interrupts();
    // The first run starts as the interrupt is let in; more requests during it add one run.
    delayMicroseconds(100);
    for (uint32_t i = 0; i < REQUESTS; ++i) {
        io::wire2Bus.request(readTouch);
    }
    delay(SETTLE_MS);
    const uint32_t runs = aRuns - before;
    Serial.printf("%lu runs\n", runs);
    if (runs != 2 || failures != 0) {
        snprintf(seen, sizeof seen, "%lu runs, %lu failed", runs, failures);
        test::fail(seen, "2 runs");
        return;
    }
    test::pass();
}

// wire2-held-low: reads every tick while a person holds SCL at GND and lets it go again.
static void testHeldLow() {
    char seen[96];
    io::wire2Bus.attach(readTouch);
    if (!driverTick.attach(
            [] {
                if (holding) {
                    io::wire2Bus.request(readTouch);
                }
            },
            DriverTick::PERIOD_US)) {
        test::fail("the driver tick refused", "attached");
        return;
    }
    seenErrors = 0;
    holding = true;
    test::step("hold a wire from GND to pin 24, SCL on Wire2, for about a second, then take it "
               "away");
    uint32_t t0 = millis();
    while ((seenErrors & HELD_LOW) == 0 && millis() - t0 < STEP_TIMEOUT_MS) {
        delay(1);
    }
    if ((seenErrors & HELD_LOW) == 0) {
        holding = false;
        snprintf(seen, sizeof seen, "error flags 0x%lx", seenErrors);
        test::fail(seen, "the line held low");
        return;
    }
    // The reads work again once no read has failed for WORKING_MS while reads keep working.
    uint32_t lastFailures = failures;
    uint32_t steadySince = millis();
    bool working = false;
    t0 = millis();
    while (!working && millis() - t0 < STEP_TIMEOUT_MS) {
        if (failures != lastFailures) {
            lastFailures = failures;
            steadySince = millis();
        }
        working = millis() - steadySince >= WORKING_MS && millis() - lastOkMs < WORKING_MS;
        delay(1);
    }
    holding = false;
    Serial.printf("error flags 0x%lx, %lu of %lu reads failed\n", seenErrors, failures, aRuns);
    if (!working) {
        test::fail("no working read after SCL was free", "reads working again");
        return;
    }
    test::pass();
}

static const Test TESTS[] = {
    {"i2c-bus.wire2-probe", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testProbe, nullptr},
    {"i2c-bus.wire2-turns", RunsOn::TEENSY, LIMIT_S, TURNS_SOAK_LIMIT_S, testTurns, nullptr},
    {"i2c-bus.wire2-repeat", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testRepeat, nullptr},
    {"i2c-bus.wire2-held-low", RunsOn::BENCH, HELD_LOW_LIMIT_S, HELD_LOW_LIMIT_S, testHeldLow,
     nullptr},
};

extern const Program I2C_BUS = {TESTS, sizeof TESTS / sizeof TESTS[0]};
