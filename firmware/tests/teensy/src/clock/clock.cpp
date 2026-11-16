// The clock program of docs/firmware/testing.md. It tests on the Teensy whether nowUs() counts
// microseconds.
//
//   rate  nowUs() and the processor's cycle counter agree over the run, read side by side
//
// Wiring: none, the Teensy alone. Both counters run from the same 24 MHz crystal, so they keep the
// same ratio for good. Each read of nowUs() stands between two reads of the cycle counter, and
// those two bound the microseconds that may have passed.

#include <Arduino.h>
#include <clock.h>
#include "test-harness.h"

constexpr uint32_t QUICK_RUN_MS = 5000;
constexpr uint32_t SOAK_RUN_MS = 60000;   // testing.md, clock rate: a minute
constexpr uint32_t QUICK_LIMIT_S = 15;
constexpr uint32_t SOAK_LIMIT_S = 75;
constexpr uint32_t CYCLES_PER_US = 600;   // F_CPU, 600 MHz, from the crystal's PLL
// GPT2 counts whole microseconds and passes its count across its clock domain, so a read may lie
// up to one count off the time the cycle counter brackets
constexpr uint32_t COUNT_SLACK_US = 1;

// the cycle counter, widened to 64 bits; it wraps every 7.2 s and is read far more often
static uint64_t cycles64() {
    static uint32_t last = 0;
    static uint64_t high = 0;
    const uint32_t now = ARM_DWT_CYCCNT;
    if (now < last) {
        high += 1ull << 32;
    }
    last = now;
    return high | now;
}

static void testRate() {
    const uint64_t start0 = cycles64();
    const uint32_t startUs = nowUs();
    const uint64_t start1 = cycles64();
    const uint32_t runMs = test::durationMs(QUICK_RUN_MS, SOAK_RUN_MS);
    const uint64_t runCycles = static_cast<uint64_t>(runMs) * 1000 * CYCLES_PER_US;
    uint32_t reads = 0;
    uint32_t countedUs = 0;
    for (;;) {
        const uint64_t c0 = cycles64();
        countedUs = nowUs() - startUs;
        const uint64_t c1 = cycles64();
        ++reads;
        // the least and the most time that can lie between the two reads of nowUs()
        const uint64_t leastUs = (c0 - start1) / CYCLES_PER_US;
        const uint64_t mostUs = (c1 - start0 + CYCLES_PER_US - 1) / CYCLES_PER_US;
        if (countedUs + COUNT_SLACK_US < leastUs || countedUs > mostUs + COUNT_SLACK_US) {
            char seen[64];
            char wanted[64];
            snprintf(seen, sizeof seen, "%lu us counted after %lu reads", countedUs, reads);
            snprintf(wanted, sizeof wanted, "%llu to %llu us", leastUs, mostUs);
            test::fail(seen, wanted);
            return;
        }
        if (c0 - start0 >= runCycles) {
            break;
        }
    }
    Serial.printf("%lu reads, nowUs() counted %lu us\n", reads, countedUs);
    test::pass();
}

static const Test TESTS[] = {
    {"clock.rate", RunsOn::TEENSY, QUICK_LIMIT_S, SOAK_LIMIT_S, testRate, nullptr},
};

extern const Program CLOCK = {TESTS, sizeof TESTS / sizeof TESTS[0]};
