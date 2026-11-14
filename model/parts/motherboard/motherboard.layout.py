"""The Teensy motherboard on a 30 x 42 perfboard. The netlist is the main board of schematic.py.

    python tools/perfboard/build.py model/parts/motherboard/motherboard.layout.py

The Teensy lies across the middle with its USB end toward the left edge. Every line of its top
row runs straight up to a header on the top edge and every line of its bottom row straight down
to one on the bottom edge. A header that takes lines from both rows sits in pieces, one piece per
row, and its cable meets each piece with a separate housing. The lines that have to leave at an
end run under the Teensy between its rows of pins: to the left the bus to the port expander board
and the servo's signal, to the right ground and 3.3 V for the sense board and the break beam.
"""
from layout import Layout

L = Layout("30x42", title="Teensy motherboard",
           netlist=("model/parts/motherboard/figures.py", "main"))
L.figure("5 V", "lighting:v_rail")
L.figure("3.3V", "teensy-4.1:v_3v3")

L.part("A1", "teensy41", at="J14", rot=0)

# ---- the top row, straight up to the top edge -------------------------------------------
# ground from the Teensy's first ground pin up to the top edge
L.bare("J15", "A15")
# the display's first housing: GND, D1, D0, D3, D2, D5, D4 and WR; SCL and SDA leave their
# columns free by going down, so D5 and D4 come over from the right in two insulated wires
L.piece("J-MB-DISPLAY", "a", "header_male",
        ["A16", "A17", "A18", "A19", "A20", "A21", "A22", "A23"], pins=[1, 2, 3, 4, 5, 7, 8, 6],
        title="J-MB-DISPLAY", sub="the display, first housing")
L.tin("A15", "A16")
for c in (17, 18, 19, 20, 23):
    L.bare(f"J{c}", f"A{c}")
L.tin("J31", "I31")
L.wire("I31", "B21", via=["I21"])
L.tin("B21", "A21")
L.tin("J32", "I32")
L.wire("I32", "B22", via=["H32", "H22"])
L.tin("B22", "A22")
# the IR isolator board's first housing: CS-B, CS-A, CLOCK, SCK and GND
L.piece("J-IR-TEENSY", "a", "header_male", ["A24", "A25", "A26", "A27", "A28"],
        pins=[4, 3, 5, 1, 8], title="J-IR-TEENSY", sub="the IR isolator board, first housing")
for c in (24, 25, 26, 27):
    L.bare(f"J{c}", f"A{c}")
L.bare("A28", "L28")                     # GND, on down under the Teensy to the right end
# the coil board's first housing: T2, T1 and T4
L.piece("J-COIL-TEENSY", "a", "header_male", ["A29", "A30", "A31"], pins=[2, 1, 4],
        title="J-COIL-TEENSY", sub="the coil board, first housing")
L.bare("J29", "A29")
L.bare("J30", "A30")
L.bare("J33", "G33")
L.bare("G33", "G31")
L.bare("G31", "A31")
# the sense board: S1, S2, S3, GND and 3V3
L.piece("J-SENSE-TEENSY", "a", "header_male", ["A34", "A35", "A36", "A37", "A38"],
        pins=[1, 2, 3, 5, 4], title="J-SENSE-TEENSY", sub="the sense board")
for c in (34, 35, 36):
    L.bare(f"J{c}", f"A{c}")
L.bare("A37", "I37")
L.bare("I37", "I39")
L.tin("A38", "B38")
L.bare("B38", "B42")

# ---- the left end: the bus to the port expander board and the rotary sensor ------------
# INT, SDA, SCL and 3V3 leave under the Teensy along rows O, N, M and L and run up side by side
# to the top edge, ground beside them from the Teensy's ground column; the rotary sensor's header
# sits across SDA, SCL, 3V3 and GND on the way
L.bare("J16", "L16")
L.bare("L16", "L9")
L.bare("L9", "A9")                       # 3V3
L.bare("J21", "M21")
L.bare("M21", "M8")
L.bare("M8", "A8")                       # SCL
L.bare("J22", "N22")
L.bare("N22", "N7")
L.bare("N7", "A7")                       # SDA
L.bare("P16", "O16")
L.bare("O16", "O6")
L.bare("O6", "A6")                       # INT
L.bare("E15", "E10")
L.bare("E10", "A10")                     # GND
L.piece("J-PE-TEENSY", "a", "header_male", ["A6", "A7", "A8", "A9", "A10"], pins=[1, 3, 2, 4, 5],
        title="J-PE-TEENSY", sub="the port expander board")
L.piece("J-MB-ROTARY", "a", "header_male", ["E7", "E8", "E9", "E10"], pins=[1, 2, 3, 4],
        title="J-MB-ROTARY", sub="the rotary sensor")
# D2 sits on the Teensy itself, from the VUSB hole onto the head of the Vin pin; the drawing
# shows it there
L.offboard("D2", "soldered onto the Teensy, from its VUSB hole to its VIN pin", key="diode_do41",
           at=["J14", "K15"])
# the servo's signal straight down to its pin
L.bare("P15", "R15")
L.part("J-MB-SERVO", "header_male", ["R15"], title="J-MB-SERVO", sub="the servo board")

# ---- the left end below: power in, VIN and the emitter, the amplifier --------------------
L.tin("J14", "J13")
L.tin("J13", "J12")
L.wire("J12", "Q12")                     # VIN across the bus
L.part("D1", "diode_do41", ["Q10", "U10"])
L.bare("Q9", "Q11")
L.tin("Q11", "Q12")
L.part("R2", "resistor", ["Q9", "Q5"])
L.piece("J-MB-EMITTER", "a", "header_male", ["R5", "S5"], pins=[1, 2],
        title="J-MB-EMITTER", sub="the break beam's emitter")
L.tin("Q5", "R5")
L.part("J-MB-PWR", "terminal_5mm", ["U1", "S1"], entry=(-1, 0), title="J-MB-PWR",
       sub="5 V from the power board")
L.bare("P14", "Z14")                     # ground down from the Teensy's bottom ground pin
L.bare("S14", "S1")                      # ground across to the terminal
L.tin("U1", "T1")                        # 5 V
L.bare("T1", "T10")
L.tin("T10", "U10")
L.bare("U10", "Y10")                     # 5 V down to the amplifier
L.part("A2", "dfr0954", at="Y10", rot=90)
L.part("R4", "resistor", ["W8", "W4"])
L.tin("W4", "X4")
L.tin("X4", "Y4")
L.bare("T8", "W8")
L.bare("Z14", "Z10")
L.bare("P18", "AA18")
L.bare("AA18", "AA10")
L.bare("P19", "AB19")
L.bare("AB19", "AB10")
L.tin("P17", "Q17")
L.wire("Q17", "AC11", via=["AC17"])       # DIN across LRC and BCLK
L.tin("AC11", "AC10")

# ---- the right end: ground and 3.3 V from under the Teensy ------------------------------
L.bare("L28", "L39")
L.bare("I39", "AD39")                    # ground down the right end
L.bare("N28", "AD28")                    # 3V3 from the Teensy's second 3V3 pin, up and down
L.bare("N28", "N35")
L.tin("N35", "N36")
L.wire("N36", "N41")
L.tin("N41", "N42")
L.bare("B42", "U42")                     # 3V3 down the right end

# ---- the bottom row, straight down to the bottom edge -----------------------------------
L.part("J-MB-AMBIENT", "header_male", ["AD20", "AD21", "AD22"], title="J-MB-AMBIENT",
       sub="chains A to C")
L.part("J-MB-PLAYFIELD", "header_male", ["AD23", "AD24", "AD25"], title="J-MB-PLAYFIELD",
       sub="chains D to F")
for c in range(20, 26):
    L.bare(f"P{c}", f"AD{c}")
# the IR isolator board's second housing: MOSI, MISO and 3V3, one row in from the edge
L.piece("J-IR-TEENSY", "b", "header_male", ["X26", "X27", "X28"], pins=[2, 6, 7],
        title="J-IR-TEENSY", sub="second housing")
L.bare("P26", "X26")
L.bare("P27", "X27")
L.piece("J-MB-TOUCH", "a", "header_male", ["AD28", "AD29", "AD30"], pins=[3, 1, 2],
        title="J-MB-TOUCH", sub="the touch controller")
L.bare("P29", "AD29")
L.bare("P30", "AD30")
L.piece("J-MB-DISPLAY", "b", "header_male", ["AD31", "AD32", "AD33", "AD34"],
        pins=[12, 11, 10, 9], title="J-MB-DISPLAY", sub="second housing")
for c in (31, 32, 33, 34):
    L.bare(f"P{c}", f"AD{c}")
L.piece("J-COIL-TEENSY", "b", "header_male", ["AD35"], pins=[3],
        title="J-COIL-TEENSY", sub="second housing")
L.bare("P35", "AD35")
# the break beam's receiver with its pull-up, and the toggle switch with its series resistor
L.part("J-MB-RECEIVER", "header_male", ["AD36", "AD37"], title="J-MB-RECEIVER",
       sub="the receiver")
L.bare("P36", "AD36")
L.part("R3", "resistor", ["U36", "U42"])
L.bare("P37", "W37")
L.part("R1", "resistor", ["W37", "W41"])
L.bare("W41", "AD41")
L.piece("J-MB-SWITCH", "a", "header_male", ["AD40", "AD41"], pins=[2, 1],
        title="J-MB-SWITCH", sub="the toggle switch")
L.bare("AD37", "AD40")
LAYOUT = L
