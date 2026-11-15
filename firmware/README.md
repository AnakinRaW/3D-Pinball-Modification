# Firmware

The firmware runs on the Teensy 4.1. Its design stands in [`docs/firmware/`](../docs/firmware/).

| Folder | Contents |
|---|---|
| `src/` | The firmware's source. |
| `tests/` | The tests of the firmware's code. |
| `bench/` | The measurement programs behind the bench figures. |
| `tools/` | Sketches that prepare the bench, such as formatting the SD card, and the build's scripts. |
| [`gist/`](gist/) | Code notes for every driver and component. |

## Building

The firmware builds with [PlatformIO](https://platformio.org). [`platformio.ini`](../platformio.ini) in the repository's root pins the Teensyduino release, the libraries and their versions, and the programs in `bench/` copy it.

```
python -m platformio run               builds the debug firmware, run in the repository's root
python -m platformio run -t upload     builds it and flashes it onto the Teensy
python -m platformio run -e release    builds the firmware for the machine
python -m platformio test -e native    runs the tests that need no hardware
```

The firmware links with [`linker/teensy41.ld`](linker/teensy41.ld), PJRC's linker script with two lines that keep LVGL's code in the flash. Without them LVGL takes about 200 KB more of RAM1.

The firmware defines `USE_ARDUINO_DEFINES` before it includes `arduino_freertos.h`, because FastLED and [WDT_T4](https://github.com/tonton81/WDT_T4) need Arduino's macros.

## Testing

TODO: how the firmware is tested, from the test code in `tests/` to the bench tests on the Teensy.

The bench tests stand in [`docs/firmware/testing.md`](../docs/firmware/testing.md).
