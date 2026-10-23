# Firmware

The firmware is the embedded software running on the Teensy 4.1 that drives the modification.

| File | Contents |
|---|---|
| [`general-design.md`](general-design.md) | General design considerations of the firmware: its layers, the event queue, the interrupt priorities, the tasks and the main program |
| [`architecture.svg`](architecture.svg) | The firmware's layers and tasks, and the path an event takes from a driver to a game |
| [`driver-design.md`](driver-design.md) | How every driver works: its rules, the interface every driver implements, its interrupts and the driver tick |
| [`input-handling.md`](input-handling.md) | Description of the event queue system to read and distribute inputs to the game logic |
| [`error-handling.md`](error-handling.md) | Error handling: the device monitor that reports failed devices, and the watchdog that restarts a firmware that stopped |
| [`logging.md`](logging.md) | The logger: the machine's log on the SD card, what belongs in it, and how a game, a component or the host writes into it |
| [`file-channel.md`](file-channel.md) | The file channel: files read ahead from the card into a ring, one after another, for the sounds |
| [`game-abstraction.md`](game-abstraction.md) | The host that runs the games, the game lifecycle, and the interface every game implements |
| [`testing.md`](testing.md) | The tests of the firmware: where each one runs, the lines a test prints, the bench, the test card, and what each test checks |
| [`drivers/`](drivers/) | One document per driver, the code that works a piece of hardware |
| [`io/`](io/) | The I/O layer beneath the drivers: the I²C buses and the port expander's pins, which drivers share |
| [`components/`](components/) | One document per component: the playfield elements and the services a game uses |
| [`firmware/gist/`](../../firmware/gist/) | Code notes for the drivers and components, which the real firmware starts from |
| [`figures.py`](figures.py) | The firmware's periods, watchdog timeout, interrupt priorities and slots, which `tools/figcheck.py` checks these documents against |
