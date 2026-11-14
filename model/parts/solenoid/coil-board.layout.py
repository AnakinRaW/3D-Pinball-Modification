"""The coil board on an 18 x 24 perfboard. The netlist is the coil board of schematic.py.

    python tools/perfboard/build.py model/parts/solenoid/coil-board.layout.py

Each channel takes five columns. The flyback diode lies on the 5 V rail of row A, the coil header
sits under its anode, and the MOSFET stands below with its tab toward the bottom edge. R1 hangs
from the gate, R2 sits two columns right of it, and the source jogs one column right under the
MOSFET's body and runs down to the ground rail of row Q. Every net of a channel is one chain of
copper without a branch; only the rails take branches. Power enters at the left edge, and
each trigger runs left in a separate row to the Teensy header at the left edge.
"""
from layout import Layout

L = Layout("18x24", title="Coil board", netlist=("model/parts/solenoid/figures.py", "coil"))
L.figure("5 V", "v_5v")


def h(row, idx):
    return f"{row}{idx + 1}"


# power entry: terminal on the left edge, C91 above it, the Teensy header below it
L.part("J-COIL-PWR", "terminal_5mm", ["H1", "J1"], entry=(-1, 0), title="J-COIL-PWR",
       sub="5 V and GND in")
L.part("C91", "electrolytic_esl_c3", ["E2", "E3"])
L.tin("H1", "H2")
L.bare("H2", "A2")                       # 5 V up past C91's plus to the rail
L.bare("A2", "A22")                      # 5 V rail
L.tin("E3", "E4")                        # C91 minus
L.bare("E4", "Q4")                       # down to the ground rail
L.bare("J1", "J4")                       # terminal GND onto it
L.bare("Q4", "Q24")                      # ground rail

L.part("J-COIL-TEENSY", "header_male", ["O1", "N1", "M1", "L1"], title="J-COIL-TEENSY",
       sub="to the Teensy")

for n in range(1, 5):
    c0 = 4 + 5 * (n - 1)
    c1, c2, c3, c4 = c0 + 1, c0 + 2, c0 + 3, c0 + 4
    L.part(f"D{n}", "diode_do41", [h("A", c2), h("E", c2)])
    L.part(f"J-C{n}", "header_male", [h("F", c1), h("F", c2)], title=f"J-C{n}",
           sub=f"coil {n}")
    L.part(f"Q{n}", "to220", [h("H", c1), h("H", c2), h("H", c3)], tab=-1)
    L.part(f"R1{n}", "resistor", [h("O", c1), h("K", c1)])
    L.part(f"R2{n}", "resistor", [h("K", c3), h("Q", c3)])

    L.tin(h("F", c1), h("F", c0))                     # coil header 5 V
    L.bare(h("F", c0), h("A", c0))                    # up to the rail
    L.bare(h("E", c2), h("H", c2))                    # drain: diode anode, coil header, MOSFET
    L.bare(h("H", c1), h("K", c1))                    # gate down to R1
    L.bare(h("K", c1), h("K", c3))                    # and across to R2
    L.tin(h("H", c3), h("I", c3))                     # source, under the MOSFET's body
    L.tin(h("I", c3), h("I", c4))
    L.bare(h("I", c4), h("Q", c4))                    # down to the ground rail
    lane = "ONML"[n - 1]                              # each trigger runs left in a separate row
    L.tin(f"{lane}2", f"{lane}1")                     # into a hole beside its header pin
    if lane == "O":                                   # channel 1 leaves left, past nothing
        L.tin(h("O", c1), h("O", c0))
        L.wire(h("O", c0), "O2", side="bottom", short=True)
    else:                                             # the others bend up to their row first
        L.tin(h("O", c1), h("O", c2))
        L.wire(h("O", c2), f"{lane}2", via=[h(lane, c2)], side="bottom")

    L.limit(f"R1{n}.2", f"Q{n}.G", 10, fig="place_gate")
    L.limit(f"R2{n}.1", f"Q{n}.G", 10, fig="place_gate")
    L.limit(f"D{n}.A", f"J-C{n}.2", 20, fig="place_flyback")
LAYOUT = L
