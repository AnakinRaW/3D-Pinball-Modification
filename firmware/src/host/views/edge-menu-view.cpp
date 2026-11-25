#include "edge-menu-view.h"
#include "logging/debug-log.h"
#include <initializer_list>

constexpr uint32_t NIGHT = 0x0e061a;      // host-screens.md, Colors: night
constexpr uint32_t VELVET = 0x2c0a46;     // host-screens.md, Colors: velvet
constexpr uint32_t FRAME = 0xc8242f;      // host-screens.md, Colors: frame
constexpr uint32_t CABINET = 0xb3242d;    // host-screens.md, Colors: cabinet
constexpr uint32_t SHADOW = 0x5e1016;     // host-screens.md, Colors: shadow
constexpr uint32_t GOLD = 0xe8c04a;       // host-screens.md, Colors: gold
constexpr uint32_t BULB = 0xffd65a;       // host-screens.md, Colors: bulb
constexpr uint32_t BULB_OFF = 0x5a3c1e;   // host-screens.md, Colors: bulb off
constexpr uint32_t CREAM = 0xfff4d6;      // host-screens.md, Colors: cream
constexpr uint32_t RAMP = 0x3d82cf;       // host-screens.md, Colors: ramp
constexpr uint32_t BOARD = 0xe2c76b;      // host-screens.md, Colors: board
constexpr uint32_t GREEN = 0x3fa66b;      // host-screens.md, Colors: green
constexpr uint32_t NEON = 0xd63a48;       // host-screens.md, Colors: neon
constexpr int32_t HEADER_PX = 34;         // host-screens.md, Controls: the header's band
constexpr int32_t TARGET_PX = 44;         // host-screens.md, Controls: the least touch target
constexpr int32_t GAP_PX = 4;             // host-screens.md, Controls: between two buttons
constexpr int32_t RIM_PX = 2;             // host-screens.md, Controls: a button's gold rim
constexpr int32_t CORNER_PX = 8;          // host-screens.md, Controls: a button's corners
constexpr int32_t INSET_PX = 8;           // the rows' distance from the drawer's sides
constexpr int32_t BULB_PX = 4;            // a marquee bulb under the header
constexpr int32_t BULB_STEP_PX = 12;      // from one marquee bulb to the next
constexpr int32_t TAB_WIDTH_PX = 16;      // the gold tab of the hint
constexpr int32_t TAB_HEIGHT_PX = 56;
constexpr int32_t ICON_X_PX = 10;        // a row's icon from its left side
constexpr int32_t TEXT_X_PX = 40;        // a row's name and status from its left side
constexpr int32_t STATUS_Y_PX = 22;      // a row's status below its top
constexpr int32_t STATUS_BULB_PX = 14;   // the Machine row's bulb
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

/// Creates a label in a color.
static lv_obj_t* label(lv_obj_t* parent, const char* text, uint32_t color) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    return l;
}

/// Gives an object a plaque's fill, corners and rim.
static void plaque(lv_obj_t* o, uint32_t fill, uint32_t rim, lv_opa_t rimOpacity) {
    lv_obj_set_style_bg_color(o, lv_color_hex(fill), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, CORNER_PX, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(rim), 0);
    lv_obj_set_style_border_opa(o, rimOpacity, 0);
    lv_obj_set_style_border_width(o, RIM_PX, 0);
    // A pressed plaque sinks to the shadow red.
    lv_obj_set_style_bg_color(o, lv_color_hex(SHADOW), LV_STATE_PRESSED);
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

    lv_obj_t* header = lv_obj_create(drawer);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, WIDTH_PX, HEADER_PX);
    lv_obj_set_style_bg_color(header, lv_color_hex(FRAME), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_clickable(header, false);
    lv_obj_align(label(header, "MENU", CREAM), LV_ALIGN_LEFT_MID, INSET_PX * HALF, 0);
    lv_obj_t* close = lv_obj_create(header);
    lv_obj_remove_style_all(close);
    lv_obj_set_size(close, TARGET_PX, HEADER_PX);
    lv_obj_align(close, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_user_data(close, reinterpret_cast<void*>(uintptr_t(Item::CLOSE)));
    lv_obj_add_event_cb(close, clicked, LV_EVENT_CLICKED, this);
    lv_obj_center(label(close, LV_SYMBOL_RIGHT LV_SYMBOL_RIGHT, GOLD));

    // The marquee bulbs under the header, lit and unlit in turn.
    for (int32_t x = BULB_STEP_PX / HALF, i = 0; x < WIDTH_PX; x += BULB_STEP_PX, ++i) {
        lv_obj_t* bulb = lv_obj_create(drawer);
        lv_obj_remove_style_all(bulb);
        lv_obj_set_size(bulb, BULB_PX, BULB_PX);
        lv_obj_set_pos(bulb, x, HEADER_PX + GAP_PX / HALF);
        lv_obj_set_style_radius(bulb, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(bulb, lv_color_hex(i % HALF == 0 ? BULB : BULB_OFF), 0);
        lv_obj_set_style_bg_opa(bulb, LV_OPA_COVER, 0);
        lv_obj_set_clickable(bulb, false);
    }

    lv_obj_t* sound = row(LV_SYMBOL_VOLUME_MAX, RAMP, "Sound", Item::SOUND);
    lv_obj_t* lights = row(LV_SYMBOL_CHARGE, BOARD, "Lights", Item::LIGHTS);
    lv_obj_t* machine = row("", CREAM, "Machine", Item::MACHINE);
    soundText = lv_obj_get_child(sound, -1);
    lightsText = lv_obj_get_child(lights, -1);
    machineText = lv_obj_get_child(machine, -1);
    machineBulb = lv_obj_create(machine);
    lv_obj_remove_style_all(machineBulb);
    lv_obj_set_size(machineBulb, STATUS_BULB_PX, STATUS_BULB_PX);
    lv_obj_align(machineBulb, LV_ALIGN_LEFT_MID, ICON_X_PX, 0);
    lv_obj_set_style_radius(machineBulb, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(machineBulb, lv_color_hex(GREEN), 0);
    lv_obj_set_style_bg_opa(machineBulb, LV_OPA_COVER, 0);
    lv_obj_set_clickable(machineBulb, false);

    lv_obj_t* end = lv_obj_create(drawer);
    lv_obj_remove_style_all(end);
    lv_obj_set_size(end, WIDTH_PX - INSET_PX * HALF, TARGET_PX);
    lv_obj_align(end, LV_ALIGN_BOTTOM_MID, 0, -GAP_PX);
    plaque(end, CABINET, GOLD, LV_OPA_COVER);
    lv_obj_set_user_data(end, reinterpret_cast<void*>(uintptr_t(Item::END_GAME)));
    lv_obj_add_event_cb(end, clicked, LV_EVENT_CLICKED, this);
    lv_obj_center(label(end, LV_SYMBOL_LEFT "  END GAME", CREAM));

    tab = lv_obj_create(parent);
    lv_obj_remove_style_all(tab);
    lv_obj_set_size(tab, TAB_WIDTH_PX, TAB_HEIGHT_PX);
    lv_obj_align(tab, LV_ALIGN_RIGHT_MID, -PEEK_PX, 0);
    lv_obj_set_style_bg_color(tab, lv_color_hex(GOLD), 0);
    lv_obj_set_style_bg_opa(tab, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(tab, CORNER_PX, 0);
    lv_obj_set_style_opa(tab, LV_OPA_TRANSP, 0);
    lv_obj_set_clickable(tab, false);
    lv_obj_center(label(tab, LV_SYMBOL_LEFT, SHADOW));
}

lv_obj_t* EdgeMenuView::row(const char* symbol, uint32_t color, const char* name, Item item) {
    // The rows stand below the header and its bulbs, a touch target and a gap apart.
    const int32_t first = HEADER_PX + INSET_PX + GAP_PX;
    const int32_t at = first + int32_t(item) * (TARGET_PX + GAP_PX);
    lv_obj_t* r = lv_obj_create(drawer);
    lv_obj_remove_style_all(r);
    lv_obj_set_size(r, WIDTH_PX - INSET_PX * HALF, TARGET_PX);
    lv_obj_set_pos(r, INSET_PX, at);
    plaque(r, VELVET, GOLD, LV_OPA_40);
    lv_obj_set_user_data(r, reinterpret_cast<void*>(uintptr_t(item)));
    lv_obj_add_event_cb(r, clicked, LV_EVENT_CLICKED, this);
    lv_obj_align(label(r, symbol, color), LV_ALIGN_LEFT_MID, ICON_X_PX, 0);
    lv_obj_align(label(r, LV_SYMBOL_RIGHT, GOLD), LV_ALIGN_RIGHT_MID, -ICON_X_PX, 0);
    lv_obj_set_pos(label(r, name, CREAM), TEXT_X_PX, GAP_PX);
    // The status comes last, so build() finds it as the row's last child.
    lv_obj_t* status = label(r, "", BOARD);
    lv_obj_set_pos(status, TEXT_X_PX, STATUS_Y_PX);
    return r;
}

void EdgeMenuView::show(const Status& status) {
    if (drawer == nullptr) {
        DEBUG_LOG("EdgeMenuView::show: no build() yet\n");
        return;
    }
    lv_label_set_text(soundText, status.sound);
    lv_label_set_text(lightsText, status.lights);
    lv_label_set_text(machineText, status.machine);
    lv_obj_set_style_text_color(machineText, lv_color_hex(status.fault ? NEON : BOARD), 0);
    lv_obj_set_style_bg_color(machineBulb, lv_color_hex(status.fault ? NEON : GREEN), 0);
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
