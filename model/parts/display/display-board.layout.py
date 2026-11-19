"""The display board on an 18 x 24 perfboard. The netlist is the display board of schematic.py.

    python tools/perfboard/build.py model/parts/display/display-board.layout.py

The flat flex cable from the display ends in the socket of the FPC board A3, which stands on a
2 x 10 socket header at the board's right end, its socket at the top. The data sit at the left end
of A3's header, the control lines and the supply at the right end. The odd pins sit on A3's inner
row. Their lines step up under A3 in bare wire and turn left: CS, WR, ground and the odd data lines
to a Dupont housing at column 9, ground on to the level converter A2 at the left end. Pin 19's
ground runs straight up to the supply header on the top edge, and pin 20's 5 V beside it. The
even data lines run straight down to a housing on the bottom edge, D/C along row Q to the same
housing. Insulated wires join RD to 5 V across D/C's line, carry 5 V from the supply header
along row I to the converter's HV, join the two grounds, and take SCL and SDA to the converter's
channels 1 and 2. Each wire leaves its end away from the tin that joins it. A2 passes the touch
lines to the header on the left edge.

Pins 12 to 14 of A3 carry IRQ, RST and Lite from the module and stay unconnected on this board.
"""
from layout import Layout

L = Layout("18x24", title="Display board",
           netlist=("model/parts/display/figures.py", "display"))
L.figure("5 V", "lighting:v_rail")
L.offboard("A1", "the display module, on the flat flex cable and A4")
L.offboard("A4", "the FPC board at the module, the cable's other end")

# A3's odd pins on row K, its even pins on row L, pins 1 and 2 in column 12
L.part("A3", "quarkzman_fpc20", at="K12", rot=0)
L.part("A2", "adafruit757", at="O3", rot=270)
L.piece("J-TOUCH", "a", "header_male", ["J1", "K1", "L1"], pins=[3, 1, 2], title="J-TOUCH",
        sub="to the Teensy")
L.piece("J-DISP-BUS", "a", "header_male", [f"{r}9" for r in "BCDEFGH"],
        pins=[10, 6, 1, 2, 4, 7, 11], title="J-DISP-BUS", sub="to the Teensy")
L.piece("J-DISP-BUS", "b", "header_male", [f"R{c}" for c in range(12, 17)],
        pins=[12, 8, 5, 3, 9], title="J-DISP-BUS", sub="to the Teensy")
L.piece("J-DISP-PWR", "a", "header_male", ["A21", "A22"], pins=[2, 1], title="J-DISP-PWR",
        sub="5 V and GND from the power board")

# the odd pins' lines step up under A3 and turn left: CS, WR, ground, D1, D3, D5 and D7
UP = {20: "B", 19: "C", 16: "D", 15: "E", 14: "F", 13: "G", 12: "H"}
for c, r in UP.items():
    L.bare(f"K{c}", f"{r}{c}")
    if c != 16:
        L.bare(f"{r}{c}", f"{r}9")
# the even pins' lines run down: D6, D4, D2 and D0 straight, D/C along row Q beside them
for c in (12, 13, 14, 15):
    L.bare(f"L{c}", f"R{c}")
L.bare("L20", "Q20")
L.bare("Q20", "Q16")
L.bare("Q16", "R16")

# ground on along row D past its housing pin and down to the converter's HV side, as the
# schematic wires it
L.bare("D16", "D4")
L.bare("D4", "O4")
L.bare("O4", "O6")
L.tin("O6", "O7")
# pin 19's ground straight up to the supply header, and in a wire over to pin 9's
L.bare("K21", "A21")
L.tin("D16", "D17")
L.wire("D17", "A20", via=["A17"])
L.tin("A20", "A21")
# pin 20's 5 V up to the supply header beside it; RD in a wire across D/C's line, 5 V in a
# wire from the header pin down the edge and along row I to the converter's HV
L.tin("L21", "L22")
L.bare("L22", "A22")
L.tin("L19", "M19")
L.wire("M19", "L23", via=["M23"])
L.tin("L23", "L22")
L.tin("A22", "A23")
L.wire("A23", "I7", via=["I23"])
L.tin("I7", "J7")
# the converter's 5 V side: SCL in a wire to channel 1, SDA in a wire to channel 2
L.tin("K17", "J17")
L.wire("J17", "K8", via=["J9", "K9"])
L.tin("K8", "K7")
L.tin("L16", "M16")
L.wire("M16", "L8", via=["M8"])
L.tin("L8", "L7")

# the touch bus's 3.3 V side to the left edge
for r in "JKL":
    L.tin(f"{r}3", f"{r}2")
    L.tin(f"{r}2", f"{r}1")

LAYOUT = L
