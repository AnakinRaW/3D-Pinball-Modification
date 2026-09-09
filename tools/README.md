# Tools

| File | Contents |
|---|---|
| [`figcheck.py`](figcheck.py) | Recomputes a subsystem's figures from its inputs, checks its document and drawings against the result, and writes the figures back into them |
| [`pdftext.py`](pdftext.py) | Pulls the shown text out of a datasheet PDF, enough to look a cited reading up |

Python 3, standard library only, no build step.

## figcheck

A subsystem declares its inputs and its formulas in `docs/parts/<subsystem>/figures.py`, which names the document and the drawings that model governs. IR sensing is the only one so far. The checker imports the model, evaluates every figure, and compares the results against those files:

```
python tools/figcheck.py docs/parts/ir-reflective/figures.py --sheets
```

Quantities carry a unit and a dimension, as exponents over volt, ampere, second, kelvin and metre. Adding a current to a time raises rather than computing, and a figure declared in mA cannot be printed as µs.

A stated figure is located in the document **by the value it computes**, inside a named block group and a section. Rewording a line therefore costs nothing, while a changed number has nowhere to land and gets reported against whatever its group carries.

Each pass can fail the run:

| Pass | What it checks |
|---|---|
| anchored | Every figure the model marks as stated is found in its group, and the value there agrees to one unit of its last printed digit. A figure declared `prints` gets a one-sided tolerance, so a bound printed on the wrong side of the computed value fails |
| orphans | Every value inside a fenced block or a table, anywhere in the document, traces back to a formula or to a declared input. Prose is scanned too, and carries the figures the model marks as stated in prose rather than on a line |
| drawings | A figure carried by an SVG `<text data-fig="...">` agrees with the model, and text elements holding an unanchored figure are listed. One element may carry several figures, and the attribute then names them space separated |
| direction | Perturbing a declared input moves the figure the way `rises_with` and `falls_with` claim, and a dependency that never moves the result is an error |
| curves | A plotted curve keeps its shape, each reading sits between the points around it, and where the sheet's table covers the same condition the reading sits inside it. This is the only check a curve reading can get, since the sheet never prints it |
| invariants | The relations the design requires hold, stated over the quantities and independent of the formulas |
| sums | The datasheets the model cites match the checksums in [`docs/datasheets/SHA256SUMS`](../docs/datasheets/SHA256SUMS) |
| stale | A number a diff removed from a document is reported wherever it still stands elsewhere in `docs/` or `firmware/` |
| datasheets | `--sheets`: each `datasheet` reading is looked up in the PDF it cites. A sheet whose text does not come out is reported unread, not passed. A `graph` reading is exempt, since a plotted curve carries no text |
| mutation | `--mutate`: every token a figure could land on is moved, and the run has to report that figure. A figure that survives is one the check would not have caught |

Flags:

| | |
|---|---|
| `--base <ref>` | The git ref the stale pass diffs against, `HEAD` by default |
| `--provenance` | Every input with its kind, its value and its source |
| `--graph <key>` | One quantity in full: its value, its source, its formula, the value of each input to it, everything it rests on and everything that rests on it |
| `--groups` | The block groups and value tokens the markdown parser found |
| `--blind` | The brief for an independent derivation: each quantity, its unit and its inputs, with no formula and no value |
| `--write-sums` | Records the checksums of the cited datasheets |
| `--write` | Writes the model's figures into the document and the drawings, so a figure is typed in one place only. Reports every edit, and reports the figure it could not place instead of guessing |
| `--sheets`, `--mutate`, `--no-stale` | Switch the named pass on, or off |

A figure that states a bound declares which way the document rounds it: `prints="down"` for a ceiling, `prints="up"` for a floor. Nearest rounding turns a 23.571 kΩ ceiling into `≤ 24 kΩ`, which the design does not satisfy, and a symmetric tolerance accepts it.

A model governs one markdown document plus its drawings. A figure that another document repeats, a firmware note for instance, is covered only by the stale pass, which reports it once the figure moves.

An input carries a provenance kind: `datasheet` and `graph` for a sheet reading, from a table and from a plotted curve; `measured` for a bench result; `assumed` and `decision` for what was assumed or chosen. A formula may hold no number beyond 0, 1 and 2, which appear as algebra. Every other constant is a declared input with a source, so a factor like the ln(9) between a 10-to-90 % rise time and a time constant cannot sit unnamed inside a derivation.

## pdftext

Walks a PDF's objects, inflates the content streams, collects the strings the text operators show, and maps two-byte codes through every ToUnicode table in the file, merged into one. Several manufacturer sheets carry the standard security handler with an empty user password, which encrypts every stream while leaving the file readable in any viewer; revisions 2 and 3 of that handler are undone here the way a viewer undoes them. Revision 4 and up may use AES, which this module does not implement, and such a file comes back empty. Where two subset fonts assign one code to different glyphs the merged table picks one, so a figure that is found is not proof the sheet states it. A figure that is not found is worth re-reading by hand, and that is what the report says.
