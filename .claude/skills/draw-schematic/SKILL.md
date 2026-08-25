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
- Escape XML in text content: `&lt;` for `<`, `&amp;` for `&`. A bare `<` inside a `<text>` element makes the file invalid and it will not render.


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

Listing a variant asserts it has been through `review-circuit` and `design-review` on its own figures. An alternative that has not been checked is named in the circuit document as an option, and stays out of the diagram's variants table.

## Placement constraints

A wiring diagram carries connectivity. Where a group must also sit physically close, the diagram says so, with the distance and the reason - otherwise the constraint survives only in someone's head between the drawing and the perfboard.

Enclose the group in a dashed rounded rectangle, in a colour that is never a net colour, and label it:

```
<rect rx="10" fill="none" stroke="#0a8a6a" stroke-width="1.5" stroke-dasharray="8 4"/>
<text font-size="11.5" font-weight="700" fill="#0a8a6a">keep together &#8212; &lt; 10 mm &#8212; LDO loop stability</text>
```

The `8 4` dash pattern distinguishes this from `5 4`, which marks an unpopulated option.

**Always state a distance.** "Close" is not a constraint. Where no figure is available from the datasheet, write the figure being assumed and mark it as an assumption.

Groups that usually carry one:

| Group | Typical limit | Reason |
|---|---|---|
| Decoupling capacitor and its IC supply pin | as short as the layout allows | Loop inductance between the capacitor and the pin |
| Regulator input and output capacitors and the regulator | per its datasheet | The output capacitor is part of the control loop |
| Gate resistor and gate pull-down at a MOSFET gate | short | Gate loop pickup, and a defined gate during boot |
| HF filter capacitor at a connector | at the entry point | It filters what arrives, so it belongs where it arrives |
| Star ground point | single point | Return current from switching loads staying out of the analog reference |

Where several groups exist, list them in a placement table beside the variants table: group, members, maximum distance, reason.

## Known limitation

The white background is fixed, so these render as a bright panel against a dark GitHub theme. Changing it means reworking every existing diagram's contrast, so it stays until that is decided.

## After drawing

Hand off to `review-schematic-svg` before the diagram is committed.
