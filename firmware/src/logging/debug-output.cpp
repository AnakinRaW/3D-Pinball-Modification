#include "debug-output.h"
#include "logging/log-line.h"
#include "core/task-lock.h"
#include <Arduino.h>
#include <inttypes.h>
#include <string.h>

// The most send() hands USB serial under one hold of the lock. It bounds how long the lock holds.
constexpr size_t SEND_PIECE_BYTES = 512;

bool DebugOutput::write(const char* format, va_list args) {
    if (inInterrupt()) {
        // An interrupt may stop a task that holds the lock, so it never takes it.
        lostCount.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    char line[LOG_LINE_BYTES];
    const size_t n = formatLogLine(line, millis(), "D", format, args);
    return writeLine(line, n);
}

bool DebugOutput::writeLine(const char* line, size_t n) {
    if (inInterrupt()) {
        lostCount.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    // The ring takes one writer at a time. Making room moves the reader's end as well.
    const TaskLock lock;
    while (ring.size() - ring.used() < n && dropOldestLine()) {
        lostCount.fetch_add(1, std::memory_order_relaxed);
    }
    if (!ring.append(line, n)) {
        // Only a line longer than the whole buffer is left.
        lostCount.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    return true;
}

bool DebugOutput::dropOldestLine() {
    for (;;) {
        size_t n = 0;
        const uint8_t* bytes = ring.peek(n);
        if (n == 0) {
            return false;
        }
        const void* const lineEnd = memchr(bytes, '\n', n);
        if (lineEnd != nullptr) {
            ring.consume(static_cast<const uint8_t*>(lineEnd) - bytes + 1);
            return true;
        }
        // The line runs on past the end of the buffer.
        ring.consume(n);
    }
}

// Formats a line of the debug output itself.
static size_t formatOwnLine(char* out, const char* format, ...)
    __attribute__((format(printf, 2, 3)));

static size_t formatOwnLine(char* out, const char* format, ...) {
    va_list args;
    va_start(args, format);
    const size_t n = formatLogLine(out, millis(), "D", format, args);
    va_end(args);
    return n;
}

void DebugOutput::addLossNotice() {
    const uint32_t lost = lostCount.load(std::memory_order_relaxed);
    if (lost == reportedLost) {
        return;
    }
    char line[LOG_LINE_BYTES];
    const size_t n = formatOwnLine(line, "debug %" PRIu32 " lines lost", lost - reportedLost);
    // The notice makes no room for itself, which would drop the lines it is about to count.
    if (ring.append(line, n)) {
        reportedLost = lost;
    }
}

size_t DebugOutput::send() {
    size_t sent = 0;
    for (;;) {
        // A writer that makes room drops lines from this end, so the lock covers the piece from
        // the look at the ring to its consume.
        const TaskLock lock;
        addLossNotice();
        size_t n = 0;
        const uint8_t* bytes = ring.peek(n);
        // The Teensy core's availableForWrite() holds back USB serial's timed send of a partly
        // filled buffer while it counts. A send it holds back is not repeated. So it is asked
        // only when bytes wait. The write that follows starts the timed send anew.
        if (n == 0) {
            return sent;
        }
        // USB serial waits once its buffers are full, so it gets no more than they take.
        const int room = Serial.availableForWrite();
        if (room <= 0) {
            return sent;
        }
        n = n < static_cast<size_t>(room) ? n : static_cast<size_t>(room);
        n = n < SEND_PIECE_BYTES ? n : SEND_PIECE_BYTES;
        const size_t taken = Serial.write(bytes, n);
        ring.consume(taken);
        sent = sent + taken;
        if (taken < n) {
            return sent;
        }
    }
}
