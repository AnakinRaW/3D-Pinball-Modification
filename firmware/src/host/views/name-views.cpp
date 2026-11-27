#include "name-views.h"
#include <stdio.h>
#include <string.h>
#include "stage-views.h"

namespace {
namespace color = controls::color;

constexpr int32_t HALF = 2;
constexpr int32_t RIM_PX = 2;
constexpr int32_t EDGE_PX = 2;   // the box and Skip from the screen's edges

// the box naming the player, and Skip beside it
constexpr int32_t BOX_WIDTH = 224;
constexpr int32_t BOX_HEIGHT = 46;
constexpr int32_t BOX_CORNER_PX = 8;
constexpr uint32_t BOX_FILL = 0x08040e;
constexpr lv_opa_t BOX_OPA = 242;   // the storyboard's 0.95
constexpr int32_t BOX_TEXT_X = 8;
constexpr int32_t CAPTION_Y = 4;
constexpr int32_t CAPTION_SPACE_PX = 1;
constexpr int32_t SECOND_LINE_Y = 17;
constexpr int32_t SKIP_WIDTH = 88;
constexpr size_t CAPTION_CHARS = 48;

// the names and the keys
constexpr int32_t KEYS_Y = 51;
constexpr int32_t KEY_STEP_Y = 47;
constexpr int32_t NAME_WIDTH = 158;
constexpr int32_t NAME_STEP_X = 162;
constexpr int32_t NAME_TEXT_X = 30;
constexpr int32_t NAME_BULB_X = 10;
constexpr int32_t NAME_BULB_PX = 12;
constexpr int32_t NAME_SPACE_PX = 1;
constexpr int32_t KEY_STEP_X = 46;   // host-screens.md: 44 px keys, 2 px apart
constexpr int32_t ACROSS = 7;        // host-screens.md: 7 keys across
constexpr char FIRST_LETTER = 'A';
constexpr char LAST_LETTER = 'Z';
constexpr int32_t DELETE_KEY = 0;   // KEY's value for delete
constexpr int32_t FIELD_SPACE_PX = 2;
constexpr int32_t CURSOR_WIDTH = 10;
constexpr int32_t CURSOR_HEIGHT = 20;
constexpr int32_t CURSOR_GAP_PX = 3;
constexpr int32_t CURSOR_Y = 19;
constexpr int32_t LENGTH_INSET_PX = 8;
constexpr int32_t LENGTH_Y = 25;
constexpr size_t LENGTH_CHARS = 24;

// the high scores
constexpr int32_t TABLE_X = 8;
constexpr int32_t TABLE_Y = 46;
constexpr int32_t TABLE_STEP_PX = 27;
constexpr int32_t TABLE_STEP_SHORT_PX = 24;   // the table's step with a line of scores below it
constexpr int32_t PLACE_X = 8;
constexpr int32_t FRESH_X = 44;
constexpr int32_t FRESH_PX = 8;
constexpr int32_t HOLDER_X = 58;
constexpr int32_t SCORE_INSET_PX = 8;
constexpr int32_t OTHERS_Y = 168;
constexpr int32_t BUTTONS_Y = 188;
constexpr int32_t GAMES_WIDTH = 120;
constexpr int32_t AGAIN_X = 136;
constexpr int32_t AGAIN_WIDTH = 176;
constexpr size_t AGAIN_CHARS = 32;

lv_obj_t* placed(lv_obj_t* o, int32_t x, int32_t y) {
    lv_obj_set_pos(o, x, y);
    return o;
}

/// Places the box that names the player, the place and the score, and Skip beside it.
///
/// @return  The box, which the caller fills below the caption.
lv_obj_t* nameBox(lv_obj_t* root, const NameFor& who, lv_obj_t** skip) {
    lv_obj_t* box = lv_obj_create(root);
    lv_obj_remove_style_all(box);
    lv_obj_set_pos(box, EDGE_PX, EDGE_PX);
    lv_obj_set_size(box, BOX_WIDTH, BOX_HEIGHT);
    lv_obj_set_clickable(box, false);
    lv_obj_set_scrollable(box, false);
    lv_obj_set_style_radius(box, BOX_CORNER_PX, 0);
    lv_obj_set_style_border_width(box, RIM_PX, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(color::GOLD), 0);
    lv_obj_set_style_bg_color(box, lv_color_hex(BOX_FILL), 0);
    lv_obj_set_style_bg_opa(box, BOX_OPA, 0);
    char digits[View::SCORE_CHARS];
    View::score(who.score, digits);
    char caption[CAPTION_CHARS];
    if (who.place != 0) {
        snprintf(caption, sizeof(caption), "PLAYER %u \xC2\xB7 %s \xC2\xB7 #78ffe6 %s#",
                 unsigned(who.player), placeName(who.place), digits);
    } else {
        snprintf(caption, sizeof(caption), "PLAYER %u \xC2\xB7 #78ffe6 %s#", unsigned(who.player),
                 digits);
    }
    lv_obj_t* c = placed(controls::label(box, caption, &archivo_black_9, color::GOLD),
                         BOX_TEXT_X - RIM_PX, CAPTION_Y - RIM_PX);
    lv_label_set_recolor(c, true);
    lv_obj_set_style_text_letter_space(c, CAPTION_SPACE_PX, 0);
    *skip = controls::button(root, controls::Plaque::VIOLET, "SKIP " LV_SYMBOL_RIGHT, SKIP_WIDTH);
    lv_obj_set_height(*skip, BOX_HEIGHT);
    lv_obj_align(*skip, LV_ALIGN_TOP_RIGHT, -EDGE_PX, EDGE_PX);
    return box;
}

/// Sets the root of a page without rays in the night color.
void night(lv_obj_t* root) {
    lv_obj_set_style_bg_color(root, lv_color_hex(color::NIGHT), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
}
}   // namespace

void ChooseNameView::build(lv_obj_t* parent, const NameFor& who, const char* const* names,
                           uint8_t count, bool lit, Pressed pressed, void* context) {
    lv_obj_t* root = create(parent, true, pressed, context);
    night(root);
    lv_obj_t* skip = nullptr;
    lv_obj_t* box = nameBox(root, who, &skip);
    bind(skip, SKIP);
    placed(controls::label(box, "Who played?", &montserrat_bold_16, color::CREAM),
           BOX_TEXT_X - RIM_PX, SECOND_LINE_Y);
    for (uint8_t i = 0; i < count && i < NAMES_MAX; ++i) {
        const bool first = lit && i == 0;
        lv_obj_t* k = controls::key(root, names[i], NAME_WIDTH, first);
        placed(k, (i % HALF) * NAME_STEP_X, KEYS_Y + (i / HALF) * KEY_STEP_Y);
        lv_obj_t* l = lv_obj_get_child(k, 0);
        lv_obj_set_style_text_font(l, &montserrat_bold_16, 0);
        lv_obj_set_style_text_letter_space(l, NAME_SPACE_PX, 0);
        lv_obj_align(l, LV_ALIGN_LEFT_MID, NAME_TEXT_X - RIM_PX, 0);
        if (first) {
            lv_obj_t* b = controls::bulb(k, controls::Light::LIT);
            lv_obj_set_size(b, NAME_BULB_PX, NAME_BULB_PX);
            lv_obj_align(b, LV_ALIGN_LEFT_MID, NAME_BULB_X - RIM_PX, 0);
        }
        bind(k, NAME, i);
    }
    lv_obj_t* other =
        controls::button(root, controls::Plaque::VIOLET, LV_SYMBOL_KEYBOARD "  OTHER NAME",
                         controls::SCREEN_WIDTH_PX);
    placed(other, 0, KEYS_Y + (NAMES_MAX / HALF) * KEY_STEP_Y);
    bind(other, OTHER);
}

void NameView::build(lv_obj_t* parent, const NameFor& who, Pressed pressed, void* context) {
    lv_obj_t* root = create(parent, true, pressed, context);
    night(root);
    memset(letters, 0, sizeof(letters));
    lv_obj_t* skip = nullptr;
    lv_obj_t* box = nameBox(root, who, &skip);
    bind(skip, SKIP);
    field = placed(controls::label(box, "", &jetbrains_mono_bold_20, color::CYAN),
                   BOX_TEXT_X - RIM_PX, SECOND_LINE_Y - RIM_PX);
    lv_obj_set_style_text_letter_space(field, FIELD_SPACE_PX, 0);
    cursor = lv_obj_create(box);
    lv_obj_remove_style_all(cursor);
    lv_obj_set_size(cursor, CURSOR_WIDTH, CURSOR_HEIGHT);
    lv_obj_set_style_bg_color(cursor, lv_color_hex(color::BULB), 0);
    lv_obj_set_style_bg_opa(cursor, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(cursor, CURSOR_GAP_PX * HALF, 0);
    lv_obj_set_style_shadow_color(cursor, lv_color_hex(color::BULB), 0);
    lv_obj_set_clickable(cursor, false);
    length = controls::label(box, "", &montserrat_semibold_12, color::MUTED);
    lv_obj_align(length, LV_ALIGN_TOP_RIGHT, -LENGTH_INSET_PX + RIM_PX, LENGTH_Y - RIM_PX);

    // The letters run A to Z, seven across, and delete and OK close the last row.
    int32_t i = 0;
    for (char c = FIRST_LETTER; c <= LAST_LETTER; ++c, ++i) {
        const char text[] = {c, '\0'};
        lv_obj_t* k = controls::key(root, text, controls::TARGET_PX, false);
        placed(k, (i % ACROSS) * KEY_STEP_X, KEYS_Y + (i / ACROSS) * KEY_STEP_Y);
        bind(k, KEY, c);
        lv_obj_add_event_cb(k, typed, LV_EVENT_CLICKED, this);
    }
    lv_obj_t* del =
        controls::button(root, controls::Plaque::RED, LV_SYMBOL_BACKSPACE, controls::TARGET_PX);
    placed(del, (i % ACROSS) * KEY_STEP_X, KEYS_Y + (i / ACROSS) * KEY_STEP_Y);
    bind(del, KEY, DELETE_KEY);
    lv_obj_add_event_cb(del, typed, LV_EVENT_CLICKED, this);
    ++i;
    lv_obj_t* ok = controls::button(root, controls::Plaque::GOLD, "OK", controls::TARGET_PX);
    placed(ok, (i % ACROSS) * KEY_STEP_X, KEYS_Y + (i / ACROSS) * KEY_STEP_Y);
    lv_obj_add_event_cb(
        ok,
        [](lv_event_t* e) {
            auto* v = static_cast<NameView*>(lv_event_get_user_data(e));
            v->report(OK, int32_t(strlen(v->letters)));
        },
        LV_EVENT_CLICKED, this);
    showName();
}

// A letter key adds its letter while the name has room, and delete takes the last one off. The
// key's own binding then tells the host.
void NameView::typed(lv_event_t* e) {
    auto* v = static_cast<NameView*>(lv_event_get_user_data(e));
    lv_obj_t* k = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    const auto packed = reinterpret_cast<uintptr_t>(lv_obj_get_user_data(k));
    const auto letter = char(packed >> 8);
    const size_t n = strlen(v->letters);
    if (letter == DELETE_KEY) {
        if (n > 0) {
            v->letters[n - 1] = '\0';
        }
    } else if (n < LETTERS_MAX) {
        v->letters[n] = letter;
    }
    v->showName();
}

void NameView::showName() {
    lv_label_set_text(field, letters);
    lv_point_t size;
    lv_text_get_size(&size, letters, &jetbrains_mono_bold_20, FIELD_SPACE_PX, 0, LV_COORD_MAX,
                     LV_TEXT_FLAG_NONE);
    lv_obj_set_pos(cursor, BOX_TEXT_X - RIM_PX + size.x + CURSOR_GAP_PX, CURSOR_Y - RIM_PX);
    char text[LENGTH_CHARS];
    snprintf(text, sizeof(text), "%u / %u", unsigned(strlen(letters)), unsigned(LETTERS_MAX));
    lv_label_set_text(length, text);
}

void HighScoresView::build(lv_obj_t* parent, const char* game, const Entry* entries, uint8_t count,
                           const char* others, uint8_t seconds, Pressed pressed, void* context) {
    lv_obj_t* root = create(parent, true, pressed, context);
    controls::rays(root, true);
    controls::header(root, game, true);
    const int32_t step = others != nullptr ? TABLE_STEP_SHORT_PX : TABLE_STEP_PX;
    const int32_t width = controls::SCREEN_WIDTH_PX - TABLE_X * HALF;
    for (uint8_t i = 0; i < count && i < PLACES; ++i) {
        const Entry& e = entries[i];
        lv_obj_t* line = controls::scoreLine(root, width, e.fresh, false);
        placed(line, TABLE_X, TABLE_Y + step * i);
        lv_obj_align(
            controls::label(line, placeName(uint8_t(i + 1)), &archivo_black_12, color::GOLD),
            LV_ALIGN_LEFT_MID, PLACE_X - 1, 0);
        if (e.fresh) {
            lv_obj_t* b = controls::bulb(line, controls::Light::LIT);
            lv_obj_set_size(b, FRESH_PX, FRESH_PX);
            lv_obj_align(b, LV_ALIGN_LEFT_MID, FRESH_X - 1, 0);
        }
        lv_obj_align(controls::label(line, e.name, &montserrat_bold_14, color::CREAM),
                     LV_ALIGN_LEFT_MID, HOLDER_X - 1, 0);
        char digits[SCORE_CHARS];
        score(e.score, digits);
        lv_obj_align(controls::label(line, digits, &jetbrains_mono_bold_16, color::CYAN),
                     LV_ALIGN_RIGHT_MID, -SCORE_INSET_PX, 0);
    }
    if (others != nullptr) {
        lv_obj_t* l = controls::label(root, others, &montserrat_semibold_12, color::MUTED);
        lv_obj_set_width(l, width);
        lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
        placed(l, TABLE_X, OTHERS_Y);
    }
    lv_obj_t* games = controls::button(root, controls::Plaque::VIOLET, "GAMES", GAMES_WIDTH);
    placed(games, TABLE_X, BUTTONS_Y);
    bind(games, GAMES);
    again = controls::button(root, controls::Plaque::GOLD, "", AGAIN_WIDTH);
    placed(again, AGAIN_X, BUTTONS_Y);
    lv_label_set_recolor(lv_obj_get_child(again, 0), true);
    bind(again, PLAY_AGAIN);
    setCountdown(seconds);
}

void HighScoresView::setCountdown(uint8_t seconds) {
    if (isOpen()) {
        char text[AGAIN_CHARS];
        snprintf(text, sizeof(text), "PLAY AGAIN  #5e1016 %u#", unsigned(seconds));
        lv_label_set_text(lv_obj_get_child(again, 0), text);
    }
}
