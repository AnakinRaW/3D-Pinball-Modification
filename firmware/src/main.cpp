// The firmware's start: setup() creates the main task and starts FreeRTOS. The main task will run
// the start order of docs/firmware/general-design.md and then the main loop. Until the drivers
// exist, it brings up LVGL and FastLED and blinks the Teensy's LED once a second.

// Keeps Arduino's macros, which FastLED's Teensy 4 code and WDT_T4 use.
#define USE_ARDUINO_DEFINES
#include "arduino_freertos.h"
#include <lvgl.h>
#include <FastLED.h>

constexpr uint32_t MAIN_STACK_BYTES = 8192;
constexpr uint32_t BLINK_MS = 500;   // half the LED's blink period

static TaskHandle_t mainTask = nullptr;

static void mainMain(void*) {
    lv_init();
    pinMode(LED_BUILTIN, OUTPUT);
    for (;;) {
        digitalToggleFast(LED_BUILTIN);
        vTaskDelay(pdMS_TO_TICKS(BLINK_MS));
    }
}

void setup() {
    xTaskCreate(mainMain, "main", MAIN_STACK_BYTES / sizeof(StackType_t), nullptr, 1, &mainTask);
    vTaskStartScheduler();
}

void loop() {}
