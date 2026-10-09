# Firmware

The firmware is the embedded software running on the Teensy 4.1 that drives the modification.

| File | Contents |
|---|---|
| [`general-design.md`](general-design.md) | General design considerations of the firmware: its layers, the event queue, the interrupt priorities, the tasks and the main program |
| [`driver-design.md`](driver-design.md) | How every driver works: its rules, the interface every driver implements, its interrupts and the driver tick |
| [`drivers/ir-sensing.md`](drivers/ir-sensing.md) | The IR ball sensing driver: its budget, sampling instant, calibration and performance reserves |
| [`drivers/break-beam.md`](drivers/break-beam.md) | The ball drain gate: the event it publishes, the block a ball has to hold, and what one block cannot tell |
| [`drivers/solenoid.md`](drivers/solenoid.md) | The solenoid driver: what it does in the interrupt, what bounds a pull-in, and what the board depends on the firmware for |
| [`drivers/lighting.md`](drivers/lighting.md) | The LED output path and the library that carries it |
| [`input-handling.md`](input-handling.md) | Description of the event queue system to read and distribute inputs to the game logic |
| [`error-handling.md`](error-handling.md) | Error handling: the device monitor that reports failed devices, and the watchdog that restarts a firmware that stopped |
| [`file-channel.md`](file-channel.md) | The file channel: files read ahead from the card into a ring, one after another, for the sounds and the movies |
| [`game-abstraction.md`](game-abstraction.md) | The host that runs the games, the game lifecycle, and the interface every game implements |
| [`drivers/`](drivers/) | One document per driver, the code that works a piece of hardware |
| [`components/`](components/) | One document per component: the playfield elements and the services a game uses |
| [`firmware/gist/`](../../firmware/gist/) | Code notes for the drivers and components, which the real firmware starts from |
| [`figures.py`](figures.py) | The firmware's periods, watchdog timeout, interrupt priorities and slots, which `tools/figcheck.py` checks these documents against |
