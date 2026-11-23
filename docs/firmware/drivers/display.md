# Display driver

The driver puts pictures on the display that [`docs/parts/display/design.md`](../../parts/display/design.md) describes. The [screen](../components/screen.md) component builds on it.

## Sending

`send()` sends a rectangle of pixels to the display and returns at once. The pixels lie row by row in the caller's buffer. Each pixel is two bytes of RGB565, high byte first. The display keeps showing what it received last, so a caller sends only what changed.

The rectangle's left edge and its width go in steps of 16 pixels, since the driver fills the bus 16 pixels at a time. `send()` refuses a rectangle off those steps or beyond the screen and returns false. It also refuses while a rectangle is still on its way.

The caller hands `send()` a function. The driver calls it from its interrupt once the last pixel has left the bus. Until then the buffer has to stay as it is.

The driver sends a rectangle column by column, as the panel builds its picture. A whole screen then [tears](../../parts/display/design.md#limitations) in only 1.2 % of the screens sent. The pixels have to lie in RAM1 or RAM2, since the PSRAM serves reads down the columns too slowly.

## Start-up

`begin()` waits until 285 ms after power-up, as [`design.md`](../../parts/display/design.md#appendix-derivations) derives. It then resets the panel by command, since a restart of the Teensy can find the panel awake. It sends the power, frame rate and gamma setup that the ILI9341 libraries send, sets the panel to RGB565 in landscape and ends with a black screen.

FlexIO3 shares its clock with FlexIO2, which the [lighting](lighting.md) drives at the same 120 MHz. `begin()` returns false when FlexIO2 runs that clock at another rate.

## Setting FlexIO3 up

The driver sets FlexIO3 up anew before each run of commands and each run of pixels, starting with FlexIO3's software reset. It holds the reset for 1 µs and waits 1 µs after it. Without the waits FlexIO3 dropped the first register write after the reset on the bench. A restart could then leave the panel asleep, with a white screen.

## Performance & Memory Usage

Sending the whole screen once takes 12.8 ms.

While the driver sends, it takes at most 16.0 % of the processor, as measured on the bench.

The driver needs no memory for pixels. It sends straight from the two buffers LVGL draws into, which the [screen component](../components/screen.md#drawing) keeps in RAM2.

## Driver events

The driver publishes no events.

## Device faults

The panel never answers, since its read line RD is tied high. The driver therefore cannot tell whether the panel works and reports no fault.

The panel shares the module's supervisor with the touch controller. If the module's 3.3 V falls below 3 V, the supervisor resets both. The panel then comes back asleep with its display off. The [touch driver](touch.md#device-faults) reports the controller failed while it is silent and recovered once it answers again. The host then calls the [screen](../components/screen.md)'s `restart()`, which calls the driver's. The driver sends the panel the same setup as `begin()`. Until the panel is back, `restarting()` is true. The screen runs no LVGL meanwhile. Then it sends the whole picture again. While the panel waits during the setup, the processor goes on with other work.

## Existing libraries

[ILI9341_t3](https://github.com/PaulStoffregen/ILI9341_t3) and [ILI9341_T4](https://github.com/vindar/ILI9341_T4) drive the ILI9341 over SPI. This build wires the panel to the 8-bit bus.

## Driver

[`firmware/src/drivers/display.h`](../../../firmware/src/drivers/display.h) holds the interface.
