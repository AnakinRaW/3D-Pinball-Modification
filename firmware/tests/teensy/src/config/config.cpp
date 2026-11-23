// The config program of docs/firmware/testing.md. It runs the config component on the card in the
// Teensy's socket, with a config file it writes there itself.
//
//   card  a config reads the file, sets two values and saves. Read again from the saved file, it
//         gets every value it had, the unknown entry included
//   heap  begin() gives back every byte libyaml took from the heap
//
// Wiring: none. The card sits in the Teensy's socket. The tests leave their file in /tests/config/
// on the card.

#include <Arduino.h>
#include <FreeRTOS.h>
#include <malloc.h>
#include <math.h>
#include <string.h>
#include <task.h>
#include "components/config.h"
#include "components/file-system.h"
#include "drivers/storage.h"
#include "test-harness.h"

constexpr uint32_t LIMIT_S = 20;
constexpr uint32_t SAVE_MS = 2000;   // far longer than a write of a few hundred bytes takes
constexpr size_t TEXT = 96;

constexpr char PATH[] = "/tests/config/config.yml";
constexpr char WRITTEN[] = "servo:\n"
                           "  start_deg: 5\n"
                           "  speed_deg_s: 80\n"
                           "solenoid:\n"
                           "  pull_ms: [30, 25, 20]\n"
                           "unknown: text\n";
constexpr char SAVED[] = "servo:\n"
                         "  start_deg: 7\n"
                         "  speed_deg_s: 80\n"
                         "solenoid:\n"
                         "  pull_ms: [30, 25, 20, 15]\n"
                         "unknown: text\n";

static FileSystem files(storage);
// One config serves both reads, so the heap keeps room for libyaml.
DMAMEM static Config first(files, PATH);
static char seen[TEXT];
static char wanted[TEXT];

// Begins the driver once and skips the test without a card.
static bool cardReady() {
    if (storage.begin()) {
        return true;
    }
    test::skip("no card in the socket");
    return false;
}

// Writes the config file the tests start from.
static bool writeFile() {
    FileRequest r;
    storage.openWriter(PATH).rewrite(WRITTEN, strlen(WRITTEN), r);
    if (!r.wait()) {
        test::fail("writing the config file failed", "it succeeds");
        return false;
    }
    return true;
}

// Checks that a config holds a value under a key.
static bool holds(const Config& config, const char* key, long value) {
    const std::optional<double> v = config.get(key, 0, 1000);
    if (!v.has_value() || lround(*v) != value) {
        snprintf(seen, sizeof seen, "%s reads %ld", key, v.has_value() ? lround(*v) : -1L);
        snprintf(wanted, sizeof wanted, "%ld", value);
        test::fail(seen, wanted);
        return false;
    }
    return true;
}

static void testCard() {
    if (!cardReady() || !writeFile()) {
        return;
    }
    first.begin();
    if (!holds(first, "servo.start_deg", 5) || !holds(first, "solenoid.pull_ms.2", 20)) {
        return;
    }
    if (!first.set("servo.start_deg", 7) || !first.set("solenoid.pull_ms.3", 15) || !first.save()) {
        test::fail("set() or save() refused", "both taken");
        return;
    }
    const uint32_t startMs = millis();
    while (first.saveStatus() == FileStatus::PENDING && millis() - startMs < SAVE_MS) {
        vTaskDelay(1);
    }
    if (first.saveStatus() != FileStatus::SUCCEEDED) {
        test::fail("the save did not succeed", "it succeeds");
        return;
    }
    static char back[sizeof SAVED + 32];
    FileRequest r;
    storage.open(PATH).read(0, back, sizeof back - 1, r);
    if (!r.wait()) {
        test::fail("the read back failed", "it succeeds");
        return;
    }
    back[r.bytes()] = '\0';
    if (strcmp(back, SAVED) != 0) {
        Serial.printf("the file holds:\n%s", back);
        test::fail("another file", "the saved values with the unknown entry");
        return;
    }
    first.begin();
    if (!holds(first, "servo.start_deg", 7) || !holds(first, "servo.speed_deg_s", 80) ||
        !holds(first, "solenoid.pull_ms.3", 15)) {
        return;
    }
    if (first.count("solenoid.pull_ms") != 4) {
        snprintf(seen, sizeof seen, "%u entries", unsigned(first.count("solenoid.pull_ms")));
        test::fail(seen, "4 entries");
        return;
    }
    test::pass();
}

static void testHeap() {
    if (!cardReady() || !writeFile()) {
        return;
    }
    const struct mallinfo before = mallinfo();
    first.begin();
    const struct mallinfo after = mallinfo();
    Serial.printf("the heap's area grew by %d bytes, %d bytes taken before and %d after\n",
                  after.arena - before.arena, before.uordblks, after.uordblks);
    if (after.uordblks != before.uordblks) {
        snprintf(seen, sizeof seen, "%d bytes taken after, %d before", after.uordblks,
                 before.uordblks);
        test::fail(seen, "as many after as before");
        return;
    }
    if (!holds(first, "servo.speed_deg_s", 80)) {
        return;
    }
    test::pass();
}

static const Test TESTS[] = {
    {"config.card", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testCard, nullptr},
    {"config.heap", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testHeap, nullptr},
};

extern const Program CONFIG = {TESTS, sizeof TESTS / sizeof TESTS[0]};
