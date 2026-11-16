// The driver-tick program of docs/firmware/testing.md. It tests on the Teensy whether the driver
// tick calls its callbacks on time.
//
//   periods   callbacks every 1, 2 and 20 ticks run on, each one exactly on time
//   refusals  attach() turns down a bad period and a ninth callback, and the others run on
//
// Wiring: none, the Teensy alone. Every callback notes the processor's cycle count when it runs,
// so the program can time each run against the one before.

#include <Arduino.h>
#include <driver-base.h>
#include "test-harness.h"

// testing.md, driver-tick periods: callbacks of one, two and twenty ticks for a minute, or for a
// few seconds in a quick run
constexpr uint32_t PERIOD_TICKS[] = {1, 2, 20};
constexpr uint8_t TIMED = sizeof(PERIOD_TICKS) / sizeof(PERIOD_TICKS[0]);
constexpr uint32_t QUICK_RUN_MS = 5000;
constexpr uint32_t SOAK_RUN_MS = 60000;
// refusals: how long the attached callbacks are watched
constexpr uint32_t KEEP_RUNNING_MS = 1000;
// A run counts as on its tick when it lies this close to it. The interrupt's entry varies by far
// less, and a missed or doubled tick moves a run by the whole period.
constexpr uint32_t ON_TICK_NS = 20000;
constexpr uint32_t COUNT_SLACK = 1;   // runs the count may differ by at the window's two ends
// the longest each test takes: its run with room to spare
constexpr uint32_t PERIODS_QUICK_LIMIT_S = 15;
constexpr uint32_t PERIODS_SOAK_LIMIT_S = 75;
constexpr uint32_t REFUSALS_LIMIT_S = 10;

struct Timed {
    uint32_t periodTicks;
    volatile uint32_t runs;
    volatile uint32_t lastCycles;
    volatile uint32_t offTick;   // runs that lay further than ON_TICK_NS from their tick
    volatile uint32_t worstNs;   // the furthest a run lay from its tick
};

static Timed timed[TIMED];
static bool timedAttached = false;
static volatile uint32_t fillerRuns[DriverTick::SLOTS];

static uint32_t cyclesToNs(uint32_t cycles) {
    return uint32_t(uint64_t(cycles) * 1000000000u / F_CPU_ACTUAL);
}

// Runs on the driver tick: times this run against the one before.
static void onTick(Timed* t) {
    const uint32_t now = ARM_DWT_CYCCNT;
    if (t->runs > 0) {
        const uint32_t gapNs = cyclesToNs(now - t->lastCycles);
        const uint32_t wantedNs = t->periodTicks * DriverTick::PERIOD_US * 1000;
        const uint32_t offNs = gapNs > wantedNs ? gapNs - wantedNs : wantedNs - gapNs;
        if (offNs > t->worstNs) {
            t->worstNs = offNs;
        }
        if (offNs > ON_TICK_NS) {
            t->offTick = t->offTick + 1;
        }
    }
    t->lastCycles = now;
    t->runs = t->runs + 1;
}

static uint32_t difference(uint32_t a, uint32_t b) {
    return a > b ? a - b : b - a;
}

// Attaches the timed callbacks once after the start. Reports a failure and returns false where
// attach() refuses one.
static bool attachTimed() {
    if (timedAttached) {
        return true;
    }
    for (uint8_t i = 0; i < TIMED; ++i) {
        timed[i].periodTicks = PERIOD_TICKS[i];
        Timed* t = &timed[i];
        if (!driverTick.attach([t] { onTick(t); }, PERIOD_TICKS[i] * DriverTick::PERIOD_US)) {
            char seen[48];
            snprintf(seen, sizeof seen, "attach() false for %lu ticks", PERIOD_TICKS[i]);
            test::fail(seen, "true");
            return false;
        }
    }
    timedAttached = true;
    return true;
}

// periods: each callback runs as often as its period gives, and every run lies one period after
// the one before.
static void testPeriods() {
    char seen[80];
    char wanted[48];
    const uint32_t t0Us = micros();
    if (!attachTimed()) {
        return;
    }
    delay(test::durationMs(QUICK_RUN_MS, SOAK_RUN_MS));
    const uint32_t elapsedUs = micros() - t0Us;
    for (uint8_t i = 0; i < TIMED; ++i) {
        const Timed& t = timed[i];
        const uint32_t periodUs = t.periodTicks * DriverTick::PERIOD_US;
        const uint32_t expected = elapsedUs / periodUs;
        Serial.printf("every %lu ticks: %lu runs of %lu, %lu off their tick, worst %lu ns\n",
                      t.periodTicks, t.runs, expected, t.offTick, t.worstNs);
        if (difference(t.runs, expected) > COUNT_SLACK) {
            snprintf(seen, sizeof seen, "%lu runs every %lu ticks", t.runs, t.periodTicks);
            snprintf(wanted, sizeof wanted, "%lu +- %lu", expected, COUNT_SLACK);
            test::fail(seen, wanted);
            return;
        }
        if (t.offTick != 0) {
            snprintf(seen, sizeof seen, "%lu runs off their tick every %lu ticks", t.offTick,
                     t.periodTicks);
            test::fail(seen, "0");
            return;
        }
    }
    test::pass();
}

// refusals: attach() refuses an odd period and a callback past the last slot, and the attached
// callbacks keep running.
static void testRefusals() {
    char seen[64];
    if (!attachTimed()) {
        return;
    }
    if (driverTick.attach([] {}, DriverTick::PERIOD_US + DriverTick::PERIOD_US / 2)) {
        test::fail("attach() true for an odd period", "false");
        return;
    }
    if (driverTick.attach([] {}, 0)) {
        test::fail("attach() true for a period of 0", "false");
        return;
    }
    uint8_t fillers = 0;
    for (uint8_t i = TIMED; i < DriverTick::SLOTS; ++i) {
        volatile uint32_t* runs = &fillerRuns[fillers];
        if (!driverTick.attach([runs] { *runs = *runs + 1; }, DriverTick::PERIOD_US)) {
            snprintf(seen, sizeof seen, "attach() false for slot %u", i + 1);
            test::fail(seen, "true");
            return;
        }
        ++fillers;
    }
    if (driverTick.attach([] {}, DriverTick::PERIOD_US)) {
        test::fail("attach() true past the last slot", "false");
        return;
    }
    uint32_t before[TIMED];
    for (uint8_t i = 0; i < TIMED; ++i) {
        before[i] = timed[i].runs;
    }
    const uint32_t fillerBefore = fillerRuns[0];
    delay(KEEP_RUNNING_MS);
    for (uint8_t i = 0; i < TIMED; ++i) {
        const uint32_t expected =
            KEEP_RUNNING_MS * 1000 / (timed[i].periodTicks * DriverTick::PERIOD_US);
        const uint32_t ran = timed[i].runs - before[i];
        if (difference(ran, expected) > COUNT_SLACK) {
            snprintf(seen, sizeof seen, "%lu runs every %lu ticks", ran, timed[i].periodTicks);
            test::fail(seen, "the attached callbacks running on");
            return;
        }
    }
    if (fillerRuns[0] == fillerBefore) {
        test::fail("the last attached callback stopped", "it running");
        return;
    }
    test::pass();
}

static const Test TESTS[] = {
    {"driver-tick.periods", RunsOn::TEENSY, PERIODS_QUICK_LIMIT_S, PERIODS_SOAK_LIMIT_S,
     testPeriods, nullptr},
    {"driver-tick.refusals", RunsOn::TEENSY, REFUSALS_LIMIT_S, REFUSALS_LIMIT_S, testRefusals,
     nullptr},
};

extern const Program DRIVER_TICK = {TESTS, sizeof TESTS / sizeof TESTS[0]};
