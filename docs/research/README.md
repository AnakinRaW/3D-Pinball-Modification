# Research

Groundwork behind the design: what the stock ROKR EG01 kit does mechanically and electrically, measurements taken from it, notes on the parts under consideration, and other pinball modifications worth learning from.

Component specifications and the reasoning behind a part choice are recorded here, one file per component, and linked from the [part list](../parts-list.md).

| Document | Contents |
|---|---|
| [`teensy-4.1.md`](teensy-4.1.md) | Specifications of the selected controller and the electrical constraints it imposes |
| [`teensy-4.1.py`](teensy-4.1.py) | The controller's figures and what each of its pins can carry, each with its source, which the subsystems' models take their Teensy figures from |
| [`Custom/adafruit-3992.md`](Custom/adafruit-3992.md) | The scoop's push-pull solenoid: its published figures, what the 2 A on its label describe, and how it pulls with and without its return spring |
| [`Custom/sezo-ws2812b-strip.md`](Custom/sezo-ws2812b-strip.md) | The LED strip the lighting is cut from: its current per color and while dark, measured on two LEDs |
| [`Custom/cny70.md`](Custom/cny70.md) | The reflective sensor held for a bench trial: its ratings, what its datasheet leaves unspecified, and how it compares to the part in the stock boards |
| [`Rokr/1_power-supply.md`](Rokr/1_power-supply.md) | The stock 5 V input: adapter rating, the protection and filtering parts at the power entry, the inventory of loads on the rail, and why an undersized supply resets the machine |
| [`Rokr/2_ir-reflective-sensor-p33.md`](Rokr/2_ir-reflective-sensor-p33.md) | The stock ball sensor: measurements, reconstructed schematics of the sensor board and of its mainboard channel, and why the stock circuit is built that way |
| [`Rokr/3_bumper-control.md`](Rokr/3_bumper-control.md) | The stock bumpers: solenoid figures, the foil-and-shell trigger contact, wiring to the mainboard, and the parts seen on the board |
| [`Rokr/4_lighting.md`](Rokr/4_lighting.md) | The stock lighting: the LED inventory by board and color, the strips and their housings, the driver stage read from the mainboard, and the TM1617 display driver |
| [`Rokr/5_sound.md`](Rokr/5_sound.md) | The stock sound: the speaker's impedance, connection and drive, the sounds heard in play and what triggers them, and the single-voice playback |
| [`Rokr/6_ir-break-beam.md`](Rokr/6_ir-break-beam.md) | The stock through-beam sensor: the emitter and receiver on their solder points, the parts of its mainboard channel, and the continuously lit emitter |

`Rokr/` holds findings about the stock machine. `Custom/` holds the notes on parts the modification adds. Manufacturer datasheets live in [`../datasheets/`](../datasheets/).
