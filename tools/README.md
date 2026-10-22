# Tools

| File | Contents |
|---|---|
| [`figcheck.py`](figcheck.py) | Recomputes a subsystem's figures from its inputs, checks its document and drawings against the result, and writes the figures back into them. Checks every marked Teensy pin against the pin table |
| [`figlib/`](figlib/) | The modules `figcheck.py` runs |
| [`svgcheck.py`](svgcheck.py) | Measures every label in a drawing and reports the ones that run off the canvas, out of their box, or onto each other, and the boxes that cross one another |
| [`pdftext.py`](pdftext.py) | Pulls the shown text out of a datasheet PDF, enough to look a cited reading up |
| [`picture.py`](picture.py) | Turns the PNGs of one screen layer, each at most 320 × 240 pixels, into picture files that share one palette of 255 colors with their opacity, dithered, as [`media.md`](../assets/media.md) describes |

Python 3, standard library only, no build step.

## figcheck

Each subsystem that derives figures holds its inputs and formulas in `docs/parts/<subsystem>/figures.py`. figcheck evaluates every model and compares the documents and drawings each one names against the result. It then checks the Teensy pins and the wiring across all models.

```
python tools/figcheck.py --sheets                                  every model, then the pins
python tools/figcheck.py docs/parts/ir-reflective/figures.py       one model, and the pins its files mark
```

Each check reports on one line. Any of them can fail the run. A failure names the file, the line and the two values, and the run ends by saying whether anything needs fixing.

| Flag | |
|---|---|
| `--sheets` | Looks every datasheet reading up in the PDF it cites |
| `--mutate` | Moves every figure the documents state and requires the run to report it |
| `--write` | Writes the model's figures into the documents and the drawings, and records the checksum of each cited datasheet that has none yet |
| `--graph <key>` | One quantity in full: its value, its source, its formula, everything it rests on and everything it feeds |
| `--provenance` | Every input with its kind, its value and its source |
| `--groups` | The block groups and value tokens the markdown parser found |
| `--blind` | Each quantity, its unit and its inputs, with no formula and no value |
| `--base <ref>` | The git ref the stale pass diffs against, `HEAD` by default. `--mutate` and `--provenance` run no stale pass |

## svgcheck

figcheck compares the values in a drawing. Whether the drawing can be read is a different question, and a label that runs off the board still renders, still diffs cleanly and still carries the right number.

```
python tools/svgcheck.py                       every SVG under docs/
python tools/svgcheck.py path/to/one.svg ...   only those
python tools/svgcheck.py --slack 4             a wider tolerance
python tools/svgcheck.py --margin 8            clearance a label is owed from a shape
python tools/svgcheck.py --margin 0            report only what actually touches
```

The margin is clearance rather than tolerance. A label is grown by it before the
overlap with a wire or a symbol is measured, so one that merely comes close is
reported the same way one sitting on it is. It defaults to 4 and applies to the
label-against-shape check alone: two lines of one caption sit close on purpose,
so `overlap` keeps the tolerance and gets no margin.

What it reports:

| | |
|---|---|
| `clipped` | the label leaves the viewBox, so part of it is cut off |
| `overruns` | the label starts inside a rectangle and ends outside it, which is what a board outline or a module block does to a caption |
| `overlap` | two labels sit on the same spot |
| `on a <shape>` | the label sits on a wire, a symbol or a pad instead of beside it |
| `clips` | two boxes cross and neither holds the other, with one of them an outline. A filled box over a filled box passes, since a mechanical sketch stacks parts on purpose |

Widths are estimated from character classes for a sans-serif face, not measured from a font, so the boxes are approximate and the tolerance is two units by default. A report is a place to look, not a verdict, and that is why this one stays out of CI where figcheck runs. Rotated labels are skipped, because an upright rectangle does not describe them.
