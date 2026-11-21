// Load test for the Teensy 4.1 with its PSRAM and the card in its socket, built like the stack test.
//
// Runs the firmware's timing-critical work side by side and measures:
// 1. how late the IR driver's interrupt (priority 64, every 600 us) and the driver tick
//    (priority 96, every 5 ms) come, without load and under load, and what the IR driver's 16
//    conversions a phase cost: the time each conversion takes beyond its clocks, the block of 16
//    against its budget, and the share of the processor their interrupts take
// 2. how long the four audio streams' reads wait while video frames, LVGL and FastLED run, against
//    the 325 ms an audio ring reads ahead
// 3. how long saves and log writes take under FreeRTOS, from the PSRAM and from RAM2, and whether
//    every byte arrives
//
// The load is a video frame read whole into the PSRAM every 40 ms, LVGL drawing that frame full
// screen, FastLED sending 240 dark LEDs on one chain and a log line once a second. An audio request
// comes every 23.2 ms from an interrupt at priority 208, as the Audio library's update starts them.
// The main loop never waits, as in the firmware. The storage task shares its priority and takes
// turns with it every tick, since its yield() hands the processor on while SdFat waits. It serves
// audio first, then video, then the writes. The card is formatted FAT32 with
// 32 KB clusters. The test writes three files and deletes them. It starts once the serial
// monitor is open.

#define USE_ARDUINO_DEFINES   // keeps Arduino's macros, which FastLED's Teensy 4 code uses
#include "arduino_freertos.h"
#include <SdFat.h>
#include <lvgl.h>
#include <FastLED.h>
#include <SPI.h>

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

// -- the IR driver's conversions -------------------------------------------------
// Each phase runs the IR driver's read block on LPSPI4: 16 transfers of 24 clocks at 1.35 MHz,
// each started by the receive interrupt of the one before, at the IR's priority 64. No converter
// is attached, so the data is junk, but the interrupts come at the real rate. Only pins 11 to 13
// are given to LPSPI4. Pin 10, its chip select, carries LED chain F. IR_SPI=0 builds the test
// without the conversions, for the comparison.
#ifndef IR_SPI
#define IR_SPI 1
#endif
constexpr uint32_t kConversions = 16;        // n_channels, model/parts/ir-reflective
constexpr uint32_t kSpiHz       = 1350000;   // f_clk, MCP3008 DS21295D at VDD = 2.7 V
constexpr uint32_t kFrameBits   = 24;        // clocks_frame, DS21295D section 6.1
constexpr float    kBudgetUs    = 320.0f;    // budget_16, model/parts/ir-reflective
constexpr float    kOvhUs       = 2.0f;      // t_ovh, model/parts/ir-reflective

struct Conversions {
    volatile uint32_t n = 0, blockStart = 0, lastStart = 0;
    volatile uint32_t blocks = 0, unfinished = 0, stepMax = 0, blockMax = 0, isrMax = 0;
    volatile uint64_t isrSum = 0;
    volatile bool     running = false;
    uint32_t          sinceUs = 0;
    void reset() {
        blocks = 0; unfinished = 0; stepMax = 0; blockMax = 0; isrMax = 0; isrSum = 0;
        sinceUs = micros();
    }
};
static Conversions conv;

FASTRUN static void startBlock() {
    const uint32_t t0 = ARM_DWT_CYCCNT;
    if (conv.running) conv.unfinished = conv.unfinished + 1;   // the last phase's block still runs
    conv.running = true;
    conv.n = 0;
    LPSPI4_TDR = 0;
    const uint32_t t1 = ARM_DWT_CYCCNT;
    conv.blockStart = t1; conv.lastStart = t1;
    conv.isrSum = conv.isrSum + (t1 - t0);
}

FASTRUN static void spiIsr() {
    const uint32_t t0 = ARM_DWT_CYCCNT;
    (void)LPSPI4_RDR;                        // the reply; reading it clears the receive flag
    const uint32_t n = conv.n + 1;
    conv.n = n;
    uint32_t t1;
    if (n < kConversions) {
        LPSPI4_TDR = 0;                      // the next conversion
        t1 = ARM_DWT_CYCCNT;
    } else {
        t1 = ARM_DWT_CYCCNT;
        conv.running = false;
        const uint32_t block = t1 - conv.blockStart;
        if (block > conv.blockMax) conv.blockMax = block;
        conv.blocks = conv.blocks + 1;
    }
    const uint32_t step = t1 - conv.lastStart;
    if (step > conv.stepMax) conv.stepMax = step;
    conv.lastStart = t1;
    const uint32_t isr = ARM_DWT_CYCCNT - t0;
    if (isr > conv.isrMax) conv.isrMax = isr;
    conv.isrSum = conv.isrSum + isr;
    asm volatile("dsb");
}

// The SCK the SPI library set, from the LPSPI clock root and the dividers, as SPI.h computes it
static float sckHz() {
    static const uint32_t clkSel[4] = {664615384, 720000000, 528000000, 396000000};
    const uint32_t cbcmr = CCM_CBCMR;
    const uint32_t clkhz = clkSel[(cbcmr >> 4) & 0x03] / (((cbcmr >> 26) & 0x07) + 1);
    const uint32_t div = LPSPI4_CCR & 0xFF, prescale = (LPSPI4_TCR >> 27) & 0x07;
    return clkhz / (float)(1u << prescale) / (div + 2);
}

static void startConversions() {
    SPI.begin();                             // pins 11 to 13 and the clock, no chip select
    SPI.beginTransaction(SPISettings(kSpiHz, MSBFIRST, SPI_MODE0));
    LPSPI4_TCR = (LPSPI4_TCR & ~LPSPI_TCR_FRAMESZ(0xFFF)) | LPSPI_TCR_FRAMESZ(kFrameBits - 1);
    LPSPI4_FCR = LPSPI_FCR_RXWATER(0);       // a receive interrupt for every reply
    attachInterruptVector(IRQ_LPSPI4, spiIsr);
    NVIC_SET_PRIORITY(IRQ_LPSPI4, 64);
    NVIC_ENABLE_IRQ(IRQ_LPSPI4);
    LPSPI4_IER = LPSPI_IER_RDIE;
}

static void printConversions(const char* when) {
    const float mhz = F_CPU_ACTUAL / 1e6f, tConv = kFrameBits / sckHz() * 1e6f;
    const float blockUs = conv.blockMax / mhz, stepUs = conv.stepMax / mhz;
    const float cpu = conv.isrSum / mhz / (float)(micros() - conv.sinceUs) * 100.0f;
    Serial.printf("  %-22s IR conversions: %lu blocks, %lu not finished in their phase\n", when,
                  conv.blocks, conv.unfinished);
    Serial.printf("  %-22s   SCK %.4f MHz, t_conv %.2f us; longest conversion %.2f us, %.2f us past t_conv\n", "",
                  sckHz() / 1e6f, tConv, stepUs, stepUs - tConv);
    Serial.printf("  %-22s   longest block %.2f us of the %.0f us budget, block / 16 - t_conv = %.2f us against t_ovh %.1f us\n", "",
                  blockUs, kBudgetUs, blockUs / kConversions - tConv, kOvhUs);
    Serial.printf("  %-22s   their interrupts take %.2f %% of the processor, the longest %.2f us\n", "",
                  cpu, conv.isrMax / mhz);
}

FASTRUN static void irIsr() {
    irLate.hit();
#if IR_SPI
    startBlock();
#endif
}
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
static volatile uint64_t frameSum = 0, frameWaitSum = 0, frameReadSum = 0;
static volatile uint32_t frameReadMax = 0;
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

// SdFat calls yield() while it waits for the card. The storage task hands the processor to the main
// task until the next tick, as the storage driver's yield() does
void yield() {
    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING || inInterrupt()) return;
    if (storageTask == nullptr || xTaskGetCurrentTaskHandle() != storageTask) { freertos::default_yield(); return; }
    taskYIELD();
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
    NVIC_SET_PRIORITY(IRQ_SDHC1, 240);
    storageTask = xTaskGetCurrentTaskHandle();
    ok = ok && frames.open("load.bin", O_RDONLY) && saves.open("save.bin", O_RDWR | O_CREAT | O_TRUNC)
            && logFile.open("log.txt", O_RDWR | O_CREAT | O_TRUNC);
    // a FAT32 file forgets on open that it lies in one piece; contiguousRange() walks its clusters
    // once and marks it, so later reads and seeks take its clusters without asking the FAT, as the
    // storage driver does for a file it opens for reading
    if (ok) frames.contiguousRange(nullptr, nullptr);
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
            const uint32_t started = us();
            arm_dcache_flush_delete(picture, kFrame);
            frames.seekSet((frameIndex % kFrames) * kFrame);
            frames.read(picture, kFrame);
            arm_dcache_flush_delete(picture, kFrame);
            const uint32_t ended = us(), d = ended - frameIssued;
            frameWaitSum = frameWaitSum + (started - frameIssued);
            frameReadSum = frameReadSum + (ended - started);
            if (ended - started > frameReadMax) frameReadMax = ended - started;
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
#if IR_SPI
    printConversions(when);
#endif
}

static void waitWrite() { while (writeWanted != kNone) vTaskDelay(1); }

static void runMainTask(void*) {
    while (!Serial) vTaskDelay(10);
    ARM_DEMCR |= ARM_DEMCR_TRCENA; ARM_DWT_CTRL |= ARM_DWT_CTRL_CYCCNTENA;
    Serial.printf("Load test, PSRAM %d MB\nwriting the test file ...\n", external_psram_size);
    xTaskCreate(storageMain, "storage", 8192 / sizeof(StackType_t), nullptr, 1, nullptr);
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
#if IR_SPI
    startConversions();
#endif
    irTimer.priority(64);
    irTimer.begin(irIsr, kIrUs);
    startGpt1(kTickUs, 96);
    startGpt2(kAudioUs, 208);

    // 1. without load
    vTaskDelay(100);
    irLate.reset(); tickLate.reset(); conv.reset();
    vTaskDelay(3000);
    Serial.println("\n1. interrupt jitter, the spread between the earliest and the latest interrupt");
    printLateness("without load");

    // 2. under load: audio, a frame every 40 ms, LVGL, FastLED, a log line a second
    irLate.reset(); tickLate.reset(); conv.reset();
    audioOn = true;
    const uint32_t start = millis();
    uint32_t nextFrame = start, nextLog = start + 1000, seed = 1;
    uint32_t lastPass = us(), passN = 0, passMax = 0;
    uint64_t passSum = 0;
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
        for (auto& l : leds) l = CRGB::Black;   // the data goes out, nothing lights
        FastLED.show();
        const uint32_t now = us();
        const uint32_t pass = now - lastPass;
        lastPass = now;
        passN = passN + 1; passSum = passSum + pass;
        if (pass > passMax) passMax = pass;
    }
    printLateness("under load");
    Serial.printf("\n2. audio reads: %lu, waited mean %.2f ms, max %.2f ms, %lu past the %.0f ms read-ahead, %lu dropped\n",
                  audioN, audioN ? audioSum / audioN / 1000.0f : 0.0f, audioMax / 1000.0f, audioLate,
                  kAheadUs / 1000.0f, audioDropped);
    Serial.printf("   main loop: %lu passes, mean %.2f ms, longest %.2f ms\n", passN, passN ? passSum / passN / 1000.0f : 0.0f, passMax / 1000.0f);
    Serial.printf("   video frames: %lu of %lu due, from request to end mean %.2f ms, max %.2f ms, %lu past 40 ms\n",
                  frameN, kLoadMs / kFrameMs, frameN ? frameSum / frameN / 1000.0f : 0.0f, frameMax / 1000.0f,
                  frameOver);
    Serial.printf("   of that, waiting behind the audio reads mean %.2f ms, the read itself mean %.2f ms, max %.2f ms; the file is %scontiguous\n",
                  frameN ? frameWaitSum / frameN / 1000.0f : 0.0f, frameN ? frameReadSum / frameN / 1000.0f : 0.0f,
                  frameReadMax / 1000.0f, frames.isContiguous() ? "" : "not ");

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
#if IR_SPI
    NVIC_DISABLE_IRQ(IRQ_LPSPI4);
#endif
    NVIC_DISABLE_IRQ(IRQ_GPT1);
    NVIC_DISABLE_IRQ(IRQ_GPT2);
    frames.close(); saves.close(); logFile.close();
    sd.remove("load.bin"); sd.remove("save.bin"); sd.remove("log.txt");
    Serial.println("\ndone, test files deleted");
    vTaskSuspend(nullptr);
}

void setup() {
    Serial.begin(115200);
    xTaskCreate(runMainTask, "main", 8192 / sizeof(StackType_t), nullptr, 1, &mainTask);
    vTaskStartScheduler();
}

void loop() {}
