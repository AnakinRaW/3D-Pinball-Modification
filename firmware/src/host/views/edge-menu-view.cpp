#include "edge-menu-view.h"
#include "host/views/controls.h"
#include "logging/debug-log.h"
#include <initializer_list>

using controls::color::CREAM;
using controls::color::FAULT_TEXT;
using controls::color::GOLD;
using controls::color::MUTED;
using controls::color::NIGHT;
using controls::color::SHADOW;
constexpr int32_t RIM_PX = 2;          // host-screens.md, Controls: a button's gold rim
constexpr int32_t CORNER_PX = 8;       // host-screens.md, Controls: a button's corners
constexpr int32_t INSET_PX = 6;        // the rows' distance from the drawer's sides
constexpr int32_t TAB_WIDTH_PX = 16;   // the gold tab of the hint
constexpr int32_t TAB_HEIGHT_PX = 56;
constexpr int32_t BULB_X_PX = 12;   // the Machine row's bulb from its left side
constexpr int32_t HALF = 2;

static int32_t closedX() {
    return EdgeMenuView::SCREEN_WIDTH_PX;
}

static int32_t openX() {
    return EdgeMenuView::SCREEN_WIDTH_PX - EdgeMenuView::WIDTH_PX;
}

static void setX(void* obj, int32_t x) {
    lv_obj_set_x(static_cast<lv_obj_t*>(obj), x);
}

static void setOpacity(void* obj, int32_t opa) {
    lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), lv_opa_t(opa), 0);
}

// The hint fades in and out through two functions, so a fade-out waiting to start leaves a fade-in
// still running alone.
static void fadeIn(void* obj, int32_t opa) {
    setOpacity(obj, opa);
}

static void fadeOut(void* obj, int32_t opa) {
    setOpacity(obj, opa);
}

void EdgeMenuView::build(lv_obj_t* parent, Chosen chosenFn, void* contextArg) {
    if (drawer != nullptr) {
        DEBUG_LOG("EdgeMenuView::build: built already\n");
        return;
    }
    chosen = chosenFn;
    context = contextArg;

    // The drawer takes every touch on it, so a tap on the menu reaches no game.
    drawer = lv_obj_create(parent);
    lv_obj_remove_style_all(drawer);
    lv_obj_set_size(drawer, WIDTH_PX, SCREEN_HEIGHT_PX);
    lv_obj_set_pos(drawer, closedX(), 0);
    lv_obj_set_scrollable(drawer, false);
    lv_obj_set_style_bg_color(drawer, lv_color_hex(NIGHT), 0);
    lv_obj_set_style_bg_opa(drawer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(drawer, lv_color_hex(GOLD), 0);
    lv_obj_set_style_border_width(drawer, RIM_PX, 0);
    lv_obj_set_style_border_side(drawer, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_opa(drawer, LV_OPA_TRANSP, 0);

    lv_obj_t* header = controls::header(drawer, "MENU", false);
    lv_obj_t* close = controls::headerButton(header, false, LV_SYMBOL_RIGHT LV_SYMBOL_RIGHT);
    choose(close, Item::CLOSE);

    lv_obj_t* sound = row(LV_SYMBOL_VOLUME_MAX, controls::color::RAMP, "Sound", Item::SOUND);
    lv_obj_t* lights = row(LV_SYMBOL_CHARGE, controls::color::BOARD, "Lights", Item::LIGHTS);
    lv_obj_t* status = row("", CREAM, "Status", Item::STATUS);
    soundRow = sound;
    lightsRow = lights;
    statusRow = status;
    statusBulb = controls::bulb(status, controls::Light::WORKS);
    lv_obj_align(statusBulb, LV_ALIGN_LEFT_MID, BULB_X_PX, 0);

    lv_obj_t* end = controls::button(drawer, controls::Plaque::RED, LV_SYMBOL_LEFT "  END GAME",
                                     WIDTH_PX - INSET_PX * HALF - RIM_PX);
    lv_obj_align(end, LV_ALIGN_BOTTOM_MID, RIM_PX / HALF, -controls::GAP_PX);
    choose(end, Item::END_GAME);

    tab = lv_obj_create(parent);
    lv_obj_remove_style_all(tab);
    lv_obj_set_size(tab, TAB_WIDTH_PX, TAB_HEIGHT_PX);
    lv_obj_align(tab, LV_ALIGN_RIGHT_MID, -PEEK_PX, 0);
    lv_obj_set_style_bg_color(tab, lv_color_hex(GOLD), 0);
    lv_obj_set_style_bg_opa(tab, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(tab, CORNER_PX, 0);
    lv_obj_set_style_opa(tab, LV_OPA_TRANSP, 0);
    lv_obj_set_clickable(tab, false);
    lv_obj_center(controls::label(tab, LV_SYMBOL_LEFT, &lv_font_montserrat_14, SHADOW));
}

lv_obj_t* EdgeMenuView::row(const char* symbol, uint32_t rgb, const char* name, Item item) {
    // The rows stand below the header, a touch target and a gap apart.
    const int32_t first = controls::HEADER_PX + controls::BULBS_PX + controls::GAP_PX * HALF;
    const int32_t at = first + int32_t(item) * (controls::TARGET_PX + controls::GAP_PX);
    lv_obj_t* r = controls::row(drawer, name, "", WIDTH_PX - INSET_PX * HALF - RIM_PX,
                                controls::TARGET_PX, symbol, rgb);
    lv_obj_set_pos(r, INSET_PX, at);
    choose(r, item);
    return r;
}

void EdgeMenuView::choose(lv_obj_t* control, Item item) {
    lv_obj_set_user_data(control, reinterpret_cast<void*>(uintptr_t(item)));
    lv_obj_add_event_cb(control, clicked, LV_EVENT_CLICKED, this);
}

void EdgeMenuView::show(const Status& status) {
    if (drawer == nullptr) {
        DEBUG_LOG("EdgeMenuView::show: no build() yet\n");
        return;
    }
    controls::setDetail(soundRow, status.sound, MUTED);
    controls::setDetail(lightsRow, status.lights, MUTED);
    controls::setDetail(statusRow, status.devices, status.fault ? FAULT_TEXT : MUTED);
    controls::setLight(statusBulb, status.fault ? controls::Light::FAILED : controls::Light::WORKS);
}

void EdgeMenuView::hint() {
    if (drawer == nullptr || open) {
        return;
    }
    lv_anim_delete(drawer, setX);
    // A hint from out of sight starts transparent, one still fading goes on from where it stands.
    if (menuLeft() >= closedX()) {
        lv_obj_set_style_opa(drawer, LV_OPA_TRANSP, 0);
        lv_obj_set_style_opa(tab, LV_OPA_TRANSP, 0);
    }
    lv_obj_set_x(drawer, closedX() - PEEK_PX);
    stopFades();
    fade(fadeIn, LV_OPA_COVER, 0, HINT_IN_MS);
}

void EdgeMenuView::releaseHint() {
    if (drawer == nullptr || open) {
        return;
    }
    // The fade-in goes on while the hint stands, so the hint stands at full strength.
    fade(fadeOut, LV_OPA_TRANSP, HINT_STAY_MS, HINT_FADE_MS);
}

void EdgeMenuView::follow(uint16_t x) {
    if (drawer == nullptr || open) {
        return;
    }
    stopFades();
    lv_obj_set_style_opa(drawer, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(tab, LV_OPA_TRANSP, 0);
    const int32_t left = int32_t(x) < openX() ? openX() : int32_t(x);
    moveTo(left > closedX() ? closedX() : left, false);
}

bool EdgeMenuView::letGo(uint16_t x) {
    if (drawer == nullptr) {
        return false;
    }
    open = int32_t(x) < closedX() - WIDTH_PX / HALF;
    stopFades();
    lv_obj_set_style_opa(drawer, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(tab, LV_OPA_TRANSP, 0);
    moveTo(open ? openX() : closedX(), true);
    return open;
}

void EdgeMenuView::raise() {
    if (drawer != nullptr) {
        lv_obj_move_foreground(tab);
        lv_obj_move_foreground(drawer);
    }
}

void EdgeMenuView::close() {
    if (!open) {
        return;
    }
    open = false;
    moveTo(closedX(), true);
}

int32_t EdgeMenuView::menuLeft() const {
    return drawer == nullptr ? closedX() : lv_obj_get_style_x(drawer, LV_PART_MAIN);
}

lv_opa_t EdgeMenuView::hintOpacity() const {
    return tab == nullptr ? lv_opa_t(LV_OPA_TRANSP) : lv_obj_get_style_opa(tab, LV_PART_MAIN);
}

void EdgeMenuView::moveTo(int32_t x, bool snap) {
    lv_anim_delete(drawer, setX);
    if (!snap) {
        lv_obj_set_x(drawer, x);
        return;
    }
    lv_anim_t a{};
    lv_anim_init(&a);
    lv_anim_set_var(&a, drawer);
    lv_anim_set_exec_cb(&a, setX);
    lv_anim_set_values(&a, menuLeft(), x);
    lv_anim_set_duration(&a, SNAP_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

void EdgeMenuView::stopFades() {
    for (lv_obj_t* o : {drawer, tab}) {
        lv_anim_delete(o, fadeIn);
        lv_anim_delete(o, fadeOut);
    }
}

void EdgeMenuView::fade(lv_anim_exec_xcb_t step, lv_opa_t to, uint32_t delayMs, uint32_t ms) {
    for (lv_obj_t* o : {drawer, tab}) {
        lv_anim_delete(o, step);
        lv_anim_t a{};
        lv_anim_init(&a);
        lv_anim_set_var(&a, o);
        lv_anim_set_exec_cb(&a, step);
        // A fade that waits starts from full strength once its wait is over, and changes nothing
        // before.
        const lv_opa_t from =
            delayMs == 0 ? lv_obj_get_style_opa(o, LV_PART_MAIN) : lv_opa_t(LV_OPA_COVER);
        lv_anim_set_values(&a, from, to);
        lv_anim_set_early_apply(&a, delayMs == 0);
        lv_anim_set_delay(&a, delayMs);
        lv_anim_set_duration(&a, ms);
        if (o == drawer) {
            lv_anim_set_user_data(&a, this);
            lv_anim_set_completed_cb(&a, faded);
        }
        lv_anim_start(&a);
    }
}

// Once the hint has faded, the drawer goes back out of sight, so a later hint starts there.
void EdgeMenuView::faded(lv_anim_t* a) {
    EdgeMenuView* self = static_cast<EdgeMenuView*>(lv_anim_get_user_data(a));
    if (!self->open && lv_obj_get_style_opa(self->drawer, LV_PART_MAIN) == LV_OPA_TRANSP) {
        lv_obj_set_x(self->drawer, closedX());
    }
}

void EdgeMenuView::clicked(lv_event_t* e) {
    EdgeMenuView* self = static_cast<EdgeMenuView*>(lv_event_get_user_data(e));
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    const Item item = Item(uintptr_t(lv_obj_get_user_data(target)));
    if (self->chosen != nullptr) {
        self->chosen(item, self->context);
    }
}
