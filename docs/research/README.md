# Research

Groundwork behind the design: what the stock ROKR EG01 kit does mechanically and electrically, measurements taken from it, notes on the parts under consideration, and other pinball modifications worth learning from.

Component specifications and the reasoning behind a part choice are recorded here, one file per component, and linked from the [part list](../parts-list.md).

| Document | Contents |
|---|---|
| [`teensy-4.1.md`](teensy-4.1.md) | Specifications of the selected controller and the electrical constraints it imposes |
| [`Rokr/2_ir-reflective-sensor-p33.md`](Rokr/2_ir-reflective-sensor-p33.md) | The stock ball sensor — measurements, reconstructed schematics of the sensor board and of its mainboard channel, and why the stock circuit is built that way |
| [`Rokr/3_bumper-control.md`](Rokr/3_bumper-control.md) | The stock bumpers — solenoid figures, the foil-and-shell trigger contact, wiring to the mainboard, and the parts seen on the board |
| [`Rokr/1_power-supply.md`](Rokr/1_power-supply.md) | The stock 5 V input — adapter rating, the protection and filtering parts at the power entry, the inventory of loads on the rail, and why an undersized supply resets the machine |

`Rokr/` holds findings about the stock machine. Manufacturer datasheets live in [`../datasheets/`](../datasheets/).
