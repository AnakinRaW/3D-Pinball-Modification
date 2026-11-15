// LED test for the Teensy 4.1: five WS2812B on chain A, pin 5, lit in five colors of the rainbow
// that drift slowly along the chain.
//
// FastLED drives the chain the way firmware/gist/drivers/lighting.cpp does: ObjectFLED as the only
// driver, WS2812B, color order GRB. The data line runs through the lighting board's level
// shifter, as docs/parts/lighting/design.md draws it, since the LEDs need at least 3.5 V for a
// high bit and a Teensy pin gives 3.3 V.
//
// The brightness stays at 64 of 255, which keeps five LEDs well under 100 mA.

#include <Arduino.h>
#include <FastLED.h>

constexpr uint8_t PIN = 5;              // pin-assignment.md: LED chain A
constexpr uint8_t COUNT = 5;
constexpr uint8_t BRIGHTNESS = 64;
constexpr uint8_t HUE_STEP = 256 / COUNT;   // five colors spread over the color wheel
constexpr uint32_t FRAME_MS = 20;

static CRGB leds[COUNT];

void setup() {
    Serial.begin(115200);
    FastLED.setExclusiveDriver<fl::Bus::FLEX_IO, 0>();   // ObjectFLED, as the lighting driver
    FastLED.addLeds<WS2812B, PIN, GRB>(leds, COUNT);
    FastLED.setBrightness(BRIGHTNESS);
    Serial.println("LED test: 5 LEDs on pin 5, rainbow");
}

void loop() {
    static uint8_t start = 0;
    fill_rainbow(leds, COUNT, start, HUE_STEP);
    FastLED.show();
    start = start + 1;
    delay(FRAME_MS);
}
