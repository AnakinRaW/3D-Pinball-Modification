# Lighting, firmware

This driver controls the LED chains. The chains themselves are described in [`docs/parts/lighting/design.md`](../../parts/lighting/design.md).

## Non-blocking output

The lighting driver sends the frames with [FastLED](https://fastled.io). On the Teensy 4.1, FastLED drives all chains in parallel by DMA through its built-in ObjectFLED driver, on any set of pins. ObjectFLED times the data with channels 0 to 2 of QuadTimer4. Therefore QuadTimer4 belongs to the lighting driver, as [`general-design.md`](../general-design.md#exclusive-peripherals) records. 

Every interrupt stays enabled while a frame is being sent. `show()` returns at once.

A `show()` that comes while the previous frame is still being sent waits for it. The [lamps component](../components/lamps.md) therefore hands on frames at a fixed rate, slower than sending the longest chain takes.

FastLED also holds the color of every LED. It keeps the current within the budget the [lighting design](../../parts/lighting/design.md#current-ceiling) sets.

`begin()` hands FastLED the strip's measured currents as its power model, `setPowerModel(PowerModelRGB(...))`. At 5 V and rounded up, an LED takes 28 mW for red, 31 mW for green and 31 mW for blue, and 9 mW while it is dark. `setPowerModel` exists from FastLED 3.10.4 on.

`begin()` also makes ObjectFLED the only driver FastLED may use, with `setExclusiveDriver<fl::Bus::FLEX_IO, 0>()`. FastLED names ObjectFLED slot 0 of its `FLEX_IO` bus from 3.10.6 on. Slot 1 of that bus is FastLED's FlexIO2 driver, and FlexIO2 belongs to the [display driver](display.md).

## Adapter profile

How much current the lighting may draw depends on the machine's adapter. `begin()` reads the adapter's rated current in amperes from the file `/adapter.txt` on the memory card, as a whole number such as `6`. The driver caps the rating at 6 A, so any larger adapter gets the 6 A profile. A rating below 6 A, a missing file, an unreadable card or any other content gets the 3 A profile.

A profile sets which perimeter LEDs are lamps, how bright the ambient and the playfield lights may be, and the cap the driver hands FastLED. The [lighting design](../../parts/lighting/design.md#current-ceiling) derives the two profiles. The [lamps component](../components/lamps.md) gets the profile with `profile()`. From it, the component takes which perimeter LEDs it uses as lamps and how bright each lamp may be.

## Device faults

The driver notes no fault. The LED chains give the Teensy no feedback, so a dead chain goes unnoticed.

## Interface

```cpp
// what the lighting may light and how bright, for one adapter rating
struct Profile {
    uint8_t  perimeterStep;   // 2 lights every second perimeter LED, 1 lights every one
    uint8_t  ambientMax;      // the brightness the perimeters and the ceiling may reach
    uint8_t  playfieldMax;    // the brightness the playfield lights may reach
    uint32_t capMa;           // the lighting's cap, in mA at 5 V
};

class Lighting : public Driver {
public:
    // reads the adapter's rating from the card, adds the six chains to FastLED on their pins and sets the
    // power model and the cap
    bool begin() override;

    const Profile& profile() const;   // gets the profile the lighting runs with

    CRGB* chain(uint8_t c);   // gets the LEDs of chain c, 0 to 5, whose colors the next show() sends

    void show();              // sends the colors of every LED by DMA; returns at once
};
```
