"""The playfield board on a 14 x 20 perfboard. The netlist is the playfield board of
schematic.py.

    python tools/perfboard/build.py model/parts/lighting/playfield-board.layout.py

U1 stands upright in the upper half. Channels 1 and 2 leave its left row of pins and channel 3 its
right row. Each input resistor lies beside its buffer input. Its chain runs up to the Teensy header
at the top edge. Chain F comes around the right edge and along the top.
Ground runs along row I and 5 V along row K, both from the edge pads at the left end. Each output
resistor lies across both rails from row H to row L, so its data line drops straight to its
terminal at the bottom edge. The middle terminal is mirrored. Its ground then sits beside J-LD's
and its 5 V beside J-LF's. One insulated wire takes the joined grounds up across the 5 V rail. A
second one brings 5 V up to U1 and C1.
"""
from layout import Layout

L = Layout("14x20", title="Playfield board",
           netlist=("model/parts/lighting/figures.py", "playfield"))
L.figure("5 V", "v_rail")

L.part("J-PLAYFIELD-TEENSY", "header_male", ["A5", "A6", "A7"], title="J-PLAYFIELD-TEENSY",
       sub="to the Teensy")
L.part("J-PLAYFIELD-PWR", "pads", ["K<", "I<"], title="J-PLAYFIELD-PWR",
       sub="5 V and GND from the power board")
L.part("U1", "cd74hct125e", pin1="B11", along=(0, 1))
L.part("C1", "ceramic_k15", ["B16", "C16"])
L.part("C2", "electrolytic_zlh_8x20", ["K7", "J7"])
L.part("R1", "resistor", ["C5", "C10"])
L.part("R2", "resistor", ["F6", "F10"])
L.part("R3", "resistor", ["G19", "G15"])
L.part("R4", "resistor", ["H4", "L4"])
L.part("R5", "resistor", ["H10", "L10"])
L.part("R6", "resistor", ["H16", "L16"])
L.part("J-LD", "terminal_5mm", ["N2", "N4", "N6"], entry=(0, 1), title="J-LD", sub="chain D")
L.part("J-LE", "terminal_5mm", ["N12", "N10", "N8"], entry=(0, 1), title="J-LE", sub="chain E")
L.part("J-LF", "terminal_5mm", ["N14", "N16", "N18"], entry=(0, 1), title="J-LF", sub="chain F")

# ground along row I, 5 V along row K
L.tin("I<", "I1")
L.bare("I1", "I18")
L.tin("K<", "K1")
L.bare("K1", "K16")
L.tin("K16", "K17")
# the terminals: J-LD's 5 V up to the rail, the two middle grounds joined and taken up in one
# wire, the two middle 5 V pins joined and taken up, J-LF's ground up past the end of row K
L.bare("N2", "K2")
L.tin("N6", "M6")
L.bare("M6", "M7")
L.tin("M7", "M8")
L.tin("M8", "N8")
L.tin("I8", "H8")
L.wire("M8", "H8", side="bottom")
L.tin("N12", "M12")
L.bare("M12", "M14")
L.tin("M14", "N14")
L.bare("M13", "K13")
L.bare("N18", "I18")
# ground of U1 on one rail under its socket: column 12 straight down onto the ground rail, pins
# 1, 4 and 7 tinned onto it from the left, pins 13 and 10 across to it along rows C and F
L.bare("B12", "I12")
L.tin("B11", "B12")
L.tin("E11", "E12")
L.tin("H11", "H12")
L.bare("C14", "C12")
L.tin("C14", "D14")
L.bare("F14", "F12")
L.bare("C14", "C16")                      # C1's ground
L.tin("J7", "I7")
# 5 V of U1 and C1, from the end of row K in an insulated wire
L.bare("B14", "B16")
L.tin("B16", "B17")
L.wire("K17", "B17", side="bottom")
# the chains from the header to the input resistors, and on into the buffer
L.bare("C5", "A5")
L.bare("F6", "A6")
L.bare("A7", "A19")
L.bare("A19", "G19")
L.tin("C11", "C10")
L.tin("F11", "F10")
L.tin("G14", "G15")
# the outputs to the output resistors, 1Y under R2 and along row G
L.bare("D11", "D8")
L.bare("D8", "G8")
L.bare("G8", "G4")
L.tin("G4", "H4")
L.tin("G11", "G10")
L.tin("G10", "H10")
L.bare("H14", "H16")
# the data lines down to the terminals
L.bare("L4", "N4")
L.bare("L10", "N10")
L.bare("L16", "N16")

L.limit("C1.1", "U1.14", 10, fig="place_bypass")
for r, pin in (("R1", "2"), ("R2", "5"), ("R3", "9")):
    L.limit(f"{r}.2", f"U1.{pin}", 10, fig="place_in")
L.limit("C2.+", "J-PLAYFIELD-PWR.1", 20, fig="place_bulk")
LAYOUT = L
