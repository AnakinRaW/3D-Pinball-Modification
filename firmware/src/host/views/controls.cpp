#include "controls.h"
#include <initializer_list>
#include <math.h>

namespace controls {

// The shades the storyboard's CSS sets beside the colors of host-screens.md's table.
constexpr uint32_t RED_TOP = 0xc8323f;       // a red plaque's top
constexpr uint32_t GOLD_TOP = 0xf6dc72;      // a gold plaque's top
constexpr uint32_t BOARD_DARK = 0xb39344;    // a gold plaque's bottom, the light fill's bottom
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
// cards: the rim of each Tone, and the top and the bottom of its title strip
constexpr uint32_t TONE_RIM[] = {color::GOLD, color::NEON, color::GOLD, color::GREEN};
constexpr uint32_t STRIP_TOP[] = {color::FRAME, 0xa3232c, 0xc7741e, color::FRAME};
constexpr uint32_t STRIP_BOTTOM[] = {BAND_BOTTOM, color::SHADOW, 0x7a3e0c, BAND_BOTTOM};
constexpr int32_t CARD_GLOW_PX = 16;   // the glow of a card's fault or of a device that works
constexpr int32_t CARD_GLOW_SPREAD_PX = 2;
constexpr lv_opa_t SHADE_OPA = 184;   // the storyboard's 0.72, a dialog's backdrop
// tiles and keys
constexpr uint32_t TILE_TOP = 0x3c105c;   // a tile's and a key's top, above the velvet
constexpr lv_opa_t TILE_OPA = 242;        // the storyboard's 0.95
constexpr lv_opa_t WORKS_RIM_OPA = 115;   // the storyboard's 0.45, a tile of a device that works
constexpr int32_t KEY_CORNER_PX = 6;
constexpr lv_opa_t KEY_RIM_OPA = 153;   // the storyboard's 0.6
// a lit plaque: its gold mixed into its dark, strong at its top and fainter at its foot, so it
// reads as a gold plaque that nothing shows through
constexpr lv_opa_t LINE_GOLD_OPA = 150;
constexpr lv_opa_t LINE_GOLD_FADE_OPA = 70;
constexpr uint32_t RGB_MASK = 0xFFFFFF;   // a color's red, green and blue, without its alpha
constexpr lv_opa_t LIT_GLOW_OPA = 115;
// pagers
constexpr int32_t PAGER_BULB_PX = 8;
constexpr int32_t PAGER_GAP_PX = 6;
constexpr int32_t PAGER_GLOW_PX = 4;
// the sunburst, from the storyboard's conic gradient at 50 % across and 62 % down
constexpr int32_t RAY_CENTER_PERMILLE = 620;
constexpr int32_t PERMILLE = 1000;
constexpr int32_t RAY_DEG = 9;   // each ray's angle
constexpr int32_t FULL_TURN_DEG = 360;
constexpr int32_t RAY_REACH_PX = 400;       // past every corner of the screen
constexpr lv_opa_t RAY_OPA = 82;            // the storyboard's 0.32
constexpr lv_opa_t DIM_RAY_OPA = 46;        // the storyboard's 0.18
constexpr uint32_t TURN_STEP_MS = 100;      // the sunburst turns ten steps a second
constexpr float TURN_DEG_PER_STEP = 0.3F;   // three degrees a second, as slow as the film's rays
constexpr float DEG_TO_RAD = 0.0174532925F;
constexpr uint8_t TURNING_MAX = 4;   // the sunbursts on the screen at once
// the curtain's folds, one FOLD_PX repeat of the storyboard's stripes, and its tassels
constexpr int32_t FOLD_PX = 20;
constexpr int32_t FOLD_WIDTHS[] = {3, 6, 4, 4, 3};
constexpr uint32_t FOLD_COLORS[] = {0x6d1219, 0xa3232c, color::FRAME, 0xa3232c, 0x6d1219};
constexpr int32_t TASSEL_ROW_PX = 5;
constexpr int32_t TASSEL_PX = 3;
constexpr int32_t TASSEL_STEP_PX = 6;
// the valance's band and its scallops
constexpr int32_t VALANCE_PX = 22;
constexpr int32_t SCALLOP_STEP_PX = 22;
constexpr int32_t SCALLOP_RIM_PX = 11;   // the gold disc's radius
constexpr int32_t SCALLOP_PX = 9;        // the red disc's radius
// the lettering
constexpr uint32_t LETTERING_FILL = 0xffd640;
constexpr uint32_t LETTERING_OUTLINE = 0xc81428;
constexpr int32_t LETTERING_SHADOW_PX = 3;
// a level: its texts and its slider below the name
constexpr int32_t LEVEL_INSET_PX = 4;
constexpr int32_t LEVEL_VALUE_RISE_PX = 2;
constexpr int32_t LEVEL_SLIDER_Y_PX = 20;
constexpr int32_t PROGRESS_PX = 14;
constexpr int32_t LIST_INSET_PX = 6;   // a list's lines from the screen's sides
constexpr int32_t CUE_PX = 36;         // a list's fade at an edge with more beyond it
constexpr int32_t CUE_ARROW_PX = 2;    // the cue's arrow from the list's edge
// a switch row's switch: its track and the steel knob inside it
constexpr int32_t SWITCH_WIDTH_PX = 40;
constexpr int32_t SWITCH_HEIGHT_PX = 22;
constexpr int32_t SWITCH_KNOB_PX = 18;
constexpr int32_t SWITCH_INSET_PX = 2;
// a line of a list of scores
constexpr uint32_t STAGE_DARK = 0x08040e;
constexpr lv_opa_t STAGE_DARK_OPA = 217;     // the storyboard's 0.85
constexpr lv_opa_t PAGE_LINE_OPA = 191;      // the storyboard's 0.75
constexpr lv_opa_t DARK_LINE_RIM_OPA = 77;   // the storyboard's 0.3
constexpr lv_opa_t PAGE_LINE_RIM_OPA = 64;   // the storyboard's 0.25
constexpr int32_t LINE_CORNER_PX = 6;
constexpr char MINUS[] = "−";   // U+2212, the minus sign beside a slider

static lv_obj_t* turning[TURNING_MAX] = {};   // the sunbursts the timer turns
static lv_timer_t* turnTimer = nullptr;
static float turnDeg = 0;   // how far the sunbursts have turned, within one crimson and velvet pair
static bool animated = true;
static lv_style_t tileStyle;
static lv_style_t keyStyle;
static lv_style_t litStyle;
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
    lv_style_set_text_color(&goldStyle, rgb(color::ON_GOLD));

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

    lv_style_init(&tileStyle);
    lv_style_set_radius(&tileStyle, CORNER_PX);
    gradient(&tileStyle, TILE_TOP, color::VELVET);
    lv_style_set_bg_opa(&tileStyle, TILE_OPA);

    lv_style_init(&keyStyle);
    lv_style_set_radius(&keyStyle, KEY_CORNER_PX);
    gradient(&keyStyle, TILE_TOP, color::VELVET);
    lv_style_set_bg_opa(&keyStyle, TILE_OPA);
    lv_style_set_border_width(&keyStyle, 1);
    lv_style_set_border_color(&keyStyle, rgb(color::GOLD));
    lv_style_set_border_opa(&keyStyle, KEY_RIM_OPA);

    // A lit plaque is gold mixed into the velvet, stronger at its top. The gold is mixed into
    // opaque colors, since LVGL's software drawing leaves out a gradient whose stops carry their
    // own opacity.
    lv_style_init(&litStyle);
    lv_style_set_radius(&litStyle, KEY_CORNER_PX);
    lv_style_set_bg_opa(&litStyle, LV_OPA_COVER);
    gradient(
        &litStyle,
        lv_color_to_u32(lv_color_mix(rgb(color::GOLD), rgb(color::VELVET), LINE_GOLD_OPA)) &
            RGB_MASK,
        lv_color_to_u32(lv_color_mix(rgb(color::GOLD), rgb(color::VELVET), LINE_GOLD_FADE_OPA)) &
            RGB_MASK);
    lv_style_set_border_width(&litStyle, RIM_PX);
    lv_style_set_border_color(&litStyle, rgb(color::BULB));
    lv_style_set_shadow_width(&litStyle, GLOW_PX);
    lv_style_set_shadow_color(&litStyle, rgb(color::BULB));
    lv_style_set_shadow_opa(&litStyle, LIT_GLOW_OPA);
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
    lv_obj_set_style_bg_color(strip, rgb(color::ON_GOLD), 0);
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

/// Places a row's title at @p x, in the row's middle without a detail and above it with one.
static void placeTitle(lv_obj_t* title, int32_t x, bool alone) {
    if (alone) {
        lv_obj_align(title, LV_ALIGN_LEFT_MID, x, 0);
    } else {
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, x, TITLE_Y_PX);
    }
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
    placeTitle(label(r, title, &montserrat_bold_14, color::CREAM), x, detail[0] == '\0');
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
    placeTitle(lv_obj_get_child(row, 1), lv_obj_get_x(d), detail[0] == '\0');
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
    lv_obj_t* minus = button(frame, Plaque::VIOLET, MINUS, TARGET_PX);
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

lv_obj_t* card(lv_obj_t* parent, Tone tone, const char* title, int32_t width, int32_t height) {
    ensureStyles();
    const auto t = uint8_t(tone);
    lv_obj_t* c = bare(parent);
    lv_obj_set_size(c, width, height);
    lv_obj_set_style_radius(c, CORNER_PX, 0);
    lv_obj_set_style_bg_color(c, rgb(color::NIGHT), 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(c, RIM_PX, 0);
    lv_obj_set_style_border_color(c, rgb(TONE_RIM[t]), 0);
    // A fault and a device that works glow in their color. A note or a warning stands on a night
    // ring with a dark shadow.
    const bool glows = tone == Tone::FAULT || tone == Tone::WORKS;
    if (!glows) {
        lv_obj_set_style_outline_width(c, CARD_OUTLINE_PX, 0);
        lv_obj_set_style_outline_color(c, rgb(color::NIGHT), 0);
        lv_obj_set_style_outline_opa(c, CARD_OUTLINE_OPA, 0);
    }
    lv_obj_set_style_shadow_width(c, glows ? CARD_GLOW_PX : CARD_SHADOW_PX, 0);
    lv_obj_set_style_shadow_spread(c, glows ? CARD_GLOW_SPREAD_PX : 0, 0);
    lv_obj_set_style_shadow_offset_y(c, glows ? 0 : CARD_SHADOW_Y_PX, 0);
    lv_obj_set_style_shadow_color(c, rgb(glows ? TONE_RIM[t] : BLACK), 0);
    lv_obj_set_style_shadow_opa(c, CARD_SHADOW_OPA, 0);
    if (title != nullptr) {
        lv_obj_t* strip = bare(c);
        lv_obj_set_size(strip, lv_pct(WHOLE_PERCENT), STRIP_PX);
        lv_obj_set_clickable(strip, false);
        lv_obj_set_style_bg_opa(strip, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(strip, rgb(STRIP_TOP[t]), 0);
        lv_obj_set_style_bg_grad_color(strip, rgb(STRIP_BOTTOM[t]), 0);
        lv_obj_set_style_bg_grad_dir(strip, LV_GRAD_DIR_VER, 0);
        // The strip rounds its top corners into the rim's, and two squares of its bottom color
        // square its lower corners. LVGL's clip_corner on the card would round them, but it
        // paints over the strip's top rows on a glowing card over a backdrop.
        lv_obj_set_style_radius(strip, CORNER_PX - RIM_PX, 0);
        for (const lv_align_t corner : {LV_ALIGN_BOTTOM_LEFT, LV_ALIGN_BOTTOM_RIGHT}) {
            lv_obj_t* square = bare(strip);
            lv_obj_set_size(square, CORNER_PX - RIM_PX, CORNER_PX - RIM_PX);
            lv_obj_set_clickable(square, false);
            lv_obj_set_style_bg_opa(square, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(square, rgb(STRIP_BOTTOM[t]), 0);
            lv_obj_align(square, corner, 0, 0);
        }
        lv_obj_center(label(strip, title, &archivo_black_14, color::CREAM));
    }
    return c;
}

lv_obj_t* dialog(lv_obj_t* parent, const char* title, int32_t width, int32_t height, Tone tone) {
    ensureStyles();
    // A backdrop over the whole parent dims it and takes every touch beside the card.
    lv_obj_t* backdrop = bare(parent);
    lv_obj_set_size(backdrop, lv_pct(WHOLE_PERCENT), lv_pct(WHOLE_PERCENT));
    lv_obj_set_clickable(backdrop, true);
    lv_obj_set_style_bg_color(backdrop, rgb(color::NIGHT), 0);
    lv_obj_set_style_bg_opa(backdrop, SHADE_OPA, 0);
    lv_obj_t* c = card(backdrop, tone, title, width, height);
    lv_obj_center(c);
    return c;
}

void closeDialog(lv_obj_t* dialog) {
    lv_obj_delete_async(lv_obj_get_parent(dialog));
}

lv_obj_t* tile(lv_obj_t* parent, Tone tone, int32_t width, int32_t height) {
    ensureStyles();
    lv_obj_t* t = bare(parent);
    lv_obj_set_size(t, width, height);
    lv_obj_add_style(t, &tileStyle, 0);
    lv_obj_add_style(t, &pressedStyle, LV_STATE_PRESSED);
    const bool works = tone == Tone::WORKS;
    lv_obj_set_style_border_width(t, works ? 1 : RIM_PX, 0);
    lv_obj_set_style_border_color(t, rgb(works ? color::GOLD : TONE_RIM[uint8_t(tone)]), 0);
    lv_obj_set_style_border_opa(t, works ? WORKS_RIM_OPA : lv_opa_t(LV_OPA_COVER), 0);
    if (tone == Tone::FAULT) {
        lv_obj_set_style_shadow_width(t, GLOW_PX, 0);
        lv_obj_set_style_shadow_color(t, rgb(color::NEON), 0);
        lv_obj_set_style_shadow_opa(t, CARD_SHADOW_OPA, 0);
    }
    return t;
}

lv_obj_t* pager(lv_obj_t* parent, uint8_t pages, uint8_t shown) {
    lv_obj_t* p = bare(parent);
    lv_obj_set_size(p, LV_SIZE_CONTENT, PAGER_BULB_PX);
    lv_obj_set_clickable(p, false);
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(p, PAGER_GAP_PX, 0);
    for (uint8_t i = 0; i < pages && i < PAGES_MAX; ++i) {
        lv_obj_t* b = bulb(p, Light::UNLIT);
        lv_obj_set_size(b, PAGER_BULB_PX, PAGER_BULB_PX);
        lv_obj_set_style_shadow_width(b, PAGER_GLOW_PX, 0);
    }
    setPage(p, shown);
    return p;
}

void setPage(lv_obj_t* pager, uint8_t shown) {
    const uint32_t n = lv_obj_get_child_count(pager);
    for (uint32_t i = 0; i < n; ++i) {
        lv_obj_t* b = lv_obj_get_child(pager, int32_t(i));
        setLight(b, i == shown ? Light::LIT : Light::UNLIT);
        lv_obj_set_style_shadow_width(b, i == shown ? PAGER_GLOW_PX : 0, 0);
    }
}

// The rays are opaque wedges of the crimson ray mixed into the night, drawn over a fill of the
// velvet ray mixed into it, which gives the storyboard's translucent sunburst at the cost of
// opaque triangles.
static void drawRays(lv_event_t* e) {
    lv_obj_t* o = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    const bool dim = lv_obj_get_user_data(o) != nullptr;
    lv_layer_t* layer = lv_event_get_layer(e);
    lv_area_t box;
    lv_obj_get_coords(o, &box);
    const int32_t cx = box.x1 + lv_area_get_width(&box) / HALF;
    const int32_t cy = box.y1 + lv_area_get_height(&box) * RAY_CENTER_PERMILLE / PERMILLE;
    lv_draw_triangle_dsc_t t;
    lv_draw_triangle_dsc_init(&t);
    t.color = lv_color_mix(rgb(color::RAY), rgb(color::NIGHT), dim ? DIM_RAY_OPA : RAY_OPA);
    t.opa = LV_OPA_COVER;
    // A point at an angle from the center, clockwise from straight up.
    const auto at = [&](float deg) {
        const float rad = deg * DEG_TO_RAD;
        return lv_point_precise_t{cx + int32_t(RAY_REACH_PX * sinf(rad)),
                                  cy - int32_t(RAY_REACH_PX * cosf(rad))};
    };
    for (int32_t a = 0; a < FULL_TURN_DEG; a += RAY_DEG * HALF) {
        const float from = float(a) + turnDeg;
        t.p[0] = {cx, cy};
        t.p[1] = at(from);
        t.p[2] = at(from + RAY_DEG);
        lv_draw_triangle(layer, &t);
    }
}

// One timer turns every sunburst a step on, so they turn together.
static void turn(lv_timer_t*) {
    turnDeg += TURN_DEG_PER_STEP;
    // The rays repeat after a crimson and a velvet ray.
    if (turnDeg >= float(RAY_DEG * HALF)) {
        turnDeg -= float(RAY_DEG * HALF);
    }
    for (lv_obj_t* r : turning) {
        if (r != nullptr) {
            lv_obj_invalidate(r);
        }
    }
}

static void stopTurning(lv_event_t* e) {
    lv_obj_t* r = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    for (lv_obj_t*& t : turning) {
        if (t == r) {
            t = nullptr;
        }
    }
}

void setAnimated(bool on) {
    animated = on;
    if (turnTimer != nullptr) {
        if (on) {
            lv_timer_resume(turnTimer);
        } else {
            lv_timer_pause(turnTimer);
        }
    }
}

bool isAnimated() {
    return animated;
}

lv_obj_t* rays(lv_obj_t* parent, bool dim) {
    lv_obj_t* r = bare(parent);
    lv_obj_set_size(r, lv_pct(WHOLE_PERCENT), lv_pct(WHOLE_PERCENT));
    lv_obj_set_clickable(r, false);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(
        r, lv_color_mix(rgb(color::VELVET), rgb(color::NIGHT), dim ? DIM_RAY_OPA : RAY_OPA), 0);
    lv_obj_set_user_data(r, dim ? r : nullptr);
    lv_obj_add_event_cb(r, drawRays, LV_EVENT_DRAW_MAIN_END, nullptr);
    for (lv_obj_t*& t : turning) {
        if (t == nullptr) {
            t = r;
            lv_obj_add_event_cb(r, stopTurning, LV_EVENT_DELETE, nullptr);
            break;
        }
    }
    if (turnTimer == nullptr) {
        turnTimer = lv_timer_create(turn, TURN_STEP_MS, nullptr);
        setAnimated(animated);
    }
    return r;
}

// A curtain's folds repeat every FOLD_PX: a dark crease, a red fold, the frame's red at its crest,
// the red fold and the dark crease again. A row of gold tassels runs along the foot.
static void drawCurtain(lv_event_t* e) {
    lv_obj_t* o = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    lv_layer_t* layer = lv_event_get_layer(e);
    lv_area_t box;
    lv_obj_get_coords(o, &box);
    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    for (int32_t x = box.x1; x <= box.x2; x += FOLD_PX) {
        int32_t at = x;
        for (size_t i = 0; i < sizeof(FOLD_WIDTHS) / sizeof(FOLD_WIDTHS[0]); ++i) {
            r.bg_color = rgb(FOLD_COLORS[i]);
            const lv_area_t band = {at, box.y1, at + FOLD_WIDTHS[i] - 1, box.y2};
            lv_draw_rect(layer, &r, &band);
            at += FOLD_WIDTHS[i];
        }
    }
    r.bg_color = rgb(color::GOLD);
    r.radius = LV_RADIUS_CIRCLE;
    const int32_t top = box.y2 - TASSEL_ROW_PX + (TASSEL_ROW_PX - TASSEL_PX) / HALF + 1;
    for (int32_t x = box.x1 + (TASSEL_STEP_PX - TASSEL_PX) / HALF; x + TASSEL_PX - 1 <= box.x2;
         x += TASSEL_STEP_PX) {
        const lv_area_t dot = {x, top, x + TASSEL_PX - 1, top + TASSEL_PX - 1};
        lv_draw_rect(layer, &r, &dot);
    }
}

lv_obj_t* curtain(lv_obj_t* parent, int32_t width) {
    lv_obj_t* c = bare(parent);
    lv_obj_set_size(c, width, lv_pct(WHOLE_PERCENT));
    lv_obj_set_clickable(c, false);
    lv_obj_add_event_cb(c, drawCurtain, LV_EVENT_DRAW_MAIN_END, nullptr);
    return c;
}

// The scallops are gold discs with a smaller red disc on each, centered on the band's lower edge.
// The band, a child, then covers their upper halves.
static void drawScallops(lv_event_t* e) {
    lv_obj_t* o = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    lv_layer_t* layer = lv_event_get_layer(e);
    lv_area_t box;
    lv_obj_get_coords(o, &box);
    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.radius = LV_RADIUS_CIRCLE;
    const int32_t cy = box.y1 + VALANCE_PX;
    for (int32_t x = box.x1; x <= box.x2; x += SCALLOP_STEP_PX) {
        const int32_t cx = x + SCALLOP_STEP_PX / HALF;
        r.bg_color = rgb(color::GOLD);
        const lv_area_t rim = {cx - SCALLOP_RIM_PX, cy - SCALLOP_RIM_PX, cx + SCALLOP_RIM_PX - 1,
                               cy + SCALLOP_RIM_PX - 1};
        lv_draw_rect(layer, &r, &rim);
        r.bg_color = rgb(BAND_BOTTOM);
        const lv_area_t disc = {cx - SCALLOP_PX, cy - SCALLOP_PX, cx + SCALLOP_PX - 1,
                                cy + SCALLOP_PX - 1};
        lv_draw_rect(layer, &r, &disc);
    }
}

lv_obj_t* valance(lv_obj_t* parent) {
    lv_obj_t* v = bare(parent);
    lv_obj_set_size(v, lv_pct(WHOLE_PERCENT), VALANCE_PX + SCALLOP_RIM_PX);
    lv_obj_set_clickable(v, false);
    lv_obj_add_event_cb(v, drawScallops, LV_EVENT_DRAW_MAIN_END, nullptr);
    lv_obj_t* band = bare(v);
    lv_obj_set_size(band, lv_pct(WHOLE_PERCENT), VALANCE_PX);
    lv_obj_set_clickable(band, false);
    lv_obj_set_style_bg_opa(band, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(band, rgb(color::FRAME), 0);
    lv_obj_set_style_bg_grad_color(band, rgb(BAND_BOTTOM), 0);
    lv_obj_set_style_bg_grad_dir(band, LV_GRAD_DIR_VER, 0);
    return v;
}

// The lettering draws its text nine times: the shadow below, the outline around and the yellow on
// top. Its user data holds a copy of the text, which goes with the object.
static void drawLettering(lv_event_t* e) {
    lv_obj_t* o = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    lv_layer_t* layer = lv_event_get_layer(e);
    lv_area_t box;
    lv_obj_get_coords(o, &box);
    const int32_t w = lv_obj_get_style_outline_pad(o, LV_PART_MAIN);
    lv_draw_label_dsc_t d;
    lv_draw_label_dsc_init(&d);
    d.text = static_cast<const char*>(lv_obj_get_user_data(o));
    d.font = lv_obj_get_style_text_font(o, LV_PART_MAIN);
    // Draws the text with its top left at x, y from the lettering's top left.
    const auto put = [&](int32_t x, int32_t y, uint32_t hex) {
        d.color = rgb(hex);
        lv_area_t at = box;
        lv_area_move(&at, x, y);
        lv_draw_label(layer, &d, &at);
    };
    put(w, w + LETTERING_SHADOW_PX, color::SHADOW);
    for (int32_t dy = -w; dy <= w; dy += w) {
        for (int32_t dx = -w; dx <= w; dx += w) {
            if (dx != 0 || dy != 0) {
                put(w + dx, w + dy, LETTERING_OUTLINE);
            }
        }
    }
    put(w, w, LETTERING_FILL);
}

static void freeText(lv_event_t* e) {
    lv_obj_t* o = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    lv_free(lv_obj_get_user_data(o));
}

lv_obj_t* lettering(lv_obj_t* parent, const char* text, const lv_font_t* font, int32_t outline) {
    lv_obj_t* l = bare(parent);
    lv_obj_set_clickable(l, false);
    lv_point_t size;
    lv_text_get_size(&size, text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_obj_set_size(l, size.x + outline * HALF, size.y + outline * HALF + LETTERING_SHADOW_PX);
    lv_obj_set_style_text_font(l, font, 0);
    // The outline's width rides in a style the lettering draws nothing else with.
    lv_obj_set_style_outline_pad(l, outline, 0);
    lv_obj_set_user_data(l, lv_strdup(text));
    lv_obj_add_event_cb(l, drawLettering, LV_EVENT_DRAW_MAIN_END, nullptr);
    lv_obj_add_event_cb(l, freeText, LV_EVENT_DELETE, nullptr);
    return l;
}

// A level's children after its name: its value and its slider.
constexpr int32_t LEVEL_VALUE = 1;
constexpr int32_t LEVEL_SLIDER = 2;

static void showValue(lv_obj_t* level) {
    lv_label_set_text_fmt(lv_obj_get_child(level, LEVEL_VALUE), "%ld %%",
                          static_cast<long>(lv_slider_get_value(levelTrack(level))));
}

static void levelChanged(lv_event_t* e) {
    showValue(static_cast<lv_obj_t*>(lv_event_get_user_data(e)));
}

lv_obj_t* level(lv_obj_t* parent, const char* name, Fill fill, int32_t value, int32_t width) {
    ensureStyles();
    lv_obj_t* lv = bare(parent);
    lv_obj_set_size(lv, width, LEVEL_PX);
    lv_obj_set_clickable(lv, false);
    // The value stands a little above the name's line.
    lv_obj_set_overflow_visible(lv, true);
    lv_obj_set_pos(label(lv, name, &archivo_black_13, color::CREAM), LEVEL_INSET_PX, 0);
    lv_obj_align(label(lv, "", &jetbrains_mono_bold_16, color::CYAN), LV_ALIGN_TOP_RIGHT,
                 -LEVEL_INSET_PX, -LEVEL_VALUE_RISE_PX);
    lv_obj_t* s = slider(lv, fill, width);
    lv_obj_set_y(s, LEVEL_SLIDER_Y_PX);
    lv_obj_add_event_cb(sliderTrack(s), levelChanged, LV_EVENT_VALUE_CHANGED, lv);
    setLevel(lv, value);
    return lv;
}

lv_obj_t* levelTrack(lv_obj_t* level) {
    return sliderTrack(lv_obj_get_child(level, LEVEL_SLIDER));
}

void setLevel(lv_obj_t* level, int32_t value) {
    lv_slider_set_value(levelTrack(level), value, LV_ANIM_OFF);
    showValue(level);
}

// A list's fades stand right after it among its parent's children: the top fade, then the foot's.
static void showCues(lv_event_t* e) {
    lv_obj_t* list = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    lv_obj_set_hidden(lv_obj_get_sibling(list, 1), lv_obj_get_scroll_top(list) <= 0);
    lv_obj_set_hidden(lv_obj_get_sibling(list, HALF), lv_obj_get_scroll_bottom(list) <= 0);
}

lv_obj_t* scrollList(lv_obj_t* parent, int32_t y, int32_t height) {
    // The list takes the touch, so a finger scrolls it.
    lv_obj_t* list = lv_obj_create(parent);
    lv_obj_remove_style_all(list);
    lv_obj_set_pos(list, 0, y);
    lv_obj_set_size(list, lv_pct(WHOLE_PERCENT), height);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, LIST_GAP_PX, 0);
    lv_obj_set_style_pad_hor(list, LIST_INSET_PX, 0);
    lv_obj_set_style_pad_bottom(list, LIST_GAP_PX, 0);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
    for (const bool top : {true, false}) {
        lv_obj_t* fade = bare(parent);
        lv_obj_set_size(fade, lv_pct(WHOLE_PERCENT), CUE_PX);
        lv_obj_set_pos(fade, 0, top ? y : y + height - CUE_PX);
        lv_obj_set_clickable(fade, false);
        lv_obj_set_style_bg_opa(fade, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(fade, rgb(color::NIGHT), 0);
        lv_obj_set_style_bg_grad_color(fade, rgb(color::NIGHT), 0);
        lv_obj_set_style_bg_main_opa(fade, top ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_grad_opa(fade, top ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
        lv_obj_set_style_bg_grad_dir(fade, LV_GRAD_DIR_VER, 0);
        lv_obj_set_hidden(fade, true);
        // A gold arrow in the fade points to where more lies.
        lv_obj_t* arrow =
            label(fade, top ? LV_SYMBOL_UP : LV_SYMBOL_DOWN, &lv_font_montserrat_14, color::BULB);
        lv_obj_align(arrow, top ? LV_ALIGN_TOP_MID : LV_ALIGN_BOTTOM_MID, 0,
                     top ? CUE_ARROW_PX : -CUE_ARROW_PX);
    }
    lv_obj_add_event_cb(list, showCues, LV_EVENT_SCROLL, nullptr);
    lv_obj_add_event_cb(list, showCues, LV_EVENT_LAYOUT_CHANGED, nullptr);
    return list;
}

lv_obj_t* progress(lv_obj_t* parent, int32_t width) {
    ensureStyles();
    lv_obj_t* b = lv_bar_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, width, PROGRESS_PX);
    lv_obj_add_style(b, &trackStyle, LV_PART_MAIN);
    lv_obj_add_style(b, &soundFill, LV_PART_INDICATOR);
    lv_obj_set_style_radius(b, PROGRESS_PX / HALF, LV_PART_MAIN);
    lv_obj_set_style_radius(b, PROGRESS_PX / HALF, LV_PART_INDICATOR);
    lv_bar_set_range(b, 0, SLIDER_MAX);
    return b;
}

// A switch row's children: its title, then the switch's track with the knob inside it.
constexpr int32_t TOGGLE_TRACK = 1;

// The switch shows gold with its knob at the right while on, and the dark track with its knob at
// the left while off.
static void showSwitch(lv_obj_t* toggle) {
    const bool on = lv_obj_has_state(toggle, LV_STATE_CHECKED);
    lv_obj_t* track = lv_obj_get_child(toggle, TOGGLE_TRACK);
    lv_obj_set_style_bg_color(track, rgb(on ? GOLD_TOP : TRACK), 0);
    lv_obj_set_style_bg_grad_color(track, rgb(on ? BOARD_DARK : TRACK), 0);
    lv_obj_set_style_border_color(track, rgb(on ? color::BULB : TRACK_RIM), 0);
    lv_obj_align(lv_obj_get_child(track, 0), on ? LV_ALIGN_RIGHT_MID : LV_ALIGN_LEFT_MID,
                 on ? -SWITCH_INSET_PX : SWITCH_INSET_PX, 0);
}

static void switched(lv_event_t* e) {
    showSwitch(static_cast<lv_obj_t*>(lv_event_get_current_target(e)));
}

lv_obj_t* toggle(lv_obj_t* parent, const char* title, bool on, int32_t width) {
    ensureStyles();
    lv_obj_t* r = bare(parent);
    lv_obj_set_size(r, width, TARGET_PX);
    lv_obj_add_style(r, &rowStyle, 0);
    lv_obj_add_style(r, &pressedStyle, LV_STATE_PRESSED);
    // LVGL turns a checkable object's checked state on each tap and then sends the change.
    lv_obj_set_checkable(r, true);
    lv_obj_align(label(r, title, &montserrat_bold_14, color::CREAM), LV_ALIGN_LEFT_MID, TEXT_X_PX,
                 0);
    lv_obj_t* track = bare(r);
    lv_obj_set_size(track, SWITCH_WIDTH_PX, SWITCH_HEIGHT_PX);
    lv_obj_align(track, LV_ALIGN_RIGHT_MID, -CHEVRON_X_PX, 0);
    lv_obj_set_clickable(track, false);
    lv_obj_set_style_radius(track, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_dir(track, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(track, 1, 0);
    lv_obj_t* knob = bare(track);
    lv_obj_set_size(knob, SWITCH_KNOB_PX, SWITCH_KNOB_PX);
    lv_obj_set_clickable(knob, false);
    lv_obj_add_style(knob, &knobStyle, 0);
    lv_obj_set_style_pad_all(knob, 0, 0);
    lv_obj_add_event_cb(r, switched, LV_EVENT_VALUE_CHANGED, nullptr);
    setOn(r, on);
    return r;
}

bool isOn(lv_obj_t* toggle) {
    return lv_obj_has_state(toggle, LV_STATE_CHECKED);
}

void setOn(lv_obj_t* toggle, bool on) {
    lv_obj_set_state(toggle, LV_STATE_CHECKED, on);
    showSwitch(toggle);
}

lv_obj_t* key(lv_obj_t* parent, const char* text, int32_t width, bool lit) {
    ensureStyles();
    lv_obj_t* k = bare(parent);
    lv_obj_set_size(k, width, TARGET_PX);
    lv_obj_add_style(k, lit ? &litStyle : &keyStyle, 0);
    lv_obj_add_style(k, &pressedStyle, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(k, RIM_PX, LV_STATE_PRESSED);
    lv_obj_center(label(k, text, &archivo_black_17, color::CREAM));
    return k;
}

lv_obj_t* scoreLine(lv_obj_t* parent, int32_t width, bool lit, bool dark) {
    ensureStyles();
    lv_obj_t* l = bare(parent);
    lv_obj_set_size(l, width, SCORE_LINE_PX);
    lv_obj_set_clickable(l, false);
    if (lit) {
        lv_obj_add_style(l, &litStyle, 0);
        lv_obj_set_style_border_width(l, 1, 0);
        lv_obj_set_style_shadow_width(l, 0, 0);
        // A lit line on the stage mixes its gold into the stage's dark, so the sunburst and the
        // curtains do not show through it.
        if (dark) {
            const lv_color_t base = rgb(STAGE_DARK);
            lv_obj_set_style_bg_color(l, lv_color_mix(rgb(color::GOLD), base, LINE_GOLD_OPA), 0);
            lv_obj_set_style_bg_grad_color(
                l, lv_color_mix(rgb(color::GOLD), base, LINE_GOLD_FADE_OPA), 0);
        }
    } else {
        lv_obj_set_style_bg_opa(l, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(l, rgb(dark ? STAGE_DARK : color::VELVET), 0);
        lv_obj_set_style_bg_opa(l, dark ? STAGE_DARK_OPA : PAGE_LINE_OPA, 0);
        lv_obj_set_style_border_width(l, 1, 0);
        lv_obj_set_style_border_color(l, rgb(color::GOLD), 0);
        lv_obj_set_style_border_opa(l, dark ? DARK_LINE_RIM_OPA : PAGE_LINE_RIM_OPA, 0);
    }
    lv_obj_set_style_radius(l, LINE_CORNER_PX, 0);
    return l;
}

}   // namespace controls
