#include "machine-views.h"
#include <stdio.h>

namespace {
namespace color = controls::color;

constexpr int32_t HALF = 2;
constexpr int32_t ACROSS = 3;   // host-screens.md: three tiles across
constexpr int32_t TILE_X = 6;
constexpr int32_t TILE_Y = 46;
constexpr int32_t TILE_WIDTH = 100;
constexpr int32_t TILE_GAP_PX = 4;
constexpr int32_t TILE_BULB_X = 7;
constexpr int32_t TILE_TEXT_X = 27;
constexpr int32_t TILE_TEXT_INSET_PX = 4;

constexpr int32_t LIST_Y = 46;
constexpr int32_t ENTRY_PAD_PX = 6;
constexpr int32_t ENTRY_TEXT_X = 30;
constexpr int32_t ENTRY_BULB_X = 8;
constexpr int32_t ENTRY_BULB_Y = 8;
constexpr int32_t ENTRY_INSET_PX = 8;
constexpr int32_t COUNT_INSET_PX = 10;
constexpr int32_t WHOLE_PERCENT = 100;
constexpr size_t COUNT_CHARS = 12;

lv_obj_t* page(lv_obj_t* root, const char* title, lv_obj_t** back) {
    controls::rays(root, true);
    lv_obj_t* header = controls::header(root, title, true);
    *back = controls::headerButton(header, true, LV_SYMBOL_LEFT);
    return header;
}

controls::Tone toneOf(controls::Light light) {
    return light == controls::Light::FAILED    ? controls::Tone::FAULT
           : light == controls::Light::WARNING ? controls::Tone::WARNING
                                               : controls::Tone::WORKS;
}
}   // namespace

void WarningLightsView::build(lv_obj_t* parent, const Tile* tiles, uint8_t count, Pressed pressed,
                              void* context) {
    lv_obj_t* root = create(parent, true, pressed, context);
    lv_obj_t* back = nullptr;
    page(root, "MACHINE", &back);
    bind(back, BACK);
    for (uint8_t i = 0; i < count && i < TILES_MAX; ++i) {
        const Tile& t = tiles[i];
        lv_obj_t* o = controls::tile(root, toneOf(t.light), TILE_WIDTH, controls::TARGET_PX);
        lv_obj_set_pos(o, TILE_X + (i % ACROSS) * (TILE_WIDTH + TILE_GAP_PX),
                       TILE_Y + (i / ACROSS) * (controls::TARGET_PX + TILE_GAP_PX));
        bind(o, TILE, i);
        lv_obj_align(controls::bulb(o, t.light), LV_ALIGN_LEFT_MID, TILE_BULB_X - HALF, 0);
        lv_obj_t* name =
            controls::label(o, t.name, &montserrat_bold_12,
                            t.light == controls::Light::WORKS ? color::DIM_TEXT : color::CREAM);
        lv_obj_set_width(name, TILE_WIDTH - TILE_TEXT_X - TILE_TEXT_INSET_PX);
        lv_label_set_long_mode(name, LV_LABEL_LONG_MODE_WRAP);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, TILE_TEXT_X - HALF, 0);
    }
}

void WarningsView::build(lv_obj_t* parent, const Warning* warnings, uint8_t count, Pressed pressed,
                         void* context) {
    lv_obj_t* root = create(parent, true, pressed, context);
    lv_obj_t* back = nullptr;
    lv_obj_t* header = page(root, "WARNINGS", &back);
    bind(back, BACK);
    char n[COUNT_CHARS];
    snprintf(n, sizeof(n), "%u", unsigned(count));
    lv_obj_align(
        controls::label(lv_obj_get_child(header, 0), n, &jetbrains_mono_bold_12, color::CYAN),
        LV_ALIGN_RIGHT_MID, -COUNT_INSET_PX, 0);

    lv_obj_t* list = controls::scrollList(root, LIST_Y, controls::SCREEN_HEIGHT_PX - LIST_Y);
    for (uint8_t i = 0; i < count; ++i) {
        const Warning& w = warnings[i];
        lv_obj_t* entry = controls::row(list, "", "", lv_pct(WHOLE_PERCENT), LV_SIZE_CONTENT);
        // A warning is read, not pressed: the row keeps its plaque and drops the rest.
        lv_obj_clean(entry);
        lv_obj_set_clickable(entry, false);
        lv_obj_set_style_pad_ver(entry, ENTRY_PAD_PX, 0);
        lv_obj_set_style_pad_left(entry, ENTRY_TEXT_X, 0);
        lv_obj_set_style_pad_right(entry, ENTRY_INSET_PX, 0);
        lv_obj_set_flex_flow(entry, LV_FLEX_FLOW_COLUMN);
        lv_obj_t* b = controls::bulb(entry, controls::Light::WARNING);
        lv_obj_set_ignore_layout(b, true);
        lv_obj_set_pos(b, ENTRY_BULB_X - ENTRY_TEXT_X, ENTRY_BULB_Y - ENTRY_PAD_PX);
        controls::label(entry, w.source, &montserrat_bold_12, color::AMBER);
        if (w.count > 1) {
            char times[COUNT_CHARS];
            snprintf(times, sizeof(times), "\xC3\x97%u", unsigned(w.count));
            lv_obj_t* t = controls::label(entry, times, &jetbrains_mono_bold_12, color::AMBER);
            lv_obj_set_ignore_layout(t, true);
            lv_obj_align(t, LV_ALIGN_TOP_RIGHT, 0, 0);
        }
        lv_obj_t* text = controls::label(entry, w.text, &montserrat_semibold_12, color::CREAM);
        lv_obj_set_width(text, lv_pct(WHOLE_PERCENT));
        lv_label_set_long_mode(text, LV_LABEL_LONG_MODE_WRAP);
    }
}
