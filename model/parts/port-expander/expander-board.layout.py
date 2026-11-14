"""The port expander board on a 14 x 20 perfboard. The netlist is the expander board of the
motherboard's schematic.py.

    python tools/perfboard/build.py model/parts/port-expander/expander-board.layout.py

J-PE-TEENSY sits at the bottom edge, and its five lines run straight up: ground on the left to a
rail along the top edge, 3V3 beside it to VDD, SCL and SDA into their pins, INT along row M to its
pin. The two pull-ups stand between the bus and 3V3.
U1's eight inputs run straight up to three headers one row in from the top edge, under the
ground rail. Two insulated wires reach the pins between the bus and INT: ground for the address
pins and 3V3 for RESET.
"""
from layout import Layout

L = Layout("14x20", title="Port expander board",
           netlist=("model/parts/motherboard/figures.py", "expander"))


def c(n):
    return n + 3                            # the board's columns, the layout centered


def h(row, n):
    return f"{row}{c(n)}"


L.part("U1", "mcp23008_p", pin1=h("H", 4), along=(1, 0))

# J-PE-TEENSY at the bottom edge: GND, 3V3, SCL, SDA and INT, the mirror of the motherboard's
L.piece("J-PE-TEENSY", "a", "header_male", [h("N", i) for i in (2, 3, 4, 5, 6)],
        pins=[5, 4, 2, 3, 1], title="J-PE-TEENSY", sub="the Teensy motherboard")
L.bare(h("N", 2), h("A", 2))                # ground up to the rail along the top edge
L.bare(h("N", 3), h("L", 3))                # 3V3 up to VDD, K free for RESET's wire
L.tin(h("L", 3), h("K", 3))
L.tin(h("K", 3), h("J", 3))
L.bare(h("J", 3), h("D", 3))
L.tin(h("E", 3), h("E", 4))
L.bare(h("N", 4), h("H", 4))                # SCL
L.bare(h("N", 5), h("H", 5))                # SDA
L.tin(h("N", 6), h("M", 6))                 # INT along row M
L.bare(h("M", 6), h("M", 11))
L.bare(h("M", 11), h("H", 11))
L.part("C1", "ceramic_k15", [h("D", 3), h("D", 2)])

# the bus pull-ups standing between SCL or SDA and 3V3
L.part("R6", "resistor", [h("J", 4), h("J", 3)], standing=True)
L.part("R5", "resistor", [h("L", 5), h("L", 3)], standing=True)

# ground for the address pins and 3V3 for RESET, each across the bus in a wire
L.bare(h("H", 6), h("H", 8))
L.tin(h("I", 1), h("I", 2))
L.wire(h("I", 1), h("I", 6))
L.tin(h("I", 6), h("H", 6))
L.wire(h("K", 3), h("I", 9), via=[h("K", 9)])
L.tin(h("I", 9), h("H", 9))
# VSS onto the ground column at the right end
L.bare(h("H", 12), h("H", 14))
L.bare(h("A", 14), h("H", 14))

# the eight inputs straight up to three headers one row in from the top edge, ground above them
L.bare(h("A", 2), h("A", 14))
L.piece("J-PE-SPARE", "a", "header_male", [h("B", i) for i in (4, 5, 6, 7, 8)],
        pins=[5, 4, 3, 2, 1], title="J-PE-SPARE", sub="E-5 to E-8")
L.tin(h("A", 4), h("B", 4))
for i in (5, 6, 7, 8, 9, 10):
    L.bare(h("E", i), h("B", i))
L.piece("J-PE-TARGETS", "a", "header_male", [h("B", 9), h("B", 10), h("B", 11)],
        pins=[2, 1, 3], title="J-PE-TARGETS", sub="the targets")
L.tin(h("A", 11), h("B", 11))
L.piece("J-PE-STORE", "a", "header_male", [h("B", 12), h("B", 13), h("B", 14)],
        pins=[2, 1, 3], title="J-PE-STORE", sub="the ball store")
L.bare(h("E", 11), h("C", 11))
L.tin(h("C", 11), h("C", 12))
L.tin(h("C", 12), h("B", 12))
L.bare(h("E", 12), h("D", 12))
L.tin(h("D", 12), h("D", 13))
L.bare(h("D", 13), h("B", 13))
LAYOUT = L
