// Tests on the computer whether the time functions keep their answers across the wrap of nowUs().
// The fake GPT2_CNT of tests/fakes stands in for GPT2, and the test sets it to each time it needs.
//
//   wrap  before(), elapsedUs() and elapsedMs() answer on both sides of the wrap as away from it
//
// Run: pio test -e computer, which runs in CI as well.

#include <clock.h>
#include <unity.h>

constexpr uint32_t AWAY_US = 1000000;   // a time far from the wrap
constexpr uint32_t US_PER_MS = 1000;
// spans from none to the longest before() can order, half the counter's range
constexpr uint32_t SPANS_US[] = {0, 1, 999, 1000, 1001, 65536, 1u << 20, 0x7FFFFFFE};

void setUp() {}

void tearDown() {}

// the time t that puts t and t + distance on both sides of the wrap, or at its edge
static uint32_t acrossWrap(uint32_t distance) {
    return 0 - distance / 2 - 1;
}

// the answers of the three functions for a span, with now lying distance after t
struct Answers {
    bool elapsedUs;
    bool elapsedMs;
    bool earlier;
    bool later;
};

static Answers answers(uint32_t t, uint32_t distance, uint32_t span) {
    GPT2_CNT = t + distance;
    return {elapsedUs(t, span), elapsedMs(t, span / US_PER_MS), before(t, t + distance),
            before(t + distance, t)};
}

// wrap: every answer equals the one the same span gives away from the wrap.
static void testWrap() {
    for (const uint32_t span : SPANS_US) {
        const uint32_t distances[] = {span == 0 ? 0 : span - 1, span, span + 1};
        for (const uint32_t distance : distances) {
            const Answers away = answers(AWAY_US, distance, span);
            const Answers wrap = answers(acrossWrap(distance), distance, span);
            TEST_ASSERT_EQUAL(distance >= span, away.elapsedUs);
            TEST_ASSERT_EQUAL(distance >= span / US_PER_MS * US_PER_MS, away.elapsedMs);
            TEST_ASSERT_EQUAL(distance > 0, away.earlier);
            TEST_ASSERT_FALSE(away.later);
            TEST_ASSERT_EQUAL(away.elapsedUs, wrap.elapsedUs);
            TEST_ASSERT_EQUAL(away.elapsedMs, wrap.elapsedMs);
            TEST_ASSERT_EQUAL(away.earlier, wrap.earlier);
            TEST_ASSERT_EQUAL(away.later, wrap.later);
        }
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(testWrap);
    return UNITY_END();
}
