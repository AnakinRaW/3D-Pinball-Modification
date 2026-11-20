// A trace of events for debugging interrupts. DEBUG_TRACE() notes a label, two numbers and the time
// in a few cycles, from any interrupt and any task. The main loop turns the notes into debug lines
// later. So an interrupt can be followed on the bench without ever formatting text or
// waiting. The compiler removes DEBUG_TRACE() in a release build, as it removes DEBUG_LOG().
#pragma once
#include <atomic>
#include <stddef.h>
#include <stdint.h>

// One noted event.
struct TraceRecord {
    const char* label;   // a string literal, such as "ir late", nullptr for a loss notice
    int32_t a;           // for a loss notice, the notes lost
    int32_t b;
    uint32_t timeUs;   // nowUs() when it was noted
    // The record's place in the trace plus one, set once the record is whole. The reader never
    // takes a record a preempted writer has not finished.
    std::atomic<uint32_t> sequence{0};
};

// The ring of notes. Any number of interrupts and tasks write. One task reads. Nobody waits
// or takes a lock.
class DebugTrace {
public:
    // Creates the trace in records.
    // records hold the notes and stay with the trace for its whole life.
    // count is their number, of which the largest power of two is used.
    DebugTrace(TraceRecord* records, size_t count);

    // Notes an event with nowUs() as its time. Any interrupt at any priority and any task may
    // call it. It never waits.
    // label is a string literal that stays valid for good. a and b are any two numbers.
    // Returns false when the trace is full. The event is then lost and counted in lost().
    bool note(const char* label, int32_t a, int32_t b);

    // Takes the oldest whole note into out. One task at a time, never an interrupt. Once every
    // waiting note is taken and notes were lost since the last loss notice, out becomes a loss
    // notice: no label, and the count of lost notes in a.
    // Returns false when no whole note waits and no loss is left to report. A note that an
    // interrupted writer has not finished waits for the next call, with every note behind it.
    bool take(TraceRecord& out);

    // Gets how many notes found the trace full.
    uint32_t lost() const { return lostCount.load(std::memory_order_relaxed); }

    DebugTrace(const DebugTrace&) = delete;
    DebugTrace& operator=(const DebugTrace&) = delete;

private:
    TraceRecord* const records;
    const uint32_t mask;                 // the record count minus one, 0 for a trace of one record
    const bool usable;                   // the trace has at least one record
    std::atomic<uint32_t> reserved{0};   // the places handed to writers so far
    std::atomic<uint32_t> taken{0};      // the notes the reader has taken so far
    std::atomic<uint32_t> lostCount{0};
    uint32_t reportedLost = 0;   // the lost notes the loss notices have stated, moved by the reader
};

// Formats a note as a debug line, such as "73021 D trace ir late 3 -12 at 73020512 us\n", or a
// loss notice as "73021 D trace 37 notes lost\n". Gets its length.
// out takes LOG_LINE_BYTES bytes. ms is the time in front of the line, such as millis().
size_t formatTraceLine(char* out, uint32_t ms, const TraceRecord& record);

// Notes an event in the firmware's trace. Does nothing in a release build.
void debugTrace(const char* label, int32_t a, int32_t b);

// Takes the oldest whole note of the firmware's trace into out. One task at a time, never an
// interrupt. Returns false when none waits. Returns false always in a release build.
bool takeDebugTrace(TraceRecord& out);

#if defined(DEBUG)
#define DEBUG_TRACE(label, a, b) debugTrace(label, a, b)
#else
// The call stands behind if (false), so its arguments stay checked and count as used. The
// compiler removes it.
#define DEBUG_TRACE(label, a, b)                                                                   \
    do {                                                                                           \
        if (false) {                                                                               \
            debugTrace(label, a, b);                                                               \
        }                                                                                          \
    } while (0)
#endif
