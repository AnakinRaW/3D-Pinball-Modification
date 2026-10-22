# Tools

| File | Contents |
|---|---|
| [`figcheck.py`](figcheck.py) | Recomputes a subsystem's figures from its inputs, checks its document and drawings against the result, and writes the figures back into them. Checks every marked Teensy pin against the pin table |
| [`figlib/`](figlib/) | The modules `figcheck.py` runs, as its [layout](#layout) lists them |
| [`svgcheck.py`](svgcheck.py) | Measures every label in a drawing and reports the ones that run off the canvas, out of their box, or onto each other, and the boxes that cross one another |
| [`pdftext.py`](pdftext.py) | Pulls the shown text out of a datasheet PDF, enough to look a cited reading up |
| [`picture.py`](picture.py) | Turns the PNGs of one screen layer, each at most 320 × 240 pixels, into picture files that share one palette of 255 colors with their opacity, dithered, as [`media.md`](../assets/media.md) describes |

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

A subsystem declares its inputs and its formulas in `docs/parts/<subsystem>/figures.py`, which names the documents and the drawings that model governs. Every subsystem that derives figures has one. Two further models hold what several subsystems share: [`docs/research/teensy-4.1.py`](../docs/research/teensy-4.1.py) the figures of the board and its chip, and [`docs/firmware/figures.py`](../docs/firmware/figures.py) the firmware's periods, watchdog, interrupt priorities and slots. The checker imports each model, evaluates every figure, and compares the results against those files. It then checks the Teensy pins of the whole tree:

```
python tools/figcheck.py --sheets                                  every model, then the pins
python tools/figcheck.py docs/parts/ir-reflective/figures.py       one model, and the pins its files mark
```

A relative model path reads from the root of the checkout that holds this `figcheck.py`, so a run from another checkout's directory checks this checkout's model. A model outside this checkout's `docs/` is refused.

A figure is declared in one model, and every other model that needs it takes it by name:

```python
MODEL.uses("v_3v3", of="teensy-4.1", group="V_GATE", section=DRIVE, stated=True)
```

The value, the unit, the kind and the source stay where the figure is declared. The taking model says only how its own documents state the figure, so each document is still checked line by line. Two models may take figures from each other, as the firmware takes the solenoids' pull-in while the solenoids take its watchdog. The models are evaluated figure by figure, and only a figure that rests on itself is a cycle.

A constant a listing writes as a bare number follows the figure it encodes:

```python
MODEL.as_written("t_tick_us", of="t_tick", unit="µs")    # kPeriodUs = 5000
```

Quantities carry a unit and a dimension, as exponents over volt, ampere, second, kelvin and meter. Adding a current to a time raises rather than computing, and a figure declared in mA cannot be printed as µs.

Every number in the prose is checked too, against every declared quantity and with its unit converted. A figure outside a block or a table has nothing anchoring it, so when the model moves under it nothing notices, which is how a bench current stayed at 192 mA after it had become 233 mA. A prose figure that stands for a bound keeps to the side the bound is printed on, so a floor quoted one digit too high fails. What a document states and the model does not compute is declared as an aside, with the reason:

```python
MODEL.aside("2.54 mm", "the connector pitch")
```

An aside is a quoted datasheet row, a package, a pitch or a plain count. A figure the model holds at another unit is declared with `as_written` instead. Where the reason reads "derived in prose", the entry is a debt: rule 15 wants that figure in the model, and the list is where it is visible until it gets there. An aside whose number no document states any more fails the run, since the model then names a number that has left the documents.

A stated figure is located in the document **by the value it computes**, inside a named block group and a section. Rewording a line therefore costs nothing, while a changed number has nowhere to land and gets reported against whatever its group carries. A table cell holding nothing but a bare number, such as an interrupt priority, can be the line a dimensionless figure states. Nothing requires such a cell to be claimed, since a pin or a count fills a table the same way.

Each check reports on a separate line, and any of them can fail the run:

| The run prints | What it checks |
|---|---|
| figures in the document | Every figure the model marks as stated is found in its group, and the value there agrees to one unit of its last printed digit. A declared input agrees digit for digit, so a resistor left at 1.1 kΩ beside a model at 1.2 kΩ fails. A figure declared `prints` gets a one-sided tolerance, so a bound printed on the wrong side of the computed value fails |
| figures named in prose | A figure the model marks as stated in prose appears somewhere in the section that mentions it |
| every number accounted for | Every value inside a fenced block or a table, anywhere in the document, traces back to a formula or to a declared input |
| figures in the drawings | A figure carried by an SVG `<text data-fig="...">` agrees with the model, and a figure in a text element that no `data-fig` names fails the run. One element may carry several figures, and the attribute then names them space separated. A figure of another model is anchored as `model:key`, such as `break-beam:r_e`, so a drawing that carries several subsystems' parts needs no `uses` in the model that owns it |
| Teensy pins | Every marked pin agrees with [`docs/pin-assignment.md`](../docs/pin-assignment.md), the table gives no pin to two signals and no signal twice, and no number is called a Teensy pin without its marker |
| what the pins can carry | The pin table's port, capability, timer and FlexIO rows are the ones the board's model gives. See [What the pins can carry](#what-the-pins-can-carry) |
| what the allocation costs | Each of the two cost tables names every port, timer and FlexIO its part of the table takes and nothing else, lists every pin a taken port has no alternative for, and states the PWM pins and analog inputs it takes correctly |
| which way a figure moves | Perturbing a declared input moves the figure the way `rises_with` and `falls_with` claim, and a dependency that never moves the result is an error |
| readings off a plotted curve | A plotted curve keeps its shape, each reading sits between the points around it, and where the sheet's table covers the same condition the reading sits inside it. This is the only check a curve reading can get, since the sheet never prints it |
| what the design requires | The relations the design requires hold, stated over the quantities and independent of the formulas |
| readings found in their sheet | `--sheets`: each `datasheet` reading is looked up in the PDF it cites. A sheet whose text does not come out is reported unread, not passed. A `graph` reading is exempt, since a plotted curve carries no text |
| datasheet files | The datasheets the model cites match the checksums in [`docs/datasheets/SHA256SUMS`](../docs/datasheets/SHA256SUMS) |
| the check would catch a slip | `--mutate`: every token a figure could land on is moved, and the run has to report that figure. A figure that survives is one the check would not have caught |
| one driver at a time | No net carries two outputs at once: an output that always drives shares its net with no other output, and tri-state outputs on one net have a select each. See [Wiring](#wiring) |
| a defined level | A net that no push-pull output drives has a pull-up or a pull-down |
| the Teensy's rail on its pins | Every output, pull and contact on a net that reaches a Teensy pin runs from the Teensy's own 3.3 V, or pulls to ground |
| the pins the table gives | A model wires only to pins the pin table gives its subsystem, and wires every pin of the allocation table that it holds. A row whose Subsystem cell links several subsystems gives the pin to each of them, so each of their models wires it |
| pin functions | Every pin's Peripheral entry is a port signal, a timer channel or a FlexIO signal the board's model gives that pin, or a plain digital pin |
| owned peripherals | A peripheral a model owns is used by no other subsystem's pin and owned by no other model. The pins of a port that one bus runs for several subsystems may also belong to the subsystems the bus serves |
| shared budgets, one line each | What every model draws from a pool in the machine adds up under the one figure that supplies it: the current of the Teensy's 3V3 pin, RAM2, and the slots of the driver tick and of the device monitor. On the bench a subsystem runs alone, and its own model checks that case |

A number a diff removed from a document is reported under **Worth knowing** wherever it still stands elsewhere in `docs/`, together with what the run covers but cannot gate. A failure names the file, the line and the two values, and the run ends by saying whether anything needs fixing.

Flags:

| | |
|---|---|
| `--base <ref>` | The git ref the stale pass diffs against and `--write` takes the old figures from, `HEAD` by default |
| `--provenance` | Every input with its kind, its value and its source |
| `--graph <key>` | One quantity in full: its value, its source, its formula, the value of each input to it, everything it rests on and everything that rests on it |
| `--groups` | The block groups and value tokens the markdown parser found |
| `--blind` | The brief for an independent derivation: each quantity, its unit and its inputs, with no formula and no value |
| `--write-sums` | Records the checksums of the cited datasheets |
| `--write` | Writes the model's figures into the document and the drawings, so a figure is typed in one place only. Reports every edit, and reports the figure it could not place instead of guessing. A figure that moved far is found on the line that still reads its value at `--base`. Where figures that print different values could each take the same lines, those lines stay as they are until a `label` in the model tells the figures apart. A label is a phrase that only the figure's own line carries. A block in which a figure found no line of its own is left unwritten. Those figures go in by hand. A second run then writes the rest. A drawing is written by its `data-fig` keys and needs no such care. A pin that moved in the pin table is carried into every place that marks it |
| `--sheets`, `--mutate`, `--no-stale` | Switch the named pass on, or off |

A sheet comes back unusable in two ways, and both are reported as unread rather
than blamed on the readings in it. Fonts the extractor cannot map give noise,
which the share of plausible characters catches. A scan of printed pages gives
clean text that is not the sheet, a few navigation labels repeated, which scores
full marks on plausibility and carries almost no vocabulary; the count of
distinct words catches that one.

A figure that states a bound declares which way the document rounds it: `prints="down"` for a ceiling, `prints="up"` for a floor. Nearest rounding turns a 23.571 kΩ ceiling into `≤ 24 kΩ`, which the design does not satisfy, and a symmetric tolerance accepts it.

A model governs one markdown document plus its drawings, and `documents=[...]` adds further files the same figures have to agree with. `ir-reflective` names [`docs/firmware/drivers/ir-sensing.md`](../docs/firmware/drivers/ir-sensing.md) there. An added file is read whole rather than by section, so its numbers are checked by value and a stale one is reported with its line; the anchoring by group and section covers the primary document only.

### Teensy pins

A Teensy pin is typed once, in [`docs/pin-assignment.md`](../docs/pin-assignment.md). Every other place marks it with the signal it carries, named as the table's Signal column names it up to the first comma, so `CS-A` stands for "CS-A, converter for channels 1 to 8":

| In | The marker |
|---|---|
| Markdown | A link to the table, titled with the signal: `[39](../../pin-assignment.md "Solenoid trigger 2")` |
| A fenced block | The line's comment, naming in order the signals of the numbers right of its `=`: `kSense[kSenses] = {36, 35, 34};  // pin-assignment.md: Solenoid sense 1 to 3` |
| A drawing | `data-pin` on the text element, several signals separated by semicolons: `<text data-pin="Solenoid sense 1">pin 36</text>` |

`Solenoid sense 1 to 3` stands for the three signals it spans. The figure passes skip a marked number, so a pin needs no aside in the model.

A number the text calls a Teensy pin in so many words, as in "Teensy pin 31", fails the run without its marker. A comment in a drawing cannot carry one, so it names no pin number.

When a diff moves a pin in the table, its old number is listed under **Worth knowing** wherever it still stands unmarked. That covers the first column of a table headed Pin under a heading that names the Teensy, and every line that names a pin together with the Teensy, PJRC or one of its peripherals. A timer or a port the table no longer names is listed the same way. The pin table, the research notes and the datasheets are left out, since they describe the board rather than this build's allocation. Any other unmarked pin number is not checked, which is right for a connector pin or an IC's.

### Wiring

Every model declares its subsystem's nets beside its figures. A net names the Teensy signal it reaches, as the pin table names it up to the first comma, and every part on it:

```python
MODEL.net("MISO, Teensy side",
          drives("U3 OUTF through R34", "push-pull", rail=TEENSY_RAIL, src="..."),
          teensy="MISO")
MODEL.net("ADC-DOUT",
          drives("U1 DOUT through R37", "tri-state", rail="3V3_ADC", select="CS-A", src="..."),
          drives("U2 DOUT through R38", "tri-state", rail="3V3_ADC", select="CS-B", src="..."),
          pull("R36", "the board's 3.3 V", src="..."),
          reads("U3 INF", src="..."))
```

| Part | What it does to the net |
|---|---|
| `drives(name, "push-pull", rail=...)` | Drives the net whenever its rail is up |
| `drives(name, "tri-state", rail=..., select=...)` | Drives it only while the signal `select` names is active |
| `drives(name, "open-drain")` | Pulls it low, and lets go otherwise |
| `contact(name, to=...)` | Closes it to ground, or to the rail `to` names |
| `pull(name, rail)` | Holds it at a rail through a resistor, `"gnd"` for a pull-down |
| `reads(name)` | Reads it and drives nothing |

The Teensy pin itself takes the part its Peripheral entry gives it. A port signal drives the net the way the board's model declares that signal, a timer channel and a plain digital output drive it push-pull, and a plain digital input reads it. Nets that reach one Teensy signal merge across the models, so a second subsystem on a pin meets the parts of the first. A pin reaches a second subsystem only where the Subsystem cell of the pin table links both, as it does for SDA and SCL of `Wire`.

`MODEL.owns(peripheral, why)` claims a peripheral the subsystem's driver programs itself. The model that holds a bus run for the drivers of other subsystems claims the bus's port with `serves=(...)`, which names those subsystems. Their pins may then carry the port, as the firmware's model claims `Wire` for the rotary sensor and the port expander. `MODEL.supplies(key, pool=...)` names the figure a pool holds, and exactly one model supplies each pool. `MODEL.draws(key, pool=...)` names what the subsystem takes from it in the machine, and `MODEL.draws(pool=...)` one slot. The Teensy's model supplies its 3V3 pin under the wiring's own name for that rail, `TEENSY_RAIL`, and RAM2, and the firmware's model the slots of the driver tick and of the device monitor.

### What the pins can carry

The board's model declares what each pin can carry, every declaration with its source:

```python
MODEL.port("SPI", ("MOSI", 11), ("MISO", 12), ("SCK", 13), ("CS", 10, 36, 37), src=CARD_PINS)
MODEL.timer("QuadTimer3", (19, "0"), (18, "1"), (14, "2"), (15, "3"), src=PWM_C)
MODEL.flexio("FlexIO3", (19, 0), (18, 1), (14, 2), (15, 3), ..., src=FLEXIO_RM)
MODEL.analog(*range(14, 28), 38, 39, 40, 41, src=CARD_PINS)
```

A signal lists the pins that can carry it, alternatives in the order the pin card gives them. `port_roles` sets how a pin carrying a signal drives its net, and which signals a port works without, as SPI does without its CS pins. A signal the pin drives and reads in turn, such as a data line of FlexSPI2, is bidirectional and enters the wiring check as a tri-state output. Invariants in the model hold the data to PJRC's own counts of pins, PWM pins and analog inputs.

`--write` writes the pin table's port rows, its PWM and analog rows, its timer rows and its FlexIO rows from the model, and the run fails on a row that differs from it. A port, a timer or a FlexIO row also names the subsystem that uses it, which the run takes from the allocation rows that name it. The allocation itself stays typed in the table. The two cost tables are typed by hand as well, since their rows carry the reasons, and the run checks them against what the pins take. A port is taken once a pin carrying a signal it needs is taken, or once a row names one of its signals. A timer is taken once a row names one of its channels, and a FlexIO once a row names one of its signals.

An input carries a provenance kind: `datasheet` and `graph` for a sheet reading, from a table and from a plotted curve; `measured` for a bench result; `assumed` and `decision` for what was assumed or chosen. A formula may hold no number beyond 0, 1 and 2, which appear as algebra. Every other constant is a declared input with a source, so a factor like the ln(9) between a 10-to-90 % rise time and a time constant cannot sit unnamed inside a derivation.

### Layout

`figcheck.py` is the entry point and holds the names a model imports. The checker itself sits in [`figlib/`](figlib/), one module for each part of the run:

| Module | Holds |
|---|---|
| `quantities.py` | Units, dimensions and the quantity type, with the functions a formula may call |
| `model.py` | `Model` and what a model declares: inputs, formulas, requirements, curves, nets and the board's pins |
| `repo.py` | The repository root, each model found and loaded once, and the figures every model had at a git revision |
| `document.py` | A markdown file read as figure tokens, each in its group and section |
| `report.py` | The errors and notes a run collects, and the lines it prints them in |
| `checks.py` | The checks of one model, from the figures in its documents to `--mutate` |
| `pins.py` | The pin table, the marked pins, the cost tables and the pins a diff moved |
| `wiring.py` | The nets, the peripherals and the shared budgets, across every model |
| `write.py` | What `--write` puts into the documents and the drawings |
| `listings.py` | What `--graph`, `--blind`, `--provenance` and `--groups` print |
| `cli.py` | The run itself: a block for each model, then the pins and the wiring |

## pdftext

Walks a PDF's objects, inflates the content streams, collects the strings the text operators show, and maps two-byte codes through every ToUnicode table in the file, merged into one. Several manufacturer sheets carry the standard security handler with an empty user password, which encrypts every stream while leaving the file readable in any viewer; revisions 2 and 3 of that handler are undone here the way a viewer undoes them. Revision 4 and up may use AES, which this module does not implement, and such a file comes back empty. Where two subset fonts assign one code to different glyphs the merged table picks one, so a figure that is found is not proof the sheet states it. A figure that is not found is worth re-reading by hand, and that is what the report says.
