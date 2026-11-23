// Tests on the computer the screen component: LVGL with a fake display driver, pictures from a fake
// card in memory, and the picture memory.
//
//   widen    every rectangle the driver gets spans whole rows and covers the change
//   slides   the slide of the highest priority shows, the later of equal priorities, a mode gets
//            its slide again, only the slide that shows takes a touch, and the next slide shows
//            once the top one is removed
//   memory   a frame gets the room a full-screen picture gave back, and a full memory fails a load
//   reset    a switch of games deletes the game's objects and slides and releases its pictures,
//            and the memory empties once the reads of a picture and a frame have landed
//   restart  the display restarts, nothing goes out meanwhile, and the whole screen goes out once
//            the display is back
//   covered  LVGL leaves out an object that another object covers, when the whole screen changes
//   picture-covers  LVGL draws nothing behind a picture that holds no opacity, and draws what lies
//            in front of it
//
// Run: pio test -e computer, which runs in CI as well.

#include "components/file-system.h"
#include "components/screen.h"
#include "components/sounds.h"
#include "components/video-player.h"
#include "io/psram.h"
#include <map>
#include <memory>
#include <vector>
#include <string>
#include <unity.h>

constexpr uint16_t W = Display::WIDTH;
constexpr uint16_t H = Display::HEIGHT;
constexpr uint32_t REFRESH_MS = 40;   // longer than LVGL's refresh period
constexpr char GAME[] = "a";
constexpr uint8_t MAGIC = 0x19;   // LV_IMAGE_HEADER_MAGIC
constexpr uint32_t FULL_BYTES = uint32_t(W) * H * 2;
constexpr size_t HEADER_BYTES = 12;   // LVGL's image header

// A card in memory. Each file is a string, each slot names the file it holds.
struct FakeCard : CardFiles {
    std::map<std::string, std::string> files;
    std::string slotPath[SLOTS];

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

// The host's calls to the screen, the file system and the video player.
class GameHost {
public:
    static void use(const char* game) { files.use(game); }
    static void reset() {
        player.reset();
        screen.reset();
    }
    static bool settled() { return screen.settled(); }
    static void restart() { screen.restart(); }
    static PictureMemory& pictures() { return screen.pictures; }
    static lv_obj_t* top() { return screen.top(); }
};

static void put16(std::string& s, uint16_t v) {
    s.push_back(char(v & 0xFF));
    s.push_back(char(v >> 8));
}

// Gets a picture file of one color, opaque or with an opacity byte for each pixel.
static std::string pictureFile(uint16_t width, uint16_t height, uint16_t color, bool opaque) {
    std::string f;
    f.push_back(char(MAGIC));
    f.push_back(char(opaque ? LV_COLOR_FORMAT_RGB565_SWAPPED : LV_COLOR_FORMAT_RGB565A8));
    put16(f, 0);
    put16(f, width);
    put16(f, height);
    put16(f, uint16_t(width * 2));
    put16(f, 0);
    for (uint32_t i = 0; i < uint32_t(width) * height; ++i) {
        if (opaque) {
            f.push_back(char(color >> 8));
            f.push_back(char(color & 0xFF));
        } else {
            put16(f, color);
        }
    }
    if (!opaque) {
        f.append(size_t(width) * height, char(0xFF));
    }
    return f;
}

// Lets the storage task and the main loop run until every read has landed.
static void settle() {
    for (int i = 0; i < 4; ++i) {
        service.serveAll();
        screen.render();
    }
}

// Lets LVGL's refresh come due and runs the screen.
static void refresh() {
    fakeMillis += REFRESH_MS;
    GPT2_CNT = GPT2_CNT + REFRESH_MS * 1000;
    screen.render();
}

static lv_obj_t* box(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_style_bg_color(o, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    return o;
}

void setUp() {
    static bool begun = false;
    if (!begun) {
        begun = true;
        service.mount();
        GameHost::use(GAME);
        audio.begin();
        sounds.begin();
        screen.begin();
        player.begin();
    }
    GameHost::reset();
    settle();
    refresh();
    display.rects.clear();
    display.hold = false;
    display.panelBack = true;
}

void tearDown() {}

static void testWiden() {
    const Layer layer = {1, 10};
    lv_obj_t* s = screen.slide(layer);
    refresh();
    display.rects.clear();
    for (int32_t x = 0; x < W; x += 37) {
        const int32_t y = 3 + x / 8;
        lv_obj_t* o = box(s, x, y, 1 + x % 7, 5);
        refresh();
        TEST_ASSERT_EQUAL_UINT32(1, display.rects.size());
        const Display::Rect r = display.rects[0];
        TEST_ASSERT_EQUAL_UINT16(0, r.x);
        TEST_ASSERT_EQUAL_UINT16(W, r.width);
        TEST_ASSERT_TRUE(r.y <= y);
        TEST_ASSERT_TRUE(r.y + r.height >= y + 5);
        TEST_ASSERT_EQUAL_HEX16(0xFFFF, display.pixel(uint16_t(x), uint16_t(y)));
        display.rects.clear();
        lv_obj_delete(o);
        refresh();
        TEST_ASSERT_EQUAL_HEX16(0x0000, display.pixel(uint16_t(x), uint16_t(y)));
        display.rects.clear();
    }
    // Two changes far apart in one refresh reach the driver as one rectangle from the first
    // changed row to the last.
    box(s, 0, 10, 16, 4);
    box(s, 300, 200, 20, 4);
    refresh();
    TEST_ASSERT_EQUAL_UINT32(1, display.rects.size());
    TEST_ASSERT_TRUE(display.rects[0].y <= 10);
    TEST_ASSERT_TRUE(display.rects[0].y + display.rects[0].height >= 204);
    TEST_ASSERT_EQUAL_UINT32(0, display.refused);
    screen.remove(layer);
}

static void testSlides() {
    const Layer low = {1, 10};
    const Layer middle = {2, 50};
    const Layer middleLater = {3, 50};
    lv_obj_t* a = screen.slide(low);
    lv_obj_t* b = screen.slide(middle);
    lv_obj_t* c = screen.slide(middleLater);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_TRUE(a != b && b != c);
    TEST_ASSERT_EQUAL_PTR(c, GameHost::top());
    TEST_ASSERT_TRUE(lv_obj_is_hidden(a));
    TEST_ASSERT_TRUE(lv_obj_is_hidden(b));
    TEST_ASSERT_FALSE(lv_obj_is_hidden(c));
    TEST_ASSERT_EQUAL_PTR(b, screen.slide(middle));
    TEST_ASSERT_EQUAL_PTR(c, GameHost::top());

    // A button at the same place on each slide: LVGL finds only the one of the slide that shows.
    lv_obj_t* buttons[3];
    lv_obj_t* slides[3] = {a, b, c};
    for (int i = 0; i < 3; ++i) {
        buttons[i] = lv_button_create(slides[i]);
        lv_obj_set_pos(buttons[i], 100, 100);
        lv_obj_set_size(buttons[i], 50, 30);
    }
    lv_obj_update_layout(lv_screen_active());
    lv_point_t p = {120, 110};
    TEST_ASSERT_EQUAL_PTR(buttons[2], lv_indev_search_obj(lv_screen_active(), &p));

    screen.remove(middleLater);
    TEST_ASSERT_EQUAL_PTR(b, GameHost::top());
    TEST_ASSERT_FALSE(lv_obj_is_hidden(b));
    TEST_ASSERT_TRUE(lv_obj_is_hidden(a));
    TEST_ASSERT_EQUAL_PTR(buttons[1], lv_indev_search_obj(lv_screen_active(), &p));
    screen.remove(middle);
    TEST_ASSERT_EQUAL_PTR(a, GameHost::top());
    TEST_ASSERT_EQUAL_PTR(buttons[0], lv_indev_search_obj(lv_screen_active(), &p));
    screen.remove(low);
    TEST_ASSERT_NULL(GameHost::top());
    TEST_ASSERT_NULL(lv_indev_search_obj(lv_screen_active(), &p));

    // An animation on each of two slides, and one started on the covered slide: only the slide
    // that shows moves, and the covered one goes on where it stood once it shows.
    lv_obj_t* under = screen.slide(low);
    lv_obj_t* over = screen.slide(middle);
    lv_obj_t* moving[3] = {box(under, 0, 0, 10, 10), box(over, 0, 0, 10, 10),
                           box(under, 0, 50, 10, 10)};
    static int deletedCalls = 0;
    deletedCalls = 0;
    for (int i = 0; i < 3; ++i) {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, moving[i]);
        lv_anim_set_values(&a, 0, 200);
        lv_anim_set_duration(&a, 2000);
        lv_anim_set_exec_cb(&a,
                            [](void* o, int32_t v) { lv_obj_set_x(static_cast<lv_obj_t*>(o), v); });
        lv_anim_set_deleted_cb(&a, [](lv_anim_t*) { ++deletedCalls; });
        TEST_ASSERT_NOT_NULL(screen.animate(a));
        if (i == 0) {
            refresh();
        }
    }
    for (int i = 0; i < 10; ++i) {
        refresh();
    }
    const int32_t underStood = lv_obj_get_x(moving[0]);
    TEST_ASSERT_TRUE(lv_obj_get_x(moving[1]) > 0);
    for (int i = 0; i < 10; ++i) {
        refresh();
    }
    TEST_ASSERT_EQUAL_INT32(underStood, lv_obj_get_x(moving[0]));
    TEST_ASSERT_EQUAL_INT32(0, lv_obj_get_x(moving[2]));
    screen.remove(middle);
    TEST_ASSERT_EQUAL_INT(1, deletedCalls);
    refresh();
    refresh();
    TEST_ASSERT_TRUE(lv_obj_get_x(moving[0]) > underStood);
    TEST_ASSERT_TRUE(lv_obj_get_x(moving[2]) > 0);
    // The animations end, and the functions their starter named run.
    for (int i = 0; i < 80; ++i) {
        refresh();
    }
    TEST_ASSERT_EQUAL_INT(3, deletedCalls);
    TEST_ASSERT_EQUAL_INT32(200, lv_obj_get_x(moving[0]));
    screen.remove(low);

    // A ninth mode gets no slide.
    for (uint8_t k = 1; k <= Screen::SLIDES; ++k) {
        TEST_ASSERT_NOT_NULL(screen.slide({k, k}));
    }
    TEST_ASSERT_NULL(screen.slide({Screen::SLIDES + 1, 1}));
}

static void testMemory() {
    card.files["/games/a/full.img"] = pictureFile(W, H, 0xF800, true);
    card.files["/games/a/small.img"] = pictureFile(40, 30, 0x07E0, true);
    // A picture stays in the screen's list for good, as a game's pictures live as long as the
    // firmware.
    static Picture full(screen);
    static Picture small1(screen);
    static Picture small2(screen);
    full.load(files.open("full.img"));
    small1.load(files.open("small.img"));
    small2.load(files.open("small.img"));
    TEST_ASSERT_TRUE(full.loading());
    settle();
    TEST_ASSERT_FALSE(full.loading());
    TEST_ASSERT_FALSE(full.failed());
    TEST_ASSERT_FALSE(small1.failed());
    TEST_ASSERT_FALSE(small2.failed());

    // The frame lands where the full-screen picture lay.
    PictureMemory& memory = GameHost::pictures();
    lv_obj_t* shown = full.show(screen.background());
    const auto* dsc = static_cast<const lv_image_dsc_t*>(lv_image_get_src(shown));
    TEST_ASSERT_NOT_NULL(dsc);
    const uint8_t* fullRoom = dsc->data - HEADER_BYTES;
    full.release();
    uint8_t* frame = memory.take(FULL_BYTES, true);
    TEST_ASSERT_TRUE(frame >= fullRoom);
    TEST_ASSERT_TRUE(frame < fullRoom + HEADER_BYTES + PictureMemory::ALIGN_BYTES);
    TEST_ASSERT_EQUAL_UINT32(0, reinterpret_cast<uintptr_t>(frame) % PictureMemory::ALIGN_BYTES);

    // Load full-screen pictures until the memory is full. Each takes its header and pixels,
    // rounded up to 32 bytes, beside the two small pictures and the frame.
    const size_t pictureRoom = (HEADER_BYTES + FULL_BYTES + 31) / 32 * 32;
    const size_t smallRoom = (HEADER_BYTES + 40 * 30 * 2 + 31) / 32 * 32;
    const size_t total = io::fakePsramBytes / 32 * 32 - 32;
    const size_t fitting = (total - 2 * smallRoom - FULL_BYTES) / pictureRoom;
    static std::vector<std::unique_ptr<Picture>> more;
    bool refused = false;
    while (!refused && more.size() < 64) {
        more.push_back(std::make_unique<Picture>(screen));
        more.back()->load(files.open("full.img"));
        settle();
        refused = more.back()->failed();
    }
    TEST_ASSERT_TRUE(refused);
    TEST_ASSERT_TRUE(more.size() - 1 >= fitting && more.size() - 1 <= fitting + 1);
    memory.give(frame);
    GameHost::reset();
    settle();
    TEST_ASSERT_TRUE(GameHost::settled());
}

static void testReset() {
    card.files["/games/a/full.img"] = pictureFile(W, H, 0x001F, true);
    std::string movie(size_t(FULL_BYTES) * 3, '\0');
    card.files["/games/a/video/m.rgb"] = movie;
    lv_obj_t* s = screen.slide({1, 10});
    lv_obj_t* back = box(screen.background(), 0, 0, 10, 10);
    static Picture p(screen);
    p.load(files.open("full.img"));
    lv_obj_t* image = p.show(s);
    settle();
    TEST_ASSERT_TRUE(lv_obj_is_valid(image));
    p.load(files.open("full.img"));
    service.serveAll();
    screen.render();
    // The pixels' read is on its way now, and the video's first read as well.
    lv_obj_t* viewport = lv_obj_create(s);
    lv_obj_remove_style_all(viewport);
    lv_obj_set_size(viewport, W, H);
    TEST_ASSERT_TRUE(player.play(viewport, "m", false, false));
    TEST_ASSERT_TRUE(p.loading());

    GameHost::reset();
    TEST_ASSERT_FALSE(lv_obj_is_valid(s));
    TEST_ASSERT_FALSE(lv_obj_is_valid(back));
    TEST_ASSERT_NULL(GameHost::top());
    TEST_ASSERT_FALSE(player.playing(viewport));
    TEST_ASSERT_FALSE(GameHost::settled());
    settle();
    TEST_ASSERT_TRUE(GameHost::settled());
    TEST_ASSERT_FALSE(p.loading());
}

static void testRestart() {
    const Layer layer = {1, 10};
    lv_obj_t* s = screen.slide(layer);
    box(s, 40, 50, 20, 20);
    refresh();
    display.rects.clear();
    const uint32_t before = display.restarts;
    const uint32_t refusedBefore = display.refused;
    GameHost::restart();
    TEST_ASSERT_EQUAL_UINT32(before + 1, display.restarts);

    // LVGL rests while the display restarts, so a change meanwhile reaches no rectangle.
    box(s, 200, 100, 20, 20);
    refresh();
    refresh();
    TEST_ASSERT_EQUAL_UINT32(0, display.rects.size());

    // The first refresh after the display is back sends every row, with the change in it.
    display.panelBack = true;
    refresh();
    TEST_ASSERT_EQUAL_UINT32(1, display.rects.size());
    const Display::Rect r = display.rects[0];
    TEST_ASSERT_EQUAL_UINT16(0, r.x);
    TEST_ASSERT_EQUAL_UINT16(0, r.y);
    TEST_ASSERT_EQUAL_UINT16(W, r.width);
    TEST_ASSERT_EQUAL_UINT16(H, r.height);
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, display.pixel(45, 55));
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, display.pixel(205, 105));
    TEST_ASSERT_EQUAL_HEX16(0x0000, display.pixel(0, 0));

    // The whole screen goes out once: a refresh without a change sends nothing.
    display.rects.clear();
    refresh();
    TEST_ASSERT_EQUAL_UINT32(0, display.rects.size());
    TEST_ASSERT_EQUAL_UINT32(refusedBefore, display.refused);
    screen.remove(layer);
}

static uint32_t draws = 0;

static void countDraw(lv_event_t*) {
    ++draws;
}

static void testCovered() {
    const Layer layer = {1, 10};
    lv_obj_t* below = box(screen.background(), 0, 0, W, H);
    lv_obj_add_event_cb(below, countDraw, LV_EVENT_DRAW_MAIN, nullptr);
    refresh();
    TEST_ASSERT_GREATER_THAN_UINT32(0, draws);

    // An object that fills the screen on a slide covers the background's object.
    lv_obj_t* above = box(screen.slide(layer), 0, 0, W, H);
    lv_obj_set_style_bg_color(above, lv_color_black(), 0);
    refresh();
    draws = 0;
    display.rects.clear();
    lv_obj_invalidate(above);
    refresh();
    TEST_ASSERT_EQUAL_UINT32(1, display.rects.size());
    TEST_ASSERT_EQUAL_UINT32(0, draws);
    TEST_ASSERT_EQUAL_HEX16(0x0000, display.pixel(10, 10));

    // Without the slide the background's object shows again and is drawn.
    screen.remove(layer);
    refresh();
    TEST_ASSERT_GREATER_THAN_UINT32(0, draws);
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, display.pixel(10, 10));
    lv_obj_delete(below);
}

static void testPictureCovers() {
    card.files["/games/a/opaque.img"] = pictureFile(W, H, 0xF800, true);
    card.files["/games/a/clear.img"] = pictureFile(W, H, 0x07E0, false);
    lv_obj_t* below = box(screen.background(), 0, 0, W, H);
    lv_obj_add_event_cb(below, countDraw, LV_EVENT_DRAW_MAIN, nullptr);
    lv_obj_t* s = screen.slide({1, 10});
    static Picture opaque(screen);
    static Picture clear(screen);

    // The image gets its pixels once the picture has loaded. An object lies in front of it.
    opaque.load(files.open("opaque.img"));
    lv_obj_t* early = opaque.show(s);
    box(s, 10, 10, 20, 20);
    settle();
    refresh();
    draws = 0;
    lv_obj_invalidate(early);
    refresh();
    TEST_ASSERT_EQUAL_UINT32(0, draws);
    TEST_ASSERT_EQUAL_HEX16(0xF800, display.pixel(100, 100));
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, display.pixel(15, 15));

    // An image of the loaded picture covers as well.
    lv_obj_delete(early);
    lv_obj_t* late = opaque.show(s);
    refresh();
    draws = 0;
    lv_obj_invalidate(late);
    refresh();
    TEST_ASSERT_EQUAL_UINT32(0, draws);
    TEST_ASSERT_EQUAL_HEX16(0xF800, display.pixel(100, 100));

    // A picture with opacity leaves what lies behind it drawn.
    opaque.release();
    clear.load(files.open("clear.img"));
    lv_obj_t* through = clear.show(s);
    settle();
    refresh();
    draws = 0;
    lv_obj_invalidate(through);
    refresh();
    TEST_ASSERT_GREATER_THAN_UINT32(0, draws);
    TEST_ASSERT_EQUAL_HEX16(0x07E0, display.pixel(100, 100));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(testWiden);
    RUN_TEST(testSlides);
    RUN_TEST(testMemory);
    RUN_TEST(testReset);
    RUN_TEST(testRestart);
    RUN_TEST(testCovered);
    RUN_TEST(testPictureCovers);
    return UNITY_END();
}
