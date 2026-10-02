# Lighting, firmware

The driver here is the output path for the LED chains. The chains themselves are in [`docs/parts/lighting/design.md`](../../docs/parts/lighting/design.md).

## The frame time has to fit the loop

A clockless strip's frame time grows with the number of LEDs, and past some strip length it exceeds the dwell that [`general-design.md`](../general-design.md) holds every participant in the loop to. Compute it from the LED count in the lighting design and hold it against the dwell before a strip is chosen.

Which library decides whether that time is blocked or not:

| Library | Behaviour |
|---|---|
| [OctoWS2811](https://www.pjrc.com/teensy/td_libs_OctoWS2811.html) | DMA, near-zero CPU, interrupts stay enabled. **Use this** |
| [WS2812Serial](https://www.pjrc.com/non-blocking-ws2812-led-library/) | Also non-blocking, PJRC's own |
| FastLED | Its built-in clockless output is timed in software, and other interrupts disrupt it. Over `USE_OCTOWS2811` it hands the transfer to OctoWS2811 instead |
| Adafruit NeoPixel | Disables all interrupts for the whole frame. **Do not use** |

**FastLED is the frontend, OctoWS2811 the output engine.** FastLED holds the pixel array, the effects and the power limit, and OctoWS2811 does the DMA transfer. OctoWS2811 times that transfer with channels 0 to 2 of QuadTimer4, so QuadTimer4 belongs to the lighting driver, as [`general-design.md`](../general-design.md#peripherals-a-driver-owns) records.

## Device faults

The driver notes no fault. The LED chains give the Teensy no feedback, so a dead chain goes unnoticed.
