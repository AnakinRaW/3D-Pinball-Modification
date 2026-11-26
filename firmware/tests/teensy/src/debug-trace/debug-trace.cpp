// The debug-trace program of docs/firmware/testing.md. It tests on the Teensy whether the debug
// trace keeps the notes of two interrupts whole and in order while one preempts the other.
//
//   from-interrupts  two interrupts of different priorities note numbered events while the test
//                    task takes them
//   round-trip       numbered notes of DEBUG_TRACE() from an interrupt reach the runner as debug
//                    lines, whole and in order
//   lost-notice      a burst larger than the trace keeps its first notes, and a notice states the
//                    rest as lost
//
// Wiring: none, the Teensy alone. GPT1 raises the more urgent interrupt and an IntervalTimer on
// the PIT the other, since the PIT's four channels share one interrupt and so one priority. Each
// interrupt numbers its notes in a. b carries the number again, so a note written in part
// shows.

#include <Arduino.h>
#include <IntervalTimer.h>
#include <new>
#include <string.h>
#include "core/clock.h"
#include "logging/debug-trace.h"
#include "test-harness.h"

constexpr uint32_t RUN_MS = 2000;
constexpr uint32_t LIMIT_S = 10;
// general-design.md, Interrupts: the driver tick's row and the DMA refills' row, so the urgent
// interrupt preempts the calm one in the middle of its note
constexpr uint8_t URGENT_PRIORITY = 96;
constexpr uint8_t CALM_PRIORITY = 128;
// Periods that share no factor, so the two interrupts meet at every phase over the run.
constexpr uint32_t URGENT_PERIOD_US = 10;
constexpr uint32_t CALM_PERIOD_US = 13;
constexpr size_t RECORDS = 1024;   // 10 ms of both interrupts' notes

// i.MX RT1060 reference manual, Rev. 3, General Purpose Timer: CLKSRC 1 is the peripheral clock,
// the 24 MHz crystal in the Teensy core's start-up, which a prescaler of 24 turns into
// microseconds. ENMOD clears the counter as GPT1 starts. Without FRR the counter restarts at the
// compare value, so OCR1 sets the period.
constexpr uint32_t GPT_CLKSRC_PERIPHERAL = 1;
constexpr uint32_t GPT_PRESCALE = 24;
constexpr uint32_t GPT_STATUS_ALL = 0x3F;   // GPT_SR: writing a 1 clears each of its six flags

constexpr uint32_t ROUND_TRIP_NOTES = 100;
// TRACE_RECORDS in firmware/src/logging/debug-trace.cpp, the notes the firmware's trace holds
constexpr uint32_t TRACE_NOTES = 256;
constexpr uint32_t BURST_NOTES = 300;   // more notes than the trace holds
constexpr uint32_t ROUND_TRIP_PERIOD_US = 1000;
constexpr uint32_t CHECK_TIMEOUT_MS = 10000;   // the runner's answer after the last line

constexpr const char* URGENT = "urgent";
constexpr const char* CALM = "calm";

EXTMEM static TraceRecord records[RECORDS];
// The start leaves EXTMEM uncleared, so every record gets its initial value first.
static DebugTrace trace = [] {
    for (TraceRecord& r : records) {
        new (&r) TraceRecord();
    }
    return DebugTrace(records, RECORDS);
}();
static volatile int32_t urgentNoted = 0;
static volatile int32_t calmNoted = 0;
static IntervalTimer pit;

static void urgentIsr() {
    GPT1_SR = GPT_SR_OF1;
    const int32_t n = urgentNoted;
    trace.note(URGENT, n, n);
    urgentNoted = n + 1;
    // The flag's clear has to reach GPT1 before the interrupt returns, or it fires twice.
    asm volatile("dsb");
}

static void calmIsr() {
    const int32_t n = calmNoted;
    trace.note(CALM, n, n);
    calmNoted = n + 1;
}

static void startGpt1() {
    CCM_CCGR1 |= CCM_CCGR1_GPT1_BUS(CCM_CCGR_ON) | CCM_CCGR1_GPT1_SERIAL(CCM_CCGR_ON);
    GPT1_CR = 0;
    GPT1_PR = GPT_PRESCALE - 1;
    GPT1_OCR1 = URGENT_PERIOD_US - 1;
    GPT1_SR = GPT_STATUS_ALL;
    GPT1_IR = GPT_IR_OF1IE;
    attachInterruptVector(IRQ_GPT1, urgentIsr);
    NVIC_SET_PRIORITY(IRQ_GPT1, URGENT_PRIORITY);
    NVIC_ENABLE_IRQ(IRQ_GPT1);
    GPT1_CR = GPT_CR_EN | GPT_CR_ENMOD | GPT_CR_CLKSRC(GPT_CLKSRC_PERIPHERAL);
}

static void stopGpt1() {
    GPT1_CR = 0;
    NVIC_DISABLE_IRQ(IRQ_GPT1);
}

// Takes every waiting note and checks it. Returns false once a check has failed.
static bool drain(int32_t& urgentNext, int32_t& calmNext) {
    TraceRecord note;
    while (trace.take(note)) {
        const bool urgent = note.label == URGENT;
        int32_t& next = urgent ? urgentNext : calmNext;
        if ((!urgent && note.label != CALM) || note.a != note.b || note.a != next) {
            char seen[64];
            snprintf(seen, sizeof seen, "%s note %ld/%ld", note.label ? note.label : "none", note.a,
                     note.b);
            test::fail(seen, "every note whole and in its interrupt's order");
            return false;
        }
        next = next + 1;
    }
    return true;
}

static void testFromInterrupts() {
    urgentNoted = 0;
    calmNoted = 0;
    int32_t urgentNext = 0;
    int32_t calmNext = 0;
    pit.priority(CALM_PRIORITY);
    if (!pit.begin(calmIsr, CALM_PERIOD_US)) {
        test::fail("no IntervalTimer free", "the PIT's interrupt running");
        return;
    }
    startGpt1();
    const uint32_t startUs = nowUs();
    bool whole = true;
    while (whole && !elapsedMs(startUs, RUN_MS)) {
        whole = drain(urgentNext, calmNext);
    }
    stopGpt1();
    pit.end();
    if (!whole || !drain(urgentNext, calmNext)) {
        return;
    }
    Serial.printf("%ld urgent and %ld calm notes taken, %lu lost\n", urgentNext, calmNext,
                  trace.lost());
    if (trace.lost() != 0 || urgentNext != urgentNoted || calmNext != calmNoted) {
        test::fail("notes missing", "every note taken, none lost");
        return;
    }
    test::pass();
}

static volatile int32_t roundTripNoted = 0;

static void noteRoundTrip() {
    const int32_t n = roundTripNoted;
    if (n < int32_t(ROUND_TRIP_NOTES)) {
        // The runner computes the second number the same way.
        DEBUG_TRACE("tr", n, n * 3 + 1);
        roundTripNoted = n + 1;
    }
}

static void testRoundTrip() {
    roundTripNoted = 0;
    pit.priority(CALM_PRIORITY);
    if (!pit.begin(noteRoundTrip, ROUND_TRIP_PERIOD_US)) {
        test::fail("no IntervalTimer free", "the PIT's interrupt running");
        return;
    }
    while (roundTripNoted < int32_t(ROUND_TRIP_NOTES)) {
        delay(1);
    }
    pit.end();
    uint32_t good = 0;
    uint32_t total = 0;
    uint32_t lost = 0;
    if (!test::check("tr", ROUND_TRIP_NOTES, good, total, lost, CHECK_TIMEOUT_MS)) {
        test::fail("no answer from the runner", "the runner's count");
        return;
    }
    if (good != ROUND_TRIP_NOTES || total != ROUND_TRIP_NOTES || lost != 0) {
        char seen[48];
        snprintf(seen, sizeof seen, "%lu of %lu notes right", good, total);
        test::fail(seen, "100 of 100");
        return;
    }
    test::pass();
}

static void testLostNotice() {
    // The test task sends no debug line during the burst, so the trace fills up.
    for (int32_t n = 0; n < int32_t(BURST_NOTES); ++n) {
        DEBUG_TRACE("tl", n, n * 3 + 1);
    }
    uint32_t good = 0;
    uint32_t total = 0;
    uint32_t lost = 0;
    if (!test::check("tl", BURST_NOTES, good, total, lost, CHECK_TIMEOUT_MS)) {
        test::fail("no answer from the runner", "the runner's count");
        return;
    }
    if (good != TRACE_NOTES || total != TRACE_NOTES || lost != BURST_NOTES - TRACE_NOTES) {
        char seen[64];
        snprintf(seen, sizeof seen, "%lu right of %lu, %lu lost", good, total, lost);
        test::fail(seen, "256 right of 256, 44 lost");
        return;
    }
    test::pass();
}

static const Test TESTS[] = {
    {"debug-trace.from-interrupts", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testFromInterrupts, nullptr},
    {"debug-trace.round-trip", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testRoundTrip, nullptr},
    {"debug-trace.lost-notice", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testLostNotice, nullptr},
};

extern const Program DEBUG_TRACE_PROGRAM = {TESTS, sizeof TESTS / sizeof TESTS[0]};
