// Tests on the computer whether the debug trace hands its notes back whole and in order, refuses a
// note when full and forms its lines. The fake GPT2_CNT of tests/fakes sets each note's time.
//
//   order   notes come out in the order they were noted, each with its label, numbers and time
//   full    a note past the trace's size is refused and counted. Room comes back once the
//           reader takes a note
//   lost    once the waiting notes are taken, a loss notice states the notes the full trace
//           refused
//   format  a note becomes a line with the time, D trace, the label, both numbers and the note's
//           time in microseconds
//
// Run: pio test -e computer, which runs in CI as well.

#include "logging/debug-trace.h"
#include "logging/log-line.h"
#include <Arduino.h>
#include <string>
#include <unity.h>

constexpr size_t RECORDS = 4;

void setUp() {
    GPT2_CNT = 1000;
}

void tearDown() {}

// order: first noted, first taken, every field intact.
static void testOrder() {
    TraceRecord records[RECORDS];
    DebugTrace trace(records, RECORDS);
    TEST_ASSERT_TRUE(trace.note("first", 1, -1));
    GPT2_CNT = 2000;
    TEST_ASSERT_TRUE(trace.note("second", 2, -2));
    TraceRecord out;
    TEST_ASSERT_TRUE(trace.take(out));
    TEST_ASSERT_EQUAL_STRING("first", out.label);
    TEST_ASSERT_EQUAL_INT32(1, out.a);
    TEST_ASSERT_EQUAL_INT32(-1, out.b);
    TEST_ASSERT_EQUAL_UINT32(1000, out.timeUs);
    TEST_ASSERT_TRUE(trace.take(out));
    TEST_ASSERT_EQUAL_STRING("second", out.label);
    TEST_ASSERT_EQUAL_UINT32(2000, out.timeUs);
    TEST_ASSERT_FALSE(trace.take(out));
    // The places wrap around the records. The order holds.
    for (int32_t i = 0; i < 10; ++i) {
        TEST_ASSERT_TRUE(trace.note("lap", i, 0));
        TEST_ASSERT_TRUE(trace.take(out));
        TEST_ASSERT_EQUAL_INT32(i, out.a);
    }
}

// full: the note past the size is refused. A take makes room again.
static void testFull() {
    TraceRecord records[RECORDS];
    DebugTrace trace(records, RECORDS);
    for (int32_t i = 0; i < int32_t(RECORDS); ++i) {
        TEST_ASSERT_TRUE(trace.note("n", i, 0));
    }
    TEST_ASSERT_FALSE(trace.note("n", 99, 0));
    TEST_ASSERT_EQUAL_UINT32(1, trace.lost());
    TraceRecord out;
    TEST_ASSERT_TRUE(trace.take(out));
    TEST_ASSERT_EQUAL_INT32(0, out.a);
    TEST_ASSERT_TRUE(trace.note("n", 4, 0));
    for (int32_t i = 1; i <= 4; ++i) {
        TEST_ASSERT_TRUE(trace.take(out));
        TEST_ASSERT_EQUAL_INT32(i, out.a);
    }
    // A trace without records refuses every note and states the loss.
    DebugTrace none(nullptr, RECORDS);
    TEST_ASSERT_FALSE(none.note("n", 0, 0));
    TEST_ASSERT_TRUE(none.take(out));
    TEST_ASSERT_NULL(out.label);
    TEST_ASSERT_EQUAL_INT32(1, out.a);
    TEST_ASSERT_FALSE(none.take(out));
}

// lost: the loss notice follows the notes that were kept.
static void testLost() {
    TraceRecord records[RECORDS];
    DebugTrace trace(records, RECORDS);
    for (int32_t i = 0; i < int32_t(RECORDS) + 2; ++i) {
        trace.note("n", i, 0);
    }
    TraceRecord out;
    for (int32_t i = 0; i < int32_t(RECORDS); ++i) {
        TEST_ASSERT_TRUE(trace.take(out));
        TEST_ASSERT_EQUAL_INT32(i, out.a);
    }
    TEST_ASSERT_TRUE(trace.take(out));
    TEST_ASSERT_NULL(out.label);
    TEST_ASSERT_EQUAL_INT32(2, out.a);
    char line[LOG_LINE_BYTES];
    const size_t n = formatTraceLine(line, 73021, out);
    TEST_ASSERT_EQUAL_STRING("73021 D trace 2 notes lost\n", std::string(line, n).c_str());
    TEST_ASSERT_FALSE(trace.take(out));
}

// format: the line carries both times, the label and the numbers.
static void testFormat() {
    TraceRecord records[RECORDS];
    DebugTrace trace(records, RECORDS);
    GPT2_CNT = 73020512;
    TEST_ASSERT_TRUE(trace.note("ir late", 3, -12));
    TraceRecord out;
    TEST_ASSERT_TRUE(trace.take(out));
    char line[LOG_LINE_BYTES];
    const size_t n = formatTraceLine(line, 73021, out);
    TEST_ASSERT_EQUAL_STRING("73021 D trace ir late 3 -12 at 73020512 us\n",
                             std::string(line, n).c_str());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(testOrder);
    RUN_TEST(testFull);
    RUN_TEST(testLost);
    RUN_TEST(testFormat);
    return UNITY_END();
}
