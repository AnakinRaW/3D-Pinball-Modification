// A buffer of debug lines and their way out to USB serial. Any task adds a line without waiting,
// and lines from two tasks never mix. The main loop hands USB only as many bytes as it takes
// without waiting. When the buffer is full, the oldest waiting lines give way to the new one.
#pragma once
#include "core/ring.h"
#include <atomic>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

class DebugOutput {
public:
    /// Creates the output with its buffer.
    ///
    /// @param buffer  Holds the waiting lines and stays with the output for its whole life.
    /// @param bytes   The buffer's size, of which the largest power of two is used.
    DebugOutput(uint8_t* buffer, size_t bytes) : ring(buffer, bytes) {}

    /// Formats a line like vprintf with the milliseconds since the start and the letter D in
    /// front, and adds it.
    ///
    /// When the buffer is full, the oldest waiting lines give way to it and are counted in lost().
    ///
    /// @param format  The format of the line, as for vprintf.
    /// @param args    The values the format takes.
    /// @return        False when the line is dropped, which happens in an interrupt alone.
    /// @note          Never waits.
    bool write(const char* format, va_list args) __attribute__((format(printf, 2, 0)));

    /// Adds a line that is formatted already.
    ///
    /// @param line  The line with its line break.
    /// @param n     The length of the line.
    /// @return      False as write() returns it.
    /// @note        Never waits.
    bool writeLine(const char* line, size_t n);

    /// Sends as many waiting bytes to USB serial as it takes without waiting.
    ///
    /// Once lines were dropped since the last notice and the buffer has room, it adds a notice
    /// such as "73021 D debug 12 lines lost" behind the waiting lines and sends that as well.
    ///
    /// @return  The count of bytes sent.
    /// @note    One task at a time, never an interrupt.
    size_t send();

    /// Gets how many lines were dropped, in an interrupt or to make room.
    uint32_t lost() const { return lostCount.load(std::memory_order_relaxed); }

    DebugOutput(const DebugOutput&) = delete;
    DebugOutput& operator=(const DebugOutput&) = delete;

private:
    /// Drops the oldest waiting line, or the rest of a line send() has begun.
    ///
    /// @return  False when the buffer is empty.
    /// @note    Under the lock.
    bool dropOldestLine();

    /// Adds the notice of the lines dropped since the last one, where the buffer has room for it.
    ///
    /// @note  Under the lock.
    void addLossNotice();

    /// Both the writers and send() hold the lock while they move the ring, since a full ring makes
    /// a writer drop lines from the reader's end.
    Ring<uint8_t> ring;
    std::atomic<uint32_t> lostCount{0};
    uint32_t reportedLost = 0;   // the dropped lines the notices have stated, moved by send()
};
