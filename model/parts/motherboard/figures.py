"""The figures of the Teensy motherboard: it derives none and repeats the values the subsystems it carries set."""
from figcheck import *

MODEL = Model.at(__file__, "motherboard", drawings=["motherboard-schematic.svg"])
MODEL.uses("v_rail", of="lighting")   # the machine's 5 V, which J-MB-PWR brings

# the parts on the board, each set by the subsystem it serves
MODEL.uses("r_s", of="controls")
MODEL.uses("r_e", of="break-beam")
MODEL.uses("r_pu", of="break-beam")
MODEL.uses("r_gain", of="audio")
MODEL.uses("r_pu_board", of="port-expander")
MODEL.uses("c1", of="port-expander")
