#include "storage.h"
#include "core/clock.h"
#include "core/task-lock.h"
#include "logging/debug-log.h"
#include "logging/logger.h"
#include <FreeRTOS.h>
#include <SdFat.h>
#include <string.h>
#include <task.h>

// Runs the serial events and PJRC's EventResponder. freertos-teensy defines it.
namespace freertos {
void default_yield();
}

constexpr UBaseType_t TASK_PRIORITY = 1;   // general-design.md: the main task's priority
constexpr uint32_t PROBE_MS = 1000;        // storage.md: from one try of a failed card to the next
constexpr size_t BOUNCE_BYTES = 4096;      // storage.md: the read buffer in RAM1
constexpr size_t CACHE_LINE_BYTES = 32;    // the Cortex-M7's data cache line
constexpr uint8_t SD_PRIORITY = 240;       // general-design.md: the SD controller's interrupt
constexpr uint8_t LIMIT_PRIORITY = 208;    // general-design.md: FreeRTOS's limit
constexpr uint32_t FIRST_IRQ = 16;         // ARMv7-M: exception 16 is the first interrupt
constexpr PartMask CARD = 1;               // part 0, the card
constexpr uint8_t DAT3_PIN = 46;         // PJRC's SD.cpp: _SD_DAT3, DAT3 of the Teensy 4.1's socket
constexpr uint32_t DAT3_SETTLE_US = 5;   // PJRC's SD.cpp: the wait for the pull-down to settle

// SdFat's card as the file system sees it. It forwards every sector and counts the transfers that
// fail, which tells a card error from a file that is not there. Each failure goes into the log.
class CountingCard : public FsBlockDevice {
public:
    explicit CountingCard(SdioCard& card) : card(card) {}

    uint32_t failures() const { return failed; }

    bool isBusy() override { return card.isBusy(); }
    uint32_t sectorCount() override { return card.sectorCount(); }
    bool syncDevice() override { return count(card.syncDevice()); }
    bool readSector(uint32_t s, uint8_t* to) override { return count(card.readSector(s, to)); }
    bool readSectors(uint32_t s, uint8_t* to, size_t n) override {
        return count(card.readSectors(s, to, n));
    }
    bool writeSector(uint32_t s, const uint8_t* from) override {
        return count(card.writeSector(s, from));
    }
    bool writeSectors(uint32_t s, const uint8_t* from, size_t n) override {
        return count(card.writeSectors(s, from, n));
    }

private:
    bool count(bool worked) {
        if (!worked) {
            failed = failed + 1;
            logWarning("storage", "card error %u at SdFat's line %lu", unsigned(card.errorCode()),
                       static_cast<unsigned long>(card.errorLine()));
        }
        return worked;
    }

    SdioCard& card;
    uint32_t failed = 0;
};

// The card's files through SdFat in its DMA mode. SdFat's buffers and the file table lie in RAM1
// with the object, where no cache sits between the processor and the card's DMA.
class SdCardFiles : public CardFiles {
public:
    bool mount() override {
        if (!start()) {
            return false;
        }
        known = card.readCID(&identity);
        return true;
    }

    bool remount() override {
        cid_t now;
        if (!start() || !card.readCID(&now)) {
            return false;
        }
        if (!known || memcmp(&now, &identity, sizeof now) != 0) {
            logWarning("storage", "another card sits in the socket, it stays unused");
            return false;
        }
        return true;
    }

    uint32_t failedTransfers() const override { return counted.failures(); }

    uint32_t errorCode() const override { return card.errorCode(); }

    bool open(uint8_t slot, const char* path, bool writes) override {
        FsFile& f = files[slot];
        if (!writes) {
            if (!f.open(&volume, path, O_RDONLY)) {
                return false;
            }
            // A FAT32 file forgets on open that its clusters lie in one piece. Marked so, its
            // reads take the next cluster without asking the FAT, which made a full-screen frame
            // 0.7 ms faster on the bench.
            f.contiguousRange(nullptr, nullptr);
            return true;
        }
        if (!makeDirectories(path)) {
            return false;
        }
        return f.open(&volume, path, O_RDWR | O_CREAT);
    }

    void close(uint8_t slot) override { files[slot].close(); }

    uint32_t size(uint8_t slot) override { return uint32_t(files[slot].fileSize()); }

    bool read(uint8_t slot, uint32_t at, uint8_t* to, size_t n, size_t& got) override {
        FsFile& f = files[slot];
        got = 0;
        if (!f.seekSet(at)) {
            return false;
        }
        if (readsInPlace(to, at, n)) {
            // SdFat's DMA writes past the cache. Before the read the lines over the buffer are
            // written back and dropped, so none lands on the new bytes. After the read they are
            // dropped again, so the processor reads what the card brought.
            arm_dcache_flush_delete(to, n);
            const int read = f.read(to, n);
            arm_dcache_flush_delete(to, n);
            if (read < 0) {
                return false;
            }
            got = size_t(read);
            return true;
        }
        while (got < n) {
            const size_t want = n - got < BOUNCE_BYTES ? n - got : BOUNCE_BYTES;
            const int read = f.read(bounce, want);
            if (read < 0) {
                return false;
            }
            memcpy(to + got, bounce, size_t(read));
            got = got + size_t(read);
            if (size_t(read) < want) {
                break;
            }
        }
        return true;
    }

    bool write(uint8_t slot, uint32_t at, const uint8_t* data, size_t n) override {
        FsFile& f = files[slot];
        // SdFat's DMA reads the memory past the cache, so the cache writes the data back first.
        arm_dcache_flush(const_cast<uint8_t*>(data), n);
        return f.seekSet(at) && f.write(data, n) == n;
    }

    bool cut(uint8_t slot, uint32_t bytes) override { return files[slot].truncate(bytes); }

    bool commit(uint8_t slot) override { return files[slot].sync(); }

private:
    // Starts the card and its file system. SdFat's start sets its interrupt's priority, which the
    // firmware's table sets back.
    bool start() {
        // SdFat looks for a missing card for 3 s. A card pulls its DAT3 line high against the
        // pull-down, which is how PJRC's SD library detects a card. Without a card the start ends
        // at once. SdFat's start gives the pin back to the SD controller.
        pinMode(DAT3_PIN, INPUT_PULLDOWN);
        delayMicroseconds(DAT3_SETTLE_US);
        if (!digitalReadFast(DAT3_PIN)) {
            return false;
        }
        const bool started = card.begin(SdioConfig(DMA_SDIO)) && volume.begin(&counted);
        NVIC_SET_PRIORITY(IRQ_SDHC1, SD_PRIORITY);
        return started;
    }

    // Creates the directories above a file where they are missing.
    bool makeDirectories(const char* path) {
        char directory[FILE_PATH_BYTES];
        const char* last = strrchr(path, '/');
        if (last == nullptr || last == path) {
            return true;
        }
        const size_t n = size_t(last - path);
        memcpy(directory, path, n);
        directory[n] = '\0';
        return volume.exists(directory) || volume.mkdir(directory, true);
    }

    SdioCard card;
    CountingCard counted{card};
    FsVolume volume;
    FsFile files[SLOTS];
    cid_t identity = {};
    bool known = false;   // mount() read the card's identity
    alignas(CACHE_LINE_BYTES) uint8_t bounce[BOUNCE_BYTES] = {};
};

// These lie in RAM1 with the storage task's stack, since SdFat reads some sectors by DMA into it.
static SdCardFiles cardFiles;
static StackType_t stack[Storage::STACK_BYTES / sizeof(StackType_t)];
static StaticTask_t taskBlock;
static TaskHandle_t storageTask = nullptr;
static TaskHandle_t starter = nullptr;   // the task begin() runs in, which waits for the mount

Storage storage;

Storage::Storage() : StorageService(cardFiles) {}

// Gets the exception the caller runs in, 0 in a task.
static uint32_t exception() {
    uint32_t ipsr = 0;
    asm volatile("mrs %0, ipsr" : "=r"(ipsr));
    return ipsr;
}

bool Storage::begin() {
    if (storageTask != nullptr) {
        return mounted();
    }
    starter = xTaskGetCurrentTaskHandle();
    storageTask = xTaskCreateStatic(run, "storage", STACK_BYTES / sizeof(StackType_t), this,
                                    TASK_PRIORITY, stack, &taskBlock);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (!mounted()) {
        logWarning("storage", "no card, or its file system does not read, error %lu",
                   static_cast<unsigned long>(lastError()));
    }
    return mounted();
}

uint32_t Storage::stackLeftBytes() const {
    if (storageTask == nullptr) {
        return 0;
    }
    return uint32_t(uxTaskGetStackHighWaterMark(storageTask)) * sizeof(StackType_t);
}

Fault Storage::failed() const {
    const bool down = mountTried && failedCard();
    return {down ? CARD : PartMask(0), down ? lastError() : 0};
}

void Storage::run(void* self) {
    Storage& s = *static_cast<Storage*>(self);
    s.mount();
    s.mountTried = true;
    xTaskNotifyGive(starter);
    uint32_t triedUs = nowUs();
    for (;;) {
        if (s.failedCard() && s.mounted() && elapsedMs(triedUs, PROBE_MS)) {
            if (s.recover()) {
                logInfo("storage", "the card works again");
            }
            // The next try comes a second after this one has ended.
            triedUs = nowUs();
        }
        if (s.serveNext()) {
            continue;
        }
        const bool probing = s.failedCard() && s.mounted();
        ulTaskNotifyTake(pdTRUE, probing ? pdMS_TO_TICKS(PROBE_MS) : portMAX_DELAY);
    }
}

bool Storage::callerMayStart() const {
    const uint32_t e = exception();
    if (e == 0) {
        return xTaskGetSchedulerState() == taskSCHEDULER_RUNNING;
    }
    return e >= FIRST_IRQ && NVIC_GET_PRIORITY(e - FIRST_IRQ) >= LIMIT_PRIORITY;
}

bool Storage::callerMayWait() const {
    return !inInterrupt() && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING &&
           xTaskGetCurrentTaskHandle() != storageTask;
}

void Storage::wake() {
    if (storageTask == nullptr) {
        return;
    }
    if (!inInterrupt()) {
        xTaskNotifyGive(storageTask);
        return;
    }
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(storageTask, &woken);
    portYIELD_FROM_ISR(woken);
}

void Storage::pause() {
    vTaskDelay(1);
}

uint32_t Storage::lock() {
    if (inInterrupt()) {
        return taskENTER_CRITICAL_FROM_ISR();
    }
    taskENTER_CRITICAL();
    return 0;
}

void Storage::unlock(uint32_t held) {
    if (inInterrupt()) {
        taskEXIT_CRITICAL_FROM_ISR(held);
        return;
    }
    taskEXIT_CRITICAL();
}

// SdFat calls yield() while it waits for the card. The storage task hands the processor to the
// main task until the next tick, so the main loop runs while the card works. Every other task
// runs the serial events, as Teensyduino's yield() does.
void yield() {
    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING || inInterrupt()) {
        return;
    }
    if (storageTask != nullptr && xTaskGetCurrentTaskHandle() == storageTask) {
        taskYIELD();
        return;
    }
    freertos::default_yield();
}
