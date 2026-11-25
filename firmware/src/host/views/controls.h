// Builds the controls every host screen is made of: the plaque buttons, the header with its marquee
// bulbs, the warning lights, the list rows, the sliders and the dialogs, in the colors and the
// fonts of the host's look. Each control is an ordinary LVGL object that a view places and sizes
// like any other, and the controls share their styles, so a screen costs no style of its own.
//
// Design: docs/firmware/host-screens.md, Look.
#pragma once
#include <lvgl.h>
#include <stdint.h>

LV_FONT_DECLARE(archivo_black_13)
LV_FONT_DECLARE(archivo_black_14)
LV_FONT_DECLARE(archivo_black_17)
LV_FONT_DECLARE(archivo_black_20)
LV_FONT_DECLARE(jetbrains_mono_bold_12)
LV_FONT_DECLARE(jetbrains_mono_bold_16)
LV_FONT_DECLARE(montserrat_semibold_12)
LV_FONT_DECLARE(montserrat_semibold_14)

namespace controls {

constexpr int32_t TARGET_PX = 44;     // host-screens.md, Controls: the least touch target
constexpr int32_t GAP_PX = 4;         // host-screens.md, Controls: between two buttons
constexpr int32_t HEADER_PX = 34;     // host-screens.md, Controls: the header's band
constexpr int32_t BULBS_PX = 6;       // the strip of marquee bulbs under the band
constexpr int32_t ROW_PX = 48;        // a list row's height, TARGET_PX in the menu
constexpr int32_t STEP_PERCENT = 5;   // host-screens.md: step_slider, a step of minus or plus
constexpr int32_t SLIDER_MAX = 100;   // a slider runs from 0 to 100 %

/// The colors of host-screens.md's table, as 0xRRGGBB.
namespace color {
constexpr uint32_t NIGHT = 0x0e061a;
constexpr uint32_t VELVET = 0x2c0a46;
constexpr uint32_t RAY = 0x781228;
constexpr uint32_t FRAME = 0xc8242f;
constexpr uint32_t CABINET = 0xb3242d;
constexpr uint32_t SHADOW = 0x5e1016;
constexpr uint32_t GOLD = 0xe8c04a;
constexpr uint32_t BULB = 0xffd65a;
constexpr uint32_t BULB_OFF = 0x5a3c1e;
constexpr uint32_t CREAM = 0xfff4d6;
constexpr uint32_t CYAN = 0x78ffe6;
constexpr uint32_t RAMP = 0x3d82cf;
constexpr uint32_t BOARD = 0xe2c76b;
constexpr uint32_t GREEN = 0x3fa66b;
constexpr uint32_t NEON = 0xd63a48;
constexpr uint32_t AMBER = 0xff9a2e;
constexpr uint32_t MUTED = 0xd9c9a8;        // the storyboard's second line of text
constexpr uint32_t FAULT_TEXT = 0xffb0b6;   // the storyboard's line of a fault
}   // namespace color

/// What a plaque says: red is the normal button, gold the one the page is for, violet the way out.
enum class Plaque : uint8_t { RED, GOLD, VIOLET };

/// What a warning light shows: a marquee bulb lit or unlit, or a device that works, warns or
/// failed.
enum class Light : uint8_t { LIT, UNLIT, WORKS, WARNING, FAILED };

/// What a slider sets: a sound, filled in ramp blue, or a light, filled in board gold.
enum class Fill : uint8_t { SOUND, LIGHT };

/// Creates a label in a font and a color.
lv_obj_t* label(lv_obj_t* parent, const char* text, const lv_font_t* font, uint32_t rgb);

/// Creates a plaque button 44 px high with its text in Archivo Black.
///
/// A pressed plaque sinks to the shadow red. A disabled one, set with lv_obj_set_state() and
/// LV_STATE_DISABLED, turns gray and takes no touch.
///
/// @param text   The button's text, written as given, such as "END GAME".
/// @param width  The button's width in pixels.
/// @return       The button. Its only child is its label.
lv_obj_t* button(lv_obj_t* parent, Plaque plaque, const char* text, int32_t width);

/// Creates the header: the frame's red band across @p parent's width with its title in
/// Archivo Black, over a strip of marquee bulbs, lit and unlit in turn.
///
/// @param centered  True centers the title. False sets it at the left, as the menu has it.
/// @return          The header, HEADER_PX plus BULBS_PX high.
lv_obj_t* header(lv_obj_t* parent, const char* title, bool centered);

/// Creates a button of the header at its left or right end, 44 px wide, with a symbol in gold.
///
/// @param symbol  The symbol, such as LV_SYMBOL_LEFT for back.
/// @return        The button, which sends LV_EVENT_CLICKED when pressed.
lv_obj_t* headerButton(lv_obj_t* header, bool left, const char* symbol);

/// Creates a warning light: a marquee bulb 14 px across in the color of @p light.
lv_obj_t* bulb(lv_obj_t* parent, Light light);

/// Changes what a warning light shows.
void setLight(lv_obj_t* bulb, Light light);

/// Creates a list row: a title, a line of detail and a chevron, on a velvet plaque.
///
/// @param height   ROW_PX in a list, TARGET_PX in the menu.
/// @param icon     A symbol before the texts, "" for room that the caller fills itself, nullptr
///                 for no room.
/// @param iconRgb  The symbol's color.
/// @return         The row, which sends LV_EVENT_CLICKED when pressed. Its first child is the
///                 line of detail.
lv_obj_t* row(lv_obj_t* parent, const char* title, const char* detail, int32_t width,
              int32_t height = ROW_PX, const char* icon = nullptr, uint32_t iconRgb = color::CREAM);

/// Changes a row's line of detail and its color.
void setDetail(lv_obj_t* row, const char* detail, uint32_t rgb);

/// Creates a slider TARGET_PX high: a minus, the track with its steel ball and a plus.
///
/// The value runs from 0 to SLIDER_MAX. Minus and plus move it by STEP_PERCENT. Every change,
/// by a finger on the track or by minus and plus, sends LV_EVENT_VALUE_CHANGED from the slider's
/// track, which sliderTrack() gets.
///
/// @return  The slider's frame, holding the minus, the track and the plus.
lv_obj_t* slider(lv_obj_t* parent, Fill fill, int32_t width);

/// Gets the track of a slider that slider() created, the object that holds its value.
lv_obj_t* sliderTrack(lv_obj_t* slider);

/// Creates a modal dialog: a night card with a gold rim, a shadow and the red title strip,
/// centered on the parent. A dark backdrop covers the whole parent behind the card, dims it to
/// half and takes every touch beside the card, so no control beneath the dialog reacts while it
/// shows.
///
/// @return  The card. Its first child is the strip, and the caller places the rest below it.
lv_obj_t* dialog(lv_obj_t* parent, const char* title, int32_t width, int32_t height);

/// Removes a dialog that dialog() created, with its backdrop, once LVGL's current event is done.
///
/// @param dialog  The card dialog() returned.
void closeDialog(lv_obj_t* dialog);

}   // namespace controls
