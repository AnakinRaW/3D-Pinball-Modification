// The firmware's start: setup() starts the clock, creates the main task and starts FreeRTOS. The
// main task will run the start order of docs/firmware/general-design.md and then the main loop.
// Until the drivers exist, it brings up LVGL and FastLED and blinks the Teensy's LED once a second.

// Keeps Arduino's macros, which FastLED's Teensy 4 code and WDT_T4 use.
#define USE_ARDUINO_DEFINES
#include "arduino_freertos.h"
#include "core/clock.h"
#include <lvgl.h>
// FastLED 3.10.9's fl/gfx/colorutils.h turns warnings off twice and back on once, so without
// the push and pop -Wunused-parameter and -Wreturn-type stay off for all code after it.
#pragma GCC diagnostic push
#include <FastLED.h>
#pragma GCC diagnostic pop

constexpr uint32_t BLINK_MS = 500;   // half the LED's blink period
constexpr uint32_t US_PER_MS = 1000;

// PJRC's guard behind the static variables is 32 bytes, and an MPU region of 32 bytes has to start
// on a multiple of its size
constexpr uintptr_t GUARD_BYTES = 32;
// ARMv7-M Architecture Reference Manual, B3.5: region 15 is the last of the 16 and wins over every
// region PJRC's start-up sets, and SIZE 4 gives 2^(4+1) = 32 bytes
constexpr uint32_t GUARD_REGION = 15;
constexpr uint32_t GUARD_SIZE = 4;

// general-design.md, Interrupts: the pin interrupts' row. tempmon.c of the Teensy core sets the
// temperature panic to 0, which the start of FreeRTOS resets to 128 with every other interrupt.
constexpr uint8_t PIN_PRIORITY = 96;
constexpr uint8_t TEMPERATURE_PANIC_PRIORITY = 0;

// PJRC's linker script: the end of the static variables in RAM1, and the top of the stack
extern "C" uint8_t _ebss[];
extern "C" uint8_t _estack[];

static StaticTask_t mainTask;

// true until the main loop runs; the watchdog's feed from FreeRTOS's idle task will read it
static volatile bool startingUp = true;

// The port's default starts a 1 ms timer and a task for PJRC's MillisTimer and EventResponder,
// which the firmware does not use.
namespace freertos {
void setup_event_responder() {}
}   // namespace freertos

// Blocks the 32 bytes at `at`, as PJRC guards the end of the static variables, so an overflowing
// stack crashes at once instead of overwriting what lies below it.
static void guardStack(uintptr_t at) {
    SCB_MPU_RBAR = at | SCB_MPU_RBAR_REGION(GUARD_REGION) | SCB_MPU_RBAR_VALID;
    SCB_MPU_RASR = SCB_MPU_RASR_TEX(0) | SCB_MPU_RASR_AP(0) | SCB_MPU_RASR_XN |
                   SCB_MPU_RASR_SIZE(GUARD_SIZE) | SCB_MPU_RASR_ENABLE;
    asm volatile("dsb");
    asm volatile("isb");
}

// The main task: sets the machine up, then runs the main loop, which never waits.
static void runMainTask(void*) {
    NVIC_SET_PRIORITY(IRQ_TEMPERATURE_PANIC, TEMPERATURE_PANIC_PRIORITY);
    NVIC_SET_PRIORITY(IRQ_GPIO6789, PIN_PRIORITY);

    lv_init();
    pinMode(LED_BUILTIN, OUTPUT);

    startingUp = false;
    uint32_t toggledUs = nowUs();
    for (;;) {
        if (elapsedMs(toggledUs, BLINK_MS)) {
            toggledUs += BLINK_MS * US_PER_MS;
            digitalToggleFast(LED_BUILTIN);
        }
        yield();
    }
}

// The main task's stack is the rest of RAM1, between PJRC's guard behind the static variables and
// a guard below the interrupts' stack of configMAIN_STACK_DEPTH bytes at the top of RAM1.
void setup() {
    startClock();   // before anything takes a time
    const uintptr_t bottom = reinterpret_cast<uintptr_t>(_ebss) + GUARD_BYTES;
    const uintptr_t guard =
        reinterpret_cast<uintptr_t>(_estack) - configMAIN_STACK_DEPTH - GUARD_BYTES;
    guardStack(guard);
    xTaskCreateStatic(runMainTask, "main", (guard - bottom) / sizeof(StackType_t), nullptr, 1,
                      reinterpret_cast<StackType_t*>(bottom), &mainTask);
    vTaskStartScheduler();   // sets every interrupt to priority 128 and never returns
}

// never runs, the main task carries the main loop
void loop() {}
