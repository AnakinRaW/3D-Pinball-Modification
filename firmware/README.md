# Firmware

The firmware is the embedded software running on the Teensy 4.1 that drives the modification.

| File | Contents |
|---|---|
| [`general-design.md`](general-design.md) | General design considerations of the firmware: its layers, the event queue, the interrupt priorities and the main program |
| [`driver-design.md`](driver-design.md) | How every driver works: its rules, the interface every driver implements, its interrupts and the driver tick |
| [`drivers/ir-sensing.md`](drivers/ir-sensing.md) | The IR ball sensing driver: its budget, sampling instant, calibration and performance reserves |
| [`drivers/break-beam.md`](drivers/break-beam.md) | The ball drain gate: the event it publishes, the block a ball has to hold, and what one block cannot tell |
| [`drivers/solenoid.md`](drivers/solenoid.md) | The solenoid driver: what it does in the interrupt, what bounds a pull-in, and what the board depends on the firmware for |
| [`drivers/lighting.md`](drivers/lighting.md) | The LED output path and the library that carries it |
| [`input-handling.md`](input-handling.md) | Description of the event queue system to read and distribute inputs to the game logic |
| [`error-handling.md`](error-handling.md) | Error handling: the device monitor that reports failed devices, and the watchdog that restarts a firmware that stopped |
| [`storage.md`](storage.md) | The storage driver: the SD card's only user, streaming the sounds and keeping every game's own files |
| [`game-abstraction.md`](game-abstraction.md) | The main file, the generic main loop, the host that runs the games, and the interface every game implements |
| [`drivers/magnetic-rotary.md`](drivers/magnetic-rotary.md) | The magnetic rotary sensor driver: how far the seal rod turned, read over I²C without waiting on the bus |
| [`components/rotating-seal.md`](components/rotating-seal.md) | The rotating seal component: the shots and full turns a game sees of the seal |
| [`drivers/controls.md`](drivers/controls.md) | The toggle switch driver: an event when the switch moves, and its position on request |
| [`drivers/servo.md`](drivers/servo.md) | The servo driver: the angle as a pulse FlexPWM1.2 repeats without an interrupt |
