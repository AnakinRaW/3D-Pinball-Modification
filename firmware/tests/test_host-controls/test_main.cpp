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
//   card      a card's rim and strip take its tone, a fault glows neon red, and a toast has no
//             strip
//   tile      a tile takes the touch, a fault glows neon red, and a device that works has a thin
//             rim
//   pager     a pager has a bulb per page with the page shown lit, and moves the light along
//   level     a level shows its value and steps with minus and plus up to the end of its track
//   toggle    a switch row turns on and off at each tap and tells its listeners
//   scroll-list  a list that holds more than it shows scrolls, with a fade at the edge more lies
//             beyond
//   animated  the sunburst turns while the background is animated and stands still otherwise
//   stage     the rays, a curtain, the valance, the lettering, a key and a score line take their
//             sizes and let the touch through, but for the key
//   fonts     every font holds the printable characters with three signs and finds LVGL's
//             symbols in its fallback
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

// row: a row shows its title and detail, takes the touch, and changes its detail. A row without a
// detail sets its title in its middle until a detail comes.
void testRow() {
    Rig rig;
    lv_obj_t* r = controls::row(rig.parent, "Record ball sensors", "After a change", 300);
    lv_obj_add_event_cb(r, counted, LV_EVENT_CLICKED, nullptr);
    settle();
    TEST_ASSERT_EQUAL_INT32(controls::ROW_PX, lv_obj_get_height(r));
    TEST_ASSERT_EQUAL_STRING("After a change", lv_label_get_text(lv_obj_get_child(r, 0)));
    TEST_ASSERT_EQUAL_STRING("Record ball sensors", lv_label_get_text(lv_obj_get_child(r, 1)));
    TEST_ASSERT_EQUAL_PTR(&montserrat_bold_14,
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
    settle();
    const auto middleOf = [](lv_obj_t* o) {
        lv_area_t a;
        lv_obj_get_coords(o, &a);
        return (a.y1 + a.y2) / 2;
    };
    lv_obj_t* title = lv_obj_get_child(menu, 1);
    TEST_ASSERT_INT32_WITHIN(1, middleOf(menu), middleOf(title));
    controls::setDetail(menu, "50 %", controls::color::MUTED);
    settle();
    TEST_ASSERT_TRUE(middleOf(title) < middleOf(menu));
    controls::setDetail(menu, "", controls::color::MUTED);
    settle();
    TEST_ASSERT_INT32_WITHIN(1, middleOf(menu), middleOf(title));

    // a detail set before the first layout puts the title above it, beside the icon
    lv_obj_t* fresh = controls::row(rig.parent, "Lights", "", 200, controls::TARGET_PX,
                                    LV_SYMBOL_CHARGE, controls::color::BOARD);
    controls::setDetail(fresh, "80 %", controls::color::MUTED);
    settle();
    lv_area_t detail;
    lv_area_t named;
    lv_obj_get_coords(lv_obj_get_child(fresh, 0), &detail);
    lv_obj_get_coords(lv_obj_get_child(fresh, 1), &named);
    TEST_ASSERT_EQUAL_INT32(detail.x1, named.x1);
    TEST_ASSERT_TRUE(named.y2 <= detail.y1);
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
    lv_obj_t* title = lv_obj_get_child(lv_obj_get_child(d, 0), -1);
    TEST_ASSERT_EQUAL_STRING("END CLASSIC?", lv_label_get_text(title));
    TEST_ASSERT_TRUE(lv_obj_is_clickable(d));
    // the backdrop dims the parent behind the card
    lv_obj_t* backdrop = lv_obj_get_parent(d);
    TEST_ASSERT_EQUAL_INT32(W, lv_obj_get_width(backdrop));
    TEST_ASSERT_EQUAL_INT32(H, lv_obj_get_height(backdrop));
    TEST_ASSERT_EQUAL_HEX32(controls::color::NIGHT,
                            hex(lv_obj_get_style_bg_color(backdrop, LV_PART_MAIN)));
    TEST_ASSERT_EQUAL_UINT8(184, lv_obj_get_style_bg_opa(backdrop, LV_PART_MAIN));   // 0.72
    // the button beside the card stays still while the dialog shows, and takes the touch after
    tap(beneath);
    TEST_ASSERT_EQUAL_UINT32(0, clicks);
    controls::closeDialog(d);
    settle();
    tap(beneath);
    TEST_ASSERT_EQUAL_UINT32(1, clicks);
}

// card: a card's rim and strip take its tone, a fault glows neon red, and a toast has no strip.
void testCard() {
    Rig rig;
    lv_obj_t* note = controls::card(rig.parent, controls::Tone::NOTE, "RESTARTED", 292, 100);
    lv_obj_t* fault = controls::card(rig.parent, controls::Tone::FAULT, "2 FAULTS", 292, 100);
    lv_obj_t* toast = controls::card(rig.parent, controls::Tone::WORKS, nullptr, 312, 46);
    TEST_ASSERT_EQUAL_HEX32(controls::color::GOLD,
                            hex(lv_obj_get_style_border_color(note, LV_PART_MAIN)));
    TEST_ASSERT_EQUAL_HEX32(controls::color::NEON,
                            hex(lv_obj_get_style_border_color(fault, LV_PART_MAIN)));
    TEST_ASSERT_EQUAL_HEX32(controls::color::NEON,
                            hex(lv_obj_get_style_shadow_color(fault, LV_PART_MAIN)));
    TEST_ASSERT_EQUAL_HEX32(controls::color::GREEN,
                            hex(lv_obj_get_style_border_color(toast, LV_PART_MAIN)));
    lv_obj_t* strip = lv_obj_get_child(fault, 0);
    TEST_ASSERT_EQUAL_INT32(controls::STRIP_PX, lv_obj_get_style_height(strip, LV_PART_MAIN));
    TEST_ASSERT_EQUAL_STRING("2 FAULTS", lv_label_get_text(lv_obj_get_child(strip, -1)));
    TEST_ASSERT_EQUAL_UINT32(0, lv_obj_get_child_count(toast));
}

// tile: a tile takes the touch, a fault glows neon red, and a device that works has a thin rim.
void testTile() {
    Rig rig;
    lv_obj_t* works = controls::tile(rig.parent, controls::Tone::WORKS, 100, 44);
    lv_obj_t* failed = controls::tile(rig.parent, controls::Tone::FAULT, 100, 44);
    lv_obj_set_x(failed, 120);
    lv_obj_add_event_cb(works, counted, LV_EVENT_CLICKED, nullptr);
    TEST_ASSERT_EQUAL_INT32(1, lv_obj_get_style_border_width(works, LV_PART_MAIN));
    TEST_ASSERT_EQUAL_INT32(2, lv_obj_get_style_border_width(failed, LV_PART_MAIN));
    TEST_ASSERT_EQUAL_HEX32(controls::color::NEON,
                            hex(lv_obj_get_style_shadow_color(failed, LV_PART_MAIN)));
    tap(works);
    TEST_ASSERT_EQUAL_UINT32(1, clicks);
}

// pager: a pager has a bulb per page with the page shown lit, and moves the light along.
void testPager() {
    Rig rig;
    lv_obj_t* p = controls::pager(rig.parent, 3, 0);
    TEST_ASSERT_EQUAL_UINT32(3, lv_obj_get_child_count(p));
    const auto lit = [&](int32_t i) {
        return hex(lv_obj_get_style_shadow_color(lv_obj_get_child(p, i), LV_PART_MAIN)) ==
                   controls::color::BULB &&
               lv_obj_get_style_shadow_width(lv_obj_get_child(p, i), LV_PART_MAIN) > 0;
    };
    TEST_ASSERT_TRUE(lit(0));
    TEST_ASSERT_FALSE(lit(1));
    controls::setPage(p, 2);
    TEST_ASSERT_FALSE(lit(0));
    TEST_ASSERT_TRUE(lit(2));
}

// level: a level shows its value, steps with minus and plus up to the end of its track, and takes
// a value set from outside.
void testLevel() {
    Rig rig;
    lv_obj_t* l = controls::level(rig.parent, "EFFECTS", controls::Fill::SOUND, 90, 304);
    lv_obj_t* track = controls::levelTrack(l);
    lv_obj_add_event_cb(track, counted, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_t* value = lv_obj_get_child(l, 1);
    settle();
    TEST_ASSERT_EQUAL_INT32(controls::LEVEL_PX, lv_obj_get_height(l));
    TEST_ASSERT_EQUAL_STRING("EFFECTS", lv_label_get_text(lv_obj_get_child(l, 0)));
    TEST_ASSERT_EQUAL_STRING("90 %", lv_label_get_text(value));
    lv_obj_t* plus = lv_obj_get_child(lv_obj_get_child(l, 2), 1);
    tap(plus);
    TEST_ASSERT_EQUAL_INT32(95, lv_slider_get_value(track));
    TEST_ASSERT_EQUAL_STRING("95 %", lv_label_get_text(value));
    TEST_ASSERT_EQUAL_UINT32(1, changes);
    tap(plus);
    tap(plus);
    TEST_ASSERT_EQUAL_INT32(controls::SLIDER_MAX, lv_slider_get_value(track));
    TEST_ASSERT_EQUAL_STRING("100 %", lv_label_get_text(value));
    controls::setLevel(l, 10);
    TEST_ASSERT_EQUAL_STRING("10 %", lv_label_get_text(value));
}

// toggle: a switch row turns on and off at each tap, tells its listeners, and takes a state set
// from outside.
void testToggle() {
    Rig rig;
    lv_obj_t* t = controls::toggle(rig.parent, "Animated background", true, 304);
    lv_obj_add_event_cb(t, counted, LV_EVENT_VALUE_CHANGED, nullptr);
    settle();
    TEST_ASSERT_EQUAL_INT32(controls::TARGET_PX, lv_obj_get_height(t));
    TEST_ASSERT_TRUE(controls::isOn(t));
    tap(t);
    TEST_ASSERT_FALSE(controls::isOn(t));
    TEST_ASSERT_EQUAL_UINT32(1, changes);
    tap(t);
    TEST_ASSERT_TRUE(controls::isOn(t));
    controls::setOn(t, false);
    TEST_ASSERT_FALSE(controls::isOn(t));
    TEST_ASSERT_EQUAL_UINT32(2, changes);
}

// scroll list: a list that holds more than it shows scrolls and shows the fade at the edge more
// lies beyond.
void testScrollList() {
    Rig rig;
    lv_obj_t* list = controls::scrollList(rig.parent, 40, 120);
    for (int i = 0; i < 8; ++i) {
        controls::row(list, "Inlane 1", "Recorded", 300, controls::TARGET_PX);
    }
    settle();
    lv_obj_t* top = lv_obj_get_sibling(list, 1);
    lv_obj_t* foot = lv_obj_get_sibling(list, 2);
    TEST_ASSERT_TRUE(lv_obj_is_hidden(top));
    TEST_ASSERT_FALSE(lv_obj_is_hidden(foot));
    lv_obj_scroll_to_y(list, LV_COORD_MAX, LV_ANIM_OFF);
    settle();
    TEST_ASSERT_FALSE(lv_obj_is_hidden(top));
    TEST_ASSERT_TRUE(lv_obj_is_hidden(foot));
}

// animated: the sunburst turns while the background is animated and stands still otherwise.
void testAnimated() {
    Rig rig;
    lv_obj_t* r = controls::rays(rig.parent, true);
    static uint32_t draws = 0;
    lv_obj_add_event_cb(r, [](lv_event_t*) { ++draws; }, LV_EVENT_DRAW_MAIN_END, nullptr);
    TEST_ASSERT_TRUE(controls::isAnimated());
    // a turning sunburst is drawn again at each step of its timer
    draws = 0;
    for (int i = 0; i < 60; ++i) {
        settle();
    }
    TEST_ASSERT_TRUE(draws > 0);
    controls::setAnimated(false);
    TEST_ASSERT_FALSE(controls::isAnimated());
    for (int i = 0; i < 10; ++i) {
        settle();
    }
    draws = 0;
    for (int i = 0; i < 60; ++i) {
        settle();
    }
    TEST_ASSERT_EQUAL_UINT32(0, draws);
    controls::setAnimated(true);

    // a running game holds it still, and it turns again once the game ends
    const auto drawsWhile = [] {
        for (int i = 0; i < 10; ++i) {
            settle();
        }
        draws = 0;
        for (int i = 0; i < 60; ++i) {
            settle();
        }
        return draws;
    };
    controls::holdTurning(true);
    TEST_ASSERT_EQUAL_UINT32(0, drawsWhile());
    TEST_ASSERT_TRUE(controls::isAnimated());
    controls::holdTurning(false);
    TEST_ASSERT_TRUE(drawsWhile() > 0);
    // a sunburst built while a game runs stands as well
    controls::holdTurning(true);
    lv_obj_t* later = controls::rays(rig.parent, false);
    lv_obj_add_event_cb(later, [](lv_event_t*) { ++draws; }, LV_EVENT_DRAW_MAIN_END, nullptr);
    TEST_ASSERT_EQUAL_UINT32(0, drawsWhile());
    controls::holdTurning(false);
}

// stage: the rays, a curtain, the valance, the lettering, a key and a score line take their sizes
// and let the touch through, but for the key.
void testStage() {
    Rig rig;
    lv_obj_t* rays = controls::rays(rig.parent, true);
    lv_obj_t* curtain = controls::curtain(rig.parent, 34);
    lv_obj_t* valance = controls::valance(rig.parent);
    lv_obj_t* lettering = controls::lettering(rig.parent, "CLASSIC", &archivo_black_40, 2);
    lv_obj_t* line = controls::scoreLine(rig.parent, 240, true, true);
    lv_obj_t* bar = controls::progress(rig.parent, 292);
    settle();
    TEST_ASSERT_EQUAL_INT32(W, lv_obj_get_width(rays));
    TEST_ASSERT_EQUAL_INT32(H, lv_obj_get_height(rays));
    TEST_ASSERT_EQUAL_INT32(34, lv_obj_get_width(curtain));
    TEST_ASSERT_EQUAL_INT32(H, lv_obj_get_height(curtain));
    TEST_ASSERT_EQUAL_INT32(W, lv_obj_get_width(valance));
    lv_point_t text;
    lv_text_get_size(&text, "CLASSIC", &archivo_black_40, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    TEST_ASSERT_EQUAL_INT32(text.x + 4, lv_obj_get_width(lettering));
    TEST_ASSERT_EQUAL_INT32(controls::SCORE_LINE_PX, lv_obj_get_height(line));
    TEST_ASSERT_EQUAL_INT32(controls::SLIDER_MAX, lv_bar_get_max_value(bar));
    for (lv_obj_t* o : {rays, curtain, valance, lettering, line}) {
        TEST_ASSERT_FALSE(lv_obj_is_clickable(o));
    }
    lv_obj_t* key = controls::key(rig.parent, "A", controls::TARGET_PX, false);
    lv_obj_t* lit = controls::key(rig.parent, "LEA", 158, true);
    lv_obj_set_x(lit, 100);
    lv_obj_add_event_cb(key, counted, LV_EVENT_CLICKED, nullptr);
    TEST_ASSERT_EQUAL_STRING("A", lv_label_get_text(lv_obj_get_child(key, 0)));
    TEST_ASSERT_EQUAL_HEX32(controls::color::BULB,
                            hex(lv_obj_get_style_border_color(lit, LV_PART_MAIN)));
    tap(key);
    TEST_ASSERT_EQUAL_UINT32(1, clicks);
}

// fonts: every font holds the printable characters with the middle dot, the times sign and the
// minus sign, and finds LVGL's symbols in its fallback.
void testFonts() {
    const lv_font_t* fonts[] = {
        &archivo_black_9,        &archivo_black_12,       &archivo_black_13,
        &archivo_black_14,       &archivo_black_17,       &archivo_black_20,
        &archivo_black_30,       &archivo_black_40,       &jetbrains_mono_bold_12,
        &jetbrains_mono_bold_16, &jetbrains_mono_bold_20, &montserrat_semibold_12,
        &montserrat_semibold_14, &montserrat_bold_12,     &montserrat_bold_14,
        &montserrat_bold_16};
    // the middle dot, the times sign and the minus sign, which every font but a title's holds
    const uint32_t signs[] = {0xB7, 0xD7, 0x2212};
    // Font Awesome's light bulb, sun and crossed-out speaker
    const uint32_t icons[] = {0xF0EB, 0xF185, 0xF6A9};
    for (const lv_font_t* f : fonts) {
        // The title sizes hold the capitals and the signs before them, and no lower case.
        const bool title = f == &archivo_black_30 || f == &archivo_black_40;
        lv_font_glyph_dsc_t g;
        for (uint32_t c = ' '; c <= (title ? '_' : '~'); ++c) {
            TEST_ASSERT_TRUE(lv_font_get_glyph_dsc(f, &g, c, 0));
            TEST_ASSERT_EQUAL_PTR(f, g.resolved_font);
        }
        for (uint32_t c : signs) {
            if (!title) {
                TEST_ASSERT_TRUE(lv_font_get_glyph_dsc(f, &g, c, 0));
                TEST_ASSERT_EQUAL_PTR(f, g.resolved_font);
            }
        }
        // the icons, which host_icons_14 holds, and LV_SYMBOL_RIGHT, which LVGL's font holds
        for (uint32_t c : icons) {
            TEST_ASSERT_TRUE(lv_font_get_glyph_dsc(f, &g, c, 0));
            TEST_ASSERT_EQUAL_PTR(&host_icons_14, g.resolved_font);
        }
        TEST_ASSERT_TRUE(lv_font_get_glyph_dsc(f, &g, 0xF054, 0));
        TEST_ASSERT_EQUAL_PTR(&lv_font_montserrat_14, g.resolved_font);
    }
    // the icons' UTF-8 names the same code points, three bytes each
    const auto decoded = [](const char* text) {
        const auto* b = reinterpret_cast<const uint8_t*>(text);
        return uint32_t(b[0] & 0x0F) << 12 | uint32_t(b[1] & 0x3F) << 6 | uint32_t(b[2] & 0x3F);
    };
    TEST_ASSERT_EQUAL_UINT32(icons[0], decoded(controls::ICON_BULB));
    TEST_ASSERT_EQUAL_UINT32(icons[1], decoded(controls::ICON_SUN));
    TEST_ASSERT_EQUAL_UINT32(icons[2], decoded(controls::ICON_MUTED));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(testButton);
    RUN_TEST(testHeader);
    RUN_TEST(testBulb);
    RUN_TEST(testRow);
    RUN_TEST(testSlider);
    RUN_TEST(testDialog);
    RUN_TEST(testCard);
    RUN_TEST(testTile);
    RUN_TEST(testPager);
    RUN_TEST(testLevel);
    RUN_TEST(testToggle);
    RUN_TEST(testScrollList);
    RUN_TEST(testAnimated);
    RUN_TEST(testStage);
    RUN_TEST(testFonts);
    return UNITY_END();
}
