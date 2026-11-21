// Stream test for the Teensy 4.1 with its PSRAM and the card in its socket, on the firmware's
// storage driver.
//
// The driver streams four sounds and a movie for 60 s while the main loop draws with LVGL, sends a
// dark frame to 240 LEDs on one chain and writes 2 KB of log a second and a 4 KB save every 3 s.
// The movie file of 9.2 MB is written anew before every run, since the card holds single writes
// longest right after a large file. Each sound reads 8 KB chunks into a ring of 192 KB in the
// PSRAM. The movie reads 25 frames ahead into the PSRAM. The test reports what a person would hear
// and see: a sound block that finds its ring empty, and a frame that is not there when its 40 ms
// come. It also reports the longest read and write of the card. It starts once the serial monitor
// is open and leaves its files in /tests/stream/ on the card.

#include <Arduino.h>
#include <FreeRTOS.h>
#include <IntervalTimer.h>
#include <lvgl.h>
#include <task.h>
#pragma GCC diagnostic push
#include <FastLED.h>
#pragma GCC diagnostic pop
#include "core/clock.h"
#include "drivers/storage.h"

constexpr uint32_t FRAME = 320 * 240 * 2;
constexpr uint32_t FILE_FRAMES = 60;
constexpr uint32_t FILE_BYTES = FILE_FRAMES * FRAME;
constexpr uint32_t CHUNK = 8192;
constexpr int32_t RING = 192 * 1024;    // buf in model/parts/audio/figures.py
constexpr uint32_t AHEAD_FRAMES = 25;   // n_video_ahead in model/parts/display/figures.py
constexpr uint32_t STREAMS = 4;
constexpr int32_t BLOCK_BYTES = 256;    // 128 mono 16-bit samples
constexpr uint32_t BLOCK_US = 2902;     // 128 samples at 44.1 kHz
constexpr float BYTES_PER_MS = 88.2f;   // one mono stream
constexpr uint32_t FRAME_MS = 40;
constexpr uint32_t RUN_MS = 60000;
constexpr uint32_t LOG_EVERY_MS = 1000;
constexpr uint32_t LOG_BYTES = 2048;   // about 16 lines a second
constexpr uint32_t SAVE_EVERY_MS = 3000;
constexpr uint32_t SAVE_BYTES = 4096;
constexpr uint32_t WRITE_CHUNK = 65536;

constexpr char MOVIE[] = "/tests/stream/movie.bin";
constexpr char LOG[] = "/tests/stream/log.txt";
constexpr char SAVE[] = "/tests/stream/save.bin";

EXTMEM static uint8_t rings[STREAMS][RING] __attribute__((aligned(32)));
EXTMEM static uint8_t frames[AHEAD_FRAMES][FRAME] __attribute__((aligned(32)));
EXTMEM static uint8_t source[WRITE_CHUNK] __attribute__((aligned(32)));
DMAMEM static uint8_t lvBuffers[2][FRAME] __attribute__((aligned(32)));
static uint8_t logText[LOG_BYTES];
static uint8_t saveData[SAVE_BYTES];
static CRGB leds[240];

struct SoundStream {
    FileRequest r{FilePriority::HIGHEST};   // audio.md: the sound's priority
    int32_t fill = 0;
    uint32_t writeAt = 0;
    uint32_t fileAt = 0;
    bool pending = false;
    int32_t minFill = RING;
    uint32_t underruns = 0;
    uint32_t failed = 0;
    uint32_t startedUs = 0;
};
static SoundStream streams[STREAMS];
static FileReader movie;
static volatile uint32_t audioReadMaxUs = 0;
static volatile bool audioFilling = false;
static volatile bool audioRunning = false;
static IntervalTimer audioTimer;

// The audio library's update: every channel plays one block out of its ring and asks for the next
// chunk once a chunk is free.
static void audioBlock() {
    if (!audioFilling) {
        return;
    }
    for (uint32_t s = 0; s < STREAMS; ++s) {
        SoundStream& st = streams[s];
        if (st.pending && st.r.ended()) {
            const uint32_t took = micros() - st.startedUs;
            if (took > audioReadMaxUs) {
                audioReadMaxUs = took;
            }
            if (st.r.status() == FileStatus::SUCCEEDED) {
                st.fill += CHUNK;
                st.writeAt = (st.writeAt + CHUNK) % RING;
                st.fileAt = (st.fileAt + CHUNK) % FILE_BYTES;
            } else {
                st.failed++;
            }
            st.pending = false;
        }
        if (audioRunning) {
            st.fill -= BLOCK_BYTES;
            if (st.fill < st.minFill) {
                st.minFill = st.fill;
            }
            if (st.fill < 0) {
                st.underruns++;
                st.fill = 0;
            }
        }
        if (!st.pending && RING - st.fill >= int32_t(CHUNK)) {
            st.startedUs = micros();
            movie.read(st.fileAt, rings[s] + st.writeAt, CHUNK, st.r);
            st.pending = true;
        }
    }
}

static bool writeMovie() {
    FileRequest r;
    Serial.printf("writing the movie file ...\n");
    const FileWriter w = storage.openWriter(MOVIE);
    for (uint32_t at = 0; at < FILE_BYTES; at += WRITE_CHUNK) {
        for (uint32_t i = 0; i < WRITE_CHUNK; ++i) {
            source[i] = uint8_t(at / WRITE_CHUNK + i);
        }
        const uint32_t n = FILE_BYTES - at < WRITE_CHUNK ? FILE_BYTES - at : WRITE_CHUNK;
        if (at == 0) {
            w.rewrite(source, n, r);
        } else {
            w.write(at, source, n, r);
        }
        if (!r.wait()) {
            return false;
        }
    }
    return true;
}

static void flush(lv_display_t* d, const lv_area_t*, uint8_t*) {
    lv_display_flush_ready(d);
}

static void runMain(void*) {
    while (!Serial) {
        vTaskDelay(10);
    }
    Serial.printf("stream test: rings of %ld KB in the PSRAM, %lu frames ahead\n", RING / 1024,
                  AHEAD_FRAMES);
    if (!storage.begin()) {
        Serial.printf("no card, stopped\n");
        vTaskSuspend(nullptr);
    }
    movie = storage.open(MOVIE);
    if (!writeMovie()) {
        Serial.printf("the movie file could not be written, stopped\n");
        vTaskSuspend(nullptr);
    }

    lv_init();
    lv_tick_set_cb([]() -> uint32_t { return xTaskGetTickCount(); });
    lv_display_t* d = lv_display_create(320, 240);
    lv_display_set_buffers(d, lvBuffers[0], lvBuffers[1], FRAME, LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(d, flush);
    static lv_image_dsc_t dsc = {};
    dsc.header = {LV_IMAGE_HEADER_MAGIC, LV_COLOR_FORMAT_RGB565_SWAPPED, 0, 320, 240, 640, 0};
    dsc.data_size = FRAME;
    dsc.data = frames[0];
    lv_obj_t* img = lv_image_create(lv_screen_active());
    lv_image_set_src(img, &dsc);
    FastLED.setExclusiveDriver<fl::Bus::FLEX_IO, 0>();
    FastLED.addLeds<WS2812B, 2, GRB>(leds, 240);
    for (uint32_t s = 0; s < STREAMS; ++s) {
        streams[s].fileAt = s * 2097152;
    }

    // The video keeps a ring of AHEAD_FRAMES frames. One request at a time fills it.
    FileRequest video(FilePriority::RAISED);   // video-player.md: the video player's priority
    bool videoPending = false;
    uint32_t tail = 0, ready = 0, nextIndex = 0, shown = 0, late = 0, minReady = AHEAD_FRAMES;
    uint32_t videoFailed = 0, videoStartUs = 0, videoReadMaxUs = 0, firstLateMs = 0, lastLateMs = 0;
    uint32_t runStart = 0;
    auto feedVideo = [&]() {
        if (videoPending && video.ended()) {
            const uint32_t took = micros() - videoStartUs;
            videoReadMaxUs = took > videoReadMaxUs ? took : videoReadMaxUs;
            if (video.status() == FileStatus::SUCCEEDED) {
                ready++;
                nextIndex = (nextIndex + 1) % FILE_FRAMES;
            } else {
                videoFailed++;
            }
            videoPending = false;
        }
        if (!videoPending && ready < AHEAD_FRAMES) {
            movie.read(nextIndex * FRAME, frames[(tail + ready) % AHEAD_FRAMES], FRAME, video);
            videoStartUs = micros();
            videoPending = true;
        }
    };

    // Every buffer fills before the clock starts, as a player fills them before its first frame.
    audioTimer.priority(208);
    audioTimer.begin(audioBlock, BLOCK_US);
    audioFilling = true;
    for (;;) {
        feedVideo();
        bool full = ready == AHEAD_FRAMES;
        for (const SoundStream& st : streams) {
            full = full && st.fill > RING - int32_t(2 * CHUNK);
        }
        if (full) {
            break;
        }
        yield();
    }
    audioRunning = true;

    const FileWriter log = storage.openWriter(LOG);
    const FileWriter save = storage.openWriter(SAVE);
    FileRequest logReq, saveReq;
    bool logPending = false, savePending = false;
    uint32_t logAt = 0, logStartUs = 0, saveStartUs = 0, logMaxUs = 0, saveMaxUs = 0;
    uint32_t logs = 0, saves = 0, writeFailed = 0;
    memset(logText, 'x', sizeof logText);
    logText[LOG_BYTES - 1] = '\n';
    log.rewrite(logText, 0, logReq);
    logReq.wait();

    const uint32_t start = millis();
    runStart = start;
    videoReadMaxUs = 0;
    audioReadMaxUs = 0;
    uint32_t nextFrame = start + FRAME_MS, nextLog = start + LOG_EVERY_MS,
             nextSave = start + SAVE_EVERY_MS, lastPassUs = micros(), passMaxUs = 0;
    while (millis() - start < RUN_MS) {
        feedVideo();
        const uint32_t now = millis();
        if (int32_t(now - nextFrame) >= 0) {
            nextFrame += FRAME_MS;
            if (ready > 0) {
                tail = (tail + 1) % AHEAD_FRAMES;
                ready--;
                shown++;
            } else {
                late++;
                if (firstLateMs == 0) {
                    firstLateMs = now - runStart;
                }
                lastLateMs = now - runStart;
            }
            if (ready < minReady) {
                minReady = ready;
            }
        }
        if (logPending && logReq.ended()) {
            const uint32_t us = micros() - logStartUs;
            logMaxUs = us > logMaxUs ? us : logMaxUs;
            if (logReq.status() == FileStatus::SUCCEEDED) {
                logAt += LOG_BYTES;
            } else {
                writeFailed++;
            }
            logs++;
            logPending = false;
        }
        if (!logPending && int32_t(now - nextLog) >= 0) {
            nextLog += LOG_EVERY_MS;
            logStartUs = micros();
            log.write(logAt, logText, LOG_BYTES, logReq);
            logPending = true;
        }
        if (savePending && saveReq.ended()) {
            const uint32_t us = micros() - saveStartUs;
            saveMaxUs = us > saveMaxUs ? us : saveMaxUs;
            writeFailed += saveReq.status() == FileStatus::SUCCEEDED ? 0 : 1;
            saves++;
            savePending = false;
        }
        if (!savePending && int32_t(now - nextSave) >= 0) {
            nextSave += SAVE_EVERY_MS;
            saveData[0] = uint8_t(saves);
            saveStartUs = micros();
            save.rewrite(saveData, SAVE_BYTES, saveReq);
            savePending = true;
        }
        lv_obj_invalidate(img);
        lv_refr_now(d);
        for (auto& l : leds) {
            l = CRGB::Black;   // the data goes out, nothing lights
        }
        FastLED.show();
        yield();
        const uint32_t nowUs = micros();
        passMaxUs = nowUs - lastPassUs > passMaxUs ? nowUs - lastPassUs : passMaxUs;
        lastPassUs = nowUs;
    }
    audioRunning = false;
    audioFilling = false;
    audioTimer.end();
    logReq.wait();
    saveReq.wait();
    video.wait();
    for (SoundStream& st : streams) {
        st.r.wait();
    }

    Serial.printf("\naudio: ");
    uint32_t under = 0, aFailed = 0;
    int32_t minFill = RING;
    for (const SoundStream& st : streams) {
        under += st.underruns;
        aFailed += st.failed;
        minFill = st.minFill < minFill ? st.minFill : minFill;
        Serial.printf("ring low %.0f ms, ", st.minFill / BYTES_PER_MS);
    }
    Serial.printf("\n       %lu blocks found their ring empty, %lu reads failed, a full ring holds "
                  "%.0f ms\n",
                  under, aFailed, RING / BYTES_PER_MS);
    Serial.printf("reads: longest video frame %.2f ms, longest audio chunk %.2f ms; late frames "
                  "from %lu ms to %lu ms into the run\n",
                  videoReadMaxUs / 1000.0f, audioReadMaxUs / 1000.0f, firstLateMs, lastLateMs);
    Serial.printf("video: %lu frames on time, %lu late, at least %lu frames ready ahead, %lu reads "
                  "failed\n",
                  shown, late, minReady, videoFailed);
    Serial.printf("writes: %lu log writes of %lu bytes, longest %.2f ms; %lu saves of %lu bytes, "
                  "longest %.2f ms; %lu failed\n",
                  logs, LOG_BYTES, logMaxUs / 1000.0f, saves, SAVE_BYTES, saveMaxUs / 1000.0f,
                  writeFailed);
    Serial.printf("main loop: longest pass %.2f ms\n", passMaxUs / 1000.0f);
    Serial.printf("\ndone\n");
    vTaskSuspend(nullptr);
}

void setup() {
    startClock();
    Serial.begin(115200);
    xTaskCreate(runMain, "main", 16384 / sizeof(StackType_t), nullptr, 1, nullptr);
    vTaskStartScheduler();
}

void loop() {}
