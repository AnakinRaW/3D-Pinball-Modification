// The screen in the Arcade's WebAssembly module. The firmware's screen component draws with LVGL
// into the simulated display, whose pixels the browser reads. The firmware's touch driver reads the
// simulated touch controller, which answers with the finger the browser reports, so taps, holds
// and moves come out of the driver as they do on the machine. Until a host and a game draw on the
// screen, a test scene shows a button and the last gesture, a page shows every control of the
// host's screens, the boot film plays through the firmware's video player, and a stand-in for the
// host drives the host's menu at the right edge from the swipe.
#include "screen-simulator.h"
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <string>
#include <vector>
#include "components/screen.h"
#include "core/clock.h"
#include "core/event-queue.h"
#include "drivers/display.h"
#include "drivers/driver-base.h"
#include "drivers/touch.h"
#include "host/views/controls.h"
#include "host/views/edge-menu-view.h"
#include "logging/debug-log.h"
#include "video-simulator.h"

using emscripten::val;

/// The host's part of the test scene, until the firmware's host is built. It drives the host's
/// menu at the right edge from the swipe and gives its rows made-up figures. It takes the host's
/// name, under which the screen hands out the host's layer and the swipe.
class GameHost {
public:
    /// Places the menu at the right edge on the host's layer.
    static void build(Screen& screen);

    /// Plays the boot film from the card's root on the whole screen, over and over and without its
    /// sound, on a slide in front of the test scene and the controls page.
    ///
    /// @return  False when the screen has no slide for it or the video player refuses it.
    static bool playBootFilm(Screen& screen);

    /// Takes the boot film's slide away, with the film.
    static void stopBootFilm(Screen& screen);

    /// Hands the menu the touch at the right edge: the hint, the swipe and its let-go.
    ///
    /// @note  Every pass calls it, after the touch events.
    static void follow(Screen& screen);

    /// Closes an open menu, as a tap on the game does.
    static void tapped();

private:
    /// Takes an item the player chose in the menu.
    static void chosen(EdgeMenuView::Item item, void* context);

    static EdgeMenuView menu;
    static Screen::Edge last;
};

EdgeMenuView GameHost::menu;
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
constexpr uint32_t NIGHT = 0x0e061a;        // host-screens.md: the screen's background
constexpr uint32_t CREAM = 0xfff4d6;        // host-screens.md: text
constexpr uint32_t RED = 0xb3242d;          // host-screens.md: a button
constexpr uint32_t GOLD = 0xe8c04a;         // host-screens.md: a button's rim
constexpr uint8_t CONTROLS_LAYER_KEY = 2;   // the controls page's slide, over the test scene
constexpr uint8_t CONTROLS_LAYER_PRIORITY = 2;
// The boot film's slide, in front of the test scene and the controls page
constexpr Layer BOOT_LAYER = {3, 3};
constexpr char BOOT_FILM[] = "intro";   // media.md: video/intro.rgb at the card's root
constexpr char BOOT_FILM_SCENE[] = "boot-film";
constexpr int32_t SHEET_INSET_PX = 4;      // the controls page's margin
constexpr int32_t SHEET_BUTTON_PX = 100;   // a button's width on the controls page
constexpr int32_t SHEET_BUTTONS_Y = 46;    // the controls page's rows, from the top
constexpr int32_t SHEET_LIGHTS_Y = 94;
constexpr int32_t SHEET_SLIDER_Y = 142;
constexpr int32_t SHEET_LIST_Y = 190;
constexpr int32_t SHEET_BULB_X = 12;   // the first warning light and the step to the next
constexpr int32_t SHEET_BULB_STEP_PX = 24;
constexpr int32_t SHEET_SLIDER_START = 50;   // the slider's value at first, in percent
constexpr int32_t DIALOG_WIDTH_PX = 280;     // host-screens.md's End game dialog
constexpr int32_t DIALOG_HEIGHT_PX = 150;
constexpr int32_t DIALOG_TEXT_X = 14;
constexpr int32_t DIALOG_TEXT_Y = 42;
constexpr int32_t DIALOG_BUTTON_PX = 122;
constexpr int32_t DIALOG_BUTTONS_Y = 92;

/// An event the browser lists: a touch event of the driver, a gesture the screen made, a press of
/// the test scene's button, or a step of the swipe and the menu.
struct Noted {
    const char* type;
    uint16_t x;
    uint16_t y;
    uint32_t ms;
    const char* name = nullptr;   // the control a press or a change came from
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
lv_obj_t* controlsPage = nullptr;   // the controls page's slide while it shows
lv_obj_t* openDialog = nullptr;     // the dialog START opens on the controls page
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

/// Notes a press or a change of a control on the controls page.
void noteControl(const char* name, uint16_t value) {
    if (noted.size() < EVENTS_KEPT) {
        noted.push_back({"control", value, 0, nowUs() / US_PER_MS, name});
    }
}

void openStartDialog();

// A control of the controls page names itself through its user data. START opens a dialog, and
// either of the dialog's buttons closes it.
void controlPressed(lv_event_t* e) {
    const char* name = static_cast<const char*>(lv_event_get_user_data(e));
    noteControl(name, 0);
    const std::string n = name;
    if (n == "START" && openDialog == nullptr) {
        openStartDialog();
    } else if ((n == "BACK" || n == "OK") && openDialog != nullptr) {
        controls::closeDialog(openDialog);
        openDialog = nullptr;
    }
}

void sliderChanged(lv_event_t* e) {
    lv_obj_t* track = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    noteControl("slider", uint16_t(lv_slider_get_value(track)));
}

lv_obj_t* pressable(lv_obj_t* control, const char* name) {
    lv_obj_add_event_cb(control, controlPressed, LV_EVENT_CLICKED, const_cast<char*>(name));
    return control;
}

void openStartDialog() {
    openDialog = controls::dialog(controlsPage, "START GAME?", DIALOG_WIDTH_PX, DIALOG_HEIGHT_PX);
    lv_obj_t* text = controls::label(openDialog, "The ball is served once the game begins.",
                                     &montserrat_semibold_14, controls::color::CREAM);
    lv_obj_set_width(text, DIALOG_WIDTH_PX - DIALOG_TEXT_X * 2);
    lv_obj_set_pos(text, DIALOG_TEXT_X, DIALOG_TEXT_Y);
    lv_obj_t* back = pressable(
        controls::button(openDialog, controls::Plaque::VIOLET, "BACK", DIALOG_BUTTON_PX), "BACK");
    lv_obj_align(back, LV_ALIGN_TOP_LEFT, DIALOG_TEXT_X - 2, DIALOG_BUTTONS_Y);
    lv_obj_t* ok = pressable(
        controls::button(openDialog, controls::Plaque::RED, "OK", DIALOG_BUTTON_PX), "OK");
    lv_obj_align(ok, LV_ALIGN_TOP_RIGHT, -(DIALOG_TEXT_X - 2), DIALOG_BUTTONS_Y);
}

/// Places every control on a slide over the test scene, as host-screens.md's controls show them.
void buildControlsPage() {
    controlsPage = screen.slide(Layer{CONTROLS_LAYER_KEY, CONTROLS_LAYER_PRIORITY});
    lv_obj_set_style_bg_color(controlsPage, lv_color_hex(NIGHT), 0);
    lv_obj_set_style_bg_opa(controlsPage, LV_OPA_COVER, 0);
    lv_obj_t* header = controls::header(controlsPage, "CONTROLS", true);
    pressable(controls::headerButton(header, true, LV_SYMBOL_LEFT), "back");
    pressable(controls::headerButton(header, false, LV_SYMBOL_SETTINGS), "gear");

    const struct {
        controls::Plaque plaque;
        const char* text;
    } buttons[] = {{controls::Plaque::GOLD, "PLAY"},
                   {controls::Plaque::RED, "START"},
                   {controls::Plaque::VIOLET, "CANCEL"}};
    int32_t x = SHEET_INSET_PX;
    for (const auto& b : buttons) {
        lv_obj_t* o =
            pressable(controls::button(controlsPage, b.plaque, b.text, SHEET_BUTTON_PX), b.text);
        lv_obj_set_pos(o, x, SHEET_BUTTONS_Y);
        x += SHEET_BUTTON_PX + SHEET_INSET_PX + 2;
    }
    lv_obj_t* off = pressable(
        controls::button(controlsPage, controls::Plaque::RED, "OFF", SHEET_BUTTON_PX), "OFF");
    lv_obj_set_pos(off, x - SHEET_BUTTON_PX - SHEET_INSET_PX - 2, SHEET_LIGHTS_Y);
    lv_obj_add_state(off, LV_STATE_DISABLED);

    const controls::Light lights[] = {controls::Light::LIT, controls::Light::UNLIT,
                                      controls::Light::WORKS, controls::Light::WARNING,
                                      controls::Light::FAILED};
    int32_t bx = SHEET_BULB_X;
    for (controls::Light l : lights) {
        lv_obj_t* b = controls::bulb(controlsPage, l);
        lv_obj_set_pos(b, bx,
                       SHEET_LIGHTS_Y +
                           (controls::TARGET_PX - lv_obj_get_style_height(b, LV_PART_MAIN)) / 2);
        bx += SHEET_BULB_STEP_PX;
    }

    lv_obj_t* slider =
        controls::slider(controlsPage, controls::Fill::SOUND, Display::WIDTH - SHEET_INSET_PX * 2);
    lv_obj_set_pos(slider, SHEET_INSET_PX, SHEET_SLIDER_Y);
    lv_obj_t* track = controls::sliderTrack(slider);
    lv_slider_set_value(track, SHEET_SLIDER_START, LV_ANIM_OFF);
    lv_obj_add_event_cb(track, sliderChanged, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t* row = pressable(controls::row(controlsPage, "Record ball sensors",
                                            "After a change to the playfield",
                                            Display::WIDTH - SHEET_INSET_PX * 2),
                              "row");
    lv_obj_set_pos(row, SHEET_INSET_PX, SHEET_LIST_Y);
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
    videoBegin(screen);
    buildTestScene();
    GameHost::build(screen);
    result = true;
    return true;
}

// Shows a scene: "test" for the test scene, "controls" for the page of every control over it, and
// "boot-film" for the boot film over both. Returns false for another name.
bool screenScene(std::string name) {
    if (name == BOOT_FILM_SCENE) {
        GameHost::stopBootFilm(screen);
        GameHost::playBootFilm(screen);
        return true;
    }
    if (name == "controls" || name == "test") {
        GameHost::stopBootFilm(screen);
    }
    if (name == "controls") {
        if (controlsPage == nullptr) {
            buildControlsPage();
        }
        screen.showOverlay(false);
        return true;
    }
    if (name == "test") {
        if (controlsPage != nullptr) {
            screen.remove(Layer{CONTROLS_LAYER_KEY, CONTROLS_LAYER_PRIORITY});
            controlsPage = nullptr;
            openDialog = nullptr;
        }
        screen.showOverlay(true);
        return true;
    }
    DEBUG_LOG("simulator: no scene %s\n", name.c_str());
    return false;
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
// buttonClicked of the test scene's button, the host stand-in's edgeTouched, swipeStarted and
// swipeLetGo with the finger's x, menuOpened and menuClosed, the menu's items sound, lights,
// machine, endGame and close, and control with the name of a control on the controls page, and
// for its slider the value in x.
val screenEvents() {
    val out = val::array();
    for (const Noted& n : noted) {
        val e = val::object();
        e.set("type", n.type);
        e.set("x", n.x);
        e.set("y", n.y);
        e.set("ms", n.ms);
        if (n.name != nullptr) {
            e.set("name", n.name);
        }
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
    videoPass();
    screen.render();
}

void GameHost::build(Screen& screen) {
    menu.build(screen.hostLayer(), chosen, nullptr);
    // Made-up figures stand in for the machine's, as the menu's storyboard shows them.
    EdgeMenuView::Status status;
    status.sound = "50 % / 70 %";
    status.lights = "80 % / 60 %";
    status.machine = "1 fault";
    status.fault = true;
    menu.show(status);
}

void GameHost::follow(Screen& screen) {
    uint16_t x = 0;
    const Screen::Edge e = screen.edge(x);
    if (e == Screen::Edge::TOUCHED && last != Screen::Edge::TOUCHED) {
        note("edgeTouched", x, 0);
        menu.hint();
    } else if (e == Screen::Edge::SWIPING) {
        if (last != Screen::Edge::SWIPING) {
            note("swipeStarted", x, 0);
        }
        menu.follow(x);
    } else if (e == Screen::Edge::LET_GO) {
        note("swipeLetGo", x, 0);
        const bool was = menu.isOpen();
        const bool opens = menu.letGo(x);
        if (opens != was) {
            note(opens ? "menuOpened" : "menuClosed", 0, 0);
        }
    } else if (e == Screen::Edge::NONE && last == Screen::Edge::TOUCHED) {
        // A tap or a rest at the edge lets the hint stand and fade, and the game gets its touch.
        menu.releaseHint();
    }
    last = e == Screen::Edge::LET_GO ? Screen::Edge::NONE : e;
}

bool GameHost::playBootFilm(Screen& screen) {
    VideoPlayer* player = videoPlayer();
    lv_obj_t* slide = screen.slide(BOOT_LAYER);
    if (player == nullptr || slide == nullptr) {
        DEBUG_LOG("simulator: no slide or no video player for the boot film\n");
        return false;
    }
    videoFiles().useRoot();
    lv_obj_t* viewport = lv_obj_create(slide);
    lv_obj_remove_style_all(viewport);
    lv_obj_set_size(viewport, Display::WIDTH, Display::HEIGHT);
    if (!player->play(viewport, BOOT_FILM, true, false)) {
        DEBUG_LOG("simulator: the video player refuses the boot film\n");
        screen.remove(BOOT_LAYER);
        return false;
    }
    return true;
}

void GameHost::stopBootFilm(Screen& screen) {
    screen.remove(BOOT_LAYER);
}

void GameHost::tapped() {
    if (!menu.isOpen()) {
        return;
    }
    menu.close();
    note("menuClosed", 0, 0);
}

void GameHost::chosen(EdgeMenuView::Item item, void*) {
    static const char* const NAMES[] = {"sound", "lights", "machine", "endGame", "close"};
    note(NAMES[uint8_t(item)], 0, 0);
    if (item == EdgeMenuView::Item::CLOSE) {
        tapped();
    }
}

EMSCRIPTEN_BINDINGS(screen) {
    emscripten::function("screenBegin", &screenBegin);
    emscripten::function("touch", &touchAt);
    emscripten::function("screenPixels", &screenPixels);
    emscripten::function("screenRectangles", &screenRectangles);
    emscripten::function("screenEvents", &screenEvents);
    emscripten::function("screenScene", &screenScene);
}
