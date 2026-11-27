#include "stage-views.h"
#include <initializer_list>
#include <utility>
#include <stdio.h>

namespace {
using controls::SCREEN_WIDTH_PX;
namespace color = controls::color;

constexpr uint32_t BOX_FILL = 0x08040e;   // the dark of a score box on the stage
constexpr lv_opa_t BOX_OPA = 230;         // the storyboard's 0.9
constexpr int32_t BOX_CORNER_PX = 8;
constexpr int32_t RIM_PX = 2;
constexpr int32_t CAPTION_SPACE_PX = 2;   // the letters of a caption such as HIGH SCORE

// the launch card
constexpr int32_t LAUNCH_CURTAIN_PX = 34;
constexpr int32_t NOW_PLAYING_Y = 58;
constexpr int32_t NOW_PLAYING_SPACE_PX = 3;
constexpr int32_t NAME_Y = 74;
constexpr int32_t NAME_OUTLINE_PX = 2;
constexpr int32_t SCORE_BOX_X = 70;
constexpr int32_t SCORE_BOX_Y = 136;
constexpr int32_t SCORE_BOX_WIDTH = 180;
constexpr int32_t SCORE_BOX_HEIGHT = 40;
constexpr int32_t SCORE_CAPTION_Y = 3;
constexpr int32_t SCORE_Y = 13;
constexpr int32_t SWIPE_Y = 200;
constexpr int32_t SWIPE_SPACE_PX = 1;
constexpr int32_t SWIPE_INSET_PX = 12;   // the swipe line's room from each curtain

// the curtain
constexpr int32_t HALF_CURTAIN_PX = 162;   // each half, overlapping the other by OVERLAP_PX
constexpr int32_t OVERLAP_PX = 4;
constexpr int32_t PLAQUE_WIDTH = 128;
constexpr int32_t PLAQUE_HEIGHT = 48;
constexpr int32_t PLAQUE_BULB_GAP_PX = 4;   // a lit bulb beside each end of the plaque
constexpr int32_t PLAQUE_BULB_PX = 8;
constexpr uint32_t PLAQUE_TOP = 0xf6dc72;
constexpr uint32_t PLAQUE_BOTTOM = 0xb39344;
constexpr int32_t FOLD_SHADOW_PX = 6;
constexpr lv_opa_t FOLD_SHADOW_OPA = 153;   // the storyboard's 0.6

// Game over
constexpr int32_t OVER_CURTAIN_PX = 26;
constexpr int32_t OVER_Y = 40;     // below the valance's scallops
constexpr int32_t BANNER_Y = 82;   // NEW HIGH SCORE, below the title
constexpr int32_t LINES_X = 30;
constexpr int32_t LINES_WIDTH = 260;
constexpr int32_t FIRST_LINE_Y = 106;
constexpr int32_t LINE_STEP_PX = 28;
constexpr int32_t RANK_X = 14;   // the rank, or WINNER on the first line
constexpr int32_t WINNER_X = 6;
constexpr int32_t PLAYER_X = 84;   // right of WINNER
constexpr int32_t SCORE_X = 110;
constexpr int32_t BADGE_INSET_PX = 6;
constexpr char TOP_TAG[] = "TOP 5";   // a score that made the game's list of five
constexpr int32_t BADGE_PAD_PX = 6;
constexpr int32_t LINE_BADGE_PAD_PX = 4;   // TOP 5 in a line, which leaves a long score room
constexpr int32_t BADGE_CORNER_PX = 4;
constexpr int32_t SINGLE_BOX_Y = 114;   // a single player's score
constexpr int32_t SINGLE_BADGE_Y = 162;
constexpr int32_t FOOTER_Y = 218;

/// Places a label centered across the parent at @p y.
lv_obj_t* centered(lv_obj_t* parent, const char* text, const lv_font_t* font, uint32_t rgb,
                   int32_t y) {
    lv_obj_t* l = controls::label(parent, text, font, rgb);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, y);
    return l;
}

/// Places the open stage: the bright rays, a curtain of @p curtainPx at each side and the valance.
void stage(lv_obj_t* root, int32_t curtainPx) {
    controls::rays(root, false);
    controls::curtain(root, curtainPx);
    lv_obj_set_x(controls::curtain(root, curtainPx), SCREEN_WIDTH_PX - curtainPx);
    controls::valance(root);
}

/// Creates a gold tag with @p text, such as WINNER, set in @p font with @p padPx at each side.
lv_obj_t* tag(lv_obj_t* parent, const char* text, const lv_font_t* font = &archivo_black_12,
              int32_t padPx = BADGE_PAD_PX) {
    lv_obj_t* t = controls::label(parent, text, font, color::ON_GOLD);
    lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(t, lv_color_hex(color::GOLD), 0);
    lv_obj_set_style_radius(t, BADGE_CORNER_PX, 0);
    lv_obj_set_style_pad_hor(t, padPx, 0);
    return t;
}

/// Creates the box of a score on the stage, with its caption above the digits.
lv_obj_t* scoreBox(lv_obj_t* parent, const char* caption, uint32_t value) {
    lv_obj_t* box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, SCORE_BOX_WIDTH, SCORE_BOX_HEIGHT);
    lv_obj_set_style_bg_color(box, lv_color_hex(BOX_FILL), 0);
    lv_obj_set_style_bg_opa(box, BOX_OPA, 0);
    lv_obj_set_style_border_width(box, RIM_PX, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(color::GOLD), 0);
    lv_obj_set_style_radius(box, BOX_CORNER_PX, 0);
    lv_obj_set_clickable(box, false);
    lv_obj_t* c = centered(box, caption, &archivo_black_9, color::GOLD, SCORE_CAPTION_Y);
    lv_obj_set_style_text_letter_space(c, CAPTION_SPACE_PX, 0);
    char digits[View::SCORE_CHARS];
    View::score(value, digits);
    centered(box, digits, &jetbrains_mono_bold_20, color::CYAN, SCORE_Y);
    return box;
}

void slide(void* object, int32_t x) {
    lv_obj_set_x(static_cast<lv_obj_t*>(object), x);
}
}   // namespace

const char* placeName(uint8_t place) {
    static const char* const NAMES[] = {"", "1ST", "2ND", "3RD", "4TH", "5TH"};
    return place < sizeof(NAMES) / sizeof(NAMES[0]) ? NAMES[place] : "";
}

void LaunchCardView::build(lv_obj_t* parent, const char* name, uint32_t highScore) {
    lv_obj_t* root = create(parent, true, nullptr, nullptr);
    stage(root, LAUNCH_CURTAIN_PX);
    lv_obj_t* now = centered(root, "NOW PLAYING", &archivo_black_12, color::GOLD, NOW_PLAYING_Y);
    lv_obj_set_style_text_letter_space(now, NOW_PLAYING_SPACE_PX, 0);
    lv_obj_align(controls::lettering(root, name, &archivo_black_40, NAME_OUTLINE_PX),
                 LV_ALIGN_TOP_MID, 0, NAME_Y);

    lv_obj_set_pos(scoreBox(root, "HIGH SCORE", highScore), SCORE_BOX_X, SCORE_BOX_Y);

    // The line stays between the curtains, in two lines where one does not fit.
    lv_obj_t* swipe =
        controls::label(root, "MENU: SWIPE IN FROM THE RIGHT", &archivo_black_9, color::MUTED);
    lv_obj_set_style_text_letter_space(swipe, SWIPE_SPACE_PX, 0);
    lv_obj_set_width(swipe, SCREEN_WIDTH_PX - (LAUNCH_CURTAIN_PX + SWIPE_INSET_PX) * 2);
    lv_label_set_long_mode(swipe, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_align(swipe, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(swipe, LV_ALIGN_TOP_MID, 0, SWIPE_Y);
}

void CurtainView::build(lv_obj_t* parent) {
    lv_obj_t* root = create(parent, false, nullptr, nullptr);
    left = controls::curtain(root, HALF_CURTAIN_PX);
    right = controls::curtain(root, HALF_CURTAIN_PX);
    for (lv_obj_t* c : {left, right}) {
        lv_obj_set_style_shadow_width(c, FOLD_SHADOW_PX, 0);
        lv_obj_set_style_shadow_color(c, lv_color_hex(0), 0);
        lv_obj_set_style_shadow_opa(c, FOLD_SHADOW_OPA, 0);
    }
    controls::valance(root);

    // The plaque and its two bulbs stand in one row centered on the screen.
    plaque = lv_obj_create(root);
    lv_obj_remove_style_all(plaque);
    lv_obj_set_size(plaque, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(plaque, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(plaque, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(plaque, PLAQUE_BULB_GAP_PX, 0);
    lv_obj_set_clickable(plaque, false);
    lv_obj_center(plaque);
    lv_obj_t* first = controls::bulb(plaque, controls::Light::LIT);
    lv_obj_t* board = lv_obj_create(plaque);
    lv_obj_remove_style_all(board);
    lv_obj_set_size(board, PLAQUE_WIDTH, PLAQUE_HEIGHT);
    lv_obj_set_style_radius(board, BOX_CORNER_PX, 0);
    lv_obj_set_style_border_width(board, RIM_PX, 0);
    lv_obj_set_style_border_color(board, lv_color_hex(color::BULB), 0);
    lv_obj_set_style_bg_opa(board, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(board, lv_color_hex(PLAQUE_TOP), 0);
    lv_obj_set_style_bg_grad_color(board, lv_color_hex(PLAQUE_BOTTOM), 0);
    lv_obj_set_style_bg_grad_dir(board, LV_GRAD_DIR_VER, 0);
    lv_obj_set_clickable(board, false);
    lv_obj_center(controls::label(board, "", &archivo_black_17, color::SHADOW));
    lv_obj_t* second = controls::bulb(plaque, controls::Light::LIT);
    for (lv_obj_t* b : {first, second}) {
        lv_obj_set_size(b, PLAQUE_BULB_PX, PLAQUE_BULB_PX);
    }
    lv_obj_set_hidden(plaque, true);
    closed = true;
    move(false, 0);
}

void CurtainView::close(const char* where, bool at) {
    lv_obj_t* name = lv_obj_get_child(lv_obj_get_child(plaque, 1), 0);
    lv_label_set_text(name, where);
    lv_obj_set_hidden(plaque, false);
    move(true, at ? 0 : CLOSE_MS);
}

void CurtainView::open() {
    lv_obj_set_hidden(plaque, true);
    move(false, OPEN_MS);
}

void CurtainView::move(bool closing, uint32_t ms) {
    closed = closing;
    const int32_t leftTo = closing ? 0 : -HALF_CURTAIN_PX;
    const int32_t rightTo = closing ? HALF_CURTAIN_PX - OVERLAP_PX : SCREEN_WIDTH_PX;
    for (const auto& [o, to] : {std::pair<lv_obj_t*, int32_t>{left, leftTo}, {right, rightTo}}) {
        lv_anim_delete(o, slide);
        if (ms == 0) {
            lv_obj_set_x(o, to);
            continue;
        }
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, o);
        lv_anim_set_exec_cb(&a, slide);
        lv_anim_set_values(&a, lv_obj_get_x(o), to);
        lv_anim_set_duration(&a, ms);
        lv_anim_set_path_cb(&a, closing ? lv_anim_path_ease_out : lv_anim_path_ease_in);
        lv_anim_start(&a);
    }
}

void GameOverView::build(lv_obj_t* parent, const Player* players, uint8_t count, Pressed pressed,
                         void* context) {
    lv_obj_t* root = create(parent, true, pressed, context);
    bind(root, GO_ON);
    stage(root, OVER_CURTAIN_PX);
    lv_obj_align(controls::lettering(root, "GAME OVER", &archivo_black_30, NAME_OUTLINE_PX),
                 LV_ALIGN_TOP_MID, 0, OVER_Y);
    const uint8_t n = count < PLAYERS_MAX ? count : PLAYERS_MAX;
    bool best = false;
    for (uint8_t i = 0; i < n; ++i) {
        best = best || players[i].place == 1;
    }
    if (best) {
        lv_obj_align(tag(root, "NEW HIGH SCORE"), LV_ALIGN_TOP_MID, 0, BANNER_Y);
    }
    if (n == 1) {
        // A single player sees the score large, marked where it made the game's list below the
        // best score, which the banner already names.
        lv_obj_align(scoreBox(root, "YOUR SCORE", players[0].score), LV_ALIGN_TOP_MID, 0,
                     SINGLE_BOX_Y);
        if (players[0].place > 1) {
            lv_obj_align(tag(root, TOP_TAG), LV_ALIGN_TOP_MID, 0, SINGLE_BADGE_Y);
        }
        centered(root, "Tap to enter your name", &montserrat_semibold_12, color::MUTED, FOOTER_Y);
        return;
    }
    // Several players stand by their scores, the winner first and lit.
    uint8_t order[PLAYERS_MAX] = {};
    for (uint8_t i = 0; i < n; ++i) {
        order[i] = i;
    }
    for (uint8_t i = 1; i < n; ++i) {
        for (uint8_t k = i; k > 0 && players[order[k]].score > players[order[k - 1]].score; --k) {
            const uint8_t t = order[k];
            order[k] = order[k - 1];
            order[k - 1] = t;
        }
    }
    for (uint8_t rank = 0; rank < n; ++rank) {
        const Player& p = players[order[rank]];
        const bool winner = rank == 0;
        lv_obj_t* line = controls::scoreLine(root, LINES_WIDTH, winner, true);
        lv_obj_set_pos(line, LINES_X, FIRST_LINE_Y + LINE_STEP_PX * rank);
        if (winner) {
            lv_obj_align(tag(line, "WINNER"), LV_ALIGN_LEFT_MID, WINNER_X - 1, 0);
        } else {
            char number[] = "1";
            number[0] = char('1' + rank);
            lv_obj_align(controls::label(line, number, &archivo_black_13, color::GOLD),
                         LV_ALIGN_LEFT_MID, RANK_X - 1, 0);
        }
        char player[] = "P1";
        player[1] = char('1' + order[rank]);
        lv_obj_align(
            controls::label(line, player, &archivo_black_13, winner ? color::CREAM : color::GOLD),
            LV_ALIGN_LEFT_MID, PLAYER_X - 1, 0);
        char digits[SCORE_CHARS];
        score(p.score, digits);
        lv_obj_align(controls::label(line, digits, &jetbrains_mono_bold_16, color::CYAN),
                     LV_ALIGN_LEFT_MID, SCORE_X - 1, 0);
        // The tag names no place in the list, which would stand beside the place in this game.
        if (p.place != 0) {
            lv_obj_align(tag(line, TOP_TAG, &archivo_black_9, LINE_BADGE_PAD_PX),
                         LV_ALIGN_RIGHT_MID, -BADGE_INSET_PX, 0);
        }
    }
    centered(root, "Tap to enter your names", &montserrat_semibold_12, color::MUTED, FOOTER_Y);
}
