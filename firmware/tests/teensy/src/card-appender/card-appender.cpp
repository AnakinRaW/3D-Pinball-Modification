// The card-appender program of docs/firmware/testing.md. It tests on the Teensy whether the card
// appender keeps the records of a write that failed on the card and writes them again later, with
// the storage driver and the card in the socket.
//
//   card-retry  the appender believes its file longer than the card holds it, so its first write
//               starts past the end and fails. The test then lengthens the file. The record waits
//               a second and reaches the card at the place the failed write would have taken
//
// Wiring: none. The card sits in the Teensy's socket. The test leaves its file in
// /tests/card-appender/ on the card.

#include <Arduino.h>
#include <FreeRTOS.h>
#include <string.h>
#include <task.h>
#include "drivers/storage.h"
#include "io/card-appender.h"
#include "test-harness.h"

constexpr uint32_t LIMIT_S = 20;
constexpr char FILE_PATH[] = "/tests/card-appender/retry.txt";
constexpr size_t START_BYTES = 16;     // what the file holds before the test
constexpr size_t CLAIMED_BYTES = 32;   // what the appender believes the file holds
constexpr char RECORD[] = "record 1\n";
constexpr size_t RECORD_BYTES = sizeof RECORD - 1;
constexpr uint32_t FAIL_MS = 300;     // long enough for the first write to start and fail
constexpr uint32_t RETRY_MS = 2500;   // more than the second the appender waits after a failure
constexpr size_t READ_BYTES = 128;

static uint8_t buffer[1024];
static char back[READ_BYTES + 1];
static char seen[96];

// Runs the appender like the main loop for ms milliseconds.
static void runFor(CardAppender& appender, uint32_t ms) {
    const uint32_t t0 = millis();
    while (millis() - t0 < ms) {
        appender.update();
        vTaskDelay(1);
    }
}

// Reads the whole test file into back. Returns its size.
static uint32_t readBack() {
    FileRequest r;
    storage.open(FILE_PATH).read(0, back, READ_BYTES, r);
    r.wait();
    back[r.bytes()] = '\0';
    return r.fileSize();
}

static void testCardRetry() {
    if (!storage.begin()) {
        test::skip("no card in the socket");
        return;
    }
    const FileWriter writer = storage.openWriter(FILE_PATH);
    uint8_t start[START_BYTES];
    memset(start, 'a', sizeof start);
    FileRequest r;
    writer.rewrite(start, sizeof start, r);
    if (!r.wait()) {
        test::fail("the test file could not be written", "a file of 16 bytes");
        return;
    }
    static CardFile file(writer);
    CardAppender appender(buffer, sizeof buffer);
    appender.begin(file, CLAIMED_BYTES);
    appender.append(RECORD, RECORD_BYTES);
    appender.flush();
    runFor(appender, FAIL_MS);
    // The file now reaches the place the appender believes its end.
    uint8_t fill[CLAIMED_BYTES - START_BYTES];
    memset(fill, 'b', sizeof fill);
    writer.write(START_BYTES, fill, sizeof fill, r);
    r.wait();
    const uint32_t before = readBack();
    runFor(appender, RETRY_MS);
    const uint32_t after = readBack();
    const bool recordThere = after == CLAIMED_BYTES + RECORD_BYTES &&
                             memcmp(back + CLAIMED_BYTES, RECORD, RECORD_BYTES) == 0;
    if (before != CLAIMED_BYTES || !recordThere || appender.lost() != 0) {
        snprintf(seen, sizeof seen, "%lu bytes after the failure, %lu after the retry, %lu lost",
                 before, after, appender.lost());
        test::fail(seen, "32 bytes, then the record at byte 32, none lost");
        return;
    }
    test::pass();
}

static const Test TESTS[] = {
    {"card-appender.card-retry", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testCardRetry, nullptr},
};

extern const Program CARD_APPENDER = {TESTS, sizeof TESTS / sizeof TESTS[0]};
