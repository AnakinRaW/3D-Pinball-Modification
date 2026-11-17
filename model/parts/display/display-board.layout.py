"""The display board on an 18 x 24 perfboard. The netlist is the display board of schematic.py.

    python tools/perfboard/build.py model/parts/display/display-board.layout.py

The flat flex cable from the display ends in the socket of the FPC board A3, which stands on a
2 x 10 socket header at the board's right end, its socket at the top. Its pin n carries pin n of
the module's 8-bit header. The odd pins sit on A3's inner row. Their lines step up under A3 in
bare wire and turn left: the data, WR and CS to Dupont housings at column 10, ground on to the
level converter A2 at the left end, SDA on in an insulated wire to the converter's channel 2. The
even pins' lines run straight down to the housing on the bottom edge. 5 V turns left to the
converter's HV and down to the supply header, and SCL leaves in an insulated wire to the
converter's channel 1. A2 passes the touch lines to the header on the left edge.

Pin 6 of A3 stays unconnected: RD takes 5 V from Vin over a bridge at the module, so Vin's line
alone carries the supply. Pins 7 to 9, RST, Lite and IRQ at the module, stay unconnected too.
"""
from layout import Layout

L = Layout("18x24", title="Display board",
           netlist=("model/parts/display/figures.py", "display"))
L.figure("5 V", "lighting:v_rail")
L.offboard("A1", "the display module, on the flat flex cable and A4")
L.offboard("A4", "the FPC board at the module, the cable's other end")

# A3's odd pins on row K, its even pins on row L, pin 1 and 2 in column 13
L.part("A3", "quarkzman_fpc20", at="K13", rot=0)
L.part("A2", "adafruit757", at="O3", rot=270)
L.piece("J-TOUCH", "a", "header_male", ["J1", "K1", "L1"], pins=[3, 1, 2], title="J-TOUCH",
        sub="to the Teensy")
L.piece("J-DISP-BUS", "a", "header_male", [f"{r}10" for r in "BCDE"], pins=[12, 8, 5, 3],
        title="J-DISP-BUS", sub="to the Teensy")
L.piece("J-DISP-BUS", "b", "header_male", ["G10", "H10"], pins=[6, 10],
        title="J-DISP-BUS", sub="to the Teensy")
L.piece("J-DISP-BUS", "c", "header_male", [f"R{c}" for c in range(17, 23)],
        pins=[9, 1, 2, 4, 7, 11], title="J-DISP-BUS", sub="to the Teensy")
L.part("J-DISP-PWR", "header_male", ["R10", "R11"], title="J-DISP-PWR",
       sub="5 V and GND from the power board")

# the odd pins' lines step up under A3 and turn left: D6, D4, D2, D0, SDA, WR, CS and ground
UP = {22: "B", 21: "C", 20: "D", 19: "E", 18: "F", 15: "G", 14: "H", 13: "I"}
for c, r in UP.items():
    L.bare(f"K{c}", f"{r}{c}")
for c in (22, 21, 20, 19, 15, 14):
    L.bare(f"{UP[c]}{c}", f"{UP[c]}10")
L.bare("F18", "F12")
L.bare("I13", "I4")
# the even pins' lines run straight down: D7, D5, D3, D1, ground and D/C
for c in (18, 19, 20, 21, 22):
    L.bare(f"L{c}", f"R{c}")
L.bare("L14", "Q14")
L.bare("Q14", "Q17")
L.tin("Q17", "R17")

# ground down to the converter's HV side, as the schematic wires it, and on to the supply header
L.bare("I4", "Q4")
L.bare("O4", "O6")
L.tin("O6", "O7")
L.tin("Q4", "Q5")
L.wire("Q5", "Q11")
L.tin("Q11", "R11")
# pin 12's ground over to the supply header's
L.tin("N18", "N17")
L.wire("N17", "R12", via=["N12"])
L.tin("R12", "R11")
# 5 V left of A3 up to row J, into the converter's HV and down to the supply header
L.tin("L13", "L12")
L.bare("L12", "J12")
L.bare("J12", "J8")
L.tin("J8", "J7")
L.bare("J10", "R10")
# the converter's 5 V side: SDA in a wire to channel 2, SCL in a wire to channel 1
L.tin("F12", "F11")
L.wire("F11", "L8", via=["F9", "L9"])
L.tin("L8", "L7")
L.tin("L17", "M17")
L.wire("M17", "K8", via=["M11", "K11"])
L.tin("K8", "K7")

# the touch bus's 3.3 V side to the left edge
for r in "JKL":
    L.tin(f"{r}3", f"{r}2")
    L.tin(f"{r}2", f"{r}1")

LAYOUT = L
