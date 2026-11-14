# Firmware

The firmware runs on the Teensy 4.1. Its design stands in [`docs/firmware/`](../docs/firmware/).

| Folder | Contents |
|---|---|
| `src/` | The firmware project. |
| `tests/` | The test code. |
| [`gist/`](gist/) | Code notes for every driver and component. |

## Building

The firmware builds with [PlatformIO](https://platformio.org). [`tests/stack/platformio.ini`](tests/stack/platformio.ini) is the template for a project. It pins the Teensyduino release, the libraries and their versions.

```
python -m platformio run               builds the project in the current folder
python -m platformio run -t upload     builds it and flashes it onto the Teensy
```

The template links with [`linker/teensy41.ld`](linker/teensy41.ld), PJRC's linker script with two lines that keep LVGL's code in the flash. Without them LVGL takes about 200 KB more of RAM1.

The firmware defines `USE_ARDUINO_DEFINES` before it includes `arduino_freertos.h`, because FastLED and [WDT_T4](https://github.com/tonton81/WDT_T4) need Arduino's macros.

## Testing

TODO: how the firmware is tested, from the test code in `tests/` to the bench tests on the Teensy.

The bench tests stand in [`docs/firmware/testing.md`](../docs/firmware/testing.md).
