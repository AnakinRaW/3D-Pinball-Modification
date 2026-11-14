"""The servo board on a 10 x 24 perfboard. The netlist is the servo board of schematic.py.

    python tools/perfboard/build.py model/parts/servo/servo-board.layout.py

The signal runs from the terminal at the left edge through R3 into the buffer U1 and out through
R2 to the servo's plug at the right edge, where C1 holds up the servo's supply. 5 V runs along
the top edge, ground along the bottom.
Ground of U1 runs around its socket, so nothing is soldered under it.
"""
from layout import Layout

L = Layout("10x24", title="Servo board", netlist=("model/parts/servo/figures.py", "servo"))
L.figure("5 V", "v_5v")

L.part("J-SERVO-PWR", "terminal_5mm", ["D1", "F1", "H1"], entry=(-1, 0), title="J-SERVO-PWR",
       sub="5 V, GND and the signal in")
L.part("J-SERVO-PLUG", "header_male", ["I24", "H24", "G24"], title="J-SERVO-PLUG",
       sub="the servo")
L.part("U1", "cd74hct125e", pin1="F10", along=(1, 0))
L.part("C1", "electrolytic_esl_c3", ["H22", "I22"])
L.part("C2", "ceramic_k15", ["C8", "D8"])
L.part("R1", "resistor", ["H4", "J4"], standing=True)
L.part("R3", "resistor", ["H7", "H11"])
L.part("R2", "resistor", ["H12", "H17"])

# 5 V: terminal up to the rail along the top; C2 and U1's pin 14 hang from it
L.tin("D1", "D2")
L.bare("D2", "A2")
L.bare("A2", "A20")
L.tin("A20", "A21")
L.bare("A8", "C8")
L.bare("A10", "C10")
# ground: terminal down to the rail along the bottom; C2 and R1 stand on it
L.bare("F1", "F2")
L.tin("F2", "F3")
L.wire("F3", "J3", side="bottom")
L.tin("J3", "J4")
L.bare("J4", "J22")
L.bare("D8", "J8")
# ground of U1, around its socket: pin 1 and pin 7 straight down onto the rail, pins 4 and 5
# joined beside each other, pins 9 to 13 from above over row B and down column 14 past pin 5
L.bare("J10", "F10")
L.bare("F16", "J16")
L.tin("F13", "F14")
L.tin("C11", "C12")
L.tin("C12", "B12")
L.bare("B12", "B14")
L.bare("B14", "J14")
L.tin("C14", "C15")
# signal: terminal, past R1, into R3; R3 up to pin 2; pin 3 out to R2, which lies across
# the ground drops; R2 to the plug
L.bare("H1", "H7")
L.bare("F11", "H11")
L.bare("F12", "H12")
L.tin("H17", "G17")
L.bare("G17", "G24")
# the plug's 5 V from the rail past C1's plus, its ground past C1's minus to the rail
L.wire("A21", "H21", side="bottom")
L.tin("H21", "H22")
L.bare("H22", "H24")
L.tin("J22", "I22")
L.bare("I22", "I24")

L.limit("C2.1", "U1.14", 10, fig="place_bypass")
L.limit("C1.+", "J-SERVO-PLUG.2", 20, fig="place_bulk")
LAYOUT = L
