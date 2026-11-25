// The display driver for the simulator, with the interface of firmware/src/drivers/display.h. It
// copies every rectangle into a screen of 320 × 240 pixels in RGB565, high byte first as the panel
// takes them, where the browser reads the picture. A rectangle goes out at once, so the screen
// never waits for the bus.
#pragma once
#include <IntervalTimer.h>
#include <stdint.h>
#include <string.h>
#include "logging/debug-log.h"

class Display {
public:
    using Sent = teensy::inplace_function<void(void), 16>;

    static constexpr uint16_t WIDTH = 320;       // the panel's pixels across in landscape
    static constexpr uint16_t HEIGHT = 240;      // its rows down
    static constexpr uint16_t STEP_PX = 16;      // the steps a rectangle's left edge and width take
    static constexpr uint32_t PIXEL_BYTES = 2;   // RGB565

    bool begin() { return true; }

    /// Ends a restart at once, since the simulated panel keeps its picture.
    void restart() {}

    bool restarting() const { return false; }

    /// Copies a rectangle into the screen and calls @p sent.
    ///
    /// @return  False, with nothing copied, for a rectangle off the 16-pixel steps, empty or beyond
    ///          the screen.
    bool send(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t* pixels,
              Sent sent) {
        if (x % STEP_PX != 0 || width % STEP_PX != 0 || width == 0 || height == 0 ||
            x + width > WIDTH || y + height > HEIGHT) {
            DEBUG_LOG("Display::send: %u x %u at %u, %u lies off the screen or its steps\n",
                      unsigned(width), unsigned(height), unsigned(x), unsigned(y));
            return false;
        }
        for (uint16_t row = 0; row < height; ++row) {
            memcpy(&screen[(size_t(y + row) * WIDTH + x) * PIXEL_BYTES],
                   pixels + size_t(row) * width * PIXEL_BYTES, size_t(width) * PIXEL_BYTES);
        }
        ++sentCount;
        sent();
        return true;
    }

    /// Gets the screen's pixels, row by row from the top left, RGB565 high byte first.
    const uint8_t* pixels() const { return screen; }

    /// Gets how many rectangles send() took since the start.
    uint32_t rectangles() const { return sentCount; }

private:
    uint8_t screen[size_t(WIDTH) * HEIGHT * PIXEL_BYTES] = {};
    uint32_t sentCount = 0;
};
