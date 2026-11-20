// The output stage of the lighting driver for the simulator, with the interface of
// firmware/src/drivers/ws2812-flexio2.h. It keeps each chain's last frame, dimmed by the brightness
// with the arithmetic the real stage sends, where the browser reads the colors. A frame goes out at
// once, so busy() never holds.
#pragma once
#include <Arduino.h>
#include <vector>
#include "core/debug-log.h"
#include "drivers/ws2812-color-encoder.h"

class Ws2812Flexio2 {
public:
    static constexpr uint8_t MAX_LANES = 8;       // RM FlexIO: FlexIO2 has eight shifters
    static constexpr uint8_t NO_SIGNAL = 0xFF;    // what signalOf() gets for a pin without FlexIO2
    static constexpr uint8_t BYTES_PER_LED = 3;   // red, green, blue

    struct Lane {
        uint8_t pin;
        uint16_t leds;
    };

    // Takes up to MAX_LANES lanes and keeps room for each lane's frame. A later call keeps the
    // lanes of the first. Returns false for more than MAX_LANES lanes.
    bool begin(const Lane* lanes, uint8_t count) {
        if (count > MAX_LANES) {
            DEBUG_LOG("Ws2812Flexio2::begin: %u lanes, at most %u\n", count, MAX_LANES);
            return false;
        }
        if (!allocated) {
            laneCount = count;
            for (uint8_t l = 0; l < count; ++l) {
                frames[l].assign(size_t(lanes[l].leds) * BYTES_PER_LED, 0);
            }
            allocated = true;
        }
        shown = this;
        return true;
    }

    bool busy() const { return false; }

    // The simulator takes no pins, so every pin counts as reaching FlexIO2.
    static uint8_t signalOf(uint8_t pin) { return pin; }

    // Keeps each lane's colors, red, green, blue, dimmed by the brightness. Returns false before
    // begin().
    bool send(const uint8_t* const* rgb, uint8_t brightness) {
        if (!allocated) {
            return false;
        }
        for (uint8_t l = 0; l < laneCount; ++l) {
            for (size_t i = 0; i < frames[l].size(); ++i) {
                frames[l][i] = ws2812::scale(rgb[l][i], brightness);
            }
        }
        lastBrightness = brightness;
        ++sent;
        return true;
    }

    // what the simulator reads: the stage that sent last, its frames and their count
    static inline Ws2812Flexio2* shown = nullptr;
    const std::vector<uint8_t>& frame(uint8_t lane) const {
        return frames[lane < MAX_LANES ? lane : 0];
    }
    uint8_t lanes() const { return laneCount; }
    uint8_t brightness() const { return lastBrightness; }
    uint32_t framesSent() const { return sent; }

private:
    bool allocated = false;
    uint8_t laneCount = 0;
    uint8_t lastBrightness = 255;
    uint32_t sent = 0;
    std::vector<uint8_t> frames[MAX_LANES];
};
