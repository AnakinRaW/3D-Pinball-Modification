// The file-channel program of docs/firmware/testing.md. It tests the file channel on the card in
// the Teensy's socket.
//
//   high-first  a channel of the highest priority gets its read ahead of the normal reads queued
//               before it, which had not started
//
// Wiring: none. The card sits in the Teensy's socket. The test leaves its file in
// /tests/file-channel/ on the card.

#include <Arduino.h>
#include "drivers/storage.h"
#include "io/file-channel.h"
#include "test-harness.h"

constexpr uint32_t LIMIT_S = 30;
constexpr size_t CHUNK_BYTES = 65536;
constexpr uint8_t QUEUED = 8;   // normal reads queued ahead of the channel
constexpr uint32_t WAIT_MS = 5000;
constexpr size_t TEXT = 96;

constexpr char FILE_PATH[] = "/tests/file-channel/queued.bin";

EXTMEM static uint8_t chunk[QUEUED][CHUNK_BYTES] __attribute__((aligned(32)));
EXTMEM static uint8_t ring[CHUNK_BYTES] __attribute__((aligned(32)));
static char seen[TEXT];

// Begins the storage driver once and skips the test without a card.
static bool cardReady() {
    if (storage.begin()) {
        return true;
    }
    test::skip("no card in the socket");
    return false;
}

// Gets how many of the requests have ended.
static uint8_t endedOf(FileRequest (&requests)[QUEUED]) {
    uint8_t n = 0;
    for (const FileRequest& r : requests) {
        n = uint8_t(n + (r.ended() ? 1 : 0));
    }
    return n;
}

static void testHighFirst() {
    if (!cardReady()) {
        return;
    }
    FileRequest write;
    storage.openWriter(FILE_PATH).rewrite(chunk, sizeof chunk, write);
    if (!write.wait()) {
        test::fail("the write of the file failed", "it succeeds");
        return;
    }
    const FileReader file = storage.open(FILE_PATH);
    FileRequest normal[QUEUED];
    for (uint8_t i = 0; i < QUEUED; ++i) {
        file.read(uint32_t(i * CHUNK_BYTES), chunk[i], CHUNK_BYTES, normal[i]);
    }
    FileChannel<1> channel(ring, CHUNK_BYTES, FilePriority::HIGHEST);
    const uint8_t before = endedOf(normal);
    channel.start(file, false);
    const uint32_t t0 = millis();
    while (channel.available() == 0 && millis() - t0 < WAIT_MS) {
    }
    const uint8_t after = endedOf(normal);
    for (FileRequest& r : normal) {
        r.wait();
    }
    Serial.printf("normal reads ended: %u before the channel started, %u when its read ended\n",
                  before, after);
    if (channel.available() == 0) {
        test::fail("the channel brought nothing", "its read ends");
        return;
    }
    // The storage task serves one request at a time, so only the read under way when the channel
    // started may end before the channel's read.
    if (after > before + 1) {
        snprintf(seen, sizeof seen, "%u normal reads ended ahead of it", after - before);
        test::fail(seen, "at most the one under way");
        return;
    }
    test::pass();
}

static const Test TESTS[] = {
    {"file-channel.high-first", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testHighFirst, nullptr},
};

extern const Program FILE_CHANNEL = {TESTS, sizeof TESTS / sizeof TESTS[0]};
