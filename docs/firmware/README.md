# Firmware

The firmware for the Teensy 4.1 that drives the modification.

| Location | Contents |
|---|---|
| [`general-design.md`](general-design.md), [`architecture.svg`](architecture.svg) | The firmware's layers, interrupt priorities, tasks and main program |
| [`driver-design.md`](driver-design.md), [`input-handling.md`](input-handling.md), [`error-handling.md`](error-handling.md) | The rules every driver follows, the event queue, device faults and the watchdog |
| [`logging.md`](logging.md), [`file-channel.md`](file-channel.md), [`testing.md`](testing.md) | Logging, the file channel, the tests |
| [`game-abstraction.md`](game-abstraction.md) | The host that runs the games and the interface of a game |
| [`drivers/`](drivers/), [`io/`](io/), [`components/`](components/) | One document per driver, the I/O layer beneath the drivers, one document per component |
| [`firmware/gist/`](../../firmware/gist/) | Code notes the real firmware starts from |
