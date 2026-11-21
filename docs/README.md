| Document | Contents |
|---|---|
| [`features.md`](features.md) | The main features of the machine, each set against what the stock machine does |
| [`use-cases.md`](use-cases.md) | What the machine does for the player and the operator, each with the document that realizes it |
| [`parts-list.md`](parts-list.md) | Bill of materials |
| [`pin-assignment.md`](pin-assignment.md) | Which Teensy 4.1 pins the modification uses, and which peripherals they lock out |
| [`firmware/`](firmware/) | Design of the firmware: one document per driver and component, and the shared timings and priorities. [`model/firmware/`](../model/firmware/) computes them |
| [`parts/`](parts/) | Design of the modification, one directory per subsystem, each with its design documents and its schematic sketches. [`model/parts/`](../model/parts/) computes their numbers |
| [`research/`](research/) | Investigation of the stock machine, per-component notes and prior art |
| [`datasheets/`](datasheets/) | Manufacturer datasheets, referenced from the research and design documents |
| [`ideas/`](ideas/) | Ideas that are not part of the design and that nothing depends on |