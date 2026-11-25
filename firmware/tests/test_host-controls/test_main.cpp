// Tests on the computer the host screens' controls: LVGL on a display that draws nowhere, with a
// pointer the test presses.
//
//   button    a plaque is a touch target high, in its kind's colors, sinks when pressed and takes
//             no touch once disabled
//   header    the band and its bulbs span the parent, the title stands centered or at the left,
//             and a header button at either end takes the touch
//   bulb      each light shows its colors, and an unlit bulb casts no glow
//   row       a row shows its title and detail, takes the touch, and changes its detail
//   slider    minus and plus step the value, stop at the ends, and tell the listeners
//   dialog    a dialog is a night card with its title in the strip, dims what lies beneath it,
//             and no control beneath it takes a touch until it closes
//   fonts     every font holds the printable characters and finds LVGL's symbols in its fallback
//
// Run: pio test -e computer, which runs in CI as well.

#include "host/views/controls.h"
#include <Arduino.h>
#include <string.h>
#include <unity.h>

constexpr int32_t W = 320;
constexpr int32_t H = 240;
constexpr uint32_t STEP_MS = 5;   // one pass of LVGL's timers
constexpr uint32_t BUFFER_ROWS = 10;
constexpr int32_t WIDTH = 120;

struct Pointer {
    bool down = false;
    int32_t x = 0;
    int32_t y = 0;
};
static Pointer pointer;
static lv_indev_t* indev = nullptr;
static uint8_t buffer[W * BUFFER_ROWS * 2];
static uint32_t clicks = 0;
static uint32_t changes = 0;

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

static void counted(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ++clicks;
    } else {
        ++changes;
    }
}

// Lays the objects out, so their coordinates hold.
static void settle() {
    fakeMillis += STEP_MS;
    lv_timer_handler();
    lv_obj_update_layout(lv_screen_active());
}

// Presses the pointer at the middle of an object and lets it go.
static void tap(lv_obj_t* o) {
    settle();
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    pointer = {true, (a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2};
    lv_indev_read(indev);
    pointer.down = false;
    lv_indev_read(indev);
}

static uint32_t hex(lv_color_t c) {
    return lv_color_to_u32(c) & 0xFFFFFF;
}

// A clean full-screen parent.
struct Rig {
    lv_obj_t* parent = nullptr;
    Rig() {
        parent = lv_obj_create(lv_screen_active());
        lv_obj_remove_style_all(parent);
        lv_obj_set_size(parent, W, H);
        lv_obj_set_clickable(parent, false);
        clicks = 0;
        changes = 0;
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

// button: a plaque is TARGET_PX high in its kind's colors, sinks when pressed, and takes no touch
// once disabled.
void testButton() {
    Rig rig;
    lv_obj_t* red = controls::button(rig.parent, controls::Plaque::RED, "START", WIDTH);
    lv_obj_t* gold = controls::button(rig.parent, controls::Plaque::GOLD, "PLAY", WIDTH);
    lv_obj_t* violet = controls::button(rig.parent, controls::Plaque::VIOLET, "CANCEL", WIDTH);
    lv_obj_set_y(gold, 50);
    lv_obj_set_y(violet, 100);
    settle();
    TEST_ASSERT_EQUAL_INT32(controls::TARGET_PX, lv_obj_get_height(red));
    TEST_ASSERT_EQUAL_INT32(WIDTH, lv_obj_get_width(red));
    TEST_ASSERT_EQUAL_STRING("START", lv_label_get_text(lv_obj_get_child(red, 0)));
    TEST_ASSERT_EQUAL_PTR(&archivo_black_14,
                          lv_obj_get_style_text_font(lv_obj_get_child(red, 0), LV_PART_MAIN));
    TEST_ASSERT_EQUAL_HEX32(controls::color::CABINET,
                            hex(lv_obj_get_style_bg_grad_color(red, LV_PART_MAIN)));
    TEST_ASSERT_EQUAL_HEX32(controls::color::GOLD,
                            hex(lv_obj_get_style_border_color(red, LV_PART_MAIN)));
    TEST_ASSERT_EQUAL_HEX32(controls::color::BULB,
                            hex(lv_obj_get_style_border_color(gold, LV_PART_MAIN)));
    TEST_ASSERT_EQUAL_HEX32(controls::color::VELVET,
                            hex(lv_obj_get_style_bg_color(violet, LV_PART_MAIN)));
    TEST_ASSERT_EQUAL_HEX32(controls::color::CREAM, hex(lv_obj_get_style_text_color(
                                                        lv_obj_get_child(red, 0), LV_PART_MAIN)));

    lv_obj_add_event_cb(red, counted, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_state(red, LV_STATE_PRESSED);
    TEST_ASSERT_EQUAL_HEX32(controls::color::SHADOW,
                            hex(lv_obj_get_style_bg_color(red, LV_PART_MAIN)));
    lv_obj_remove_state(red, LV_STATE_PRESSED);
    tap(red);
    TEST_ASSERT_EQUAL_UINT32(1, clicks);
    lv_obj_add_state(red, LV_STATE_DISABLED);
    tap(red);
    TEST_ASSERT_EQUAL_UINT32(1, clicks);
    TEST_ASSERT_TRUE(hex(lv_obj_get_style_bg_color(red, LV_PART_MAIN)) != controls::color::SHADOW);
}

// header: the band and its bulbs span the parent, the title stands centered or at the left, and a
// header button at either end takes the touch.
void testHeader() {
    Rig rig;
    lv_obj_t* h = controls::header(rig.parent, "SOUND", true);
    lv_obj_t* back = controls::headerButton(h, true, LV_SYMBOL_LEFT);
    lv_obj_t* gear = controls::headerButton(h, false, LV_SYMBOL_SETTINGS);
    lv_obj_add_event_cb(back, counted, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(gear, counted, LV_EVENT_CLICKED, nullptr);
    settle();
    TEST_ASSERT_EQUAL_INT32(W, lv_obj_get_width(h));
    TEST_ASSERT_EQUAL_INT32(controls::HEADER_PX + controls::BULBS_PX, lv_obj_get_height(h));
    lv_obj_t* title = lv_obj_get_child(lv_obj_get_child(h, 0), 0);
    TEST_ASSERT_EQUAL_PTR(&archivo_black_17, lv_obj_get_style_text_font(title, LV_PART_MAIN));
    lv_area_t t;
    lv_obj_get_coords(title, &t);
    TEST_ASSERT_INT32_WITHIN(1, W / 2, (t.x1 + t.x2 + 1) / 2);
    lv_area_t b;
    lv_obj_get_coords(back, &b);
    TEST_ASSERT_EQUAL_INT32(0, b.x1);
    TEST_ASSERT_EQUAL_INT32(controls::TARGET_PX, lv_obj_get_width(back));
    lv_obj_get_coords(gear, &b);
    TEST_ASSERT_EQUAL_INT32(W - 1, b.x2);
    tap(back);
    tap(gear);
    TEST_ASSERT_EQUAL_UINT32(2, clicks);

    lv_obj_t* left = controls::header(rig.parent, "MENU", false);
    lv_obj_set_y(left, 100);
    settle();
    lv_obj_get_coords(lv_obj_get_child(lv_obj_get_child(left, 0), 0), &t);
    TEST_ASSERT_TRUE(t.x1 < W / 4);
}

// bulb: each light shows its colors, and an unlit bulb casts no glow.
void testBulb() {
    Rig rig;
    lv_obj_t* b = controls::bulb(rig.parent, controls::Light::WORKS);
    TEST_ASSERT_EQUAL_INT32(14, lv_obj_get_style_width(b, LV_PART_MAIN));
    TEST_ASSERT_EQUAL_HEX32(controls::color::GREEN,
                            hex(lv_obj_get_style_shadow_color(b, LV_PART_MAIN)));
    TEST_ASSERT_TRUE(lv_obj_get_style_shadow_width(b, LV_PART_MAIN) > 0);
    controls::setLight(b, controls::Light::FAILED);
    TEST_ASSERT_EQUAL_HEX32(controls::color::NEON,
                            hex(lv_obj_get_style_shadow_color(b, LV_PART_MAIN)));
    controls::setLight(b, controls::Light::WARNING);
    TEST_ASSERT_EQUAL_HEX32(controls::color::AMBER,
                            hex(lv_obj_get_style_shadow_color(b, LV_PART_MAIN)));
    controls::setLight(b, controls::Light::LIT);
    TEST_ASSERT_EQUAL_HEX32(controls::color::BULB,
                            hex(lv_obj_get_style_shadow_color(b, LV_PART_MAIN)));
    controls::setLight(b, controls::Light::UNLIT);
    TEST_ASSERT_EQUAL_INT32(0, lv_obj_get_style_shadow_width(b, LV_PART_MAIN));
}

// row: a row shows its title and detail, takes the touch, and changes its detail.
void testRow() {
    Rig rig;
    lv_obj_t* r = controls::row(rig.parent, "Record ball sensors", "After a change", 300);
    lv_obj_add_event_cb(r, counted, LV_EVENT_CLICKED, nullptr);
    settle();
    TEST_ASSERT_EQUAL_INT32(controls::ROW_PX, lv_obj_get_height(r));
    TEST_ASSERT_EQUAL_STRING("After a change", lv_label_get_text(lv_obj_get_child(r, 0)));
    TEST_ASSERT_EQUAL_STRING("Record ball sensors", lv_label_get_text(lv_obj_get_child(r, 1)));
    TEST_ASSERT_EQUAL_PTR(&montserrat_semibold_14,
                          lv_obj_get_style_text_font(lv_obj_get_child(r, 1), LV_PART_MAIN));
    tap(r);
    TEST_ASSERT_EQUAL_UINT32(1, clicks);
    controls::setDetail(r, "1 fault", controls::color::FAULT_TEXT);
    TEST_ASSERT_EQUAL_STRING("1 fault", lv_label_get_text(lv_obj_get_child(r, 0)));
    TEST_ASSERT_EQUAL_HEX32(controls::color::FAULT_TEXT,
                            hex(lv_obj_get_style_text_color(lv_obj_get_child(r, 0), LV_PART_MAIN)));

    lv_obj_t* menu = controls::row(rig.parent, "Sound", "", 200, controls::TARGET_PX,
                                   LV_SYMBOL_VOLUME_MAX, controls::color::RAMP);
    TEST_ASSERT_EQUAL_INT32(controls::TARGET_PX, lv_obj_get_style_height(menu, LV_PART_MAIN));
    lv_obj_t* icon = lv_obj_get_child(menu, -1);
    TEST_ASSERT_EQUAL_STRING(LV_SYMBOL_VOLUME_MAX, lv_label_get_text(icon));
    TEST_ASSERT_EQUAL_HEX32(controls::color::RAMP,
                            hex(lv_obj_get_style_text_color(icon, LV_PART_MAIN)));
}

// slider: minus and plus step the value by STEP_PERCENT, stop at 0 and SLIDER_MAX, and tell the
// listeners of the track.
void testSlider() {
    Rig rig;
    lv_obj_t* s = controls::slider(rig.parent, controls::Fill::SOUND, 304);
    lv_obj_t* track = controls::sliderTrack(s);
    lv_obj_add_event_cb(track, counted, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_t* minus = lv_obj_get_child(s, 0);
    lv_obj_t* plus = lv_obj_get_child(s, 1);
    settle();
    TEST_ASSERT_EQUAL_INT32(controls::TARGET_PX, lv_obj_get_height(s));
    TEST_ASSERT_EQUAL_INT32(0, lv_slider_get_value(track));
    tap(plus);
    tap(plus);
    TEST_ASSERT_EQUAL_INT32(2 * controls::STEP_PERCENT, lv_slider_get_value(track));
    TEST_ASSERT_EQUAL_UINT32(2, changes);
    tap(minus);
    tap(minus);
    tap(minus);
    TEST_ASSERT_EQUAL_INT32(0, lv_slider_get_value(track));
    lv_slider_set_value(track, controls::SLIDER_MAX - 2, LV_ANIM_OFF);
    tap(plus);
    TEST_ASSERT_EQUAL_INT32(controls::SLIDER_MAX, lv_slider_get_value(track));
    TEST_ASSERT_EQUAL_HEX32(controls::color::RAMP,
                            hex(lv_obj_get_style_bg_grad_color(track, LV_PART_INDICATOR)));
}

// dialog: a dialog is a night card with a gold rim and its title in the strip, dims what lies
// beneath it, and no control beneath it takes a touch until it closes.
void testDialog() {
    Rig rig;
    lv_obj_t* beneath = controls::button(rig.parent, controls::Plaque::GOLD, "PLAY", 100);
    lv_obj_add_event_cb(beneath, counted, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* d = controls::dialog(rig.parent, "END CLASSIC?", 280, 166);
    settle();
    lv_area_t card;
    lv_obj_get_coords(d, &card);
    TEST_ASSERT_EQUAL_INT32((W - 280) / 2, card.x1);
    TEST_ASSERT_EQUAL_INT32((H - 166) / 2, card.y1);
    TEST_ASSERT_EQUAL_INT32(280, lv_obj_get_width(d));
    TEST_ASSERT_EQUAL_INT32(166, lv_obj_get_height(d));
    TEST_ASSERT_EQUAL_HEX32(controls::color::NIGHT,
                            hex(lv_obj_get_style_bg_color(d, LV_PART_MAIN)));
    TEST_ASSERT_EQUAL_HEX32(controls::color::GOLD,
                            hex(lv_obj_get_style_border_color(d, LV_PART_MAIN)));
    lv_obj_t* title = lv_obj_get_child(lv_obj_get_child(d, 0), 0);
    TEST_ASSERT_EQUAL_STRING("END CLASSIC?", lv_label_get_text(title));
    TEST_ASSERT_TRUE(lv_obj_is_clickable(d));
    // the backdrop dims the parent behind the card
    lv_obj_t* backdrop = lv_obj_get_parent(d);
    TEST_ASSERT_EQUAL_INT32(W, lv_obj_get_width(backdrop));
    TEST_ASSERT_EQUAL_INT32(H, lv_obj_get_height(backdrop));
    TEST_ASSERT_EQUAL_HEX32(0x000000, hex(lv_obj_get_style_bg_color(backdrop, LV_PART_MAIN)));
    TEST_ASSERT_EQUAL_UINT8(LV_OPA_50, lv_obj_get_style_bg_opa(backdrop, LV_PART_MAIN));
    // the button beside the card stays still while the dialog shows, and takes the touch after
    tap(beneath);
    TEST_ASSERT_EQUAL_UINT32(0, clicks);
    controls::closeDialog(d);
    settle();
    tap(beneath);
    TEST_ASSERT_EQUAL_UINT32(1, clicks);
}

// fonts: every font holds the printable characters and finds LVGL's symbols in its fallback.
void testFonts() {
    const lv_font_t* fonts[] = {&archivo_black_13,       &archivo_black_14,
                                &archivo_black_17,       &archivo_black_20,
                                &jetbrains_mono_bold_12, &jetbrains_mono_bold_16,
                                &montserrat_semibold_12, &montserrat_semibold_14};
    for (const lv_font_t* f : fonts) {
        lv_font_glyph_dsc_t g;
        for (uint32_t c = ' '; c <= '~'; ++c) {
            TEST_ASSERT_TRUE(lv_font_get_glyph_dsc(f, &g, c, 0));
            TEST_ASSERT_EQUAL_PTR(f, g.resolved_font);
        }
        // LV_SYMBOL_RIGHT, which the fallback holds
        TEST_ASSERT_TRUE(lv_font_get_glyph_dsc(f, &g, 0xF054, 0));
        TEST_ASSERT_EQUAL_PTR(&lv_font_montserrat_14, g.resolved_font);
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(testButton);
    RUN_TEST(testHeader);
    RUN_TEST(testBulb);
    RUN_TEST(testRow);
    RUN_TEST(testSlider);
    RUN_TEST(testDialog);
    RUN_TEST(testFonts);
    return UNITY_END();
}
