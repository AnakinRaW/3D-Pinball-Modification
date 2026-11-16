// Tests on the Teensy whether the driver tick calls its callbacks on time.
//
//   periods   callbacks every 1, 2 and 20 ticks run for a minute, each one exactly on time
//   refusals  attach() turns down a bad period and a ninth callback, and the others run on
//
// Wiring: none, the Teensy alone.
// Run: open the serial monitor, and the program starts. Each test prints RUN and then PASS, FAIL
// or SKIP, and the last line reads END <passed> <failed> <skipped>. The tests are rows of
// docs/firmware/testing.md.

#include <Arduino.h>
#include <driver-base.h>

// testing.md, driver-tick periods: callbacks of one, two and twenty ticks for a minute
constexpr uint32_t PERIOD_TICKS[] = {1, 2, 20};
constexpr uint8_t TIMED = sizeof(PERIOD_TICKS) / sizeof(PERIOD_TICKS[0]);
constexpr uint32_t RUN_MS = 60000;
constexpr uint32_t KEEP_RUNNING_MS =
    1000;   // refusals: how long the attached callbacks are watched
// A run counts as on its tick when it lies this close to it. The interrupt's entry varies by far
// less, and a missed or doubled tick moves a run by the whole period.
constexpr uint32_t ON_TICK_NS = 20000;
constexpr uint32_t COUNT_SLACK = 1;   // runs the count may differ by at the window's two ends

// Every callback notes the processor's cycle count when it runs, so each run is timed against the
// one before.
struct Timed {
    uint32_t periodTicks;
    volatile uint32_t runs;
    volatile uint32_t lastCycles;
    volatile uint32_t offTick;   // runs that lay further than ON_TICK_NS from their tick
    volatile uint32_t worstNs;   // the furthest a run lay from its tick
};

static Timed timed[TIMED];
static volatile uint32_t fillerRuns[DriverTick::SLOTS];

static uint32_t passed = 0;
static uint32_t failed = 0;
static uint32_t skipped = 0;

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

static void run(const char* test) {
    Serial.printf("RUN driver-tick.%s\n", test);
}

static void pass(const char* test) {
    Serial.printf("PASS driver-tick.%s\n", test);
    ++passed;
}

static void fail(const char* test, const char* seen, const char* wanted) {
    Serial.printf("FAIL driver-tick.%s %s expected %s\n", test, seen, wanted);
    ++failed;
}

static uint32_t difference(uint32_t a, uint32_t b) {
    return a > b ? a - b : b - a;
}

// periods: each callback runs as often as its period gives, and every run lies one period after
// the one before.
static void testPeriods() {
    const char* test = "periods";
    run(test);
    char seen[80];
    char wanted[48];
    const uint32_t t0Us = micros();
    for (uint8_t i = 0; i < TIMED; ++i) {
        timed[i].periodTicks = PERIOD_TICKS[i];
        Timed* t = &timed[i];
        if (!driverTick.attach([t] { onTick(t); }, PERIOD_TICKS[i] * DriverTick::PERIOD_US)) {
            snprintf(seen, sizeof seen, "attach() false for %lu ticks", PERIOD_TICKS[i]);
            fail(test, seen, "true");
            return;
        }
    }
    delay(RUN_MS);
    const uint32_t elapsedUs = micros() - t0Us;
    bool ok = true;
    for (uint8_t i = 0; i < TIMED; ++i) {
        const Timed& t = timed[i];
        const uint32_t periodUs = t.periodTicks * DriverTick::PERIOD_US;
        const uint32_t expected = elapsedUs / periodUs;
        Serial.printf("every %lu ticks: %lu runs of %lu, %lu off their tick, worst %lu ns\n",
                      t.periodTicks, t.runs, expected, t.offTick, t.worstNs);
        if (difference(t.runs, expected) > COUNT_SLACK) {
            snprintf(seen, sizeof seen, "%lu runs every %lu ticks", t.runs, t.periodTicks);
            snprintf(wanted, sizeof wanted, "%lu +- %lu", expected, COUNT_SLACK);
            fail(test, seen, wanted);
            ok = false;
        } else if (t.offTick != 0) {
            snprintf(seen, sizeof seen, "%lu runs off their tick every %lu ticks", t.offTick,
                     t.periodTicks);
            fail(test, seen, "0");
            ok = false;
        }
    }
    if (ok) {
        pass(test);
    }
}

// refusals: attach() refuses an odd period and a callback past the last slot, and the attached
// callbacks keep running.
static void testRefusals() {
    const char* test = "refusals";
    run(test);
    char seen[64];
    if (driverTick.attach([] {}, DriverTick::PERIOD_US + DriverTick::PERIOD_US / 2)) {
        fail(test, "attach() true for an odd period", "false");
        return;
    }
    if (driverTick.attach([] {}, 0)) {
        fail(test, "attach() true for a period of 0", "false");
        return;
    }
    uint8_t fillers = 0;
    for (uint8_t i = TIMED; i < DriverTick::SLOTS; ++i) {
        volatile uint32_t* runs = &fillerRuns[fillers];
        if (!driverTick.attach([runs] { *runs = *runs + 1; }, DriverTick::PERIOD_US)) {
            snprintf(seen, sizeof seen, "attach() false for slot %u", i + 1);
            fail(test, seen, "true");
            return;
        }
        ++fillers;
    }
    if (driverTick.attach([] {}, DriverTick::PERIOD_US)) {
        fail(test, "attach() true past the last slot", "false");
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
            fail(test, seen, "the attached callbacks running on");
            return;
        }
    }
    if (fillerRuns[0] == fillerBefore) {
        fail(test, "the last attached callback stopped", "it running");
        return;
    }
    pass(test);
}

void setup() {
    Serial.begin(115200);
    while (!Serial) {
    }
    delay(200);
    ARM_DEMCR |= ARM_DEMCR_TRCENA;
    ARM_DWT_CTRL |= ARM_DWT_CTRL_CYCCNTENA;

    testPeriods();
    testRefusals();
    Serial.printf("END %lu %lu %lu\n", passed, failed, skipped);
}

void loop() {}
