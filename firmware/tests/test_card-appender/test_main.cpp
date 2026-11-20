// Tests on the computer whether the card appender writes its records once a second, sooner when
// half its buffer is taken, keeps them through a failed write and never loses one unnoticed. The
// files are fakes whose writes run until the test ends them. The fake GPT2_CNT of tests/fakes
// sets the time.
//
//   period        no write before a second has passed, then one write with every waiting record
//   half-full     a buffer half taken is written at once
//   flush         flush() makes the next update() write at once
//   retry         a failed write keeps its records. They go to the same place a second later,
//                 not sooner however full the buffer is
//   start         records before start() go to the old file, those after it start the new one
//                 anew. A second start() waits for the first
//   lost          a record that does not fit is refused and counted until takeLost()
//   no-file       a record before begin() or start() is refused and counted
//   wrap          records that run past the end of the buffer reach the card in two writes, the
//                 second right after the first
//
// Run: pio test -e computer, which runs in CI as well.

#include "io/card-appender.h"
#include "core/clock.h"
#include <string>
#include <unity.h>

constexpr size_t BUFFER_BYTES = 64;
constexpr uint32_t US_PER_MS = 1000;
constexpr uint32_t START_US = 5000000;

// A file whose write runs until the test calls end().
struct FakeFile : AppendFile {
    std::string content;
    AppendStatus state = AppendStatus::IDLE;
    uint32_t writes = 0;
    bool lastRewrite = false;
    uint32_t lastAt = 0;
    std::string lastBytes;

    void write(uint32_t at, const uint8_t* data, size_t n) override {
        TEST_ASSERT_TRUE(state != AppendStatus::PENDING);
        writes = writes + 1;
        lastRewrite = false;
        lastAt = at;
        lastBytes.assign(reinterpret_cast<const char*>(data), n);
        state = AppendStatus::PENDING;
    }

    void rewrite(const uint8_t* data, size_t n) override {
        write(0, data, n);
        lastRewrite = true;
    }

    AppendStatus status() const override { return state; }

    // Ends the running write, with the bytes reaching the file or not.
    void end(bool ok) {
        TEST_ASSERT_TRUE(state == AppendStatus::PENDING);
        if (ok) {
            if (lastRewrite) {
                content.clear();
            }
            content.resize(lastAt);
            content.append(lastBytes);
        }
        state = ok ? AppendStatus::SUCCEEDED : AppendStatus::FAILED;
    }
};

void setUp() {
    GPT2_CNT = START_US;
}

void tearDown() {}

static void advanceMs(uint32_t ms) {
    GPT2_CNT = GPT2_CNT + ms * US_PER_MS;
}

// period: the records wait for a second and then go in one write.
static void testPeriod() {
    uint8_t buffer[BUFFER_BYTES];
    CardAppender appender(buffer, sizeof(buffer));
    FakeFile file;
    file.content = "old\n";
    appender.begin(file, 4);
    TEST_ASSERT_TRUE(appender.append("a\n", 2));
    TEST_ASSERT_TRUE(appender.append("b\n", 2));
    advanceMs(CardAppender::WRITE_PERIOD_MS - 1);
    appender.update();
    TEST_ASSERT_EQUAL_UINT32(0, file.writes);
    advanceMs(1);
    appender.update();
    TEST_ASSERT_EQUAL_UINT32(1, file.writes);
    TEST_ASSERT_EQUAL_UINT32(4, file.lastAt);
    file.end(true);
    appender.update();
    TEST_ASSERT_EQUAL_STRING("old\na\nb\n", file.content.c_str());
    TEST_ASSERT_EQUAL_UINT32(8, appender.fileBytes());
}

// half-full: half the buffer taken starts a write without waiting for the second.
static void testHalfFull() {
    uint8_t buffer[BUFFER_BYTES];
    CardAppender appender(buffer, sizeof(buffer));
    FakeFile file;
    appender.begin(file, 0);
    const std::string record(BUFFER_BYTES / 2 - 1, 'r');
    TEST_ASSERT_TRUE(appender.append(record.data(), record.size()));
    appender.update();
    TEST_ASSERT_EQUAL_UINT32(0, file.writes);
    TEST_ASSERT_TRUE(appender.append("s", 1));
    appender.update();
    TEST_ASSERT_EQUAL_UINT32(1, file.writes);
    TEST_ASSERT_EQUAL_size_t(BUFFER_BYTES / 2, file.lastBytes.size());
}

// flush: flush() starts the write at the next update().
static void testFlush() {
    uint8_t buffer[BUFFER_BYTES];
    CardAppender appender(buffer, sizeof(buffer));
    FakeFile file;
    appender.begin(file, 0);
    TEST_ASSERT_TRUE(appender.append("end of game\n", 12));
    appender.flush();
    appender.update();
    TEST_ASSERT_EQUAL_UINT32(1, file.writes);
    file.end(true);
    appender.update();
    TEST_ASSERT_EQUAL_STRING("end of game\n", file.content.c_str());
}

// retry: a failed write keeps its records for the same place, a second later.
static void testRetry() {
    uint8_t buffer[BUFFER_BYTES];
    CardAppender appender(buffer, sizeof(buffer));
    FakeFile file;
    file.content = "0123";
    appender.begin(file, 4);
    TEST_ASSERT_TRUE(appender.append("a\n", 2));
    advanceMs(CardAppender::WRITE_PERIOD_MS);
    appender.update();
    file.end(false);
    appender.update();
    TEST_ASSERT_EQUAL_UINT32(1, file.writes);
    // A buffer half taken does not hurry the next try.
    const std::string record(BUFFER_BYTES / 2, 'r');
    TEST_ASSERT_TRUE(appender.append(record.data(), record.size()));
    appender.flush();
    advanceMs(CardAppender::WRITE_PERIOD_MS - 1);
    appender.update();
    TEST_ASSERT_EQUAL_UINT32(1, file.writes);
    advanceMs(1);
    appender.update();
    TEST_ASSERT_EQUAL_UINT32(2, file.writes);
    TEST_ASSERT_EQUAL_UINT32(4, file.lastAt);
    file.end(true);
    appender.update();
    TEST_ASSERT_EQUAL_STRING(("0123a\n" + record).c_str(), file.content.c_str());
    TEST_ASSERT_EQUAL_UINT32(0, appender.lost());
}

// start: the old file gets the records before start(), the new one is started anew with the rest.
static void testStart() {
    uint8_t buffer[BUFFER_BYTES];
    CardAppender appender(buffer, sizeof(buffer));
    FakeFile first;
    FakeFile second;
    FakeFile third;
    second.content = "a whole old game\n";
    appender.begin(first, 0);
    TEST_ASSERT_TRUE(appender.append("old\n", 4));
    TEST_ASSERT_TRUE(appender.start(second));
    TEST_ASSERT_EQUAL_UINT32(0, appender.fileBytes());
    TEST_ASSERT_TRUE(appender.append("new\n", 4));
    TEST_ASSERT_EQUAL_UINT32(4, appender.fileBytes());
    TEST_ASSERT_FALSE(appender.start(third));
    appender.flush();
    appender.update();
    TEST_ASSERT_EQUAL_STRING("old\n", first.lastBytes.c_str());
    first.end(true);
    appender.flush();
    appender.update();
    TEST_ASSERT_EQUAL_UINT32(1, second.writes);
    TEST_ASSERT_TRUE(second.lastRewrite);
    second.end(true);
    appender.update();
    TEST_ASSERT_EQUAL_STRING("old\n", first.content.c_str());
    TEST_ASSERT_EQUAL_STRING("new\n", second.content.c_str());
    TEST_ASSERT_TRUE(appender.start(third));
    TEST_ASSERT_EQUAL_UINT32(0, third.writes);
}

// lost: a record that does not fit is lost and counted until takeLost() reads the count.
static void testLost() {
    uint8_t buffer[BUFFER_BYTES];
    CardAppender appender(buffer, sizeof(buffer));
    FakeFile file;
    appender.begin(file, 0);
    const std::string record(BUFFER_BYTES - 1, 'r');
    TEST_ASSERT_TRUE(appender.append(record.data(), record.size()));
    TEST_ASSERT_FALSE(appender.append("ab", 2));
    TEST_ASSERT_FALSE(appender.append("cd", 2));
    TEST_ASSERT_TRUE(appender.append("e", 1));
    TEST_ASSERT_EQUAL_UINT32(2, appender.takeLost());
    TEST_ASSERT_EQUAL_UINT32(0, appender.takeLost());
    TEST_ASSERT_EQUAL_UINT32(2, appender.lost());
}

// Appends a record and writes it after the next second.
static void appendAndWrite(CardAppender& appender, FakeFile& file, const std::string& record) {
    TEST_ASSERT_TRUE(appender.append(record.data(), record.size()));
    advanceMs(CardAppender::WRITE_PERIOD_MS);
    appender.update();
    file.end(true);
    appender.update();
}

// wrap: the piece past the end of the buffer follows the first piece at once.
static void testWrap() {
    uint8_t buffer[BUFFER_BYTES];
    CardAppender appender(buffer, sizeof(buffer));
    FakeFile file;
    appender.begin(file, 0);
    const std::string a(30, 'a');
    const std::string b(30, 'b');
    appendAndWrite(appender, file, a);
    appendAndWrite(appender, file, b);
    // 20 bytes from place 60 of 64: 4 up to the end, 16 from the start.
    const std::string c(20, 'c');
    TEST_ASSERT_TRUE(appender.append(c.data(), c.size()));
    advanceMs(CardAppender::WRITE_PERIOD_MS);
    appender.update();
    TEST_ASSERT_EQUAL_size_t(4, file.lastBytes.size());
    file.end(true);
    appender.update();
    TEST_ASSERT_EQUAL_UINT32(4, file.writes);
    TEST_ASSERT_EQUAL_size_t(16, file.lastBytes.size());
    file.end(true);
    appender.update();
    TEST_ASSERT_EQUAL_STRING((a + b + c).c_str(), file.content.c_str());
}

// no-file: without a file every record is refused.
static void testNoFile() {
    uint8_t buffer[BUFFER_BYTES];
    CardAppender appender(buffer, sizeof(buffer));
    TEST_ASSERT_FALSE(appender.append("a", 1));
    appender.update();
    TEST_ASSERT_EQUAL_UINT32(1, appender.lost());
    FakeFile file;
    TEST_ASSERT_TRUE(appender.start(file));
    TEST_ASSERT_TRUE(appender.append("b\n", 2));
    appender.flush();
    appender.update();
    TEST_ASSERT_TRUE(file.lastRewrite);
    TEST_ASSERT_EQUAL_STRING("b\n", file.lastBytes.c_str());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(testPeriod);
    RUN_TEST(testHalfFull);
    RUN_TEST(testFlush);
    RUN_TEST(testRetry);
    RUN_TEST(testStart);
    RUN_TEST(testLost);
    RUN_TEST(testNoFile);
    RUN_TEST(testWrap);
    return UNITY_END();
}
