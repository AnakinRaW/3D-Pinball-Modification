// The screen program of docs/firmware/testing.md. It runs the screen component on the display
// driver with pictures it writes into the test game's directory on the card.
//
//   load    pictures load into the picture memory, a short and a missing file fail
//   show    one picture shows in three images, an image deleted with its parent leaves the
//           others, and release() deletes them
//   redraw  LVGL never waits for the driver while it changes the whole screen in every refresh,
//           also beside a full-screen video
//   power-lost  the module's 5 V pulled for a moment: once the touch controller answers again,
//               restart() brings the picture back (asks at the bench)
//   layers  an object on every layer, a message box and a video lie over one another in the
//           order of the layers (asks at the bench)
//   taps    a tap, a hold, a press of a button and a finger slid onto the button, each made at
//           the bench, reach the game or the button as the screen hands them on
//
// Wiring: the Adafruit 2090 on its 8-bit bus, as pin-assignment.md gives it, and the card. The
// screen shows the test's pictures meanwhile. The tests leave their files in /games/tests/.
// power-lost and taps need the module's touch controller on Wire2 as well.

#include <Arduino.h>
#include <stdio.h>
#include "components/file-system.h"
#include "components/screen.h"
#include "components/sounds.h"
#include "components/video-player.h"
#include "core/clock.h"
#include "core/device-monitor.h"
#include "drivers/audio.h"
#include "drivers/display.h"
#include "drivers/storage.h"
#include "drivers/touch.h"
#include "test-drivers.h"
#include "test-harness.h"
#include "test-media.h"

constexpr uint32_t LIMIT_S = 60;     // the first run writes the test files
constexpr uint32_t LOAD_MS = 1000;   // far longer than a picture takes from the card
constexpr uint32_t REDRAW_MS = 3000;
/// The longest LVGL may wait for the driver before it draws: none, with room for the event's own
/// calls
constexpr uint32_t WAIT_MAX_US = 50;
constexpr size_t TEXT = 128;
constexpr uint32_t POWER_LIMIT_S = 300;
constexpr uint32_t LOST_TIMEOUT_MS = 80000;   // power-lost: the time for each step at the bench
constexpr uint32_t RESTART_TIMEOUT_MS = 2000;
constexpr uint32_t ASK_TIMEOUT_MS = 120000;
constexpr uint32_t BENCH_LIMIT_S = 300;
constexpr uint32_t SHOW_MS = 5000;   // layers: the time the layers show before the question
constexpr uint32_t STEP_TIMEOUT_MS = 60000;   // taps: the time for each step at the bench
constexpr Layer SLIDE = {1, 1};

using test::audio;
using test::display;
using test::files;
using test::screen;
using test::sounds;
using test::touch;
using test::videoPlayer;
static char seen[TEXT];
static char wanted[TEXT];

/// The host's calls to the screen, the file system and the video player.
class GameHost {
public:
    static void use(FileSystem& f, const char* game) { f.use(game); }
    static void reset(Screen& s, VideoPlayer& p) {
        p.reset();
        s.reset();
    }
    static void restart(Screen& s) { s.restart(); }
    static lv_obj_t* hostLayer(Screen& s) { return s.hostLayer(); }
};

// Begins the card, the display and the screen once, writes the pictures and starts each test on
// an empty screen. Skips the test without a card.
static bool ready() {
    static bool begun = false;
    if (!storage.begin()) {
        test::skip("no card in the socket");
        return false;
    }
    if (!begun) {
        if (!display.begin()) {
            test::fail("the display's begin() false", "true");
            return false;
        }
        screen.begin();
        videoPlayer.begin();
        begun = true;
    }
    if (!media::writePictures() || !media::writeLoop()) {
        test::fail("writing the test files failed", "they reach the card");
        return false;
    }
    GameHost::use(files, media::GAME);
    GameHost::reset(screen, videoPlayer);
    return true;
}

// Renders until no picture loads or LOAD_MS has passed.
static void renderWhileLoading(Picture* const* pictures, size_t n) {
    const uint32_t t0 = millis();
    while (millis() - t0 < LOAD_MS) {
        bool loading = false;
        for (size_t i = 0; i < n; ++i) {
            loading = loading || pictures[i]->loading();
        }
        screen.render();
        if (!loading) {
            return;
        }
    }
}

static void testLoad() {
    if (!ready()) {
        return;
    }
    static Picture background(screen);
    static Picture shortened(screen);
    static Picture icon(screen);
    static Picture missing(screen);
    Picture* const all[] = {&background, &shortened, &icon, &missing};
    background.load(files.open("background.img"));
    shortened.load(files.open("short.img"));
    icon.load(files.open("icon.img"));
    missing.load(files.open("none.img"));
    for (Picture* p : all) {
        if (!p->loading()) {
            test::fail("a picture not loading right after load()", "loading() true");
            return;
        }
    }
    lv_obj_t* back = background.show(screen.background());
    lv_obj_t* front = icon.show(screen.background());
    lv_obj_center(front);
    renderWhileLoading(all, 4);
    const bool expected[] = {false, true, false, true};
    for (size_t i = 0; i < 4; ++i) {
        if (all[i]->loading() || all[i]->failed() != expected[i]) {
            snprintf(seen, sizeof seen, "picture %u: loading %d, failed %d", unsigned(i),
                     all[i]->loading(), all[i]->failed());
            snprintf(wanted, sizeof wanted, "loaded, failed %d", expected[i]);
            test::fail(seen, wanted);
            return;
        }
    }
    lv_obj_update_layout(back);
    if (lv_obj_get_width(back) != media::W || lv_obj_get_height(back) != media::H ||
        lv_obj_get_width(front) != media::ICON_W || lv_obj_get_height(front) != media::ICON_H) {
        snprintf(seen, sizeof seen, "images of %ld x %ld and %ld x %ld",
                 long(lv_obj_get_width(back)), long(lv_obj_get_height(back)),
                 long(lv_obj_get_width(front)), long(lv_obj_get_height(front)));
        test::fail(seen, "the pictures' sizes");
        return;
    }
    // The pictures show for a moment.
    const uint32_t t0 = millis();
    while (millis() - t0 < LOAD_MS) {
        screen.render();
    }
    test::pass();
}

static void testShow() {
    if (!ready()) {
        return;
    }
    static Picture icon(screen);
    lv_obj_t* parents[3];
    lv_obj_t* images[3];
    for (int i = 0; i < 3; ++i) {
        parents[i] = lv_obj_create(screen.background());
        lv_obj_remove_style_all(parents[i]);
        lv_obj_set_size(parents[i], media::ICON_W, media::ICON_H);
        lv_obj_set_pos(parents[i], 20 + i * 100, 96);
    }
    icon.load(files.open("icon.img"));
    for (int i = 0; i < 3; ++i) {
        images[i] = icon.show(parents[i]);
    }
    Picture* const all[] = {&icon};
    renderWhileLoading(all, 1);
    if (icon.failed()) {
        test::fail("icon.img failed", "it loads");
        return;
    }
    const void* src = lv_image_get_src(images[0]);
    if (src == nullptr || lv_image_get_src(images[1]) != src ||
        lv_image_get_src(images[2]) != src) {
        test::fail("images with different pixels", "the same pixels in all three");
        return;
    }
    lv_obj_delete(parents[1]);
    if (!lv_obj_is_valid(images[0]) || !lv_obj_is_valid(images[2])) {
        test::fail("an image gone with another's parent", "the other two stay");
        return;
    }
    const uint32_t t0 = millis();
    while (millis() - t0 < LOAD_MS) {
        screen.render();
    }
    icon.release();
    if (lv_obj_is_valid(images[0]) || lv_obj_is_valid(images[2])) {
        test::fail("an image left after release()", "release() deletes every image");
        return;
    }
    if (!lv_obj_is_valid(parents[0]) || !lv_obj_is_valid(parents[2])) {
        test::fail("a parent deleted by release()", "the parents stay");
        return;
    }
    test::pass();
}

static volatile uint32_t waitStartUs = 0;
static uint32_t longestWaitUs = 0;
static uint32_t waits = 0;

static void onWait(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_FLUSH_WAIT_START) {
        waitStartUs = nowUs();
        return;
    }
    const uint32_t us = nowUs() - waitStartUs;
    longestWaitUs = us > longestWaitUs ? us : longestWaitUs;
    waits = waits + 1;
}

// Changes the whole screen in every pass for REDRAW_MS and measures LVGL's waits for the driver
// and the longest pass.
static bool redrawRun(const char* name, uint32_t& longestPassUs) {
    lv_obj_t* cover = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(cover);
    lv_obj_set_size(cover, media::W, media::H);
    lv_obj_set_style_bg_opa(cover, LV_OPA_50, 0);
    longestWaitUs = 0;
    waits = 0;
    longestPassUs = 0;
    uint32_t passes = 0;
    const uint32_t t0 = millis();
    while (millis() - t0 < REDRAW_MS) {
        // Two close grays, so the whole screen changes with a calm shimmer.
        lv_obj_set_style_bg_color(cover, lv_color_hex(passes & 1 ? 0x202020 : 0x282828), 0);
        const uint32_t start = nowUs();
        screen.render();
        const uint32_t us = nowUs() - start;
        longestPassUs = us > longestPassUs ? us : longestPassUs;
        passes = passes + 1;
    }
    lv_obj_delete(cover);
    Serial.printf("%s: %lu passes, longest %lu us, %lu waits, longest wait %lu us\n", name, passes,
                  longestPassUs, waits, longestWaitUs);
    if (longestWaitUs > WAIT_MAX_US) {
        snprintf(seen, sizeof seen, "%s: LVGL waited %lu us for the driver", name, longestWaitUs);
        snprintf(wanted, sizeof wanted, "at most %lu us", WAIT_MAX_US);
        test::fail(seen, wanted);
        return false;
    }
    return true;
}

static void testRedraw() {
    if (!ready()) {
        return;
    }
    static bool hooked = false;
    if (!hooked) {
        lv_display_t* d = lv_display_get_default();
        lv_display_add_event_cb(d, onWait, LV_EVENT_FLUSH_WAIT_START, nullptr);
        lv_display_add_event_cb(d, onWait, LV_EVENT_FLUSH_WAIT_FINISH, nullptr);
        hooked = true;
    }
    uint32_t alone = 0;
    if (!redrawRun("without a video", alone)) {
        return;
    }
    lv_obj_t* slide = screen.slide({1, 1});
    lv_obj_t* viewport = lv_obj_create(slide);
    lv_obj_remove_style_all(viewport);
    lv_obj_set_size(viewport, media::W, media::H);
    if (!videoPlayer.play(viewport, "loop", true, false)) {
        test::fail("play() false", "the video plays");
        return;
    }
    uint32_t withVideo = 0;
    if (!redrawRun("beside a video", withVideo)) {
        return;
    }
    test::pass();
}

// Gets whether an event is the device monitor's report about the touch controller.
static bool isTouchFault(const DriverEvent& e) {
    const bool fault =
        e.type == DriverEventType::DEVICE_FAILED || e.type == DriverEventType::DEVICE_RECOVERED;
    return fault && faultDevice(e.source) == Device::TOUCH;
}

// Renders as the main loop does until the monitor's next event about the touch controller.
static bool nextTouchFault(DriverEvent& e) {
    const uint32_t t0 = millis();
    while (millis() - t0 < LOST_TIMEOUT_MS) {
        if (events.read(&e, 1) == 1 && isTouchFault(e)) {
            return true;
        }
        screen.render();
    }
    return false;
}

static void testPowerLost() {
    if (!ready()) {
        return;
    }
    if (!touch.begin()) {
        test::fail("the touch driver's begin() false", "true");
        return;
    }
    if (!touch.present()) {
        test::skip("no touch controller answers");
        return;
    }
    if (!deviceMonitor.watch(Device::TOUCH, touch)) {
        test::fail("the monitor refused the touch driver", "it watches it");
        return;
    }
    static Picture background(screen);
    Picture* const all[] = {&background};
    background.load(files.open("background.img"));
    background.show(screen.background());
    renderWhileLoading(all, 1);
    if (background.loading() || background.failed()) {
        test::fail("background.img did not load", "it loads");
        return;
    }
    uint32_t t0 = millis();
    while (millis() - t0 < LOAD_MS) {
        screen.render();
    }
    test::step("pull the 5 V lead from the display module's Vin for about two seconds, then plug "
               "it back in");
    DriverEvent e;
    if (!nextTouchFault(e) || e.type != DriverEventType::DEVICE_FAILED) {
        test::fail("no DEVICE_FAILED for the touch controller", "DEVICE_FAILED");
        return;
    }
    if (!nextTouchFault(e) || e.type != DriverEventType::DEVICE_RECOVERED) {
        test::fail("no DEVICE_RECOVERED after the 5 V came back", "DEVICE_RECOVERED");
        return;
    }
    // What the host does on DEVICE_RECOVERED.
    GameHost::restart(screen);
    const bool heldAtOnce = display.restarting();
    t0 = millis();
    while (display.restarting() && millis() - t0 < RESTART_TIMEOUT_MS) {
        screen.render();
    }
    const uint32_t restartMs = millis() - t0;
    if (!heldAtOnce || display.restarting()) {
        snprintf(seen, sizeof seen, "restarting at once %d, still restarting after %lu ms",
                 heldAtOnce, restartMs);
        test::fail(seen, "restarting at once, and the panel back in time");
        return;
    }
    Serial.printf("the panel was back %lu ms after restart()\n", restartMs);
    // The screen sends the picture in the passes after the panel is back.
    t0 = millis();
    while (millis() - t0 < LOAD_MS) {
        screen.render();
    }
    const test::Answer a =
        test::ask("Does the screen show the test's background picture again?", ASK_TIMEOUT_MS);
    if (a != test::Answer::YES) {
        test::fail(a == test::Answer::NO ? "answered no" : "no answer", "yes");
        return;
    }
    test::pass();
}

// Creates a box of one color with its name on it.
static lv_obj_t* named(lv_obj_t* parent, const char* name, uint32_t color, int32_t x, int32_t y,
                       int32_t w, int32_t h) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_t* label = lv_label_create(o);
    lv_label_set_text(label, name);
    lv_obj_set_style_text_color(label, lv_color_black(), 0);
    lv_obj_set_pos(label, 4, 2);
    return o;
}

static void testLayers() {
    if (!ready()) {
        return;
    }
    if (!media::writeBenchMedia()) {
        test::fail("writing the bench media failed", "they reach the card");
        return;
    }
    static Picture background(screen);
    background.load(files.open("background.img"));
    background.show(screen.background());
    named(screen.background(), "background", 0x3060FF, 10, 10, 140, 90);
    lv_obj_t* slide = screen.slide(SLIDE);
    named(slide, "slide", 0xFF40FF, 40, 40, 140, 90);
    lv_obj_t* viewport = lv_obj_create(slide);
    lv_obj_remove_style_all(viewport);
    lv_obj_set_pos(viewport, 80, 60);
    lv_obj_set_size(viewport, media::VIEWPORT_W, media::VIEWPORT_H);
    named(screen.overlay(), "overlay", 0x40FFFF, 170, 100, 120, 80);
    lv_obj_t* box = lv_msgbox_create(nullptr);
    lv_msgbox_add_text(box, "message box");
    // A game switch leaves the host's objects, so the test deletes its object itself.
    lv_obj_t* host = named(GameHost::hostLayer(screen), "host", 0xFF3030, 200, 140, 110, 80);
    const bool played = videoPlayer.play(viewport, "viewport", true, false);
    test::step("watch the screen: the background picture with a blue box, a pink box, the green "
               "video with its ball, a cyan box, a message box over a darkened screen and a red "
               "box, each labeled");
    const uint32_t t0 = millis();
    while (played && millis() - t0 < SHOW_MS) {
        screen.render();
    }
    const test::Answer a =
        played ? test::ask("From the back, do they lie in this order: the picture, background, "
                           "slide, the video, overlay, the message box, host?",
                           ASK_TIMEOUT_MS)
               : test::Answer::NONE;
    videoPlayer.stop(viewport);
    lv_msgbox_close(box);
    lv_obj_delete(host);
    if (!played) {
        test::fail("play() false", "the video plays");
        return;
    }
    if (a != test::Answer::YES) {
        test::fail(a == test::Answer::NO ? "the layers lay in another order" : "no answer",
                   "the order of the layers");
        return;
    }
    test::pass();
}

static uint32_t taps = 0;
static uint32_t holds = 0;
static uint32_t presses = 0;
static uint32_t slides = 0;         // touches that came down beside the button and left on it
static TouchPoint gestureAt = {};   // where the last tap or hold came down
static TouchPoint touchDownAt = {};
static lv_area_t buttonArea = {};

static void countPress(lv_event_t*) {
    presses = presses + 1;
}

static bool onButton(TouchPoint p) {
    return p.x >= buttonArea.x1 && p.x <= buttonArea.x2 && p.y >= buttonArea.y1 &&
           p.y <= buttonArea.y2;
}

// Hands the touch events to the screen as the host does, counts what came of them and renders.
static void pumpTouches() {
    DriverEvent e;
    while (events.read(&e, 1) == 1) {
        PinballEvents out;
        if (!screen.translate(e, out)) {
            continue;
        }
        const TouchPoint at = payload<DriverEventType::TOUCH_DOWN>(e);
        if (e.type == DriverEventType::TOUCH_DOWN) {
            touchDownAt = at;
        }
        if (e.type == DriverEventType::TOUCH_UP && !onButton(touchDownAt) && onButton(at)) {
            slides = slides + 1;
        }
        for (uint8_t i = 0; i < out.count; ++i) {
            gestureAt = payload<EventType::SCREEN_TAPPED>(out.events[i]);
            taps += out.events[i].type == EventType::SCREEN_TAPPED ? 1 : 0;
            holds += out.events[i].type == EventType::SCREEN_HELD ? 1 : 0;
        }
    }
    screen.render();
}

// Shows a step and pumps the touches until it is done or STEP_TIMEOUT_MS has passed.
template <typename Done> static bool stepUntil(const char* text, Done done) {
    test::step(text);
    const uint32_t t0 = millis();
    while (!done()) {
        if (millis() - t0 > STEP_TIMEOUT_MS) {
            return false;
        }
        pumpTouches();
    }
    // The finger leaves the screen before the next step.
    while (touch.finger().down && millis() - t0 < STEP_TIMEOUT_MS) {
        pumpTouches();
    }
    pumpTouches();
    return true;
}

static void testTaps() {
    if (!ready()) {
        return;
    }
    if (!touch.begin()) {
        test::fail("the touch driver's begin() false", "true");
        return;
    }
    if (!touch.present()) {
        test::skip("no touch controller answers");
        return;
    }
    lv_obj_t* button = named(screen.slide(SLIDE), "BUTTON", 0xFF3030, 110, 90, 100, 60);
    lv_obj_set_clickable(button, true);
    lv_obj_add_event_cb(button, countPress, LV_EVENT_CLICKED, nullptr);
    lv_obj_update_layout(button);
    lv_obj_get_coords(button, &buttonArea);
    DriverEvent stale;
    while (events.read(&stale, 1) == 1) {
    }
    taps = 0;
    holds = 0;
    presses = 0;
    slides = 0;
    if (!stepUntil("tap the screen once, away from the red BUTTON", [] { return taps > 0; }) ||
        onButton(gestureAt)) {
        test::fail("no tap beside the button", "one tap");
        return;
    }
    if (!stepUntil("rest a finger away from the button for two seconds, then lift it",
                   [] { return holds > 0; })) {
        test::fail("no hold", "one hold");
        return;
    }
    if (!stepUntil("tap the red BUTTON once", [] { return presses > 0; })) {
        test::fail("no press of the button", "one press");
        return;
    }
    if (!stepUntil(
            "put a finger down beside the BUTTON, slide it onto the button and lift it there",
            [] { return slides > 0; })) {
        test::fail("no finger slid onto the button", "one");
        return;
    }
    Serial.printf("%lu taps, %lu holds, %lu presses, %lu slides onto the button\n", taps, holds,
                  presses, slides);
    if (taps != 1 || holds != 1 || presses != 1) {
        snprintf(seen, sizeof seen, "%lu taps, %lu holds, %lu presses", taps, holds, presses);
        test::fail(seen, "one tap, one hold and one press: the slide makes none");
        return;
    }
    test::pass();
}

static const Test TESTS[] = {
    {"screen.load", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testLoad, nullptr},
    {"screen.show", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testShow, nullptr},
    {"screen.redraw", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testRedraw, nullptr},
    {"screen.power-lost", RunsOn::BENCH, POWER_LIMIT_S, POWER_LIMIT_S, testPowerLost, nullptr},
    {"screen.layers", RunsOn::BENCH, BENCH_LIMIT_S, BENCH_LIMIT_S, testLayers, nullptr},
    {"screen.taps", RunsOn::BENCH, BENCH_LIMIT_S, BENCH_LIMIT_S, testTaps, nullptr},
};

extern const Program SCREEN = {TESTS, sizeof TESTS / sizeof TESTS[0]};
