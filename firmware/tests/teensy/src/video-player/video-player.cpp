// The video-player program of docs/firmware/testing.md. It plays videos it writes into the test
// game's directory on the card, on the display driver and the real audio driver. A tap behind the
// master gain records every sample bound for the amplifier.
//
//   frames         loop.rgb plays with its sound under the game's load, every frame in its 40 ms
//                  and in step with its sound, and no channel's sound runs dry
//   viewport       a video fills only its viewport, the picture around it, a caption in front
//                  (asks at the bench)
//   missing-movie  a video the card does not hold ends at once with MOVIE_ENDED
//   sync           each flash of sync.rgb comes with its click, and the music goes on after it
//                  (asks at the bench)
//
// Wiring: the Adafruit 2090 on its 8-bit bus and the touch controller on Wire2, as
// pin-assignment.md gives them, and the card. frames sends dark light frames on chain F, which has
// no LEDs at the bench, so no LED lights. The DFR0954 with the speaker on I²S2 for sync; frames
// runs without it, since the tap records inside the Teensy. The tests leave their files in
// /games/tests/.

#include <Arduino.h>
#include <AudioStream.h>
#include <stdio.h>
// FastLED's fl/gfx/colorutils.h turns warnings off twice and back on once, so without
// the push and pop -Wunused-parameter and -Wreturn-type stay off for all code after it.
#pragma GCC diagnostic push
#include <FastLED.h>
#pragma GCC diagnostic pop
#include "components/file-system.h"
#include "components/screen.h"
#include "components/sounds.h"
#include "components/video-player.h"
#include "core/clock.h"
#include "drivers/audio.h"
#include "drivers/display.h"
#include "drivers/lighting.h"
#include "drivers/storage.h"
#include "drivers/touch.h"
#include "io/i2c-bus.h"
#include "test-harness.h"
#include "test-media.h"

constexpr uint32_t LIMIT_S = 90;   // the first run writes the test files
constexpr uint32_t FRAMES_QUICK_MS = 20000;
constexpr uint32_t FRAMES_SOAK_MS = 600000;
constexpr uint32_t FRAMES_SOAK_LIMIT_S = 720;
constexpr uint32_t BENCH_LIMIT_S = 300;
constexpr uint32_t ASK_MS = 240000;
constexpr size_t TEXT = 160;
constexpr uint32_t N = SOUND_BLOCK_SAMPLES;
constexpr uint32_t EVENTS = 16;
constexpr uint32_t SETTLE_MS = 200;         // the effects glide in and the video's sound starts
constexpr uint32_t END_MS = 200;            // far longer than a missing file takes to report
constexpr uint32_t LAMPS_FRAME_MS = 11;     // the lamps' frame period
constexpr uint32_t WRITE_EVERY_MS = 1000;   // frames: a write to the card every second
constexpr uint32_t WRITE_BYTES = 4096;
constexpr uint32_t BIG_BYTES = 4u * 1024 * 1024;   // frames: the file written first
constexpr uint8_t BENCH_CHAIN = Lighting::CHAINS - 1;
constexpr Layer SLIDE = {1, 10};
constexpr Layer MUSIC = {2, 5};
/// The master gain as the Audio library applies it: the sample times the gain in 16.16, cut down
constexpr int32_t MULTIPLIER = int32_t(AudioDriver::MASTER_GAIN * 65536.0f);
/// The sum of the samples: the effects in bits 10 to 12, the video's sound below
constexpr int32_t VIDEO_BITS = 1023;
constexpr int32_t EFFECT_SHIFT = 10;
constexpr int32_t EFFECTS_ALL = 7;
/// Half the video's sound, below which its share counts as missing
constexpr int32_t VIDEO_PRESENT = media::LOOP_SAMPLE / 2;
/// The share of a frame's sound by which the frame on the screen may trail it, since the sound
/// plays on while LVGL draws: one frame
constexpr uint32_t BEHIND_FRAMES = 1;
/// A frame that follows the one before after more than this has come late: one frame period of
/// 40 ms and the jitter of the sound's blocks and of LVGL's drawing
constexpr uint32_t LATE_US = 48000;

// the tap's record: 2^18 samples, 5.9 s
constexpr uint32_t RECORD = 1u << 18;
constexpr uint32_t RECORD_MASK = RECORD - 1;

// the programs' shared PSRAM
static_assert(RECORD * sizeof(int16_t) <= test::SCRATCH_BYTES, "the record fits");
static int16_t* const samples = reinterpret_cast<int16_t*>(test::scratch());
EXTMEM static uint8_t writeBuffer[WRITE_BYTES] __attribute__((aligned(32)));

// The audio and sounds programs name their taps Tap as well, so this one stays inside this file.
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

    void start() {
        recording = false;
        count = 0;
        recording = true;
    }

    void stop() { recording = false; }

    uint32_t recorded() const { return count; }

    int16_t at(uint32_t i) const { return samples[i & RECORD_MASK]; }

private:
    audio_block_t* inputs[1];
    volatile bool recording = false;
    volatile uint32_t count = 0;
};

}   // namespace

/// The host's calls to the screen, the file system, the sounds and the video player.
class GameHost {
public:
    static void use(FileSystem& f, const char* game) { f.use(game); }
    static void reset(Screen& s, VideoPlayer& p, Sounds& sounds) {
        p.reset();
        s.reset();
        sounds.reset();
    }
    static uint32_t played(const Sounds& sounds) { return sounds.videoPlayed(); }
};

static Display display;
static Screen screen(display);
static EventQueue queue;
static AudioDriver audio(queue);
static FileSystem files(storage);
static Sounds sounds(audio, files);
static VideoPlayer player(screen, files, sounds);
static Lighting lighting;
static Touch touch(io::wire2Bus, queue, driverTick);
static Tap* tap = nullptr;
static char seen[TEXT];
static uint32_t movieEnded = 0;

// Begins the drivers and the components once, writes the test files and starts each test on an
// empty screen with silent channels. Skips the test without a card.
static bool ready() {
    static bool begun = false;
    if (!storage.begin()) {
        test::skip("no card in the socket");
        return false;
    }
    if (!begun) {
        if (!display.begin() || !audio.begin()) {
            test::fail("the display's or the audio driver's begin() false", "true");
            return false;
        }
        // Made after the driver's stages, the tap is updated behind the master gain.
        static Tap made;
        static AudioConnection wire(audio.output(), 0, made, 0);
        tap = &made;
        screen.begin();
        player.begin();
        begun = true;
    }
    if (!media::writeLoop() || !media::writeEffects() || !media::writePictures() ||
        !media::writeBenchMedia()) {
        test::fail("writing the test files failed", "they reach the card");
        return false;
    }
    GameHost::use(files, media::GAME);
    sounds.begin();
    GameHost::reset(screen, player, sounds);
    // frames decodes the sum of all four channels at full scale. The sounds scale music and
    // effects at 1 down to a half each, so the test hands the driver its volumes directly.
    for (uint8_t c = 0; c < AudioDriver::CHANNELS; ++c) {
        audio.volume(c, 1);
    }
    movieEnded = 0;
    return true;
}

// Runs one pass of the main loop: hands the driver events to the sounds, takes the video
// player's event and renders.
static void pass() {
    DriverEvent e[EVENTS];
    const uint32_t n = queue.read(e, EVENTS);
    for (uint32_t i = 0; i < n; ++i) {
        PinballEvents out;
        sounds.translate(e[i], out);
    }
    PinballEvents out;
    if (player.update(nowUs(), out)) {
        movieEnded = movieEnded + out.count;
    }
    screen.render();
}

static lv_obj_t* viewport(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    return o;
}

// ---- frames

static int32_t lastFrame = -1;
static uint32_t framesShown = 0;
static uint32_t framesSkipped = 0;
static uint32_t framesOff = 0;
static int32_t worstOff = 0;
static uint32_t lastFrameUs = 0;
static uint32_t longestGapUs = 0;   // the longest time between two frames
static uint32_t lateFrames = 0;     // frames that came more than LATE_US after the one before
constexpr uint32_t NOTED = 8;       // the frames out of step that the report names
static char noted[NOTED][48];
static uint32_t notedCount = 0;

// Notes a frame out of step for the report.
static void note(const char* what, int32_t k, int32_t sound) {
    if (notedCount < NOTED) {
        snprintf(noted[notedCount], sizeof noted[0], "%s at %lu ms: frame %ld, %ld", what, millis(),
                 long(k), long(sound));
        notedCount = notedCount + 1;
    }
}

// Reads the frame LVGL sends from its top left pixel and compares it with the frame before and
// with how far the sound has played.
static void onFlush(lv_event_t* e) {
    lv_display_t* d = static_cast<lv_display_t*>(lv_event_get_target(e));
    const uint8_t* p = lv_display_get_buf_active(d)->data;
    const int32_t k = int32_t(p[0] << 8 | p[1]);
    if (k == lastFrame || k >= int32_t(media::LOOP_FRAMES)) {
        return;
    }
    framesShown = framesShown + 1;
    const uint32_t nowAt = nowUs();
    // The first frame stands until the first frames have landed and the sound has started, so the
    // gaps count from the second frame on.
    if (framesShown > 2) {
        const uint32_t gap = nowAt - lastFrameUs;
        longestGapUs = gap > longestGapUs ? gap : longestGapUs;
        if (gap > LATE_US) {
            lateFrames = lateFrames + 1;
            note("late", k, int32_t(gap / 1000));
        }
    }
    lastFrameUs = nowAt;
    const int32_t sound =
        int32_t(GameHost::played(sounds) / media::FRAME_SAMPLES) % int32_t(media::LOOP_FRAMES);
    if (lastFrame >= 0 && k != (lastFrame + 1) % int32_t(media::LOOP_FRAMES)) {
        framesSkipped = framesSkipped + 1;
        note("skip", k, sound);
    }
    lastFrame = k;
    int32_t off = (sound - k + int32_t(media::LOOP_FRAMES)) % int32_t(media::LOOP_FRAMES);
    if (off > int32_t(media::LOOP_FRAMES) / 2) {
        off -= int32_t(media::LOOP_FRAMES);
    }
    if (off < 0 || off > int32_t(BEHIND_FRAMES)) {
        framesOff = framesOff + 1;
        worstOff = off;
        note("off", k, sound);
    }
}

static uint32_t checked = 0;
static uint32_t effectsMissing = 0;
static uint32_t videoGapBlocks = 0;
static bool soundSeen = false;   // the video's sound has reached the record

// Decodes the recorded sums up to the record's end: every effect has to be there in every sample,
// and the video's sound in every block from its first on.
static bool checkTap() {
    const uint32_t upTo = tap->recorded();
    if (upTo - checked >= RECORD) {
        return false;
    }
    while (upTo - checked >= N) {
        bool video = false;
        for (uint32_t i = 0; i < N; ++i) {
            const int32_t sum =
                (int32_t(tap->at(checked + i)) * 65536 + MULTIPLIER / 2) / MULTIPLIER;
            if (((sum + media::LOOP_SAMPLE / 4) >> EFFECT_SHIFT) != EFFECTS_ALL) {
                effectsMissing = effectsMissing + 1;
            }
            video = video || (sum & VIDEO_BITS) >= VIDEO_PRESENT;
        }
        soundSeen = soundSeen || video;
        if (soundSeen && !video && notedCount < NOTED) {
            snprintf(noted[notedCount], sizeof noted[0], "sound gap at %lu ms of the record",
                     checked / (media::RATE_HZ / 1000));
            notedCount = notedCount + 1;
        }
        videoGapBlocks += soundSeen && !video ? 1 : 0;
        checked += N;
    }
    return true;
}

static void testFrames() {
    if (!ready()) {
        return;
    }
    // A file of several megabytes first, so the card has its housekeeping to do.
    char path[FILE_PATH_BYTES];
    media::pathOf("big.bin", path);
    {
        FileRequest r;
        const FileWriter big = storage.openWriter(path);
        big.rewrite(media::chunk, 0, r);
        r.wait();
        for (uint32_t at = 0; at < BIG_BYTES; at += media::CHUNK_BYTES) {
            big.write(at, media::chunk, media::CHUNK_BYTES, r);
            if (!r.wait()) {
                test::fail("writing big.bin failed", "it reaches the card");
                return;
            }
        }
    }
    // The game's load: dark light frames on chain F and the touch driver's reads.
    static bool loadBegun = false;
    if (!loadBegun) {
        Lighting::Settings settings;
        settings.leds[BENCH_CHAIN] = Lighting::MAX_CHAIN_LEDS;
        lighting.configure(settings);
        if (!lighting.begin()) {
            test::fail("the lighting's begin() false", "true");
            return;
        }
        fill_solid(lighting.chain(BENCH_CHAIN), lighting.leds(BENCH_CHAIN), CRGB::Black);
        if (!touch.begin()) {
            Serial.println(
                "the touch controller does not answer, so frames runs without its reads");
        }
        lv_display_add_event_cb(lv_display_get_default(), onFlush, LV_EVENT_FLUSH_START, nullptr);
        loadBegun = true;
    }
    const char* const effects[] = {"audio/1.wav", "audio/2.wav", "audio/3.wav"};
    for (uint8_t c = 0; c < 3; ++c) {
        media::pathOf(effects[c], path);
        audio.volume(uint8_t(c + 1), 1);
        audio.play(uint8_t(c + 1), storage.open(path), true);
    }
    lv_obj_t* v = viewport(screen.slide(SLIDE), 0, 0, media::W, media::H);
    lastFrame = -1;
    framesShown = framesSkipped = framesOff = notedCount = lateFrames = longestGapUs = 0;
    worstOff = 0;
    if (!player.play(v, "loop", true, true)) {
        test::fail("play() false", "the video plays");
        return;
    }
    uint32_t t0 = millis();
    while (millis() - t0 < SETTLE_MS) {
        pass();
    }
    tap->start();
    checked = effectsMissing = videoGapBlocks = 0;
    soundSeen = false;
    media::pathOf("writes.bin", path);
    const FileWriter writes = storage.openWriter(path);
    FileRequest write;
    uint32_t written = 0;
    uint32_t lightFrames = 0;
    uint32_t refusedLight = 0;
    t0 = millis();
    uint32_t nextLight = t0;
    uint32_t nextWrite = t0;
    const uint32_t durationMs = test::durationMs(FRAMES_QUICK_MS, FRAMES_SOAK_MS);
    uint32_t longestPassUs = 0;
    bool kept = true;
    while (millis() - t0 < durationMs && kept) {
        const uint32_t now = millis();
        if (int32_t(now - nextLight) >= 0) {
            nextLight += LAMPS_FRAME_MS;
            lightFrames = lightFrames + 1;
            refusedLight += lighting.show() ? 0 : 1;
        }
        if (int32_t(now - nextWrite) >= 0 && write.status() != FileStatus::PENDING) {
            nextWrite += WRITE_EVERY_MS;
            writes.write(written * WRITE_BYTES, writeBuffer, WRITE_BYTES, write);
            written = written + 1;
        }
        const uint32_t passStart = nowUs();
        pass();
        const uint32_t passUs = nowUs() - passStart;
        longestPassUs = passUs > longestPassUs ? passUs : longestPassUs;
        kept = checkTap() && player.playing(v);
    }
    const uint32_t loops = (millis() - t0) / (media::LOOP_FRAMES * 1000 / media::FRAMES_PER_S) + 1;
    player.stop(v);
    for (uint8_t c = 1; c <= 3; ++c) {
        audio.stop(c);
    }
    tap->stop();
    write.wait();
    Serial.printf("%lu frames late, the longest gap %lu us, the longest pass %lu us\n", lateFrames,
                  longestGapUs, longestPassUs);
    Serial.printf("%lu frames shown, %lu skipped, %lu off the sound (last %ld), %lu loops, "
                  "%lu samples lacked an effect, %lu blocks lacked the video's sound, "
                  "%lu writes, %lu light frames, %lu refused\n",
                  framesShown, framesSkipped, framesOff, long(worstOff), loops, effectsMissing,
                  videoGapBlocks, written, lightFrames, refusedLight);
    for (uint32_t i = 0; i < notedCount; ++i) {
        Serial.println(noted[i]);
    }
    if (!kept) {
        test::fail("the check fell behind the record or the video stopped", "it keeps up");
        return;
    }
    const uint32_t expectedFrames = durationMs * media::FRAMES_PER_S / 1000;
    if (framesSkipped != 0 || framesOff != 0 || lateFrames != 0 ||
        framesShown + media::FRAMES_PER_S < expectedFrames) {
        snprintf(seen, sizeof seen, "%lu of %lu frames shown, %lu skipped, %lu off the sound",
                 framesShown, expectedFrames, framesSkipped, framesOff);
        test::fail(seen, "every frame in its 40 ms, in step with the sound");
        return;
    }
    if (effectsMissing != 0 || videoGapBlocks != 0 || !soundSeen) {
        snprintf(seen, sizeof seen, "%lu samples lacked an effect, %lu blocks the video's sound",
                 effectsMissing, videoGapBlocks);
        test::fail(seen, "none, once the video's sound has started");
        return;
    }
    test::pass();
}

// ---- viewport

static void testViewport() {
    if (!ready()) {
        return;
    }
    static Picture background(screen);
    lv_obj_t* slide = screen.slide(SLIDE);
    background.load(files.open("background.img"));
    background.show(slide);
    lv_obj_t* v = viewport(slide, 80, 60, media::VIEWPORT_W, media::VIEWPORT_H);
    lv_obj_t* caption = lv_label_create(slide);
    lv_label_set_text(caption, "CAPTION");
    lv_obj_set_style_text_color(caption, lv_color_white(), 0);
    lv_obj_set_style_bg_color(caption, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(caption, LV_OPA_COVER, 0);
    lv_obj_set_pos(caption, 120, 170);
    if (!player.play(v, "viewport", true, false)) {
        test::fail("play() false", "the video plays");
        return;
    }
    test::step("watch: a small yellow ball rolls to and fro in a green video in the middle of "
               "the background picture, the word CAPTION over its lower edge");
    const uint32_t t0 = millis();
    while (millis() - t0 < 5000) {
        pass();
    }
    const test::Answer a = test::ask("does the video fill only its viewport, with the picture "
                                     "around it and CAPTION in front of the video?",
                                     ASK_MS);
    player.stop(v);
    if (a != test::Answer::YES) {
        test::fail(a == test::Answer::NO ? "the layering looked wrong" : "no answer",
                   "the video in its viewport, the caption in front");
        return;
    }
    test::pass();
}

// ---- missing-movie

static void testMissingMovie() {
    if (!ready()) {
        return;
    }
    lv_obj_t* v = viewport(screen.slide(SLIDE), 0, 0, media::W, media::H);
    if (!player.play(v, "none", false, false)) {
        test::fail("play() false", "true, and the video ends");
        return;
    }
    const uint32_t t0 = millis();
    while (millis() - t0 < END_MS) {
        pass();
    }
    if (movieEnded != 1 || player.playing(v)) {
        snprintf(seen, sizeof seen, "%lu MOVIE_ENDED, playing %d", movieEnded, player.playing(v));
        test::fail(seen, "one MOVIE_ENDED, not playing");
        return;
    }
    test::pass();
}

// ---- sync

static void testSync() {
    if (!ready()) {
        return;
    }
    sounds.musicVolume(0.7f);
    sounds.music(MUSIC, "music.wav", true);
    test::step("listen: a melody plays, then the screen flashes white four times with a click "
               "each, then the melody goes on");
    uint32_t t0 = millis();
    while (millis() - t0 < 3000) {
        pass();
    }
    lv_obj_t* v = viewport(screen.slide(SLIDE), 0, 0, media::W, media::H);
    if (!player.play(v, "sync", false, true)) {
        test::fail("play() false", "the video plays");
        return;
    }
    t0 = millis();
    while (millis() - t0 < 8000) {
        pass();
    }
    const test::Answer a = test::ask(
        "did each flash come with its click, and did the melody go on where it stopped?", ASK_MS);
    sounds.stopMusic(MUSIC);
    if (movieEnded != 1) {
        snprintf(seen, sizeof seen, "%lu MOVIE_ENDED", movieEnded);
        test::fail(seen, "one");
        return;
    }
    if (a != test::Answer::YES) {
        test::fail(a == test::Answer::NO ? "out of step or the melody lost" : "no answer",
                   "flash and click together");
        return;
    }
    test::pass();
}

static const Test TESTS[] = {
    {"video-player.frames", RunsOn::TEENSY, LIMIT_S, FRAMES_SOAK_LIMIT_S, testFrames, nullptr},
    {"video-player.viewport", RunsOn::BENCH, BENCH_LIMIT_S, BENCH_LIMIT_S, testViewport, nullptr},
    {"video-player.missing-movie", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testMissingMovie, nullptr},
    {"video-player.sync", RunsOn::BENCH, BENCH_LIMIT_S, BENCH_LIMIT_S, testSync, nullptr},
};

extern const Program VIDEO_PLAYER = {TESTS, sizeof TESTS / sizeof TESTS[0]};
