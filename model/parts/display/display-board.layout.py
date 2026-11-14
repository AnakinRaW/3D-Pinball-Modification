"""The display board on a 6 x 28 perfboard cut to 6 x 14. The netlist is the display board of
schematic.py.

    python tools/perfboard/build.py model/parts/display/display-board.layout.py

The display module and its bus cable stay off the board. The board carries the level converter
A2 standing across it, its 3.3 V side to the right toward the Teensy's touch header, its 5 V side
to the left toward the module's header and the supply pads. The Teensy's lines run straight
across. On the module's side 5 V crosses the signal pair once in an insulated wire, since the
header keeps 5 V and GND off mirrored pins.
"""
from layout import Layout

L = Layout("6x28", title="Display board", cut=(1, 14),
           netlist=("model/parts/display/figures.py", "display"))
L.figure("5 V", "lighting:v_rail")
L.offboard("A1", "the display module, on the cables from J-DISP-MOD and J-DISP-BUS")
L.offboard("J-DISP-BUS", "the bus cable, from the Teensy straight to the module's header")

L.part("A2", "adafruit757", at="A10", rot=90)
L.part("J-DISP-MOD", "header_male", ["E1", "D1", "C1", "B1"], title="J-DISP-MOD",
       sub="to the display module")
L.part("J-TOUCH", "header_male", ["D14", "E14", "F14"], title="J-TOUCH", sub="to the Teensy")
L.part("J-DISP-PWR", "pads", ["C<", "B<"], title="J-DISP-PWR", sub="5 V and GND from the power board")

# the 5 V side: the signal pair straight across, ground over the top, 5 V in an insulated wire;
# the supply pads sit right beside the header's supply pins
L.bare("D6", "D1")                       # SCL
L.bare("E6", "E1")                       # SDA
L.bare("A6", "A2")                       # ground of the converter
L.tin("A2", "B2")
L.tin("B2", "B1")
L.tin("B<", "B1")
L.tin("C<", "C1")
L.tin("F6", "F5")
L.wire("F5", "C2", side="bottom")
L.tin("C2", "C1")
# the 3.3 V side: all three lines straight across to the header
L.bare("D10", "D14")                     # SCL
L.bare("E10", "E14")                     # SDA
L.bare("F10", "F14")                     # 3V3
LAYOUT = L
