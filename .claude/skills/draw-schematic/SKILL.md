---
name: draw-schematic
description: Use when drawing, generating or updating a wiring diagram or schematic SVG for this modification - produces diagrams in the repository's established house style with consistent net colors, module blocks and pin markers. Trigger on "draw the schematic", "make a diagram of this circuit", "visualize this wiring", or when a circuit documented in markdown needs a picture.
---

# Drawing a schematic

Diagrams in this repository are hand-authored SVG showing modules as labeled blocks and nets as colored wires between named pins. They exist to make a circuit followable by someone who cannot read a conventional symbol schematic. The EasyEDA project remains the design of record.

Existing examples to match: `docs/parts/servo/servo-schematic.svg`, `docs/parts/ir-reflective/pulsed-schematic.svg`.

## Before drawing

Read the markdown that describes the circuit. The diagram carries no fact the document does not - values, pin numbers and net names are copied from it, never invented. Where the two disagree, the document wins and the disagreement is reported.

## Canvas

```
<svg viewBox="0 0 W H" xmlns="http://www.w3.org/2000/svg" font-family="Segoe UI, Arial, sans-serif">
  <rect x="0" y="0" width="W" height="H" fill="#ffffff"/>
```

Title at `x="30" y="34"`, `font-size="20"`, `font-weight="700"`, `fill="#1a1a1a"`.
Subtitle beneath at `y="56"`, `font-size="13"`, `fill="#666"` - one line naming what the circuit does.

## Net colors

A wire is drawn in the color of the physical wire that carries it, after the wire code in `docs/connectors.md`, section Wire colors. The pin circle, the junction dot and the label follow the wire.

| Wire code | Wire | Edge | Label |
|---|---|---|---|
| Black, GND | `#1a1a1a` | | `#1a1a1a` |
| Red, 5 V | `#c0392b` | | `#c0392b` |
| Yellow, 3.3 V supply | `#f1c40f` | `#9a7d0a` | `#8a6d0b` |
| Blue, 3.3 V logic going out | `#1f4fd1` | | `#1f4fd1` |
| Green, 3.3 V signal coming in | `#3cbf4e` | `#23862f` | `#1f7a33` |
| White, I2C and every line behind a level shifter or a driver | `#ffffff` | `#808080` | `#4d4d4d` |
| A pin left unconnected | `#9a9a9a` | | `#8a8a8a` |

A wire with an edge color is drawn twice. The edge goes first, 2.5 wider than the wire, then the wire on top. Where the segment joins another segment of its class, the edge reaches half its own width past the end and closes the corner. At any other end that a pin does not cover, it stops half its width short. A pin or a junction dot of such a class takes a rim of 1.5 in the edge color and is drawn after every wire.

A pin where a stock or vendor cable lands shows that cable's color, and its label names it, as `Pin 2 (black)` at a J-S of the IR sensors does.

A net flag, the small box naming a net that leaves the drawing, carries the label color on a tint of its wire's color:

| Wire code | Flag fill | Flag stroke |
|---|---|---|
| Red | `#fdecea` | `#c0392b` |
| Yellow | `#fdf5d3` | `#9a7d0a` |
| Blue | `#e8eefb` | `#1f4fd1` |
| Green | `#e6f4e6` | `#23862f` |
| White | `#ffffff` | `#808080` |

The drawings of the stock machine in `docs/research/` show the stock wiring and keep the colors they were drawn in, one per class of net: supply `#c0392b`, signal `#2c3e50`, ground `#1a1a1a`, LED and drive `#b7950b`, clock and control `#8a2be2`.

## Block colors

One color per kind of block, so a connector, a part on the board and a part out in the machine read apart at a glance. The label is the block's title; secondary lines inside a block stay `#666` or `#777`.

| Block | Fill | Stroke | Label | Use |
|---|---|---|---|---|
| Controller | `#eef3f8` | `#4a6785` | `#2c3e50` | The Teensy, and a stock controller |
| Board component | `#f8f4ee` | `#8a6d0b` | `#5c4a00` | An IC or a module on the modification's electronics: U1, DFR0954, WS2811, MCP3008 |
| Connector | `#f0f0f0` | `#6b6b6b` | `#333333` | J-PWR, J-T, J-Sn, a jack |
| External component | `#fbeef5` | `#a8437a` | `#7a2556` | What sits out in the machine: servo, solenoid, speaker, switch, foil, a sensor at the playfield, a stock board |

A net flag, the small box naming a net that leaves the drawing, keeps its net color.

## Elements

| Element | Markup |
|---|---|
| Block | `<rect rx="8" stroke-width="2"/>` in the colors of its kind from the block table |
| Block label | `font-size="16" font-weight="700"` in the label color of its kind |
| Pin | `<circle r="6"/>` on the block edge, filled in its net color |
| Pin label | `font-size="13" font-weight="700"` in the net color, with the pin number beneath at `font-size="11.5" fill="#777"` |
| Wire | `<line stroke-width="2.5"/>` in the net color, orthogonal segments only |
| Junction | `<circle r="4"/>` in the net color - **required** wherever wires join |
| Crossing | no dot, and where ambiguity remains, route around |
| Resistor | `<rect fill="#fff" stroke="#1a1a1a" stroke-width="2"/>` - `26x60` vertical, `70x20` horizontal |
| Value label | `font-weight="700" fill="#1a1a1a"`, **inside the body** with `text-anchor="middle"` and the size dropped until it fits. Beside the body only where the text cannot fit inside, such as a 26-wide vertical one |
| Annotation | `font-size="11.5"` to `12.5"`, `fill="#666"` or `#777` |
| Optional / not populated | `stroke-dasharray="5 4"` |

## Layout

- Controller on the left, peripherals on the right, supply at the top, ground at the bottom.
- Orthogonal wires. Diagonals only where a straight run would cross three other nets.
- No text overlapping a wire or another label.
- Repeated identical channels: draw one in full, and label the rest as identical rather than drawing seven copies.

## Language and notation

Rule 1 makes English the repository language, and that includes every label inside a diagram.

- English labels throughout. Not "Sensorplatine", "Widerstand", "TAKT".
- Decimal **point**: `3.3 V`, never `3,3 V`.
- Space between number and unit: `220 Ohm`, `4.7 kOhm`, `9 mA`. Use the symbol where the font renders it.
- File names kebab-case and English: `pulsed-schematic.svg`.
- Escape XML in text content: `&lt;` for `<`, `&amp;` for `&`. A bare `<` inside a `<text>` element makes the file invalid and it will not render.


## Packages

The build is through-hole (RULES.md rule 17). Where the **type designation** changes with the package, the symbol carries the through-hole type, such as CD74HCT125E or IRL540N. A 220 Ohm resistor and a 100 nF ceramic carry their value alone. The diagram has no variants table and names no SMD type.

A part that exists only in an SMD package is the exception. Its label names the package and the adapter it sits on, such as ISO7761F, SOIC-16 wide, on an adapter.

## Placement constraints

A wiring diagram carries connectivity. Where a group must also sit physically close, the diagram says so, with the distance and the reason - otherwise the constraint survives only in someone's head between the drawing and the perfboard.

Enclose the group in a dashed rounded rectangle, in a color that is never a net color, and label it:

```
<rect rx="10" fill="none" stroke="#0a8a6a" stroke-width="1.5" stroke-dasharray="8 4"/>
<text font-size="11.5" font-weight="700" fill="#0a8a6a">keep together, under 10 mm: LDO loop stability</text>
```

The `8 4` dash pattern distinguishes this from `5 4`, which marks an unpopulated option.

**Always state a distance.** "Close" is not a constraint. Where no datasheet gives a figure, the model declares the chosen one as `assumed`, and the drawing states the figure alone.

Groups that usually carry one:

| Group | Typical limit | Reason |
|---|---|---|
| Decoupling capacitor and its IC supply pin | as short as the layout allows | Loop inductance between the capacitor and the pin |
| Regulator input and output capacitors and the regulator | per its datasheet | The output capacitor is part of the control loop |
| Gate resistor and gate pull-down at a MOSFET gate | short | Gate loop pickup, and a defined gate during boot |
| HF filter capacitor at a connector | at the entry point | It filters what arrives, so it belongs where it arrives |
| Star ground point | single point | Return current from switching loads staying out of the analog reference |

Where several groups exist, list them in a placement table clear of the circuit body: group, members, maximum distance, reason.

## Known limitation

The white background is fixed, so these render as a bright panel against a dark GitHub theme. Changing it means reworking every existing diagram's contrast, so it stays until that is decided.

## After drawing

Hand off to `review-schematic-svg` before the diagram is committed.
