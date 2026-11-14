"""The IR isolator board on a 10 x 24 perfboard. The netlist is the iso board of schematic.py.

    python tools/perfboard/build.py model/parts/ir-reflective/isolator-board.layout.py

U3 lies turned on its adapter, side 1 toward J-IR-TEENSY at the left edge and side 2 toward
J-IR-ISO along the top edge, which faces the IR board. Each of the Teensy's lines runs straight
along its row into side 1, MISO through R34. Each line of side 2 runs right along its row and
turns up to its pin of J-IR-ISO, the upper rows turning first. Two ground rails lie under the
adapter, GND1 beside side 1 and GND2 beside side 2. R36 lies across the side-2 lines from the
3V3 line down to INF's.
"""
from layout import Layout, row_name

L = Layout("10x24", title="IR isolator board",
           netlist=("model/parts/ir-reflective/figures.py", "iso"))


def h(r, c):
    return f"{row_name(r)}{c}"


L.part("U3", "adafruit1207", at=h(1, 7), rot=90, carries=True)

# side 1: J-IR-TEENSY at the left edge, each line straight along its row
L.piece("J-IR-TEENSY", "a", "header_male", [h(r, 1) for r in range(1, 9)],
        pins=[7, 1, 2, 3, 4, 5, 6, 8], title="J-IR-TEENSY", sub="to the Teensy")
for r in (1, 2, 3, 4, 5, 6, 8):
    L.bare(h(r, 1), h(r, 7))
L.part("R34", "resistor", [h(7, 2), h(7, 6)])
L.tin(h(7, 1), h(7, 2))
L.tin(h(7, 6), h(7, 7))
# GND1 under the adapter beside side 1, C3 above VCC1
L.bare(h(1, 8), h(8, 8))
L.tin(h(8, 7), h(8, 8))
L.part("C3", "ceramic_k15", [h(1, 6), h(0, 6)])
L.tin(h(0, 6), h(0, 7))
L.tin(h(0, 7), h(0, 8))
L.tin(h(0, 8), h(1, 8))
L.limit("C3.1", "U3.VCC1", 10, fig="place_decoupling")

# side 2: GND2 under the adapter, C4 above VCC2
L.bare(h(1, 11), h(8, 11))
L.tin(h(8, 13), h(8, 12))
L.tin(h(8, 12), h(8, 11))
L.part("C4", "ceramic_k15", [h(0, 12), h(0, 11)])
L.tin(h(0, 11), h(1, 11))
L.tin(h(0, 12), h(0, 13))
L.tin(h(0, 13), h(1, 13))
L.limit("C4.1", "U3.VCC2", 10, fig="place_decoupling")

# J-IR-ISO along the top edge, 3V3 at its left end beside C4; each line of side 2 right along its
# row and up to its pin, the upper rows turning first, the two leads of a pair tinned together
L.piece("J-IR-ISO", "a", "header_male", [h(0, c) for c in range(13, 24)],
        pins=[10, 1, 5, 2, 6, 3, 7, 9, 4, 8, 11], title="J-IR-ISO", sub="to the IR board")
for row, col in ((2, 15), (3, 17), (4, 18), (5, 19), (6, 20), (7, 22), (8, 23)):
    L.bare(h(row, 13), h(row, col))
    L.bare(h(row, col), h(0, col))
L.tin(h(0, 15), h(0, 14))                               # the two SCK leads
L.tin(h(0, 17), h(0, 16))                               # the two MOSI leads
L.tin(h(0, 22), h(0, 21))                               # the two ADC-DOUT leads
# R36 from the 3V3 pin down to INF's line, its body across the lines between
L.part("R36", "resistor", [h(1, 14), h(7, 14)])
L.tin(h(1, 13), h(1, 14))
VARIANT = ("IR sensing", ["16 channels", "11 channels, 26x31", "11 channels, 30x42"])
LAYOUT = L
