// The logger program of docs/firmware/testing.md. It tests on the Teensy whether the lines of two
// tasks reach the log whole and in their order while one task preempts the other.
//
//   two-tasks  a task above the test task's priority writes numbered lines in short bursts while
//              the test task writes its own numbered lines and runs update()
//
// Wiring: none. The log's files are two files in RAM2 that end every write at once, so the test
// needs no card.

#include <Arduino.h>
#include <FreeRTOS.h>
#include <string.h>
#include <task.h>
#include "logging/logger.h"
#include "test-harness.h"

constexpr uint32_t LIMIT_S = 20;
constexpr size_t FILE_BYTES = 192 * 1024;   // more than the test writes into one file
constexpr uint32_t URGENT_LINES = 2000;
constexpr uint32_t URGENT_BURST = 5;         // lines between two pauses of the urgent task
constexpr uint32_t CALM_PAUSE_US = 40;       // keeps the calm task's lines under the file's size
constexpr UBaseType_t URGENT_PRIORITY = 2;   // above the test task's 1, as the storage task
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

DMAMEM static char firstBytes[FILE_BYTES];
DMAMEM static char secondBytes[8];
DMAMEM static uint8_t buffer[Logger::BUFFER_BYTES];
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

static const Test TESTS[] = {
    {"logger.two-tasks", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testTwoTasks, nullptr},
};

extern const Program LOGGER = {TESTS, sizeof TESTS / sizeof TESTS[0]};
