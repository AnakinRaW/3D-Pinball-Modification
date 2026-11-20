#include "logger.h"
#include "logging/debug-log.h"
#include "logging/log-line.h"
#include "core/task-lock.h"
#include "core/version.h"
#include <Arduino.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The first line of each file numbers it: "<ms> I logger file <number>".
constexpr char HEAD_TAG[] = "I logger";
constexpr char HEAD_TEXT[] = " I logger file ";
// The level's letter and the writer's name, such as "W lamps". A longer name is cut.
constexpr size_t TAG_BYTES = 24;
// The banner at every start names the machine, the version and the kind of build that runs.
constexpr char MACHINE_NAME[] = "AnakinRaW's PINBALL Modification";
constexpr char BANNER_RULE[] = "====================================";
#if defined(DEBUG)
constexpr char BUILD[] = "debug";
#else
constexpr char BUILD[] = "release";
#endif

// The logger whose begin() ran last. The firmware's logger lives for the whole run.
static std::atomic<Logger*> firmwareLog{nullptr};

// Formats a line of the logger itself.
static size_t formatOwnLine(char* out, uint32_t ms, const char* tag, const char* format, ...)
    __attribute__((format(printf, 4, 5)));

static size_t formatOwnLine(char* out, uint32_t ms, const char* tag, const char* format, ...) {
    va_list args;
    va_start(args, format);
    const size_t n = formatLogLine(out, ms, tag, format, args);
    va_end(args);
    return n;
}

// Gets the number in a file's head line, false when the bytes start with no head line.
static bool readHead(const LogFileStart& start, uint32_t& number) {
    char text[Logger::HEAD_BYTES + 1];
    const size_t n = start.got < Logger::HEAD_BYTES ? start.got : Logger::HEAD_BYTES;
    if (start.bytes == nullptr) {
        return false;
    }
    memcpy(text, start.bytes, n);
    text[n] = '\0';
    char* const end = strchr(text, '\n');
    if (end == nullptr) {
        return false;
    }
    *end = '\0';
    // The time in front of the head line is digits alone.
    const char* const head = strstr(text, HEAD_TEXT);
    if (head == nullptr || head == text || strspn(text, "0123456789") != size_t(head - text)) {
        return false;
    }
    const char* const digits = head + sizeof(HEAD_TEXT) - 1;
    char* after = nullptr;
    const unsigned long value = strtoul(digits, &after, 10);
    if (after == digits || *after != '\0') {
        return false;
    }
    number = static_cast<uint32_t>(value);
    return true;
}

Logger::Logger(AppendFile& first, AppendFile& second, uint8_t* buffer, size_t bytes)
    : files{&first, &second}, appender(buffer, bytes) {}

void Logger::begin(const LogFileStart& first, const LogFileStart& second) {
    const uint32_t sizeBytes[2] = {first.sizeBytes, second.sizeBytes};
    uint32_t number[2] = {0, 0};
    const bool numbered[2] = {readHead(first, number[0]), readHead(second, number[1])};
    // The file with the higher number was started last. A file without a head line is the older
    // one. Without any head line the log goes on in the first file.
    current = numbered[1] && (!numbered[0] || number[1] > number[0]) ? 1 : 0;
    fileNumber = numbered[current] ? number[current] : 1;
    headOwed = sizeBytes[current] == 0;
    {
        const TaskLock lock;
        appender.begin(*files[current], sizeBytes[current]);
        ready = true;
    }
    firmwareLog.store(this, std::memory_order_release);
    // A start stands out in the log.
    write(LogLevel::INFO, "logger", "%s", BANNER_RULE);
    write(LogLevel::INFO, "logger", "%s", MACHINE_NAME);
    write(LogLevel::INFO, "logger", "firmware %s %s build", FIRMWARE_VERSION, BUILD);
    write(LogLevel::INFO, "logger", "%s", BANNER_RULE);
}

// Gets the tag of a line: the level's letter and the writer's name, such as "W lamps".
static void formatTag(char (&tag)[TAG_BYTES], LogLevel level, const char* who) {
    snprintf(tag, sizeof(tag), "%c %s", level == LogLevel::WARNING ? 'W' : 'I', who);
}

// Writes a line into the firmware's log, or before it has begun into the debug output.
static void logArgs(LogLevel level, const char* who, const char* format, va_list args)
    __attribute__((format(printf, 3, 0)));

void logInfo(const char* who, const char* format, ...) {
    va_list args;
    va_start(args, format);
    logArgs(LogLevel::INFO, who, format, args);
    va_end(args);
}

void logWarning(const char* who, const char* format, ...) {
    va_list args;
    va_start(args, format);
    logArgs(LogLevel::WARNING, who, format, args);
    va_end(args);
}

static void logArgs(LogLevel level, const char* who, const char* format, va_list args) {
    Logger* const log = firmwareLog.load(std::memory_order_acquire);
    if (log != nullptr) {
        log->writeArgs(level, who, format, args);
        return;
    }
    if (inInterrupt()) {
        return;
    }
    char tag[TAG_BYTES];
    formatTag(tag, level, who);
    char line[LOG_LINE_BYTES];
    debugLogLine(line, formatLogLine(line, millis(), tag, format, args));
}

bool Logger::write(LogLevel level, const char* who, const char* format, ...) {
    va_list args;
    va_start(args, format);
    const bool added = writeArgs(level, who, format, args);
    va_end(args);
    return added;
}

bool Logger::writeArgs(LogLevel level, const char* who, const char* format, va_list args) {
    if (inInterrupt()) {
        // An interrupt may stop the task that holds the lock, so it never takes it.
        lostTotal.fetch_add(1, std::memory_order_relaxed);
        lostUnreported.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    char tag[TAG_BYTES];
    formatTag(tag, level, who);
    const uint32_t ms = millis();
    char line[LOG_LINE_BYTES];
    const size_t n = formatLogLine(line, ms, tag, format, args);
    debugLogLine(line, n);

    bool added = false;
    {
        // The buffer takes one writer at a time. The lines of a new file have to follow its
        // head line.
        const TaskLock lock;
        added = ready && appendLine(ms, line, n);
    }
    if (!added) {
        lostTotal.fetch_add(1, std::memory_order_relaxed);
        lostUnreported.fetch_add(1, std::memory_order_relaxed);
    }
    return added;
}

bool Logger::appendLine(uint32_t ms, const char* line, size_t n) {
    if (!headOwed && appender.fileBytes() + n > FILE_LIMIT_BYTES) {
        if (appender.start(*files[1 - current])) {
            current = static_cast<uint8_t>(1 - current);
            fileNumber = fileNumber + 1;
            headOwed = true;
        }
    }
    if (headOwed && !appendHead(ms)) {
        return false;
    }
    const uint32_t unreported = lostUnreported.load(std::memory_order_relaxed);
    if (unreported != 0) {
        char notice[LOG_LINE_BYTES];
        const size_t length =
            formatOwnLine(notice, ms, "W logger", "%" PRIu32 " lines lost", unreported);
        if (!appender.append(notice, length)) {
            appender.takeLost();
            return false;
        }
        lostUnreported.fetch_sub(unreported, std::memory_order_relaxed);
    }
    if (!appender.append(line, n)) {
        // The logger counts its lost lines itself, the notice included.
        appender.takeLost();
        return false;
    }
    return true;
}

bool Logger::appendHead(uint32_t ms) {
    char head[LOG_LINE_BYTES];
    const size_t n = formatOwnLine(head, ms, HEAD_TAG, "file %" PRIu32, fileNumber);
    if (!appender.append(head, n)) {
        appender.takeLost();
        return false;
    }
    headOwed = false;
    return true;
}

void Logger::update() {
    appender.update();
}
