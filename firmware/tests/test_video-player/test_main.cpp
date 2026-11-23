// Tests on the computer the video player: LVGL with a fake display driver, a fake card in memory,
// and the sounds on a fake audio driver whose blocks play when the test calls them. Frame k of
// every video is one color, 0x1000 + k, so a pixel of the screen names the frame that shows.
//
//   play         a video reads its frames ahead from video/<name>.rgb, and its sound starts with it
//   movie-ends   a video that runs out ends its sound and hands MOVIE_ENDED with its mode's key
//   stop         stop() ends the video and its sound and frees its memory
//   no-room      a full picture memory and a viewport beyond the screen refuse a video
//   owner        only the slide that shows plays a video, and a second video there replaces the
//                first
//   hidden       a covered or removed slide stops its video without MOVIE_ENDED, an object over
//                the viewport does not
//   late-card    a frame the card brings late stays, and the video catches up with the clock
//   short-sound  a video whose sound runs out or is missing plays on by the clock
//   sound-clock  a video with sound holds while its sound holds
//   covers       LVGL draws nothing that lies behind a video's frame
//
// Run: pio test -e computer, which runs in CI as well.

#include "components/file-system.h"
#include "components/screen.h"
#include "components/sounds.h"
#include "components/video-player.h"
#include "io/psram.h"
#include <map>
#include <memory>
#include <string>
#include <unity.h>
#include <vector>

constexpr uint16_t W = Display::WIDTH;
constexpr uint16_t H = Display::HEIGHT;
constexpr uint32_t FULL_BYTES = uint32_t(W) * H * 2;
constexpr uint32_t FRAME_US = 40000;
constexpr uint32_t PASS_US = 5000;   // a pass of the main loop in the tests
constexpr uint32_t FRAME_SAMPLES = 1764;
constexpr uint32_t RATE_HZ = 44100;
constexpr uint32_t N = SOUND_BLOCK_SAMPLES;
constexpr uint16_t FIRST_COLOR = 0x1000;
constexpr char GAME[] = "a";
constexpr uint8_t MAGIC = 0x19;   // LV_IMAGE_HEADER_MAGIC
constexpr Layer LOW = {1, 10};
constexpr Layer HIGH = {2, 50};
constexpr Layer HIGH_LATER = {3, 50};
constexpr Layer MUSIC_LAYER = {9, 5};
constexpr int16_t MUSIC_SAMPLE = 1000;   // every sample of the music, the videos' sounds are silent

// A card in memory that notes every read. Each file is a string, each slot names its file.
struct FakeCard : CardFiles {
    struct ReadAt {
        std::string path;
        uint32_t at;
    };
    std::map<std::string, std::string> files;
    std::string slotPath[SLOTS];
    std::vector<ReadAt> reads;

    bool mount() override { return true; }
    bool remount() override { return true; }
    uint32_t failedTransfers() const override { return 0; }
    uint32_t errorCode() const override { return 0; }
    bool open(uint8_t slot, const char* path, bool) override {
        if (files.count(path) == 0) {
            return false;
        }
        slotPath[slot] = path;
        return true;
    }
    void close(uint8_t) override {}
    uint32_t size(uint8_t slot) override { return uint32_t(files[slotPath[slot]].size()); }
    bool read(uint8_t slot, uint32_t at, uint8_t* to, size_t n, size_t& got) override {
        const std::string& f = files[slotPath[slot]];
        reads.push_back({slotPath[slot], at});
        got = at >= f.size() ? 0 : std::min(n, f.size() - at);
        f.copy(reinterpret_cast<char*>(to), got, at);
        return true;
    }
    bool write(uint8_t, uint32_t, const uint8_t*, size_t) override { return false; }
    bool cut(uint8_t, uint32_t) override { return false; }
    bool commit(uint8_t) override { return false; }
};

// The service with the storage task played by the test.
struct TestService : StorageService {
    explicit TestService(FakeCard& card) : StorageService(card) {}
    ~TestService() = default;

    void serveAll() {
        while (serveNext()) {
        }
    }
};

static FakeCard card;
static TestService service(card);
static FileSystem files(service);
static Display display;
static Screen screen(display);
static AudioDriver audio;
static Sounds sounds(audio, files);
static VideoPlayer player(screen, files, sounds);
static bool cardHeld = false;            // the storage task serves no request while it is set
static std::vector<PinballEvent> made;   // what the video player's update() handed out

// The host's calls to the screen, the file system, the sounds and the video player.
class GameHost {
public:
    static void use(const char* game) { files.use(game); }
    static void reset() {
        player.reset();
        screen.reset();
        sounds.reset();
    }
    static bool settled() { return screen.settled(); }
    static bool videoSounding() { return sounds.videoSounding(); }
};

static void put16(std::string& s, uint16_t v) {
    s.push_back(char(v & 0xFF));
    s.push_back(char(v >> 8));
}

static void put32(std::string& s, uint32_t v) {
    put16(s, uint16_t(v & 0xFFFF));
    put16(s, uint16_t(v >> 16));
}

// Gets a video file whose frame k is one color, FIRST_COLOR + k, high byte first.
static std::string movie(uint32_t frames, uint16_t width = W, uint16_t height = H) {
    std::string f;
    f.reserve(size_t(frames) * width * height * 2);
    for (uint32_t k = 0; k < frames; ++k) {
        const uint16_t color = uint16_t(FIRST_COLOR + k);
        for (uint32_t i = 0; i < uint32_t(width) * height; ++i) {
            f.push_back(char(color >> 8));
            f.push_back(char(color & 0xFF));
        }
    }
    return f;
}

// Gets a WAV file whose every sample is @p sample.
static std::string wav(uint32_t samples, int16_t sample = 0) {
    std::string body = "WAVEfmt ";
    put32(body, 16);
    put16(body, 1);
    put16(body, 1);
    put32(body, RATE_HZ);
    put32(body, RATE_HZ * 2);
    put16(body, 2);
    put16(body, 16);
    body += "data";
    put32(body, samples * 2);
    for (uint32_t i = 0; i < samples; ++i) {
        put16(body, uint16_t(sample));
    }
    std::string file = "RIFF";
    put32(file, uint32_t(body.size()));
    return file + body;
}

// Gets a full-screen picture file.
static std::string pictureFile() {
    std::string f;
    f.push_back(char(MAGIC));
    f.push_back(char(LV_COLOR_FORMAT_RGB565_SWAPPED));
    put16(f, 0);
    put16(f, W);
    put16(f, H);
    put16(f, uint16_t(W * 2));
    put16(f, 0);
    f.append(FULL_BYTES, '\0');
    return f;
}

// Runs one pass of the main loop @p us after the last one: the storage task serves the card, the
// video player hands its event out, and the screen renders.
static void pass(uint32_t us = PASS_US) {
    GPT2_CNT = GPT2_CNT + us;
    fakeMillis = GPT2_CNT / 1000;
    if (!cardHeld) {
        service.serveAll();
    }
    PinballEvents out;
    if (player.update(GPT2_CNT, out)) {
        made.insert(made.end(), out.events, out.events + out.count);
    }
    screen.render();
}

// Runs passes for @p us.
static void run(uint32_t us) {
    for (uint32_t t = 0; t < us; t += PASS_US) {
        pass();
    }
}

// Plays blocks of sound, handing the driver's events to the sounds.
static void blocks(uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) {
        service.serveAll();
        for (const DriverEvent& e : audio.block()) {
            PinballEvents out;
            sounds.translate(e, out);
        }
    }
}

// Gets the frame the screen shows at a point, -1 for a pixel of no frame.
static int shownAt(uint16_t x = W / 2, uint16_t y = H / 2) {
    const uint16_t p = display.pixel(x, y);
    return p >= FIRST_COLOR && p < FIRST_COLOR + 0x400 ? int(p - FIRST_COLOR) : -1;
}

// Runs passes that take no time until the first frames have landed and the video's sound starts.
static void startSound() {
    for (int i = 0; i < 4; ++i) {
        pass(0);
    }
}

static lv_obj_t* viewportOn(lv_obj_t* parent, int32_t x = 0, int32_t y = 0, int32_t w = W,
                            int32_t h = H) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    return o;
}

// Counts the reads of a file.
static uint32_t readsOf(const std::string& path) {
    uint32_t n = 0;
    for (const auto& r : card.reads) {
        n += r.path == path ? 1 : 0;
    }
    return n;
}

void setUp() {
    static bool begun = false;
    if (!begun) {
        begun = true;
        service.mount();
        GameHost::use(GAME);
        audio.begin();
        sounds.begin();
        sounds.musicVolume(1);
        screen.begin();
        player.begin();
        card.files["/games/a/video/m.rgb"] = movie(30);
        card.files["/games/a/video/ten.rgb"] = movie(10);
        card.files["/games/a/video/small.rgb"] = movie(10, 64, 48);
        card.files["/games/a/audio/m.wav"] = wav(60 * FRAME_SAMPLES);
        card.files["/games/a/audio/ten.wav"] = wav(20 * FRAME_SAMPLES);
        card.files["/games/a/audio/music.wav"] = wav(600 * FRAME_SAMPLES, MUSIC_SAMPLE);
        card.files["/games/a/full.img"] = pictureFile();
    }
    cardHeld = false;
    GameHost::reset();
    for (int i = 0; i < 4; ++i) {
        pass();
    }
    blocks(4);
    made.clear();
    card.reads.clear();
}

void tearDown() {}

static void testPlay() {
    lv_obj_t* s = screen.slide(LOW);
    lv_obj_t* v = viewportOn(s);
    TEST_ASSERT_TRUE(player.play(v, "m", false, false));
    TEST_ASSERT_TRUE(player.playing(v));
    TEST_ASSERT_EQUAL_STRING("", audio.files[Sounds::MUSIC].c_str());
    // The passes take no time, so the frames after the first land ahead of it.
    for (int i = 0; i < 20; ++i) {
        pass(0);
    }
    TEST_ASSERT_EQUAL_INT(0, shownAt());
    TEST_ASSERT_EQUAL_UINT32(VideoPlayer::FRAMES_AHEAD, readsOf("/games/a/video/m.rgb"));
    for (uint32_t k = 0; k < VideoPlayer::FRAMES_AHEAD; ++k) {
        TEST_ASSERT_EQUAL_UINT32(k * FULL_BYTES, card.reads[k].at);
    }
    run(3 * FRAME_US);
    TEST_ASSERT_EQUAL_INT(3, shownAt());
    player.stop(v);

    TEST_ASSERT_TRUE(player.play(v, "m", false, true));
    // The sound starts once the first frames have landed.
    TEST_ASSERT_FALSE(GameHost::videoSounding());
    startSound();
    TEST_ASSERT_EQUAL_STRING("/games/a/audio/m.wav", audio.files[Sounds::MUSIC].c_str());
    TEST_ASSERT_TRUE(GameHost::videoSounding());
}

static void testMovieEnds() {
    sounds.music(MUSIC_LAYER, "music.wav", true);
    blocks(10);
    lv_obj_t* s = screen.slide(HIGH);
    lv_obj_t* v = viewportOn(s);
    TEST_ASSERT_TRUE(player.play(v, "ten", false, true));
    // The sound is twice as long as the video.
    for (int i = 0; i < 400 && player.playing(v); ++i) {
        blocks(2);
        pass();
    }
    TEST_ASSERT_FALSE(player.playing(v));
    TEST_ASSERT_FALSE(GameHost::videoSounding());
    TEST_ASSERT_EQUAL_UINT32(0, made.size());
    pass();
    TEST_ASSERT_EQUAL_UINT32(1, made.size());
    TEST_ASSERT_EQUAL(EventType::MOVIE_ENDED, made[0].type);
    TEST_ASSERT_EQUAL_UINT8(HIGH.key, made[0].source);
    pass();
    TEST_ASSERT_EQUAL_UINT32(1, made.size());
    // The music goes on.
    blocks(6);
    TEST_ASSERT_EQUAL_INT16(MUSIC_SAMPLE, audio.last[Sounds::MUSIC]);
}

static void testStop() {
    sounds.music(MUSIC_LAYER, "music.wav", true);
    blocks(10);
    lv_obj_t* s = screen.slide(LOW);
    lv_obj_t* v = viewportOn(s);
    TEST_ASSERT_TRUE(player.play(v, "m", true, true));
    blocks(4);
    run(2 * FRAME_US);
    TEST_ASSERT_FALSE(GameHost::settled());
    player.stop(v);
    TEST_ASSERT_FALSE(player.playing(v));
    TEST_ASSERT_FALSE(GameHost::videoSounding());
    run(2 * FRAME_US);
    TEST_ASSERT_TRUE(GameHost::settled());
    blocks(6);
    TEST_ASSERT_EQUAL_INT16(MUSIC_SAMPLE, audio.last[Sounds::MUSIC]);
    TEST_ASSERT_EQUAL_UINT32(0, made.size());
}

static void testNoRoom() {
    lv_obj_t* s = screen.slide(LOW);
    lv_obj_t* small = viewportOn(s, 16, 16, 64, 48);
    TEST_ASSERT_TRUE(player.play(small, "small", true, false));
    run(FRAME_US);
    // Pictures fill the memory the small video leaves.
    static std::vector<std::unique_ptr<Picture>> pictures;
    bool full = false;
    while (!full) {
        pictures.push_back(std::make_unique<Picture>(screen));
        pictures.back()->load(files.open("full.img"));
        for (int i = 0; i < 3; ++i) {
            pass(0);
        }
        full = pictures.back()->failed();
    }
    const std::string music = audio.files[Sounds::MUSIC];
    const uint32_t stops = audio.stops[Sounds::MUSIC];
    TEST_ASSERT_FALSE(player.play(viewportOn(s), "m", false, true));
    TEST_ASSERT_FALSE(player.play(viewportOn(s, 300, 0, 64, 48), "small", false, true));
    TEST_ASSERT_FALSE(player.play(viewportOn(s, 0, 0, 0, 0), "small", false, true));
    TEST_ASSERT_EQUAL_STRING(music.c_str(), audio.files[Sounds::MUSIC].c_str());
    TEST_ASSERT_EQUAL_UINT32(stops, audio.stops[Sounds::MUSIC]);
    TEST_ASSERT_TRUE(player.playing(small));
    const int before = shownAt(40, 40);
    run(3 * FRAME_US);
    TEST_ASSERT_TRUE(shownAt(40, 40) > before);
}

static void testOwner() {
    lv_obj_t* hidden = screen.slide(LOW);
    lv_obj_t* shown = screen.slide(HIGH);
    lv_obj_t* first = viewportOn(shown);
    TEST_ASSERT_TRUE(player.play(first, "m", false, false));
    TEST_ASSERT_FALSE(player.play(viewportOn(hidden), "m", false, false));
    TEST_ASSERT_FALSE(player.play(viewportOn(screen.background()), "m", false, false));
    TEST_ASSERT_TRUE(player.playing(first));
    run(2 * FRAME_US);
    TEST_ASSERT_EQUAL_INT(2, shownAt());
    lv_obj_t* second = viewportOn(shown, 0, 0, 64, 48);
    TEST_ASSERT_TRUE(player.play(second, "small", false, false));
    TEST_ASSERT_FALSE(player.playing(first));
    TEST_ASSERT_TRUE(player.playing(second));
    run(FRAME_US);
    TEST_ASSERT_EQUAL_INT(1, shownAt(10, 10));
    TEST_ASSERT_EQUAL_INT(-1, shownAt(200, 200));
}

static void testHidden() {
    sounds.music(MUSIC_LAYER, "music.wav", true);
    blocks(10);
    // A slide of higher priority covers the video's slide.
    lv_obj_t* low = screen.slide(LOW);
    lv_obj_t* v = viewportOn(low);
    TEST_ASSERT_TRUE(player.play(v, "m", false, true));
    run(FRAME_US);
    screen.slide(HIGH);
    run(FRAME_US);
    TEST_ASSERT_FALSE(player.playing(v));
    TEST_ASSERT_FALSE(GameHost::videoSounding());
    // A slide of equal priority, taken later, covers it as well.
    lv_obj_t* high = screen.slide(HIGH);
    v = viewportOn(high);
    TEST_ASSERT_TRUE(player.play(v, "m", false, true));
    run(FRAME_US);
    lv_obj_t* later = screen.slide(HIGH_LATER);
    run(FRAME_US);
    TEST_ASSERT_FALSE(player.playing(v));
    TEST_ASSERT_FALSE(GameHost::videoSounding());
    // A slide removed takes its video with it.
    v = viewportOn(later);
    TEST_ASSERT_TRUE(player.play(v, "m", false, true));
    run(FRAME_US);
    screen.remove(HIGH_LATER);
    TEST_ASSERT_FALSE(player.playing(v));
    TEST_ASSERT_FALSE(GameHost::videoSounding());
    run(4 * FRAME_US);
    TEST_ASSERT_EQUAL_UINT32(0, made.size());
    // An object in front of the whole viewport leaves the video playing.
    v = viewportOn(high);
    TEST_ASSERT_TRUE(player.play(v, "m", false, false));
    viewportOn(high);
    run(4 * FRAME_US);
    TEST_ASSERT_TRUE(player.playing(v));
    blocks(6);
    TEST_ASSERT_EQUAL_INT16(MUSIC_SAMPLE, audio.last[Sounds::MUSIC]);
}

static void testLateCard() {
    lv_obj_t* v = viewportOn(screen.slide(LOW));
    TEST_ASSERT_TRUE(player.play(v, "m", false, false));
    pass(0);
    TEST_ASSERT_EQUAL_INT(0, shownAt());
    // The card holds the reads of the frames after the first back for four frames.
    cardHeld = true;
    run(4 * FRAME_US);
    TEST_ASSERT_EQUAL_INT(0, shownAt());
    cardHeld = false;
    for (int i = 0; i < 6; ++i) {
        pass(0);
    }
    TEST_ASSERT_EQUAL_INT(4, shownAt());
    run(FRAME_US);
    TEST_ASSERT_EQUAL_INT(5, shownAt());
}

static void testShortSound() {
    // The sound of ten lasts 20 frames, so a video of 30 frames with it outlasts its sound.
    card.files["/games/a/audio/long.wav"] = wav(10 * FRAME_SAMPLES);
    card.files["/games/a/video/long.rgb"] = movie(20);
    lv_obj_t* v = viewportOn(screen.slide(LOW));
    TEST_ASSERT_TRUE(player.play(v, "long", false, true));
    startSound();
    TEST_ASSERT_TRUE(GameHost::videoSounding());
    for (int i = 0; i < 200 && GameHost::videoSounding(); ++i) {
        blocks(1);
        pass(N * 1000000ull / RATE_HZ);
    }
    TEST_ASSERT_FALSE(GameHost::videoSounding());
    const int atEnd = shownAt();
    TEST_ASSERT_TRUE(atEnd >= 8 && atEnd <= 11);
    run(3 * FRAME_US);
    TEST_ASSERT_EQUAL_INT(atEnd + 3, shownAt());
    run(20 * FRAME_US);
    TEST_ASSERT_FALSE(player.playing(v));
    TEST_ASSERT_EQUAL_UINT32(1, made.size());

    // A sound file the card does not hold ends at once.
    made.clear();
    card.files["/games/a/video/mute.rgb"] = movie(5);
    v = viewportOn(screen.slide(LOW));
    TEST_ASSERT_TRUE(player.play(v, "mute", false, true));
    startSound();
    blocks(4);
    TEST_ASSERT_FALSE(GameHost::videoSounding());
    run(2 * FRAME_US);
    TEST_ASSERT_EQUAL_INT(2, shownAt());
    run(5 * FRAME_US);
    TEST_ASSERT_EQUAL_UINT32(1, made.size());
}

static void testSoundClock() {
    lv_obj_t* v = viewportOn(screen.slide(LOW));
    TEST_ASSERT_TRUE(player.play(v, "m", false, true));
    pass(0);
    // The sound waits, so the first frame stays.
    run(5 * FRAME_US);
    TEST_ASSERT_EQUAL_INT(0, shownAt());
    // The sound plays six frames' worth of samples.
    const uint32_t six = (6 * FRAME_SAMPLES + N - 1) / N + 2;
    blocks(six);
    pass(0);
    pass(0);
    const uint32_t played = audio.position(Sounds::MUSIC).played();
    TEST_ASSERT_EQUAL_INT(int(played / FRAME_SAMPLES), shownAt());
    run(5 * FRAME_US);
    TEST_ASSERT_EQUAL_INT(int(played / FRAME_SAMPLES), shownAt());
}

static uint32_t draws = 0;

static void countDraw(lv_event_t*) {
    ++draws;
}

static void testCovers() {
    lv_obj_t* below = viewportOn(screen.background());
    lv_obj_set_style_bg_opa(below, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(below, countDraw, LV_EVENT_DRAW_MAIN, nullptr);
    lv_obj_t* v = viewportOn(screen.slide(LOW));
    TEST_ASSERT_TRUE(player.play(v, "m", false, false));
    run(2 * FRAME_US);
    const int first = shownAt();
    draws = 0;
    run(3 * FRAME_US);
    TEST_ASSERT_EQUAL_INT(first + 3, shownAt());
    TEST_ASSERT_EQUAL_UINT32(0, draws);

    // Without the video the object behind it shows again and is drawn.
    player.stop(v);
    run(4 * FRAME_US);
    TEST_ASSERT_GREATER_THAN_UINT32(0, draws);
    TEST_ASSERT_EQUAL_INT(-1, shownAt());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(testPlay);
    RUN_TEST(testMovieEnds);
    RUN_TEST(testStop);
    RUN_TEST(testNoRoom);
    RUN_TEST(testOwner);
    RUN_TEST(testHidden);
    RUN_TEST(testLateCard);
    RUN_TEST(testShortSound);
    RUN_TEST(testSoundClock);
    RUN_TEST(testCovers);
    // Every read lands before the program ends, so no request waits in its destructor.
    GameHost::reset();
    service.serveAll();
    screen.render();
    return UNITY_END();
}
