// The pictures, videos and sounds of the screen and video player programs, which the tests write
// into the test game's directory /games/tests/ on the card. A file that already stands there with
// its size and its first bytes is left as it is, so only the first run spends the time writing.
//
// loop.rgb shows a ball that circles once per pass over a dark blue gradient. Frame k carries the
// number k in its top left pixel, high byte first, so a test reads which frame LVGL draws from the
// pixel it sends.
#pragma once
#include <Arduino.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "drivers/display.h"
#include "drivers/storage.h"

namespace media {

constexpr char GAME[] = "tests";
constexpr char DIRECTORY[] = "/games/tests/";
constexpr uint16_t W = Display::WIDTH;
constexpr uint16_t H = Display::HEIGHT;
constexpr uint32_t FULL_BYTES = uint32_t(W) * H * 2;
constexpr uint32_t RATE_HZ = 44100;        // audio model: f_s
constexpr uint32_t FRAME_SAMPLES = 1764;   // 44.1 kHz over 25 frames a second
constexpr uint32_t FRAMES_PER_S = 25;      // display model: f_movie
constexpr uint32_t LOOP_FRAMES = 100;      // loop.rgb, four seconds
constexpr uint32_t VIEWPORT_FRAMES = 50;   // viewport.rgb, two seconds
constexpr uint16_t VIEWPORT_W = 160;
constexpr uint16_t VIEWPORT_H = 120;
constexpr uint32_t SYNC_FRAMES = 100;   // sync.rgb, a flash every second
constexpr uint16_t ICON_W = 64;
constexpr uint16_t ICON_H = 48;
constexpr uint8_t MAGIC = 0x19;            // LV_IMAGE_HEADER_MAGIC
constexpr uint8_t RGB565_SWAPPED = 0x1B;   // LVGL's color formats
constexpr uint8_t RGB565A8 = 0x14;
constexpr size_t HEADER_BYTES = 12;
constexpr size_t WAV_HEADER_BYTES = 44;
// loop.wav and the three effects of frames, each a constant, so a test tells the four channels
// apart in their sum: the video's sound in the lowest 10 bits, effect c in bit 9 + c
constexpr int16_t LOOP_SAMPLE = 512;
constexpr int16_t EFFECT_SAMPLES[3] = {1024, 2048, 4096};
constexpr uint32_t EFFECT_SAMPLE_COUNT = RATE_HZ;   // one second, which the driver loops
constexpr uint32_t CLICK_SAMPLES = RATE_HZ / 50;    // a click of 20 ms
constexpr float CLICK_HZ = 1000.0f;
constexpr float HALF_SCALE = 0.5f * 32767;
constexpr uint32_t MUSIC_SAMPLES = RATE_HZ * 8;   // music.wav, looped by the sounds
constexpr uint32_t NOTE_SAMPLES = RATE_HZ / 4;
constexpr size_t CHUNK_BYTES = 8192;
constexpr size_t PROBE_BYTES = 256;    // the first bytes compared with a file on the card
constexpr int32_t BALL_R = 18;         // loop.rgb: the ball's radius
constexpr int32_t ORBIT_R = 70;        // the circle it runs on, around the screen's center
constexpr int32_t SMALL_BALL_R = 10;   // viewport.rgb   // one write

/// The bytes of one write.
extern uint8_t chunk[CHUNK_BYTES];

/// Gets the full path of a file of the test game.
inline void pathOf(const char* name, char (&path)[FILE_PATH_BYTES]) {
    snprintf(path, sizeof path, "%s%s", DIRECTORY, name);
}

/// Gets whether a file of the test game stands on the card at a size.
template <typename ByteAt> bool present(const char* name, uint32_t size, ByteAt byteAt) {
    char path[FILE_PATH_BYTES];
    pathOf(name, path);
    static uint8_t probe[PROBE_BYTES];
    FileRequest r;
    storage.open(path).read(0, probe, sizeof probe, r);
    r.wait();
    if (r.fileSize() != size) {
        return false;
    }
    for (uint32_t i = 0; i < r.bytes(); ++i) {
        if (probe[i] != byteAt(i)) {
            return false;
        }
    }
    return true;
}

/// Writes a file of the test game whose byte i comes from @p byteAt, chunk by chunk.
///
/// @return  False where a write fails.
template <typename ByteAt> bool write(const char* name, uint32_t size, ByteAt byteAt) {
    if (present(name, size, byteAt)) {
        return true;
    }
    char path[FILE_PATH_BYTES];
    pathOf(name, path);
    const FileWriter file = storage.openWriter(path);
    FileRequest r;
    file.rewrite(chunk, 0, r);
    if (!r.wait()) {
        return false;
    }
    for (uint32_t at = 0; at < size; at += CHUNK_BYTES) {
        const uint32_t n = size - at < CHUNK_BYTES ? size - at : CHUNK_BYTES;
        for (uint32_t i = 0; i < n; ++i) {
            chunk[i] = byteAt(at + i);
        }
        file.write(at, chunk, n, r);
        if (!r.wait()) {
            return false;
        }
    }
    return true;
}

/// Gets byte @p i of a picture file's header.
inline uint8_t headerByte(uint32_t i, uint8_t format, uint16_t width, uint16_t height) {
    const uint16_t fields[] = {0, width, height, uint16_t(width * 2), 0};
    if (i == 0) {
        return MAGIC;
    }
    if (i == 1) {
        return format;
    }
    const uint16_t v = fields[(i - 2) / 2];
    return (i % 2 == 0) ? uint8_t(v & 0xFF) : uint8_t(v >> 8);
}

/// Gets an RGB565 color from 8-bit channels.
inline uint16_t rgb(uint32_t r, uint32_t g, uint32_t b) {
    return uint16_t((r >> 3) << 11 | (g >> 2) << 5 | (b >> 3));
}

/// Gets the color of the background at a pixel: a calm gradient from deep blue to violet.
inline uint16_t backgroundAt(uint32_t x, uint32_t y) {
    return rgb(30 + x * 70 / (W - 1), 20 + y * 40 / (H - 1), 90 + y * 60 / (H - 1));
}

/// Gets the color of a ball of radius @p r centered at @p cx, @p cy over a background color, with
/// a highlight toward its top left.
inline uint16_t ballAt(int32_t x, int32_t y, int32_t cx, int32_t cy, int32_t r, uint16_t back) {
    const int32_t dx = x - cx;
    const int32_t dy = y - cy;
    if (dx * dx + dy * dy >= r * r) {
        return back;
    }
    const int32_t hx = dx + r / 3;
    const int32_t hy = dy + r / 3;
    const int32_t shade = 255 - (hx * hx + hy * hy) * 120 / (r * r);
    const uint32_t level = uint32_t(shade < 120 ? 120 : shade);
    return rgb(level, level * 210 / 255, level / 5);
}

/// Gets the color of frame k of loop.rgb at a pixel: the ball on its circle over a dark blue
/// gradient, the frame's number at the top left.
inline uint16_t loopAt(uint32_t k, uint32_t x, uint32_t y) {
    if (x == 0 && y == 0) {
        return uint16_t(k);
    }
    static uint32_t frame = UINT32_MAX;
    static int32_t cx = 0;
    static int32_t cy = 0;
    if (frame != k) {
        const float angle = 2.0f * float(M_PI) * float(k) / float(LOOP_FRAMES);
        cx = int32_t(W / 2 + ORBIT_R * cosf(angle));
        cy = int32_t(H / 2 + ORBIT_R * sinf(angle));
        frame = k;
    }
    return ballAt(int32_t(x), int32_t(y), cx, cy, BALL_R,
                  rgb(10, 20 + y * 30 / (H - 1), 60 + y * 60 / (H - 1)));
}

/// Gets a sample of a WAV file's header for @p count samples at 44.1 kHz.
inline uint8_t wavHeaderByte(uint32_t i, uint32_t count) {
    static const char* const TAGS[] = {"RIFF", "WAVE", "fmt ", "data"};
    const uint32_t words[] = {
        0,           36 + count * 2,  0, 0,        16, 1 | (1u << 16), RATE_HZ,
        RATE_HZ * 2, 2 | (16u << 16), 0, count * 2};
    const uint32_t word = i / 4;
    if (word == 0 || word == 2 || word == 3 || word == 9) {
        const int tag = word == 0 ? 0 : (word == 2 ? 1 : (word == 3 ? 2 : 3));
        return uint8_t(TAGS[tag][i % 4]);
    }
    return uint8_t(words[word] >> (8 * (i % 4)));
}

/// Writes a WAV file of the test game whose sample i comes from @p sampleAt.
template <typename SampleAt> bool writeWav(const char* name, uint32_t count, SampleAt sampleAt) {
    return write(name, WAV_HEADER_BYTES + count * 2, [&](uint32_t i) -> uint8_t {
        if (i < WAV_HEADER_BYTES) {
            return wavHeaderByte(i, count);
        }
        const uint16_t s = uint16_t(sampleAt((i - WAV_HEADER_BYTES) / 2));
        return (i - WAV_HEADER_BYTES) % 2 == 0 ? uint8_t(s & 0xFF) : uint8_t(s >> 8);
    });
}

/// Writes a video of the test game whose pixel x, y of frame k comes from @p colorAt.
template <typename ColorAt>
bool writeVideo(const char* name, uint32_t frames, uint16_t width, uint16_t height,
                ColorAt colorAt) {
    const uint32_t frameBytes = uint32_t(width) * height * 2;
    return write(name, frames * frameBytes, [&](uint32_t i) -> uint8_t {
        const uint32_t k = i / frameBytes;
        const uint32_t p = (i % frameBytes) / 2;
        const uint16_t c = colorAt(k, p % width, p / width);
        return i % 2 == 0 ? uint8_t(c >> 8) : uint8_t(c & 0xFF);
    });
}

/// Writes the pictures: background.img, short.img cut off halfway through its pixels, and
/// icon.img with transparent corners.
inline bool writePictures() {
    const uint32_t full = HEADER_BYTES + FULL_BYTES;
    const auto background = [](uint32_t i) -> uint8_t {
        if (i < HEADER_BYTES) {
            return headerByte(i, RGB565_SWAPPED, W, H);
        }
        const uint32_t p = (i - HEADER_BYTES) / 2;
        const uint16_t c = backgroundAt(p % W, p / W);
        return (i - HEADER_BYTES) % 2 == 0 ? uint8_t(c >> 8) : uint8_t(c & 0xFF);
    };
    const uint32_t colors = uint32_t(ICON_W) * ICON_H * 2;
    const auto icon = [&](uint32_t i) -> uint8_t {
        if (i < HEADER_BYTES) {
            return headerByte(i, RGB565A8, ICON_W, ICON_H);
        }
        const uint32_t j = i - HEADER_BYTES;
        if (j < colors) {
            // a yellow icon, its colors low byte first
            const uint16_t c = 0xFFE0;
            return j % 2 == 0 ? uint8_t(c & 0xFF) : uint8_t(c >> 8);
        }
        const uint32_t p = j - colors;
        const int32_t dx = int32_t(p % ICON_W) - ICON_W / 2;
        const int32_t dy = int32_t(p / ICON_W) - ICON_H / 2;
        return dx * dx + dy * dy < (ICON_H / 2) * (ICON_H / 2) ? 0xFF : 0x00;
    };
    return write("background.img", full, background) &&
           write("short.img", HEADER_BYTES + FULL_BYTES / 2, background) &&
           write("icon.img", HEADER_BYTES + colors + uint32_t(ICON_W) * ICON_H, icon);
}

/// Writes loop.rgb and loop.wav, the sound as long as the video.
inline bool writeLoop() {
    return writeVideo("video/loop.rgb", LOOP_FRAMES, W, H, loopAt) &&
           writeWav("audio/loop.wav", LOOP_FRAMES * FRAME_SAMPLES,
                    [](uint32_t) { return LOOP_SAMPLE; });
}

/// Writes the three effects of frames, 1.wav to 3.wav.
inline bool writeEffects() {
    const char* const names[] = {"audio/1.wav", "audio/2.wav", "audio/3.wav"};
    for (uint8_t c = 0; c < 3; ++c) {
        const int16_t s = EFFECT_SAMPLES[c];
        if (!writeWav(names[c], EFFECT_SAMPLE_COUNT, [s](uint32_t) { return s; })) {
            return false;
        }
    }
    return true;
}

/// Gets a sample of a click at the start of every second, silence between.
inline int16_t clickAt(uint32_t i) {
    const uint32_t t = i % RATE_HZ;
    if (t >= CLICK_SAMPLES) {
        return 0;
    }
    return int16_t(HALF_SCALE * sinf(2.0f * float(M_PI) * CLICK_HZ * float(t) / RATE_HZ));
}

/// Gets a sample of the music: C major up and down, a quarter second a note, at a quarter scale.
inline int16_t musicAt(uint32_t i) {
    static const float HZ[] = {261.63f, 293.66f, 329.63f, 349.23f,
                               392.00f, 440.00f, 493.88f, 523.25f};
    constexpr uint32_t NOTES = sizeof HZ / sizeof HZ[0];
    const uint32_t k = (i / NOTE_SAMPLES) % (2 * NOTES - 2);
    const uint32_t step = k < NOTES ? k : 2 * NOTES - 2 - k;
    const uint32_t t = i % NOTE_SAMPLES;
    const uint32_t edge = t < NOTE_SAMPLES - 1 - t ? t : NOTE_SAMPLES - 1 - t;
    const float fade = edge < 220 ? float(edge) / 220.0f : 1.0f;
    return int16_t(0.5f * HALF_SCALE * fade *
                   sinf(2.0f * float(M_PI) * HZ[step] * float(t) / RATE_HZ));
}

/// Writes viewport.rgb, sync.rgb with sync.wav, and music.wav for the bench tests.
inline bool writeBenchMedia() {
    return writeVideo("video/viewport.rgb", VIEWPORT_FRAMES, VIEWPORT_W, VIEWPORT_H,
                      [](uint32_t k, uint32_t x, uint32_t y) -> uint16_t {
                          // a small ball that rolls across and back once per pass
                          const uint32_t half = VIEWPORT_FRAMES / 2;
                          const uint32_t step = k < half ? k : VIEWPORT_FRAMES - k;
                          const int32_t span = VIEWPORT_W - 2 * SMALL_BALL_R;
                          const int32_t cx = SMALL_BALL_R + int32_t(step) * span / int32_t(half);
                          return ballAt(int32_t(x), int32_t(y), cx, VIEWPORT_H / 2, SMALL_BALL_R,
                                        rgb(20, 70 + y * 60 / (VIEWPORT_H - 1), 60));
                      }) &&
           writeVideo("video/sync.rgb", SYNC_FRAMES, W, H,
                      [](uint32_t k, uint32_t, uint32_t) -> uint16_t {
                          return k % FRAMES_PER_S == 0 ? 0xFFFF : 0x0000;
                      }) &&
           writeWav("audio/sync.wav", SYNC_FRAMES * FRAME_SAMPLES, clickAt) &&
           writeWav("audio/music.wav", MUSIC_SAMPLES, musicAt);
}

}   // namespace media
