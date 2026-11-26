// The sounds program of docs/firmware/testing.md. It runs the sounds component on the real audio
// driver with sounds it writes to the card in the Teensy's socket, in the directory of a test game.
// A tap behind the master gain records every sample bound for the amplifier.
//
//   handover  a video's sound covers a mode's track, which goes on from the sample where it was
//             paused, the track glides out without a jump and each sound glides in
//   resume    the same with music, by ear (asks at the bench)
//
// Wiring: the DFR0954 on I²S2 with the speaker, as docs/parts/audio/design.md draws it, for
// resume. handover runs without it, since the tap records inside the Teensy. The tests leave their
// files in /games/tests/audio/ on the card.

#include <Arduino.h>
#include <AudioStream.h>
#include <math.h>
#include <string.h>
#include "components/file-system.h"
#include "components/sounds.h"
#include "core/event-queue.h"
#include "drivers/audio.h"
#include "drivers/storage.h"
#include "test-drivers.h"
#include "test-harness.h"

constexpr uint32_t LIMIT_S = 30;
constexpr uint32_t BENCH_LIMIT_S = 300;
constexpr uint32_t ASK_MS = 240000;
constexpr size_t TEXT = 128;
constexpr uint32_t N = SOUND_BLOCK_SAMPLES;
constexpr uint32_t RATE_HZ = 44100;   // audio model: f_s
constexpr uint32_t EVENTS = 16;

// the master gain as the Audio library applies it: the sample times the gain in 16.16, cut down
constexpr int32_t MULTIPLIER = int32_t(AudioDriver::MASTER_GAIN * 65536.0f);

// the sounds of handover: a rising ramp as the track, a falling one as the video's sound
constexpr uint32_t RAMP_FIRST = 1000;   // the ramp's first sample, which stays nonzero at the tap
constexpr uint32_t RAMP_SAMPLES = 20000;   // one pass of the ramp, after which it starts over
constexpr uint32_t TRACK_SAMPLES = RATE_HZ * 2;
constexpr uint32_t VIDEO_SAMPLES = RATE_HZ / 4;
constexpr uint32_t TRACK_MS = 300;   // how long the track plays before the video's sound
constexpr uint32_t AFTER_MS = 600;   // longer than the video's sound and the track's return

// the sounds of resume: a melody as the track, a tone as the video's sound, at half scale
constexpr uint32_t MELODY_SAMPLES = RATE_HZ * 8;
constexpr uint32_t NOTE_SAMPLES = RATE_HZ / 4;
constexpr uint32_t TONE_SAMPLES = RATE_HZ * 3 / 2;
constexpr uint32_t FADE_SAMPLES = RATE_HZ / 200;   // 5 ms at either end of a note
constexpr float HALF_SCALE = 0.5f * 32767;
constexpr float TONE_HZ = 880.0f;
constexpr uint32_t MELODY_MS = 2500;   // how long the melody plays before the tone
constexpr uint32_t REST_MS = 4000;     // the tone and the melody's return

// the tap's record: 2^18 samples, 5.9 s
constexpr uint32_t RECORD = 1u << 18;
constexpr uint32_t RECORD_MASK = RECORD - 1;

// a jump between neighbors larger than a glide's step from the loudest sample
constexpr int32_t JUMP_MAX = 200;
constexpr uint32_t SETTLE_MS = 30;   // longer than the card's first read and a few blocks

constexpr char GAME[] = "tests";
constexpr char TRACK[] = "track.wav";
constexpr char VIDEO[] = "video.wav";
constexpr char MELODY[] = "melody.wav";
constexpr char TONE[] = "tone.wav";
constexpr char DIRECTORY[] = "/games/tests/audio/";
constexpr Layer BASE = {0, 1};   // the mode whose track plays

constexpr uint32_t FILE_BYTES = MELODY_SAMPLES * 2 + 64;

// the programs' shared PSRAM
static_assert(RECORD * sizeof(int16_t) + FILE_BYTES <= test::SCRATCH_BYTES, "the buffers fit");
static int16_t* const samples = reinterpret_cast<int16_t*>(test::scratch());
static uint8_t* const fileBuffer = test::scratch() + RECORD * sizeof(int16_t);

// The audio program names its tap Tap as well, so this one stays inside this file.
namespace {

/// Records the blocks bound for the amplifier into a ring in the PSRAM, an update without a block
/// as silence, so the record keeps time.
class Tap : public AudioStream {
public:
    Tap() : AudioStream(1, inputs) {}

    void update() override {
        audio_block_t* b = receiveReadOnly(0);
        if (recording) {
            const uint32_t at = count;
            for (uint32_t i = 0; i < N; ++i) {
                samples[(at + i) & RECORD_MASK] = b != nullptr ? b->data[i] : 0;
            }
            count = at + N;
        }
        if (b != nullptr) {
            release(b);
        }
    }

    /// Starts the record from sample 0.
    void start() {
        recording = false;
        count = 0;
        recording = true;
    }

    void stop() { recording = false; }

    /// Gets the samples recorded since start().
    uint32_t recorded() const { return count; }

    /// Gets a recorded sample, valid while fewer than RECORD samples have followed it.
    int16_t at(uint32_t i) const { return samples[i & RECORD_MASK]; }

private:
    audio_block_t* inputs[1];
    volatile bool recording = false;
    volatile uint32_t count = 0;
};

}   // namespace

/// The host's calls to the file system and the sounds.
class GameHost {
public:
    static void use(FileSystem& files, const char* game) { files.use(game); }
    static void reset(Sounds& sounds) { sounds.reset(); }
};

/// The video player's calls to the sounds.
class VideoPlayer {
public:
    static void sound(Sounds& sounds, const char* name) { sounds.videoSound(name, false); }
    static bool sounding(const Sounds& sounds) { return sounds.videoSounding(); }
};

using test::audio;
using test::files;
using test::sounds;
static Tap* tap = nullptr;
static bool written = false;
static uint32_t musicEnded = 0;   // the MUSIC_ENDED events pump() saw
static char seen[TEXT];
static char wanted[TEXT];

// Gets the rising ramp's sample i, which starts over after RAMP_SAMPLES.
static int16_t rampAt(uint32_t i) {
    return int16_t(RAMP_FIRST + i % RAMP_SAMPLES);
}

// Gets the falling ramp's sample i.
static int16_t rampDownAt(uint32_t i) {
    return int16_t(-int32_t(RAMP_FIRST) - int32_t(i % RAMP_SAMPLES));
}

// Gets a note of a fixed length with fades at either end.
static int16_t note(float hz, uint32_t i, uint32_t length) {
    const uint32_t edge = min(i, length - 1 - i);
    const float fade = edge < FADE_SAMPLES ? float(edge) / float(FADE_SAMPLES) : 1.0f;
    return int16_t(HALF_SCALE * fade * sinf(2.0f * float(M_PI) * hz * float(i) / RATE_HZ));
}

// the melody of resume: C major up and down, a quarter second a note
static int16_t melodyAt(uint32_t i) {
    static const float HZ[] = {261.63f, 293.66f, 329.63f, 349.23f,
                               392.00f, 440.00f, 493.88f, 523.25f};
    constexpr uint32_t NOTES = sizeof HZ / sizeof HZ[0];
    const uint32_t k = (i / NOTE_SAMPLES) % (2 * NOTES - 2);
    const uint32_t step = k < NOTES ? k : 2 * NOTES - 2 - k;
    return note(HZ[step], i % NOTE_SAMPLES, NOTE_SAMPLES);
}

static int16_t toneAt(uint32_t i) {
    return note(TONE_HZ, i, TONE_SAMPLES);
}

// Gets a sample as the master gain leaves it.
static int16_t gained(int32_t s) {
    return int16_t((s * MULTIPLIER) >> 16);
}

// Gets a sample of the first block of a sound, which glides in from 0 at full volume, as the
// channel computes it.
static int16_t glidedIn(int16_t s, uint32_t i) {
    const float scaled = float(s) * float(i + 1) / float(N);
    return int16_t(scaled < 0 ? scaled - 0.5f : scaled + 0.5f);
}

static void put16(uint8_t* p, uint16_t v) {
    p[0] = uint8_t(v & 0xFF);
    p[1] = uint8_t(v >> 8);
}

static void put32(uint8_t* p, uint32_t v) {
    put16(p, uint16_t(v & 0xFFFF));
    put16(p + 2, uint16_t(v >> 16));
}

// Writes a WAV file into the test game's audio directory, with samples from a function. Returns
// false where the write fails.
static bool writeWav(const char* name, uint32_t count, int16_t (*sample)(uint32_t)) {
    uint8_t* p = fileBuffer;
    memcpy(p, "RIFF", 4);
    put32(p + 4, 36 + count * 2);
    memcpy(p + 8, "WAVEfmt ", 8);
    put32(p + 16, 16);
    put16(p + 20, 1);
    put16(p + 22, 1);
    put32(p + 24, RATE_HZ);
    put32(p + 28, RATE_HZ * 2);
    put16(p + 32, 2);
    put16(p + 34, 16);
    memcpy(p + 36, "data", 4);
    put32(p + 40, count * 2);
    for (uint32_t i = 0; i < count; ++i) {
        put16(p + 44 + 2 * i, uint16_t(sample(i)));
    }
    char path[FILE_PATH_BYTES];
    snprintf(path, sizeof path, "%s%s", DIRECTORY, name);
    FileRequest r;
    storage.openWriter(path).rewrite(fileBuffer, 44 + count * 2, r);
    return r.wait();
}

// Begins the drivers and the sounds, writes the sounds once, and skips the test without a card.
static bool ready() {
    if (!storage.begin()) {
        test::skip("no card in the socket");
        return false;
    }
    if (!audio.begin()) {
        test::fail("the audio driver's begin() false", "true");
        return false;
    }
    if (tap == nullptr) {
        // Made after the driver's stages, the tap is updated behind the master gain and gets each
        // block in the update that computed it.
        static Tap made;
        static AudioConnection wire(audio.output(), 0, made, 0);
        tap = &made;
    }
    if (!written) {
        written =
            writeWav(TRACK, TRACK_SAMPLES, rampAt) && writeWav(VIDEO, VIDEO_SAMPLES, rampDownAt) &&
            writeWav(MELODY, MELODY_SAMPLES, melodyAt) && writeWav(TONE, TONE_SAMPLES, toneAt);
        if (!written) {
            test::fail("writing the sounds failed", "they reach the card");
            return false;
        }
    }
    GameHost::use(files, GAME);
    Sounds::Settings settings;
    settings.musicVolume = 1;
    settings.effectsVolume = 0;
    sounds.configure(settings);
    sounds.begin();
    GameHost::reset(sounds);
    delay(SETTLE_MS);
    DriverEvent drop[EVENTS];
    while (events.read(drop, EVENTS) > 0) {
    }
    musicEnded = 0;
    return true;
}

// Hands the driver's events to the sounds for a while, as the host would.
static void pump(uint32_t ms) {
    const uint32_t t0 = millis();
    while (millis() - t0 < ms) {
        DriverEvent e[EVENTS];
        const uint32_t n = events.read(e, EVENTS);
        for (uint32_t i = 0; i < n; ++i) {
            PinballEvents out;
            sounds.translate(e[i], out);
            for (uint8_t k = 0; k < out.count; ++k) {
                musicEnded = musicEnded + (out.events[k].type == EventType::MUSIC_ENDED ? 1 : 0);
            }
        }
        delay(1);
    }
}

// Gets the first recorded sample at or after from that is not 0, or the end of the record.
static uint32_t firstSound(uint32_t from) {
    uint32_t i = from;
    while (i < tap->recorded() && tap->at(i) == 0) {
        i = i + 1;
    }
    return i;
}

// Checks that the record from at holds count samples of a sound that starts at sample first,
// glides in across its first block and then follows sample, all behind the master gain.
static bool follows(uint32_t at, uint32_t count, int16_t (*sample)(uint32_t), uint32_t first,
                    const char* what) {
    for (uint32_t i = 0; i < count; ++i) {
        const int16_t s = sample(first + i);
        const int16_t want = gained(i < N ? glidedIn(s, i) : s);
        const int16_t got = tap->at(at + i);
        if (got != want) {
            snprintf(seen, sizeof seen, "%s: sample %lu is %d", what, i, got);
            snprintf(wanted, sizeof wanted, "%d", want);
            test::fail(seen, wanted);
            return false;
        }
    }
    return true;
}

// Gets the largest jump between neighbors in the record from from to to.
static int32_t largestJump(uint32_t from, uint32_t to) {
    int32_t most = 0;
    for (uint32_t i = from + 1; i < to; ++i) {
        const int32_t d = int32_t(tap->at(i)) - int32_t(tap->at(i - 1));
        most = max(most, d < 0 ? -d : d);
    }
    return most;
}

static void testHandover() {
    if (!ready()) {
        return;
    }
    tap->start();
    sounds.music(BASE, TRACK);
    pump(TRACK_MS);
    // The sounds pause the track where the driver's position stands right before the video's
    // sound.
    const uint32_t paused = audio.position(Sounds::MUSIC).played();
    VideoPlayer::sound(sounds, VIDEO);
    pump(AFTER_MS);
    tap->stop();
    const bool sounding = VideoPlayer::sounding(sounds);
    const bool ended = sounds.musicEnded(BASE);
    GameHost::reset(sounds);
    if (sounding) {
        test::fail("the video's sound still plays", "its end handed to the sounds");
        return;
    }
    if (musicEnded != 0 || ended) {
        test::fail("the track ended", "the track playing on, and no MUSIC_ENDED");
        return;
    }
    const uint32_t trackAt = firstSound(0);
    if (!follows(trackAt, paused, rampAt, 0, "the track")) {
        return;
    }
    // The video's sound starts at the first negative sample.
    uint32_t videoAt = trackAt;
    while (videoAt < tap->recorded() && tap->at(videoAt) >= 0) {
        videoAt = videoAt + 1;
    }
    if (!follows(videoAt, VIDEO_SAMPLES, rampDownAt, 0, "the video's sound")) {
        return;
    }
    // The track goes on at the first positive sample behind the video's sound.
    uint32_t backAt = videoAt + VIDEO_SAMPLES;
    while (backAt < tap->recorded() && tap->at(backAt) <= 0) {
        backAt = backAt + 1;
    }
    Serial.printf("paused at track sample %lu\n", paused);
    if (!follows(backAt, 4 * N, rampAt, paused, "the track going on")) {
        return;
    }
    // follows() has checked every glide in sample by sample, so the glide out of the track is left,
    // up to the video's first block. The video's sound ends at the last sample of its file.
    const int32_t jump = largestJump(trackAt + paused - 1, videoAt + N);
    if (jump > JUMP_MAX) {
        snprintf(seen, sizeof seen, "a jump of %ld between neighbors", jump);
        snprintf(wanted, sizeof wanted, "at most %ld", JUMP_MAX);
        test::fail(seen, wanted);
        return;
    }
    test::pass();
}

static void testResume() {
    if (!ready()) {
        return;
    }
    sounds.music(BASE, MELODY);
    pump(MELODY_MS);
    VideoPlayer::sound(sounds, TONE);
    pump(REST_MS);
    GameHost::reset(sounds);
    const test::Answer a = test::ask(
        "did the melody stop for the high tone and go on from the note where it stopped, without "
        "a click?",
        ASK_MS);
    if (a != test::Answer::YES) {
        test::fail(a == test::Answer::NO ? "a click or the wrong place heard" : "no answer",
                   "the melody going on cleanly");
        return;
    }
    test::pass();
}

static const Test TESTS[] = {
    {"sounds.handover", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testHandover, nullptr},
    {"sounds.resume", RunsOn::BENCH, BENCH_LIMIT_S, BENCH_LIMIT_S, testResume, nullptr},
};

extern const Program SOUNDS = {TESTS, sizeof TESTS / sizeof TESTS[0]};
