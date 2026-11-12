"""The sense board on a 6 x 28 perfboard. The netlist is the sense board of schematic.py.

    python tools/perfboard/build.py model/parts/solenoid/sense-board.layout.py
"""
from layout import Layout

L = Layout("6x28", title="Sense board", netlist=("model/parts/solenoid/figures.py", "sense"))
L.part("J-SENSE-TEENSY", "header_male", ["B1", "C1", "D1", "E1", "F1"], title="J-SENSE-TEENSY",
       sub="to the Teensy")
L.part("J-M", "pads", ["B>", "C>", "D>", "E>"], title="J-M", sub="playfield wires on the edge pads")
L.part("R41", "resistor", ["B2", "B6"])
L.part("R42", "resistor", ["C5", "C9"])
L.part("R43", "resistor", ["D2", "D6"])
L.part("R91", "resistor", ["E5", "E9"])
L.part("C1", "ceramic_k15", ["B28", "A28"])
L.part("R31", "resistor", ["B26", "F26"])
L.part("C2", "ceramic_k15", ["C24", "A24"])
L.part("R32", "resistor", ["C28", "F28"])
L.part("C3", "ceramic_k15", ["D24", "F24"])
L.part("R33", "resistor", ["D22", "A22"])

L.wire("A12", "F12")
for a, b in (("B1", "B2"), ("C1", "C5"), ("D1", "D2"), ("E1", "E5"), ("F1", "F28"), ("A12", "A28"),
             ("B6", "B28"), ("C9", "C28"), ("D6", "D28"), ("E9", "E28")):
    L.bare(a, b)
for a, b in (("B28", "B>"), ("C28", "C>"), ("D28", "D>"), ("E28", "E>")):
    L.tin(a, b)

for n, (c, r) in enumerate((("C1", "R31"), ("C2", "R32"), ("C3", "R33")), 1):
    for ref in (c, r):
        L.limit(f"{ref}.1", f"J-M.{n}", 20, fig="place_filter")
LAYOUT = L
