# Tools

| File | Contents |
|---|---|
| [`figcheck.py`](figcheck.py) | Recomputes a subsystem's figures from its inputs, checks its document and drawings against the result, and writes the figures back into them. Checks every marked Teensy pin against the pin table |
| [`svgcheck.py`](svgcheck.py) | Measures every label in a drawing and reports the ones that run off the canvas, out of their box, or onto each other, and the boxes that cross one another |
| [`pdftext.py`](pdftext.py) | Pulls the shown text out of a datasheet PDF, enough to look a cited reading up |

Python 3, standard library only, no build step.

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

## figcheck

A subsystem declares its inputs and its formulas in `docs/parts/<subsystem>/figures.py`, which names the documents and the drawings that model governs. IR sensing, the break beam and the bumpers have one so far. The checker imports each model, evaluates every figure, and compares the results against those files. It then checks the Teensy pins of the whole tree:

```
python tools/figcheck.py --sheets                                  every model, then the pins
python tools/figcheck.py docs/parts/ir-reflective/figures.py       one model, and the pins its files mark
```

Quantities carry a unit and a dimension, as exponents over volt, ampere, second, kelvin and metre. Adding a current to a time raises rather than computing, and a figure declared in mA cannot be printed as µs.

Every number in the prose is checked too, against every declared quantity and with its unit converted. A figure outside a block or a table has nothing anchoring it, so when the model moves under it nothing notices, which is how a bench current stayed at 192 mA after it had become 233 mA. What a document states and the model does not compute is declared as an aside, with the reason:

```python
MODEL.aside("2.54 mm", "the connector pitch")
```

An aside is a quoted datasheet row, a package, a pitch, a plain count, or a figure the model already holds at another unit. Where the reason reads "derived in prose", the entry is a debt: rule 15 wants that figure in the model, and the list is where it is visible until it gets there.

A stated figure is located in the document **by the value it computes**, inside a named block group and a section. Rewording a line therefore costs nothing, while a changed number has nowhere to land and gets reported against whatever its group carries.

Each check reports on its own line, and any of them can fail the run:

| The run prints | What it checks |
|---|---|
| figures in the document | Every figure the model marks as stated is found in its group, and the value there agrees to one unit of its last printed digit. A figure declared `prints` gets a one-sided tolerance, so a bound printed on the wrong side of the computed value fails |
| figures named in prose | A figure the model marks as stated in prose appears somewhere in the section that mentions it |
| every number accounted for | Every value inside a fenced block or a table, anywhere in the document, traces back to a formula or to a declared input |
| figures in the drawings | A figure carried by an SVG `<text data-fig="...">` agrees with the model, and text elements holding an unanchored figure are listed. One element may carry several figures, and the attribute then names them space separated |
| Teensy pins | Every marked pin agrees with [`docs/pin-assignment.md`](../docs/pin-assignment.md), and the table gives no pin to two signals and no signal twice |
| which way a figure moves | Perturbing a declared input moves the figure the way `rises_with` and `falls_with` claim, and a dependency that never moves the result is an error |
| readings off a plotted curve | A plotted curve keeps its shape, each reading sits between the points around it, and where the sheet's table covers the same condition the reading sits inside it. This is the only check a curve reading can get, since the sheet never prints it |
| what the design requires | The relations the design requires hold, stated over the quantities and independent of the formulas |
| readings found in their sheet | `--sheets`: each `datasheet` reading is looked up in the PDF it cites. A sheet whose text does not come out is reported unread, not passed. A `graph` reading is exempt, since a plotted curve carries no text |
| datasheet files | The datasheets the model cites match the checksums in [`docs/datasheets/SHA256SUMS`](../docs/datasheets/SHA256SUMS) |
| the check would catch a slip | `--mutate`: every token a figure could land on is moved, and the run has to report that figure. A figure that survives is one the check would not have caught |

A number a diff removed from a document is reported under **Worth knowing** wherever it still stands elsewhere in `docs/` or `firmware/`, together with what the run covers but cannot gate. A failure names the file, the line and the two values, and the run ends by saying whether anything needs fixing.

Flags:

| | |
|---|---|
| `--base <ref>` | The git ref the stale pass diffs against, `HEAD` by default |
| `--provenance` | Every input with its kind, its value and its source |
| `--graph <key>` | One quantity in full: its value, its source, its formula, the value of each input to it, everything it rests on and everything that rests on it |
| `--groups` | The block groups and value tokens the markdown parser found |
| `--blind` | The brief for an independent derivation: each quantity, its unit and its inputs, with no formula and no value |
| `--write-sums` | Records the checksums of the cited datasheets |
| `--write` | Writes the model's figures into the document and the drawings, so a figure is typed in one place only. Reports every edit, and reports the figure it could not place instead of guessing. A block in which any figure found no line is left unwritten, because a figure that moved far loses its own line and another figure of the block can take it. The far movers then go in by hand, and a second run writes the rest. A drawing is written by its `data-fig` keys and needs no such care. A pin that moved in the pin table is carried into every place that marks it |
| `--sheets`, `--mutate`, `--no-stale` | Switch the named pass on, or off |

A sheet comes back unusable in two ways, and both are reported as unread rather
than blamed on the readings in it. Fonts the extractor cannot map give noise,
which the share of plausible characters catches. A scan of printed pages gives
clean text that is not the sheet, a few navigation labels repeated, which scores
full marks on plausibility and carries almost no vocabulary; the count of
distinct words catches that one. `IRL540N.PDF` is the scan in this repository,
and its readings are taken from the rendered page by eye.

A figure that states a bound declares which way the document rounds it: `prints="down"` for a ceiling, `prints="up"` for a floor. Nearest rounding turns a 23.571 kΩ ceiling into `≤ 24 kΩ`, which the design does not satisfy, and a symmetric tolerance accepts it.

A model governs one markdown document plus its drawings, and `documents=[...]` adds further files the same figures have to agree with. `ir-reflective` names [`firmware/ir-sensing.md`](../firmware/ir-sensing.md) there. An added file is read whole rather than by section, so its numbers are checked by value and a stale one is reported with its line; the anchoring by group and section covers the primary document only.

### Teensy pins

A Teensy pin is typed once, in [`docs/pin-assignment.md`](../docs/pin-assignment.md). Every other place marks it with the signal it carries, named as the table's Signal column names it up to the first comma, so `CS-A` stands for "CS-A, converter for channels 1 to 8":

| In | The marker |
|---|---|
| Markdown | A link to the table, titled with the signal: `[34](../../pin-assignment.md "Bumper trigger 2")` |
| A fenced block | The line's comment, naming in order the signals of the numbers right of its `=`: `kSense[kSenses] = {1, 14, 15};  // pin-assignment.md: Bumper sense 1 to 3` |
| A drawing | `data-pin` on the text element, several signals separated by semicolons: `<text data-pin="Bumper sense 1">pin 1</text>` |

`Bumper sense 1 to 3` stands for the three signals it spans. The figure passes skip a marked number, so a pin needs no aside in the model. A pin number that carries no marker is not checked, which is right for a connector pin or a pin the research notes name.

An input carries a provenance kind: `datasheet` and `graph` for a sheet reading, from a table and from a plotted curve; `measured` for a bench result; `assumed` and `decision` for what was assumed or chosen. A formula may hold no number beyond 0, 1 and 2, which appear as algebra. Every other constant is a declared input with a source, so a factor like the ln(9) between a 10-to-90 % rise time and a time constant cannot sit unnamed inside a derivation.

## pdftext

Walks a PDF's objects, inflates the content streams, collects the strings the text operators show, and maps two-byte codes through every ToUnicode table in the file, merged into one. Several manufacturer sheets carry the standard security handler with an empty user password, which encrypts every stream while leaving the file readable in any viewer; revisions 2 and 3 of that handler are undone here the way a viewer undoes them. Revision 4 and up may use AES, which this module does not implement, and such a file comes back empty. Where two subset fonts assign one code to different glyphs the merged table picks one, so a figure that is found is not proof the sheet states it. A figure that is not found is worth re-reading by hand, and that is what the report says.
