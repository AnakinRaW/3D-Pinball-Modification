"""The power board on an 18 x 24 perfboard. The netlist is the power board of schematic.py.

    python tools/perfboard/build.py model/parts/power/power-board.layout.py

Two thick rails cross the board: ground along row H, 5 V along row J. J-IN feeds them from the
left edge and J-SOL takes the coils' current at the right edge, C1 and C2 standing on the rails
beside J-IN with nothing tapping the rails between them. Q1 and R1 are left out, so ground runs
straight onto J-IN's negative. The fuse sits below the rails right of C2 and feeds the fused rail
along row P. The four fused terminals stand along the bottom edge in mirrored pairs, so the
ground pins of a pair sit side by side, a free row below the fused rail, and reach the ground
rail right of C2 in one insulated wire. The lights' pads sit above J-SOL at the right end, 5 V
under ground, each taking its 5 V across the ground rail in an insulated wire.
"""
from layout import Layout

L = Layout("18x24", title="Power board", netlist=("model/parts/power/figures.py", "power"))
L.figure("5 V", "lighting:v_rail")
L.figure("1.3 mm", "d_rail_wire")

L.unfitted("Q1", joins=("D", "S"), why="the optional reverse-polarity switch, left out: the "
           "ground rail runs straight onto J-IN's negative")
L.unfitted("R1", why="Q1's gate resistor, left out with Q1")

# the rails, from J-IN at the left edge to J-SOL at the right edge
L.part("J-IN", "terminal_5mm", ["J1", "H1"], entry=(-1, 0), title="J-IN", sub="the adapter")
L.part("J-SOL", "terminal_5mm", ["J24", "H24"], entry=(1, 0), title="J-SOL",
       sub="the coil board")
L.bare("J1", "J24", gauge="1.3 mm")      # 5 V
L.bare("H1", "H24", gauge="1.3 mm")      # ground
L.part("C1", "electrolytic_zlh_8x20", ["J5", "H5"])
L.part("C2", "electrolytic_zlh_8x20", ["J9", "H9"])

# the fuse, from the 5 V rail right of C2 down to the fused rail along row P
L.part("F1", "fuse_holder_5x20", ["M12", "M21"])
L.bare("M12", "J12", gauge="1.3 mm")
L.bare("M21", "P21")
L.bare("P6", "P23")

# the fused terminals along the bottom edge in mirrored pairs: each 5 V pin up onto row P, the
# two ground pins in the middle of a pair joined along row R, a free row below the fused rail,
# and taken up to the ground rail right of C2 in one insulated wire
for x, ref, sub, flip in ((6, "J-SERVO", "the servo board", False),
                          (11, "J-MAIN", "the motherboard", True),
                          (16, "J-IR", "the IR board", False),
                          (21, "J-DISP", "the display board", True)):
    v, g = (x + 2, x) if flip else (x, x + 2)
    L.part(ref, "terminal_5mm", [f"R{v}", f"R{g}"], entry=(0, 1), title=ref, sub=sub)
    L.bare(f"R{v}", f"P{v}")
L.bare("R8", "R11")
L.tin("R10", "Q10")
L.wire("Q10", "G10")
L.tin("G10", "H10")
L.bare("R18", "R21")
L.tin("R20", "Q20")
L.wire("Q20", "G20")
L.tin("G20", "H20")

# the lights at the right end above J-SOL, each pair 5 V under ground with a free row between
# the pairs: ground straight onto the rail, 5 V out into the open board and across the ground
# rail in an insulated wire, each in a separate column
L.part("J-LIGHT1", "pads", ["E>", "F>"], title="J-LIGHT1", sub="the ambient board")
L.part("J-LIGHT2", "pads", ["B>", "C>"], title="J-LIGHT2", sub="the playfield board")
L.tin("F>", "F24")
L.bare("F24", "H24")
L.tin("E>", "E24")
L.bare("E24", "E22")
L.tin("E22", "E21")
L.wire("E21", "K21")
L.tin("K21", "J21")
L.tin("C>", "C24")
L.bare("C24", "C18")
L.bare("C18", "H18")
L.tin("B>", "B24")
L.bare("B24", "B15")
L.tin("B15", "B14")
L.wire("B14", "K14")
L.tin("K14", "J14")

L.limit("C1.+", "J-IN.1", 21, fig="place_bulk")
L.limit("C2.+", "J-IN.1", 21, fig="place_bulk")
LAYOUT = L
