// Tests on the Teensy whether the touch driver finds the touch controller and reports touches.
//
//   reads  with the module connected, the driver finds the controller and reads it without a fault
//   touch       touches on marks in the corners and the center land inside them, and a drag and a
//               rest bring the events touch.md describes (asks at the bench)
//   touch-lost  the module's 5 V pulled for a moment: the controller is reported failed and
//               recovered, and the display, restarted as the host restarts it, shows its picture
//               again (asks at the bench)
//
// Wiring: the Adafruit 2090 with its touch controller on Wire2 through the level converter, and
// its 8-bit bus, as pin-assignment.md gives them. The driver publishes into a queue this program
// reads, so no other part of the firmware takes its events.

#include <Arduino.h>
#include "core/clock.h"
#include "core/device-monitor.h"
#include "core/event-queue.h"
#include "drivers/display.h"
#include "drivers/touch.h"
#include "test-harness.h"

constexpr uint32_t READS_MS = 2000;   // a hundred reads
constexpr uint32_t STEP_TIMEOUT_MS = 30000;
constexpr uint32_t REST_MS = 2000;   // longer than Touch::HOLD_MS
constexpr uint32_t BYTES_PER_PIXEL = 2;
constexpr uint32_t W = Display::WIDTH;
constexpr uint32_t H = Display::HEIGHT;
constexpr uint32_t MARK_PX = 32;
constexpr uint16_t WHITE = 0xFFFF;
constexpr uint16_t BLACK = 0x0000;
constexpr uint32_t DONE_TIMEOUT_MS = 100;
constexpr uint32_t MIN_MOVES = 3;   // a drag across the screen moves many times SLOP_PX
constexpr uint32_t EVENTS = 64;
constexpr uint32_t LOST_TIMEOUT_MS = 60000;   // touch-lost: the time for each step at the bench
constexpr uint32_t RESTART_TIMEOUT_MS = 2000;
constexpr uint32_t ASK_TIMEOUT_MS = 60000;
constexpr uint16_t BLUE = 0x001F;
constexpr uint32_t READS_LIMIT_S = 10;
constexpr uint32_t TOUCH_LIMIT_S = 300;

struct Mark {
    const char* where;
    uint16_t x, y;   // the top left corner, on the display's 16-pixel steps
};
constexpr Mark MARKS[] = {{"top left", 16, 16},
                          {"top right", W - 16 - MARK_PX, 16},
                          {"bottom left", 16, H - 16 - MARK_PX},
                          {"bottom right", W - 16 - MARK_PX, H - 16 - MARK_PX},
                          {"center", W / 2 - MARK_PX / 2, H / 2 - MARK_PX / 2}};

static EventQueue queue;
static Touch touch(io::wire2Bus, queue, driverTick);
static Display display;
static bool begun = false;
EXTMEM static uint8_t frame[W * H * BYTES_PER_PIXEL] __attribute__((aligned(32)));
static volatile bool frameOut = false;
static DriverEvent taken[EVENTS];

// Begins the touch driver once after the start.
static bool ready() {
    if (!begun) {
        begun = touch.begin();
        if (!begun) {
            test::fail("begin() false", "true");
        }
    }
    return begun;
}

static const char* nameOf(DriverEventType t) {
    switch (t) {
    case DriverEventType::TOUCH_DOWN:
        return "TouchDown";
    case DriverEventType::TOUCH_MOVED:
        return "TouchMoved";
    case DriverEventType::TOUCH_HELD:
        return "TouchHeld";
    case DriverEventType::TOUCH_UP:
        return "TouchUp";
    default:
        return "another event";
    }
}

// ---- touch-lost

static EventQueue monitorEvents;
static DriverEvent monitorStorage[DeviceMonitor::DEPTH];
static DeviceMonitor monitor(monitorEvents, driverTick, monitorStorage);

// Fills the screen blue with a white square in the middle and sends it.
static bool showBlueSquare() {
    for (uint32_t y = 0; y < H; ++y) {
        for (uint32_t x = 0; x < W; ++x) {
            const bool square = x >= W / 2 - MARK_PX && x < W / 2 + MARK_PX &&
                                y >= H / 2 - MARK_PX && y < H / 2 + MARK_PX;
            const uint16_t c = square ? WHITE : BLUE;
            uint8_t* p = frame + (y * W + x) * BYTES_PER_PIXEL;
            p[0] = uint8_t(c >> 8);
            p[1] = uint8_t(c);
        }
    }
    frameOut = false;
    if (!display.send(0, 0, W, H, frame, [] { frameOut = true; })) {
        return false;
    }
    const uint32_t t0 = millis();
    while (!frameOut && millis() - t0 < DONE_TIMEOUT_MS) {
    }
    return frameOut;
}

// Waits for the monitor's next event about the touch controller.
static bool nextFault(DriverEvent& e) {
    const uint32_t t0 = millis();
    while (millis() - t0 < LOST_TIMEOUT_MS) {
        if (monitorEvents.read(&e, 1) == 1 && faultDevice(e.source) == Device::TOUCH) {
            return true;
        }
        delay(1);
    }
    return false;
}

static void testTouchLost() {
    char seen[96];
    if (!display.begin()) {
        test::fail("the display's begin() false", "true");
        return;
    }
    if (!ready()) {
        return;
    }
    if (!touch.present()) {
        test::skip("no touch controller answers");
        return;
    }
    if (!monitor.watch(Device::TOUCH, touch) || !showBlueSquare()) {
        test::fail("the monitor or the picture refused", "both taken");
        return;
    }
    test::step("pull the 5 V lead from the display module's Vin for about two seconds, then plug "
               "it back in");
    DriverEvent e;
    if (!nextFault(e) || e.type != DriverEventType::DEVICE_FAILED) {
        test::fail("no DEVICE_FAILED for the touch controller", "DEVICE_FAILED");
        return;
    }
    const uint32_t code = payload<DriverEventType::DEVICE_FAILED>(e);
    Serial.printf("DEVICE_FAILED with the error flags 0x%lx\n", code);
    if (!nextFault(e) || e.type != DriverEventType::DEVICE_RECOVERED) {
        test::fail("no DEVICE_RECOVERED after the 5 V came back", "DEVICE_RECOVERED");
        return;
    }
    // What the host does on DEVICE_RECOVERED: the screen restarts the display and sends its
    // picture again once the panel is back.
    display.restart();
    const uint32_t t0 = millis();
    while (display.restarting() && millis() - t0 < RESTART_TIMEOUT_MS) {
        delay(1);
    }
    const uint32_t restartMs = millis() - t0;
    if (display.restarting() || !showBlueSquare()) {
        snprintf(seen, sizeof seen, "restarting after %lu ms, or the picture refused", restartMs);
        test::fail(seen, "the panel back and the picture sent");
        return;
    }
    Serial.printf("the panel was back %lu ms after restart()\n", restartMs);
    const test::Answer a = test::ask(
        "Does the screen show blue with a white square in the middle again?", ASK_TIMEOUT_MS);
    if (a != test::Answer::YES) {
        test::fail(a == test::Answer::NO ? "answered no" : "no answer", "yes");
        return;
    }
    if (code == 0) {
        test::fail("DEVICE_FAILED with no error flags", "the flags of the failed read");
        return;
    }
    test::pass();
}

static void testReads() {
    if (!ready()) {
        return;
    }
    if (!touch.present()) {
        test::fail("present() false", "true with the module connected");
        return;
    }
    delay(READS_MS);
    const Fault f = touch.failed();
    const uint32_t n = queue.read(taken, EVENTS);
    Serial.printf("failed parts %u, code 0x%lx, %lu events\n", f.parts, f.code, n);
    if (f.parts != 0) {
        test::fail("the controller reported failed", "no failed part");
        return;
    }
    if (n != 0 || touch.finger().down) {
        test::fail("a touch", "none, nobody touches the screen");
        return;
    }
    test::pass();
}

// ---- touch

static void drawMarks() {
    for (uint32_t y = 0; y < H; ++y) {
        for (uint32_t x = 0; x < W; ++x) {
            bool on = false;
            for (const Mark& m : MARKS) {
                on = on || (x >= m.x && x < m.x + MARK_PX && y >= m.y && y < m.y + MARK_PX);
            }
            uint8_t* p = frame + (y * W + x) * BYTES_PER_PIXEL;
            const uint16_t c = on ? WHITE : BLACK;
            p[0] = uint8_t(c >> 8);
            p[1] = uint8_t(c);
        }
    }
    frameOut = false;
    if (display.send(0, 0, W, H, frame, [] { frameOut = true; })) {
        const uint32_t t0 = millis();
        while (!frameOut && millis() - t0 < DONE_TIMEOUT_MS) {
        }
    }
}

// Waits for the next touch event and gets it.
static bool nextEvent(DriverEvent& e, uint32_t timeoutMs) {
    const uint32_t t0 = millis();
    while (millis() - t0 < timeoutMs) {
        if (queue.read(&e, 1) == 1) {
            return true;
        }
        delay(1);
    }
    return false;
}

static bool expect(DriverEventType type, DriverEvent& e, const char* step) {
    char seen[96];
    if (!nextEvent(e, STEP_TIMEOUT_MS)) {
        snprintf(seen, sizeof seen, "%s: no event", step);
        test::fail(seen, nameOf(type));
        return false;
    }
    if (e.type != type) {
        snprintf(seen, sizeof seen, "%s: %s", step, nameOf(e.type));
        test::fail(seen, nameOf(type));
        return false;
    }
    return true;
}

static void testTouch() {
    char seen[96];
    char wanted[64];
    char text[96];
    if (!display.begin()) {
        test::fail("the display's begin() false", "true");
        return;
    }
    if (!ready()) {
        return;
    }
    if (!touch.present()) {
        test::skip("no touch controller answers");
        return;
    }
    drawMarks();
    queue.read(taken, EVENTS);
    DriverEvent e;
    for (const Mark& m : MARKS) {
        snprintf(text, sizeof text, "touch the white mark at the %s, then lift the finger",
                 m.where);
        test::step(text);
        if (!expect(DriverEventType::TOUCH_DOWN, e, m.where)) {
            return;
        }
        const TouchPoint at = payload<DriverEventType::TOUCH_DOWN>(e);
        Serial.printf("%s: TouchDown at %u, %u\n", m.where, at.x, at.y);
        if (at.x + Touch::SLOP_PX < m.x || at.x >= m.x + MARK_PX + Touch::SLOP_PX ||
            at.y + Touch::SLOP_PX < m.y || at.y >= m.y + MARK_PX + Touch::SLOP_PX) {
            snprintf(seen, sizeof seen, "%s: %u, %u", m.where, at.x, at.y);
            snprintf(wanted, sizeof wanted, "within %u px of %u, %u to %u, %u", Touch::SLOP_PX, m.x,
                     m.y, unsigned(m.x + MARK_PX - 1), unsigned(m.y + MARK_PX - 1));
            test::fail(seen, wanted);
            return;
        }
        // TouchMoved may come while the finger rolls on the mark.
        do {
            if (!nextEvent(e, STEP_TIMEOUT_MS)) {
                test::fail("no TouchUp", "TouchUp");
                return;
            }
        } while (e.type == DriverEventType::TOUCH_MOVED);
        if (e.type != DriverEventType::TOUCH_UP) {
            snprintf(seen, sizeof seen, "%s after the touch", nameOf(e.type));
            test::fail(seen, "TouchUp");
            return;
        }
    }

    test::step("drag a finger from the left edge to the right edge, then lift it");
    if (!expect(DriverEventType::TOUCH_DOWN, e, "drag")) {
        return;
    }
    uint32_t moves = 0;
    for (;;) {
        if (!nextEvent(e, STEP_TIMEOUT_MS)) {
            test::fail("drag: no TouchUp", "TouchUp");
            return;
        }
        if (e.type == DriverEventType::TOUCH_MOVED) {
            ++moves;
        } else {
            break;
        }
    }
    if (e.type != DriverEventType::TOUCH_UP || moves < MIN_MOVES) {
        snprintf(seen, sizeof seen, "drag: %lu TouchMoved, then %s", moves, nameOf(e.type));
        snprintf(wanted, sizeof wanted, "%lu TouchMoved at least, then TouchUp", MIN_MOVES);
        test::fail(seen, wanted);
        return;
    }

    snprintf(text, sizeof text, "rest a finger on the center mark for %lu s, then lift it",
             REST_MS / 1000);
    test::step(text);
    if (!expect(DriverEventType::TOUCH_DOWN, e, "rest")) {
        return;
    }
    const uint32_t downUs = e.time;
    do {
        if (!nextEvent(e, STEP_TIMEOUT_MS)) {
            test::fail("rest: no TouchHeld", "TouchHeld");
            return;
        }
    } while (e.type == DriverEventType::TOUCH_MOVED);
    if (e.type != DriverEventType::TOUCH_HELD) {
        snprintf(seen, sizeof seen, "rest: %s", nameOf(e.type));
        test::fail(seen, "TouchHeld");
        return;
    }
    const uint32_t heldMs = (e.time - downUs) / 1000;
    do {
        if (!nextEvent(e, STEP_TIMEOUT_MS)) {
            test::fail("rest: no TouchUp", "TouchUp");
            return;
        }
    } while (e.type == DriverEventType::TOUCH_MOVED);
    Serial.printf("TouchHeld %lu ms after TouchDown\n", heldMs);
    if (e.type != DriverEventType::TOUCH_UP || heldMs < Touch::HOLD_MS ||
        heldMs > Touch::HOLD_MS + Touch::TICK_US / 1000) {
        snprintf(seen, sizeof seen, "TouchHeld after %lu ms, then %s", heldMs, nameOf(e.type));
        snprintf(wanted, sizeof wanted, "after %lu ms, then TouchUp", Touch::HOLD_MS);
        test::fail(seen, wanted);
        return;
    }
    test::pass();
}

static const Test TESTS[] = {
    {"touch.touch", RunsOn::BENCH, TOUCH_LIMIT_S, TOUCH_LIMIT_S, testTouch, nullptr},
    {"touch.touch-lost", RunsOn::BENCH, TOUCH_LIMIT_S, TOUCH_LIMIT_S, testTouchLost, nullptr},
    {"touch.reads", RunsOn::TEENSY, READS_LIMIT_S, READS_LIMIT_S, testReads, nullptr},
};

extern const Program TOUCH_PROGRAM = {TESTS, sizeof TESTS / sizeof TESTS[0]};
