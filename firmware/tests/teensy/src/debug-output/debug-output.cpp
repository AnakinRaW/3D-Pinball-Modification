// The debug-output program of docs/firmware/testing.md. It tests on the Teensy whether a debug
// output sends its lines without waiting, drops the lines of an interrupt and brings its lines to
// the runner whole. The first two tests fill a separate output, so the firmware's debug lines do
// not mix into what they count.
//
//   send-time       a burst of lines leaves through send(), no call of which takes as long as a
//                   wait for USB would
//   interrupt-line  a line added in an interrupt is dropped and counted
//   round-trip      numbered lines of DEBUG_LOG() reach the runner whole and in order
//
// Wiring: none, the Teensy on USB with the runner reading.

#include <Arduino.h>
#include <IntervalTimer.h>
#include "logging/debug-log.h"
#include "logging/debug-output.h"
#include "test-harness.h"

constexpr uint32_t LIMIT_S = 15;
// the firmware's debug buffer, DEBUG_BUFFER_BYTES in firmware/src/logging/debug-log.cpp
constexpr size_t BUFFER_BYTES = 16384;
// A burst of 300 short lines, about 9 KB, fits into the 16 KB buffer whole.
constexpr uint32_t BURST_LINES = 300;
// A wait for USB lasts at least one millisecond tick of the Teensy core's timeout, so a call
// shorter than that has not waited.
constexpr uint32_t WAIT_US = 1000;
constexpr uint32_t DRAIN_MS = 5000;   // the longest the runner takes to read the burst
constexpr uint32_t IDLE_MS = 3;       // ms without a sent byte that end the drain
constexpr uint32_t INTERRUPT_LINES = 100;
constexpr uint32_t INTERRUPT_PERIOD_US = 1000;
constexpr uint32_t ROUND_TRIP_LINES = 200;
// Knuth's multiplicative hash, so no two lines carry the same payload
constexpr uint32_t PAYLOAD_FACTOR = 2654435761u;
constexpr uint32_t CHECK_TIMEOUT_MS = 10000;   // the runner's answer after the last line
constexpr char INTERRUPT_LINE[] = "from an interrupt\n";

DMAMEM static uint8_t buffer[BUFFER_BYTES];
static DebugOutput output(buffer, sizeof buffer);
static volatile uint32_t interruptLines = 0;
static IntervalTimer timer;

static uint32_t cyclesToUs(uint32_t cycles) {
    return cycles / (F_CPU_ACTUAL / 1000000);
}

// Adds one line to the test's output, as debugLog() adds one to the firmware's.
static void writeLine(const char* format, ...) __attribute__((format(printf, 1, 2)));

static void writeLine(const char* format, ...) {
    va_list args;
    va_start(args, format);
    output.write(format, args);
    va_end(args);
}

static void testSendTime() {
    const uint32_t lostBefore = output.lost();
    for (uint32_t i = 0; i < BURST_LINES; ++i) {
        writeLine("debug-output.send-time line %lu", i);
    }
    if (output.lost() != lostBefore) {
        test::fail("lines dropped", "a burst that fits the buffer kept whole");
        return;
    }
    uint32_t worstUs = 0;
    uint32_t calls = 0;
    uint32_t idleMs = 0;
    const uint32_t startMs = millis();
    while (idleMs < IDLE_MS && millis() - startMs < DRAIN_MS) {
        const uint32_t t0 = ARM_DWT_CYCCNT;
        const size_t sent = output.send();
        const uint32_t us = cyclesToUs(ARM_DWT_CYCCNT - t0);
        worstUs = us > worstUs ? us : worstUs;
        calls = calls + 1;
        idleMs = sent == 0 ? idleMs + 1 : 0;
        delay(1);
    }
    Serial.printf("%lu calls of send(), the longest %lu us\n", calls, worstUs);
    if (worstUs >= WAIT_US) {
        char seen[32];
        snprintf(seen, sizeof seen, "a call of %lu us", worstUs);
        test::fail(seen, "every call under 1000 us");
        return;
    }
    test::pass();
}

static void lineFromInterrupt() {
    if (interruptLines < INTERRUPT_LINES) {
        output.writeLine(INTERRUPT_LINE, sizeof INTERRUPT_LINE - 1);
        interruptLines = interruptLines + 1;
    }
}

static void testInterrupt() {
    const uint32_t lostBefore = output.lost();
    interruptLines = 0;
    if (!timer.begin(lineFromInterrupt, INTERRUPT_PERIOD_US)) {
        test::fail("no IntervalTimer free", "a timer for the test");
        return;
    }
    while (interruptLines < INTERRUPT_LINES) {
        delay(1);
    }
    timer.end();
    const uint32_t dropped = output.lost() - lostBefore;
    if (dropped != INTERRUPT_LINES) {
        char seen[32];
        snprintf(seen, sizeof seen, "%lu dropped", dropped);
        test::fail(seen, "100 dropped");
        return;
    }
    test::pass();
}

static void testRoundTrip() {
    for (uint32_t i = 0; i < ROUND_TRIP_LINES; ++i) {
        DEBUG_LOG("rt %lu %08lx", i, i * PAYLOAD_FACTOR);
    }
    uint32_t good = 0;
    uint32_t total = 0;
    uint32_t lost = 0;
    if (!test::check("rt", ROUND_TRIP_LINES, good, total, lost, CHECK_TIMEOUT_MS)) {
        test::fail("no answer from the runner", "the runner's count");
        return;
    }
    if (good != ROUND_TRIP_LINES || total != ROUND_TRIP_LINES || lost != 0) {
        char seen[48];
        snprintf(seen, sizeof seen, "%lu of %lu lines right", good, total);
        test::fail(seen, "200 of 200");
        return;
    }
    test::pass();
}

static const Test TESTS[] = {
    {"debug-output.send-time", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testSendTime, nullptr},
    {"debug-output.interrupt-line", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testInterrupt, nullptr},
    {"debug-output.round-trip", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testRoundTrip, nullptr},
};

extern const Program DEBUG_OUTPUT = {TESTS, sizeof TESTS / sizeof TESTS[0]};
