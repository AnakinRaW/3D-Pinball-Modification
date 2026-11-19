// Tests on the computer whether the driver tick calls the right callbacks on the right ticks. A
// fake IntervalTimer stands in for the Teensy's, and the tests step it one tick at a time.
//
//   due          callbacks every 1, 2 and 20 ticks run on exactly their ticks
//   refused      attach() turns down a bad period and a ninth callback
//   timer-start  the first attach() starts the timer, the second leaves it alone
//   no-timer     with no IntervalTimer free, attach() fails and keeps nothing
//
// Run: pio test -e computer, which runs in CI as well.

#include "drivers/driver-base.h"
#include <unity.h>
#include <vector>

constexpr uint8_t TICK_PRIORITY = 96;    // general-design.md: the driver tick's row
constexpr uint32_t STEPPED_TICKS = 40;   // due: a common multiple of the three periods, twice
constexpr uint32_t PERIOD_TICKS[] = {1, 2, 20};
constexpr uint8_t PERIODS = sizeof(PERIOD_TICKS) / sizeof(PERIOD_TICKS[0]);

void setUp() {
    fakeTimers = FakeTimers{};
}

void tearDown() {}

// Steps the running fake timer by ticks.
static void step(uint32_t ticks) {
    for (uint32_t i = 0; i < ticks; ++i) {
        fakeTimers.running->fire();
    }
}

// due: each callback runs on exactly the ticks its period gives.
static void testDue() {
    DriverTick tick;
    uint32_t now = 0;
    std::vector<uint32_t> ran[PERIODS];
    for (uint8_t i = 0; i < PERIODS; ++i) {
        std::vector<uint32_t>* log = &ran[i];
        uint32_t* clock = &now;
        TEST_ASSERT_TRUE(tick.attach([log, clock] { log->push_back(*clock); },
                                     PERIOD_TICKS[i] * DriverTick::PERIOD_US));
    }
    for (uint32_t t = 1; t <= STEPPED_TICKS; ++t) {
        now = t;
        step(1);
    }
    for (uint8_t i = 0; i < PERIODS; ++i) {
        TEST_ASSERT_EQUAL_UINT32(STEPPED_TICKS / PERIOD_TICKS[i], ran[i].size());
        for (uint32_t k = 0; k < ran[i].size(); ++k) {
            TEST_ASSERT_EQUAL_UINT32((k + 1) * PERIOD_TICKS[i], ran[i][k]);
        }
    }
}

// refused: attach() refuses a period of no ticks, one off the tick and a callback past the last
// slot, and the callbacks attached before keep running.
static void testRefused() {
    DriverTick tick;
    uint32_t runs = 0;
    TEST_ASSERT_FALSE(tick.attach([] {}, 0));
    TEST_ASSERT_FALSE(tick.attach([] {}, DriverTick::PERIOD_US / 2));
    TEST_ASSERT_FALSE(tick.attach([] {}, DriverTick::PERIOD_US + DriverTick::PERIOD_US / 2));
    for (uint8_t i = 0; i < DriverTick::SLOTS; ++i) {
        uint32_t* counter = &runs;
        TEST_ASSERT_TRUE(
            tick.attach([counter] { *counter = *counter + 1; }, DriverTick::PERIOD_US));
    }
    TEST_ASSERT_FALSE(tick.attach([] {}, DriverTick::PERIOD_US));
    step(1);
    TEST_ASSERT_EQUAL_UINT32(DriverTick::SLOTS, runs);
}

// timer-start: the first attach() starts the timer with the tick's period and priority, and a
// later one leaves it running as it is.
static void testTimerStart() {
    DriverTick tick;
    TEST_ASSERT_EQUAL_UINT32(0, fakeTimers.begins);
    TEST_ASSERT_TRUE(tick.attach([] {}, DriverTick::PERIOD_US));
    TEST_ASSERT_EQUAL_UINT32(1, fakeTimers.begins);
    TEST_ASSERT_EQUAL_UINT32(DriverTick::PERIOD_US, fakeTimers.periodUs);
    TEST_ASSERT_EQUAL_UINT8(TICK_PRIORITY, fakeTimers.priority);
    TEST_ASSERT_TRUE(tick.attach([] {}, DriverTick::PERIOD_US));
    TEST_ASSERT_EQUAL_UINT32(1, fakeTimers.begins);
}

// no-timer: when no IntervalTimer is free, attach() returns false and keeps no callback, and the
// next attach() tries the timer again.
static void testNoTimer() {
    DriverTick tick;
    uint32_t runs = 0;
    uint32_t* counter = &runs;
    fakeTimers.refuse = true;
    TEST_ASSERT_FALSE(tick.attach([counter] { *counter = *counter + 1; }, DriverTick::PERIOD_US));
    TEST_ASSERT_NULL(fakeTimers.running);
    fakeTimers.refuse = false;
    TEST_ASSERT_TRUE(tick.attach([] {}, DriverTick::PERIOD_US));
    TEST_ASSERT_EQUAL_UINT32(2, fakeTimers.begins);
    step(1);
    TEST_ASSERT_EQUAL_UINT32(0, runs);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(testDue);
    RUN_TEST(testRefused);
    RUN_TEST(testTimerStart);
    RUN_TEST(testNoTimer);
    return UNITY_END();
}
