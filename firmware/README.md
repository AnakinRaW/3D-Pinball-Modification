# Firmware

The firmware runs on the Teensy 4.1. Its design stands in [`docs/firmware/`](../docs/firmware/).

| Folder | Contents |
|---|---|
| `src/` | The firmware project. |
| `tests/` | The test code. |
| [`gist/`](gist/) | Code notes for every driver and component. |

## Building

TODO: the build tool, the IDE setup, and the commands that build the firmware and flash it onto the Teensy.

The firmware builds with the newest Teensyduino release and the libraries it ships. On top of those it uses [freertos-teensy](https://github.com/tsandmann/freertos-teensy) v11.2.0_v4 with its pull request 43 and the settings in [`gist/freertos-config.h`](gist/freertos-config.h), and [WDT_T4](https://github.com/tonton81/WDT_T4) for the watchdog.

## Testing

TODO: how the firmware is tested, from the test code in `tests/` to the bench tests on the Teensy.

The bench tests stand in [`docs/firmware/testing.md`](../docs/firmware/testing.md).
