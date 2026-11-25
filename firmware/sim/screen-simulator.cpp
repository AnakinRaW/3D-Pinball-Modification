// The screen in the Arcade's WebAssembly module. The firmware's screen component draws with LVGL
// into the simulated display, whose pixels the browser reads. The firmware's touch driver reads the
// simulated touch controller, which answers with the finger the browser reports, so taps, holds
// and moves come out of the driver as they do on the machine. Until a host and a game draw on the
// screen, a test scene shows a button and the last gesture, and a stand-in for the host shows the
// hint at the right edge and a menu that the swipe pulls in.
#include "screen-simulator.h"
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <vector>
#include "components/screen.h"
#include "core/clock.h"
#include "core/event-queue.h"
#include "drivers/display.h"
#include "drivers/driver-base.h"
#include "drivers/touch.h"
#include "logging/debug-log.h"

using emscripten::val;

/// The host's part of the test scene, until the firmware's host is built: the gold tab and the
/// menu's edge while a finger rests on the right edge, and a menu that follows the swipe. It takes
/// the host's name, under which the screen hands out the host's layer and the swipe.
class GameHost {
public:
    /// Places the tab and the closed menu on the host's layer.
    static void build(Screen& screen);

    /// Follows the touch at the right edge: shows the tab and the menu's edge, moves the menu with
    /// the swipe and snaps it open or back once the finger leaves.
    ///
    /// @note  Every pass calls it, after the touch events.
    static void follow(Screen& screen);

    /// Closes an open menu, as a tap on the game does.
    static void tapped();

private:
    /// Moves the menu's left edge to @p x, at once or in the snap's time.
    static void moveMenu(int32_t x, bool snap);

    static void closePressed(lv_event_t*);

    static lv_obj_t* tab;
    static lv_obj_t* menu;
    static bool open;
    static Screen::Edge last;
};

lv_obj_t* GameHost::tab = nullptr;
lv_obj_t* GameHost::menu = nullptr;
bool GameHost::open = false;
Screen::Edge GameHost::last = Screen::Edge::NONE;

namespace {

constexpr uint32_t US_PER_MS = 1000;
constexpr uint32_t TOUCH_READY_MS =
    580;   // touch.cpp: the controller's first answer after power-up
constexpr uint32_t EVENTS_PER_PASS = 16;   // the driver events one pass takes at most
constexpr size_t EVENTS_KEPT = 256;        // the events screenEvents() hands over at most
// FT6x06 register map: TD_STATUS holds the number of touch points, and the upper four bits of a
// coordinate share P1_XH and P1_YH with the event flag
constexpr uint8_t ONE_TOUCH = 1;
constexpr uint8_t NO_TOUCH = 0;
constexpr uint8_t CONTACT_FLAG = 0x80;
constexpr uint8_t BYTE_BITS = 8;
constexpr uint8_t READ_BYTES = 5;
constexpr uint8_t TEST_LAYER_KEY = 1;   // the test scene's slide
constexpr uint8_t TEST_LAYER_PRIORITY = 1;
constexpr int32_t BUTTON_WIDTH = 128;   // host-screens.md: at least a 44 px touch target
constexpr int32_t BUTTON_HEIGHT = 48;
constexpr int32_t MARGIN_PX = 8;
constexpr uint32_t NIGHT = 0x0e061a;     // host-screens.md: the screen's background
constexpr uint32_t CREAM = 0xfff4d6;     // host-screens.md: text
constexpr uint32_t RED = 0xb3242d;       // host-screens.md: a button
constexpr uint32_t GOLD = 0xe8c04a;      // host-screens.md: a button's rim
constexpr uint32_t VELVET = 0x2c0a46;    // host-screens.md: tiles and rows
constexpr int32_t MENU_WIDTH_PX = 208;   // host-screens.md: the menu's drawer
constexpr int32_t PEEK_PX = 12;          // the menu's edge while a finger rests on the right edge
constexpr int32_t TAB_WIDTH_PX = 8;      // the gold tab at the right edge
constexpr int32_t TAB_HEIGHT_PX = 56;
constexpr int32_t CLOSE_WIDTH_PX = 96;   // host-screens.md: at least a 44 px touch target
constexpr int32_t CLOSE_HEIGHT_PX = 44;
constexpr uint32_t SNAP_MS = 150;   // host-screens.md: the menu snaps open or back

/// An event the browser lists: a touch event of the driver, a gesture the screen made, a press of
/// the test scene's button, or a step of the swipe and the menu.
struct Noted {
    const char* type;
    uint16_t x;
    uint16_t y;
    uint32_t ms;
};

Display display;
EventQueue driverEvents;
DriverTick driverTick;
io::I2cBus touchBus;
Touch touch(touchBus, driverEvents, driverTick);
Screen screen(display);
IntervalTimer* tickTimer = nullptr;   // the driver tick's timer, which the passes fire
uint32_t lastTickUs = 0;
uint32_t answered = 0;   // the controller's reads the simulator has answered
Finger finger = {false, 0, 0};
std::vector<Noted> noted;
lv_obj_t* gestureLabel = nullptr;
lv_obj_t* buttonLabel = nullptr;
uint32_t presses = 0;
bool begun = false;

void note(const char* type, uint16_t x, uint16_t y) {
    if (noted.size() < EVENTS_KEPT) {
        noted.push_back({type, x, y, nowUs() / US_PER_MS});
    }
}

/// Gets the controller's reading at a picture's coordinate: the inverse of the touch driver's map
/// between the readings at the picture's two edges, rounded to the nearest reading.
uint16_t reading(int32_t first, int32_t last, uint16_t at, int32_t pixels) {
    const int32_t scaled = int32_t(at) * (last - first);
    const int32_t steps = pixels - 1;
    const int32_t rounded =
        scaled >= 0 ? (scaled * 2 + steps) / (2 * steps) : -((-scaled * 2 + steps) / (2 * steps));
    return uint16_t(first + rounded);
}

/// Ends the read the touch driver asked for with the bytes of the browser's finger.
void answerRead() {
    io::I2cTransfer* read = touchBus.reading;
    if (read == nullptr || read->requests == answered) {
        return;
    }
    answered = read->requests;
    // The landscape picture's x runs along the panel's y and its y along the panel's x, as the
    // touch driver maps them.
    const Touch::Settings s = touch.settings();
    const uint16_t panelY = reading(s.rawLeft, s.rawRight, finger.x, Display::WIDTH);
    const uint16_t panelX = reading(s.rawTop, s.rawBottom, finger.y, Display::HEIGHT);
    const uint8_t bytes[READ_BYTES] = {finger.down ? ONE_TOUCH : NO_TOUCH,
                                       uint8_t(CONTACT_FLAG | panelX >> BYTE_BITS), uint8_t(panelX),
                                       uint8_t(panelY >> BYTE_BITS), uint8_t(panelY)};
    touchBus.end(*read, bytes, 0);
}

const char* touchName(DriverEventType type) {
    switch (type) {
    case DriverEventType::TOUCH_DOWN:
        return "touchDown";
    case DriverEventType::TOUCH_MOVED:
        return "touchMoved";
    case DriverEventType::TOUCH_HELD:
        return "touchHeld";
    case DriverEventType::TOUCH_UP:
        return "touchUp";
    default:
        return nullptr;
    }
}

/// Shows a gesture in the test scene and notes it for the browser.
void showGesture(const PinballEvent& e) {
    const bool tap = e.type == EventType::SCREEN_TAPPED;
    const TouchPoint at =
        tap ? payload<EventType::SCREEN_TAPPED>(e) : payload<EventType::SCREEN_HELD>(e);
    note(tap ? "screenTapped" : "screenHeld", at.x, at.y);
    lv_label_set_text_fmt(gestureLabel, "%s at %u, %u", tap ? "Tap" : "Hold", unsigned(at.x),
                          unsigned(at.y));
    if (tap) {
        GameHost::tapped();
    }
}

void pressed(lv_event_t*) {
    ++presses;
    note("buttonClicked", 0, 0);
    lv_label_set_text_fmt(buttonLabel, "Pressed %lu", static_cast<unsigned long>(presses));
}

/// Places the test scene on the game's layers: a background, a button on a slide and the last
/// gesture on the overlay.
void buildTestScene() {
    lv_obj_t* back = screen.background();
    lv_obj_set_style_bg_color(back, lv_color_hex(NIGHT), 0);
    lv_obj_set_style_bg_opa(back, LV_OPA_COVER, 0);
    lv_obj_t* title = lv_label_create(back);
    lv_label_set_text(title, "Arcade test scene");
    lv_obj_set_style_text_color(title, lv_color_hex(CREAM), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, MARGIN_PX, MARGIN_PX);

    lv_obj_t* slide = screen.slide(Layer{TEST_LAYER_KEY, TEST_LAYER_PRIORITY});
    lv_obj_t* button = lv_button_create(slide);
    lv_obj_set_size(button, BUTTON_WIDTH, BUTTON_HEIGHT);
    lv_obj_center(button);
    lv_obj_set_style_bg_color(button, lv_color_hex(RED), 0);
    lv_obj_set_style_border_color(button, lv_color_hex(GOLD), 0);
    lv_obj_set_style_border_width(button, 2, 0);
    lv_obj_add_event_cb(button, pressed, LV_EVENT_CLICKED, nullptr);
    buttonLabel = lv_label_create(button);
    lv_label_set_text(buttonLabel, "Press");
    lv_obj_set_style_text_color(buttonLabel, lv_color_hex(CREAM), 0);
    lv_obj_center(buttonLabel);

    gestureLabel = lv_label_create(screen.overlay());
    lv_label_set_text(gestureLabel, "Tap or hold beside the button");
    lv_obj_set_style_text_color(gestureLabel, lv_color_hex(CREAM), 0);
    lv_obj_align(gestureLabel, LV_ALIGN_BOTTOM_LEFT, MARGIN_PX, -MARGIN_PX);
}

// Starts the display, the touch driver and the screen as the main task does, and places the test
// scene. The touch driver waits for the controller's first answer after power-up, so a clock
// before it moves on to that time. Returns false when the touch driver does not begin, and on a
// second call what the first returned.
bool screenBegin() {
    static bool result = false;
    if (begun) {
        return result;
    }
    begun = true;
    if (fakeMillis < TOUCH_READY_MS) {
        GPT2_CNT = TOUCH_READY_MS * US_PER_MS;
        fakeMillis = TOUCH_READY_MS;
    }
    display.begin();
    if (!touch.begin()) {
        DEBUG_LOG("simulator: the touch driver does not begin\n");
        return false;
    }
    tickTimer = fakeTimers.running;
    lastTickUs = nowUs();
    screen.begin();
    buildTestScene();
    GameHost::build(screen);
    result = true;
    return true;
}

// Sets the finger the simulated touch controller reports from its next read on, held inside the
// screen. down false lifts it.
void touchAt(bool down, int32_t x, int32_t y) {
    const int32_t maxX = Display::WIDTH - 1;
    const int32_t maxY = Display::HEIGHT - 1;
    finger.down = down;
    finger.x = uint16_t(x < 0 ? 0 : x > maxX ? maxX : x);
    finger.y = uint16_t(y < 0 ? 0 : y > maxY ? maxY : y);
}

// Gets the display's pixels: 320 × 240, row by row from the top left, two bytes each in RGB565,
// high byte first. The view lies in the module's memory, so the browser copies it at once.
val screenPixels() {
    return val(emscripten::typed_memory_view(
        size_t(Display::WIDTH) * Display::HEIGHT * Display::PIXEL_BYTES, display.pixels()));
}

// Gets how many rectangles the display has taken, which rises with every change on the screen.
uint32_t screenRectangles() {
    return display.rectangles();
}

// Gets the events since the last call, oldest first, each {type, x, y, ms}: the touch driver's
// touchDown, touchMoved, touchHeld and touchUp, the screen's screenTapped and screenHeld,
// buttonClicked of the test scene's button, and the host stand-in's edgeTouched, swipeStarted and
// swipeLetGo with the finger's x, menuOpened and menuClosed.
val screenEvents() {
    val out = val::array();
    for (const Noted& n : noted) {
        val e = val::object();
        e.set("type", n.type);
        e.set("x", n.x);
        e.set("y", n.y);
        e.set("ms", n.ms);
        out.call<void>("push", e);
    }
    noted.clear();
    return out;
}

}   // namespace

void screenPass() {
    if (!begun || tickTimer == nullptr) {
        return;
    }
    if (elapsedUs(lastTickUs, DriverTick::PERIOD_US)) {
        lastTickUs += DriverTick::PERIOD_US;
        tickTimer->fire();
    }
    answerRead();
    DriverEvent pending[EVENTS_PER_PASS];
    const uint32_t n = driverEvents.read(pending, EVENTS_PER_PASS);
    for (uint32_t i = 0; i < n; ++i) {
        const DriverEvent& e = pending[i];
        if (const char* name = touchName(e.type)) {
            const TouchPoint at = payload<DriverEventType::TOUCH_DOWN>(e);
            note(name, at.x, at.y);
        }
        PinballEvents made;
        if (!screen.translate(e, made)) {
            continue;
        }
        for (uint8_t k = 0; k < made.count; ++k) {
            showGesture(made.events[k]);
        }
    }
    GameHost::follow(screen);
    screen.render();
}

void GameHost::build(Screen& screen) {
    lv_obj_t* layer = screen.hostLayer();
    // The menu takes every touch on it, so a tap on the menu reaches no game.
    menu = lv_obj_create(layer);
    lv_obj_remove_style_all(menu);
    lv_obj_set_size(menu, MENU_WIDTH_PX, Display::HEIGHT);
    lv_obj_set_pos(menu, Display::WIDTH, 0);
    lv_obj_set_style_bg_color(menu, lv_color_hex(VELVET), 0);
    lv_obj_set_style_bg_opa(menu, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(menu, lv_color_hex(GOLD), 0);
    lv_obj_set_style_border_width(menu, 2, 0);
    lv_obj_set_style_border_side(menu, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_scrollable(menu, false);
    lv_obj_t* title = lv_label_create(menu);
    lv_label_set_text(title, "Menu");
    lv_obj_set_style_text_color(title, lv_color_hex(CREAM), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, MARGIN_PX * 2, MARGIN_PX * 2);
    lv_obj_t* close = lv_button_create(menu);
    lv_obj_set_size(close, CLOSE_WIDTH_PX, CLOSE_HEIGHT_PX);
    lv_obj_align(close, LV_ALIGN_BOTTOM_LEFT, MARGIN_PX * 2, -MARGIN_PX * 2);
    lv_obj_set_style_bg_color(close, lv_color_hex(RED), 0);
    lv_obj_set_style_border_color(close, lv_color_hex(GOLD), 0);
    lv_obj_set_style_border_width(close, 2, 0);
    lv_obj_add_event_cb(close, closePressed, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* label = lv_label_create(close);
    lv_label_set_text(label, "Close");
    lv_obj_set_style_text_color(label, lv_color_hex(CREAM), 0);
    lv_obj_center(label);

    // The tab lies in front of the menu, on the menu's edge while a finger rests on the right edge.
    tab = lv_obj_create(layer);
    lv_obj_remove_style_all(tab);
    lv_obj_set_size(tab, TAB_WIDTH_PX, TAB_HEIGHT_PX);
    lv_obj_align(tab, LV_ALIGN_RIGHT_MID, TAB_WIDTH_PX / 2 - PEEK_PX, 0);
    lv_obj_set_style_bg_color(tab, lv_color_hex(GOLD), 0);
    lv_obj_set_style_bg_opa(tab, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(tab, TAB_WIDTH_PX / 2, 0);
    lv_obj_set_clickable(tab, false);
    lv_obj_set_hidden(tab, true);
}

void GameHost::follow(Screen& screen) {
    uint16_t x = 0;
    const Screen::Edge e = screen.edge(x);
    const int32_t openX = Display::WIDTH - MENU_WIDTH_PX;
    if (e == Screen::Edge::TOUCHED) {
        if (last != Screen::Edge::TOUCHED) {
            note("edgeTouched", x, 0);
            lv_obj_set_hidden(tab, false);
            if (!open) {
                moveMenu(Display::WIDTH - PEEK_PX, false);
            }
        }
    } else if (e == Screen::Edge::SWIPING) {
        if (last != Screen::Edge::SWIPING) {
            note("swipeStarted", x, 0);
            lv_obj_set_hidden(tab, true);
        }
        // The menu's left edge follows the finger, held between open and closed.
        const int32_t left = x < openX ? openX : x;
        moveMenu(left, false);
    } else if (e == Screen::Edge::LET_GO) {
        note("swipeLetGo", x, 0);
        // A finger let go past half the menu's width opens it, and one let go before that lets
        // it slide back.
        const bool opens = x < Display::WIDTH - MENU_WIDTH_PX / 2;
        moveMenu(opens ? openX : Display::WIDTH, true);
        if (opens != open) {
            note(opens ? "menuOpened" : "menuClosed", 0, 0);
        }
        open = opens;
    } else if (last == Screen::Edge::TOUCHED) {
        // A tap or a rest at the edge lets the hint go, and the game gets its touch.
        lv_obj_set_hidden(tab, true);
        if (!open) {
            moveMenu(Display::WIDTH, false);
        }
    }
    last = e == Screen::Edge::LET_GO ? Screen::Edge::NONE : e;
}

void GameHost::tapped() {
    if (!open) {
        return;
    }
    open = false;
    moveMenu(Display::WIDTH, true);
    note("menuClosed", 0, 0);
}

void GameHost::closePressed(lv_event_t*) {
    tapped();
}

void GameHost::moveMenu(int32_t x, bool snap) {
    lv_anim_delete(menu, nullptr);
    if (!snap) {
        lv_obj_set_x(menu, x);
        return;
    }
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, menu);
    lv_anim_set_values(&a, lv_obj_get_x(menu), x);
    lv_anim_set_duration(&a, SNAP_MS);
    lv_anim_set_exec_cb(&a,
                        [](void* obj, int32_t v) { lv_obj_set_x(static_cast<lv_obj_t*>(obj), v); });
    lv_anim_start(&a);
}

EMSCRIPTEN_BINDINGS(screen) {
    emscripten::function("screenBegin", &screenBegin);
    emscripten::function("touch", &touchAt);
    emscripten::function("screenPixels", &screenPixels);
    emscripten::function("screenRectangles", &screenRectangles);
    emscripten::function("screenEvents", &screenEvents);
}
