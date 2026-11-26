// Tests on the computer the host's menu at the right edge: LVGL on a display that draws nowhere,
// with a pointer the test presses where it likes.
//
//   hint       the gold tab and the drawer's edge fade in, stand once the finger leaves, then fade
//              and the drawer goes out of sight
//   follow     the drawer's left edge follows the finger between open and closed, the hint goes
//   let-go     a finger let go past half the drawer's width snaps it open, before that back, and
//              close() snaps it back
//   choose     a press on a row, on End game and on the close in the header names its item, and
//              the rows show the host's status
//   while-open an open menu takes no hint
//   raise      raise() brings the menu in front of an overlay placed after it
//
// Run: pio test -e computer, which runs in CI as well.

#include "host/views/edge-menu-view.h"
#include <Arduino.h>
#include <unity.h>
#include <string>
#include <vector>

constexpr int32_t W = EdgeMenuView::SCREEN_WIDTH_PX;
constexpr int32_t H = EdgeMenuView::SCREEN_HEIGHT_PX;
constexpr int32_t OPEN_X = W - EdgeMenuView::WIDTH_PX;
constexpr uint32_t STEP_MS = 5;   // one pass of LVGL's timers
constexpr uint32_t REFRESH_MS =
    40;   // longer than LVGL's refresh period, which its animations step at
constexpr uint32_t BUFFER_ROWS = 10;
constexpr int32_t OVERLAY_PX = 44;   // an overlay's touch area in the screen's top right corner

// The pointer LVGL reads.
struct Pointer {
    bool down = false;
    int32_t x = 0;
    int32_t y = 0;
};
static Pointer pointer;
static lv_indev_t* indev = nullptr;
static uint8_t buffer[W * BUFFER_ROWS * 2];
static std::vector<EdgeMenuView::Item> chosen;

static uint32_t tick() {
    return fakeMillis;
}

static void flush(lv_display_t* d, const lv_area_t*, uint8_t*) {
    lv_display_flush_ready(d);
}

static void read(lv_indev_t*, lv_indev_data_t* data) {
    data->point.x = pointer.x;
    data->point.y = pointer.y;
    data->state = pointer.down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void note(EdgeMenuView::Item item, void* context) {
    TEST_ASSERT_EQUAL_PTR(&chosen, context);
    chosen.push_back(item);
}

// Runs LVGL's timers for ms.
static void run(uint32_t ms) {
    for (uint32_t t = 0; t < ms; t += STEP_MS) {
        fakeMillis += STEP_MS;
        lv_timer_handler();
    }
}

// Presses the pointer at a point and lets it go.
static void tap(int32_t x, int32_t y) {
    pointer = {true, x, y};
    lv_indev_read(indev);
    pointer.down = false;
    lv_indev_read(indev);
}

// A view built on a clean screen.
struct Rig {
    lv_obj_t* parent = nullptr;
    EdgeMenuView view;

    Rig() {
        parent = lv_obj_create(lv_screen_active());
        lv_obj_remove_style_all(parent);
        lv_obj_set_size(parent, W, H);
        lv_obj_set_clickable(parent, false);
        view.build(parent, note, &chosen);
        chosen.clear();
    }
    ~Rig() { lv_obj_delete(parent); }
};

void setUp() {
    if (indev == nullptr) {
        lv_init();
        lv_tick_set_cb(tick);
        lv_display_t* d = lv_display_create(W, H);
        lv_display_set_buffers(d, buffer, nullptr, sizeof buffer, LV_DISPLAY_RENDER_MODE_PARTIAL);
        lv_display_set_flush_cb(d, flush);
        indev = lv_indev_create();
        lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(indev, read);
        lv_timer_set_period(lv_indev_get_read_timer(indev), UINT32_MAX);
    }
    pointer = {};
}

void tearDown() {}

// hint: the hint fades in, stands HINT_STAY_MS once the finger leaves, then fades in
// HINT_FADE_MS, and the drawer goes back out of sight.
void testHint() {
    Rig rig;
    TEST_ASSERT_EQUAL_INT32(W, rig.view.menuLeft());
    TEST_ASSERT_EQUAL_UINT8(LV_OPA_TRANSP, rig.view.hintOpacity());
    rig.view.hint();
    TEST_ASSERT_EQUAL_INT32(W - EdgeMenuView::PEEK_PX, rig.view.menuLeft());
    run(EdgeMenuView::HINT_IN_MS / 2);
    const lv_opa_t half = rig.view.hintOpacity();
    TEST_ASSERT_TRUE(half > LV_OPA_TRANSP && half < LV_OPA_COVER);
    run(EdgeMenuView::HINT_IN_MS);
    TEST_ASSERT_EQUAL_UINT8(LV_OPA_COVER, rig.view.hintOpacity());

    rig.view.releaseHint();
    run(EdgeMenuView::HINT_STAY_MS - STEP_MS * 2);
    TEST_ASSERT_EQUAL_UINT8(LV_OPA_COVER, rig.view.hintOpacity());
    run(EdgeMenuView::HINT_FADE_MS / 2);
    const lv_opa_t fading = rig.view.hintOpacity();
    TEST_ASSERT_TRUE(fading > LV_OPA_TRANSP && fading < LV_OPA_COVER);
    run(EdgeMenuView::HINT_FADE_MS);
    TEST_ASSERT_EQUAL_UINT8(LV_OPA_TRANSP, rig.view.hintOpacity());
    TEST_ASSERT_EQUAL_INT32(W, rig.view.menuLeft());
    TEST_ASSERT_FALSE(rig.view.isOpen());

    // A finger that leaves while the hint still fades in lets it reach full strength first.
    rig.view.hint();
    run(EdgeMenuView::HINT_IN_MS / 3);
    rig.view.releaseHint();
    run(EdgeMenuView::HINT_IN_MS + REFRESH_MS);
    TEST_ASSERT_EQUAL_UINT8(LV_OPA_COVER, rig.view.hintOpacity());
    run(EdgeMenuView::HINT_STAY_MS + EdgeMenuView::HINT_FADE_MS);
    TEST_ASSERT_EQUAL_UINT8(LV_OPA_TRANSP, rig.view.hintOpacity());
}

// follow: the drawer's left edge follows the finger, held between open and closed, and the hint
// goes at once.
void testFollow() {
    Rig rig;
    rig.view.hint();
    run(EdgeMenuView::HINT_IN_MS);
    rig.view.follow(W - 30);
    TEST_ASSERT_EQUAL_INT32(W - 30, rig.view.menuLeft());
    TEST_ASSERT_EQUAL_UINT8(LV_OPA_TRANSP, rig.view.hintOpacity());
    rig.view.follow(200);
    TEST_ASSERT_EQUAL_INT32(200, rig.view.menuLeft());
    rig.view.follow(10);
    TEST_ASSERT_EQUAL_INT32(OPEN_X, rig.view.menuLeft());
    TEST_ASSERT_FALSE(rig.view.isOpen());
}

// let-go: a finger let go past half the drawer's width snaps it open in SNAP_MS, one let go before
// that lets it slide back, and close() snaps an open drawer back.
void testLetGo() {
    Rig rig;
    rig.view.follow(200);
    TEST_ASSERT_FALSE(rig.view.letGo(W - EdgeMenuView::WIDTH_PX / 2 + 10));
    run(EdgeMenuView::SNAP_MS + REFRESH_MS);
    TEST_ASSERT_EQUAL_INT32(W, rig.view.menuLeft());

    rig.view.follow(180);
    TEST_ASSERT_TRUE(rig.view.letGo(180));
    TEST_ASSERT_TRUE(rig.view.isOpen());
    run(EdgeMenuView::SNAP_MS / 2);
    TEST_ASSERT_TRUE(rig.view.menuLeft() < 180 && rig.view.menuLeft() > OPEN_X);
    run(EdgeMenuView::SNAP_MS);
    TEST_ASSERT_EQUAL_INT32(OPEN_X, rig.view.menuLeft());

    rig.view.close();
    TEST_ASSERT_FALSE(rig.view.isOpen());
    run(EdgeMenuView::SNAP_MS + REFRESH_MS);
    TEST_ASSERT_EQUAL_INT32(W, rig.view.menuLeft());
}

// choose: a press on each row, on End game and on the close in the header names its item, and the
// rows show what show() set.
void testChoose() {
    Rig rig;
    rig.view.follow(OPEN_X);
    rig.view.letGo(OPEN_X);
    run(EdgeMenuView::SNAP_MS + REFRESH_MS);
    // the rows from the header down, End game at the bottom, the close at the header's right end
    tap(OPEN_X + 150, 68);
    tap(OPEN_X + 150, 116);
    tap(OPEN_X + 150, 164);
    tap(OPEN_X + 150, 214);
    tap(W - 10, 17);
    tap(OPEN_X + 100, 38);
    const std::vector<EdgeMenuView::Item> want = {
        EdgeMenuView::Item::SOUND, EdgeMenuView::Item::LIGHTS, EdgeMenuView::Item::MACHINE,
        EdgeMenuView::Item::END_GAME, EdgeMenuView::Item::CLOSE};
    TEST_ASSERT_EQUAL_UINT32(want.size(), chosen.size());
    for (size_t i = 0; i < want.size(); ++i) {
        TEST_ASSERT_EQUAL_UINT8(uint8_t(want[i]), uint8_t(chosen[i]));
    }

    EdgeMenuView::Status status;
    status.sound = "50 %";
    status.lights = "80 %";
    status.machine = "1 fault";
    status.fault = true;
    rig.view.show(status);
    std::vector<std::string> texts;
    for (int32_t y : {68, 116, 164}) {
        lv_point_t at = {OPEN_X + 150, y};
        lv_obj_t* row = lv_indev_search_obj(lv_screen_active(), &at);
        TEST_ASSERT_NOT_NULL(row);
        texts.push_back(lv_label_get_text(lv_obj_get_child(row, 0)));
    }
    TEST_ASSERT_EQUAL_STRING("50 %", texts[0].c_str());
    TEST_ASSERT_EQUAL_STRING("80 %", texts[1].c_str());
    TEST_ASSERT_EQUAL_STRING("1 fault", texts[2].c_str());
}

// while-open: an open menu takes no hint, and follow() leaves it where it stands.
void testWhileOpen() {
    Rig rig;
    rig.view.follow(OPEN_X);
    rig.view.letGo(OPEN_X);
    run(EdgeMenuView::SNAP_MS + REFRESH_MS);
    rig.view.hint();
    rig.view.follow(250);
    run(EdgeMenuView::HINT_IN_MS + REFRESH_MS);
    TEST_ASSERT_EQUAL_INT32(OPEN_X, rig.view.menuLeft());
    TEST_ASSERT_EQUAL_UINT8(LV_OPA_TRANSP, rig.view.hintOpacity());
}

// raise: an overlay placed after the menu takes the touch on the menu's close until raise() brings
// the menu in front of it.
void testRaise() {
    Rig rig;
    lv_obj_t* overlay = lv_obj_create(rig.parent);
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, OVERLAY_PX, OVERLAY_PX);
    lv_obj_align(overlay, LV_ALIGN_TOP_RIGHT, 0, 0);
    rig.view.follow(OPEN_X);
    rig.view.letGo(OPEN_X);
    run(EdgeMenuView::SNAP_MS + REFRESH_MS);
    tap(W - 10, 17);
    TEST_ASSERT_EQUAL_UINT32(0, chosen.size());
    rig.view.raise();
    tap(W - 10, 17);
    TEST_ASSERT_EQUAL_UINT32(1, chosen.size());
    TEST_ASSERT_EQUAL_UINT8(uint8_t(EdgeMenuView::Item::CLOSE), uint8_t(chosen[0]));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(testHint);
    RUN_TEST(testFollow);
    RUN_TEST(testLetGo);
    RUN_TEST(testChoose);
    RUN_TEST(testWhileOpen);
    RUN_TEST(testRaise);
    return UNITY_END();
}
