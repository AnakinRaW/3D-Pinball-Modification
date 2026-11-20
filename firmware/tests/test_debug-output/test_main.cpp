// Tests on the computer whether the debug output forms its lines, keeps them whole and sends them
// without waiting. The fake Serial of tests/fakes takes no more bytes than the test gives it room
// for. The fake millis() gives the time. The fake task lock lets the test pose as an interrupt.
//
//   format     each line starts with the milliseconds since the start and D. It ends in one
//              line break, whether the text brings one, none or two
//   cut        a line longer than the limit is cut to it and ends in "..."
//   full       a line that finds the buffer full takes the place of the oldest lines. The lines
//              between stay intact. A notice behind them states how many were dropped
//   half-sent  a line that needs the room of a line send() has begun drops that line's rest
//   interrupt  a line written in an interrupt is dropped, and a notice states it. The lock is
//              not taken
//   room       send() hands USB serial no more than it has room for. The next call goes on
//              where it stopped
//
// Run: pio test -e computer, which runs in CI as well.

#include "logging/debug-output.h"
#include "logging/log-line.h"
#include "core/task-lock.h"
#include <Arduino.h>
#include <string>
#include <unity.h>

constexpr size_t BUFFER_BYTES = 256;
constexpr uint32_t AT_MS = 73021;    // the time the lines are written at
constexpr int UNLIMITED = 1 << 20;   // room for every byte

void setUp() {
    fakeMillis = AT_MS;
    Serial.sent.clear();
    Serial.room = UNLIMITED;
    fakeInInterrupt = false;
}

void tearDown() {}

// Writes a line through the variadic form, as debugLog() does.
static bool writeTo(DebugOutput& out, const char* format, ...)
    __attribute__((format(printf, 2, 3)));

static bool writeTo(DebugOutput& out, const char* format, ...) {
    va_list args;
    va_start(args, format);
    const bool added = out.write(format, args);
    va_end(args);
    return added;
}

// format: the time stands in front. Every line ends in exactly one line break.
static void testFormat() {
    uint8_t buffer[BUFFER_BYTES];
    DebugOutput out(buffer, sizeof(buffer));
    TEST_ASSERT_TRUE(writeTo(out, "servo %d\n", 2));
    TEST_ASSERT_TRUE(writeTo(out, "no break"));
    TEST_ASSERT_TRUE(writeTo(out, "two breaks\n\n"));
    out.send();
    TEST_ASSERT_EQUAL_STRING("73021 D servo 2\n73021 D no break\n73021 D two breaks\n",
                             Serial.sent.c_str());
}

// cut: a long line is cut to LOG_LINE_BYTES and marked.
static void testCut() {
    uint8_t buffer[BUFFER_BYTES];
    DebugOutput out(buffer, sizeof(buffer));
    const std::string longText(2 * LOG_LINE_BYTES, 'x');
    TEST_ASSERT_TRUE(writeTo(out, "%s", longText.c_str()));
    out.send();
    TEST_ASSERT_EQUAL_size_t(LOG_LINE_BYTES, Serial.sent.size());
    TEST_ASSERT_EQUAL_STRING("73021 D x", Serial.sent.substr(0, 9).c_str());
    TEST_ASSERT_EQUAL_STRING("...\n", Serial.sent.substr(LOG_LINE_BYTES - 4).c_str());
}

// full: a new line pushes out the oldest. The lines between stay.
static void testFull() {
    uint8_t buffer[BUFFER_BYTES];
    DebugOutput out(buffer, sizeof(buffer));
    // Each line is 16 bytes: "73021 D line NN\n", so sixteen fill the buffer.
    for (int i = 0; i < 16; ++i) {
        TEST_ASSERT_TRUE(writeTo(out, "line %02d", i));
    }
    TEST_ASSERT_TRUE(writeTo(out, "line %02d", 16));
    // A line of 32 bytes takes the place of two.
    TEST_ASSERT_TRUE(writeTo(out, "a line twice as long %02d", 1));
    TEST_ASSERT_EQUAL_UINT32(3, out.lost());
    out.send();
    // The notice of the three dropped lines follows the lines, once the buffer has room.
    const std::string notice = "73021 D debug 3 lines lost\n";
    TEST_ASSERT_EQUAL_size_t(BUFFER_BYTES + notice.size(), Serial.sent.size());
    TEST_ASSERT_EQUAL_STRING(notice.c_str(), Serial.sent.substr(BUFFER_BYTES).c_str());
    TEST_ASSERT_EQUAL_STRING("73021 D line 03\n", Serial.sent.substr(0, 16).c_str());
    TEST_ASSERT_EQUAL_STRING("73021 D line 16\n73021 D a line twice as long 01\n",
                             Serial.sent.substr(208, 48).c_str());
}

// half-sent: the rest of a line USB has begun goes. The next line comes whole.
static void testHalfSent() {
    uint8_t buffer[BUFFER_BYTES];
    DebugOutput out(buffer, sizeof(buffer));
    for (int i = 0; i < 16; ++i) {
        TEST_ASSERT_TRUE(writeTo(out, "line %02d", i));
    }
    Serial.room = 6;
    TEST_ASSERT_EQUAL_size_t(6, out.send());
    // The 6 bytes sent and the rest of line 0 make room for exactly one line.
    TEST_ASSERT_TRUE(writeTo(out, "line %02d", 16));
    TEST_ASSERT_EQUAL_UINT32(1, out.lost());
    Serial.room = UNLIMITED;
    out.send();
    TEST_ASSERT_EQUAL_STRING("73021 73021 D line 01\n", Serial.sent.substr(0, 22).c_str());
    const std::string notice = "73021 D debug 1 lines lost\n";
    TEST_ASSERT_EQUAL_STRING(("73021 D line 16\n" + notice).c_str(),
                             Serial.sent.substr(Serial.sent.size() - 16 - notice.size()).c_str());
}

// interrupt: an interrupt's line is dropped before the lock.
static void testInterrupt() {
    uint8_t buffer[BUFFER_BYTES];
    DebugOutput out(buffer, sizeof(buffer));
    fakeInInterrupt = true;
    const uint32_t locks = fakeLocksTaken;
    TEST_ASSERT_FALSE(writeTo(out, "from an interrupt"));
    TEST_ASSERT_FALSE(out.writeLine("x\n", 2));
    TEST_ASSERT_EQUAL_UINT32(locks, fakeLocksTaken);
    TEST_ASSERT_EQUAL_UINT32(2, out.lost());
    fakeInInterrupt = false;
    out.send();
    TEST_ASSERT_EQUAL_STRING("73021 D debug 2 lines lost\n", Serial.sent.c_str());
}

// room: send() takes the room USB serial has. The next call goes on from there.
static void testRoom() {
    uint8_t buffer[BUFFER_BYTES];
    DebugOutput out(buffer, sizeof(buffer));
    TEST_ASSERT_TRUE(writeTo(out, "first"));
    TEST_ASSERT_TRUE(writeTo(out, "second"));
    Serial.room = 0;
    TEST_ASSERT_EQUAL_size_t(0, out.send());
    Serial.room = 10;
    TEST_ASSERT_EQUAL_size_t(10, out.send());
    TEST_ASSERT_EQUAL_STRING("73021 D fi", Serial.sent.c_str());
    Serial.room = UNLIMITED;
    out.send();
    TEST_ASSERT_EQUAL_STRING("73021 D first\n73021 D second\n", Serial.sent.c_str());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(testFormat);
    RUN_TEST(testCut);
    RUN_TEST(testFull);
    RUN_TEST(testHalfSent);
    RUN_TEST(testInterrupt);
    RUN_TEST(testRoom);
    return UNITY_END();
}
