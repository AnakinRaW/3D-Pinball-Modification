#include "game-views.h"

namespace {
namespace color = controls::color;

constexpr int32_t HALF = 2;
constexpr int32_t STATUS_BULB_PX = 16;
constexpr int32_t STATUS_INSET_PX = 6;   // the bulb from the screen's top right corner
constexpr int32_t STATUS_AREA_HEIGHT = 38;

constexpr int32_t CARD_X = 6;   // the card indicator, from the bottom left corner
constexpr int32_t CARD_BOTTOM_PX = 4;
constexpr int32_t CARD_WIDTH = 44;
constexpr int32_t CARD_HEIGHT = 18;
constexpr int32_t CARD_CORNER_PX = 4;
constexpr lv_opa_t CARD_OPA = 217;   // the storyboard's 0.85
constexpr int32_t ICON_X = 3;
constexpr int32_t DOT_X = 18;
constexpr int32_t DOT_STEP_PX = 7;
constexpr int32_t DOT_PX = 5;
constexpr uint8_t DOTS = 3;
constexpr lv_opa_t DOT_OPAS[DOTS] = {LV_OPA_COVER, LV_OPA_50, LV_OPA_20};
constexpr uint32_t DOT_STEP_MS = 300;   // a running dot moves on

constexpr int32_t TOAST_INSET_PX = 4;
constexpr int32_t TOAST_HEIGHT = 46;
constexpr int32_t TOAST_BULB_X = 10;
constexpr int32_t TOAST_TEXT_X = 36;
constexpr int32_t TOAST_TITLE_Y = 6;
constexpr int32_t TOAST_DETAIL_Y = 24;
constexpr int32_t TOAST_WIDTH = controls::SCREEN_WIDTH_PX - TOAST_INSET_PX * HALF;
constexpr int32_t SCALE_WHOLE = 256;   // the shrink's steps

// The dots run by turning their opacities one place on.
void runDots(lv_timer_t* t) {
    auto* card = static_cast<lv_obj_t*>(lv_timer_get_user_data(t));
    static uint8_t phase = 0;
    phase = uint8_t((phase + 1) % DOTS);
    for (uint8_t i = 0; i < DOTS; ++i) {
        lv_obj_set_style_bg_opa(lv_obj_get_child(card, int32_t(i) + 1),
                                DOT_OPAS[(i + DOTS - phase) % DOTS], 0);
    }
}

void stopDots(lv_event_t* e) {
    lv_timer_delete(static_cast<lv_timer_t*>(lv_event_get_user_data(e)));
}

void dropTo(void* o, int32_t y) {
    lv_obj_set_y(static_cast<lv_obj_t*>(o), y);
}

// The toast shrinks toward the status bulb at the screen's top right and fades as it goes. The
// step runs from SCALE_WHOLE, the whole toast, down to 0.
void shrinkTo(void* o, int32_t step) {
    auto* toast = static_cast<lv_obj_t*>(o);
    const int32_t width =
        TOAST_WIDTH - (TOAST_WIDTH - STATUS_BULB_PX) * (SCALE_WHOLE - step) / SCALE_WHOLE;
    const int32_t height =
        TOAST_HEIGHT - (TOAST_HEIGHT - STATUS_BULB_PX) * (SCALE_WHOLE - step) / SCALE_WHOLE;
    lv_obj_set_size(toast, width, height);
    lv_obj_set_x(toast, TOAST_INSET_PX + TOAST_WIDTH - width);
    lv_obj_set_style_opa(toast, step >= SCALE_WHOLE ? lv_opa_t(LV_OPA_COVER) : lv_opa_t(step), 0);
}

void shrunk(lv_anim_t* a) {
    lv_obj_set_hidden(static_cast<lv_obj_t*>(a->var), true);
}
}   // namespace

void StatusView::build(lv_obj_t* parent, Pressed pressed, void* context) {
    lv_obj_t* root = create(parent, false, pressed, context);
    // The status bulb's touch area is a target wide and reaches the screen's top right corner.
    status = lv_obj_create(root);
    lv_obj_remove_style_all(status);
    lv_obj_set_size(status, controls::TARGET_PX, STATUS_AREA_HEIGHT);
    lv_obj_align(status, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_t* b = controls::bulb(status, controls::Light::FAILED);
    lv_obj_set_size(b, STATUS_BULB_PX, STATUS_BULB_PX);
    lv_obj_align(b, LV_ALIGN_TOP_RIGHT, -STATUS_INSET_PX, STATUS_INSET_PX + HALF);
    bind(status, STATUS);

    card = lv_obj_create(root);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, CARD_WIDTH, CARD_HEIGHT);
    lv_obj_align(card, LV_ALIGN_BOTTOM_LEFT, CARD_X, -CARD_BOTTOM_PX);
    lv_obj_set_clickable(card, false);
    lv_obj_set_style_radius(card, CARD_CORNER_PX, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(color::NIGHT), 0);
    lv_obj_set_style_bg_opa(card, CARD_OPA, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(color::GOLD), 0);
    lv_obj_align(controls::label(card, LV_SYMBOL_SD_CARD, &lv_font_montserrat_14, color::GOLD),
                 LV_ALIGN_LEFT_MID, ICON_X, 0);
    for (uint8_t i = 0; i < DOTS; ++i) {
        lv_obj_t* dot = lv_obj_create(card);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, DOT_PX, DOT_PX);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, lv_color_hex(color::CYAN), 0);
        lv_obj_set_style_bg_opa(dot, DOT_OPAS[i], 0);
        lv_obj_align(dot, LV_ALIGN_LEFT_MID, DOT_X - 1 + DOT_STEP_PX * i, 0);
    }
    // The icon is the card's first child, and the dots follow it from index 1.
    lv_timer_t* t = lv_timer_create(runDots, DOT_STEP_MS, card);
    lv_obj_add_event_cb(card, stopDots, LV_EVENT_DELETE, t);
    setFault(false);
    setWriting(false);
}

void StatusView::setFault(bool fault) {
    if (isOpen()) {
        lv_obj_set_hidden(status, !fault);
    }
}

void StatusView::setWriting(bool writing) {
    if (isOpen()) {
        lv_obj_set_hidden(card, !writing);
    }
}

void ToastView::build(lv_obj_t* parent, Pressed pressed, void* context) {
    create(parent, false, pressed, context);
    toast = nullptr;
}

void ToastView::show(bool failed, const char* title, const char* detail, bool at) {
    if (!isOpen()) {
        return;
    }
    if (toast != nullptr) {
        lv_anim_delete(toast, nullptr);
        lv_obj_delete(toast);
    }
    toast = controls::card(root(), failed ? controls::Tone::FAULT : controls::Tone::WORKS, nullptr,
                           TOAST_WIDTH, TOAST_HEIGHT);
    lv_obj_set_x(toast, TOAST_INSET_PX);
    lv_obj_t* b = controls::bulb(toast, failed ? controls::Light::FAILED : controls::Light::WORKS);
    lv_obj_set_size(b, STATUS_BULB_PX, STATUS_BULB_PX);
    lv_obj_align(b, LV_ALIGN_LEFT_MID, TOAST_BULB_X - HALF, 0);
    lv_obj_set_pos(controls::label(toast, title, &archivo_black_12, color::CREAM),
                   TOAST_TEXT_X - HALF, TOAST_TITLE_Y - HALF);
    lv_obj_set_pos(controls::label(toast, detail, &montserrat_semibold_12, color::MUTED),
                   TOAST_TEXT_X - HALF, TOAST_DETAIL_Y - HALF);
    if (failed) {
        bind(toast, OPEN);
    } else {
        lv_obj_set_clickable(toast, false);
    }
    if (at) {
        lv_obj_set_y(toast, TOAST_INSET_PX);
        return;
    }
    lv_anim_t drop;
    lv_anim_init(&drop);
    lv_anim_set_var(&drop, toast);
    lv_anim_set_exec_cb(&drop, dropTo);
    lv_anim_set_values(&drop, -TOAST_HEIGHT, TOAST_INSET_PX);
    lv_anim_set_duration(&drop, DROP_MS);
    lv_anim_set_path_cb(&drop, lv_anim_path_ease_out);
    lv_anim_start(&drop);
    lv_anim_t shrink;
    lv_anim_init(&shrink);
    lv_anim_set_var(&shrink, toast);
    lv_anim_set_exec_cb(&shrink, shrinkTo);
    lv_anim_set_values(&shrink, SCALE_WHOLE, 0);
    lv_anim_set_delay(&shrink, DROP_MS + (failed ? FAULT_MS : WORKS_MS));
    lv_anim_set_duration(&shrink, DROP_MS);
    lv_anim_set_path_cb(&shrink, lv_anim_path_ease_in);
    lv_anim_set_completed_cb(&shrink, shrunk);
    lv_anim_start(&shrink);
}

bool ToastView::isShowing() const {
    return isOpen() && toast != nullptr && !lv_obj_is_hidden(toast);
}
