// LED test for the Teensy 4.1: five WS2812B on chain A, lit in five colors of the rainbow that
// drift slowly along the chain.
//
// The lighting driver of firmware/src/drivers/lighting.h sends the chain, as the firmware does.
// The data line runs through the lighting board's level shifter, as docs/parts/lighting/design.md
// draws it, since the LEDs need at least 3.5 V for a high bit and a Teensy pin gives 3.3 V.
//
// The colors stay at 64 of 255, which keeps five LEDs well under 100 mA.

#include <Arduino.h>
#include "drivers/lighting.h"

constexpr uint8_t CHAIN_A = 0;
constexpr uint8_t COUNT = 5;
constexpr uint8_t BRIGHTNESS = 64;
constexpr uint8_t HUE_STEP = 256 / COUNT;   // five colors spread over the color wheel
constexpr uint32_t FRAME_MS = 20;

static Lighting lighting;

void setup() {
    Serial.begin(115200);
    Lighting::Settings bench;
    bench.leds[CHAIN_A] = COUNT;
    lighting.configure(bench);
    if (!lighting.begin()) {
        Serial.println("LED test: the lighting driver did not start");
        return;
    }
    Serial.println("LED test: 5 LEDs on chain A, rainbow");
}

void loop() {
    static uint8_t start = 0;
    CRGB* leds = lighting.chain(CHAIN_A);
    if (leds == nullptr) {
        return;
    }
    fill_rainbow(leds, COUNT, start, HUE_STEP);
    nscale8_video(leds, COUNT, BRIGHTNESS);
    lighting.show();
    start = start + 1;
    delay(FRAME_MS);
}
