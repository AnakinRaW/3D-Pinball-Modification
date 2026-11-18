# Lighting, firmware

This driver controls the LED chains. The chains themselves are described in [`docs/parts/lighting/design.md`](../../parts/lighting/design.md).

## Sending the frames

The driver sends the colors of the six LED chains, all chains at the same time. An LED reads each bit from how long the data line stays high, so a bit that comes out late or too long shows as a wrong color.

FastLED's ObjectFLED driver, the usual way to drive many chains from a Teensy 4.1, sets every edge with a DMA write to the pin. Traffic on the peripheral bus delays those writes and stretches bits. On the bench its LEDs flickered whenever the main loop read the clock. FastLED's FlexIO2 driver sends a single chain only.

The lighting therefore sends its frames through a FlexIO2 output of this repository, the library [`firmware/lib/ws2812-flexio2/`](../../../firmware/lib/ws2812-flexio2/). FlexIO2 times every bit in hardware and sends all six chains at once. Neither the processor nor other work on the Teensy can disturb a frame. The main loop never waits for one. Every frame stays under the current the machine's adapter allows.

## Non-blocking output

Every interrupt stays enabled while a frame is being sent. `show()` returns at once.

A `show()` that comes while the previous frame is still being sent returns false and sends nothing. The [lamps component](../components/lamps.md) therefore hands on frames at a fixed rate, slower than sending the longest chain takes.

## Current cap

The driver keeps the colors of every LED in one block, chain after chain. It keeps the current within the budget the [lighting design](../../parts/lighting/design.md#current-ceiling) sets. Before each frame, `show()` works out with FastLED's `calculate_max_brightness_for_power_mW()` the brightness at which the frame stays under the cap. It scales every color by that brightness while it encodes the frame.

`begin()` hands FastLED the strip's measured currents as its power model, `setPowerModel(PowerModelRGB(...))`. At 5 V and rounded up, an LED takes 28 mW for red, 31 mW for green and 31 mW for blue, and 3 mW while it is dark. `setPowerModel` exists from FastLED 3.10.4 on.

## Adapter profile

How much current the lighting may draw depends on the machine's adapter. The main task hands the adapter's rating from the [config](#config) to `configure()` before `begin()`. `begin()` selects the 6 A profile for an adapter rated at 6 A and the 3 A profile for anything less.

A profile sets which perimeter LEDs are lamps, how bright the ambient and the playfield lights may be, and the cap. The [lighting design](../../parts/lighting/design.md#current-ceiling) derives the two profiles. The [lamps component](../components/lamps.md) gets the profile with `profile()`. From it, the component takes which perimeter LEDs it uses as lamps and how bright each lamp may be.

## Config

The adapter's rated current stands in the [machine config](../components/config.md):

| Key | Default | Range |
|---|---|---|
| `lighting.adapter_a` | 3 | 3 to 6 A |

`configure()` also takes the LEDs of each chain, which the main task takes from the [lamp groups](../components/lamps.md#config) of the config. A chain without LEDs leaves its pin as it is. A chain holds at most 160 LEDs, one meter of the strip. The frame period and RAM2 both leave room for six chains of that length. `begin()` reserves memory once, during the start: a color for every LED, and the buffer FlexIO2 sends the frame from. No memory is reserved later.

## How a frame goes out

FlexIO2 is a part of the processor that shifts prepared bit patterns out on its pins, timed by its clock. Before each frame, `show()` turns every color into such a pattern. While the frame goes out, DMA hands FlexIO2 the patterns piece by piece. Every bit therefore leaves exactly on time, on all chains at once, with no code running. The library [`firmware/lib/ws2812-flexio2/`](../../../firmware/lib/ws2812-flexio2/) does the sending. The library [`firmware/lib/ws2812-color-encoder/`](../../../firmware/lib/ws2812-color-encoder/) turns the colors into patterns. FlexIO2 belongs to the lighting driver, as [`general-design.md`](../general-design.md#exclusive-peripherals) records.

A 0 is 300 ns high and a 1 is 700 ns high. The chips on the two strips are unknown, so these lie inside the windows of every WS2812B datasheet at hand, the older WS2812B revision and the [WS2812B-2020](../../datasheets/WS2812B-2020-V6-Worldsemi.pdf) datasheet.

After the longest chain the lines stay low for longer than 280 µs, after which the LEDs take over the frame.

FlexIO2 shares its 120 MHz clock with the [display's](display.md) bus. The library sets the clock only while no display runs on it. Where the clock runs at another rate, the library refuses to start.

The DMA channel that feeds FlexIO2 has the highest priority, so no other channel can delay a refill and stop a frame halfway.

## Device faults

The driver notes no fault. The LED chains give the Teensy no feedback, so a dead chain goes unnoticed.

## Limitations

Each bit the driver sends is a pulse followed by a pause on the data line. The pauses sit on the edge of what the datasheets allow: 600 ns after a 1 and 1000 ns after a 0. The level shifter can shift them by up to 45 ns, enough to land just outside.

## Interface

The driver is the library [`firmware/lib/lighting/`](../../../firmware/lib/lighting/). Its interface stands in [`lighting.h`](../../../firmware/lib/lighting/lighting.h). A bench program configures only the LEDs it has.
