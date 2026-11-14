// Card speed test for the Teensy 4.1's built-in SD socket.
//
// Writes a test file to the card, then reads it back into the PSRAM the way the video player
// and the audio read: full-screen frames in 16 KB chunks, and 4 KB chunks at random places.
// Each run checks every byte it read. The runs read with SdFat's FIFO mode and with its DMA mode
// through the file, and with its DMA mode as one raw read of the file's sectors per frame. The
// estimates the times are compared with stand in the display's appendix.
//
// The test starts once the serial monitor is open and deletes its file at the end.

#include <SdFat.h>

extern "C" uint8_t external_psram_size;

constexpr uint32_t kChunk       = 16 * 1024;        // the chunk a movie frame is read in
static uint32_t    kAudio       = 4 * 1024;         // the chunk an audio stream is read in, set by each run
constexpr uint32_t kFrame       = 320 * 240 * 2;    // one full-screen frame in RGB565
constexpr uint32_t kFrames      = 200;              // frames per run, 8 s of video
constexpr uint32_t kFileSize    = kFrames * kFrame;
constexpr uint32_t kRandomReads = 1000;
const char* const  kName        = "speed.bin";

EXTMEM uint8_t frameMem[kFrame + 32] __attribute__((aligned(32)));
DMAMEM uint8_t writeBuf[kChunk] __attribute__((aligned(32)));   // also the RAM2 chunk of the copy run

enum Mode { kFifo, kDma, kCopy, kRaw, kCluster, kWhole };

SdFs   sd;
FsFile file;

// the word the file holds at a byte offset, so a read can be checked without a copy
static uint32_t wordAt(uint32_t offset) {
    uint32_t x = (offset + 1) * 2654435761u;
    return x ^ (x >> 16);
}

struct Stat {
    uint32_t n = 0, max = 0;
    uint64_t sum = 0;
    void  add(uint32_t us) { n++; sum += us; if (us > max) max = us; }
    float meanMs() const { return n ? (float)sum / n / 1000.0f : 0.0f; }
    float maxMs() const { return max / 1000.0f; }
};

static bool check(const uint8_t* dst, uint32_t base, uint32_t bytes) {
    const uint32_t* w = (const uint32_t*)dst;
    for (uint32_t i = 0; i < bytes / 4; i++) {
        if (w[i] != wordAt(base + 4 * i)) {
            Serial.printf("  DATA WRONG at file offset %lu: read %08lX, expected %08lX\n",
                          base + 4 * i, w[i], wordAt(base + 4 * i));
            return false;
        }
    }
    return true;
}

static bool writeFile() {
    if (!file.open(kName, O_RDWR | O_CREAT | O_TRUNC)) return false;
    if (!file.preAllocate(kFileSize)) Serial.println("  preAllocate failed, the file may be fragmented");
    uint32_t* w = (uint32_t*)writeBuf;
    for (uint32_t off = 0; off < kFileSize; off += kChunk) {
        for (uint32_t i = 0; i < kChunk / 4; i++) w[i] = wordAt(off + 4 * i);
        if (file.write(writeBuf, kChunk) != kChunk) { file.close(); return false; }
    }
    file.close();
    return true;
}

// SdFat's DMA writes memory past the cache, so a DMA read cleans and invalidates the cache over
// its buffer before and after, as the storage driver does. FIFO reads go through the processor.
// The copy run reads by DMA into the RAM2 chunk and copies it into the PSRAM. The raw run reads
// the file's sectors by DMA in one command for the whole request, past SdFat's file layer, which
// cuts every read at the end of a cluster.
static uint32_t firstSector = 0;

static int readInto(uint8_t* dst, uint32_t n, Mode mode) {
    if (mode == kRaw) {
        arm_dcache_flush_delete(dst, n);
        uint32_t at = firstSector + file.curPosition() / 512;
        bool ok = sd.card()->readSectors(at, dst, n / 512);
        arm_dcache_flush_delete(dst, n);
        file.seekCur(n);
        return ok ? (int)n : -1;
    }
    uint8_t* to = mode == kCopy ? writeBuf : dst;
    if (mode != kFifo) arm_dcache_flush_delete(to, n);
    int got = file.read(to, n);
    if (mode != kFifo) arm_dcache_flush_delete(to, n);
    if (mode == kCopy) memcpy(dst, writeBuf, n);
    return got;
}

static void run(const char* label, SdioConfig config, Mode mode, uint32_t shift, uint32_t audioChunk = 4 * 1024) {
    kAudio = audioChunk;
    Serial.printf("\n%s\n", label);
    sd.end();
    if (!sd.begin(config) || !file.open(kName, O_RDONLY)) {
        Serial.println("  card or file did not open");
        return;
    }
    uint32_t lastSector = 0;
    if (mode == kRaw && !file.contiguousRange(&firstSector, &lastSector)) {
        Serial.println("  the file is not contiguous, run skipped");
        file.close();
        return;
    }
    uint8_t* dst = frameMem + shift;
    Stat chunk, frame, audio;
    bool ok = true;

    for (uint32_t f = 0; f < kFrames && ok; f++) {
        uint32_t base = f * kFrame;
        file.seekSet(base);
        elapsedMicros frameTime = 0;
        for (uint32_t done = 0; done < kFrame && ok; ) {
            uint32_t n = min(kChunk, kFrame - done);
            if (mode == kRaw || mode == kWhole) n = kFrame;
            if (mode == kCluster) {   // up to the end of the cluster the read starts in
                uint32_t cluster = sd.bytesPerCluster();
                n = min(kFrame - done, cluster - (base + done) % cluster);
            }
            elapsedMicros chunkTime = 0;
            ok = readInto(dst + done, n, mode) == (int)n;
            if (n == kChunk) chunk.add(chunkTime);   // the short last chunk of a frame is left out
            done += n;
        }
        frame.add(frameTime);
        if (ok) ok = check(dst, base, kFrame);
    }

    randomSeed(1);
    for (uint32_t i = 0; i < kRandomReads && ok; i++) {
        uint32_t base = random(kFileSize / kAudio) * kAudio;
        file.seekSet(base);
        elapsedMicros t = 0;
        ok = readInto(dst, kAudio, mode) == (int)kAudio;
        audio.add(t);
        if (ok) ok = check(dst, base, kAudio);
    }
    file.close();

    if (!ok) { Serial.println("  RUN FAILED"); return; }
    if (chunk.n) Serial.printf("  16 KB chunk      mean %6.2f ms   max %6.2f ms   estimate 1.33 ms\n",
                               chunk.meanMs(), chunk.maxMs());
    Serial.printf("  150 KB frame     mean %6.2f ms   max %6.2f ms   estimate 12.4 ms, limit 40 ms\n",
                  frame.meanMs(), frame.maxMs());
    Serial.printf("  %lu KB random      mean %6.2f ms   max %6.2f ms\n", kAudio / 1024,
                  audio.meanMs(), audio.maxMs());
    Serial.printf("  frame rate read  %.1f MB/s\n", kFrame / frame.meanMs() / 1000.0f);
    Serial.println("  data             every byte as written");
}

void setup() {
    Serial.begin(115200);
    while (!Serial) {}   // the test waits for the serial monitor, so no line is lost
    Serial.printf("Card speed test, PSRAM %d MB\n", external_psram_size);
    if (external_psram_size == 0) { Serial.println("no PSRAM, stopped"); return; }

    if (!sd.begin(SdioConfig(FIFO_SDIO))) { Serial.println("no card, stopped"); return; }
    Serial.printf("card %.1f GB, %s\n", sd.card()->sectorCount() * 512.0 / 1e9,
                  sd.fatType() == FAT_TYPE_EXFAT ? "exFAT" : "FAT32");
    Serial.printf("cluster %lu bytes\n", sd.bytesPerCluster());

    Serial.printf("writing %lu MB test file ...\n", kFileSize / 1000000);
    elapsedMillis writeTime = 0;
    if (!writeFile()) { Serial.println("write failed, stopped"); return; }
    Serial.printf("  written at %.1f MB/s\n", kFileSize / (float)writeTime / 1000.0f);

    run("DMA_SDIO, the whole frame in one read, 4 KB audio chunks", SdioConfig(DMA_SDIO), kWhole, 0, 4 * 1024);
    run("DMA_SDIO, the whole frame in one read, 8 KB audio chunks", SdioConfig(DMA_SDIO), kWhole, 0, 8 * 1024);

    sd.end();
    if (sd.begin(SdioConfig(FIFO_SDIO))) sd.remove(kName);
    Serial.println("\ndone, test file deleted");
}

void loop() {}
