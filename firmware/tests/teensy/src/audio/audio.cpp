// The audio program of docs/firmware/testing.md. It tests the audio driver with sounds it writes
// to the card in the Teensy's socket. A tap behind the master gain records every sample bound for
// the amplifier, so the tests compare what the amplifier gets with the files.
//
//   samples   every sample of a file reaches the tap once and in order, behind any chunks
//   mix       four full-scale sounds together stay within full scale times the master gain
//   loop      a sound that loops runs on from its first sample after its last, with no gap
//   replace   a new sound glides the old one out and starts from its first sample
//   glide     starting, a new volume and stopping glide with no jump between neighbors
//   volume    the tap records the samples scaled by the volume, held inside 0 to 1
//   resume    a sound goes on from the position noted before another sound played
//   ended     SOUND_ENDED comes once for a sound that plays out, and for no other
//   missing   a missing file ends at once while the other channels play on
//   starved   a long write to the card leaves only silence and no stray sample
//   by-ear    music and effects play without dropouts or clicks (asks at the bench)
//
// Wiring: the DFR0954 on I²S2 with the speaker, as docs/parts/audio/design.md draws it. Without
// it the tests run all the same, since the tap records inside the Teensy. The tests leave their
// files in /tests/audio/ on the card.

#include <Arduino.h>
#include <AudioStream.h>
#include <math.h>
#include <string.h>
#include "core/event-queue.h"
#include "drivers/audio.h"
#include "drivers/storage.h"
#include "test-drivers.h"
#include "test-harness.h"

constexpr uint32_t LIMIT_S = 30;
constexpr uint32_t SOAK_LIMIT_S = 120;
constexpr uint32_t BENCH_LIMIT_S = 600;
constexpr uint32_t ASK_MS = 300000;
constexpr size_t TEXT = 128;
constexpr uint32_t N = SOUND_BLOCK_SAMPLES;
constexpr uint32_t RATE_HZ = 44100;   // audio model: f_s
constexpr uint32_t EVENTS = 16;

// the master gain as the Audio library applies it: the sample times the gain in 16.16, cut down
constexpr int32_t MULTIPLIER = int32_t(AudioDriver::MASTER_GAIN * 65536.0f);

// the sounds the tests write
constexpr uint32_t RAMP_FIRST = 1000;   // the ramp's first sample, which stays nonzero at the tap
constexpr uint32_t RAMP_SAMPLES = 20000;   // the ramp's length, 0.45 s
constexpr int16_t LEVEL = 20000;           // the sample of the constant sound
constexpr uint32_t LEVEL_SAMPLES = RATE_HZ * 2;
constexpr uint32_t FULL_SAMPLES = RATE_HZ;
constexpr uint32_t SHORT_SAMPLES = RATE_HZ / 5;
constexpr uint32_t WAV_HEAD_BYTES = 44;
constexpr uint32_t FILE_BYTES = 262144;

// the tap's record: 2^19 samples, 11.9 s
constexpr uint32_t RECORD = 1u << 19;
constexpr uint32_t RECORD_MASK = RECORD - 1;

// a jump between neighbors larger than a glide's step from the loudest sample
constexpr int32_t JUMP_MAX = 200;
constexpr uint32_t SETTLE_MS = 30;     // longer than the card's first read and a few blocks
constexpr uint32_t ENDED_MS = 50;      // a sound that ends at once reports within this
constexpr uint32_t BIG_WRITES = 128;   // 8 MB to the card in chunks of FILE_BYTES / 4
constexpr uint32_t WRITE_BYTES = 65536;

constexpr char RAMP[] = "/tests/audio/ramp.wav";
constexpr char RAMP_CHUNKS[] = "/tests/audio/ramp-chunks.wav";
constexpr char RAMP_DOWN[] = "/tests/audio/ramp-down.wav";
constexpr char LEVEL_FILE[] = "/tests/audio/level.wav";
constexpr char FULL[] = "/tests/audio/full.wav";
constexpr char SHORT[] = "/tests/audio/short.wav";
constexpr char TONES[] = "/tests/audio/tones.wav";
constexpr char BLIP[] = "/tests/audio/blip.wav";
constexpr char MISSING[] = "/tests/audio/missing.wav";
constexpr char BIG[] = "/tests/audio/big.bin";

// the programs' shared PSRAM
static_assert(RECORD * sizeof(int16_t) + FILE_BYTES <= test::SCRATCH_BYTES, "the buffers fit");
static int16_t* const samples = reinterpret_cast<int16_t*>(test::scratch());

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

using test::audio;
static uint8_t* const fileBuffer = test::scratch() + RECORD * sizeof(int16_t);
static Tap* tap = nullptr;
static bool written = false;
static char seen[TEXT];
static char wanted[TEXT];

// Gets the ramp's sample i, counting up from RAMP_FIRST.
static int16_t rampAt(uint32_t i) {
    return int16_t(RAMP_FIRST + i % RAMP_SAMPLES);
}

// Gets the falling ramp's sample i.
static int16_t rampDownAt(uint32_t i) {
    return int16_t(-int32_t(RAMP_FIRST) - int32_t(i % RAMP_SAMPLES));
}

// Gets a sample as the master gain leaves it.
static int16_t gained(int32_t s) {
    return int16_t((s * MULTIPLIER) >> 16);
}

// Gets a sample of the first block of a sound, which glides in from 0 at full volume, as the
// channel computes it.
static int16_t glidedIn(int16_t s, uint32_t i) {
    const float gain = 0.0f + (1.0f - 0.0f) * float(i + 1) / float(N);
    const float scaled = float(s) * gain;
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

// Writes a WAV file with samples from a function, and chunks of padding ahead of the data chunk
// where extra is set. Returns false where the write fails.
static bool writeWav(const char* path, uint32_t count, int16_t (*sample)(uint32_t), bool extra) {
    uint8_t* p = fileBuffer;
    memcpy(p, "RIFF", 4);
    memcpy(p + 8, "WAVEfmt ", 8);
    put32(p + 16, 16);
    put16(p + 20, 1);
    put16(p + 22, 1);
    put32(p + 24, RATE_HZ);
    put32(p + 28, RATE_HZ * 2);
    put16(p + 32, 2);
    put16(p + 34, 16);
    p = p + 36;
    if (extra) {
        // A LIST chunk of an odd size with its pad byte, then a JUNK chunk.
        memcpy(p, "LIST", 4);
        put32(p + 4, 27);
        memset(p + 8, 'x', 28);
        p = p + 36;
        memcpy(p, "JUNK", 4);
        put32(p + 4, 500);
        memset(p + 8, 0, 500);
        p = p + 508;
    }
    memcpy(p, "data", 4);
    put32(p + 4, count * 2);
    p = p + 8;
    for (uint32_t i = 0; i < count; ++i) {
        put16(p + 2 * i, uint16_t(sample(i)));
    }
    p = p + 2 * count;
    const uint32_t bytes = uint32_t(p - fileBuffer);
    put32(fileBuffer + 4, bytes - 8);
    FileRequest r;
    storage.openWriter(path).rewrite(fileBuffer, bytes, r);
    return r.wait();
}

static int16_t levelAt(uint32_t) {
    return LEVEL;
}

static int16_t fullAt(uint32_t i) {
    return int16_t((i / 50) % 2 == 0 ? 32767 : -32768);
}

// the tones of by-ear: A4, C#5 and E5 for 400 ms each, then 400 ms of silence, at half scale with
// 5 ms fades
static int16_t toneAt(uint32_t i) {
    static const float HZ[] = {440.0f, 554.37f, 659.26f};
    const uint32_t note = i / (RATE_HZ * 2 / 5);
    if (note >= 3) {
        return 0;
    }
    const uint32_t k = i % (RATE_HZ * 2 / 5);
    const uint32_t edge = min(k, RATE_HZ * 2 / 5 - 1 - k);
    const float fade = edge < RATE_HZ / 200 ? float(edge) / float(RATE_HZ / 200) : 1.0f;
    return int16_t(0.5f * fade * sinf(2.0f * float(M_PI) * HZ[note] * float(k) / RATE_HZ) * 32767);
}

// a blip of 100 ms at 1 kHz, at a quarter of full scale with 5 ms fades
static int16_t blipAt(uint32_t i) {
    const uint32_t length = RATE_HZ / 10;
    const uint32_t edge = min(i, length - 1 - i);
    const float fade = edge < RATE_HZ / 200 ? float(edge) / float(RATE_HZ / 200) : 1.0f;
    return int16_t(0.25f * fade * sinf(2.0f * float(M_PI) * 1000.0f * float(i) / RATE_HZ) * 32767);
}

// Begins the drivers and writes the sounds once, and skips the test without a card.
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
        written = writeWav(RAMP, RAMP_SAMPLES, rampAt, false) &&
                  writeWav(RAMP_CHUNKS, RAMP_SAMPLES, rampAt, true) &&
                  writeWav(RAMP_DOWN, RAMP_SAMPLES, rampDownAt, false) &&
                  writeWav(LEVEL_FILE, LEVEL_SAMPLES, levelAt, false) &&
                  writeWav(FULL, FULL_SAMPLES, fullAt, false) &&
                  writeWav(SHORT, SHORT_SAMPLES, rampAt, false) &&
                  writeWav(TONES, RATE_HZ * 8 / 5, toneAt, false) &&
                  writeWav(BLIP, RATE_HZ / 10, blipAt, false);
        if (!written) {
            test::fail("writing the sounds failed", "they reach the card");
            return false;
        }
    }
    for (uint8_t c = 0; c < AudioDriver::CHANNELS; ++c) {
        audio.stop(c);
        audio.volume(c, 1);
    }
    delay(SETTLE_MS);
    DriverEvent drop[EVENTS];
    while (events.read(drop, EVENTS) > 0) {
    }
    return true;
}

// Gets the numbers of the SOUND_ENDED events that have come, with their channels.
static uint32_t endedEvents(uint32_t (&numbers)[EVENTS], uint8_t (&channels)[EVENTS]) {
    DriverEvent e[EVENTS];
    const uint32_t n = events.read(e, EVENTS);
    uint32_t k = 0;
    for (uint32_t i = 0; i < n; ++i) {
        if (e[i].type == DriverEventType::SOUND_ENDED) {
            numbers[k] = payload<DriverEventType::SOUND_ENDED>(e[i]);
            channels[k] = e[i].source;
            k = k + 1;
        }
    }
    return k;
}

// Waits for the SOUND_ENDED of a number for up to ms, and gets whether it came.
static bool waitEnded(uint32_t number, uint32_t ms) {
    const uint32_t t0 = millis();
    while (millis() - t0 < ms) {
        uint32_t numbers[EVENTS];
        uint8_t channels[EVENTS];
        const uint32_t n = endedEvents(numbers, channels);
        for (uint32_t i = 0; i < n; ++i) {
            if (numbers[i] == number) {
                return true;
            }
        }
        delay(1);
    }
    return false;
}

// Gets the first recorded sample at or after from that is not 0, or the end of the record.
static uint32_t firstSound(uint32_t from) {
    const uint32_t end = tap->recorded();
    uint32_t i = from;
    while (i < end && tap->at(i) == 0) {
        i = i + 1;
    }
    return i;
}

// Waits up to a second for the first sample that is not 0, and gets it, or the end of the record.
static uint32_t waitSound() {
    const uint32_t t0 = millis();
    uint32_t at = firstSound(0);
    while (at >= tap->recorded() && millis() - t0 < 1000) {
        delay(1);
        at = firstSound(0);
    }
    return at;
}

// Checks that the record from at holds count samples of a sound that starts at its first sample,
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

static void testSamples() {
    if (!ready()) {
        return;
    }
    const char* files[] = {RAMP, RAMP_CHUNKS};
    for (const char* file : files) {
        tap->start();
        const uint32_t number = audio.play(0, storage.open(file));
        if (!waitEnded(number, 2000)) {
            test::fail("no SOUND_ENDED", "one at the sound's end");
            return;
        }
        delay(SETTLE_MS);
        tap->stop();
        const uint32_t at = firstSound(0);
        if (!follows(at, RAMP_SAMPLES, rampAt, 0, file)) {
            return;
        }
        for (uint32_t i = at + RAMP_SAMPLES; i < tap->recorded(); ++i) {
            if (tap->at(i) != 0) {
                snprintf(seen, sizeof seen, "%s: sample %lu after the end is %d", file, i - at,
                         tap->at(i));
                test::fail(seen, "silence");
                return;
            }
        }
    }
    test::pass();
}

static void testMix() {
    if (!ready()) {
        return;
    }
    tap->start();
    for (uint8_t c = 0; c < AudioDriver::CHANNELS; ++c) {
        audio.play(c, storage.open(FULL));
    }
    delay(1200);
    tap->stop();
    int32_t highest = 0;
    int32_t lowest = 0;
    for (uint32_t i = 0; i < tap->recorded(); ++i) {
        highest = max(highest, int32_t(tap->at(i)));
        lowest = min(lowest, int32_t(tap->at(i)));
    }
    Serial.printf("samples from %ld to %ld, full scale behind the master gain %d to %d\n", lowest,
                  highest, gained(-32768), gained(32767));
    if (highest > gained(32767) || lowest < gained(-32768) || highest == 0) {
        snprintf(seen, sizeof seen, "samples from %ld to %ld", lowest, highest);
        snprintf(wanted, sizeof wanted, "within %d to %d", gained(-32768), gained(32767));
        test::fail(seen, wanted);
        return;
    }
    test::pass();
}

// Checks a stretch of the record against the ramp looping, from sample next of the ramp on, and
// moves next and checked along. Silence in between is counted in silent where allowed.
static bool loopFollows(uint32_t& checked, uint32_t upTo, uint32_t& next, uint32_t* silent) {
    for (; checked < upTo; ++checked) {
        const int16_t got = tap->at(checked);
        if (silent != nullptr && got == 0) {
            *silent = *silent + 1;
            continue;
        }
        const int16_t want = gained(rampAt(next));
        if (got != want) {
            snprintf(seen, sizeof seen, "record sample %lu is %d", checked, got);
            snprintf(wanted, sizeof wanted, "ramp sample %lu, %d", next % RAMP_SAMPLES, want);
            test::fail(seen, wanted);
            return false;
        }
        next = next + 1;
    }
    return true;
}

// Plays the ramp in a loop and checks the record while it plays, for loops loops. Where writes is
// set, a long write to the card runs meanwhile instead, the check runs until it has ended, and
// silence is allowed.
static void loopRun(uint32_t loops, bool writes) {
    tap->start();
    audio.play(0, storage.open(RAMP), true);
    const uint32_t at = waitSound();
    if (at >= tap->recorded()) {
        test::fail("no sound", "the ramp");
        return;
    }
    // The first block glides in. The check starts behind it.
    uint32_t checked = at + N;
    uint32_t next = N;
    uint32_t silent = 0;
    FileRequest write;
    uint32_t written = 0;
    const FileWriter big = storage.openWriter(BIG);
    bool writing = writes;
    while (writes ? writing : next < loops * RAMP_SAMPLES) {
        if (writes && write.status() != FileStatus::PENDING) {
            if (write.status() == FileStatus::FAILED) {
                test::fail("a write failed", "every write succeeds");
                audio.stop(0);
                return;
            }
            if (written < BIG_WRITES) {
                big.write(written * WRITE_BYTES, fileBuffer, WRITE_BYTES, write);
                written = written + 1;
            } else {
                writing = false;
            }
        }
        const uint32_t upTo = tap->recorded();
        if (upTo - checked >= RECORD) {
            test::fail("the check fell behind the record", "it keeps up");
            audio.stop(0);
            return;
        }
        if (!loopFollows(checked, upTo, next, writes ? &silent : nullptr)) {
            audio.stop(0);
            return;
        }
        delay(10);
    }
    audio.stop(0);
    write.wait();
    tap->stop();
    Serial.printf("%lu samples checked, %lu silent, %lu writes\n", next, silent, written);
    if (writes && written < BIG_WRITES) {
        snprintf(seen, sizeof seen, "%lu writes during the sound", written);
        test::fail(seen, "every write");
        return;
    }
    test::pass();
}

static void testLoop() {
    if (!ready()) {
        return;
    }
    loopRun(test::soak() ? 100 : 5, false);
}

static void testReplace() {
    if (!ready()) {
        return;
    }
    tap->start();
    audio.play(0, storage.open(RAMP), true);
    delay(300);
    const uint32_t cut = tap->recorded();
    audio.play(0, storage.open(RAMP_DOWN));
    delay(300);
    tap->stop();
    audio.stop(0);
    // The new sound is the first negative sample behind the cut.
    uint32_t at = cut;
    while (at < tap->recorded() && tap->at(at) >= 0) {
        at = at + 1;
    }
    const int32_t jump = largestJump(cut, at);
    Serial.printf("largest jump around the cut %ld, the new sound at sample %lu after it\n", jump,
                  at - cut);
    if (jump > JUMP_MAX) {
        snprintf(seen, sizeof seen, "a jump of %ld", jump);
        snprintf(wanted, sizeof wanted, "at most %ld", JUMP_MAX);
        test::fail(seen, wanted);
        return;
    }
    if (!follows(at, 4 * N, rampDownAt, 0, "the new sound")) {
        return;
    }
    test::pass();
}

// Gets the sample the constant sound leaves at a volume, behind the master gain.
static int16_t levelAtVolume(float volume) {
    const float scaled = float(LEVEL) * volume;
    return gained(int16_t(scaled + 0.5f));
}

static void testGlide() {
    if (!ready()) {
        return;
    }
    tap->start();
    audio.play(0, storage.open(LEVEL_FILE));
    delay(200);
    audio.volume(0, 0.3f);
    delay(200);
    const int16_t plateau = tap->at(tap->recorded() - 1);
    audio.stop(0);
    delay(100);
    tap->stop();
    const int32_t jump = largestJump(0, tap->recorded());
    Serial.printf("largest jump %ld, the level at 0.3 %d\n", jump, plateau);
    if (jump > JUMP_MAX) {
        snprintf(seen, sizeof seen, "a jump of %ld", jump);
        snprintf(wanted, sizeof wanted, "at most %ld", JUMP_MAX);
        test::fail(seen, wanted);
        return;
    }
    if (plateau != levelAtVolume(0.3f) || tap->at(tap->recorded() - 1) != 0) {
        snprintf(seen, sizeof seen, "level %d at 0.3, %d after the stop", plateau,
                 tap->at(tap->recorded() - 1));
        snprintf(wanted, sizeof wanted, "%d, then 0", levelAtVolume(0.3f));
        test::fail(seen, wanted);
        return;
    }
    test::pass();
}

static void testVolume() {
    if (!ready()) {
        return;
    }
    struct Case {
        float set;
        float counts;
    };
    const Case cases[] = {{0.5f, 0.5f}, {0.25f, 0.25f}, {2.0f, 1.0f}, {-1.0f, 0.0f}, {1, 1}};
    tap->start();
    audio.play(0, storage.open(LEVEL_FILE), true);
    for (const Case& c : cases) {
        audio.volume(0, c.set);
        delay(50);
        const int16_t got = tap->at(tap->recorded() - 1);
        if (got != levelAtVolume(c.counts)) {
            snprintf(seen, sizeof seen, "volume %d/100: %d", int(c.set * 100), got);
            snprintf(wanted, sizeof wanted, "%d", levelAtVolume(c.counts));
            test::fail(seen, wanted);
            audio.stop(0);
            return;
        }
    }
    audio.stop(0);
    tap->stop();
    test::pass();
}

static void testResume() {
    if (!ready()) {
        return;
    }
    tap->start();
    audio.play(0, storage.open(RAMP));
    delay(200);
    const AudioDriver::Position at = audio.position(0);
    audio.play(0, storage.open(RAMP_DOWN));
    delay(200);
    const uint32_t back = tap->recorded();
    const uint32_t number = audio.play(0, at);
    delay(200);
    tap->stop();
    audio.stop(0);
    if (at.empty() || number == 0) {
        test::fail("an empty position", "the ramp where it stood");
        return;
    }
    // The ramp goes on at the first positive sample behind the falling ramp.
    uint32_t i = back;
    while (i < tap->recorded() && tap->at(i) <= 0) {
        i = i + 1;
    }
    Serial.printf("noted at ramp sample %lu\n", at.played());
    if (!follows(i, 4 * N, rampAt, at.played(), "the ramp after the position")) {
        return;
    }
    test::pass();
}

static void testEnded() {
    if (!ready()) {
        return;
    }
    // The loop, the replaced sound and the stopped one end with no event.
    audio.play(0, storage.open(RAMP), true);
    const uint32_t plays = audio.play(1, storage.open(SHORT));
    audio.play(2, storage.open(LEVEL_FILE));
    audio.play(3, storage.open(LEVEL_FILE));
    delay(100);
    audio.play(2, storage.open(LEVEL_FILE));
    audio.stop(3);
    const uint32_t t0 = millis();
    uint32_t playsCount = 0;
    while (millis() - t0 < 1500) {
        uint32_t numbers[EVENTS];
        uint8_t channels[EVENTS];
        const uint32_t n = endedEvents(numbers, channels);
        for (uint32_t k = 0; k < n; ++k) {
            if (numbers[k] == plays && channels[k] == 1) {
                playsCount = playsCount + 1;
            } else {
                snprintf(seen, sizeof seen, "SOUND_ENDED %lu on channel %u", numbers[k],
                         channels[k]);
                snprintf(wanted, sizeof wanted, "only %lu on channel 1", plays);
                test::fail(seen, wanted);
                audio.stop(0);
                audio.stop(2);
                return;
            }
        }
        delay(5);
    }
    audio.stop(0);
    audio.stop(2);
    if (playsCount != 1) {
        snprintf(seen, sizeof seen, "%lu SOUND_ENDED for the short sound", playsCount);
        test::fail(seen, "one");
        return;
    }
    test::pass();
}

static void testMissing() {
    if (!ready()) {
        return;
    }
    tap->start();
    audio.play(0, storage.open(RAMP), true);
    delay(100);
    const uint32_t number = audio.play(1, storage.open(MISSING));
    const bool ended = waitEnded(number, ENDED_MS);
    delay(200);
    const uint32_t at = waitSound();
    uint32_t checked = at + N;
    uint32_t next = N;
    const bool on = loopFollows(checked, tap->recorded(), next, nullptr);
    audio.stop(0);
    tap->stop();
    if (!on) {
        return;
    }
    if (!ended) {
        snprintf(seen, sizeof seen, "no SOUND_ENDED within %lu ms", ENDED_MS);
        test::fail(seen, "SOUND_ENDED at once");
        return;
    }
    test::pass();
}

static void testStarved() {
    if (!ready()) {
        return;
    }
    loopRun(0, true);
}

static void testByEar() {
    if (!ready()) {
        return;
    }
    test::step("listen: tones loop on channel 0, a blip on channels 1 to 3, one blip restarting");
    audio.volume(0, 0.7f);
    audio.play(0, storage.open(TONES), true);
    const uint32_t t0 = millis();
    uint32_t restarts = 0;
    while (millis() - t0 < test::durationMs(20000, 180000)) {
        audio.play(1, storage.open(BLIP));
        delay(300);
        audio.play(2, storage.open(BLIP));
        delay(300);
        // Channel 3 restarts its blip before it has ended, many times in a row.
        for (int i = 0; i < 5; ++i) {
            audio.play(3, storage.open(BLIP));
            restarts = restarts + 1;
            delay(40);
        }
        delay(400);
    }
    for (uint8_t c = 0; c < AudioDriver::CHANNELS; ++c) {
        audio.stop(c);
    }
    const test::Answer a = test::ask(
        "did every sound play without dropouts or clicks, each blip from its start, and the tones "
        "loop without a gap?",
        ASK_MS);
    if (a != test::Answer::YES) {
        test::fail(a == test::Answer::NO ? "dropouts or clicks heard" : "no answer", "clean sound");
        return;
    }
    Serial.printf("%lu restarts\n", restarts);
    test::pass();
}

static const Test TESTS[] = {
    {"audio.samples", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testSamples, nullptr},
    {"audio.mix", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testMix, nullptr},
    {"audio.loop", RunsOn::TEENSY, LIMIT_S, SOAK_LIMIT_S, testLoop, nullptr},
    {"audio.replace", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testReplace, nullptr},
    {"audio.glide", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testGlide, nullptr},
    {"audio.volume", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testVolume, nullptr},
    {"audio.resume", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testResume, nullptr},
    {"audio.ended", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testEnded, nullptr},
    {"audio.missing", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testMissing, nullptr},
    {"audio.starved", RunsOn::TEENSY, SOAK_LIMIT_S, SOAK_LIMIT_S, testStarved, nullptr},
    {"audio.by-ear", RunsOn::BENCH, BENCH_LIMIT_S, BENCH_LIMIT_S, testByEar, nullptr},
};

extern const Program AUDIO = {TESTS, sizeof TESTS / sizeof TESTS[0]};
