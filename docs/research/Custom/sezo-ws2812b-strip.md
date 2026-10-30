# SEZO WS2812B strip

The lighting is cut from a SEZO strip of WS2812B LEDs, 2 m long and 2.7 mm wide. Each LED carries its own controller, which takes the LED's color from the data line.

## Current

The strip draws far less than the usual budget of 20 mA per color. A piece of two LEDs cut from it was measured on a bench supply at 5.1 V, with the multimeter's current range in series. FastLED set both LEDs to one color at a time, at full brightness.

| Both LEDs show | Piece | One LED, its dark draw taken off |
|---|---|---|
| Black | 3.5 mA | |
| Red | 14.6 mA | 5.55 mA |
| Green | 15.8 mA | 6.15 mA |
| Blue | 15.7 mA | 6.1 mA |
| White | 37.5 mA | 17 mA |
| Green at half its value | 9.8 mA | 3.15 mA |

The controller of an LED draws 1.75 mA, also while the LED is dark.

The three colors add up to 17.8 mA, a little more than the 17 mA white draws.

Green at half its value draws about half of what it draws at full, so the current follows the value in a straight line.

The [lighting design](../../parts/lighting/design.md#current-ceiling) budgets the strip with these currents, and the [lighting driver](../../firmware/drivers/lighting.md) hands them to FastLED as its power model.

## Sources

- The bench measurement above
- [SEZO strip listing](https://amzn.eu/d/0jcgB8mw): the length, the width and the LED density

## Test Code 
```cpp
#include <FastLED.h>

constexpr uint8_t  kPin   = 5;
constexpr uint16_t kCount = 2;

CRGB leds[kCount];

void setup() {
  FastLED.addLeds<WS2812B, kPin, GRB>(leds, kCount);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 1000);
  FastLED.setBrightness(255);
  fill_solid(leds, kCount, CRGB(0, 255, 0)); // Green
  FastLED.show();
}

void loop() { 
}
```
