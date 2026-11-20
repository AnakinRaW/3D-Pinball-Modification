// The task-lock program of docs/firmware/testing.md. It tests on the Teensy whether the task lock
// keeps a task above the test task's priority out of a piece of code the test task is in.
//
//   exclusive  two tasks count one shared counter up by reading it, waiting a moment and
//              writing it back. Without the lock some counts get lost, which shows the test can
//              see the race. With the lock none get lost
//
// Wiring: none, the Teensy alone.

#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include "core/task-lock.h"
#include "test-harness.h"

constexpr uint32_t LIMIT_S = 20;
constexpr uint32_t RUN_MS = 2000;
constexpr UBaseType_t URGENT_PRIORITY = 2;   // above the test task's 1, as the storage task
constexpr uint32_t URGENT_STACK_BYTES = 2048;
constexpr uint32_t URGENT_COUNTS = 50;   // counts of the urgent task after each wake
// The pause between read and write, long enough that the urgent task's wake often falls into it.
constexpr uint32_t GAP_CYCLES = 200;

static volatile uint32_t counter = 0;
static volatile uint32_t urgentCounted = 0;
static volatile bool locking = false;
static volatile bool running = false;

// Counts the shared counter up once: read, wait, write back.
static void countOnce() {
    const uint32_t value = counter;
    const uint32_t start = ARM_DWT_CYCCNT;
    while (ARM_DWT_CYCCNT - start < GAP_CYCLES) {
    }
    counter = value + 1;
}

static void countUrgent(void*) {
    while (running) {
        for (uint32_t i = 0; i < URGENT_COUNTS; ++i) {
            if (locking) {
                const TaskLock lock;
                countOnce();
            } else {
                countOnce();
            }
            urgentCounted = urgentCounted + 1;
        }
        vTaskDelay(1);
    }
    vTaskDelete(nullptr);
}

// Runs both tasks for RUN_MS and gets how many counts got lost.
static int32_t lostCounts(bool withLock, bool& started) {
    counter = 0;
    urgentCounted = 0;
    locking = withLock;
    running = true;
    started = xTaskCreate(countUrgent, "urgent", URGENT_STACK_BYTES / sizeof(StackType_t), nullptr,
                          URGENT_PRIORITY, nullptr) == pdPASS;
    if (!started) {
        running = false;
        return 0;
    }
    uint32_t calmCounted = 0;
    const uint32_t startMs = millis();
    while (millis() - startMs < RUN_MS) {
        if (withLock) {
            const TaskLock lock;
            countOnce();
        } else {
            countOnce();
        }
        calmCounted = calmCounted + 1;
    }
    running = false;
    // Lets the urgent task see the end and finish its last round.
    vTaskDelay(10);
    return int32_t(calmCounted + urgentCounted) - int32_t(counter);
}

static void testExclusive() {
    bool started = false;
    const int32_t lostWithout = lostCounts(false, started);
    if (!started) {
        test::fail("no memory for the urgent task", "a task");
        return;
    }
    const int32_t lostWith = lostCounts(true, started);
    Serial.printf("without the lock %ld counts lost, with it %ld\n", lostWithout, lostWith);
    if (lostWithout <= 0) {
        test::fail("no count lost without the lock", "a race the test can see");
        return;
    }
    if (lostWith != 0) {
        char seen[32];
        snprintf(seen, sizeof seen, "%ld lost with the lock", lostWith);
        test::fail(seen, "none lost");
        return;
    }
    test::pass();
}

static const Test TESTS[] = {
    {"task-lock.exclusive", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testExclusive, nullptr},
};

extern const Program TASK_LOCK = {TESTS, sizeof TESTS / sizeof TESTS[0]};
