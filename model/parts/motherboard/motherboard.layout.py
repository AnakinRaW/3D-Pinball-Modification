"""The Teensy motherboard on a 30 x 42 perfboard. The netlist is the main board of schematic.py.

    python tools/perfboard/build.py model/parts/motherboard/motherboard.layout.py

The Teensy lies across the middle with its USB end toward the left edge. Every line of its top
row runs up to a header on the top edge and every line of its bottom row straight down to one on
the bottom edge, the headers of the top edge one free column apart. The IR isolator board's
header takes lines from both rows and sits in pieces, one piece per row. The coil board's trigger
3 and the display's D/C, CS, D7 and D6 leave the bottom row and reach their headers on the top
edge in insulated wires around the Teensy's right end, the display's four on the top side, so
they never pass between the Teensy and its sockets. The lines that have to leave at an end run
under the Teensy between its rows of pins: to the left the bus to the port expander board and the
servo's signal, to the right ground and 3.3 V for the sense board and the break beam.
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
# the display in one housing from column 11 to 22: D4, D/C, CS, D7, GND, D6, D1, D0, D3, D2, D5
# and WR. SCL and SDA leave their columns free by going down, so D5 and D4 come over from the
# right in two insulated wires, D4 on to the housing's left end; D/C, CS, D7 and D6 come from the
# bottom row on the top side; WR steps one column left, so column 23 stays free before the IR
# housing
L.piece("J-MB-DISPLAY", "a", "header_male",
        [f"A{c}" for c in range(11, 23)], pins=[8, 9, 10, 11, 1, 12, 2, 3, 4, 5, 7, 6],
        title="J-MB-DISPLAY", sub="the display")
for c in (17, 18, 19, 20):
    L.bare(f"J{c}", f"A{c}")
L.bare("J23", "G23")
L.bare("G23", "G22")
L.bare("G22", "A22")                     # WR
L.tin("J31", "I31")
L.wire("I31", "B21", via=["I21"])
L.tin("B21", "A21")
L.tin("J32", "I32")
L.wire("I32", "B11", via=["H32", "H11"])
L.tin("B11", "A11")
# the IR isolator board's first housing: CS-B, CS-A, CLOCK, SCK and GND
L.piece("J-IR-TEENSY", "a", "header_male", ["A24", "A25", "A26", "A27", "A28"],
        pins=[4, 3, 5, 1, 8], title="J-IR-TEENSY", sub="the IR isolator board, first housing")
for c in (24, 25, 26, 27):
    L.bare(f"J{c}", f"A{c}")
L.bare("A28", "L28")                     # GND, on down under the Teensy to the right end
# the coil board one column right of the IR housing: T2, T1, T4 and T3, the last from the bottom
# row in an insulated wire around the right end; T2 and T1 step one column right, T1 first
L.piece("J-COIL-TEENSY", "a", "header_male", ["A30", "A31", "A32", "A33"], pins=[2, 1, 4, 3],
        title="J-COIL-TEENSY", sub="the coil board")
L.bare("J29", "F29")
L.bare("F29", "F30")
L.bare("F30", "A30")                     # T2
L.bare("J30", "G30")
L.bare("G30", "G31")
L.bare("G31", "A31")                     # T1
L.bare("J33", "G33")
L.bare("G33", "G32")
L.bare("G32", "A32")                     # T4
# the sense board one column right of the coil housing: S1, S2, S3, GND and 3V3; the senses
# step one column right, S3 first, and ground comes down column 38 to the right end's rail
L.piece("J-SENSE-TEENSY", "a", "header_male", ["A35", "A36", "A37", "A38", "A39"],
        pins=[1, 2, 3, 5, 4], title="J-SENSE-TEENSY", sub="the sense board")
for c, r in ((34, "F"), (35, "G"), (36, "H")):
    L.bare(f"J{c}", f"{r}{c}")
    L.bare(f"{r}{c}", f"{r}{c + 1}")
    L.bare(f"{r}{c + 1}", f"A{c + 1}")
L.bare("A38", "I38")
L.bare("I38", "I39")
L.tin("A39", "B39")
L.bare("B39", "B42")

# ---- the left end: the bus to the port expander board and the rotary sensor ------------
# INT, SDA, SCL and 3V3 leave under the Teensy along rows O, N, M and L and run up side by side
# to the top edge, ground beside them from the Teensy's ground column; the rotary sensor's header
# sits across SDA, SCL, 3V3 and GND on the way
L.bare("J16", "L16")
L.bare("L16", "L5")
L.bare("L5", "A5")                       # 3V3
L.bare("J21", "M21")
L.bare("M21", "M4")
L.bare("M4", "A4")                       # SCL
L.bare("J22", "N22")
L.bare("N22", "N3")
L.bare("N3", "A3")                       # SDA
L.bare("P16", "O16")
L.bare("O16", "O2")
L.bare("O2", "A2")                       # INT
L.bare("E15", "E6")
L.bare("E6", "A6")                       # GND
L.piece("J-PE-TEENSY", "a", "header_male", ["A2", "A3", "A4", "A5", "A6"], pins=[1, 3, 2, 4, 5],
        title="J-PE-TEENSY", sub="the port expander board")
L.piece("J-MB-ROTARY", "a", "header_male", ["E3", "E4", "E5", "E6"], pins=[1, 2, 3, 4],
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
# D6, D7, CS and D/C on the top side around the Teensy's right end, nested so they never cross:
# down from the bottom row, right, up beyond the Teensy, left above it and up to their pins
for c in (31, 32, 33, 34):
    L.tin(f"P{c}", f"Q{c}")
for start, low, right, high, end in ((31, "T", 41, "C", 16), (32, "S", 40, "D", 14),
                                     (33, "R", 39, "E", 13), (34, "Q", 38, "F", 12)):
    via = ([f"{low}{start}"] if low != "Q" else []) + [f"{low}{right}", f"{high}{right}",
                                                       f"{high}{end}"]
    L.wire(f"Q{start}", f"B{end}", via=via, side="top")
    L.tin(f"B{end}", f"A{end}")
L.tin("P35", "O35")
L.wire("O35", "C33", via=["O40", "C40"])   # T3 around the right end up to its housing
L.tin("C33", "B33")
L.tin("B33", "A33")
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
