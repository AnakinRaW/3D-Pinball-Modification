"""The servo board, pin by pin. figures.py loads this file, and the checks of docs/parts/servo/
read it from there.

Connector pin orders: J-SERVO-PWR carries 5 V on pin 1, GND on pin 2 and the Teensy's signal on
pin 3. J-SERVO-PLUG, where the servo plugs in, carries GND on pin 1, 5 V on pin 2 and the pulse
on pin 3.
"""
from figcheck import Schematic

SCHEMATIC = Schematic()

SERVO = SCHEMATIC.board("servo", "Servo board")
SERVO.part("J-SERVO-PWR", "terminal_5mm", pins=3)
SERVO.part("J-SERVO-PLUG", "header_male", pins=3)
SERVO.part("U1", "cd74hct125e", type="CD74HCT125E")
SERVO.part("R1", "resistor", value="r_pd")
SERVO.part("R2", "resistor", value="r_series")
SERVO.part("R3", "resistor", value="r_in")
SERVO.part("C1", "electrolytic_esl_c3", type="KEMET ESL107M016AC3AA", value="c_c1")
SERVO.part("C2", "ceramic_k15", value="c_bypass")

SERVO.net("SIG", "J-SERVO-PWR.3", "R1.1", "R3.1", color="blue", label="SIG",
          teensy="Servo signal")
SERVO.net("BUF-IN", "R3.2", "U1.2", color="blue", label="1A")
SERVO.net("BUF-OUT", "U1.3", "R2.1", color="white", label="1Y")
SERVO.net("PULSE", "R2.2", "J-SERVO-PLUG.3", color="white", label="5 V pulse")
SERVO.net("5V", "J-SERVO-PWR.1", "U1.14", "C2.1", "C1.+", "J-SERVO-PLUG.2", color="red",
          label="5 V", rail="5V")
# the three unused buffers have their input and their enable pin on ground
SERVO.net("GND", "J-SERVO-PWR.2", "R1.2", "U1.1", "U1.4", "U1.5", "U1.7", "U1.9", "U1.10",
          "U1.12", "U1.13", "C2.2", "C1.-", "J-SERVO-PLUG.1", color="black", label="GND",
          rail="gnd")
SERVO.unconnected("U1.6", "U1.8", "U1.11",
                  why="the outputs 2Y, 3Y and 4Y of the three unused buffers")
