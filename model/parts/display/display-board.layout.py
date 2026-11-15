"""The display board on an 18 x 24 perfboard. The netlist is the display board of schematic.py.

    python tools/perfboard/build.py model/parts/display/display-board.layout.py

The flat flex cable from the display ends in the socket of the FPC board A3, which stands on a
2 x 10 socket header at the board's right end, its socket at the top. Its pin n carries pin n of
the module's 8-bit header. The even pins' lines step under A3 in bare wire and turn left to a
Dupont housing at column 10. The odd pins' lines run straight down to the housings on the bottom
edge. SCL, SDA and 5 V go on to the level converter A2 at the left end, which passes the touch
lines to the header on the left edge. The supply header sits on the bottom edge below A3, its
5 V and ground a free column apart.

Pin 6 of A3 stays unconnected: RD takes 5 V from Vin over a bridge at the module, so Vin's line
alone carries the supply. Pins 7 to 9, RST, Lite and IRQ at the module, stay unconnected too.
"""
from layout import Layout

L = Layout("18x24", title="Display board",
           netlist=("model/parts/display/figures.py", "display"))
L.figure("5 V", "lighting:v_rail")
L.offboard("A1", "the display module, on the flat flex cable and A4")
L.offboard("A4", "the FPC board at the module, the cable's other end")

L.part("A3", "quarkzman_fpc20", at="K13", rot=0)
L.part("A2", "adafruit757", at="O3", rot=270)
L.piece("J-TOUCH", "a", "header_male", ["J1", "K1", "L1"], pins=[3, 1, 2], title="J-TOUCH",
        sub="to the Teensy")
L.piece("J-DISP-BUS", "a", "header_male", [f"{r}10" for r in "BCDEF"], pins=[11, 7, 4, 2, 1],
        title="J-DISP-BUS", sub="to the Teensy")
L.piece("J-DISP-BUS", "b", "header_male", ["R14", "R15", "R16"], pins=[10, 6, 9],
        title="J-DISP-BUS", sub="to the Teensy")
L.piece("J-DISP-BUS", "c", "header_male", ["R19", "R20", "R21", "R22"], pins=[3, 5, 8, 12],
        title="J-DISP-BUS", sub="to the Teensy")
L.part("J-DISP-PWR", "header_male", ["R10", "R11"], title="J-DISP-PWR",
       sub="5 V and GND from the power board")

# the even pins' lines step under A3 and turn left: the data, ground, SCL, D/C and 5 V
UP = {22: "B", 21: "C", 20: "D", 19: "E", 18: "F", 17: "G", 14: "H", 13: "I"}
for c, r in UP.items():
    L.bare(f"J{c}", f"{r}{c}")
for c in (22, 21, 20, 19):
    L.bare(f"{UP[c]}{c}", f"{UP[c]}10")
L.bare("F18", "F4")
L.bare("G17", "G9")
L.bare("H14", "H11")
L.bare("I13", "I7")
# the odd pins' lines run straight down to the housings on the bottom edge, SDA part of the way
for c in (14, 15, 19, 20, 21, 22):
    L.bare(f"K{c}", f"R{c}")
L.bare("K18", "M18")

# ground down to the converter's HV side, as the schematic wires it
L.bare("F4", "Q4")
L.bare("O4", "O6")
L.tin("O6", "O7")
# 5 V into the converter's HV and down to the supply header, a free column from ground
L.tin("I7", "J7")
L.bare("I10", "R10")
# pin 1's ground down to the supply header, and in a wire across to the ground on the left
L.bare("K13", "R13")
L.tin("R13", "R12")
L.tin("R12", "R11")
L.tin("Q13", "Q12")
L.wire("Q12", "Q5")
L.tin("Q5", "Q4")
# D/C on the top side, left of A3, down to its housing
L.tin("H11", "H10")
L.wire("H10", "Q16", via=["Q10"], side="top")
L.tin("Q16", "R16")
# the converter's 5 V side: SCL in a wire to channel 1, SDA in a wire to channel 2
L.tin("G9", "H9")
L.wire("H9", "K8", via=["K9"])
L.tin("K8", "K7")
L.tin("M18", "N18")
L.wire("N18", "L8", via=["N8"])
L.tin("L8", "L7")

# the touch bus's 3.3 V side to the left edge
for r in "JKL":
    L.tin(f"{r}3", f"{r}2")
    L.tin(f"{r}2", f"{r}1")

LAYOUT = L
