// Builds the controls every host screen is made of: the plaque buttons, the header with its marquee
// bulbs, the warning lights, the list rows, the sliders, the cards and the dialogs, the tiles, the
// keys, and the stage of sunburst, curtain, valance and lettering, in the colors and the fonts of
// the host's look. Each control is an ordinary LVGL object that a view places and sizes
// like any other, and the controls share their styles, so a screen costs no style of its own.
//
// Design: docs/firmware/host-screens.md, Look.
#pragma once
#include <lvgl.h>
#include <stdint.h>

LV_FONT_DECLARE(archivo_black_9)
LV_FONT_DECLARE(archivo_black_12)
LV_FONT_DECLARE(archivo_black_13)
LV_FONT_DECLARE(archivo_black_14)
LV_FONT_DECLARE(archivo_black_17)
LV_FONT_DECLARE(archivo_black_20)
LV_FONT_DECLARE(archivo_black_30)
LV_FONT_DECLARE(archivo_black_40)
LV_FONT_DECLARE(jetbrains_mono_bold_12)
LV_FONT_DECLARE(jetbrains_mono_bold_16)
LV_FONT_DECLARE(jetbrains_mono_bold_20)
LV_FONT_DECLARE(montserrat_semibold_12)
LV_FONT_DECLARE(montserrat_semibold_14)
LV_FONT_DECLARE(montserrat_bold_12)
LV_FONT_DECLARE(montserrat_bold_14)
LV_FONT_DECLARE(montserrat_bold_16)
LV_FONT_DECLARE(host_icons_14)

namespace controls {

constexpr int32_t SCREEN_WIDTH_PX = 320;   // the display's pixels across
constexpr int32_t SCREEN_HEIGHT_PX = 240;

constexpr int32_t TARGET_PX = 44;   // host-screens.md, Controls: the least touch target
/// The icons beyond LVGL's symbols, which every font of the screens finds in host_icons_14, in
/// UTF-8 like LVGL's symbols
constexpr char ICON_BULB[] = "\xEF\x83\xAB";    // U+F0EB, the playfield's lamps
constexpr char ICON_SUN[] = "\xEF\x86\x85";     // U+F185, the ambient light
constexpr char ICON_MUTED[] = "\xEF\x9A\xA9";   // U+F6A9, the crossed-out speaker of muted sound

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
constexpr uint32_t ON_GOLD = 0x2a0610;      // text on a gold plaque
constexpr uint32_t GRAY = 0x8a8290;         // the name of a game that cannot start
constexpr uint32_t DIM_TEXT = 0xe8dcc0;     // the name on a tile of a device that works
}   // namespace color

/// What a plaque says: red is the normal button, gold the one the page is for, violet the way out.
enum class Plaque : uint8_t { RED, GOLD, VIOLET };

/// What a warning light shows: a marquee bulb lit or unlit, or a device that works, warns or
/// failed.
enum class Light : uint8_t { LIT, UNLIT, WORKS, WARNING, FAILED };

/// What a slider sets: a sound, filled in ramp blue, or a light, filled in board gold.
enum class Fill : uint8_t { SOUND, LIGHT };

/// What a card or a tile reports: a note in gold, a fault in neon red, a warning in amber, or a
/// device that works in green.
enum class Tone : uint8_t { NOTE, FAULT, WARNING, WORKS };

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
/// @param detail   The line under the title. "" sets the title in the row's middle.
/// @param height   ROW_PX in a list, TARGET_PX in the menu.
/// @param icon     A symbol before the texts, "" for room that the caller fills itself, nullptr
///                 for no room.
/// @param iconRgb  The symbol's color.
/// @return         The row, which sends LV_EVENT_CLICKED when pressed. Its first child is the
///                 line of detail.
lv_obj_t* row(lv_obj_t* parent, const char* title, const char* detail, int32_t width,
              int32_t height = ROW_PX, const char* icon = nullptr, uint32_t iconRgb = color::CREAM);

/// Changes a row's line of detail and its color. "" sets the title in the row's middle.
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

constexpr int32_t STRIP_PX = 30;   // a card's title strip

/// Creates a card: a night plaque with a rim, a shadow and a title strip, in the colors of
/// @p tone. A fault glows neon red and a device that works glows green.
///
/// @param title   The strip's text, nullptr for a card without a strip, such as a toast.
/// @param height  The card's height, LV_SIZE_CONTENT to fit what the caller places in it.
/// @return        The card. With a title, its first child is the strip, and the caller places the
///                rest below it, STRIP_PX from the top.
lv_obj_t* card(lv_obj_t* parent, Tone tone, const char* title, int32_t width, int32_t height);

/// Creates a modal dialog: a card centered on the parent. A backdrop in the night color covers
/// the whole parent behind the card, dims it and takes every touch beside the card, so no control
/// beneath the dialog reacts while it shows.
///
/// @return  The card, as card() gives it.
lv_obj_t* dialog(lv_obj_t* parent, const char* title, int32_t width, int32_t height,
                 Tone tone = Tone::NOTE);

/// Removes a dialog that dialog() created, with its backdrop, once LVGL's current event is done.
///
/// @param dialog  The card dialog() returned.
void closeDialog(lv_obj_t* dialog);

/// Creates a tile: a velvet plaque that takes the touch, rimmed in the color of @p tone. A fault
/// glows neon red. A device that works gets a thin dim rim, so a tile that needs a look stands out.
lv_obj_t* tile(lv_obj_t* parent, Tone tone, int32_t width, int32_t height);

constexpr uint8_t PAGES_MAX = 16;   // the most bulbs a pager holds

/// Creates a row of small marquee bulbs, one per page, with the page shown lit.
///
/// @param pages  How many bulbs, 1 to PAGES_MAX.
lv_obj_t* pager(lv_obj_t* parent, uint8_t pages, uint8_t shown);

/// Lights the bulb of @p shown and puts out the others.
void setPage(lv_obj_t* pager, uint8_t shown);

/// Creates the sunburst of the film's diorama screen across the whole parent: rays of crimson and
/// velvet from a point below the center.
///
/// @param dim  True for the faint rays behind a page, false for the stage of a game's start and
///             end.
lv_obj_t* rays(lv_obj_t* parent, bool dim);

/// Lets every sunburst turn slowly, as the film's rays do, or stand still.
///
/// The sunbursts turn at first. The host sets the player's choice from the saved settings.
void setAnimated(bool on);

/// Gets whether the sunbursts turn.
bool isAnimated();

/// Creates a stage curtain: red folds over the whole parent's height with a row of gold tassels at
/// the foot.
lv_obj_t* curtain(lv_obj_t* parent, int32_t width);

/// Creates the stage's valance across the parent's top: a red band with gold-rimmed scallops
/// hanging below it.
lv_obj_t* valance(lv_obj_t* parent);

/// Creates the gold lettering of the film's MULTIBALL: yellow text with a red outline and a dark
/// shadow below.
///
/// @param text     The text, in capitals.
/// @param outline  The outline's width in pixels.
lv_obj_t* lettering(lv_obj_t* parent, const char* text, const lv_font_t* font, int32_t outline);

constexpr int32_t LEVEL_PX = 64;   // a level's name and its slider

/// Creates a level: its name and its value in percent above a slider.
///
/// The value follows the slider. Every change sends LV_EVENT_VALUE_CHANGED from the slider's
/// track, which levelTrack() gets.
///
/// @param value  The value at first, 0 to SLIDER_MAX.
/// @param width  The level's width, the slider's with its minus and plus.
/// @return       The level, LEVEL_PX high.
lv_obj_t* level(lv_obj_t* parent, const char* name, Fill fill, int32_t value, int32_t width);

/// Gets the track of a level that level() created, the object that holds its value.
lv_obj_t* levelTrack(lv_obj_t* level);

/// Sets a level's value without telling the listeners.
void setLevel(lv_obj_t* level, int32_t value);

/// Creates a list that a finger scrolls up and down, with no scroll bar. A night fade with a gold
/// arrow at its top and at its foot shows that more lies beyond, each while something lies beyond
/// that edge.
///
/// @param y       The list's top in @p parent.
/// @param height  The list's height, down to the foot of @p parent for the rest of a page.
/// @return        The list, which stacks what the caller adds to it, LIST_GAP_PX apart.
lv_obj_t* scrollList(lv_obj_t* parent, int32_t y, int32_t height);

constexpr int32_t LIST_GAP_PX = 4;   // between two lines of a list

/// Creates a progress bar: the slider's track with its fill in ramp blue, 0 to SLIDER_MAX.
lv_obj_t* progress(lv_obj_t* parent, int32_t width);

/// Creates a switch row: its title at the left and a switch at the right on a velvet plaque. A tap
/// anywhere on the row turns the switch on or off and sends LV_EVENT_VALUE_CHANGED.
lv_obj_t* toggle(lv_obj_t* parent, const char* title, bool on, int32_t width);

/// Gets whether a switch row is on.
bool isOn(lv_obj_t* toggle);

/// Turns a switch row on or off without telling the listeners.
void setOn(lv_obj_t* toggle, bool on);

/// Creates a key: a velvet plaque with a thin gold rim and its text in Archivo Black, which sinks
/// to the shadow red when pressed.
///
/// @param lit  True for the key of a choice made before, rimmed and lit in gold.
/// @return     The key, TARGET_PX high, which sends LV_EVENT_CLICKED when pressed. Its only child
///             is its label.
lv_obj_t* key(lv_obj_t* parent, const char* text, int32_t width, bool lit);

constexpr int32_t SCORE_LINE_PX = 26;   // a line of a list of scores

/// Creates a line of a list of scores: a plaque SCORE_LINE_PX high, lit in gold for a score that
/// made the list.
///
/// @param dark  True gives an unlit line the near-black of the stage, false the velvet of a page.
lv_obj_t* scoreLine(lv_obj_t* parent, int32_t width, bool lit, bool dark);

}   // namespace controls
