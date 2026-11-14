# Perfboard layouts

A board is laid out as a layout file, a Python module that builds a `Layout` from [`layout.py`](layout.py) and names it `LAYOUT`. [`build.py`](build.py) checks it and draws it:

```
python tools/perfboard/build.py <layout.py>              check only
python tools/perfboard/build.py <layout.py> <out.svg>    check, then draw
```

A layout is drawn only once no error is left. Placement and routing are the design work. The checker states whether the result can be built and makes the netlist, and the house-style notes say where it could be tidier.

| File | Holds |
|---|---|
| [`inventory.py`](inventory.py) | Boards and their edge pads, the connection kinds, every part's size and pinout with its datasheet location, the role of each IC's and transistor's pin, the breakout boards' outlines and pin rows, the resistor color code |
| [`draw.py`](draw.py) | Each part and connection drawn from above as it looks, at its inventory size |
| [`layout.py`](layout.py) | The layout format: board, parts on holes, connections, netlist, distance limits |
| [`check.py`](check.py) | The hard rules and the house style |
| [`render.py`](render.py) | Top side, bottom side turned over sideways, and the tables |
| [`catalog.py`](catalog.py) | Every inventory part on one sheet: `python tools/perfboard/catalog.py out.svg` |
| [`stock.py`](stock.py) | Every layout's perfboard and screw terminals counted against the stock in `inventory.py`: `python tools/perfboard/stock.py`, run after a layout takes or changes a board |

## Hard rules

The checker reports each as an error.

| Rule | |
|---|---|
| H1 | The grid is 2.54 mm and every hole is plated through, so top and bottom of a hole are one piece of copper |
| H3 | Edge pads sit along both short ends, one per hole row, the two outer rows without; each pad stands alone |
| H4 | A board may be cut, with one straight cut |
| H5 | A hole carries one net, and one lead or one wire end |
| H6 | Tin joins two neighboring holes only, never diagonally |
| H7 | A bare wire runs straight along a row or column and joins every hole it passes. On the top it runs under no part |
| H8 | Two bare runs never cross, on either side. A crossing takes a part's body or an insulated wire |
| H9 | An insulated wire joins two points, has no branch, is at least 10 mm long, and runs over no part. Each end goes through a hole that holds no lead and that no bare run crosses, tinned to the copper it joins, since a wire end cannot be soldered onto a run. A wire marked `short=True` may be shorter where a straight route is worth the risk to its insulation; the check lists it as a note |
| H10 | A link is bare wire through two holes; every hole it spans is free or of its own net |
| H13 | Parts sit on the top unless a sensor has to face another way |
| H14 | No lead sits under another part's body; bodies do not overlap |
| H16 | A part's leads span what its inventory entry allows: a lying resistor 4 to 6 holes, standing 1 to 2; radial parts 1 to 2; headers and TO-220 in neighboring holes; a screw terminal every second hole |
| H17 | Sizes and pinouts come from the inventory |
| H18 | A breakout board stands as a daughter board on socket headers |
| H19 | A 90° header sits at the board edge, its pins pointing off the board |
| V1 | The copper makes exactly the netlist the layout declares, pin by pin |
| V2 | Every distance limit holds, measured straight from hole to hole |
| S13 | A screw terminal sits in the outermost holes, its pin row parallel to the edge, the wire openings facing off the board |

## House style

The checker reports each as a note where the layout leaves it.

| Rule | |
|---|---|
| S1 | One free hole between holes of different nets, except between the pins of one part |
| S2 | Parts lie along the grid, never at an angle |
| S3 | A resistor lies with its leads 4 holes apart; standing only where room or a distance limit asks for it |
| S4 | A ceramic sits on neighboring holes, its legs bent out only where S1 gains by it |
| S5 | Parts of one kind face one way: diode bands, electrolytic plus, MOSFET tabs |
| S6 | Repeated channels are built alike, shifted or mirrored |
| S7 | The signal runs from the input side to the load side |
| S8 | Insulated wire is first choice where it is clearer, in the net's color of `docs/connectors.md`; bare wire for straight runs on the bottom; tin for one step. An insulated wire bends where its way turns and runs to its end, rather than handing over to bare copper on the way. It starts on the side it leaves toward, never running back over the lead it serves. Wires that run the same way lie in parallel rows and do not cross |
| S9 | All copper runs on the bottom, insulated wires included, so the board never switches sides; the top carries only a link where the bottom has no room |
| S10 | GND and the supply run as rails along an edge, on free edge pads where there are some |
| S11 | Coil and motor current keeps its own short paths, apart from the signals |
| S12 | A connector sits at the edge where it can; a straight Dupont header may sit inside when the edge costs a much harder layout. Every cable leaves away from the board. The outputs of one connector stay together at one end |
| S14 | Under a daughter board lies nothing taller than its socket header |
| S15 | A cable soldered in for good lands on an edge pad |
| S16 | The smallest board in stock that fits; cut once and straight, only where it saves much and the rest stays usable. All layouts together take no more boards and screw terminals than the stock holds (`stock.py`). Layouts that are alternative builds of one thing name their family and variant in `VARIANT`, and a variant takes no stock from the others |
| S17 | The corner holes stay free |
| S18 | Top and bottom in one drawing. The bottom is drawn as the board looks turned over its long edge: column 1 stays on the left, row A comes to the bottom, and the top's parts show faintly. The top shows the bottom's copper faintly. `--mirror` turns the board over its short edge instead |
| S25 | Every MOSFET, IC, diode, electrolytic and sensor carries its pin names beside its pins, on both sides of the drawing |
| S19 | Under the drawing: the parts, the copper per net, the distance limits. The side headings stand alone, and every hole a wire or a link ends in shows its solder joint |
| S20 | Every DIP IC sits in a socket |
| S21 | A layout that fits no board is split over several boards |
| S23 | Parts spread over the board: no empty stretch while another place is crowded. Room goes first where leads, rails and groups meet. A board that is not cut is used to its edges |
| S24 | A net's copper runs as one chain from pin to pin, with corners and no branch. Only a supply rail branches, where a part taps it |
| S22 | No part is added that the design does not list; a new part, a spare capacitor included, is asked for first |
| S27 | Runs of different nets do not share a column or a row over a stretch, and wires do not lie along another run; each crossing takes its own column. Labels that overlap in the drawing point to a layout that is too tight |
| S26 | The simpler layout wins over one with fewer notes: straight lanes to one end beat a split design, a straight drop onto a rail beats a detour around it |

## Layout format

```python
from layout import Layout

L = Layout("6x28", title="Sense board", netlist=("model/parts/solenoid/figures.py", "sense"))
L.part("J1", "header_male", ["B1", "C1", "D1"], title="J-SENSE-TEENSY", sub="to the Teensy")
L.part("R41", "resistor", ["B2", "B6"], value=10000, fig="r_series")
L.part("C1", "ceramic_k15", ["B28", "A28"])
L.part("Q1", "to220", ["G5", "G4", "G3"], tab=+1)          # holes of pins 1, 2, 3
L.part("U1", "cd74hct125e", pin1="C3", along=(1, 0))       # in a socket unless socket=False
L.part("A1", "teensy41", at="B2", rot=0)                   # the hole under the module's first pin
L.part("U3", "adafruit1207", at="C5", carries=True)        # an SOIC chip on its adapter
L.offboard("A2", "on a cable from J3")                     # a netlist part that sits off this board
L.unfitted("Q1", joins=("D", "S"), why="...")             # an optional part left out, a wire in its place
L.part("X1", "terminal_5mm", ["B28", "D28"], entry=(1, 0)) # entry: the way the wires come in from
L.part("J2", "pads", ["B>", "C>"])                         # edge pads: C< left of column 1, C> right
L.piece("J4", "a", "header_male", ["A3", "A4"], pins=[2, 1])  # one housing of a header in pieces
L.bare("A1", "A5"); L.tin("A5", "A6"); L.link("C3", "C7"); L.wire("J4", "J24", via=["I4"])
L.net("GND", "J1.3", "R41.2", color="black", label="GND")
L.limit("C1.1", "J2.1", 20, fig="place_filter")
L.figure("5 V", "v_5v")                                    # a label that states a model's figure
LAYOUT = L
```

`netlist` names the subsystem's model and the board it declares, as [`tools/CLAUDE.md`](../CLAUDE.md#boards) describes. Its nets stand in for `L.net`, and a resistor takes its value and its `fig` from there. The check reports a part placed here and missing there or the other way round, another inventory part, another value, and a net the layout declares itself. A layout of a board no model declares yet names its nets with `L.net`. A pin is named `REF.PIN`: `1` and `2` for a resistor or a ceramic, `K`/`A` for a diode, `+`/`-` for an electrolytic, `G`/`D`/`S` for a TO-220, the pin number for an IC, a header, a terminal, a module and the pads. A `tab` of +1 puts the TO-220's metal tab to the left of the way from pin 1 to pin 3 on the drawing. A value label that states a figure of a subsystem's model carries that figure's key in `fig`, so figcheck reads the drawing. A part's value comes with its key from the netlist. A net label such as a rail's "5 V" is declared with `L.figure(text, key)`, and every text of the drawing that holds it carries the key.

An adapter placed with `carries=True` holds the chip the netlist declares under the same designator. The adapter's pin n is the chip's pin n, so its pins take the chip's names from the netlist, and the check compares the chip's entry with the netlist's. `L.offboard(ref, how)` names a part of the netlist that sits off the board, such as a module on a cable. The check then expects it unplaced, and the Parts table lists it with `how`. `L.unfitted(ref, joins, why)` names an optional part of the netlist the build leaves out: the nets on its two `joins` pins become one net that the copper makes in its place, the nets on its other pins go with it, and the Parts table lists it as not fitted.

`L.piece(ref, name, key, holes, pins)` places one piece of a header the netlist declares as one part, and `pins` names the netlist's pins of that piece in the order of `holes`. The pieces of one designator together hold every pin once, so a header can take its pins from both rows of the Teensy, its cable meeting each piece with a housing. A single piece places a whole header in a pin order other than its numbering. A module joins its pins of one name on its own board, such as the Teensy's three GND pins, so the copper needs to reach only one of them.
