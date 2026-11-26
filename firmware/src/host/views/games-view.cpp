#include "games-view.h"
#include <initializer_list>

namespace {
namespace color = controls::color;

constexpr uint32_t NEON_TITLE = 0xffd0d4;   // the PINBALL neon of the header
constexpr int32_t CARD_X = 48;              // the middle card
constexpr int32_t CARD_Y = 46;
constexpr int32_t CARD_WIDTH = 224;
constexpr int32_t CARD_HEIGHT = 134;
constexpr int32_t CARD_GAP_PX = 8;   // between two cards of the ring
constexpr int32_t CARD_STEP_PX = CARD_WIDTH + CARD_GAP_PX;
// The ring holds five cards, the middle one and two on each side, so a drag by a whole card still
// brings a card in at the far side. It is as wide as they are, so LVGL redraws every card it moves.
constexpr int32_t RING_SIDE = 2;   // the cards on each side of the middle one
constexpr int32_t RING_CARDS = RING_SIDE * 2 + 1;
constexpr int32_t RING_X = CARD_X - RING_SIDE * CARD_STEP_PX;   // the ring at rest
constexpr int32_t MIDDLE_X = RING_SIDE * CARD_STEP_PX;          // the middle card in the ring
constexpr lv_opa_t NEIGHBOR_OPA = 140;   // a neighbor stands back behind the middle card
constexpr int32_t COVER_HEIGHT = 64;
constexpr int32_t NAME_Y = 68;
constexpr int32_t NAME_OUTLINE_PX = 1;
constexpr int32_t CAPTION_Y = 96;
constexpr int32_t CAPTION_SPACE_PX = 2;
constexpr int32_t SCORE_Y = 106;
constexpr lv_opa_t BROKEN_COVER_OPA = 140;   // the storyboard's 0.55
constexpr int32_t ERROR_BULB_X = 10;
constexpr int32_t ERROR_BULB_Y = 100;
constexpr int32_t ERROR_BULB_PX = 12;
constexpr int32_t ERROR_X = 28;
constexpr int32_t ERROR_AT_Y = 96;
constexpr int32_t ERROR_Y = 110;
constexpr int32_t ERROR_INSET_PX = 8;
constexpr int32_t PAGER_Y = 184;
constexpr int32_t PLAY_Y = 194;
constexpr int32_t PLAY_WIDTH = 140;
constexpr int32_t STATUS_BULB_PX = 16;
constexpr int32_t WHOLE_PERCENT = 100;

lv_obj_t* placed(lv_obj_t* o, int32_t x, int32_t y) {
    lv_obj_set_pos(o, x, y);
    return o;
}

void moveRing(void* ring, int32_t x) {
    lv_obj_set_x(static_cast<lv_obj_t*>(ring), x);
}
}   // namespace

void GamesView::build(lv_obj_t* parent, const Game* list, uint8_t n, uint8_t first, bool fault,
                      Pressed pressed, void* context) {
    lv_obj_t* root = create(parent, true, pressed, context);
    games = list;
    count = n;
    index = first < n ? first : 0;
    turning = 0;
    sliding = false;
    dragged = false;
    middleCover = nullptr;
    controls::rays(root, true);
    // The ring of cards lies under the header, so a neighbor slides in beneath it.
    ring = lv_obj_create(root);
    lv_obj_remove_style_all(ring);
    lv_obj_set_size(ring, RING_CARDS * CARD_STEP_PX, CARD_HEIGHT);
    lv_obj_set_pos(ring, RING_X, CARD_Y);
    lv_obj_set_clickable(ring, false);
    lv_obj_set_scrollable(ring, false);

    lv_obj_t* header = controls::header(root, "PINBALL", true);
    lv_obj_set_style_text_color(lv_obj_get_child(lv_obj_get_child(header, 0), 0),
                                lv_color_hex(NEON_TITLE), 0);
    if (fault) {
        lv_obj_t* status = controls::headerButton(header, true, "");
        lv_obj_t* b = controls::bulb(status, controls::Light::FAILED);
        lv_obj_set_size(b, STATUS_BULB_PX, STATUS_BULB_PX);
        lv_obj_center(b);
        bind(status, STATUS);
    }
    bind(controls::headerButton(header, false, LV_SYMBOL_SETTINGS), SETTINGS);

    pages = controls::pager(root, count, index);
    lv_obj_align(pages, LV_ALIGN_TOP_MID, 0, PAGER_Y);
    play = controls::button(root, controls::Plaque::GOLD, "PLAY", PLAY_WIDTH);
    lv_obj_align(play, LV_ALIGN_TOP_MID, 0, PLAY_Y);
    lv_obj_add_event_cb(
        play,
        [](lv_event_t* e) {
            const auto* v = static_cast<const GamesView*>(lv_event_get_user_data(e));
            v->report(PLAY, v->index);
        },
        LV_EVENT_CLICKED, this);
    place();
}

void GamesView::place() {
    // Cleaning the ring deletes the cover, with the wait and any video in it.
    lv_obj_clean(ring);
    middleCover = nullptr;
    previewed = false;
    lv_obj_set_x(ring, RING_X);
    for (int32_t k = -RING_SIDE; k <= RING_SIDE; ++k) {
        if (k == 0 || count > 1) {
            const int32_t at = (int32_t(index) + k % count + count) % count;
            card(games[at], MIDDLE_X + k * CARD_STEP_PX, k == 0);
        }
    }
    lv_obj_set_state(play, LV_STATE_DISABLED, games[index].broken);
    controls::setPage(pages, index);
    awaitPreview();
}

lv_obj_t* GamesView::preview() const {
    if (!isOpen() || sliding || offset() != 0) {
        return nullptr;
    }
    return middleCover;
}

void GamesView::awaitPreview() {
    if (middleCover == nullptr || previewed || games[index].broken) {
        return;
    }
    // An animation of the cover without a value to set waits for its end, and LVGL deletes it
    // with the cover.
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, middleCover);
    lv_anim_set_duration(&a, PREVIEW_MS);
    lv_anim_set_completed_cb(&a, waited);
    lv_anim_set_user_data(&a, this);
    lv_anim_start(&a);
}

void GamesView::stopAwaiting() {
    if (middleCover != nullptr) {
        lv_anim_delete(middleCover, nullptr);
    }
}

void GamesView::waited(lv_anim_t* a) {
    auto* v = static_cast<GamesView*>(lv_anim_get_user_data(a));
    if (v->previewed || v->preview() == nullptr) {
        return;
    }
    v->previewed = true;
    v->report(PREVIEW, v->index);
}

lv_obj_t* GamesView::card(const Game& g, int32_t x, bool middle) {
    lv_obj_t* c = controls::card(ring, g.broken ? controls::Tone::FAULT : controls::Tone::NOTE,
                                 nullptr, CARD_WIDTH, CARD_HEIGHT);
    lv_obj_set_pos(c, x, 0);
    // Every card takes the touch, so a finger on it drags the ring and a tap on a neighbor turns
    // the ring.
    lv_obj_set_clickable(c, true);
    lv_obj_add_event_cb(c, touched, LV_EVENT_ALL, this);
    if (!middle) {
        lv_obj_set_style_opa(c, NEIGHBOR_OPA, 0);
        lv_obj_add_event_cb(c, tapped, LV_EVENT_CLICKED, this);
    }

    lv_obj_t* cover = lv_obj_create(c);
    lv_obj_remove_style_all(cover);
    lv_obj_set_size(cover, lv_pct(WHOLE_PERCENT), COVER_HEIGHT);
    lv_obj_set_clickable(cover, false);
    lv_obj_set_scrollable(cover, false);
    if (middle) {
        middleCover = cover;
    }
    if (g.cover != nullptr) {
        lv_obj_t* picture = lv_image_create(cover);
        lv_image_set_src(picture, g.cover);
        lv_obj_center(picture);
    } else {
        controls::rays(cover, false);
    }
    if (g.broken) {
        lv_obj_set_style_opa(cover, BROKEN_COVER_OPA, 0);
        lv_obj_t* name = controls::label(c, g.name, &archivo_black_20, color::GRAY);
        lv_obj_align(name, LV_ALIGN_TOP_MID, 0, NAME_Y);
        lv_obj_t* b =
            placed(controls::bulb(c, controls::Light::FAILED), ERROR_BULB_X, ERROR_BULB_Y);
        lv_obj_set_size(b, ERROR_BULB_PX, ERROR_BULB_PX);
        lv_obj_t* at = placed(controls::label(c, g.errorAt, &montserrat_bold_12, color::FAULT_TEXT),
                              ERROR_X, ERROR_AT_Y);
        lv_obj_t* what = placed(controls::label(c, g.error, &montserrat_semibold_12, color::MUTED),
                                ERROR_X, ERROR_Y);
        for (lv_obj_t* l : {at, what}) {
            lv_obj_set_width(l, CARD_WIDTH - ERROR_X - ERROR_INSET_PX);
            lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
        }
    } else {
        lv_obj_align(controls::lettering(c, g.name, &archivo_black_20, NAME_OUTLINE_PX),
                     LV_ALIGN_TOP_MID, 0, NAME_Y - NAME_OUTLINE_PX);
        lv_obj_t* caption = controls::label(c, "HIGH SCORE", &archivo_black_9, color::GOLD);
        lv_obj_set_style_text_letter_space(caption, CAPTION_SPACE_PX, 0);
        lv_obj_align(caption, LV_ALIGN_TOP_MID, 0, CAPTION_Y);
        char digits[SCORE_CHARS];
        score(g.highScore, digits);
        lv_obj_align(controls::label(c, digits, &jetbrains_mono_bold_16, color::CYAN),
                     LV_ALIGN_TOP_MID, 0, SCORE_Y);
    }
    return c;
}

void GamesView::step(bool forward) {
    if (!isOpen() || count < 2 || sliding) {
        return;
    }
    slide(forward ? -CARD_STEP_PX : CARD_STEP_PX, forward ? 1 : -1);
}

void GamesView::drag(int32_t dx) {
    if (!isOpen() || count < 2 || sliding) {
        return;
    }
    stopAwaiting();
    const int32_t x = dx < -CARD_STEP_PX ? -CARD_STEP_PX : dx > CARD_STEP_PX ? CARD_STEP_PX : dx;
    lv_obj_set_x(ring, RING_X + x);
}

int32_t GamesView::offset() const {
    return isOpen() ? lv_obj_get_style_x(ring, LV_PART_MAIN) - RING_X : 0;
}

void GamesView::letGo() {
    if (!isOpen() || count < 2 || sliding) {
        return;
    }
    const int32_t x = offset();
    if (x <= -SNAP_PX) {
        slide(-CARD_STEP_PX, 1);
    } else if (x >= SNAP_PX) {
        slide(CARD_STEP_PX, -1);
    } else {
        slide(0, 0);
    }
}

void GamesView::slide(int32_t to, int8_t turn) {
    stopAwaiting();
    sliding = true;
    turning = turn;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, ring);
    lv_anim_set_exec_cb(&a, moveRing);
    lv_anim_set_values(&a, lv_obj_get_style_x(ring, LV_PART_MAIN), RING_X + to);
    lv_anim_set_duration(&a, SLIDE_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_completed_cb(&a, turned);
    lv_anim_set_user_data(&a, this);
    lv_anim_start(&a);
}

// Once the ring has slid, the card it brought takes the middle and the ring stands again.
void GamesView::turned(lv_anim_t* a) {
    auto* v = static_cast<GamesView*>(lv_anim_get_user_data(a));
    v->sliding = false;
    if (v->turning == 0) {
        // The card slid back, so it waits for its preview again unless it has asked already.
        v->awaitPreview();
        return;
    }
    v->index = uint8_t((v->index + v->count + v->turning) % v->count);
    v->turning = 0;
    v->place();
    v->report(SHOWN, v->index);
}

// A finger on a card drags the ring once it moved DRAG_SLOP_PX, and lets it go when it lifts. A
// finger that dragged makes no tap.
void GamesView::touched(lv_event_t* e) {
    auto* v = static_cast<GamesView*>(lv_event_get_user_data(e));
    lv_point_t at;
    switch (lv_event_get_code(e)) {
    case LV_EVENT_PRESSED:
        lv_indev_get_point(lv_indev_active(), &at);
        v->pressX = at.x;
        v->dragged = false;
        break;
    case LV_EVENT_PRESSING:
        lv_indev_get_point(lv_indev_active(), &at);
        if (at.x - v->pressX >= DRAG_SLOP_PX || v->pressX - at.x >= DRAG_SLOP_PX) {
            v->dragged = true;
        }
        if (v->dragged) {
            v->drag(at.x - v->pressX);
        }
        break;
    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST:
        if (v->dragged) {
            v->letGo();
        }
        break;
    default:
        break;
    }
}

// A tap on a neighbor turns the ring toward it.
void GamesView::tapped(lv_event_t* e) {
    auto* v = static_cast<GamesView*>(lv_event_get_user_data(e));
    if (v->dragged) {
        return;
    }
    lv_obj_t* c = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    v->step(lv_obj_get_x(c) > MIDDLE_X);
}
