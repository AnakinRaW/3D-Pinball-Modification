// The logger program of docs/firmware/testing.md. It tests on the Teensy whether the lines of two
// tasks reach the log whole and in their order while one task preempts the other. It also tests
// how the log goes on across restarts and full files on the card.
//
//   two-tasks     a task above the test task's priority writes numbered lines in short bursts
//                 while the test task writes its own numbered lines and runs update()
//   warning-tasks the same two tasks write warnings into the firmware's warning list, some
//                 shared and more than it holds. Every warning lands in exactly one count or in
//                 dropped(), and every entry stays whole. The urgent task's longest call is
//                 printed
//   order         numbered lines written over three starts stand in the log in their order
//   files         with the newer file at its limit, the log starts the other file anew with a
//                 higher number. The full file stays as it was
//   card-missing  without a card the log does not begin. A line is lost and counted (bench)
//
// Wiring: none. The card sits in the Teensy's socket. two-tasks writes into two files in RAM2 that
// end every write at once. The other tests leave their files in /tests/logger/ on the card.

#include <Arduino.h>
#include <FreeRTOS.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <task.h>
#include "drivers/storage.h"
#include "logging/logger.h"
#include "logging/warning-list.h"
#include "test-harness.h"

constexpr uint32_t LIMIT_S = 20;
constexpr size_t FILE_BYTES = 192 * 1024;   // more than the test writes into one file
constexpr uint32_t URGENT_LINES = 2000;
constexpr uint32_t URGENT_BURST = 5;         // lines between two pauses of the urgent task
constexpr uint32_t CALM_PAUSE_US = 40;       // keeps the calm task's lines under the file's size
constexpr UBaseType_t URGENT_PRIORITY = 2;   // above the test task's 1
constexpr uint32_t URGENT_STACK_BYTES = 4096;

// A file in RAM whose writes end at once.
class RamFile : public AppendFile {
public:
    explicit RamFile(char* bytes) : bytes(bytes) {}

    void write(uint32_t at, const uint8_t* data, size_t n) override {
        if (at + n > FILE_BYTES) {
            state = AppendStatus::FAILED;
            return;
        }
        memcpy(bytes + at, data, n);
        size = at + n > size ? at + n : size;
        state = AppendStatus::SUCCEEDED;
    }

    void rewrite(const uint8_t* data, size_t n) override {
        size = 0;
        write(0, data, n);
    }

    AppendStatus status() const override { return state; }

    char* const bytes;
    size_t size = 0;
    AppendStatus state = AppendStatus::IDLE;
};

// The log's buffer and the first file lie in the PSRAM, as the firmware's log buffer does. RAM2
// holds LVGL's buffers.
EXTMEM static char firstBytes[FILE_BYTES];
DMAMEM static char secondBytes[8];
EXTMEM static uint8_t buffer[Logger::BUFFER_BYTES];
static RamFile first(firstBytes);
static RamFile second(secondBytes);
static Logger logger(first, second, buffer, sizeof buffer);
static volatile bool urgentDone = false;

static void writeUrgent(void*) {
    for (uint32_t i = 0; i < URGENT_LINES; ++i) {
        logger.write(LogLevel::INFO, "urgent", "%lu", i);
        if ((i + 1) % URGENT_BURST == 0) {
            vTaskDelay(1);
        }
    }
    urgentDone = true;
    vTaskDelete(nullptr);
}

// Checks one line: "<ms> I <who> <number>", the number the next of its writer, or a line of the
// banner.
static bool checkLine(const char* line, size_t n, uint32_t& urgentNext, uint32_t& calmNext) {
    char text[64];
    if (n == 0 || n >= sizeof text) {
        return false;
    }
    memcpy(text, line, n);
    text[n] = '\0';
    unsigned long ms = 0;
    char who[16];
    unsigned long number = 0;
    if (sscanf(text, "%lu I %15s", &ms, who) != 2) {
        return false;
    }
    // The banner of the start stands between the head line and the writers' lines.
    if (strcmp(who, "logger") == 0) {
        return true;
    }
    if (sscanf(text, "%lu I %15s %lu", &ms, who, &number) != 3) {
        return false;
    }
    uint32_t& next = strcmp(who, "urgent") == 0 ? urgentNext : calmNext;
    if (strcmp(who, "urgent") != 0 && strcmp(who, "calm") != 0) {
        return false;
    }
    if (number != next) {
        return false;
    }
    next = next + 1;
    return true;
}

static void testTwoTasks() {
    // Both files are empty, as on a new card.
    logger.begin(LogFileStart{}, LogFileStart{});
    urgentDone = false;
    if (xTaskCreate(writeUrgent, "urgent", URGENT_STACK_BYTES / sizeof(StackType_t), nullptr,
                    URGENT_PRIORITY, nullptr) != pdPASS) {
        test::fail("no memory for the urgent task", "a task");
        return;
    }
    uint32_t calm = 0;
    while (!urgentDone) {
        logger.write(LogLevel::INFO, "calm", "%lu", calm);
        calm = calm + 1;
        logger.update();
        delayMicroseconds(CALM_PAUSE_US);
    }
    logger.flush();
    for (uint32_t i = 0; i < 4; ++i) {
        logger.update();
    }
    if (logger.lost() != 0) {
        char seen[32];
        snprintf(seen, sizeof seen, "%lu lines lost", logger.lost());
        test::fail(seen, "none lost");
        return;
    }
    // The head line comes first, then every line of both writers in its order.
    const char* at = first.bytes;
    const char* const end = first.bytes + first.size;
    const char* lineEnd = static_cast<const char*>(memchr(at, '\n', end - at));
    if (lineEnd == nullptr || strncmp(at + strcspn(at, " "), " I logger file 1\n", 17) != 0) {
        test::fail("no head line", "the head line of file 1");
        return;
    }
    at = lineEnd + 1;
    uint32_t urgentNext = 0;
    uint32_t calmNext = 0;
    while (at < end) {
        lineEnd = static_cast<const char*>(memchr(at, '\n', end - at));
        if (lineEnd == nullptr || !checkLine(at, lineEnd - at, urgentNext, calmNext)) {
            char seen[48];
            snprintf(seen, sizeof seen, "a broken line at byte %u", unsigned(at - first.bytes));
            test::fail(seen, "whole lines in order");
            return;
        }
        at = lineEnd + 1;
    }
    Serial.printf("%lu urgent and %lu calm lines, %u bytes\n", urgentNext, calmNext,
                  unsigned(first.size));
    if (urgentNext != URGENT_LINES || calmNext != calm) {
        test::fail("lines missing", "every line written");
        return;
    }
    test::pass();
}

// ---- the warning list

constexpr uint32_t WARNING_ROUNDS = 2000;   // warnings the urgent task writes
constexpr uint32_t WARNING_KINDS = 10;      // the different texts each task writes in turn
constexpr uint32_t US_PER_S = 1000000;

static volatile uint32_t urgentWarnings = 0;
static volatile uint32_t longestAddCycles = 0;
static char warningText[96];

// Takes a warning into the test's list, as logWarning() does.
static void warn(const char* who, const char* format, ...) __attribute__((format(printf, 2, 3)));

FLASHMEM static void warn(const char* who, const char* format, ...) {
    va_list args;
    va_start(args, format);
    warningList.add(who, millis(), format, args);
    va_end(args);
}

FLASHMEM static void warnUrgent(void*) {
    for (uint32_t i = 0; i < WARNING_ROUNDS; ++i) {
        const uint32_t t0 = ARM_DWT_CYCCNT;
        // Every second warning is the text both tasks share.
        if (i % 2 == 0) {
            warn("both", "shared");
        } else {
            warn("urgent", "kind %lu", (i / 2) % WARNING_KINDS);
        }
        const uint32_t cycles = ARM_DWT_CYCCNT - t0;
        if (cycles > longestAddCycles) {
            longestAddCycles = cycles;
        }
        urgentWarnings = i + 1;
        if ((i + 1) % URGENT_BURST == 0) {
            vTaskDelay(1);
        }
    }
    urgentDone = true;
    vTaskDelete(nullptr);
}

// Checks that an entry is whole: its writer and its text belong together.
FLASHMEM static bool wholeEntry(const WarningEntry& e) {
    unsigned long kind = 0;
    if (strcmp(e.who, "both") == 0) {
        return strcmp(e.text, "shared") == 0;
    }
    if (strcmp(e.who, "urgent") != 0 && strcmp(e.who, "calm") != 0) {
        return false;
    }
    char rest = 0;
    return sscanf(e.text, "kind %lu%c", &kind, &rest) == 1 && kind < WARNING_KINDS;
}

FLASHMEM static void testWarningTasks() {
    // The firmware's list may hold warnings of the start already. Each of them counts as written.
    uint32_t before = warningList.dropped();
    for (uint8_t i = 0; i < warningList.count(); ++i) {
        WarningEntry e;
        warningList.read(i, e);
        before = before + e.count;
    }
    const uint8_t first = warningList.count();
    // The shared text takes an entry before the urgent task starts.
    warn("both", "shared");
    uint32_t calm = 1;
    urgentDone = false;
    if (xTaskCreate(warnUrgent, "urgent", URGENT_STACK_BYTES / sizeof(StackType_t), nullptr,
                    URGENT_PRIORITY, nullptr) != pdPASS) {
        test::fail("no memory for the urgent task", "a task");
        return;
    }
    while (!urgentDone) {
        if (calm % 2 == 0) {
            warn("both", "shared");
        } else {
            warn("calm", "kind %lu", (calm / 2) % WARNING_KINDS);
        }
        calm = calm + 1;
        delayMicroseconds(CALM_PAUSE_US);
    }
    // Every warning stands in exactly one count or in dropped().
    uint32_t counted = 0;
    for (uint8_t i = 0; i < warningList.count(); ++i) {
        WarningEntry e;
        if (!warningList.read(i, e) || (i >= first && !wholeEntry(e))) {
            snprintf(warningText, sizeof warningText, "entry %u broken", i);
            test::fail(warningText, "whole entries");
            return;
        }
        for (uint8_t j = 0; j < i; ++j) {
            WarningEntry other;
            warningList.read(j, other);
            if (strcmp(other.who, e.who) == 0 && strcmp(other.text, e.text) == 0) {
                snprintf(warningText, sizeof warningText, "entries %u and %u equal", j, i);
                test::fail(warningText, "one entry for each warning");
                return;
            }
        }
        counted = counted + e.count;
    }
    const uint32_t written = before + calm + urgentWarnings;
    const uint32_t longestUs = uint32_t(uint64_t(longestAddCycles) * US_PER_S / F_CPU_ACTUAL);
    Serial.printf("%lu warnings, %lu counted in %u entries, %lu dropped, longest call %lu us\n",
                  written, counted, warningList.count(), warningList.dropped(), longestUs);
    if (warningList.count() != WarningList::ENTRIES || counted + warningList.dropped() != written) {
        snprintf(warningText, sizeof warningText, "%lu counted, %lu dropped, %lu written", counted,
                 warningList.dropped(), written);
        test::fail(warningText,
                   "a full list whose counts and drops add up to the warnings written");
        return;
    }
    test::pass();
}

// ---- the log on the card

constexpr char ORDER_FIRST[] = "/tests/logger/order-a.txt";
constexpr char ORDER_SECOND[] = "/tests/logger/order-b.txt";
constexpr char FULL_FIRST[] = "/tests/logger/full-a.txt";
constexpr char FULL_SECOND[] = "/tests/logger/full-b.txt";
constexpr char MISSING_FIRST[] = "/tests/logger/missing-a.txt";
constexpr char MISSING_SECOND[] = "/tests/logger/missing-b.txt";
constexpr uint32_t ORDER_LINES = 10;   // lines each start writes
constexpr uint32_t ORDER_STARTS = 3;   // the first start and two restarts
constexpr uint32_t SETTLE_MS = 2000;   // more than the card took for any write on the bench
constexpr size_t READ_BYTES = 16384;   // more than the order test's file holds
constexpr size_t FILL_BYTES = 65536;
constexpr uint32_t FULL_NUMBER = 7;    // the head number of the full file
constexpr uint32_t OLDER_NUMBER = 6;   // the head number of the other file
constexpr size_t TAIL_BYTES = 64;      // the end of the full file that has to stay
constexpr uint32_t BENCH_LIMIT_S = 120;
constexpr uint32_t PULL_MS = 60000;   // how long a person may take to pull the card

EXTMEM static uint8_t orderBuffer[Logger::BUFFER_BYTES];
EXTMEM static uint8_t fullBuffer[Logger::BUFFER_BYTES];
EXTMEM static uint8_t missingBuffer[Logger::BUFFER_BYTES];
EXTMEM static char readBack[READ_BYTES + 1] __attribute__((aligned(32)));
EXTMEM static uint8_t fill[FILL_BYTES] __attribute__((aligned(32)));
static uint8_t firstHead[Logger::HEAD_BYTES];
static uint8_t secondHead[Logger::HEAD_BYTES];
static char seenText[96];

// Begins the driver once and skips the test without a card.
static bool cardReady() {
    if (storage.begin()) {
        return true;
    }
    test::skip("no card in the socket");
    return false;
}

// Reads the start of a file, as the main task does for the logger's begin().
static LogFileStart startOf(const char* path, uint8_t (&head)[Logger::HEAD_BYTES]) {
    FileRequest r;
    storage.open(path).read(0, head, sizeof head, r);
    r.wait();
    return {head, r.bytes(), r.fileSize()};
}

// Begins a log in two files on the card, as the main task does.
static void beginOn(Logger& log, const char* first, const char* second) {
    log.begin(startOf(first, firstHead), startOf(second, secondHead));
}

// Writes what the log holds and gives the card time to take it.
static void settle(Logger& log) {
    log.flush();
    const uint32_t t0 = millis();
    while (millis() - t0 < SETTLE_MS) {
        log.update();
        vTaskDelay(1);
    }
}

// Reads a whole file into readBack and ends it with a zero. Returns its length.
static size_t readAll(const char* path) {
    FileRequest r;
    storage.open(path).read(0, readBack, READ_BYTES, r);
    r.wait();
    readBack[r.bytes()] = '\0';
    return r.bytes();
}

// Replaces a file's content with n bytes from data and waits for the card.
static bool put(const char* path, const void* data, size_t n) {
    FileRequest r;
    storage.openWriter(path).rewrite(data, n, r);
    return r.wait();
}

static Logger& orderLog() {
    static CardFile first(storage.openWriter(ORDER_FIRST));
    static CardFile second(storage.openWriter(ORDER_SECOND));
    static Logger log(first, second, orderBuffer, sizeof orderBuffer);
    return log;
}

// Writes the numbered lines of one start.
static void writeStart(uint32_t start) {
    for (uint32_t i = 0; i < ORDER_LINES; ++i) {
        orderLog().write(LogLevel::INFO, "order", "%lu %lu", start, i);
    }
    settle(orderLog());
}

// Gets how many starts have written their lines into the order test's log so far and whether
// every line stands in its order.
static uint32_t startsInLog(bool& inOrder) {
    readAll(ORDER_FIRST);
    uint32_t next = 0;
    inOrder = true;
    for (const char* at = strstr(readBack, " I order "); at != nullptr;
         at = strstr(at + 1, " I order ")) {
        unsigned long start = 0;
        unsigned long line = 0;
        if (sscanf(at, " I order %lu %lu", &start, &line) != 2 ||
            start * ORDER_LINES + line != next) {
            inOrder = false;
        }
        next = next + 1;
    }
    return next / ORDER_LINES;
}

static void testOrder() {
    if (!cardReady() || !put(ORDER_FIRST, "", 0) || !put(ORDER_SECOND, "", 0)) {
        return;
    }
    beginOn(orderLog(), ORDER_FIRST, ORDER_SECOND);
    writeStart(0);
    test::restart();
}

static void resumeOrder() {
    if (!cardReady()) {
        return;
    }
    beginOn(orderLog(), ORDER_FIRST, ORDER_SECOND);
    bool inOrder = false;
    const uint32_t starts = startsInLog(inOrder);
    if (inOrder && starts > 0 && starts < ORDER_STARTS) {
        writeStart(starts);
        test::restart();
    }
    if (!inOrder || starts != ORDER_STARTS) {
        snprintf(seenText, sizeof seenText, "%lu starts, %s", starts,
                 inOrder ? "in order" : "out of order");
        test::fail(seenText, "3 starts, every line in order");
        return;
    }
    test::pass();
}

static Logger& fullLog() {
    static CardFile first(storage.openWriter(FULL_FIRST));
    static CardFile second(storage.openWriter(FULL_SECOND));
    static Logger log(first, second, fullBuffer, sizeof fullBuffer);
    return log;
}

// Writes the full file: a head line with FULL_NUMBER, then lines of x up to the file's limit.
static bool writeFullFile() {
    FileRequest r;
    const FileWriter file = storage.openWriter(FULL_FIRST);
    for (uint32_t at = 0; at < Logger::FILE_LIMIT_BYTES; at += FILL_BYTES) {
        memset(fill, 'x', FILL_BYTES);
        fill[FILL_BYTES - 1] = '\n';
        if (at == 0) {
            const int n = snprintf(reinterpret_cast<char*>(fill), FILL_BYTES,
                                   "0 I logger file %lu\n", FULL_NUMBER);
            fill[n] = 'x';
            fill[n - 1] = '\n';
        }
        if (at == 0) {
            file.rewrite(fill, FILL_BYTES, r);
        } else {
            file.write(at, fill, FILL_BYTES, r);
        }
        if (!r.wait()) {
            return false;
        }
    }
    char older[32];
    const int n = snprintf(older, sizeof older, "0 I logger file %lu\n", OLDER_NUMBER);
    return put(FULL_SECOND, older, size_t(n));
}

static void testFiles() {
    if (!cardReady()) {
        return;
    }
    if (!writeFullFile()) {
        test::fail("the full file could not be written", "a full file on the card");
        return;
    }
    beginOn(fullLog(), FULL_FIRST, FULL_SECOND);
    fullLog().write(LogLevel::INFO, "files", "after the full file");
    settle(fullLog());
    readAll(FULL_SECOND);
    char wantedHead[32];
    snprintf(wantedHead, sizeof wantedHead, " I logger file %lu\n", FULL_NUMBER + 1);
    const char* const head = strstr(readBack, wantedHead);
    const char* const lineEnd = strchr(readBack, '\n');
    if (head == nullptr || lineEnd == nullptr || head + strlen(wantedHead) - 1 != lineEnd ||
        strstr(readBack, " I files after the full file") == nullptr) {
        test::fail("the other file without a new head line and the line",
                   "a head line one number higher, then the line");
        return;
    }
    FileRequest r;
    char tail[TAIL_BYTES];
    storage.open(FULL_FIRST).read(Logger::FILE_LIMIT_BYTES - TAIL_BYTES, tail, TAIL_BYTES, r);
    r.wait();
    if (r.fileSize() != Logger::FILE_LIMIT_BYTES ||
        memcmp(tail, fill + FILL_BYTES - TAIL_BYTES, TAIL_BYTES) != 0) {
        snprintf(seenText, sizeof seenText, "a full file of %lu bytes or with other bytes",
                 r.fileSize());
        test::fail(seenText, "the full file as it was");
        return;
    }
    test::pass();
}

static void testCardMissing() {
    if (!cardReady()) {
        return;
    }
    test::step("pull the card out of the socket");
    const uint32_t t0 = millis();
    while (millis() - t0 < PULL_MS) {
        // The read takes whole sectors, so it reaches the card past SdFat's cache of one sector.
        FileRequest r;
        storage.open(FULL_FIRST).read(0, fill, FILL_BYTES, r);
        if (!r.wait()) {
            test::restart();
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    test::fail("the card still answers", "a read fails once the card is out");
}

static void resumeCardMissing() {
    static CardFile first(storage.openWriter(MISSING_FIRST));
    static CardFile second(storage.openWriter(MISSING_SECOND));
    static Logger log(first, second, missingBuffer, sizeof missingBuffer);
    // The main task begins the log only once the card is mounted.
    const bool card = storage.begin();
    if (card) {
        beginOn(log, MISSING_FIRST, MISSING_SECOND);
    }
    const uint32_t t0 = micros();
    const bool written = log.write(LogLevel::INFO, "missing", "a line without a card");
    const uint32_t us = micros() - t0;
    test::step("put the card back into the socket");
    if (card || written || log.lost() != 1 || us > 50) {
        snprintf(seenText, sizeof seenText, "card %d, written %d, lost %lu, %lu us", card, written,
                 log.lost(), us);
        test::fail(seenText, "no card, a lost line counted at once");
        return;
    }
    test::pass();
}

static const Test TESTS[] = {
    {"logger.two-tasks", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testTwoTasks, nullptr},
    {"logger.warning-tasks", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testWarningTasks, nullptr},
    {"logger.order", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testOrder, resumeOrder},
    {"logger.files", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testFiles, nullptr},
    {"logger.card-missing", RunsOn::BENCH, BENCH_LIMIT_S, BENCH_LIMIT_S, testCardMissing,
     resumeCardMissing},
};

extern const Program LOGGER = {TESTS, sizeof TESTS / sizeof TESTS[0]};
