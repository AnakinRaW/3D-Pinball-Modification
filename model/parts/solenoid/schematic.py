"""The solenoid boards, pin by pin. figures.py loads this file, and the perfboard layouts, the
KiCad export and the checks of docs/parts/solenoid/ read it from there.

Connector pin orders: J-COIL-TEENSY carries trigger 1 to 4 on pins 1 to 4. J-SENSE-TEENSY carries
sense 1 to 3, 3V3 and GND on pins 1 to 5. J-COIL-PWR carries 5 V on pin 1 and GND on pin 2. Each
J-Cn carries 5 V on pin 1 and the drain on pin 2. J-M carries shell 1 to 3 on pins 1 to 3 and the
foil on pin 4.
"""
from figcheck import TEENSY_RAIL, Schematic

SCHEMATIC = Schematic()

COIL = SCHEMATIC.board("coil", "Coil board")
COIL.part("J-COIL-TEENSY", "header_male", pins=4)
COIL.part("J-COIL-PWR", "terminal_5mm", pins=2)
COIL.part("C91", "electrolytic_esl_c3", type="KEMET ESL107M016AC3AA", value="c_c91")
for n in range(1, 5):
    COIL.part(f"J-C{n}", "header_male", pins=2)
    COIL.part(f"Q{n}", "to220", type="IRL540N")
    COIL.part(f"D{n}", "diode_do41", type="1N5819")
    COIL.part(f"R1{n}", "resistor", value="r_gate")
    COIL.part(f"R2{n}", "resistor", value="r_gate_pd")
    COIL.net(f"TRIGGER{n}", f"J-COIL-TEENSY.{n}", f"R1{n}.1", color="blue",
             label=f"TRIGGER {n}", teensy=f"Solenoid trigger {n}")
    COIL.net(f"GATE{n}", f"R1{n}.2", f"R2{n}.1", f"Q{n}.G", color="blue", label=f"GATE {n}")
    COIL.net(f"DRAIN{n}", f"Q{n}.D", f"D{n}.A", f"J-C{n}.2", label=f"DRAIN {n}")
COIL.net("5V", "J-COIL-PWR.1", "C91.+", *(f"D{n}.K" for n in range(1, 5)),
         *(f"J-C{n}.1" for n in range(1, 5)), color="red", label="5 V", rail="5V")
COIL.net("PGND", "J-COIL-PWR.2", "C91.-", *(f"Q{n}.S" for n in range(1, 5)),
         *(f"R2{n}.2" for n in range(1, 5)), color="black", label="GND", rail="gnd")

SENSE = SCHEMATIC.board("sense", "Sense board")
SENSE.part("J-SENSE-TEENSY", "header_male", pins=5)
SENSE.part("J-M", "pads", pins=4, title="playfield wires on the edge pads")
SENSE.part("R91", "resistor", value="r_foil")
for n in range(1, 4):
    SENSE.part(f"R4{n}", "resistor", value="r_series")
    SENSE.part(f"R3{n}", "resistor", value="r_pulldown")
    SENSE.part(f"C{n}", "ceramic_k15", type="K104K15X7RF5TL2", value="c_filter")
    SENSE.net(f"SENSE{n}", f"J-SENSE-TEENSY.{n}", f"R4{n}.1", color="green",
              label=f"SENSE {n}", teensy=f"Solenoid sense {n}")
    SENSE.net(f"SHELL{n}", f"R4{n}.2", f"R3{n}.1", f"C{n}.1", f"J-M.{n}", color="green",
              label=f"SHELL {n}")
    SENSE.contact(f"J-M.{n}", "J-M.4", what=f"the ball between the foil and shell {n}")
SENSE.net("3V3", "J-SENSE-TEENSY.4", "R91.1", color="yellow", label="3V3", rail=TEENSY_RAIL)
SENSE.net("FOIL", "R91.2", "J-M.4", color="yellow", label="FOIL")
SENSE.net("GND", "J-SENSE-TEENSY.5", *(f"R3{n}.2" for n in range(1, 4)),
          *(f"C{n}.2" for n in range(1, 4)), color="black", label="GND", rail="gnd")
