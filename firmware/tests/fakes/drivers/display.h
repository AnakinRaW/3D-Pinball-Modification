// The display driver of firmware/src/drivers/display.h for the tests that run on the computer. It
// records every rectangle it gets and copies its pixels into a screen in memory. The rectangle's
// function runs at once, or once the test calls finish() when the test holds the bus. A restart
// lasts until the test sets panelBack.
#pragma once
#include <IntervalTimer.h>
#include <stdint.h>
#include <string.h>
#include <vector>

class Display {
public:
    using Sent = teensy::inplace_function<void(void), 16>;

    static constexpr uint16_t WIDTH = 320;
    static constexpr uint16_t HEIGHT = 240;
    static constexpr uint16_t STEP_PX = 16;

    struct Rect {
        uint16_t x, y, width, height;
    };

    bool begin() { return true; }

    void restart() {
        ++restarts;
        panelBack = false;
    }

    bool restarting() const { return !panelBack; }

    bool send(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t* pixels,
              Sent done) {
        if (busy || x % STEP_PX != 0 || width % STEP_PX != 0 || width == 0 || height == 0 ||
            x + width > WIDTH || y + height > HEIGHT) {
            ++refused;
            return false;
        }
        rects.push_back({x, y, width, height});
        for (uint16_t row = 0; row < height; ++row) {
            memcpy(&screen[(size_t(y + row) * WIDTH + x) * 2], pixels + size_t(row) * width * 2,
                   size_t(width) * 2);
        }
        if (hold) {
            busy = true;
            pending = done;
        } else {
            done();
        }
        return true;
    }

    /// Ends the rectangle a held bus carries.
    void finish() {
        if (busy) {
            busy = false;
            pending();
        }
    }

    /// Gets the pixel at @p x, @p y as RGB565.
    uint16_t pixel(uint16_t x, uint16_t y) const {
        const size_t at = (size_t(y) * WIDTH + x) * 2;
        return uint16_t(screen[at] << 8 | screen[at + 1]);
    }

    std::vector<Rect> rects;   // every rectangle sent, in order
    uint32_t refused = 0;      // the rectangles send() refused
    bool hold = false;         // the test ends each rectangle with finish()
    uint32_t restarts = 0;     // the calls of restart()
    bool panelBack = true;     // false from restart() on, until the test ends the restart

private:
    std::vector<uint8_t> screen = std::vector<uint8_t>(size_t(WIDTH) * HEIGHT * 2);
    bool busy = false;
    Sent pending;
};
