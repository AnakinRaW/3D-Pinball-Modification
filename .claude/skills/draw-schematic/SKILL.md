---
name: draw-schematic
description: Use when drawing, generating or updating a wiring diagram or schematic SVG for this modification - produces diagrams in the repository's established house style with consistent net colours, module blocks and pin markers. Trigger on "draw the schematic", "make a diagram of this circuit", "visualise this wiring", or when a circuit documented in markdown needs a picture.
---

# Drawing a schematic

Diagrams in this repository are hand-authored SVG showing modules as labelled blocks and nets as coloured wires between named pins. They exist to make a circuit followable by someone who cannot read a conventional symbol schematic. The EasyEDA project remains the design of record.

Existing examples to match: `docs/parts/ir-reflective/pulsed-schematic.svg`, `docs/research/Rokr/IR-Reflective-Mainboard-Schematic.svg`.

## Before drawing

Read the markdown that describes the circuit. The diagram carries no fact the document does not - values, pin numbers and net names are copied from it, never invented. Where the two disagree, the document wins and the disagreement is reported.

## Canvas

```
<svg viewBox="0 0 W H" xmlns="http://www.w3.org/2000/svg" font-family="Segoe UI, Arial, sans-serif">
  <rect x="0" y="0" width="W" height="H" fill="#ffffff"/>
```

Title at `x="30" y="34"`, `font-size="20"`, `font-weight="700"`, `fill="#1a1a1a"`.
Subtitle beneath at `y="56"`, `font-size="13"`, `fill="#666"` - one line naming what the circuit does.

## Net colours

One colour per net class, used for the wire, the pin circle and the label. Never reuse a colour for an unrelated net.

| Net | Colour | Use |
|---|---|---|
| Supply 3V3 / 5V | `#c0392b` | Wire, pin, label |
| Signal / analog | `#2c3e50` | Wire, pin, label |
| Ground | `#1a1a1a` | Wire, pin, label |
| LED / drive | wire `#b7950b`, pin fill `#f1c40f` stroke `#b7950b`, label `#8a6d0b` | Emitter and load lines |
| Clock / control | `#8a2be2` | Toggled digital lines |

## Elements

| Element | Markup |
|---|---|
| Module block | `<rect rx="8" stroke-width="2"/>` - controller `fill="#eef3f8" stroke="#4a6785"`, peripheral `fill="#f8f4ee" stroke="#8a6d0b"` |
| Module label | `font-size="16" font-weight="700"`, controller `#2c3e50`, peripheral `#5c4a00` |
| Pin | `<circle r="6"/>` on the block edge, filled in its net colour |
| Pin label | `font-size="13" font-weight="700"` in the net colour, with the pin number beneath at `font-size="11.5" fill="#777"` |
| Wire | `<line stroke-width="2.5"/>` in the net colour, orthogonal segments only |
| Junction | `<circle r="4"/>` in the net colour - **required** wherever wires join |
| Crossing | no dot, and where ambiguity remains, route around |
| Resistor | `<rect fill="#fff" stroke="#1a1a1a" stroke-width="2"/>` - `26x60` vertical, `70x20` horizontal |
| Value label | `font-size="13" font-weight="700" fill="#1a1a1a"` beside the body |
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


## Package variants

The drawn symbol is package-agnostic - a resistor looks the same whether it is axial or 0805. Where the **type designation** changes with the package, both types are named in a variants table, so the diagram serves a perfboard build and an SMD board equally.

Include a part only where its designation actually differs. A 220 Ohm resistor and a 100 nF ceramic are the same part in either package and carry their value alone.

Where the difference does arise, and the pair is not already sourced in the circuit document, source it before writing it into a diagram. Do not invent an equivalent.

| Typical case | Through-hole | SMD |
|---|---|---|
| Linear regulator | LD1117V33, TO-220 | AMS1117-3.3, SOT-223 |
| Schottky diode | 1N5819, DO-41 | SS14, SMA |
| Small-signal transistor | TO-92 type | SOT-23 type |
| Logic-level MOSFET | TO-220 type | SOT-23 type |
| Electrolytic | radial | SMD aluminium or tantalum |

Rendering, placed clear of the circuit body:

```
<text font-size="13" font-weight="700" fill="#1a1a1a">Package variants</text>
<line stroke="#ccc" stroke-width="1"/>          separator under the header row
<text font-size="12" font-weight="700" fill="#1a1a1a">   column headings: Ref, Function, Through-hole, SMD
<text font-size="11.5" fill="#444">             rows
```

The reference designator in the table matches the one on the drawn symbol.

## Known limitation

The white background is fixed, so these render as a bright panel against a dark GitHub theme. Changing it means reworking every existing diagram's contrast, so it stays until that is decided.

## After drawing

Hand off to `review-schematic-svg` before the diagram is committed.
