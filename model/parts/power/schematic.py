"""The power board, pin by pin. figures.py loads this file, and the checks of docs/parts/power/
read it from there. No net of the board reaches a Teensy pin.

Connector pin orders: J-IN and every output carry 5 V on pin 1 and GND on pin 2. J-IN's GND is
the adapter's negative, which reaches the board's ground through Q1. J-LIGHT1 and J-LIGHT2 are
soldered into the edge pads, the other outputs are screw terminals.
"""
from figcheck import Schematic

SCHEMATIC = Schematic()

POWER = SCHEMATIC.board("power", "Power board")
POWER.part("J-IN", "terminal_5mm", pins=2)
POWER.part("Q1", "stp160n3ll", type="STP160N3LL")
POWER.part("R1", "resistor", value="r_gate")
POWER.part("C1", "electrolytic_zlh_8x20", type="Rubycon 16ZLH1000MEFC8X20", value="c_one")
POWER.part("C2", "electrolytic_zlh_8x20", type="Rubycon 16ZLH1000MEFC8X20", value="c_one")
POWER.part("F1", "fuse_holder_5x20", type="T2A fuse", value="i_f1")
POWER.part("J-LIGHT1", "pads", pins=2)
POWER.part("J-LIGHT2", "pads", pins=2)
for ref in ("J-SOL", "J-SERVO", "J-MAIN", "J-IR", "J-DISP"):
    POWER.part(ref, "terminal_5mm", pins=2)

UNFUSED = ("J-LIGHT1", "J-LIGHT2", "J-SOL")
FUSED = ("J-SERVO", "J-MAIN", "J-IR", "J-DISP")
POWER.net("5V", "J-IN.1", "C1.+", "C2.+", "R1.2", "F1.1", *(f"{j}.1" for j in UNFUSED),
          color="red", label="5 V", rail="5V")
POWER.net("5V-FUSED", "F1.2", *(f"{j}.1" for j in FUSED), color="red", label="5 V",
          rail="5V")
POWER.net("ADAPTER-", "J-IN.2", "Q1.D", color="black", label="GND")
POWER.net("GATE", "Q1.G", "R1.1", color="red", label="G")
POWER.net("GND", "Q1.S", "C1.-", "C2.-", *(f"{j}.2" for j in UNFUSED + FUSED),
          color="black", label="GND", rail="gnd")
