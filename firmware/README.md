# Firmware

The firmware is the embedded software running on the Teensy 4.1 that drives the modification.

| File | Contents |
|---|---|
| [`general-design.md`](general-design.md) | General design considerations of the firmware, such as driver design and interrupt rules |
| [`ir-sensing.md`](ir-sensing.md) | The IR ball sensing driver: its budget, sampling instant, calibration and performance reserves |
| [`break-beam.md`](break-beam.md) | The ball drain gate: the event it publishes, the block a ball has to hold, and what one block cannot tell |
| [`bumper.md`](bumper.md) | The bumper driver: what it does in the interrupt, what bounds a pull-in, and what the board depends on the firmware for |
| [`lighting.md`](lighting.md) | The LED output path and the library that carries it |
| [`input-handling.md`](input-handling.md) | Description of the event queue system to read and distribute inputs to the game logic |
| [`storage.md`](storage.md) | The storage driver: the SD card's only user, streaming the sounds and keeping every game's own files |
| [`game-abstraction.md`](game-abstraction.md) | The main file, the generic main loop, the host that runs the games, and the interface every game implements |
| [`magnetic-rotary.md`](magnetic-rotary.md) | The magnetic rotary sensor driver: how far the seal rod turned, read over I²C without waiting on the bus |
| [`components/rotating-seal.md`](components/rotating-seal.md) | The rotating seal component: the shots and full turns a game sees of the seal |
| [`controls.md`](controls.md) | The toggle switch driver: an event when the switch moves, and its position on request |
| [`servo.md`](servo.md) | The servo driver: the angle as a pulse FlexPWM1.2 repeats on its own, with no interrupt |
