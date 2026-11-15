// Card-pull test for the Teensy 4.1 under FreeRTOS, with the storage task of the stack test.
//
// The storage task reads full-screen frames into the PSRAM without a pause, by SdFat's DMA as the
// storage driver does. The main task spins and records the longest time it did not get to run.
// The card is pulled while the reads run and put back later. The test reports:
// - how long the read takes that meets the pulled card, and how long the failing reads after it take
// - the longest the main task waited meanwhile
// - whether a new mount, tried once a second as the storage driver's probe does, brings the card
//   back, and how long one try takes
// It runs 60 s, prints a line twice a second and once at every change, and deletes its file at the
// end where the card is back. It starts once the serial monitor is open.

#define USE_ARDUINO_DEFINES
#include "arduino_freertos.h"
#include <SdFat.h>

constexpr uint32_t kFrame   = 320 * 240 * 2;
constexpr uint32_t kFrames  = 30;
constexpr UBaseType_t kCard = 1;

EXTMEM static uint8_t picture[kFrame] __attribute__((aligned(32)));
DMAMEM static uint8_t fillBuf[16384] __attribute__((aligned(32)));

static SdFs   sd;
static FsFile file;
static TaskHandle_t mainTask = nullptr, storageTask = nullptr;
static void (*sdfatIsr)() = nullptr;

static volatile uint32_t readsOk = 0, readsFailed = 0, lastReadUs = 0, longestFailUs = 0, firstFailUs = 0;
static volatile uint32_t mounts = 0, mountsOk = 0, longestMountUs = 0;
static volatile uint8_t  lastError = 0;
static volatile bool     up = false;          // the card reads
static volatile bool     stopping = false;

static bool inInterrupt() { uint32_t ipsr; asm volatile("mrs %0, ipsr" : "=r"(ipsr)); return ipsr != 0; }

void yield() {
    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING || inInterrupt()) return;
    if (storageTask == nullptr || xTaskGetCurrentTaskHandle() != storageTask) { freertos::default_yield(); return; }
    ulTaskNotifyTakeIndexed(kCard, pdTRUE, 1);
}

static void sdInterrupt() {
    sdfatIsr();
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveIndexedFromISR(storageTask, kCard, &woken);
    portYIELD_FROM_ISR(woken);
}

// mounts the card by DMA and takes over SdFat's interrupt, as the storage driver does
static bool mount() {
    file.close();
    sd.end();
    const bool ok = sd.begin(SdioConfig(DMA_SDIO));   // SdFat waits through yield(), where the storage task sleeps
    sdfatIsr = _VectorsRam[IRQ_SDHC1 + 16];
    attachInterruptVector(IRQ_SDHC1, sdInterrupt);
    NVIC_SET_PRIORITY(IRQ_SDHC1, 240);
    return ok && file.open("pull.bin", O_RDONLY);
}

static void storageMain(void*) {
    storageTask = xTaskGetCurrentTaskHandle();
    bool ok = false;
    {
        TaskHandle_t self = storageTask;
        storageTask = nullptr;
        if (sd.begin(SdioConfig(FIFO_SDIO)) && file.open("pull.bin", O_RDWR | O_CREAT | O_TRUNC)) {
            file.preAllocate(kFrames * kFrame);
            ok = true;
            for (uint32_t off = 0; ok && off < kFrames * kFrame; off += sizeof fillBuf)
                ok = file.write(fillBuf, sizeof fillBuf) == sizeof fillBuf;
            file.close();
        }
        storageTask = self;
    }
    ok = ok && mount();
    up = ok;
    xTaskNotifyGive(mainTask);

    uint32_t f = 0, lastMount = 0;
    while (!stopping) {
        if (!up) {                              // the probe: a new mount once a second
            if (millis() - lastMount < 1000) { vTaskDelay(10); continue; }
            lastMount = millis();
            const uint32_t t = micros();
            const bool back = mount();
            const uint32_t d = micros() - t;
            mounts = mounts + 1;
            if (d > longestMountUs) longestMountUs = d;
            if (back) { mountsOk = mountsOk + 1; up = true; }
            continue;
        }
        file.seekSet((f % kFrames) * kFrame);
        arm_dcache_flush_delete(picture, kFrame);
        const uint32_t t = micros();
        const int n = file.read(picture, kFrame);
        const uint32_t d = micros() - t;
        arm_dcache_flush_delete(picture, kFrame);
        lastReadUs = d;
        if (n == (int)kFrame) { readsOk = readsOk + 1; f++; continue; }
        if (readsFailed == 0) firstFailUs = d;
        readsFailed = readsFailed + 1;
        if (d > longestFailUs) longestFailUs = d;
        lastError = sd.sdErrorCode();
        if (readsFailed % 3 == 0) up = false;  // three failed reads in a row count as a failed card
        vTaskDelay(1);
    }
    file.close();
    if (up) sd.remove("pull.bin");
    xTaskNotifyGive(mainTask);
    vTaskSuspend(nullptr);
}

static void mainMain(void*) {
    while (!Serial) vTaskDelay(10);
    Serial.println("Card-pull test: pull the card about 15 s in, put it back about 15 s later");
    xTaskCreate(storageMain, "storage", 8192 / sizeof(StackType_t), nullptr, 2, nullptr);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (!up) { Serial.println("the card did not start, stopped"); vTaskSuspend(nullptr); }

    const uint32_t start = millis();
    uint32_t last = micros(), maxGap = 0, maxGapAll = 0, nextPrint = start + 500;
    bool wasUp = true;
    while (millis() - start < 60000) {
        const uint32_t now = micros();
        if (now - last > maxGap) maxGap = now - last;
        last = now;
        if (wasUp != up) {
            wasUp = up;
            Serial.printf("%5.1f s  the card is %s\n", (millis() - start) / 1000.0f, up ? "back" : "down");
            last = micros();
        }
        if ((int32_t)(millis() - nextPrint) >= 0) {
            if (maxGap > maxGapAll) maxGapAll = maxGap;
            Serial.printf("%5.1f s  reads ok %4lu failed %3lu, last read %6.2f ms, main task waited at most %6.2f ms, mounts %lu of %lu\n",
                          (millis() - start) / 1000.0f, readsOk, readsFailed, lastReadUs / 1000.0f, maxGap / 1000.0f,
                          mountsOk, mounts);
            maxGap = 0;
            nextPrint += 500;
            last = micros();
        }
    }
    Serial.printf("\nfirst failing read %.2f ms, longest failing read %.2f ms, SdFat error 0x%02X\n",
                  firstFailUs / 1000.0f, longestFailUs / 1000.0f, lastError);
    Serial.printf("mounts tried %lu, %lu brought the card back, the longest took %.2f ms\n",
                  mounts, mountsOk, longestMountUs / 1000.0f);
    Serial.printf("the main task waited at most %.2f ms in a row over the whole run\n", maxGapAll / 1000.0f);
    stopping = true;
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(3000));
    Serial.println("\ndone");
    vTaskSuspend(nullptr);
}

void setup() {
    Serial.begin(115200);
    xTaskCreate(mainMain, "main", 8192 / sizeof(StackType_t), nullptr, 1, &mainTask);
    vTaskStartScheduler();
}

void loop() {}
