#include "debug-trace.h"
#include "core/clock.h"
#include "logging/log-line.h"
#include <inttypes.h>
#include <stdarg.h>

#if defined(DEBUG)
// The notes the firmware's trace holds, a burst of interrupts while the main loop is away.
constexpr size_t TRACE_RECORDS = 256;
#endif

// Gets the largest power of two not above n, 0 for 0.
static uint32_t floorPow2(size_t n) {
    uint32_t p = 1;
    while (n != 0 && p <= n / 2) {
        p = p * 2;
    }
    return n == 0 ? 0 : p;
}

DebugTrace::DebugTrace(TraceRecord* records, size_t count)
    : records(records), mask(floorPow2(count) == 0 ? 0 : floorPow2(count) - 1),
      usable(records != nullptr && count != 0) {}

bool DebugTrace::note(const char* label, int32_t a, int32_t b) {
    if (!usable) {
        lostCount.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    // Reserves a place with a compare-and-swap, which a more urgent interrupt can only make retry,
    // so no writer ever waits for another.
    uint32_t place = reserved.load(std::memory_order_relaxed);
    do {
        if (place - taken.load(std::memory_order_acquire) > mask) {
            lostCount.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
    } while (!reserved.compare_exchange_weak(place, place + 1, std::memory_order_acq_rel,
                                             std::memory_order_relaxed));
    TraceRecord& r = records[place & mask];
    r.label = label;
    r.a = a;
    r.b = b;
    r.timeUs = nowUs();
    // The record is whole before the reader sees it.
    r.sequence.store(place + 1, std::memory_order_release);
    return true;
}

bool DebugTrace::take(TraceRecord& out) {
    const uint32_t place = taken.load(std::memory_order_relaxed);
    if (!usable || records[place & mask].sequence.load(std::memory_order_acquire) != place + 1) {
        // The notes were lost after every note still waiting, so their notice comes last.
        const uint32_t lost = lostCount.load(std::memory_order_relaxed);
        if (lost == reportedLost) {
            return false;
        }
        out.label = nullptr;
        out.a = static_cast<int32_t>(lost - reportedLost);
        out.b = 0;
        out.timeUs = nowUs();
        reportedLost = lost;
        return true;
    }
    const TraceRecord& r = records[place & mask];
    out.label = r.label;
    out.a = r.a;
    out.b = r.b;
    out.timeUs = r.timeUs;
    // The place is free for a writer only once the record is copied.
    taken.store(place + 1, std::memory_order_release);
    return true;
}

// Formats a line of the trace.
static size_t formatLine(char* out, uint32_t ms, const char* format, ...)
    __attribute__((format(printf, 3, 4)));

static size_t formatLine(char* out, uint32_t ms, const char* format, ...) {
    va_list args;
    va_start(args, format);
    const size_t n = formatLogLine(out, ms, "D trace", format, args);
    va_end(args);
    return n;
}

size_t formatTraceLine(char* out, uint32_t ms, const TraceRecord& record) {
    if (record.label == nullptr) {
        return formatLine(out, ms, "%" PRId32 " notes lost", record.a);
    }
    return formatLine(out, ms, "%s %" PRId32 " %" PRId32 " at %" PRIu32 " us", record.label,
                      record.a, record.b, record.timeUs);
}

#if defined(DEBUG)

// In RAM1, so a note from an urgent interrupt never waits for the PSRAM's cache.
static TraceRecord traceRecords[TRACE_RECORDS];
static DebugTrace firmwareTrace(traceRecords, TRACE_RECORDS);

void debugTrace(const char* label, int32_t a, int32_t b) {
    firmwareTrace.note(label, a, b);
}

bool takeDebugTrace(TraceRecord& out) {
    return firmwareTrace.take(out);
}

#else

void debugTrace(const char*, int32_t, int32_t) {}

bool takeDebugTrace(TraceRecord&) {
    return false;
}

#endif
