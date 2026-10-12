# Touch driver

The driver reads the touch panel of the [display](../../parts/display/design.md).

## Start-up

The touch controller sits on `Wire2`.

`begin()` lets `Wire2` set up SDA and SCL, their pull-ups and a bus clock of 100 kHz. The controller answers no earlier than 580 ms after power-up, so `begin()` waits until then. It then checks once whether the controller answers at its address. `present()` reports the result. 

From then on the driver programs the LPI2C4 I²C controller itself and no longer calls `Wire2`, because the `Wire` library blocks the caller. 

Without a touch controller the driver publishes no touch.

The main task starts the driver after the display driver, so the black screen does not wait for the controller.

## Reading

A read asks the controller how many fingers touch the screen and where the first one touches. The answer is five bytes. 

Every 20 ms the driver tick evaluates the last answer and starts the next read. During the read, a DMA channel writes the five bytes directly into the driver's read buffer. 

The controller reports positions in the panel's portrait orientation. The driver turns them into the picture's landscape orientation, the same way the [display driver](display.md#start-up) turns the panel's memory at start-up. It publishes the four touch events below, and `finger()` gives the same position whenever a component asks.

## Driver events

| Type | When | Payload |
|---|---|---|
| `TouchDown` | A finger touches the screen | x and y |
| `TouchMoved` | The finger has moved 10 pixels or more from the position last reported | x and y |
| `TouchHeld` | The finger has rested within 10 pixels of where it touched down for 1 s | x and y |
| `TouchUp` | The finger has left the screen | x and y, where it was last |

A payload holds x in its lower 16 bits and y in its upper 16, 0 at the top left. The source is always `0` in this build. The timestamp is `nowUs()` in the driver tick that read the touch. A full queue drops the event.

## Device faults

A controller that did not answer at `begin()` means this machine does not use touch input. This is not a fault.

A controller that answered at `begin()` counts as [failed](../error-handling.md#device-faults) once it has answered no read for 1 s. A read gets an answer whether or not a finger touches the screen. The error code holds the LPI2C4 error flags of the last failed read. 

A failed read is retried at once. When the controller blocks the bus, the driver sends clock pulses until the controller releases it.

## Existing libraries

The driver drives the I²C controller itself, since [Adafruit_FT6206](https://github.com/adafruit/Adafruit_FT6206_Library) waits in `Wire` until each read has ended. At 100 kHz its read costs 1.74 ms of waiting, 8.7 % of the processor's time at one read every 20 ms.

## Driver

```cpp
// where a finger touches the screen, in the picture's pixels
struct Finger {
    bool     down;              // a finger touches the screen
    uint16_t x;                 // 0 at the left edge
    uint16_t y;                 // 0 at the top edge
};

class Touch : public Driver {
public:
    // sets up Wire2, checks once whether the touch controller answers, attaches to the driver tick
    // and registers with the device monitor; false when one of the two is full
    bool begin() override;

    bool   present() const;     // gets whether the machine has a touch controller, which begin() finds out once
    Finger finger() const;      // gets where a finger touches the screen

    // gets which parts have failed, bit 0 once the controller that answered at begin() has brought
    // no read for 1 s, with the LPI2C4 error flags of the last failed read as the error code
    Fault failed() const override;
};
```
