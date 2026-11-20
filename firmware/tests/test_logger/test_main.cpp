// Tests on the computer whether the logger forms its lines, writes them to the card in one write a
// second, keeps them through a failed write, states its lost lines and takes its two files in
// turns. The files are fakes whose writes end at once. The fakes of tests/fakes set the time:
// GPT2_CNT for the appender's second, fakeMillis for the time in front of a line.
//
//   format        each line starts with the milliseconds since the start, then the level's letter
//                 and the writer's name. The banner follows the head line
//   buffer-full   a line that does not fit is lost whole and counted. The others stay intact. A
//                 line states the count once one fits again
//   one-write     lines written at a high rate reach the card in one write a second
//   failed-write  the lines of a failed write reach the card with the next write, at its place
//   head          begin() goes on in the file whose head line carries the higher number, also
//                 when both files are full. Without any head line it goes on in the first file
//   turn          a line that would take the file past its limit starts the other file anew,
//                 with a head line one number higher
//   no-card       without a card the main task calls no begin(). Every line is lost and counted
//   interrupt     a line from an interrupt is lost and counted
//   free-lines    logInfo() and logWarning() write into the logger whose begin() ran last
//
// Run: pio test -e computer, which runs in CI as well.

#include "logging/logger.h"
#include "core/clock.h"
#include "core/task-lock.h"
#include "core/version.h"
#include <Arduino.h>
#include <string>
#include <unity.h>

constexpr size_t BUFFER_BYTES = 1024;
constexpr uint32_t US_PER_MS = 1000;

// A file whose writes end at once, or fail while failing is set.
struct FakeFile : AppendFile {
    std::string content;
    bool failing = false;
    AppendStatus state = AppendStatus::IDLE;
    uint32_t writes = 0;

    void write(uint32_t at, const uint8_t* data, size_t n) override {
        writes = writes + 1;
        if (failing) {
            state = AppendStatus::FAILED;
            return;
        }
        content.resize(at);
        content.append(reinterpret_cast<const char*>(data), n);
        state = AppendStatus::SUCCEEDED;
    }

    void rewrite(const uint8_t* data, size_t n) override {
        content.clear();
        write(0, data, n);
    }

    AppendStatus status() const override { return state; }
};

static uint8_t buffer[BUFFER_BYTES];

void setUp() {
    fakeInInterrupt = false;
}

void tearDown() {}

static void advanceMs(uint32_t ms) {
    GPT2_CNT = GPT2_CNT + ms * US_PER_MS;
    fakeMillis = fakeMillis + ms;
}

// Gets the time the next line carries, as text.
static std::string nowText() {
    return std::to_string(millis());
}

// Gets what the start of a file holds, as the main task reads it for begin().
static LogFileStart startOf(const FakeFile& f) {
    LogFileStart start;
    start.bytes = reinterpret_cast<const uint8_t*>(f.content.data());
    start.got = f.content.size();
    start.sizeBytes = static_cast<uint32_t>(f.content.size());
    return start;
}

// Gets the banner begin() writes at time t, as text.
static std::string banner(const std::string& t) {
    const std::string rule = t + " I logger ====================================\n";
    return rule + t + " I logger AnakinRaW's PINBALL Modification\n" + t + " I logger firmware " +
           FIRMWARE_VERSION + " release build\n" + rule;
}

// Lets a second pass, so the next update() writes. Then writes.
static void writeNow(Logger& logger) {
    advanceMs(CardAppender::WRITE_PERIOD_MS);
    logger.update();
    logger.update();
}

// format: time, level and writer in front of every line, the banner behind the head line.
static void testFormat() {
    FakeFile a;
    FakeFile b;
    Logger logger(a, b, buffer, sizeof(buffer));
    fakeMillis = 73021;
    logger.begin(startOf(a), startOf(b));
    TEST_ASSERT_TRUE(logger.write(LogLevel::INFO, "host", "game %d", 1));
    fakeMillis = 73023;
    TEST_ASSERT_TRUE(logger.write(LogLevel::WARNING, "lamps", "no lamp %u", 12u));
    writeNow(logger);
    TEST_ASSERT_EQUAL_STRING(("73021 I logger file 1\n" + banner("73021") +
                              "73021 I host game 1\n73023 W lamps no lamp 12\n")
                                 .c_str(),
                             a.content.c_str());
}

// buffer-full: lost lines are whole, counted and stated once a line fits again.
static void testBufferFull() {
    FakeFile a;
    FakeFile b;
    a.content = "2000 I logger file 3\n";
    Logger logger(a, b, buffer, sizeof(buffer));
    const std::string t0 = nowText();
    logger.begin(startOf(a), startOf(b));
    // Each line is 64 bytes: the time, " I x " and the text with its line break.
    const std::string text(64 - t0.size() - 6, 't');
    uint32_t written = 0;
    while (logger.write(LogLevel::INFO, "x", "%s", text.c_str())) {
        written = written + 1;
    }
    TEST_ASSERT_EQUAL_UINT32((BUFFER_BYTES - banner(t0).size()) / 64, written);
    // The write that ended the loop is the one lost line.
    TEST_ASSERT_EQUAL_UINT32(1, logger.lost());
    writeNow(logger);
    const std::string t1 = nowText();
    TEST_ASSERT_TRUE(logger.write(LogLevel::INFO, "x", "after"));
    writeNow(logger);
    const std::string line = t0 + " I x " + text + "\n";
    std::string expected = "2000 I logger file 3\n" + banner(t0);
    for (uint32_t i = 0; i < written; ++i) {
        expected += line;
    }
    expected += t1 + " W logger 1 lines lost\n" + t1 + " I x after\n";
    TEST_ASSERT_EQUAL_STRING(expected.c_str(), a.content.c_str());
}

// one-write: a second of lines goes to the card in one write.
static void testOneWrite() {
    FakeFile a;
    FakeFile b;
    Logger logger(a, b, buffer, sizeof(buffer));
    logger.begin(startOf(a), startOf(b));
    for (uint32_t second = 0; second < 5; ++second) {
        for (uint32_t ms = 0; ms < CardAppender::WRITE_PERIOD_MS; ms += 100) {
            TEST_ASSERT_TRUE(logger.write(LogLevel::INFO, "t", "%u", static_cast<unsigned>(ms)));
            advanceMs(100);
            logger.update();
        }
    }
    TEST_ASSERT_EQUAL_UINT32(5, a.writes);
    TEST_ASSERT_EQUAL_UINT32(0, logger.lost());
}

// failed-write: the lines of a failed write go to the card with the next, at the same place.
static void testFailedWrite() {
    FakeFile a;
    FakeFile b;
    a.content = "1 I logger file 1\n";
    Logger logger(a, b, buffer, sizeof(buffer));
    const std::string t0 = nowText();
    logger.begin(startOf(a), startOf(b));
    TEST_ASSERT_TRUE(logger.write(LogLevel::INFO, "t", "kept"));
    a.failing = true;
    writeNow(logger);
    TEST_ASSERT_EQUAL_STRING("1 I logger file 1\n", a.content.c_str());
    a.failing = false;
    const std::string t1 = nowText();
    TEST_ASSERT_TRUE(logger.write(LogLevel::INFO, "t", "next"));
    writeNow(logger);
    TEST_ASSERT_EQUAL_STRING(
        ("1 I logger file 1\n" + banner(t0) + t0 + " I t kept\n" + t1 + " I t next\n").c_str(),
        a.content.c_str());
    TEST_ASSERT_EQUAL_UINT32(0, logger.lost());
}

// head: the file with the higher number in its head line is the current one.
static void testHead() {
    // 2 bytes short of the limit with the head line in front
    const std::string full(Logger::FILE_LIMIT_BYTES - 20, 'f');
    {
        // Both full, as a power cut between turn and first write leaves them.
        FakeFile a;
        FakeFile b;
        a.content = "9 I logger file 6\n" + full;
        b.content = "9 I logger file 7\n" + full;
        Logger logger(a, b, buffer, sizeof(buffer));
        const std::string t = nowText();
        logger.begin(startOf(a), startOf(b));
        TEST_ASSERT_TRUE(logger.write(LogLevel::INFO, "t", "x"));
        writeNow(logger);
        // The banner does not fit into file 7, so the log turns to the first file as file 8.
        TEST_ASSERT_EQUAL_STRING((t + " I logger file 8\n" + banner(t) + t + " I t x\n").c_str(),
                                 a.content.c_str());
        TEST_ASSERT_EQUAL_size_t(18 + full.size(), b.content.size());
    }
    {
        FakeFile a;
        FakeFile b;
        a.content = "written before head lines\n";
        Logger logger(a, b, buffer, sizeof(buffer));
        const std::string t = nowText();
        logger.begin(startOf(a), startOf(b));
        TEST_ASSERT_TRUE(logger.write(LogLevel::INFO, "t", "x"));
        writeNow(logger);
        TEST_ASSERT_EQUAL_STRING(
            ("written before head lines\n" + banner(t) + t + " I t x\n").c_str(),
            a.content.c_str());
        TEST_ASSERT_EQUAL_size_t(0, b.content.size());
    }
}

// turn: past the limit the log starts the other file anew, numbered one higher.
static void testTurn() {
    FakeFile a;
    FakeFile b;
    const std::string t = nowText();
    // 20 bytes short of the limit with the head line and the banner behind it
    const std::string full(Logger::FILE_LIMIT_BYTES - 38 - banner(t).size(), 'f');
    a.content = "1 I logger file 4\n" + full;
    b.content = "1 I logger file 3\nthe file before\n";
    Logger logger(a, b, buffer, sizeof(buffer));
    logger.begin(startOf(a), startOf(b));
    TEST_ASSERT_TRUE(logger.write(LogLevel::INFO, "t", "fits"));
    TEST_ASSERT_TRUE(logger.write(LogLevel::INFO, "t", "goes to the other file"));
    writeNow(logger);
    writeNow(logger);
    TEST_ASSERT_EQUAL_STRING(("1 I logger file 4\n" + full + banner(t) + t + " I t fits\n").c_str(),
                             a.content.c_str());
    TEST_ASSERT_EQUAL_STRING(
        (t + " I logger file 5\n" + t + " I t goes to the other file\n").c_str(),
        b.content.c_str());
    TEST_ASSERT_TRUE(a.content.size() <= Logger::FILE_LIMIT_BYTES);
}

// no-card: without begin(), as without a card, the lines are lost.
static void testNoCard() {
    FakeFile a;
    FakeFile b;
    Logger logger(a, b, buffer, sizeof(buffer));
    TEST_ASSERT_FALSE(logger.write(LogLevel::INFO, "t", "no card"));
    TEST_ASSERT_FALSE(logger.write(LogLevel::INFO, "t", "still none"));
    writeNow(logger);
    TEST_ASSERT_EQUAL_UINT32(2, logger.lost());
    TEST_ASSERT_EQUAL_UINT32(0, a.writes + b.writes);
}

// interrupt: a line from an interrupt is lost before the lock.
static void testInterrupt() {
    FakeFile a;
    FakeFile b;
    Logger logger(a, b, buffer, sizeof(buffer));
    logger.begin(startOf(a), startOf(b));
    fakeInInterrupt = true;
    const uint32_t locks = fakeLocksTaken;
    TEST_ASSERT_FALSE(logger.write(LogLevel::WARNING, "isr", "x"));
    TEST_ASSERT_EQUAL_UINT32(locks, fakeLocksTaken);
    TEST_ASSERT_EQUAL_UINT32(1, logger.lost());
}

// free-lines: the free functions reach the logger that began last.
static void testFreeLines() {
    FakeFile a;
    FakeFile b;
    Logger logger(a, b, buffer, sizeof(buffer));
    logger.begin(startOf(a), startOf(b));
    const std::string t = nowText();
    logInfo("host", "game %d", 2);
    logWarning("lamps", "no lamp %u", 7u);
    writeNow(logger);
    const std::string expected = t + " I host game 2\n" + t + " W lamps no lamp 7\n";
    TEST_ASSERT_EQUAL_STRING(expected.c_str(),
                             a.content.substr(a.content.size() - expected.size()).c_str());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(testFormat);
    RUN_TEST(testBufferFull);
    RUN_TEST(testOneWrite);
    RUN_TEST(testFailedWrite);
    RUN_TEST(testHead);
    RUN_TEST(testTurn);
    RUN_TEST(testNoCard);
    RUN_TEST(testInterrupt);
    RUN_TEST(testFreeLines);
    return UNITY_END();
}
