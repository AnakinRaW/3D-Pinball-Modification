// Load test for the Teensy 4.1 with its PSRAM and the card in its socket, built like the stack test.
//
// Runs the firmware's timing-critical work side by side and measures:
// 1. how late the IR driver's interrupt (priority 64, every 600 us) and the driver tick
//    (priority 96, every 5 ms) come, without load and under load
// 2. how long the four audio streams' reads wait while video frames, LVGL and FastLED run, against
//    the 325 ms an audio ring reads ahead
// 3. how long saves and log writes take under FreeRTOS, from the PSRAM and from RAM2, and whether
//    every byte arrives
//
// The load is a video frame read whole into the PSRAM every 40 ms, LVGL drawing that frame full
// screen, FastLED sending 240 LEDs on one chain and a log line once a second. An audio request
// comes every 23.2 ms from an interrupt at priority 208, as the Audio library's update starts them.
// The storage task serves audio first, then the rest in order. The card is formatted FAT32 with
// 32 KB clusters. The test writes three files and deletes them. It starts once the serial
// monitor is open.

#define USE_ARDUINO_DEFINES   // keeps Arduino's macros, which FastLED's Teensy 4 code uses
#include "arduino_freertos.h"
#include <SdFat.h>
#include <lvgl.h>
#include <FastLED.h>

extern "C" uint8_t external_psram_size;

constexpr uint32_t kFrame      = 320 * 240 * 2;
constexpr uint32_t kFrames     = 60;
constexpr uint32_t kChunk      = 8 * 1024;        // an audio chunk
constexpr uint32_t kSave       = 4 * 1024;        // a save
constexpr uint32_t kLogLine    = 128;
constexpr uint32_t kIrUs       = 600;             // the IR phase
constexpr uint32_t kTickUs     = 5000;            // the driver tick
constexpr uint32_t kAudioUs    = 23220;           // four streams of 88.2 KB/s, 8 KB each
constexpr uint32_t kFrameMs    = 40;
constexpr uint32_t kAheadUs    = 278639;          // what an audio ring reads ahead, docs/parts/audio
constexpr uint32_t kLoadMs     = 20000;
constexpr uint32_t kSaves      = 50;
constexpr UBaseType_t kCard    = 1;               // the storage task's notification of the SD interrupt
constexpr UBaseType_t kWork    = 0;               // and of new work

EXTMEM static uint8_t picture[kFrame] __attribute__((aligned(32)));
EXTMEM static uint8_t savePsram[kSave] __attribute__((aligned(32)));
DMAMEM static uint8_t saveRam2[kSave] __attribute__((aligned(32)));
DMAMEM static uint8_t lvBuffers[2][kFrame] __attribute__((aligned(32)));
DMAMEM static uint8_t audioBuf[4][kChunk] __attribute__((aligned(32)));
DMAMEM static uint8_t checkBuf[kSave] __attribute__((aligned(32)));
DMAMEM static uint8_t fillBuf[16384] __attribute__((aligned(32)));
static CRGB leds[240];

static SdFs   sd;
static FsFile frames, saves, logFile;
static TaskHandle_t mainTask = nullptr, storageTask = nullptr;
static void (*sdfatIsr)() = nullptr;

static inline uint32_t us() { return micros(); }   // the cycle counter divided down would wrap every 7 s

// -- interrupt timing ---------------------------------------------------------
struct Lateness {
    volatile uint32_t period = 0, expected = 0, n = 0;
    volatile int32_t  lo = INT32_MAX, hi = INT32_MIN;
    void reset() { n = 0; lo = INT32_MAX; hi = INT32_MIN; }
    void hit() {
        const uint32_t now = ARM_DWT_CYCCNT;
        if (n == 0) expected = now; else expected = expected + period;
        const int32_t late = (int32_t)(now - expected);
        if (late < lo) lo = late;
        if (late > hi) hi = late;
        n = n + 1;
    }
    float jitterUs() const { return n > 1 ? (hi - lo) / (float)(F_CPU_ACTUAL / 1000000) : 0.0f; }
};
static Lateness irLate, tickLate;
static IntervalTimer irTimer;

FASTRUN static void irIsr() { irLate.hit(); }
FASTRUN static void tickIsr() { GPT1_SR = GPT_SR_OF1; tickLate.hit(); asm volatile("dsb"); }

// -- storage requests ---------------------------------------------------------
struct AudioReq { uint8_t stream; uint32_t at; uint32_t issued; };
static AudioReq audioQ[64];
static volatile uint32_t audioHead = 0, audioTail = 0;
static uint32_t audioPos[4] = {};
static volatile uint32_t audioN = 0, audioMax = 0, audioLate = 0, audioDropped = 0;
static volatile uint64_t audioSum = 0;

static volatile bool     frameWanted = false;
static volatile uint32_t frameIssued = 0, frameN = 0, frameMax = 0, frameOver = 0;
static volatile uint64_t frameSum = 0;
static volatile uint32_t frameIndex = 0;

enum WriteKind : uint8_t { kNone, kLog, kSavePsram, kSaveRam2 };
static volatile WriteKind writeWanted = kNone;
static volatile uint32_t  writeSeed = 0;
static volatile uint32_t  logN = 0, logMax = 0, savePN = 0, savePMax = 0, saveRN = 0, saveRMax = 0, saveBad = 0;
static volatile bool      audioOn = false;
static volatile int       storageStep = 0;   // how far the storage task's start got
static volatile uint32_t  clusterBytes = 0;

FASTRUN static void audioIsr() {
    GPT2_SR = GPT_SR_OF1;
    if (audioOn) {
        static uint8_t s = 0;
        const uint32_t next = (audioHead + 1) % 64;
        if (next == audioTail) audioDropped = audioDropped + 1;
        else {
            audioQ[audioHead] = {s, audioPos[s], us()};
            audioPos[s] = (audioPos[s] + kChunk) % (2 * 1024 * 1024);
            audioHead = next;
            s = (s + 1) % 4;
            BaseType_t woken = pdFALSE;
            vTaskNotifyGiveIndexedFromISR(storageTask, kWork, &woken);
            portYIELD_FROM_ISR(woken);
        }
    }
    asm volatile("dsb");
}

static void startGpt1(uint32_t periodUs, uint8_t prio) {
    CCM_CCGR1 |= CCM_CCGR1_GPT1_BUS(CCM_CCGR_ON) | CCM_CCGR1_GPT1_SERIAL(CCM_CCGR_ON);
    GPT1_CR = 0; GPT1_PR = 23; GPT1_OCR1 = periodUs - 1; GPT1_SR = 0x3F; GPT1_IR = GPT_IR_OF1IE;
    attachInterruptVector(IRQ_GPT1, tickIsr); NVIC_SET_PRIORITY(IRQ_GPT1, prio); NVIC_ENABLE_IRQ(IRQ_GPT1);
    GPT1_CR = GPT_CR_EN | GPT_CR_ENMOD | GPT_CR_CLKSRC(1);   // perclk, the 24 MHz crystal, divided by 24
}

static void startGpt2(uint32_t periodUs, uint8_t prio) {
    CCM_CCGR0 |= CCM_CCGR0_GPT2_BUS(CCM_CCGR_ON) | CCM_CCGR0_GPT2_SERIAL(CCM_CCGR_ON);
    GPT2_CR = 0; GPT2_PR = 23; GPT2_OCR1 = periodUs - 1; GPT2_SR = 0x3F; GPT2_IR = GPT_IR_OF1IE;
    attachInterruptVector(IRQ_GPT2, audioIsr); NVIC_SET_PRIORITY(IRQ_GPT2, prio); NVIC_ENABLE_IRQ(IRQ_GPT2);
    GPT2_CR = GPT_CR_EN | GPT_CR_ENMOD | GPT_CR_CLKSRC(1);
}

// -- the storage task, as in the stack test ----------------------------------
static bool inInterrupt() { uint32_t ipsr; asm volatile("mrs %0, ipsr" : "=r"(ipsr)); return ipsr != 0; }

// kSpinUs 0 sleeps on every wait, as the storage driver's yield() does. Above 0, a wait with no
// data transfer running, such as SdFat asking a busy card for its status between two commands,
// spins that long before it sleeps, since no interrupt ends such a wait and a sleep lasts a tick
#ifndef SPIN_US
#define SPIN_US 0
#endif
constexpr uint32_t kSpinUs   = SPIN_US;
constexpr uint32_t kDataBusy = 1u << 1;   // PRES_STATE CDIHB, a data transfer runs
static volatile uint32_t sleeps = 0, timeouts = 0, timeoutsInTransfer = 0;   // the storage task's sleeps in yield(), those that ran out the tick, and those of them during a transfer

void yield() {
    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING || inInterrupt()) return;
    if (storageTask == nullptr || xTaskGetCurrentTaskHandle() != storageTask) { freertos::default_yield(); return; }
    if (kSpinUs && !(USDHC1_PRES_STATE & kDataBusy)) {
        static uint32_t since = 0, last = 0;
        const uint32_t now = micros();
        if (now - last > 50) since = now;   // a new wait
        last = now;
        if (now - since < kSpinUs) return;
    }
    const uint32_t woke = ulTaskNotifyTakeIndexed(kCard, pdTRUE, 1);
    sleeps = sleeps + 1;
    if (woke == 0) { timeouts = timeouts + 1; if (USDHC1_PRES_STATE & kDataBusy) timeoutsInTransfer = timeoutsInTransfer + 1; }
}

static void sdInterrupt() {
    sdfatIsr();
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveIndexedFromISR(storageTask, kCard, &woken);
    portYIELD_FROM_ISR(woken);
}

static void pattern(uint8_t* p, uint32_t n, uint32_t seed) {
    for (uint32_t i = 0; i < n; i += 4) *(uint32_t*)(p + i) = (seed + i) * 2654435761u;
}

static void serveWrite(WriteKind kind, uint32_t seed) {
    const uint32_t t = us();
    if (kind == kLog) {
        char line[kLogLine];
        memset(line, 'a' + seed % 26, sizeof line);
        line[kLogLine - 1] = '\n';
        logFile.write(line, sizeof line);
        logFile.sync();
        const uint32_t d = us() - t;
        logN = logN + 1;
        if (d > logMax) logMax = d;
        return;
    }
    uint8_t* src = kind == kSavePsram ? savePsram : saveRam2;
    pattern(src, kSave, seed);
    arm_dcache_flush(src, kSave);          // SdFat's DMA reads the memory past the cache
    saves.seekSet(0);
    saves.write(src, kSave);
    saves.sync();
    const uint32_t d = us() - t;
    if (kind == kSavePsram) { savePN = savePN + 1; if (d > savePMax) savePMax = d; }
    else                    { saveRN = saveRN + 1; if (d > saveRMax) saveRMax = d; }
    saves.seekSet(0);
    arm_dcache_flush_delete(checkBuf, kSave);
    saves.read(checkBuf, kSave);
    arm_dcache_flush_delete(checkBuf, kSave);
    pattern(fillBuf, kSave, seed);
    if (memcmp(checkBuf, fillBuf, kSave) != 0) saveBad = saveBad + 1;
}

static void storageMain(void*) {
    bool ok = sd.begin(SdioConfig(FIFO_SDIO));
    clusterBytes = ok ? sd.bytesPerCluster() : 0;
    ok = ok && clusterBytes >= 32768;
    if (ok) storageStep = 1;
    if (ok && frames.open("load.bin", O_RDWR | O_CREAT | O_TRUNC)) {
        frames.preAllocate(kFrames * kFrame);
        for (uint32_t off = 0; ok && off < kFrames * kFrame; off += sizeof fillBuf)
            ok = frames.write(fillBuf, sizeof fillBuf) == sizeof fillBuf;
        frames.close();
        if (ok) storageStep = 2;
    } else ok = false;
    sd.end();
    ok = ok && sd.begin(SdioConfig(DMA_SDIO));
    if (ok) storageStep = 3;
    sdfatIsr = _VectorsRam[IRQ_SDHC1 + 16];
    attachInterruptVector(IRQ_SDHC1, sdInterrupt);
    NVIC_SET_PRIORITY(IRQ_SDHC1, 240);
    storageTask = xTaskGetCurrentTaskHandle();
    ok = ok && frames.open("load.bin", O_RDONLY) && saves.open("save.bin", O_RDWR | O_CREAT | O_TRUNC)
            && logFile.open("log.txt", O_RDWR | O_CREAT | O_TRUNC);
    if (ok) storageStep = 4;
    xTaskNotifyGive(mainTask);
    if (!ok) vTaskSuspend(nullptr);

    for (;;) {
        if (audioTail != audioHead) {                       // audio first
            const AudioReq r = audioQ[audioTail];
            audioTail = (audioTail + 1) % 64;
            uint8_t* dst = audioBuf[r.stream];
            arm_dcache_flush_delete(dst, kChunk);
            frames.seekSet(r.at);
            frames.read(dst, kChunk);
            arm_dcache_flush_delete(dst, kChunk);
            const uint32_t d = us() - r.issued;
            audioN = audioN + 1; audioSum = audioSum + d;
            if (d > audioMax) audioMax = d;
            if (d > kAheadUs) audioLate = audioLate + 1;
            continue;
        }
        if (frameWanted) {
            arm_dcache_flush_delete(picture, kFrame);
            frames.seekSet((frameIndex % kFrames) * kFrame);
            frames.read(picture, kFrame);
            arm_dcache_flush_delete(picture, kFrame);
            const uint32_t d = us() - frameIssued;
            frameIndex = frameIndex + 1;
            frameN = frameN + 1; frameSum = frameSum + d;
            if (d > frameMax) frameMax = d;
            if (d > kFrameMs * 1000) frameOver = frameOver + 1;
            frameWanted = false;
            continue;
        }
        if (writeWanted != kNone) {
            const WriteKind k = writeWanted;
            serveWrite(k, writeSeed);
            writeWanted = kNone;
            continue;
        }
        ulTaskNotifyTakeIndexed(kWork, pdTRUE, 1);
    }
}

// -- the main task --------------------------------------------------------------
static void flush(lv_display_t* d, const lv_area_t*, uint8_t*) { lv_display_flush_ready(d); }

static void printLateness(const char* when) {
    Serial.printf("  %-22s IR jitter %6.2f us over %lu interrupts, tick jitter %6.2f us over %lu\n", when,
                  irLate.jitterUs(), irLate.n, tickLate.jitterUs(), tickLate.n);
}

static void waitWrite() { while (writeWanted != kNone) vTaskDelay(1); }

static void mainMain(void*) {
    while (!Serial) vTaskDelay(10);
    ARM_DEMCR |= ARM_DEMCR_TRCENA; ARM_DWT_CTRL |= ARM_DWT_CTRL_CYCCNTENA;
    Serial.printf("Load test, PSRAM %d MB\nwriting the test file ...\n", external_psram_size);
    Serial.printf("status waits spin %lu us before the storage task sleeps\n", kSpinUs);
    xTaskCreate(storageMain, "storage", 8192 / sizeof(StackType_t), nullptr, 2, nullptr);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (storageStep != 4) {
        Serial.printf("the card failed at step %d, cluster %lu bytes, SdFat error 0x%02X, stopped\n", storageStep, clusterBytes, sd.sdErrorCode());
        vTaskSuspend(nullptr);
    }

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
    FastLED.setExclusiveDriver<fl::Bus::FLEX_IO, 0>();
    FastLED.addLeds<WS2812B, 2, GRB>(leds, 240);

    irLate.period   = kIrUs * (F_CPU_ACTUAL / 1000000);
    tickLate.period = kTickUs * (F_CPU_ACTUAL / 1000000);
    irTimer.priority(64);
    irTimer.begin(irIsr, kIrUs);
    startGpt1(kTickUs, 96);
    startGpt2(kAudioUs, 208);

    // 1. without load
    vTaskDelay(100);
    irLate.reset(); tickLate.reset();
    vTaskDelay(3000);
    Serial.println("\n1. interrupt jitter, the spread between the earliest and the latest interrupt");
    printLateness("without load");

    // 2. under load: audio, a frame every 40 ms, LVGL, FastLED, a log line a second
    irLate.reset(); tickLate.reset();
    audioOn = true;
    const uint32_t start = millis();
    uint32_t nextFrame = start, nextLog = start + 1000, seed = 1;
    while (millis() - start < kLoadMs) {
        if ((int32_t)(millis() - nextFrame) >= 0 && !frameWanted) {
            frameIssued = us(); frameWanted = true;
            xTaskNotifyGiveIndexed(storageTask, kWork);
            nextFrame += kFrameMs;
        }
        if ((int32_t)(millis() - nextLog) >= 0 && writeWanted == kNone) {
            writeSeed = seed++; writeWanted = kLog;
            xTaskNotifyGiveIndexed(storageTask, kWork);
            nextLog += 1000;
        }
        lv_obj_invalidate(img);
        lv_refr_now(d);
        for (auto& l : leds) l = CRGB(seed, 0, 0);
        FastLED.show();
        vTaskDelay(1);
    }
    printLateness("under load");
    Serial.printf("\n2. audio reads: %lu, waited mean %.2f ms, max %.2f ms, %lu past the %.0f ms read-ahead, %lu dropped\n",
                  audioN, audioN ? audioSum / audioN / 1000.0f : 0.0f, audioMax / 1000.0f, audioLate,
                  kAheadUs / 1000.0f, audioDropped);
    Serial.printf("   storage task: %lu sleeps in yield(), %lu ran out their tick, %lu of those during a transfer\n", sleeps, timeouts, timeoutsInTransfer);
    Serial.printf("   video frames: %lu of %lu due, from request to end mean %.2f ms, max %.2f ms, %lu past 40 ms\n",
                  frameN, kLoadMs / kFrameMs, frameN ? frameSum / frameN / 1000.0f : 0.0f, frameMax / 1000.0f,
                  frameOver);

    // 3. saves from the PSRAM and from RAM2, with audio still streaming
    const uint32_t audioMaxBefore = audioMax;
    audioMax = 0;
    for (uint32_t i = 0; i < kSaves; i++) {
        writeSeed = seed++; writeWanted = (i % 2) ? kSaveRam2 : kSavePsram;
        xTaskNotifyGiveIndexed(storageTask, kWork);
        waitWrite();
        vTaskDelay(20);
    }
    audioOn = false;
    vTaskDelay(100);
    Serial.printf("\n3. log lines: %lu, each written and synced in at most %.2f ms\n", logN, logMax / 1000.0f);
    Serial.printf("   saves of 4 KB from the PSRAM: %lu, at most %.2f ms; from RAM2: %lu, at most %.2f ms; %lu read back wrong\n",
                  savePN, savePMax / 1000.0f, saveRN, saveRMax / 1000.0f, saveBad);
    Serial.printf("   audio reads waited at most %.2f ms while the saves ran\n", audioMax / 1000.0f);
    (void)audioMaxBefore;

    irTimer.end();
    NVIC_DISABLE_IRQ(IRQ_GPT1);
    NVIC_DISABLE_IRQ(IRQ_GPT2);
    frames.close(); saves.close(); logFile.close();
    sd.remove("load.bin"); sd.remove("save.bin"); sd.remove("log.txt");
    Serial.println("\ndone, test files deleted");
    vTaskSuspend(nullptr);
}

void setup() {
    Serial.begin(115200);
    xTaskCreate(mainMain, "main", 8192 / sizeof(StackType_t), nullptr, 1, &mainTask);
    vTaskStartScheduler();
}

void loop() {}
