"""The port expander board on a 14 x 20 perfboard, built standing on its short edge. The netlist
is the expander board of the motherboard's schematic.py.

    python tools/perfboard/build.py model/parts/port-expander/expander-board.layout.py

U1 stands across the board, its bus pins toward J-PE-TEENSY on the left short edge and its eight
inputs toward the contacts' headers, one column in from the right short edge. 3V3 runs along row
C to VDD with C1 beside it, and down under the socket to RESET. SCL and SDA step down to the
header, and the two pull-ups lie from row C down to them. The address pins, VSS and the header's
ground share column 8, which runs down into one ground rail around the contacts' headers, along
the bottom edge, the right short edge and the top edge; each header's ground pin sits on it. INT
leaves under the socket and reaches the header in an insulated wire. Each input runs right along
its row and steps to its pin, the outer rows stepping first.
"""
from layout import Layout

L = Layout("14x20", title="Port expander board",
           netlist=("model/parts/motherboard/figures.py", "expander"))

L.part("U1", "mcp23008_p", pin1="D9", along=(0, 1))

# J-PE-TEENSY on the left short edge: 3V3, SCL, SDA, GND and INT
L.piece("J-PE-TEENSY", "a", "header_male", ["F1", "G1", "H1", "I1", "J1"],
        pins=[4, 2, 3, 5, 1], title="J-PE-TEENSY", sub="the Teensy motherboard")

# 3V3 up to row C, along it to VDD, and down under the socket to RESET
L.bare("F1", "F3")
L.bare("F3", "C3")
L.bare("C3", "C12")
L.tin("C12", "D12")
L.bare("C10", "I10")
L.tin("I10", "I9")
L.part("C1", "ceramic_k15", ["C11", "B11"])
L.tin("B11", "A11")

# SCL and SDA step down to the header, the pull-ups lie from row C down to them
L.bare("D9", "D6")
L.bare("D6", "G6")
L.bare("G6", "G1")
L.bare("E9", "E7")
L.bare("E7", "H7")
L.bare("H7", "H1")
L.part("R6", "resistor", ["G4", "C4"])
L.part("R5", "resistor", ["H6", "C6"])

# ground: the address pins, VSS and the header on column 8, down into the rail around the
# contacts' headers
L.bare("F9", "H9")
L.tin("F9", "F8")
L.bare("F8", "N8")
L.tin("L9", "L8")
L.bare("I8", "I1")
L.bare("N8", "N20")
L.bare("N20", "A20")
L.bare("A20", "A11")

# INT under the socket and across the ground column in a wire
L.tin("K9", "K10")
L.wire("K10", "J2", via=["M10", "M2"])
L.tin("J2", "J1")

# the inputs right along their rows to their headers, the outer rows stepping first
for row, to, col in (("E", "B", 13), ("F", "C", 14), ("G", "D", 15), ("H", "E", 16),
                     ("I", "G", 17), ("J", "H", 18), ("L", "M", 13), ("K", "L", 14)):
    L.bare(f"{row}12", f"{row}{col}")
    L.bare(f"{row}{col}", f"{to}{col}")
    L.bare(f"{to}{col}", f"{to}19")
L.piece("J-PE-SPARE", "a", "header_male", ["A19", "B19", "C19", "D19", "E19"],
        pins=[5, 4, 3, 2, 1], title="J-PE-SPARE", sub="E-5 to E-8")
L.piece("J-PE-TARGETS", "a", "header_male", ["G19", "H19", "I19"],
        pins=[2, 1, 3], title="J-PE-TARGETS", sub="the targets")
L.tin("I19", "I20")
L.piece("J-PE-STORE", "a", "header_male", ["L19", "M19", "N19"],
        pins=[2, 1, 3], title="J-PE-STORE", sub="the ball store")
LAYOUT = L
