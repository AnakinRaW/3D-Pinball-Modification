"""The power board on an 18 x 24 perfboard. The netlist is the power board of schematic.py.

    python tools/perfboard/build.py model/parts/power/power-board.layout.py

Two thick rails cross the board: 5 V along row I, ground along row K. J-IN feeds them from the
left edge and J-SOL takes the coils' current at the right edge, C1 and C2 standing on the rails
beside J-IN with nothing tapping the rails between them. Q1 and R1 are left out, so ground runs
straight onto J-IN's negative. The fuse sits above the rails right of C2 and feeds the fused rail
along row C. The four fused terminals stand along the top edge in mirrored pairs, so the ground
pins of a pair sit side by side, a free row above the fused rail, and reach the ground rail right
of C2 in one insulated wire. The lights' pads sit below J-SOL at the right end, ground over 5 V,
each taking its 5 V across the ground rail in an insulated wire.
"""
from layout import Layout

L = Layout("18x24", title="Power board", netlist=("model/parts/power/figures.py", "power"))
L.figure("5 V", "lighting:v_rail")
L.figure("1.3 mm", "d_rail_wire")

L.unfitted("Q1", joins=("D", "S"), why="the optional reverse-polarity switch, left out: the "
           "ground rail runs straight onto J-IN's negative")
L.unfitted("R1", why="Q1's gate resistor, left out with Q1")

# the rails, from J-IN at the left edge to J-SOL at the right edge
L.part("J-IN", "terminal_5mm", ["I1", "K1"], entry=(-1, 0), title="J-IN", sub="the adapter")
L.part("J-SOL", "terminal_5mm", ["I24", "K24"], entry=(1, 0), title="J-SOL",
       sub="the coil board")
L.bare("I1", "I24", gauge="1.3 mm")      # 5 V
L.bare("K1", "K24", gauge="1.3 mm")      # ground
L.part("C1", "electrolytic_zlh_8x20", ["I5", "K5"])
L.part("C2", "electrolytic_zlh_8x20", ["I9", "K9"])

# the fuse, from the 5 V rail right of C2 up to the fused rail along row C
L.part("F1", "fuse_holder_5x20", ["F12", "F21"])
L.bare("F12", "I12", gauge="1.3 mm")
L.bare("F21", "C21")
L.bare("C6", "C23")

# the fused terminals along the top edge in mirrored pairs: each 5 V pin down onto row C, the
# two ground pins in the middle of a pair joined along row A, a free row above the fused rail,
# and taken down to the ground rail right of C2 in one insulated wire
for x, ref, sub, flip in ((6, "J-SERVO", "the servo board", False),
                          (11, "J-MAIN", "the motherboard", True),
                          (16, "J-IR", "the IR board", False),
                          (21, "J-DISP", "the display board", True)):
    v, g = (x + 2, x) if flip else (x, x + 2)
    L.part(ref, "terminal_5mm", [f"A{v}", f"A{g}"], entry=(0, -1), title=ref, sub=sub)
    L.bare(f"A{v}", f"C{v}")
L.bare("A8", "A11")
L.tin("A10", "B10")
L.wire("B10", "L10")
L.tin("L10", "K10")
L.bare("A18", "A21")
L.tin("A20", "B20")
L.wire("B20", "L20")
L.tin("L20", "K20")

# the lights at the right end below J-SOL, each pair ground over 5 V with a free row between
# the pairs: ground straight onto the rail, 5 V out into the open board and across the ground
# rail in an insulated wire of its own column
L.part("J-LIGHT1", "pads", ["N>", "M>"], title="J-LIGHT1", sub="the ambient board")
L.part("J-LIGHT2", "pads", ["Q>", "P>"], title="J-LIGHT2", sub="the playfield board")
L.tin("M>", "M24")
L.bare("M24", "K24")
L.tin("N>", "N24")
L.bare("N24", "N22")
L.tin("N22", "N21")
L.wire("N21", "H21")
L.tin("H21", "I21")
L.tin("P>", "P24")
L.bare("P24", "P18")
L.bare("P18", "K18")
L.tin("Q>", "Q24")
L.bare("Q24", "Q15")
L.tin("Q15", "Q14")
L.wire("Q14", "H14")
L.tin("H14", "I14")

L.limit("C1.+", "J-IN.1", 21, fig="place_bulk")
L.limit("C2.+", "J-IN.1", 21, fig="place_bulk")
LAYOUT = L
