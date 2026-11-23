// The card-upload program of docs/firmware/testing.md. It runs the card upload on the card in the
// Teensy's socket, with the test playing the computer through a link in memory.
//
//   card  a file of three chunks lands on the card whole, and no step of the upload takes long
//
// Wiring: none. The card sits in the Teensy's socket. The test leaves its file in
// /tests/card-upload/ on the card.

#include <Arduino.h>
#include <FreeRTOS.h>
#include <string.h>
#include <task.h>
#include "components/card-upload.h"
#include "components/file-system.h"
#include "core/clock.h"
#include "drivers/storage.h"
#include "test-harness.h"

constexpr uint32_t LIMIT_S = 30;
constexpr uint32_t UPLOAD_MS = 5000;     // far longer than three chunks take on the card
constexpr uint32_t STEP_MAX_US = 1000;   // a step of the upload, a checksum over a chunk included
constexpr size_t FILE_BYTES = 10000;     // three chunks, the last one short
constexpr size_t HEADER_BYTES = 64;      // the longest command line
constexpr size_t TEXT = 96;

constexpr char PATH[] = "/tests/card-upload/file.bin";
constexpr size_t SENT_BYTES = FILE_BYTES + 4 * HEADER_BYTES;   // the file with its commands

// The test's bytes lie in the PSRAM, so RAM1 and the heap in RAM2 keep their room.
EXTMEM static uint8_t sent[SENT_BYTES];
EXTMEM static uint8_t file[FILE_BYTES];
EXTMEM static uint8_t back[FILE_BYTES + 1];

/// The computer's side: the bytes it has sent and the answers it got.
class MemoryLink final : public UploadLink {
public:
    size_t read(uint8_t* to, size_t n) override {
        const size_t k = n < length - taken ? n : length - taken;
        memcpy(to, sent + taken, k);
        taken += k;
        return k;
    }

    void answer(const char* line, size_t n) override {
        snprintf(last, sizeof last, "%.*s", int(n > 0 ? n - 1 : 0), line);
        ++answers;
    }

    /// Adds bytes for the upload to take.
    void send(const void* bytes, size_t n) {
        memcpy(sent + length, bytes, n);
        length += n;
    }

    char last[TEXT] = {};   // the last answer without its line break
    uint32_t answers = 0;

private:
    size_t length = 0;
    size_t taken = 0;
};

static void noRestart() {}

static FileSystem files(storage);
static MemoryLink link;
DMAMEM static CardUpload upload(link, files, noRestart);
static char seen[TEXT];

// Gets the CRC-32 of a run of bytes as zlib.crc32() computes it.
static uint32_t crc(const uint8_t* data, size_t n) {
    uint32_t c = 0xFFFFFFFF;
    for (size_t i = 0; i < n; ++i) {
        c ^= data[i];
        for (int b = 0; b < 8; ++b) {
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
        }
    }
    return ~c;
}

static void testCard() {
    if (!storage.begin()) {
        test::skip("no card in the socket");
        return;
    }
    for (size_t i = 0; i < FILE_BYTES; ++i) {
        file[i] = uint8_t(i * 7 + (i >> 8));
    }
    char line[HEADER_BYTES];
    int n = snprintf(line, sizeof line, "CARD PUT %s %u\n", PATH, unsigned(FILE_BYTES));
    link.send(line, size_t(n));
    for (size_t at = 0; at < FILE_BYTES; at += CardUpload::CHUNK_BYTES) {
        const size_t k =
            FILE_BYTES - at < CardUpload::CHUNK_BYTES ? FILE_BYTES - at : CardUpload::CHUNK_BYTES;
        n = snprintf(line, sizeof line, "CARD DATA %u %08lx\n", unsigned(k), crc(file + at, k));
        link.send(line, size_t(n));
        link.send(file + at, k);
    }
    uint32_t longestUs = 0;
    const uint32_t startMs = millis();
    while (strncmp(link.last, "CARD DONE", 9) != 0 && strncmp(link.last, "CARD FAIL", 9) != 0 &&
           millis() - startMs < UPLOAD_MS) {
        const uint32_t before = nowUs();
        upload.update();
        const uint32_t took = nowUs() - before;
        longestUs = took > longestUs ? took : longestUs;
        vTaskDelay(1);
    }
    Serial.printf("the longest step took %lu us, the last answer: %s\n", longestUs, link.last);
    if (strcmp(link.last, "CARD DONE 10000") != 0) {
        test::fail(link.last, "CARD DONE 10000");
        return;
    }
    FileRequest r;
    storage.open(PATH).read(0, back, sizeof back, r);
    if (!r.wait() || r.bytes() != FILE_BYTES || memcmp(back, file, FILE_BYTES) != 0) {
        snprintf(seen, sizeof seen, "%u bytes read back, %s", unsigned(r.bytes()),
                 r.bytes() == FILE_BYTES ? "other bytes" : "another size");
        test::fail(seen, "the file sent");
        return;
    }
    if (longestUs > STEP_MAX_US) {
        snprintf(seen, sizeof seen, "a step of %lu us", longestUs);
        test::fail(seen, "every step within 1000 us");
        return;
    }
    test::pass();
}

static const Test TESTS[] = {
    {"card-upload.card", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testCard, nullptr},
};

extern const Program CARD_UPLOAD = {TESTS, sizeof TESTS / sizeof TESTS[0]};
