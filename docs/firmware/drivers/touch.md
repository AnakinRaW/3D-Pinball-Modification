# Touch driver

The driver reads the touch panel of the [display](../../parts/display/design.md).

## Start-up

The touch controller sits on `Wire2`, which an [I²C bus](../io/i2c-bus.md) runs.

The controller answers no earlier than 580 ms after power-up, so `begin()` waits until then. It then checks once through the bus whether the controller answers at its address. `present()` reports the result. The display and the touch controller sit on one module, so the answer also tells whether the machine has its display.

Without the display module the driver publishes no touch.

The main task starts the driver after the display driver, so the black screen does not wait for the controller.

## Reading

A read asks the controller how many fingers touch the screen and where the first one touches. The answer is five bytes.

Every 20 ms the driver tick requests a read from the bus. When the read has ended, the bus calls the driver back with the five bytes.

The controller reports positions in the panel's portrait orientation. The driver turns them into the picture's landscape orientation, the same way the [display driver](display.md#start-up) turns the panel's memory at start-up. With the panel set to landscape by the memory access value 0x28, the axes swap. The picture's x follows the panel's y backward, and the picture's y follows the panel's x.

The touch layer lies shifted and stretched against the picture by a few pixels, and differently on every module. The driver maps each axis linearly between the controller's readings at the picture's two edges, which the [config](#config) holds. A position beyond an edge lands on that edge.

The driver publishes the four touch events below, and `finger()` gives the same position whenever a component asks.

## Config

The four readings at the picture's edges stand in the [machine config](../components/config.md). Their defaults were measured on the bench with this build's module:

| Key | Default | Range |
|---|---|---|
| `touch.left_edge` | 327 | 0 to 4095, the panel's y at the picture's left edge |
| `touch.right_edge` | 1 | 0 to 4095, the panel's y at the picture's right edge |
| `touch.top_edge` | 3 | 0 to 4095, the panel's x at the picture's top edge |
| `touch.bottom_edge` | 254 | 0 to 4095, the panel's x at the picture's bottom edge |

The main task hands the four values to `configure()` before `begin()`. A value above 4095 counts as 4095. An axis whose two readings lie less than 100 apart keeps its defaults.

## Driver events

| Type | When | Payload |
|---|---|---|
| `TouchDown` | A finger touches the screen | x and y |
| `TouchMoved` | The finger has moved 10 pixels or more along either axis from the position last reported | x and y |
| `TouchHeld` | The finger has rested within 10 pixels of where it touched down for 1 s | x and y |
| `TouchUp` | The finger has left the screen | x and y, where it was last |

A payload holds x in its lower 16 bits and y in its upper 16, 0 at the top left. The source is always `0` in this build. The timestamp is `nowUs()` at the end of the read that saw the touch. A full queue drops the event.

## Device faults

A controller that did not answer at `begin()` means the machine runs without its display module. This is not a fault.

A controller that answered at `begin()` counts as [failed](../error-handling.md#device-faults) once it has answered no read for 100 ms. A read gets an answer whether or not a finger touches the screen. The error code holds the LPI2C4 error flags of the last failed read. A failed controller counts as a failed display.

A failed read is dropped. The next tick requests a new one.

A finger that touches the screen when the controller fails leaves it with `TouchUp` at the first failed read past 100 ms, and `finger()` then shows no finger.

## Existing libraries

The driver reads the controller through the I²C bus, since [Adafruit_FT6206](https://github.com/adafruit/Adafruit_FT6206_Library) waits in `Wire` until each read has ended. At 100 kHz its read costs 1.74 ms of waiting, 8.7 % of the processor's time at one read every 20 ms.

## Driver

[`firmware/src/drivers/touch.h`](../../../firmware/src/drivers/touch.h) holds the interface.
