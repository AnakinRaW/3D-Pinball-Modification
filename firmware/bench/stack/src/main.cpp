// Stack test for the Teensy 4.1 with its PSRAM and the card in its socket.
//
// Builds FreeRTOS from freertos-teensy 11.2.0-4 with its pull request 43 and the four settings of
// firmware/gist/freertos-config.h, LVGL 9.6 with firmware/gist/lv_conf.h, FastLED and SdFat into
// one program, and checks on the bench:
// 1. the storage task sleeps while SdFat's DMA reads a frame, so the main task keeps running
// 2. how long a frame read whole takes under FreeRTOS
// 3. how long LVGL takes to draw a full-screen picture from the PSRAM into its buffer in RAM2
// 4. FastLED sends a frame to one chain by DMA while the tasks run
//
// The card is formatted FAT32 with 32 KB clusters. The test writes a file of 60 frames and deletes
// it at the end. It starts once the serial monitor is open.

#define USE_ARDUINO_DEFINES   // keeps Arduino's macros, which FastLED's Teensy 4 code uses
#include "arduino_freertos.h"
#include <SdFat.h>
#include <lvgl.h>
#include <FastLED.h>

extern "C" uint8_t external_psram_size;

constexpr uint32_t   kFrame   = 320 * 240 * 2;   // one full-screen frame in RGB565
constexpr uint32_t   kFrames  = 60;              // frames in the test file
constexpr uint32_t   kWrite   = 16 * 1024;       // the piece the file is written in
constexpr uint32_t   kPhaseMs = 3000;            // each phase of the main task's count
constexpr UBaseType_t kCard   = 1;               // the storage task's notification of the SD interrupt
const char* const    kName    = "stack.bin";

EXTMEM static uint8_t picture[kFrame] __attribute__((aligned(32)));    // the frame the card fills
DMAMEM static uint8_t lvBuffers[2][kFrame] __attribute__((aligned(32)));
DMAMEM static uint8_t writeBuf[kWrite] __attribute__((aligned(32)));
static CRGB leds[8];

static SdFs   sd;
static FsFile file;
static TaskHandle_t mainTask    = nullptr;
static TaskHandle_t storageTask = nullptr;     // set once the DMA mount is done, so yield() sleeps from then on
static void       (*sdfatIsr)() = nullptr;

static volatile bool     reading     = false;  // the main task asks the storage task to read frames
static volatile bool     storageOk   = false;
static volatile uint32_t framesRead  = 0;
static volatile uint32_t frameUsSum  = 0;
static volatile uint32_t frameUsMax  = 0;

static bool inInterrupt() {
    uint32_t ipsr;
    asm volatile("mrs %0, ipsr" : "=r"(ipsr));
    return ipsr != 0;
}

// SdFat calls yield() while it waits for the card. The storage task sleeps until the SD
// controller's interrupt or the next tick, as in firmware/gist/drivers/storage.cpp; every other
// caller gets freertos-teensy's default_yield()
void yield() {
    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING || inInterrupt()) return;
    if (storageTask == nullptr || xTaskGetCurrentTaskHandle() != storageTask) {
        freertos::default_yield();
        return;
    }
    ulTaskNotifyTakeIndexed(kCard, pdTRUE, 1);
}

// the SD controller's interrupt: SdFat's handler ends the transfer, and the storage task wakes
static void sdInterrupt() {
    sdfatIsr();
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveIndexedFromISR(storageTask, kCard, &woken);
    portYIELD_FROM_ISR(woken);
}

static bool writeFile() {
    if (!file.open(kName, O_RDWR | O_CREAT | O_TRUNC)) return false;
    file.preAllocate(kFrames * kFrame);
    for (uint32_t i = 0; i < kWrite; i++) writeBuf[i] = (uint8_t)(i * 7);
    for (uint32_t off = 0; off < kFrames * kFrame; off += kWrite)
        if (file.write(writeBuf, kWrite) != kWrite) { file.close(); return false; }
    file.close();
    return true;
}

static void storageMain(void*) {
    bool ok = sd.begin(SdioConfig(FIFO_SDIO)) && sd.bytesPerCluster() >= 32768 && writeFile();
    sd.end();
    ok = ok && sd.begin(SdioConfig(DMA_SDIO));
    sdfatIsr = _VectorsRam[IRQ_SDHC1 + 16];
    attachInterruptVector(IRQ_SDHC1, sdInterrupt);
    NVIC_SET_PRIORITY(IRQ_SDHC1, 240);
    storageTask = xTaskGetCurrentTaskHandle();
    ok = ok && file.open(kName, O_RDONLY);
    storageOk = ok;
    xTaskNotifyGive(mainTask);

    uint32_t f = 0;
    for (;;) {
        if (!reading || !ok) { vTaskDelay(1); continue; }
        file.seekSet(f * kFrame);
        arm_dcache_flush_delete(picture, kFrame);
        uint32_t t = micros();
        int n = file.read(picture, kFrame);
        t = micros() - t;
        arm_dcache_flush_delete(picture, kFrame);
        if (n != (int)kFrame) { ok = false; continue; }
        framesRead = framesRead + 1;
        frameUsSum = frameUsSum + t;
        if (t > frameUsMax) frameUsMax = t;
        f = (f + 1) % kFrames;
    }
}

static void flush(lv_display_t* d, const lv_area_t*, uint8_t*) { lv_display_flush_ready(d); }

// counts how often a tight loop runs in kPhaseMs
static uint32_t countLoop() {
    volatile uint32_t n = 0;
    const uint32_t t0 = millis();
    while (millis() - t0 < kPhaseMs) n = n + 1;
    return n;
}

static void drawTimes(lv_display_t* d, lv_obj_t* img, float& mean, float& max) {
    uint32_t sum = 0, top = 0;
    for (int i = 0; i < 30; i++) {
        lv_obj_invalidate(img);
        uint32_t t = micros();
        lv_refr_now(d);
        t = micros() - t;
        sum += t;
        if (t > top) top = t;
    }
    mean = sum / 30 / 1000.0f;
    max  = top / 1000.0f;
}

static void mainMain(void*) {
    while (!Serial) vTaskDelay(10);
    Serial.printf("Stack test, PSRAM %d MB, FreeRTOS %s, LVGL %d.%d.%d, FastLED %d\n", external_psram_size,
                  tskKERNEL_VERSION_NUMBER, LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH,
                  FASTLED_VERSION);
    Serial.println("writing the test file ...");
    xTaskCreate(storageMain, "storage", 8192 / sizeof(StackType_t), nullptr, 2, nullptr);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (!storageOk) { Serial.println("card, its 32 KB clusters or the file failed, stopped"); vTaskSuspend(nullptr); }

    // 1. and 2. the main task's loop with the storage task idle, then while it reads frames
    const uint32_t idle = countLoop();
    reading = true;
    const uint32_t busy = countLoop();
    reading = false;
    vTaskDelay(50);
    Serial.printf("\n1. main loop runs, storage idle %lu, storage reading %lu: %.1f %% of the processor left\n",
                  idle, busy, 100.0f * busy / idle);
    Serial.printf("2. frames read whole under FreeRTOS: %lu, mean %.2f ms, max %.2f ms\n",
                  framesRead, framesRead ? frameUsSum / framesRead / 1000.0f : 0.0f, frameUsMax / 1000.0f);

    // 3. LVGL draws the frame in the PSRAM as a full-screen picture
    lv_init();
    lv_tick_set_cb([]() -> uint32_t { return xTaskGetTickCount(); });
    lv_display_t* d = lv_display_create(320, 240);
    lv_display_set_buffers(d, lvBuffers[0], lvBuffers[1], kFrame, LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(d, flush);
    static lv_image_dsc_t dsc = {};
    dsc.header = {LV_IMAGE_HEADER_MAGIC, LV_COLOR_FORMAT_RGB565_SWAPPED, 0, 320, 240, 640, 0};
    dsc.data_size = kFrame;
    dsc.data = picture;
    lv_obj_t* img = lv_image_create(lv_screen_active());
    lv_image_set_src(img, &dsc);
    float mean, max;
    drawTimes(d, img, mean, max);
    Serial.printf("3. LVGL draws the PSRAM picture, storage idle: mean %.2f ms, max %.2f ms\n", mean, max);
    reading = true;
    drawTimes(d, img, mean, max);
    reading = false;
    Serial.printf("   the same while the storage task reads frames: mean %.2f ms, max %.2f ms\n", mean, max);

    // 4. FastLED's ObjectFLED driver on one chain
    FastLED.setExclusiveDriver<fl::Bus::FLEX_IO, 0>();
    FastLED.addLeds<WS2812B, 2, GRB>(leds, 8);
    uint32_t t = micros();
    FastLED.show();
    Serial.printf("4. FastLED show() of 8 LEDs returned after %lu us\n", micros() - t);

    vTaskDelay(50);
    file.close();
    sd.remove(kName);
    Serial.println("\ndone, test file deleted");
    vTaskSuspend(nullptr);
}

void setup() {
    Serial.begin(115200);
    xTaskCreate(mainMain, "main", 8192 / sizeof(StackType_t), nullptr, 1, &mainTask);
    vTaskStartScheduler();
}

void loop() {}
