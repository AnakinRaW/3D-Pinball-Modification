// Tests on the computer whether the break beam driver turns the edges of the receiver's output into
// one BALL_DRAINED per crossing and reports a beam that stays broken. The fake pin keeps what the
// driver set up, and a test sets its level and calls the function the driver attached to it, as
// the pin interrupt would.
//
//   at-start       begin() sets the pin up, and a beam broken at the start fails 5 s later
//   crossing       a break after a clear beam gives one BALL_DRAINED with the time of the break
//   flicker        a beam clear for less than 10 ms between two breaks gives one event
//   stays-broken   a beam broken for 5 s fails, and works again once clear
//   wrap           the flicker window and the fault time hold across the wrap of nowUs()
//   second-driver  a second driver's begin() fails, and it answers with empty data
//
// One driver holds the pin's interrupt for the whole program, and at-start begins it, so at-start
// runs first.
//
// Run: pio test -e computer, which runs in CI as well.

#include "drivers/break-beam.h"
#include <unity.h>

constexpr uint8_t BEAM_PIN = 31;      // pin-assignment.md: Receiver output of the ball drain gate
constexpr uint32_t T0_US = 5000000;   // begin(), some time after the start
constexpr uint32_t US_PER_MS = 1000;
constexpr uint32_t FAIL_US = BreakBeamDriver::FAIL_MS * US_PER_MS;
constexpr uint32_t DEAD_US = BreakBeamDriver::DEAD_US;
constexpr uint32_t LONG_US = 1000000;   // a beam clear this long sees the next break as a crossing
constexpr uint32_t BALL_US = 3000;      // break-beam design: the fastest ball blocks the beam 3 ms
constexpr uint32_t EVENTS = 8;

static EventQueue queue;
static BreakBeamDriver beam(queue);
static DriverEvent out[EVENTS];

// Sets the pin's level at timeUs and calls the interrupt the driver attached.
static void edge(uint32_t timeUs, uint8_t level) {
    GPT2_CNT = timeUs;
    fakePins[BEAM_PIN].level = level;
    TEST_ASSERT_NOT_NULL(fakePins[BEAM_PIN].isr);
    fakePins[BEAM_PIN].isr();
}

// Gets the events published since the last call.
static uint32_t taken() {
    return queue.read(out, EVENTS);
}

// Clears the beam at timeUs and lets it stay clear long enough, so the next break is a crossing.
// Returns the time it stays clear until.
static uint32_t settle(uint32_t timeUs) {
    edge(timeUs, LOW);
    taken();
    return timeUs + LONG_US;
}

static void testAtStart() {
    GPT2_CNT = T0_US;
    fakePins[BEAM_PIN].level = HIGH;
    TEST_ASSERT_TRUE(beam.begin());
    TEST_ASSERT_EQUAL_INT(INPUT, fakePins[BEAM_PIN].mode);
    TEST_ASSERT_TRUE((fakePins[BEAM_PIN].pad & IOMUXC_PAD_HYS) != 0);
    TEST_ASSERT_EQUAL_INT(CHANGE, fakePins[BEAM_PIN].interruptMode);
    // A second call answers as the first.
    TEST_ASSERT_TRUE(beam.begin());
    TEST_ASSERT_TRUE(beam.blocked());
    TEST_ASSERT_EQUAL_UINT32(0, taken());
    // The fault counts from begin(), not from the start.
    GPT2_CNT = T0_US + FAIL_US - 1;
    TEST_ASSERT_EQUAL_UINT16(0, beam.failed().parts);
    GPT2_CNT = T0_US + FAIL_US;
    TEST_ASSERT_EQUAL_UINT16(1, beam.failed().parts);
    TEST_ASSERT_EQUAL_UINT32(0, taken());
}

static void testCrossing() {
    const uint32_t t = settle(T0_US + FAIL_US);
    TEST_ASSERT_FALSE(beam.blocked());
    edge(t, HIGH);
    TEST_ASSERT_TRUE(beam.blocked());
    TEST_ASSERT_EQUAL_UINT32(1, taken());
    TEST_ASSERT_EQUAL_UINT16(uint16_t(DriverEventType::BALL_DRAINED), uint16_t(out[0].type));
    TEST_ASSERT_EQUAL_UINT8(BreakBeamDriver::GATE, out[0].source);
    TEST_ASSERT_EQUAL_UINT32(t, out[0].time);
    TEST_ASSERT_EQUAL_HEX32(0, out[0].payload);
    // The ball leaves the beam, which publishes nothing.
    edge(t + BALL_US, LOW);
    TEST_ASSERT_FALSE(beam.blocked());
    TEST_ASSERT_EQUAL_UINT32(0, taken());
}

static void testFlicker() {
    const uint32_t t = settle(T0_US + 2 * FAIL_US);
    edge(t, HIGH);
    TEST_ASSERT_EQUAL_UINT32(1, taken());
    // The beam's edge flickers as the ball leaves: clear for less than DEAD_US, then broken.
    edge(t + BALL_US, LOW);
    edge(t + BALL_US + DEAD_US - 1, HIGH);
    TEST_ASSERT_EQUAL_UINT32(0, taken());
    // Clear for exactly DEAD_US, and the next break is a crossing.
    const uint32_t cleared = t + BALL_US + DEAD_US;
    edge(cleared, LOW);
    edge(cleared + DEAD_US, HIGH);
    TEST_ASSERT_EQUAL_UINT32(1, taken());
    TEST_ASSERT_EQUAL_UINT32(cleared + DEAD_US, out[0].time);
    // Two edges closer together than the interrupt's response give one call, which reads the
    // level that holds. A call that reads HIGH twice gives no second event.
    edge(cleared + DEAD_US + 1, HIGH);
    TEST_ASSERT_EQUAL_UINT32(0, taken());
    edge(cleared + DEAD_US + BALL_US, LOW);
}

static void testStaysBroken() {
    const uint32_t t = settle(T0_US + 3 * FAIL_US);
    TEST_ASSERT_EQUAL_UINT16(0, beam.failed().parts);
    edge(t, HIGH);
    taken();
    GPT2_CNT = t + FAIL_US - 1;
    TEST_ASSERT_EQUAL_UINT16(0, beam.failed().parts);
    // A flicker while the ball sits in the beam starts the count afresh.
    edge(t + FAIL_US - 1, LOW);
    edge(t + FAIL_US, HIGH);
    TEST_ASSERT_EQUAL_UINT32(0, taken());
    GPT2_CNT = t + 2 * FAIL_US - 1;
    TEST_ASSERT_EQUAL_UINT16(0, beam.failed().parts);
    GPT2_CNT = t + 2 * FAIL_US;
    const Fault f = beam.failed();
    TEST_ASSERT_EQUAL_UINT16(1, f.parts);
    TEST_ASSERT_EQUAL_UINT32(0, f.code);
    // The ball is taken out, and the beam works again.
    edge(t + 2 * FAIL_US + 1, LOW);
    TEST_ASSERT_EQUAL_UINT16(0, beam.failed().parts);
    TEST_ASSERT_EQUAL_UINT32(0, taken());
}

static void testWrap() {
    // The clear begins before the wrap of nowUs(), and the breaks come after it.
    const uint32_t beforeWrap = UINT32_MAX - DEAD_US / 2;
    settle(beforeWrap - LONG_US);
    edge(beforeWrap - BALL_US, HIGH);
    TEST_ASSERT_EQUAL_UINT32(1, taken());
    edge(beforeWrap, LOW);
    edge(beforeWrap + DEAD_US - 1, HIGH);
    TEST_ASSERT_EQUAL_UINT32(0, taken());
    edge(beforeWrap + DEAD_US, LOW);
    edge(beforeWrap + 2 * DEAD_US, HIGH);
    TEST_ASSERT_EQUAL_UINT32(1, taken());
    // The beam stays broken across the wrap.
    const uint32_t broken = beforeWrap + 2 * DEAD_US;
    GPT2_CNT = broken + FAIL_US - 1;
    TEST_ASSERT_EQUAL_UINT16(0, beam.failed().parts);
    GPT2_CNT = broken + FAIL_US;
    TEST_ASSERT_EQUAL_UINT16(1, beam.failed().parts);
    edge(broken + FAIL_US, LOW);
}

static void testSecondDriver() {
    EventQueue otherQueue;
    BreakBeamDriver second(otherQueue);
    // Before begin() and after the failed begin(), the driver answers with empty data.
    fakePins[BEAM_PIN].level = HIGH;
    GPT2_CNT = T0_US;
    TEST_ASSERT_FALSE(second.blocked());
    TEST_ASSERT_EQUAL_UINT16(0, second.failed().parts);
    TEST_ASSERT_FALSE(second.begin());
    TEST_ASSERT_FALSE(second.begin());
    GPT2_CNT = UINT32_MAX / 2;
    TEST_ASSERT_FALSE(second.blocked());
    TEST_ASSERT_EQUAL_UINT16(0, second.failed().parts);
    // The first driver keeps the interrupt and its events.
    const uint32_t t = settle(UINT32_MAX / 2);
    edge(t, HIGH);
    TEST_ASSERT_EQUAL_UINT32(1, taken());
    DriverEvent none[1];
    TEST_ASSERT_EQUAL_UINT32(0, otherQueue.read(none, 1));
    TEST_ASSERT_TRUE(beam.blocked());
    edge(t + BALL_US, LOW);
}

void setUp() {}
void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(testAtStart);
    RUN_TEST(testCrossing);
    RUN_TEST(testFlicker);
    RUN_TEST(testStaysBroken);
    RUN_TEST(testWrap);
    RUN_TEST(testSecondDriver);
    return UNITY_END();
}
