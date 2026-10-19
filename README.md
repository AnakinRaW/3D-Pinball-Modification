# ROKR 3D Pinball Modification

Modification of the Robotime ROKR Pinball Machine (EG01) wooden 3D-puzzle kit with custom electronics.

This repository hosts everything for the project:

| Content | Location |
|---|---|
| Firmware: source, tests and code notes | [`firmware/`](firmware/) |
| Firmware design, one document per driver and component | [`docs/firmware/`](docs/firmware/) |
| PCB designs (schematics & layout) | [`hardware/pcb/`](hardware/pcb/) |
| 3D CAD files | [`hardware/cad/`](hardware/cad/) |
| Documentation | [`docs/`](docs/) |
| Design, one directory per subsystem | [`docs/parts/`](docs/parts/) |
| Datasheets | [`docs/datasheets/`](docs/datasheets/) |
| Research | [`docs/research/`](docs/research/) |
| Ideas, not part of the design | [`docs/ideas/`](docs/ideas/) |
| Part list (BOM) | [`docs/parts-list.md`](docs/parts-list.md) |
| Photos, renders & other assets | [`assets/`](assets/) |
| Checking tools | [`tools/`](tools/) |

## Reversibility

The modification is planned to be reversible: the stock machine can be returned to its original functionality.

## Status

Early stage. Research and design in progress; nothing has been built yet.

The modification is built around a **Teensy 4.1** controller.

## Bench warnings

- Do not plug or unplug the Teensy while the machine is powered, and do not connect a powered board to an unpowered Teensy. Its pins must not carry voltage while it is off.
- With the Teensy connected powered from USB, ensure that grounds between Teensy and an externally powered board are connected.
- Do not use an adapter that puts out more than 5.5 V or is rated above 6 A. The power board has no overvoltage protection, and the Teensy's VIN ends at 5.5 V. Above 6 A a short on a board's feed wire no longer trips the adapter and overheats the wire.

## Disclaimer

This modification is developed with AI and in the context that electricity is involved, reuse any information at your own risk.

## License

MIT, see [LICENSE](LICENSE).
