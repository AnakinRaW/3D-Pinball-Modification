"""The display board on an 18 x 24 perfboard. The netlist is the display board of schematic.py.

    python tools/perfboard/build.py model/parts/display/display-board.layout.py

The flat flex cable from the display ends in the socket of the FPC board A3, which stands on a
2 x 10 socket header at the board's right end, its socket at the top. Its pin n carries pin 21 - n
of the module's 8-bit header, so the data sit at the left end of A3's header and the control
lines and the supply at the right end. The odd pins sit on A3's inner row. Their lines step up
under A3 in bare wire and turn left: D/C, ground and the odd data lines to a Dupont housing at
column 10, ground on to the level converter A2 at the left end. 5 V runs straight up to the
supply header on the top edge and on in an insulated wire to the converter's HV, SCL leaves in an
insulated wire to the converter's channel 1. The even pins' lines run straight down to the
housings on the bottom edge. SDA leaves in an insulated wire to the converter's channel 2, and the
second ground pin runs up to the supply header. A2 passes the touch lines to the header on the
left edge.

Pin 15 carries RD, which an insulated wire joins to the 5 V line. Pins 12 to 14 carry IRQ, Lite
and RST from the module and stay unconnected on this board.
"""
from layout import Layout

L = Layout("18x24", title="Display board",
           netlist=("model/parts/display/figures.py", "display"))
L.figure("5 V", "lighting:v_rail")
L.offboard("A1", "the display module, on the flat flex cable and A4")
L.offboard("A4", "the FPC board at the module, the cable's other end")

# A3's odd pins on row K, its even pins on row L, pins 1 and 2 in column 13
L.part("A3", "quarkzman_fpc20", at="K13", rot=0)
L.part("A2", "adafruit757", at="O3", rot=270)
L.piece("J-TOUCH", "a", "header_male", ["J1", "K1", "L1"], pins=[3, 1, 2], title="J-TOUCH",
        sub="to the Teensy")
L.piece("J-DISP-BUS", "a", "header_male", [f"{r}10" for r in "CDEFGH"],
        pins=[9, 1, 2, 4, 7, 11], title="J-DISP-BUS", sub="to the Teensy")
L.piece("J-DISP-BUS", "b", "header_male", [f"R{c}" for c in range(13, 17)], pins=[12, 8, 5, 3],
        title="J-DISP-BUS", sub="to the Teensy")
L.piece("J-DISP-BUS", "c", "header_male", ["R20", "R21"], pins=[6, 10],
        title="J-DISP-BUS", sub="to the Teensy")
L.part("J-DISP-PWR", "header_male", ["A22", "A23"], title="J-DISP-PWR",
       sub="5 V and GND from the power board")

# the odd pins' lines step up under A3 and turn left: D/C, ground, D1, D3, D5 and D7
UP = {21: "C", 17: "D", 16: "E", 15: "F", 14: "G", 13: "H"}
for c, r in UP.items():
    L.bare(f"K{c}", f"{r}{c}")
    if c != 17:
        L.bare(f"{r}{c}", f"{r}10")
# 5 V straight up to the supply header
L.bare("K22", "A22")
# the even pins' lines run straight down: D6, D4, D2, D0, WR and CS
for c in (13, 14, 15, 16, 20, 21):
    L.bare(f"L{c}", f"R{c}")

# ground on along row D past its housing pin and down to the converter's HV side, as the
# schematic wires it
L.bare("D17", "D4")
L.bare("D4", "O4")
L.bare("O4", "O6")
L.tin("O6", "O7")
# pin 20's ground up to the supply header, and in a wire over to pin 9's
L.tin("L22", "L23")
L.bare("L23", "A23")
L.tin("D17", "D18")
L.wire("D18", "D24")
L.tin("D24", "D23")
# 5 V in a wire to the converter's HV
L.tin("B22", "B21")
L.wire("B21", "J8", via=["B8"])
L.tin("J8", "J7")
# RD's pin in a wire up to the 5 V line, over D/C's run in row C
L.tin("K20", "J20")
L.wire("J20", "B20")
L.tin("B20", "B21")
# the converter's 5 V side: SCL in a wire to channel 1, SDA in a wire to channel 2
L.tin("K18", "J18")
L.wire("J18", "K8", via=["J9", "K9"])
L.tin("K8", "K7")
L.tin("L17", "M17")
L.wire("M17", "L8", via=["M8"])
L.tin("L8", "L7")

# the touch bus's 3.3 V side to the left edge
for r in "JKL":
    L.tin(f"{r}3", f"{r}2")
    L.tin(f"{r}2", f"{r}1")

LAYOUT = L
