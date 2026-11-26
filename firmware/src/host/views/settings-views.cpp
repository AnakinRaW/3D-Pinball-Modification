#include "settings-views.h"
#include <initializer_list>
#include <ctype.h>
#include <stdio.h>

namespace {
namespace color = controls::color;

constexpr int32_t HALF = 2;
constexpr int32_t INSET_PX = 8;   // a page's content from the screen's sides
constexpr int32_t CONTENT_Y = controls::HEADER_PX + controls::BULBS_PX + controls::GAP_PX + 2;

// Settings: two tiles across, two down, and the switch row below them
constexpr int32_t TILE_WIDTH = 149;
constexpr int32_t TILE_HEIGHT = 68;
constexpr int32_t TILE_GAP_PX = 6;
constexpr int32_t TILE_ICON_X = 10;
constexpr int32_t TILE_ICON_Y = 10;
constexpr int32_t TILE_TEXT_X = 46;
constexpr int32_t TILE_TITLE_Y = 12;
constexpr int32_t TILE_DETAIL_X = 10;   // below the icon, across the whole tile
constexpr int32_t TILE_DETAIL_Y = 46;
constexpr int32_t SERVICE_Y = 192;
// the Machine tile's three bulbs
struct Bulb {
    controls::Light light;
    int32_t x;
    int32_t y;
};
constexpr Bulb CLUSTER[] = {{controls::Light::FAILED, 12, 10},
                            {controls::Light::WARNING, 12, 30},
                            {controls::Light::WORKS, 26, 20}};
constexpr uint32_t SOUND_ICON = 0x6fb2ec;
constexpr uint32_t OPTIONS_ICON = 0xcfd4da;
constexpr int32_t ICON_SCALE = 512;   // twice LVGL's transform scale of 256
constexpr int32_t WHOLE_PERCENT = 100;

// Sound and Lights
constexpr int32_t FIRST_LEVEL_Y = 60;
constexpr int32_t SECOND_LEVEL_Y = 146;

// Service
constexpr int32_t ROW_INSET_PX = 6;
constexpr int32_t FIRST_ROW_Y = 48;
constexpr int32_t ROW_STEP_PX = 48;
constexpr int32_t ABOUT_Y = 196;   // the first line below the rows
constexpr int32_t DIALOG_WIDTH = 280;
constexpr int32_t DIALOG_TEXT_X = 14;
constexpr int32_t DIALOG_TEXT_Y = 40;
constexpr int32_t DIALOG_BUTTON_WIDTH = 120;
constexpr int32_t DIALOG_BOTTOM_PX = 12;
constexpr int32_t DIALOG_HEIGHT = 170;
constexpr size_t TITLE_CHARS = 24;
constexpr int32_t ABOUT_STEP_PX = 18;
constexpr size_t LINE_CHARS = 64;

// the ball sensors and the recording of one
constexpr int32_t LIST_Y = 46;
constexpr int32_t RECORD_INSET_PX = 14;
constexpr int32_t RECORD_TEXT_Y = 56;
constexpr int32_t NOTE_Y = 110;
constexpr int32_t BAR_Y = 140;
constexpr int32_t RECORD_BUTTON_WIDTH = 120;
constexpr int32_t RECORD_BUTTON_Y = 184;
constexpr int32_t RECORD_BUTTON_GAP_PX = 8;
constexpr size_t TEXT_CHARS = 64;

// the touch calibration
constexpr int32_t MARK_PX = 24;   // the mark's crosshair, its center MARK_PX / 2 in
constexpr int32_t MARK_RING_PX = 18;
constexpr int32_t MARK_DOT_PX = 5;
constexpr int32_t CALIBRATE_TITLE_Y = 80;
constexpr int32_t CALIBRATE_TEXT_Y = 104;
constexpr int32_t CALIBRATE_PAGER_Y = 132;
constexpr int32_t CALIBRATE_BUTTON_Y = 176;

lv_obj_t* placed(lv_obj_t* o, int32_t x, int32_t y) {
    lv_obj_set_pos(o, x, y);
    return o;
}

/// Places a page: the faint rays and the header with its title, and back at its left if
/// @p back is not nullptr.
lv_obj_t* page(lv_obj_t* root, const char* title, lv_obj_t** back) {
    controls::rays(root, true);
    lv_obj_t* header = controls::header(root, title, true);
    if (back != nullptr) {
        *back = controls::headerButton(header, true, LV_SYMBOL_LEFT);
    }
    return header;
}

lv_obj_t* centered(lv_obj_t* parent, const char* text, const lv_font_t* font, uint32_t rgb,
                   int32_t y) {
    lv_obj_t* l = controls::label(parent, text, font, rgb);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, y);
    return l;
}

/// Creates a label that wraps at @p width.
lv_obj_t* wrapped(lv_obj_t* parent, const char* text, const lv_font_t* font, uint32_t rgb,
                  int32_t width) {
    lv_obj_t* l = controls::label(parent, text, font, rgb);
    lv_obj_set_width(l, width);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
    return l;
}

// The mark is a gold ring with a crosshair and a dot in the bulb's yellow, drawn in one object.
void drawMark(lv_event_t* e) {
    lv_obj_t* o = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    const bool held = lv_obj_get_user_data(o) != nullptr;
    lv_layer_t* layer = lv_event_get_layer(e);
    lv_area_t box;
    lv_obj_get_coords(o, &box);
    const int32_t cx = box.x1 + MARK_PX / HALF;
    const int32_t cy = box.y1 + MARK_PX / HALF;
    const lv_color_t ring = lv_color_hex(held ? color::GREEN : color::GOLD);
    const lv_color_t cross = lv_color_hex(held ? color::GREEN : color::BULB);
    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.radius = LV_RADIUS_CIRCLE;
    r.bg_opa = LV_OPA_TRANSP;
    r.border_width = 2;
    r.border_color = ring;
    const lv_area_t around = {cx - MARK_RING_PX / HALF, cy - MARK_RING_PX / HALF,
                              cx + MARK_RING_PX / HALF, cy + MARK_RING_PX / HALF};
    lv_draw_rect(layer, &r, &around);
    lv_draw_line_dsc_t l;
    lv_draw_line_dsc_init(&l);
    l.color = cross;
    l.width = 2;
    l.p1 = {box.x1, cy};
    l.p2 = {box.x2, cy};
    lv_draw_line(layer, &l);
    l.p1 = {cx, box.y1};
    l.p2 = {cx, box.y2};
    lv_draw_line(layer, &l);
    r.bg_opa = LV_OPA_COVER;
    r.bg_color = cross;
    r.border_width = 0;
    const lv_area_t dot = {cx - MARK_DOT_PX / HALF, cy - MARK_DOT_PX / HALF,
                           cx + MARK_DOT_PX / HALF, cy + MARK_DOT_PX / HALF};
    lv_draw_rect(layer, &r, &dot);
}
}   // namespace

void SettingsView::build(lv_obj_t* parent, const State& state, Pressed pressed, void* context) {
    lv_obj_t* root = create(parent, true, pressed, context);
    lv_obj_t* back = nullptr;
    page(root, "SETTINGS", &back);
    bind(back, BACK);
    struct Tile {
        Action action;
        const char* name;
        const char* detail;
        const char* icon;
        uint32_t iconRgb;
    };
    const Tile tiles[] = {{SOUND, "SOUND", state.sound, LV_SYMBOL_VOLUME_MAX, SOUND_ICON},
                          {LIGHTS, "LIGHTS", state.lights, LV_SYMBOL_CHARGE, color::BOARD},
                          {STATUS, "STATUS", state.status, nullptr, 0},
                          {OPTIONS, "OPTIONS", state.options, LV_SYMBOL_SETTINGS, OPTIONS_ICON}};
    for (int32_t i = 0; i < int32_t(sizeof(tiles) / sizeof(tiles[0])); ++i) {
        const Tile& t = tiles[i];
        const bool status = t.action == STATUS;
        const controls::Tone tone =
            status && state.fault ? controls::Tone::FAULT : controls::Tone::NOTE;
        lv_obj_t* o = controls::tile(root, tone, TILE_WIDTH, TILE_HEIGHT);
        lv_obj_set_pos(o, INSET_PX + (i % HALF) * (TILE_WIDTH + TILE_GAP_PX),
                       CONTENT_Y + (i / HALF) * (TILE_HEIGHT + TILE_GAP_PX));
        bind(o, t.action);
        placed(controls::label(o, t.name, &archivo_black_14, color::CREAM), TILE_TEXT_X,
               TILE_TITLE_Y);
        if (status) {
            // Three bulbs stand for the warning lights: a failed device, a warning and one that
            // works.
            for (const Bulb& b : CLUSTER) {
                placed(controls::bulb(o, b.light), b.x, b.y);
            }
        } else {
            // LVGL's symbols come at 14 px, drawn here at twice the size.
            lv_obj_t* icon = placed(controls::label(o, t.icon, &lv_font_montserrat_14, t.iconRgb),
                                    TILE_ICON_X, TILE_ICON_Y);
            lv_obj_set_style_transform_scale(icon, ICON_SCALE, 0);
            lv_obj_set_style_transform_pivot_x(icon, 0, 0);
            lv_obj_set_style_transform_pivot_y(icon, 0, 0);
        }
        placed(controls::label(o, t.detail, &montserrat_semibold_12,
                               status && state.fault ? color::FAULT_TEXT : color::MUTED),
               TILE_DETAIL_X, TILE_DETAIL_Y);
    }
    lv_obj_t* service =
        controls::row(root, "Service", "Set up this machine",
                      controls::SCREEN_WIDTH_PX - INSET_PX * HALF, controls::TARGET_PX);
    placed(service, INSET_PX, SERVICE_Y);
    bind(service, SERVICE);
}

void OptionsView::build(lv_obj_t* parent, const State& state, Pressed pressed, void* context) {
    lv_obj_t* root = create(parent, true, pressed, context);
    lv_obj_t* back = nullptr;
    page(root, "OPTIONS", &back);
    bind(back, BACK);
    lv_obj_t* list = controls::scrollList(root, LIST_Y, controls::SCREEN_HEIGHT_PX - LIST_Y);
    struct Switch {
        Action action;
        const char* title;
        bool on;
    };
    const Switch switches[] = {{ANIMATED, "Animated background", state.animated},
                               {BOOT_FILM, "Boot film", state.bootFilm}};
    for (const Switch& sw : switches) {
        lv_obj_t* s = controls::toggle(list, sw.title, sw.on, lv_pct(WHOLE_PERCENT));
        // The switch carries its action, which a change reports with the switch's state.
        lv_obj_set_user_data(s, reinterpret_cast<void*>(uintptr_t(sw.action)));
        lv_obj_add_event_cb(
            s,
            [](lv_event_t* e) {
                const auto* v = static_cast<const OptionsView*>(lv_event_get_user_data(e));
                auto* o = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
                const auto action = uint8_t(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(o)));
                v->report(action, controls::isOn(o) ? 1 : 0);
            },
            LV_EVENT_VALUE_CHANGED, this);
    }
}

void LevelsView::build(lv_obj_t* parent, const Levels& l, Pressed pressed, void* context) {
    lv_obj_t* root = create(parent, true, pressed, context);
    lv_obj_t* back = nullptr;
    page(root, l.title, &back);
    bind(back, BACK);
    const int32_t ys[] = {FIRST_LEVEL_Y, SECOND_LEVEL_Y};
    const Action actions[] = {FIRST, SECOND};
    for (uint8_t i = 0; i < HALF; ++i) {
        levels[i] = controls::level(root, l.names[i], l.fill, l.values[i],
                                    controls::SCREEN_WIDTH_PX - INSET_PX * HALF);
        lv_obj_set_pos(levels[i], INSET_PX, ys[i]);
        bindValue(controls::levelTrack(levels[i]), actions[i]);
    }
}

int32_t LevelsView::value(uint8_t which) const {
    return isOpen() && which < HALF ? lv_slider_get_value(controls::levelTrack(levels[which])) : 0;
}

void ServiceView::build(lv_obj_t* parent, const About& about, Pressed pressed, void* context) {
    lv_obj_t* root = create(parent, true, pressed, context);
    adapterA = about.adapterA;
    dialog = nullptr;
    lv_obj_t* back = nullptr;
    page(root, "SERVICE", &back);
    bind(back, BACK);
    const int32_t width = controls::SCREEN_WIDTH_PX - ROW_INSET_PX * HALF;
    lv_obj_t* record =
        controls::row(root, "Ball sensors", "Record a sensor again", width, controls::TARGET_PX);
    lv_obj_t* calibrate = controls::row(root, "Calibrate touch", "Four marks at the corners", width,
                                        controls::TARGET_PX);
    adapterRow = controls::row(root, "Lights adapter", "", width, controls::TARGET_PX);
    placed(record, ROW_INSET_PX, FIRST_ROW_Y);
    placed(calibrate, ROW_INSET_PX, FIRST_ROW_Y + ROW_STEP_PX);
    placed(adapterRow, ROW_INSET_PX, FIRST_ROW_Y + ROW_STEP_PX * HALF);
    bind(record, RECORD);
    bind(calibrate, CALIBRATE);
    lv_obj_add_event_cb(
        adapterRow,
        [](lv_event_t* e) { static_cast<ServiceView*>(lv_event_get_user_data(e))->ask(); },
        LV_EVENT_CLICKED, this);
    showAdapter();
    const struct {
        const char* name;
        const char* value;
    } lines[] = {{"Firmware", about.firmware}, {"Config", about.config}};
    int32_t y = ABOUT_Y;
    for (const auto& l : lines) {
        char line[LINE_CHARS];
        snprintf(line, sizeof(line), "%s  #fff4d6 %s#", l.name, l.value);
        lv_obj_t* o = placed(controls::label(root, line, &montserrat_semibold_12, color::MUTED),
                             INSET_PX + 4, y);
        lv_label_set_recolor(o, true);
        y += ABOUT_STEP_PX;
    }
}

void ServiceView::showAdapter() {
    char detail[LINE_CHARS];
    snprintf(detail, sizeof(detail), "%u A adapter \xC2\xB7 changes at the next start",
             unsigned(adapterA));
    controls::setDetail(adapterRow, detail, color::MUTED);
}

// The dialog offers the other rating. A larger adapter warns that an adapter too weak for it
// overloads, a smaller one only asks.
void ServiceView::ask() {
    if (dialog != nullptr) {
        return;
    }
    const bool larger = adapterA == SMALL_ADAPTER_A;
    const uint8_t to = larger ? LARGE_ADAPTER_A : SMALL_ADAPTER_A;
    char title[TITLE_CHARS];
    snprintf(title, sizeof(title), "SWITCH TO %u A?", unsigned(to));
    dialog = controls::dialog(root(), title, DIALOG_WIDTH, DIALOG_HEIGHT,
                              larger ? controls::Tone::WARNING : controls::Tone::NOTE);
    char text[TEXT_CHARS * HALF];
    if (larger) {
        snprintf(text, sizeof(text),
                 "Only with a %u A adapter. A %u A adapter cannot carry the brighter lights: it "
                 "switches off or runs hot.",
                 unsigned(LARGE_ADAPTER_A), unsigned(SMALL_ADAPTER_A));
    } else {
        snprintf(text, sizeof(text), "The lights then stay within a %u A adapter's rating.",
                 unsigned(SMALL_ADAPTER_A));
    }
    lv_obj_t* t = wrapped(dialog, text, &montserrat_semibold_14, color::CREAM,
                          DIALOG_WIDTH - DIALOG_TEXT_X * HALF);
    placed(t, DIALOG_TEXT_X - HALF, DIALOG_TEXT_Y - HALF);
    // 3 A stands at the left and 6 A at the right, whichever is set. The set one keeps it, the
    // other changes to it.
    char small[TITLE_CHARS];
    char large[TITLE_CHARS];
    snprintf(small, sizeof(small), "%s %u A", larger ? "KEEP" : "USE", unsigned(SMALL_ADAPTER_A));
    snprintf(large, sizeof(large), "%s %u A", larger ? "USE" : "KEEP", unsigned(LARGE_ADAPTER_A));
    lv_obj_t* left =
        controls::button(dialog, larger ? controls::Plaque::VIOLET : controls::Plaque::GOLD, small,
                         DIALOG_BUTTON_WIDTH);
    lv_obj_t* right =
        controls::button(dialog, larger ? controls::Plaque::RED : controls::Plaque::VIOLET, large,
                         DIALOG_BUTTON_WIDTH);
    lv_obj_align(left, LV_ALIGN_BOTTOM_LEFT, DIALOG_TEXT_X - HALF, -DIALOG_BOTTOM_PX);
    lv_obj_align(right, LV_ALIGN_BOTTOM_RIGHT, -(DIALOG_TEXT_X - HALF), -DIALOG_BOTTOM_PX);
    lv_obj_t* no = larger ? left : right;
    lv_obj_t* yes = larger ? right : left;
    lv_obj_set_user_data(no, nullptr);
    lv_obj_set_user_data(yes, yes);
    lv_obj_add_event_cb(no, answered, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(yes, answered, LV_EVENT_CLICKED, this);
}

// Either answer closes the dialog. The change reports the new rating.
void ServiceView::answered(lv_event_t* e) {
    auto* v = static_cast<ServiceView*>(lv_event_get_user_data(e));
    lv_obj_t* button = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    const bool change = lv_obj_get_user_data(button) != nullptr;
    controls::closeDialog(v->dialog);
    v->dialog = nullptr;
    if (change) {
        v->adapterA = v->adapterA == SMALL_ADAPTER_A ? LARGE_ADAPTER_A : SMALL_ADAPTER_A;
        v->showAdapter();
        v->report(ADAPTER, v->adapterA);
    }
}

void SensorsView::build(lv_obj_t* parent, const Sensor* sensors, uint8_t count, Pressed pressed,
                        void* context) {
    lv_obj_t* root = create(parent, true, pressed, context);
    lv_obj_t* back = nullptr;
    page(root, "BALL SENSORS", &back);
    bind(back, BACK);
    lv_obj_t* list = controls::scrollList(root, LIST_Y, controls::SCREEN_HEIGHT_PX - LIST_Y);
    for (uint8_t i = 0; i < count; ++i) {
        const Sensor& sensor = sensors[i];
        const char* detail = sensor.recorded == Recorded::YES     ? "Recorded"
                             : sensor.recorded == Recorded::AGAIN ? "Record it again"
                                                                  : "Not recorded";
        lv_obj_t* r =
            controls::row(list, sensor.name, detail, lv_pct(WHOLE_PERCENT), controls::TARGET_PX);
        controls::setDetail(r, detail,
                            sensor.recorded == Recorded::YES ? color::MUTED : color::AMBER);
        bind(r, SENSOR, i);
    }
}

void RecordSensorView::build(lv_obj_t* parent, const char* sensor, Pressed pressed, void* context) {
    lv_obj_t* root = create(parent, true, pressed, context);
    snprintf(name, sizeof(name), "%s", sensor);
    // The header names the sensor in capitals, as every title stands.
    char title[sizeof(name)] = {};
    for (size_t i = 0; i + 1 < sizeof(title) && name[i] != '\0'; ++i) {
        title[i] = char(toupper(static_cast<unsigned char>(name[i])));
    }
    page(root, title, nullptr);
    const int32_t width = controls::SCREEN_WIDTH_PX - RECORD_INSET_PX * HALF;
    text = placed(wrapped(root, "", &montserrat_bold_14, color::CREAM, width), RECORD_INSET_PX,
                  RECORD_TEXT_Y);
    note = placed(wrapped(root, "", &montserrat_semibold_12, color::AMBER, width), RECORD_INSET_PX,
                  NOTE_Y);
    bar = placed(controls::progress(root, width), RECORD_INSET_PX, BAR_Y);
    cancel = controls::button(root, controls::Plaque::VIOLET, "CANCEL", RECORD_BUTTON_WIDTH);
    start = controls::button(root, controls::Plaque::GOLD, "START", RECORD_BUTTON_WIDTH);
    finish = controls::button(root, controls::Plaque::GOLD, "DONE", RECORD_BUTTON_WIDTH);
    placed(cancel,
           controls::SCREEN_WIDTH_PX / HALF - RECORD_BUTTON_WIDTH - RECORD_BUTTON_GAP_PX / HALF,
           RECORD_BUTTON_Y);
    placed(start, controls::SCREEN_WIDTH_PX / HALF + RECORD_BUTTON_GAP_PX / HALF, RECORD_BUTTON_Y);
    lv_obj_align(finish, LV_ALIGN_TOP_MID, 0, RECORD_BUTTON_Y);
    bind(cancel, CANCEL);
    bind(start, START);
    bind(finish, DONE);
    setStage(Stage::CLEAR);
}

void RecordSensorView::setStage(Stage stage, const char* line) {
    if (!isOpen()) {
        return;
    }
    char t[TEXT_CHARS];
    switch (stage) {
    case Stage::CLEAR:
        snprintf(t, sizeof(t), "Keep every ball off the sensors, then press Start.");
        break;
    case Stage::MEASURING_CLEAR:
        snprintf(t, sizeof(t), "Measuring every sensor clear.");
        break;
    case Stage::BALL:
        snprintf(t, sizeof(t), "Lay a ball on %s, then press Start.", name);
        break;
    case Stage::MEASURING_BALL:
        snprintf(t, sizeof(t), "Measuring %s with the ball.", name);
        break;
    case Stage::DONE:
        snprintf(t, sizeof(t), "%s is recorded.", name);
        break;
    }
    lv_label_set_text(text, t);
    lv_label_set_text(note, stage == Stage::DONE ? line : "");
    const bool measuring = stage == Stage::MEASURING_CLEAR || stage == Stage::MEASURING_BALL;
    const bool waiting = stage == Stage::CLEAR || stage == Stage::BALL;
    lv_obj_set_hidden(bar, !measuring);
    lv_obj_set_hidden(start, !waiting);
    lv_obj_set_hidden(cancel, stage == Stage::DONE);
    lv_obj_set_hidden(finish, stage != Stage::DONE);
    setProgress(0);
}

void RecordSensorView::setProgress(int32_t percent) {
    if (isOpen()) {
        lv_bar_set_value(bar, percent, LV_ANIM_OFF);
    }
}

void CalibrateTouchView::build(lv_obj_t* parent, Pressed pressed, void* context) {
    lv_obj_t* root = create(parent, true, pressed, context);
    lv_obj_set_style_bg_color(root, lv_color_hex(color::NIGHT), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    cross = lv_obj_create(root);
    lv_obj_remove_style_all(cross);
    lv_obj_set_size(cross, MARK_PX + 1, MARK_PX + 1);
    lv_obj_set_clickable(cross, false);
    lv_obj_add_event_cb(cross, drawMark, LV_EVENT_DRAW_MAIN_END, nullptr);
    centered(root, "TOUCH THE GOLD MARK", &archivo_black_14, color::GOLD, CALIBRATE_TITLE_Y);
    centered(root, "Hold the finger on its center until it turns green", &montserrat_semibold_12,
             color::MUTED, CALIBRATE_TEXT_Y);
    pages = controls::pager(root, MARKS, 0);
    lv_obj_align(pages, LV_ALIGN_TOP_MID, 0, CALIBRATE_PAGER_Y);
    lv_obj_t* cancel =
        controls::button(root, controls::Plaque::VIOLET, "CANCEL", RECORD_BUTTON_WIDTH);
    lv_obj_align(cancel, LV_ALIGN_TOP_MID, 0, CALIBRATE_BUTTON_Y);
    bind(cancel, CANCEL);
    setMark(0);
}

void CalibrateTouchView::setMark(uint8_t mark) {
    if (!isOpen() || mark >= MARKS) {
        return;
    }
    // The mark's center lies MARK_PX / 2 in from the corner's two edges.
    const bool right = mark == 1 || mark == HALF;
    const bool bottom = mark >= HALF;
    lv_obj_set_pos(cross, right ? controls::SCREEN_WIDTH_PX - MARK_PX - 1 : 0,
                   bottom ? controls::SCREEN_HEIGHT_PX - MARK_PX - 1 : 0);
    controls::setPage(pages, mark);
    setHeld(false);
}

void CalibrateTouchView::setHeld(bool held) {
    if (isOpen()) {
        lv_obj_set_user_data(cross, held ? cross : nullptr);
        lv_obj_invalidate(cross);
    }
}
