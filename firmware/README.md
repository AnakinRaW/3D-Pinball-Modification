# Firmware

The firmware runs on the Teensy 4.1. Its design stands in [`docs/firmware/`](../docs/firmware/).

| Folder | Contents |
|---|---|
| `src/` | The firmware's source. |
| `include/` | The settings headers that the firmware, the test image and the bench programs share, such as the FreeRTOS settings. |
| `tests/` | The tests of the firmware's code, with every Teensy test in the test image `tests/teensy/`. |
| `bench/` | The measurement programs behind the bench figures. |
| `tools/` | Sketches that prepare the bench, such as formatting the SD card, the build's scripts, `teensy-tests.py`, which runs the Teensy tests, and `test-coverage.py`, which counts the rows of testing.md that have a test. |
| [`gist/`](gist/) | Code notes for the drivers and components not yet written as code. |

## Building

The firmware builds with [PlatformIO](https://platformio.org). [`platformio.ini`](../platformio.ini) in the repository's root pins the Teensyduino release, the libraries and their versions, and the programs in `bench/` copy it.

```
python -m platformio run               builds the debug firmware, run in the repository's root
python -m platformio run -t upload     builds it and flashes it onto the Teensy
python -m platformio run -e release    builds the firmware for the machine
python -m platformio test -e computer  runs the tests that need no hardware
python firmware/tools/tidy.py          checks the code with clang-tidy
```

The firmware links with [`linker/teensy41.ld`](linker/teensy41.ld), PJRC's linker script with two lines that keep LVGL's code in the flash. Without them LVGL takes about 200 KB more of RAM1.

The firmware defines `USE_ARDUINO_DEFINES` before it includes `arduino_freertos.h`, because FastLED and [WDT_T4](https://github.com/tonton81/WDT_T4) need Arduino's macros.

## Testing

The tests stand in [`docs/firmware/testing.md`](../docs/firmware/testing.md). The Teensy tests run on the bench from the repository's root:

```
python firmware/tools/teensy-tests.py --upload     builds and flashes the test image, then runs every Teensy test
python firmware/tools/teensy-tests.py "servo.*"    runs the servo program on the image already flashed
python firmware/tools/teensy-tests.py --soak       runs them for the durations testing.md states
python firmware/tools/teensy-tests.py --bench      adds the bench tests, for a person at the bench
python firmware/tools/test-coverage.py --missing   counts the rows of testing.md that have a test, and names the others
```
