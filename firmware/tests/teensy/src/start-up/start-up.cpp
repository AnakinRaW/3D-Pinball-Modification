// Tests on the Teensy whether the start-up leaves the firmware in the state the main program
// expects.
//
//   heap  a block from malloc() before main() and one in the main task both lie in RAM2, apart
//
// Wiring: none, the Teensy alone. The image runs its tests in a FreeRTOS task, as the firmware
// runs its main loop, and freertos-teensy brings its own sbrk() for malloc().

#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include "test-harness.h"

constexpr uint32_t BLOCK_BYTES = 1024;
// pjrc.com/store/teensy41.html, Memory: RAM2, the OCRAM that malloc() takes its memory from
constexpr uintptr_t RAM2_START = 0x20200000;
constexpr uintptr_t RAM2_BYTES = 512 * 1024;
constexpr uint32_t LIMIT_S = 10;

// Takes its block in its constructor, which the Teensy core runs before main().
struct EarlyBlock {
    EarlyBlock() : block(malloc(BLOCK_BYTES)) {}
    void* block;
};

static EarlyBlock early;

static bool inRam2(const void* block) {
    const uintptr_t at = reinterpret_cast<uintptr_t>(block);
    return at >= RAM2_START && at + BLOCK_BYTES <= RAM2_START + RAM2_BYTES;
}

static void failAt(const char* which, const void* block) {
    char seen[48];
    snprintf(seen, sizeof seen, "%s at 0x%08X", which, reinterpret_cast<uintptr_t>(block));
    test::fail(seen, "a block inside RAM2");
}

static void testHeap() {
    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        test::fail("no FreeRTOS task", "the test in the main task");
        return;
    }
    void* const late = malloc(BLOCK_BYTES);
    const uintptr_t a = reinterpret_cast<uintptr_t>(early.block);
    const uintptr_t b = reinterpret_cast<uintptr_t>(late);
    if (early.block == nullptr || !inRam2(early.block)) {
        failAt("the block before main()", early.block);
    } else if (late == nullptr || !inRam2(late)) {
        failAt("the block in the main task", late);
    } else if (a < b + BLOCK_BYTES && b < a + BLOCK_BYTES) {
        char seen[64];
        snprintf(seen, sizeof seen, "blocks at 0x%08X and 0x%08X", a, b);
        test::fail(seen, "two blocks apart");
    } else {
        Serial.printf("blocks at 0x%08X and 0x%08X\n", a, b);
        test::pass();
    }
    free(late);
}

static const Test TESTS[] = {
    {"start-up.heap", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testHeap, nullptr},
};

extern const Program START_UP = {TESTS, sizeof TESTS / sizeof TESTS[0]};
