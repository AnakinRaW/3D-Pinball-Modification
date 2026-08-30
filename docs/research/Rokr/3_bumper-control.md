# Stock bumper mechanism and control

Three bumpers sit on the EG01 playfield. Each is an open-frame push/pull solenoid mounted through a playfield cutout, plunger upward, carrying a V-shaped metal shell above the playfield surface. Detection uses no separate sensor: the ball closes a contact between a conductive foil on the playfield and the shell.

![Stock bumper trigger and drive](bumper-mechanism.svg)

## Bumper sensing

A conductive foil glued onto the playfield sits at 5 V whenever the machine runs (measured). Shell, plunger and frame form one conductive node; diode test and resistance from each coil pin to the frame read open, so this node is isolated from the coil.

A ball on the foil touching a shell connects that bumper's frame to 5 V. The conductive path reads 7–30 Ω, measured along the foil and across a closed contact, at various points. Each frame has its own wire to the mainboard, which is how the board tells the three bumpers apart.

## Trigger behaviour

One trigger produces one short pull-in. Manually forcing a permanent contact of a sensing eyelet and the foil eyelet does not keep the coil energized.
This is presumably done in order to protect the solenoid. Various other vendors include a warning in their datasheet that such a mini solenoid should not be driven longer than a couple of seconds. Whether hardware or firmware enforces the cut-off is not identified.

At power-on all three coils pull in once, together, and light and sound follow after; a restart after winning or losing a game does the same. Nothing triggers three bumpers at once, and a game ending is a software event that probably does not reset the controller, so firmware fires all three at the start of a game. The first one after power-on might also have a different cause: an NPN at the gate inverts, so the gate sits high for as long as the controller has not booted, and the coils stay energized until firmware takes the pins.

## Solenoid

| Property | Value |
|---|---|
| Supply | 5 V, measured in operation |
| Winding resistance | ≈ 7.35 Ω, measured |
| Coil current | ≈ 0.68 A, measured in series at the coil while running; 5.0 V ÷ 7.35 Ω gives the same, so the stock drive path drops nothing measurable |
| Power while energized | ≈ 3.4 W — 5.0 V × 0.68 A |
| Stroke | ≈ 6 mm, measured installed with the shell mounted |

The manufacturer is unknown, probably some custom designed component.

## Wiring

Four eyelet-terminal wires and three 2-pin connectors leave the mainboard:

| Connection | Count | Attachment |
|---|---|---|
| Foil | 1 | eyelet terminal screwed onto the foil |
| Frame sense | 3 | eyelet terminal on each solenoid frame |
| Coil | 3 | 2-pin connector per coil |

Which coil pin is the fixed one and which is switched was not traced. A plain solenoid pulls in on either current direction, so the winding itself has no polarity. The distinction belongs to the drive circuit. It becomes binding once a flyback diode is fitted across the coil.

## Mainboard

Nothing on the board was probed; parts were read visually.

The coil connectors and the four eyelet wires land in two separate areas of the board. This allows the guess which parts belong to which mechanism, though this is not evidence.

| Seen | Where | Reading |
|---|---|---|
| 3 × 8-pin package | at the 2-pin coil connectors, closer to them than the J3Y | markings not read |
| 3 × SOT-23 marked `J3Y` | same area, behind the 8-pin packages | maps to an S8050 NPN SOT-23 |
| 1 diode + 2 resistors per channel — `D3`, `R9`, `R6` on one channel | same area, at the coil pins | types and values not read |
| `C10` | at the four eyelet wires — foil and the three shells | type and value not read |

### Trigger circuit

The 8-pin packages sit nearest the coil connector. That is where the switch belongs, and a SOIC-8 MOSFET is the typical part in such a spot. The S8050, printed as J3Y on the package, behind them probably drives the gate.

*The S8050 is rated 500 mA continuous (datasheet, maximum ratings), below the coil's 0.68 A.*

Of the three parts at the coil pins, `D3` reads as the flyback diode across the coil and `R9`/`R6` presumably as the base and gate network.

**TODO: Read SMD parts for a better guess how the stock machine works.**

### Sense circuit

`C10` sits at the four eyelet-ended wires. The foil is the largest conductor in the machine and works as an antenna in both directions. It picks up noise from around it, and it radiates what the board puts on it. The capacitor therefore presumably sits on the foil's 5 V feed, where it passes the 5 V and shorts high-frequency noise to ground.

No resistor sits close to the eyelet wires. While no ball touches a bumper shell, that shell's sense line is open: nothing sets its voltage, and it picks up whatever noise is nearby. Thus, somewhere else a pull-down resistor to ground must exist. Otherwise, the controller would read noise as a closed contact and would fire the bumper. 

## Sources

- Meter and in-operation measurements on the stock machine — winding resistance, coil-to-frame isolation, closed-contact resistance, foil potential, supply voltage, stroke
- Observation on the stock machine — all three coils pull in once, together, ahead of light and sound, both when the supply is connected and on a restart that keeps it connected
- [BL Galaxy Electrical S8050 datasheet](../../datasheets/S8050.PDF) — document BL/SSSTC079 Rev. A: J3Y marking (ordering information), maximum ratings
