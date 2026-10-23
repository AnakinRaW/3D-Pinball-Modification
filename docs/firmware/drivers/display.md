# Display driver

The driver puts pictures on the display that [`docs/parts/display/design.md`](../../parts/display/design.md) describes. The [screen](../components/screen.md) component builds on it.

## Sending

`send()` sends a rectangle of pixels to the display and returns at once. The pixels lie row by row in the caller's buffer. Each pixel is two bytes of RGB565, high byte first. The display keeps showing what it received last, so a caller sends only what changed.

The rectangle's left edge and its width go in steps of 16 pixels, since the driver fills the bus 16 pixels at a time. `send()` refuses a rectangle off those steps or beyond the screen and returns false. It also refuses while a rectangle is still on its way.

The caller hands `send()` a function. The driver calls it from its interrupt once the last pixel has left the bus. Until then the buffer has to stay as it is.

## Start-up

`begin()` waits until 285 ms after power-up, as [`design.md`](../../parts/display/design.md#appendix-derivations) derives. It then resets the panel by command, since a restart of the Teensy can find the panel awake. It sets the panel to RGB565 in landscape and ends with a black screen.

## Performance & Memory Usage

Sending the whole screen once takes 12.8 ms.

While the driver sends, it takes an estimated 9.4 % of the processor.

The driver needs no memory for pixels. It sends straight from the two buffers LVGL draws into, which the [screen component](../components/screen.md#drawing) keeps in RAM2.

## Driver events

The driver publishes no events.

## Device faults

The panel never answers, since its read line RD is tied high. The driver therefore cannot tell whether the panel works and reports no fault.

The panel shares the module's supervisor with the touch controller. If the module's 3.3 V falls below 3 V, the supervisor resets both. The panel then comes back asleep with its display off. The [touch driver](touch.md#device-faults) reports the controller failed while it is silent and recovered once it answers again. The host then calls the [screen](../components/screen.md)'s `restart()`, which calls the driver's. The driver sends the panel the same setup as `begin()`. Until the panel is back, `restarting()` is true. The screen runs no LVGL meanwhile. Then it sends the whole picture again. While the panel waits during the setup, the processor goes on with other work.

## Existing libraries

[ILI9341_t3](https://github.com/PaulStoffregen/ILI9341_t3) and [ILI9341_T4](https://github.com/vindar/ILI9341_T4) drive the ILI9341 over SPI. This build wires the panel to the 8-bit bus.

## Driver

```cpp
class Display : public Driver {
public:
    using Sent = teensy::inplace_function<void(void), 16>;   // a lambda capturing a pointer fits

    // sets up FlexIO3 and the panel, leaves the screen black and attaches to the driver tick;
    // false when the tick is full
    bool begin() override;
    void restart();                                          // sets the panel up again after a supervisor reset, holding rectangles back until it is; returns at once
    bool restarting() const;                                 // gets whether a restart still holds the bus

    // sends the rectangle at x, y from pixels, RGB565 high byte first, and returns at once; calls
    // sent from the interrupt once the last pixel is out; false for a rectangle off the 16-pixel
    // steps, beyond the screen, or while one is on its way
    bool send(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t* pixels, Sent sent);
};
```
