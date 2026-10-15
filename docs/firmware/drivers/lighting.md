# Lighting, firmware

This driver controls the LED chains. The chains themselves are described in [`docs/parts/lighting/design.md`](../../parts/lighting/design.md).

## Non-blocking output

The lighting driver sends the frames with [FastLED](https://fastled.io). On the Teensy 4.1, FastLED drives all chains in parallel by DMA through its built-in ObjectFLED driver, on any set of pins. ObjectFLED times the data with channels 0 to 2 of QuadTimer4. Therefore QuadTimer4 belongs to the lighting driver, as [`general-design.md`](../general-design.md#exclusive-peripherals) records. 

Every interrupt stays enabled while a frame is being sent. `show()` returns at once.

A `show()` that comes while the previous frame is still being sent waits for it. The [lamps component](../components/lamps.md) therefore hands on frames at a fixed rate, slower than sending the longest chain takes.

FastLED also holds the color of every LED. It keeps the current within the budget the [lighting design](../../parts/lighting/design.md#current-ceiling) sets.

## Device faults

The driver notes no fault. The LED chains give the Teensy no feedback, so a dead chain goes unnoticed.

## Interface

```cpp
class Lighting : public Driver {
public:
    bool begin() override;    // adds the six chains to FastLED on their pins and sets the current budget

    CRGB* chain(uint8_t c);   // gets the LEDs of chain c, 0 to 5, whose colors the next show() sends

    void show();              // sends the colors of every LED by DMA; returns at once
};
```
