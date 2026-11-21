// The storage program of docs/firmware/testing.md. It tests the storage driver on the card in the
// Teensy's socket.
//
//   mount         begin() finds the card. The card is not reported failed
//   round-trip    a write, a read back, a shorter rewrite and a read again each bring what was
//                 written last. fileSize() follows
//   missing       a missing file reads as 0 bytes. A write creates it with its directory
//   order         a read started behind a write of the same place brings the new data
//   scope-end     a request that goes out of scope while pending waits for its end. Nothing lands
//                 in its buffer afterwards
//   interrupts    a request from an interrupt above FreeRTOS's limit fails at once. A request
//                 from the limit works. wait() refuses in an interrupt
//   loop-runs     while megabytes go to the card and back, the test task never waits as long as a
//                 driver tick
//   yield         yield() returns at once from a task and from an interrupt while a read runs
//   cached-reads  reads into RAM2 and the PSRAM, on cache lines and off them, bring the right bytes
//                 and leave the bytes around the buffer alone
//   stack         the storage task leaves a quarter of its stack untouched after the tests above
//   card-back     a pulled card fails every request and comes back once it is in again. The
//                 storage task still leaves a quarter of its stack untouched (bench)
//   no-card       a start without the card ends at once. Every request fails at once (bench)
//   readable      the computer's disk check finds no error on the card afterwards (bench)
//
// Wiring: none. The card sits in the Teensy's socket. The tests leave their files in
// /tests/storage/ on the card.

#include <Arduino.h>
#include <FreeRTOS.h>
#include <IntervalTimer.h>
#include <string.h>
#include <task.h>
#include "drivers/driver-base.h"
#include "drivers/storage.h"
#include "test-harness.h"

constexpr uint32_t LIMIT_S = 30;
constexpr uint32_t BENCH_LIMIT_S = 120;
constexpr uint32_t ASK_MS = 300000;
constexpr uint32_t PULL_MS = 60000;   // how long a person may take to pull or insert the card
constexpr uint32_t POLL_MS = 200;
constexpr size_t TEXT = 96;
constexpr size_t PATTERN_BYTES = 8192;
constexpr size_t CHUNK_BYTES = 65536;
constexpr uint32_t BIG_CHUNKS = 64;   // 4 MB
constexpr uint32_t AT_ONCE_US = 50;   // what a call that only starts or returns may take
constexpr uint32_t GUARD = 0xA5;
constexpr uint32_t SETTLE_MS = 50;          // longer than any write into a buffer could lag
constexpr uint8_t URGENT_PRIORITY = 128;    // above FreeRTOS's limit of 208
constexpr uint8_t ALLOWED_PRIORITY = 208;   // FreeRTOS's limit
constexpr uint32_t TIMER_US = 1000;
constexpr uint32_t NO_CARD_BEGIN_MS = 100;   // a start without a card, far below SdFat's 3 s search
constexpr uint32_t STACK_MARGIN_PART = 4;    // a quarter of the stack stays untouched

constexpr char ROUND[] = "/tests/storage/round.bin";
constexpr char NONE[] = "/tests/storage/none.bin";
constexpr char NESTED[] = "/tests/storage/made/new.bin";
constexpr char PATTERN[] = "/tests/storage/pattern.bin";
constexpr char BIG[] = "/tests/storage/big.bin";

static uint8_t pattern[PATTERN_BYTES];
EXTMEM static uint8_t chunk[CHUNK_BYTES] __attribute__((aligned(32)));
DMAMEM static uint8_t ram2[PATTERN_BYTES + 64] __attribute__((aligned(32)));
EXTMEM static uint8_t psram[PATTERN_BYTES + 64] __attribute__((aligned(32)));
static char seen[TEXT];
static char wanted[TEXT];

// Gets the byte the test files hold at position i of a file numbered seed.
static uint8_t byteAt(uint32_t i, uint32_t seed) {
    return uint8_t(i * 7 + (i >> 8) + seed * 13);
}

// Begins the driver once and skips the test without a card.
static bool cardReady() {
    if (storage.begin()) {
        return true;
    }
    test::skip("no card in the socket");
    return false;
}

// Waits for a request and fails the test where it did not succeed.
static bool done(FileRequest& r, const char* what) {
    if (!r.wait()) {
        snprintf(seen, sizeof seen, "%s failed", what);
        test::fail(seen, "it succeeds");
        return false;
    }
    return true;
}

// Writes the pattern file, which several tests read.
static bool writePattern() {
    for (uint32_t i = 0; i < PATTERN_BYTES; ++i) {
        pattern[i] = byteAt(i, 0);
    }
    FileRequest r;
    storage.openWriter(PATTERN).rewrite(pattern, PATTERN_BYTES, r);
    return done(r, "writing the pattern file");
}

static void testMount() {
    if (!cardReady()) {
        return;
    }
    const Fault f = storage.failed();
    if (f.parts != 0) {
        snprintf(seen, sizeof seen, "parts %u code %lu", unsigned(f.parts), f.code);
        test::fail(seen, "no failed part");
        return;
    }
    test::pass();
}

// Reads the round-trip file and compares it with n bytes of seed's pattern.
static bool readsBack(size_t n, uint32_t seed) {
    static uint8_t back[1024];
    FileRequest r;
    storage.open(ROUND).read(0, back, sizeof back, r);
    if (!done(r, "the read back")) {
        return false;
    }
    if (r.bytes() != n || r.fileSize() != n) {
        snprintf(seen, sizeof seen, "%u bytes, size %lu", unsigned(r.bytes()), r.fileSize());
        snprintf(wanted, sizeof wanted, "%u bytes and size", unsigned(n));
        test::fail(seen, wanted);
        return false;
    }
    for (size_t i = 0; i < n; ++i) {
        if (back[i] != byteAt(i, seed)) {
            snprintf(seen, sizeof seen, "byte %u is %u", unsigned(i), back[i]);
            test::fail(seen, "the bytes written last");
            return false;
        }
    }
    return true;
}

static void testRoundTrip() {
    if (!cardReady()) {
        return;
    }
    static uint8_t data[1000];
    for (uint32_t i = 0; i < sizeof data; ++i) {
        data[i] = byteAt(i, 1);
    }
    const FileWriter f = storage.openWriter(ROUND);
    FileRequest r;
    f.rewrite(data, sizeof data, r);
    if (!done(r, "the first rewrite") || !readsBack(1000, 1)) {
        return;
    }
    for (uint32_t i = 0; i < sizeof data; ++i) {
        data[i] = byteAt(i, 2);
    }
    f.rewrite(data, 300, r);
    if (!done(r, "the shorter rewrite") || !readsBack(300, 2)) {
        return;
    }
    f.write(300, data + 300, 10, r);
    if (!done(r, "the write at the end") || !readsBack(310, 2)) {
        return;
    }
    test::pass();
}

static void testMissing() {
    if (!cardReady()) {
        return;
    }
    uint8_t into[16];
    FileRequest r;
    storage.open(NONE).read(0, into, sizeof into, r);
    if (!done(r, "the read of a missing file")) {
        return;
    }
    if (r.bytes() != 0 || r.fileSize() != 0) {
        snprintf(seen, sizeof seen, "%u bytes, size %lu", unsigned(r.bytes()), r.fileSize());
        test::fail(seen, "0 bytes and size 0");
        return;
    }
    const uint8_t text[] = {'m', 'a', 'd', 'e'};
    storage.openWriter(NESTED).rewrite(text, sizeof text, r);
    if (!done(r, "the write into a new directory")) {
        return;
    }
    storage.open(NESTED).read(0, into, sizeof into, r);
    if (!done(r, "the read of the new file")) {
        return;
    }
    if (r.bytes() != sizeof text || memcmp(into, text, sizeof text) != 0) {
        test::fail("other bytes in the new file", "made");
        return;
    }
    test::pass();
}

static void testOrder() {
    if (!cardReady() || !writePattern()) {
        return;
    }
    static uint8_t fresh[512];
    static uint8_t back[512];
    for (uint32_t i = 0; i < sizeof fresh; ++i) {
        fresh[i] = byteAt(i, 3);
    }
    FileRequest write;
    FileRequest read;
    storage.openWriter(PATTERN).write(0, fresh, sizeof fresh, write);
    storage.open(PATTERN).read(0, back, sizeof back, read);
    if (!done(write, "the write") || !done(read, "the read")) {
        return;
    }
    if (memcmp(back, fresh, sizeof back) != 0) {
        test::fail("the old bytes", "the bytes of the write before");
        return;
    }
    test::pass();
}

static void testScopeEnd() {
    if (!cardReady() || !writePattern()) {
        return;
    }
    memset(ram2, GUARD, sizeof ram2);
    {
        FileRequest r;
        storage.open(PATTERN).read(0, ram2, PATTERN_BYTES, r);
        if (r.status() != FileStatus::PENDING) {
            test::fail("a request that ended at once", "a pending request");
            return;
        }
        // The destructor waits here.
    }
    // Every byte landed before the destructor returned, so marking the buffer now has to stick.
    memset(ram2, GUARD, PATTERN_BYTES);
    vTaskDelay(pdMS_TO_TICKS(SETTLE_MS));
    for (size_t i = 0; i < PATTERN_BYTES; ++i) {
        if (ram2[i] != GUARD) {
            snprintf(seen, sizeof seen, "byte %u changed after the destructor", unsigned(i));
            test::fail(seen, "no byte changes");
            return;
        }
    }
    test::pass();
}

// ---- interrupts

static IntervalTimer timer;
static FileRequest isrRequest;   // an interrupt's request lives as long as the firmware
static uint8_t isrBuffer[64];
static volatile bool fired = false;
static volatile FileStatus isrStatus = FileStatus::IDLE;
static volatile bool isrWaited = true;
static volatile uint32_t yieldUs = 0;

static void startFromInterrupt() {
    if (fired) {
        return;
    }
    fired = true;
    storage.open(PATTERN).read(0, isrBuffer, sizeof isrBuffer, isrRequest);
    isrStatus = isrRequest.status();
    isrWaited = isrRequest.wait();
}

// Runs startFromInterrupt() once from the timer's interrupt at priority.
static void fireAt(uint8_t priority) {
    fired = false;
    timer.priority(priority);
    timer.begin(startFromInterrupt, TIMER_US);
    while (!fired) {
        vTaskDelay(1);
    }
    timer.end();
}

static void testInterrupts() {
    if (!cardReady() || !writePattern()) {
        return;
    }
    fireAt(URGENT_PRIORITY);
    if (isrStatus != FileStatus::FAILED) {
        test::fail("a request from priority 128 that did not fail at once", "FAILED at once");
        return;
    }
    fireAt(ALLOWED_PRIORITY);
    if (isrStatus != FileStatus::PENDING || isrWaited) {
        test::fail("a request from priority 208 not pending, or wait() not refused",
                   "PENDING and wait() false");
        return;
    }
    if (!done(isrRequest, "the read from priority 208")) {
        return;
    }
    if (memcmp(isrBuffer, pattern, sizeof isrBuffer) != 0) {
        test::fail("other bytes", "the pattern file's first bytes");
        return;
    }
    test::pass();
}

// ---- the main loop's passes

// Waits for r like the main loop: it polls, calls yield() once a pass and notes the longest pass.
static bool poll(FileRequest& r, uint32_t& longestUs) {
    uint32_t lastUs = micros();
    while (!r.ended()) {
        yield();
        const uint32_t nowUs = micros();
        if (nowUs - lastUs > longestUs) {
            longestUs = nowUs - lastUs;
        }
        lastUs = nowUs;
    }
    return r.status() == FileStatus::SUCCEEDED;
}

static void testLoopRuns() {
    if (!cardReady()) {
        return;
    }
    const FileWriter f = storage.openWriter(BIG);
    uint32_t longestUs = 0;
    FileRequest r;
    for (uint32_t c = 0; c < BIG_CHUNKS; ++c) {
        for (uint32_t i = 0; i < CHUNK_BYTES; ++i) {
            chunk[i] = byteAt(i, c);
        }
        if (c == 0) {
            f.rewrite(chunk, CHUNK_BYTES, r);
        } else {
            f.write(c * CHUNK_BYTES, chunk, CHUNK_BYTES, r);
        }
        if (!poll(r, longestUs)) {
            snprintf(seen, sizeof seen, "the write of chunk %lu failed", c);
            test::fail(seen, "every write succeeds");
            return;
        }
    }
    for (uint32_t c = 0; c < BIG_CHUNKS; ++c) {
        f.read(c * CHUNK_BYTES, chunk, CHUNK_BYTES, r);
        if (!poll(r, longestUs) || r.bytes() != CHUNK_BYTES) {
            snprintf(seen, sizeof seen, "the read of chunk %lu failed", c);
            test::fail(seen, "every read succeeds");
            return;
        }
        for (uint32_t i = 0; i < CHUNK_BYTES; ++i) {
            if (chunk[i] != byteAt(i, c)) {
                snprintf(seen, sizeof seen, "chunk %lu byte %lu", c, i);
                test::fail(seen, "the bytes written");
                return;
            }
        }
    }
    Serial.printf("the longest pass took %lu us\n", longestUs);
    if (longestUs >= DriverTick::PERIOD_US) {
        snprintf(seen, sizeof seen, "a pass of %lu us", longestUs);
        snprintf(wanted, sizeof wanted, "below %lu us", DriverTick::PERIOD_US);
        test::fail(seen, wanted);
        return;
    }
    test::pass();
}

static void yieldFromInterrupt() {
    if (fired) {
        return;
    }
    fired = true;
    const uint32_t t0 = micros();
    yield();
    yieldUs = micros() - t0;
}

static void testYield() {
    if (!cardReady()) {
        return;
    }
    FileRequest r;
    storage.open(BIG).read(0, chunk, CHUNK_BYTES, r);
    uint32_t longestUs = 0;
    while (!r.ended()) {
        const uint32_t t0 = micros();
        yield();
        const uint32_t us = micros() - t0;
        longestUs = us > longestUs ? us : longestUs;
    }
    fired = false;
    timer.priority(ALLOWED_PRIORITY);
    timer.begin(yieldFromInterrupt, TIMER_US);
    while (!fired) {
        vTaskDelay(1);
    }
    timer.end();
    if (r.status() != FileStatus::SUCCEEDED || r.bytes() != CHUNK_BYTES) {
        test::fail("the read failed", "the read ends normally");
        return;
    }
    if (longestUs > AT_ONCE_US || yieldUs > AT_ONCE_US) {
        snprintf(seen, sizeof seen, "yield() took %lu us in the task, %lu us in an interrupt",
                 longestUs, yieldUs);
        snprintf(wanted, sizeof wanted, "%lu us at most", AT_ONCE_US);
        test::fail(seen, wanted);
        return;
    }
    test::pass();
}

// Reads n bytes from position at of the pattern file into area + offset and checks them and the
// bytes around them.
static bool readsInto(uint8_t* area, size_t offset, uint32_t at, size_t n, const char* where) {
    memset(area, GUARD, PATTERN_BYTES + 64);
    FileRequest r;
    storage.open(PATTERN).read(at, area + offset, n, r);
    if (!done(r, where)) {
        return false;
    }
    for (size_t i = 0; i < PATTERN_BYTES + 64; ++i) {
        const bool inside = i >= offset && i < offset + n;
        const uint8_t want = inside ? pattern[at + i - offset] : uint8_t(GUARD);
        if (area[i] != want) {
            snprintf(seen, sizeof seen, "%s: byte %u of the area is %u", where, unsigned(i),
                     area[i]);
            snprintf(wanted, sizeof wanted, "%u", want);
            test::fail(seen, wanted);
            return false;
        }
    }
    return true;
}

static void testCachedReads() {
    if (!cardReady() || !writePattern()) {
        return;
    }
    if (readsInto(ram2, 0, 0, 4096, "RAM2 on lines") &&
        readsInto(ram2, 3, 5, 4000, "RAM2 off lines") &&
        readsInto(psram, 0, 512, 4096, "PSRAM on lines") &&
        readsInto(psram, 7, 9, 5000, "PSRAM off lines")) {
        test::pass();
    }
}

// Checks that the storage task has left a quarter of its stack untouched and prints how much it
// took at most.
static bool stackHolds() {
    const uint32_t left = storage.stackLeftBytes();
    Serial.printf("the storage task took at most %lu of its %lu stack bytes\n",
                  Storage::STACK_BYTES - left, Storage::STACK_BYTES);
    if (left < Storage::STACK_BYTES / STACK_MARGIN_PART) {
        snprintf(seen, sizeof seen, "%lu bytes left", left);
        snprintf(wanted, sizeof wanted, "%lu bytes left at least",
                 Storage::STACK_BYTES / STACK_MARGIN_PART);
        test::fail(seen, wanted);
        return false;
    }
    return true;
}

static void testStack() {
    if (cardReady() && stackHolds()) {
        test::pass();
    }
}

// ---- the bench

// Reads the pattern file every POLL_MS until a read's outcome is as wanted, for up to PULL_MS.
// The read takes whole sectors, so it reaches the card past SdFat's cache of one sector.
static bool readsUntil(bool works) {
    const uint32_t t0 = millis();
    while (millis() - t0 < PULL_MS) {
        FileRequest r;
        storage.open(PATTERN).read(0, ram2, PATTERN_BYTES, r);
        if (r.wait() == works) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
    return false;
}

static void testCardBack() {
    if (!cardReady() || !writePattern()) {
        return;
    }
    test::step("pull the card out of the socket");
    if (!readsUntil(false)) {
        test::fail("the card still answers", "a read fails once the card is out");
        return;
    }
    // After the first card error the driver tries to start the card anew. The card counts as
    // failed once that try has failed.
    const uint32_t t0 = millis();
    while (storage.failed().parts != 1 && millis() - t0 < PULL_MS) {
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
    FileRequest r;
    uint8_t into[16];
    storage.open(PATTERN).read(0, into, sizeof into, r);
    if (r.status() != FileStatus::FAILED || storage.failed().parts != 1) {
        test::fail("a request that did not fail at once", "FAILED at once, the card failed");
        return;
    }
    test::step("put the card back into the socket");
    if (!readsUntil(true)) {
        test::fail("the card stays failed", "the card works again");
        return;
    }
    // The card errors went into the log, whose formatting is the deepest call of the task.
    if (stackHolds()) {
        test::pass();
    }
}

static void testNoCard() {
    if (!cardReady()) {
        return;
    }
    test::step("pull the card out of the socket");
    if (!readsUntil(false)) {
        test::fail("the card still answers", "a read fails once the card is out");
        return;
    }
    test::restart();
}

static void resumeNoCard() {
    const uint32_t t0 = millis();
    if (storage.begin()) {
        test::fail("begin() found a card", "begin() returns false");
        return;
    }
    const uint32_t beginMs = millis() - t0;
    FileRequest r;
    uint8_t into[16];
    storage.open(PATTERN).read(0, into, sizeof into, r);
    const Fault f = storage.failed();
    test::step("put the card back into the socket");
    if (r.status() != FileStatus::FAILED || f.parts != 1 || beginMs > NO_CARD_BEGIN_MS) {
        snprintf(seen, sizeof seen, "request %u, parts %u, begin() %lu ms", unsigned(r.status()),
                 unsigned(f.parts), beginMs);
        snprintf(wanted, sizeof wanted, "FAILED at once, the card failed, begin() within %lu ms",
                 NO_CARD_BEGIN_MS);
        test::fail(seen, wanted);
        return;
    }
    test::pass();
}

static void testReadable() {
    if (test::ask("put the card into the computer: does its disk check find no error?", ASK_MS) ==
        test::Answer::YES) {
        test::pass();
    } else {
        test::fail("an error, or no answer", "y");
    }
}

static const Test TESTS[] = {
    {"storage.mount", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testMount, nullptr},
    {"storage.round-trip", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testRoundTrip, nullptr},
    {"storage.missing", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testMissing, nullptr},
    {"storage.order", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testOrder, nullptr},
    {"storage.scope-end", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testScopeEnd, nullptr},
    {"storage.interrupts", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testInterrupts, nullptr},
    {"storage.loop-runs", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testLoopRuns, nullptr},
    {"storage.yield", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testYield, nullptr},
    {"storage.cached-reads", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testCachedReads, nullptr},
    {"storage.stack", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testStack, nullptr},
    {"storage.card-back", RunsOn::BENCH, BENCH_LIMIT_S, BENCH_LIMIT_S, testCardBack, nullptr},
    {"storage.no-card", RunsOn::BENCH, BENCH_LIMIT_S, BENCH_LIMIT_S, testNoCard, resumeNoCard},
    {"storage.readable", RunsOn::BENCH, ASK_MS / 1000 + 10, ASK_MS / 1000 + 10, testReadable,
     nullptr},
};

extern const Program STORAGE = {TESTS, sizeof TESTS / sizeof TESTS[0]};
