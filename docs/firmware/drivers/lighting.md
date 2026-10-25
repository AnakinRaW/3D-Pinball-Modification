# Lighting, firmware

This driver controls the LED chains. The chains themselves are described in [`docs/parts/lighting/design.md`](../../parts/lighting/design.md).

## Non-blocking output

The lighting driver sends the frames with [FastLED](https://fastled.io). On the Teensy 4.1, FastLED drives all chains in parallel by DMA through its built-in ObjectFLED driver, on any set of pins. ObjectFLED times the data with channels 0 to 2 of QuadTimer4. Therefore QuadTimer4 belongs to the lighting driver, as [`general-design.md`](../general-design.md#exclusive-peripherals) records. 

Every interrupt stays enabled while a frame is being sent. `show()` returns at once.

A `show()` that comes while the previous frame is still being sent waits for it. The [lamps component](../components/lamps.md) therefore hands on frames at a fixed rate, slower than sending the longest chain takes.

FastLED also holds the color of every LED. It keeps the current within the budget the [lighting design](../../parts/lighting/design.md#current-ceiling) sets.

`begin()` hands FastLED the strip's measured currents as its power model, `setPowerModel(PowerModelRGB(...))`. At 5 V and rounded up, an LED takes 28 mW for red, 31 mW for green and 31 mW for blue, and 9 mW while it is dark. `setPowerModel` exists from FastLED 3.10.4 on.

`begin()` also makes ObjectFLED the only driver FastLED may use, with `setExclusiveDriver<fl::Bus::FLEX_IO, 0>()`. FastLED names ObjectFLED slot 0 of its `FLEX_IO` bus from 3.10.6 on. Slot 1 of that bus is FastLED's FlexIO2 driver, and FlexIO2 belongs to the [display driver](display.md).

## Device faults

The driver notes no fault. The LED chains give the Teensy no feedback, so a dead chain goes unnoticed.

## Interface

```cpp
class Lighting : public Driver {
public:
    bool begin() override;    // adds the six chains to FastLED on their pins and sets the power model and the current budget

    CRGB* chain(uint8_t c);   // gets the LEDs of chain c, 0 to 5, whose colors the next show() sends

    void show();              // sends the colors of every LED by DMA; returns at once
};
```
