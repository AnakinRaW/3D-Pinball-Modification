// The machine's log on the SD card. A game, a component, a driver's owner or the host writes a
// line. The logger stamps it with the time since the start and who wrote it. No writer ever
// waits for the card. The log takes two files of up to 1 MB in turns, so the latest megabyte of
// lines always stays on the card.
//
// Design: docs/firmware/logging.md
#pragma once
#include "io/card-appender.h"
#include <atomic>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

/// How much a line weighs. Its letter follows the time in the line.
enum class LogLevel : uint8_t {
    INFO,      // I: a fact worth keeping, such as a restart's cause
    WARNING,   // W: something went wrong, such as a setting out of its range
};

/// What the start of a log file holds, as the main task reads it from the card at start-up.
struct LogFileStart {
    const uint8_t* bytes = nullptr;   // the file's first bytes
    size_t got = 0;                   // how many first bytes there are
    uint32_t sizeBytes = 0;           // the file's size, 0 for a missing file
};

class Logger {
public:
    static constexpr uint32_t FILE_LIMIT_BYTES = 1048576;   // logging.md: one log file's size
    static constexpr size_t BUFFER_BYTES = 65536;           // logging.md: the buffer in the PSRAM
    static constexpr size_t HEAD_BYTES = 64;                // the start of a file begin() looks at

    /// Creates the logger.
    ///
    /// It writes nothing before begin().
    ///
    /// @param first   The log's first file on the card.
    /// @param second  The log's second file on the card.
    /// @param buffer  Holds the lines until they are on the card. It stays with the logger for its
    ///                whole life.
    /// @param bytes   The buffer's size, of which the largest power of two is used.
    Logger(AppendFile& first, AppendFile& second, uint8_t* buffer, size_t bytes);

    /// Finds the file the log goes on in and its end, makes the logger the one logInfo() and
    /// logWarning() write to, then writes the banner of a start: the machine's name, the firmware's
    /// version and whether a debug or a release build runs.
    ///
    /// Bytes past HEAD_BYTES are not looked at.
    ///
    /// @param first   The start of the file the constructor took first.
    /// @param second  The start of the file the constructor took second.
    /// @note          Never waits. The main task calls it once at start-up, before the main loop.
    ///                Without a card the main task does not call it, and every line is lost.
    void begin(const LogFileStart& first, const LogFileStart& second);

    /// Writes one line, formatted like printf, into the log.
    ///
    /// In a debug build the line goes to the debug output as well. A lost line is counted in lost()
    /// and the log states the count in a separate line once a line fits again.
    ///
    /// @param level   The line's weight.
    /// @param who     Names the writer in a word, such as "lamps".
    /// @param format  The line's format, as for printf, followed by the values it takes.
    /// @return        False when the line is lost: before begin(), in an interrupt, or when the
    ///                buffer is full.
    /// @note          Never waits. Any task, never an interrupt.
    bool write(LogLevel level, const char* who, const char* format, ...)
        __attribute__((format(printf, 4, 5)));

    /// Writes one line as write() does, with its arguments in @p args.
    ///
    /// @param level   The line's weight.
    /// @param who     Names the writer in a word, such as "lamps".
    /// @param format  The line's format, as for vprintf.
    /// @param args    The values the format takes.
    bool writeArgs(LogLevel level, const char* who, const char* format, va_list args)
        __attribute__((format(printf, 4, 0)));

    /// Writes the waiting lines to the card once a second, or sooner once half the buffer is
    /// taken.
    ///
    /// @note  Never waits. The main loop calls it in every pass.
    void update();

    /// Makes the next update() write the waiting lines at once.
    ///
    /// @note  Any task may call it, never an interrupt.
    void flush() { appender.flush(); }

    /// Gets how many lines were lost since the start.
    uint32_t lost() const { return lostTotal.load(std::memory_order_relaxed); }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    /// Appends a formatted line, after the head line of a new file and the count of lost lines.
    ///
    /// @param ms    The time in front of the line, in milliseconds.
    /// @param line  The formatted line.
    /// @param n     The line's length.
    /// @note        Under the lock.
    bool appendLine(uint32_t ms, const char* line, size_t n);

    /// Appends the head line that numbers the current file.
    ///
    /// @param ms  The time in front of the line, in milliseconds.
    /// @note      Under the lock.
    bool appendHead(uint32_t ms);

    AppendFile* files[2];
    CardAppender appender;
    uint8_t current = 0;       // the index in files of the file the log writes
    uint32_t fileNumber = 0;   // the number in the current file's head line
    bool ready = false;        // begin() found the log's end
    bool headOwed = false;     // the current file still lacks its head line
    std::atomic<uint32_t> lostTotal{0};
    std::atomic<uint32_t> lostUnreported{0};   // lost lines no line of the log states yet
};

/// Writes a line of the level INFO into the firmware's log, the Logger whose begin() ran last, as
/// Logger::write() does.
///
/// Before any Logger has begun, a debug build shows the line on USB in the same form, and a
/// release build drops it.
///
/// @param who     Names the writer in a word, such as "lamps".
/// @param format  The line's format, as for printf, followed by the values it takes.
/// @note          Any task may call it, never an interrupt. It never waits.
void logInfo(const char* who, const char* format, ...) __attribute__((format(printf, 2, 3)));

/// Writes a line of the level WARNING, otherwise as logInfo().
///
/// The warning also enters warningList, before the log has begun and without a card as well.
///
/// @param who     Names the writer in a word, such as "lamps".
/// @param format  The line's format, as for printf, followed by the values it takes.
void logWarning(const char* who, const char* format, ...) __attribute__((format(printf, 2, 3)));
