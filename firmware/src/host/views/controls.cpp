#include "controls.h"
#include <initializer_list>

namespace controls {

// The shades the storyboard's CSS sets beside the colors of host-screens.md's table.
constexpr uint32_t RED_TOP = 0xc8323f;       // a red plaque's top
constexpr uint32_t GOLD_TOP = 0xf6dc72;      // a gold plaque's top
constexpr uint32_t BOARD_DARK = 0xb39344;    // a gold plaque's bottom, the light fill's bottom
constexpr uint32_t ON_GOLD = 0x2a0610;       // text on gold, the bulbs' strip
constexpr uint32_t BAND_BOTTOM = 0x8a1a22;   // the header band's and the strip's bottom
constexpr uint32_t OFF_FILL = 0x2a2430;      // a disabled plaque
constexpr uint32_t OFF_RIM = 0x5a5260;
constexpr uint32_t OFF_TEXT = 0x8a8290;
constexpr uint32_t TRACK = 0x1a0a2a;   // a slider's track
constexpr uint32_t TRACK_RIM = 0x6d5a2a;
constexpr uint32_t RAMP_LIGHT = 0x6fb2ec;    // the sound fill's top
constexpr uint32_t STEEL_LIGHT = 0xe9eef2;   // the knob's steel ball
constexpr uint32_t STEEL_DARK = 0x3c3f47;
constexpr uint32_t INK = 0x17171b;   // the knob's rim
constexpr uint32_t BLACK = 0x000000;
// the warning lights: top, bottom and glow of each Light
constexpr uint32_t LIGHT_TOP[] = {0xfff6d8, 0x8a6a3a, 0x5ad08a, 0xffc070, 0xff5a66};
constexpr uint32_t LIGHT_BOTTOM[] = {0xc8901a, 0x2a1a0e, 0x1f6a3e, 0xa85a10, 0xa3232c};
constexpr uint32_t LIGHT_GLOW[] = {color::BULB, 0, color::GREEN, color::AMBER, color::NEON};
constexpr int32_t CORNER_PX = 8;          // host-screens.md, Controls: a button's corners
constexpr int32_t RIM_PX = 2;             // host-screens.md, Controls: a button's gold rim
constexpr int32_t BULB_PX = 14;           // a warning light
constexpr int32_t GLOW_PX = 8;            // a lit warning light's glow
constexpr int32_t MARQUEE_PX = 4;         // a marquee bulb under the header
constexpr int32_t MARQUEE_STEP_PX = 10;   // from one marquee bulb to the next
constexpr int32_t TITLE_INSET_PX = 16;    // a title set at the header's left
constexpr int32_t TEXT_X_PX = 12;         // a row's texts without an icon
constexpr int32_t ICON_TEXT_X_PX = 38;    // a row's texts after its icon
constexpr int32_t TITLE_Y_PX = 5;         // a row's title below its top
constexpr int32_t DETAIL_Y_PX = 23;       // a row's detail below its top
constexpr int32_t CHEVRON_X_PX = 10;      // a row's chevron from its right side
constexpr int32_t TRACK_PX = 10;          // a slider's track
constexpr int32_t TRACK_GAP_PX = 10;      // between the track and minus or plus
constexpr int32_t KNOB_PX = 26;           // the knob's steel ball
constexpr int32_t STRIP_PX = 30;          // a dialog's title strip
constexpr int32_t CARD_SHADOW_PX = 14;    // a dialog's shadow
constexpr int32_t CARD_SHADOW_Y_PX = 6;
constexpr int32_t CARD_OUTLINE_PX = 3;       // the night ring around a dialog
constexpr lv_opa_t VIOLET_OPA = 217;         // the storyboard's 0.85
constexpr lv_opa_t ROW_OPA = 230;            // the storyboard's 0.9
constexpr lv_opa_t ROW_RIM_OPA = 140;        // the storyboard's 0.55
constexpr lv_opa_t DIVIDER_OPA = 90;         // the storyboard's 0.35
constexpr lv_opa_t BULB_RIM_OPA = 153;       // the storyboard's 0.6
constexpr lv_opa_t CARD_SHADOW_OPA = 179;    // the storyboard's 0.7
constexpr lv_opa_t CARD_OUTLINE_OPA = 204;   // the storyboard's 0.8
constexpr int32_t HALF = 2;
constexpr int32_t WHOLE_PERCENT = 100;   // the parent's whole width

static lv_style_t plaqueStyle;
static lv_style_t redStyle;
static lv_style_t goldStyle;
static lv_style_t violetStyle;
static lv_style_t pressedStyle;
static lv_style_t disabledStyle;
static lv_style_t rowStyle;
static lv_style_t trackStyle;
static lv_style_t soundFill;
static lv_style_t lightFill;
static lv_style_t knobStyle;
static bool styled = false;

static lv_color_t rgb(uint32_t hex) {
    return lv_color_hex(hex);
}

static void gradient(lv_style_t* s, uint32_t top, uint32_t bottom) {
    lv_style_set_bg_color(s, rgb(top));
    lv_style_set_bg_grad_color(s, rgb(bottom));
    lv_style_set_bg_grad_dir(s, LV_GRAD_DIR_VER);
}

// The styles are shared by every control and set up once, on the first control.
static void ensureStyles() {
    if (styled) {
        return;
    }
    styled = true;
    lv_style_init(&plaqueStyle);
    lv_style_set_radius(&plaqueStyle, CORNER_PX);
    lv_style_set_border_width(&plaqueStyle, RIM_PX);
    lv_style_set_bg_opa(&plaqueStyle, LV_OPA_COVER);
    lv_style_set_text_font(&plaqueStyle, &archivo_black_14);

    lv_style_init(&redStyle);
    gradient(&redStyle, RED_TOP, color::CABINET);
    lv_style_set_border_color(&redStyle, rgb(color::GOLD));
    lv_style_set_text_color(&redStyle, rgb(color::CREAM));

    lv_style_init(&goldStyle);
    gradient(&goldStyle, GOLD_TOP, BOARD_DARK);
    lv_style_set_border_color(&goldStyle, rgb(color::BULB));
    lv_style_set_text_color(&goldStyle, rgb(ON_GOLD));

    lv_style_init(&violetStyle);
    lv_style_set_bg_color(&violetStyle, rgb(color::VELVET));
    lv_style_set_bg_opa(&violetStyle, VIOLET_OPA);
    lv_style_set_border_color(&violetStyle, rgb(color::GOLD));
    lv_style_set_text_color(&violetStyle, rgb(color::CREAM));

    // A pressed plaque or row sinks to the shadow red.
    lv_style_init(&pressedStyle);
    lv_style_set_bg_color(&pressedStyle, rgb(color::SHADOW));
    lv_style_set_bg_opa(&pressedStyle, LV_OPA_COVER);
    lv_style_set_bg_grad_dir(&pressedStyle, LV_GRAD_DIR_NONE);
    lv_style_set_border_color(&pressedStyle, rgb(color::BULB));
    lv_style_set_text_color(&pressedStyle, rgb(color::CREAM));

    lv_style_init(&disabledStyle);
    lv_style_set_bg_color(&disabledStyle, rgb(OFF_FILL));
    lv_style_set_bg_opa(&disabledStyle, LV_OPA_COVER);
    lv_style_set_bg_grad_dir(&disabledStyle, LV_GRAD_DIR_NONE);
    lv_style_set_border_color(&disabledStyle, rgb(OFF_RIM));
    lv_style_set_text_color(&disabledStyle, rgb(OFF_TEXT));

    lv_style_init(&rowStyle);
    lv_style_set_radius(&rowStyle, CORNER_PX);
    lv_style_set_bg_color(&rowStyle, rgb(color::VELVET));
    lv_style_set_bg_opa(&rowStyle, ROW_OPA);
    lv_style_set_border_width(&rowStyle, 1);
    lv_style_set_border_color(&rowStyle, rgb(color::GOLD));
    lv_style_set_border_opa(&rowStyle, ROW_RIM_OPA);

    lv_style_init(&trackStyle);
    lv_style_set_bg_color(&trackStyle, rgb(TRACK));
    lv_style_set_bg_opa(&trackStyle, LV_OPA_COVER);
    lv_style_set_border_width(&trackStyle, 1);
    lv_style_set_border_color(&trackStyle, rgb(TRACK_RIM));
    lv_style_set_radius(&trackStyle, TRACK_PX / HALF);

    lv_style_init(&soundFill);
    gradient(&soundFill, RAMP_LIGHT, color::RAMP);
    lv_style_set_bg_opa(&soundFill, LV_OPA_COVER);
    lv_style_set_radius(&soundFill, TRACK_PX / HALF);
    lv_style_init(&lightFill);
    gradient(&lightFill, color::BOARD, BOARD_DARK);
    lv_style_set_bg_opa(&lightFill, LV_OPA_COVER);
    lv_style_set_radius(&lightFill, TRACK_PX / HALF);

    // The knob's padding around the track's height makes the steel ball KNOB_PX across.
    lv_style_init(&knobStyle);
    gradient(&knobStyle, STEEL_LIGHT, STEEL_DARK);
    lv_style_set_bg_opa(&knobStyle, LV_OPA_COVER);
    lv_style_set_radius(&knobStyle, LV_RADIUS_CIRCLE);
    lv_style_set_border_width(&knobStyle, 1);
    lv_style_set_border_color(&knobStyle, rgb(INK));
    lv_style_set_pad_all(&knobStyle, (KNOB_PX - TRACK_PX) / HALF);
}

/// Creates an object without LVGL's theme, which the controls draw themselves.
static lv_obj_t* bare(lv_obj_t* parent) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_scrollable(o, false);
    return o;
}

lv_obj_t* label(lv_obj_t* parent, const char* text, const lv_font_t* font, uint32_t hex) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, rgb(hex), 0);
    return l;
}

lv_obj_t* button(lv_obj_t* parent, Plaque plaque, const char* text, int32_t width) {
    ensureStyles();
    lv_obj_t* b = bare(parent);
    lv_obj_set_size(b, width, TARGET_PX);
    lv_obj_add_style(b, &plaqueStyle, 0);
    lv_obj_add_style(b,
                     plaque == Plaque::GOLD  ? &goldStyle
                     : plaque == Plaque::RED ? &redStyle
                                             : &violetStyle,
                     0);
    lv_obj_add_style(b, &pressedStyle, LV_STATE_PRESSED);
    lv_obj_add_style(b, &disabledStyle, LV_STATE_DISABLED);
    // The label takes its font and its color from the plaque, so a state changes both.
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_obj_center(l);
    return b;
}

// The marquee bulbs are drawn into the strip, so the header costs one object for all of them.
static void drawMarquee(lv_event_t* e) {
    lv_obj_t* strip = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    lv_layer_t* layer = lv_event_get_layer(e);
    lv_area_t box;
    lv_obj_get_coords(strip, &box);
    lv_draw_rect_dsc_t dot;
    lv_draw_rect_dsc_init(&dot);
    dot.radius = LV_RADIUS_CIRCLE;
    const int32_t top = box.y1 + (BULBS_PX - MARQUEE_PX) / HALF;
    int32_t i = 0;
    for (int32_t x = box.x1 + (MARQUEE_STEP_PX - MARQUEE_PX) / HALF; x + MARQUEE_PX <= box.x2 + 1;
         x += MARQUEE_STEP_PX, ++i) {
        dot.bg_color = rgb(i % HALF == 0 ? color::BULB : color::BULB_OFF);
        const lv_area_t at = {x, top, x + MARQUEE_PX - 1, top + MARQUEE_PX - 1};
        lv_draw_rect(layer, &dot, &at);
    }
}

lv_obj_t* header(lv_obj_t* parent, const char* title, bool centered) {
    ensureStyles();
    lv_obj_t* h = bare(parent);
    lv_obj_set_size(h, lv_pct(WHOLE_PERCENT), HEADER_PX + BULBS_PX);
    lv_obj_set_clickable(h, false);
    lv_obj_t* band = bare(h);
    lv_obj_set_size(band, lv_pct(WHOLE_PERCENT), HEADER_PX);
    lv_obj_set_clickable(band, false);
    lv_obj_set_style_bg_opa(band, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(band, rgb(color::FRAME), 0);
    lv_obj_set_style_bg_grad_color(band, rgb(BAND_BOTTOM), 0);
    lv_obj_set_style_bg_grad_dir(band, LV_GRAD_DIR_VER, 0);
    lv_obj_t* t = label(band, title, &archivo_black_17, color::CREAM);
    if (centered) {
        lv_obj_center(t);
    } else {
        lv_obj_align(t, LV_ALIGN_LEFT_MID, TITLE_INSET_PX, 0);
    }
    lv_obj_t* strip = bare(h);
    lv_obj_set_size(strip, lv_pct(WHOLE_PERCENT), BULBS_PX);
    lv_obj_set_y(strip, HEADER_PX);
    lv_obj_set_clickable(strip, false);
    lv_obj_set_style_bg_opa(strip, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(strip, rgb(ON_GOLD), 0);
    lv_obj_add_event_cb(strip, drawMarquee, LV_EVENT_DRAW_MAIN_END, nullptr);
    return h;
}

lv_obj_t* headerButton(lv_obj_t* header, bool left, const char* symbol) {
    ensureStyles();
    lv_obj_t* band = lv_obj_get_child(header, 0);
    lv_obj_t* b = bare(band);
    lv_obj_set_size(b, TARGET_PX, HEADER_PX);
    lv_obj_align(b, left ? LV_ALIGN_LEFT_MID : LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_border_color(b, rgb(color::GOLD), 0);
    lv_obj_set_style_border_opa(b, DIVIDER_OPA, 0);
    lv_obj_set_style_border_side(b, left ? LV_BORDER_SIDE_RIGHT : LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_bg_color(b, rgb(color::SHADOW), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_center(label(b, symbol, &lv_font_montserrat_14, color::GOLD));
    return b;
}

lv_obj_t* bulb(lv_obj_t* parent, Light light) {
    lv_obj_t* b = bare(parent);
    lv_obj_set_size(b, BULB_PX, BULB_PX);
    lv_obj_set_clickable(b, false);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_dir(b, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_border_color(b, rgb(BLACK), 0);
    lv_obj_set_style_border_opa(b, BULB_RIM_OPA, 0);
    setLight(b, light);
    return b;
}

void setLight(lv_obj_t* bulb, Light light) {
    const auto i = uint8_t(light);
    lv_obj_set_style_bg_color(bulb, rgb(LIGHT_TOP[i]), 0);
    lv_obj_set_style_bg_grad_color(bulb, rgb(LIGHT_BOTTOM[i]), 0);
    // An unlit bulb casts no glow.
    lv_obj_set_style_shadow_width(bulb, light == Light::UNLIT ? 0 : GLOW_PX, 0);
    lv_obj_set_style_shadow_color(bulb, rgb(LIGHT_GLOW[i]), 0);
    lv_obj_set_style_shadow_opa(bulb, CARD_SHADOW_OPA, 0);
}

lv_obj_t* row(lv_obj_t* parent, const char* title, const char* detail, int32_t width,
              int32_t height, const char* icon, uint32_t iconRgb) {
    ensureStyles();
    lv_obj_t* r = bare(parent);
    lv_obj_set_size(r, width, height);
    lv_obj_add_style(r, &rowStyle, 0);
    lv_obj_add_style(r, &pressedStyle, LV_STATE_PRESSED);
    const int32_t x = icon == nullptr ? TEXT_X_PX : ICON_TEXT_X_PX;
    // The detail comes first, so setDetail() finds it as the row's first child.
    lv_obj_set_pos(label(r, detail, &montserrat_semibold_12, color::MUTED), x, DETAIL_Y_PX);
    lv_obj_set_pos(label(r, title, &montserrat_semibold_14, color::CREAM), x, TITLE_Y_PX);
    lv_obj_align(label(r, LV_SYMBOL_RIGHT, &lv_font_montserrat_14, color::GOLD), LV_ALIGN_RIGHT_MID,
                 -CHEVRON_X_PX, 0);
    if (icon != nullptr && icon[0] != '\0') {
        lv_obj_align(label(r, icon, &lv_font_montserrat_14, iconRgb), LV_ALIGN_LEFT_MID, TEXT_X_PX,
                     0);
    }
    return r;
}

void setDetail(lv_obj_t* row, const char* detail, uint32_t hex) {
    lv_obj_t* d = lv_obj_get_child(row, 0);
    lv_label_set_text(d, detail);
    lv_obj_set_style_text_color(d, rgb(hex), 0);
}

// Minus and plus move the slider by a step and tell its listeners as a finger on the track does.
static void stepped(lv_event_t* e) {
    lv_obj_t* track = static_cast<lv_obj_t*>(lv_event_get_user_data(e));
    lv_obj_t* pressed = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    const bool up = lv_obj_get_index(pressed) == 1;
    int32_t v = lv_slider_get_value(track) + (up ? STEP_PERCENT : -STEP_PERCENT);
    v = v < 0 ? 0 : v > SLIDER_MAX ? SLIDER_MAX : v;
    lv_slider_set_value(track, v, LV_ANIM_OFF);
    lv_obj_send_event(track, LV_EVENT_VALUE_CHANGED, nullptr);
}

lv_obj_t* slider(lv_obj_t* parent, Fill fill, int32_t width) {
    ensureStyles();
    lv_obj_t* frame = bare(parent);
    lv_obj_set_size(frame, width, TARGET_PX);
    lv_obj_set_clickable(frame, false);
    lv_obj_t* minus = button(frame, Plaque::VIOLET, "-", TARGET_PX);
    lv_obj_t* plus = button(frame, Plaque::VIOLET, "+", TARGET_PX);
    lv_obj_align(plus, LV_ALIGN_RIGHT_MID, 0, 0);
    for (lv_obj_t* b : {minus, plus}) {
        lv_obj_set_style_text_font(b, &archivo_black_20, 0);
    }
    lv_obj_t* track = lv_slider_create(frame);
    lv_obj_remove_style_all(track);
    lv_obj_set_size(track, width - (TARGET_PX + TRACK_GAP_PX) * HALF, TRACK_PX);
    lv_obj_align(track, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_style(track, &trackStyle, LV_PART_MAIN);
    lv_obj_add_style(track, fill == Fill::SOUND ? &soundFill : &lightFill, LV_PART_INDICATOR);
    lv_obj_add_style(track, &knobStyle, LV_PART_KNOB);
    lv_slider_set_range(track, 0, SLIDER_MAX);
    lv_obj_add_event_cb(minus, stepped, LV_EVENT_CLICKED, track);
    lv_obj_add_event_cb(plus, stepped, LV_EVENT_CLICKED, track);
    return frame;
}

lv_obj_t* sliderTrack(lv_obj_t* slider) {
    // slider() creates the minus, the plus and then the track.
    return lv_obj_get_child(slider, HALF);
}

lv_obj_t* dialog(lv_obj_t* parent, const char* title, int32_t width, int32_t height) {
    ensureStyles();
    // A dark backdrop over the whole parent dims it and takes every touch beside the card.
    lv_obj_t* backdrop = bare(parent);
    lv_obj_set_size(backdrop, lv_pct(WHOLE_PERCENT), lv_pct(WHOLE_PERCENT));
    lv_obj_set_clickable(backdrop, true);
    lv_obj_set_style_bg_color(backdrop, rgb(BLACK), 0);
    lv_obj_set_style_bg_opa(backdrop, LV_OPA_50, 0);
    lv_obj_t* card = bare(backdrop);
    lv_obj_center(card);
    lv_obj_set_size(card, width, height);
    lv_obj_set_style_radius(card, CORNER_PX, 0);
    lv_obj_set_style_clip_corner(card, true, 0);
    lv_obj_set_style_bg_color(card, rgb(color::NIGHT), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, RIM_PX, 0);
    lv_obj_set_style_border_color(card, rgb(color::GOLD), 0);
    lv_obj_set_style_outline_width(card, CARD_OUTLINE_PX, 0);
    lv_obj_set_style_outline_color(card, rgb(color::NIGHT), 0);
    lv_obj_set_style_outline_opa(card, CARD_OUTLINE_OPA, 0);
    lv_obj_set_style_shadow_width(card, CARD_SHADOW_PX, 0);
    lv_obj_set_style_shadow_offset_y(card, CARD_SHADOW_Y_PX, 0);
    lv_obj_set_style_shadow_color(card, rgb(BLACK), 0);
    lv_obj_set_style_shadow_opa(card, CARD_SHADOW_OPA, 0);
    lv_obj_t* strip = bare(card);
    lv_obj_set_size(strip, lv_pct(WHOLE_PERCENT), STRIP_PX);
    lv_obj_set_clickable(strip, false);
    lv_obj_set_style_bg_opa(strip, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(strip, rgb(color::FRAME), 0);
    lv_obj_set_style_bg_grad_color(strip, rgb(BAND_BOTTOM), 0);
    lv_obj_set_style_bg_grad_dir(strip, LV_GRAD_DIR_VER, 0);
    lv_obj_center(label(strip, title, &archivo_black_14, color::CREAM));
    return card;
}

void closeDialog(lv_obj_t* dialog) {
    lv_obj_delete_async(lv_obj_get_parent(dialog));
}

}   // namespace controls
