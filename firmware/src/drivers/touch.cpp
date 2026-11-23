#include "touch.h"
#include "core/clock.h"
#include "logging/logger.h"

constexpr uint32_t TOUCH_READY_MS =
    580;   // design.md: the driver's first touch read, after power-up
/// FT6x06 register map: TD_STATUS holds the number of touch points in its lower four bits, at most
/// two, and P1_XH and P1_YH hold the upper four bits of a coordinate
constexpr uint8_t COUNT_MASK = 0x0F;
constexpr uint8_t MAX_TOUCHES = 2;
constexpr uint8_t HIGH_MASK = 0x0F;
constexpr uint8_t BYTE_BITS = 8;
constexpr int32_t PICTURE_WIDTH = 320;   // the display's landscape picture
constexpr int32_t PICTURE_HEIGHT = 240;
constexpr uint32_t DOWN_BIT = 1u << 31;
constexpr uint32_t Y_SHIFT = 16;
constexpr uint32_t Y_MASK = 0x7FFF;
constexpr uint32_t X_MASK = 0xFFFF;

static uint32_t pack(bool down, TouchPoint at) {
    return (down ? DOWN_BIT : 0) | uint32_t(at.y) << Y_SHIFT | at.x;
}

/// Gets the picture's coordinate of a reading, held inside the picture.
///
/// @param first   The reading at the picture's coordinate 0.
/// @param last    The reading at the picture's last coordinate.
/// @param raw     The reading.
/// @param pixels  The picture's pixels along the axis.
static uint16_t scaled(int32_t first, int32_t last, uint16_t raw, int32_t pixels) {
    const int32_t span = last - first;
    // Half a span rounds to the nearest pixel, with the sign the span runs in.
    const int32_t at = ((int32_t(raw) - first) * (pixels - 1) * 2 + span) / (2 * span);
    if (at < 0) {
        return 0;
    }
    return uint16_t(at < pixels ? at : pixels - 1);
}

/// Gets the larger of the two distances along the axes.
static uint16_t distance(TouchPoint a, TouchPoint b) {
    const uint16_t dx = a.x > b.x ? a.x - b.x : b.x - a.x;
    const uint16_t dy = a.y > b.y ? a.y - b.y : b.y - a.y;
    return dx > dy ? dx : dy;
}

/// Holds a reading inside 0 to RAW_MAX.
///
/// A value it moves is named in the log.
///
/// @param name   The setting's name.
static uint16_t heldReading(const char* name, uint16_t value) {
    if (value <= Touch::RAW_MAX) {
        return value;
    }
    logWarning("touch", "%s %u lies above %u", name, unsigned(value), unsigned(Touch::RAW_MAX));
    return Touch::RAW_MAX;
}

/// Gets whether two readings at opposite edges lie far enough apart to map an axis.
static bool spanned(uint16_t a, uint16_t b) {
    return (a > b ? a - b : b - a) >= Touch::SPAN_MIN;
}

void Touch::configure(const Settings& s) {
    const uint16_t left = heldReading("left_edge", s.rawLeft);
    const uint16_t right = heldReading("right_edge", s.rawRight);
    const uint16_t top = heldReading("top_edge", s.rawTop);
    const uint16_t bottom = heldReading("bottom_edge", s.rawBottom);
    held = Settings{};
    if (spanned(left, right)) {
        held.rawLeft = left;
        held.rawRight = right;
    } else {
        logWarning("touch", "left_edge %u and right_edge %u lie too close, the defaults stay",
                   unsigned(left), unsigned(right));
    }
    if (spanned(top, bottom)) {
        held.rawTop = top;
        held.rawBottom = bottom;
    } else {
        logWarning("touch", "top_edge %u and bottom_edge %u lie too close, the defaults stay",
                   unsigned(top), unsigned(bottom));
    }
}

Touch::Touch(io::I2cBus& bus, EventQueue& queue, DriverTick& ticker)
    : bus(bus), queue(queue), ticker(ticker), probe(ADDRESS, nullptr, 0, nullptr, 0, {}),
      read(ADDRESS, &TD_STATUS, 1, bytes, READ_BYTES, [this](uint32_t errors) { done(errors); }) {}

bool Touch::begin() {
    if (begun) {
        return begunOk;
    }
    begun = true;
    // The tick asks for a read only once the controller has answered.
    if (!ticker.attach(
            [this] {
                if (found) {
                    bus.request(read);
                }
            },
            TICK_US)) {
        logWarning("touch", "the driver tick refused the touch reads");
        return false;
    }
    if (!queue.attach(storage, QUEUE_DEPTH, out)) {
        logWarning("touch", "the event queue refused the touch events");
    }
    bus.attach(read);
    while (millis() < TOUCH_READY_MS) {
    }
    const uint32_t errors = bus.requestAndWait(probe);
    if (errors != 0) {
        logInfo("touch", "no touch controller answers, so the machine runs without its display");
    }
    lastGoodUs = nowUs();
    found = errors == 0;
    begunOk = true;
    return true;
}

Finger Touch::finger() const {
    const uint32_t p = packed;
    const bool down = (p & DOWN_BIT) != 0 && !elapsedMs(lastGoodUs, FAIL_MS);
    return {down, uint16_t(p & X_MASK), uint16_t(p >> Y_SHIFT & Y_MASK)};
}

Fault Touch::failed() const {
    if (!found || !elapsedMs(lastGoodUs, FAIL_MS)) {
        return {};
    }
    return {1, lastErrors};
}

void Touch::done(uint32_t errors) {
    if (errors != 0) {
        // The next tick asks for a new read. A finger the controller can no longer see leaves the
        // screen once the controller counts as failed.
        lastErrors = errors;
        const uint32_t p = packed;
        if ((p & DOWN_BIT) != 0 && elapsedMs(lastGoodUs, FAIL_MS)) {
            const TouchPoint last = {uint16_t(p & X_MASK), uint16_t(p >> Y_SHIFT & Y_MASK)};
            packed = pack(false, last);
            out->publish<DriverEventType::TOUCH_UP>(nowUs(), 0, last);
        }
        return;
    }
    lastGoodUs = nowUs();
    const uint8_t touches = bytes[0] & COUNT_MASK;
    const bool down = touches > 0 && touches <= MAX_TOUCHES;
    const uint16_t panelX = uint16_t((bytes[1] & HIGH_MASK) << BYTE_BITS | bytes[2]);
    const uint16_t panelY = uint16_t((bytes[3] & HIGH_MASK) << BYTE_BITS | bytes[4]);
    // The display's landscape setup swaps the panel's axes and runs the panel's y backward, so the
    // picture's x follows the panel's y backward, and the picture's y follows the panel's x. The
    // touch layer lies a little shifted and stretched against the picture, so each axis maps
    // between the readings measured at the picture's two edges.
    const uint16_t x = scaled(held.rawLeft, held.rawRight, panelY, PICTURE_WIDTH);
    const uint16_t y = scaled(held.rawTop, held.rawBottom, panelX, PICTURE_HEIGHT);
    report(down, {x, y});
}

void Touch::report(bool down, TouchPoint at) {
    const uint32_t now = nowUs();
    const uint32_t before = packed;
    const bool wasDown = (before & DOWN_BIT) != 0;
    if (!down) {
        if (wasDown) {
            const TouchPoint last = {uint16_t(before & X_MASK),
                                     uint16_t(before >> Y_SHIFT & Y_MASK)};
            packed = pack(false, last);
            out->publish<DriverEventType::TOUCH_UP>(now, 0, last);
        }
        return;
    }
    packed = pack(true, at);
    if (!wasDown) {
        downAt = at;
        reported = at;
        downUs = now;
        resting = true;
        out->publish<DriverEventType::TOUCH_DOWN>(now, 0, at);
        return;
    }
    if (distance(at, reported) >= SLOP_PX) {
        reported = at;
        out->publish<DriverEventType::TOUCH_MOVED>(now, 0, at);
    }
    // A finger that strays from where it came down brings no TouchHeld for this touch.
    if (resting && distance(at, downAt) >= SLOP_PX) {
        resting = false;
    }
    if (resting && elapsedMs(downUs, HOLD_MS)) {
        resting = false;
        out->publish<DriverEventType::TOUCH_HELD>(now, 0, at);
    }
}
