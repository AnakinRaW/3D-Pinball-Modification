"""The Teensy motherboard, pin by pin. figures.py loads this file. Every part on the board is
listed by the subsystem it serves, and its value is that subsystem's figure.

Connector pin orders: J-MB-PWR carries 5 V on pin 1 and GND on pin 2. J-IR-TEENSY, J-COIL-TEENSY
and J-SENSE-TEENSY mate with the headers of the same name on the IR mainboard and the solenoid
boards, pin for pin. J-MB-AMBIENT carries chains A to C and J-MB-PLAYFIELD chains D to F on pins 1
to 3. J-MB-TOUCH carries SCL, SDA and 3V3 on pins 1 to 3. J-MB-DISPLAY carries GND, D1, D0, D3,
D2, WR, D5, D4, D/C, CS, D7 and D6 on pins 1 to 12. J-MB-ROTARY carries SDA, SCL, 3V3 and GND on
pins 1 to 4. J-MB-STORE carries E-1, E-2 and the wall foil's GND, J-MB-TARGETS E-3, E-4 and the
bell foil's GND, on pins 1 to 3. J-MB-SPARE carries E-5 to E-8 on pins 1 to 4 and GND on pin 5. J-MB-EMITTER carries the feed behind R2, J-MB-RECEIVER the
receiver's output and J-MB-SWITCH the switch behind R1 on pin 1, each with GND on pin 2.
J-MB-SERVO is the edge pad the servo board's signal wire is soldered to.
"""
from figcheck import TEENSY_RAIL, Schematic

SCHEMATIC = Schematic()

MB = SCHEMATIC.board("main", "Teensy motherboard")
MB.part("A1", "teensy41", type="Teensy 4.1")
MB.part("A2", "dfr0954", type="DFR0954", listed_in="audio")
MB.part("U1", "mcp23008_p", type="MCP23008-E/P", listed_in="port-expander")
MB.part("D1", "diode_do41", type="1N5819", listed_in="power")
MB.part("D2", "diode_do41", type="1N5819", listed_in="power")
MB.part("R1", "resistor", value="r_s", listed_in="controls")
MB.part("R2", "resistor", value="r_e", listed_in="break-beam")
MB.part("R3", "resistor", value="r_pu", listed_in="break-beam")
MB.part("R4", "resistor", value="r_gain", listed_in="audio")
MB.part("R5", "resistor", value="r_pu_board", listed_in="port-expander")
MB.part("R6", "resistor", value="r_pu_board", listed_in="port-expander")
MB.part("C1", "ceramic_k15", type="K104K15X7RF5TL2", value="c1", listed_in="port-expander")

MB.part("J-MB-PWR", "terminal_5mm", pins=2)
MB.part("J-MB-SERVO", "pads", pins=1, title="the servo board's signal wire on an edge pad")
MB.part("J-MB-AMBIENT", "header_male", pins=3)
MB.part("J-MB-PLAYFIELD", "header_male", pins=3)
MB.part("J-MB-TOUCH", "header_male", pins=3)
MB.part("J-MB-DISPLAY", "header_male", pins=12)
MB.part("J-MB-ROTARY", "header_male", pins=4)
MB.part("J-IR-TEENSY", "header_male", pins=8)
MB.part("J-COIL-TEENSY", "header_male", pins=4)
MB.part("J-SENSE-TEENSY", "header_male", pins=5)
MB.part("J-MB-STORE", "header_male", pins=3)
MB.part("J-MB-TARGETS", "header_male", pins=3)
MB.part("J-MB-SPARE", "header_male", pins=5)
MB.part("J-MB-EMITTER", "header_male", pins=2)
MB.part("J-MB-RECEIVER", "header_male", pins=2)
MB.part("J-MB-SWITCH", "header_male", pins=2)

# -- supply -------------------------------------------------------------------------------------
MB.net("5V", "J-MB-PWR.1", "D1.A", "A2.VCC", "R4.1", color="red", label="5 V", rail="5V")
MB.net("VIN", "D1.K", "D2.K", "A1.Vin", "R2.1", color="red", label="VIN", rail="VIN")
MB.net("GND", "J-MB-PWR.2", "A1.GND", "A1.GND#2", "A1.GND#3", "A2.GND", "U1.3", "U1.4", "U1.5",
       "U1.9", "C1.2", "J-MB-DISPLAY.1", "J-MB-ROTARY.4", "J-IR-TEENSY.8", "J-SENSE-TEENSY.5",
       "J-MB-STORE.3", "J-MB-TARGETS.3", "J-MB-SPARE.5", "J-MB-EMITTER.2", "J-MB-RECEIVER.2", "J-MB-SWITCH.2",
       color="black", label="GND", rail="gnd")
MB.net("3V3", "U1.6", "U1.18", "C1.1", "R3.2", "R5.2", "R6.2",
       "J-MB-TOUCH.3", "J-MB-ROTARY.3", "J-IR-TEENSY.7", "J-SENSE-TEENSY.4",
       "A1.3.3V", "A1.3.3V#2", color="yellow", label="3V3", rail=TEENSY_RAIL)
MB.unconnected("D2.A", why="D2 runs from the Teensy's VUSB hole, which the inventory's teensy41 "
               "entry does not hold among its pins")

# -- servo --------------------------------------------------------------------------------------
MB.net("SERVO", "A1.0", "J-MB-SERVO.1", color="blue", label="servo", teensy="Servo signal")

# -- audio --------------------------------------------------------------------------------------
MB.net("GAIN", "R4.2", "A2.GAIN", color="red", label="GAIN")
for pin, name in ((2, "DIN"), (3, "LRC"), (4, "BCLK")):
    MB.net(name, f"A1.{pin}", f"A2.{name}", color="blue", label=name,
           teensy=f"{name} to the DFR0954")
MB.unconnected("A1.33", why="MCLK, which I2S2 drives and the DFR0954 does not take, "
               "docs/pin-assignment.md")
MB.unconnected("A2.SD", why="the module pulls SD up itself, which plays the left channel, "
               "DFR0954-DFRobot-schematic.pdf")
MB.unconnected("A2.NC", why="NC on the module, DFR0954-DFRobot-schematic.pdf")
MB.unconnected("A2.SPK+", "A2.SPK-", why="the speaker plugs into the module's own PH2.0 socket")
MB.unconnected("A2.VCC#2", "A2.GND#2", why="the module joins them to its first VCC and GND, "
               "DFR0954-DFRobot-schematic.pdf")

# -- lighting -----------------------------------------------------------------------------------
for i, chain in enumerate("ABCDEF"):
    conn = "J-MB-AMBIENT" if i < 3 else "J-MB-PLAYFIELD"
    MB.net(f"CHAIN{chain}", f"A1.{5 + i}", f"{conn}.{i % 3 + 1}", color="blue",
           label=f"chain {chain}", teensy=f"LED chain {chain}")

# -- IR sensing ---------------------------------------------------------------------------------
for pin, sig, n, color in ((13, "SCK", 1, "blue"), (11, "MOSI", 2, "blue"),
                           (15, "CS-A", 3, "blue"), (16, "CS-B", 4, "blue"),
                           (14, "CLOCK", 5, "blue"), (12, "MISO", 6, "green")):
    MB.net(sig, f"A1.{pin}", f"J-IR-TEENSY.{n}", color=color, label=sig, teensy=sig)

# -- display ------------------------------------------------------------------------------------
for pin, sig, n in ((23, "D1", 2), (22, "D0", 3), (21, "D3", 4), (20, "D2", 5), (17, "WR", 6),
                    (39, "D5", 7), (38, "D4", 8), (29, "D/C", 9), (28, "CS", 10), (27, "D7", 11),
                    (26, "D6", 12)):
    MB.net(f"DISP-{sig.replace('/', '')}", f"A1.{pin}", f"J-MB-DISPLAY.{n}", color="blue",
           label=sig, teensy=f"{sig} to the display" if sig != "CS" else
           "CS to the display controller")
MB.net("TOUCH-SCL", "A1.24", "J-MB-TOUCH.1", color="white", label="SCL",
       teensy="SCL to the touch controller")
MB.net("TOUCH-SDA", "A1.25", "J-MB-TOUCH.2", color="white", label="SDA",
       teensy="SDA to the touch controller")

# -- Wire: the port expander and the rotary sensor ----------------------------------------------
MB.net("SDA", "A1.18", "U1.2", "R5.1", "J-MB-ROTARY.1", color="white", label="SDA",
       teensy="SDA on Wire")
MB.net("SCL", "A1.19", "U1.1", "R6.1", "J-MB-ROTARY.2", color="white", label="SCL",
       teensy="SCL on Wire")
MB.net("INT", "A1.1", "U1.8", color="green", label="INT",
       teensy="Interrupt from the port expander")
MB.unconnected("U1.7", why="NC, DS20001919F Table 1-1")

# -- contacts on the port expander --------------------------------------------------------------
for n, conn, pin in ((1, "J-MB-STORE", 1), (2, "J-MB-STORE", 2), (3, "J-MB-TARGETS", 1),
                     (4, "J-MB-TARGETS", 2)):
    MB.net(f"E-{n}", f"U1.{9 + n}", f"{conn}.{pin}", color="green", label=f"E-{n}")
MB.contact("J-MB-STORE.1", "J-MB-STORE.3", what="a stored ball between the front and the wall foil")
MB.contact("J-MB-STORE.2", "J-MB-STORE.3", what="a stored ball between the back and the wall foil")
MB.contact("J-MB-TARGETS.1", "J-MB-TARGETS.3", what="the ball between bell 1 and the bell foil")
MB.contact("J-MB-TARGETS.2", "J-MB-TARGETS.3", what="the ball between bell 2 and the bell foil")
# E-5 to E-8 wait for contacts to come; the firmware's pull-up holds them high until then
for n in range(5, 9):
    MB.net(f"E-{n}", f"U1.{9 + n}", f"J-MB-SPARE.{n - 4}", color="green", label=f"E-{n}")

# -- solenoids ----------------------------------------------------------------------------------
for n, pin in ((1, 40), (2, 41), (3, 30), (4, 37)):
    MB.net(f"TRIGGER{n}", f"A1.{pin}", f"J-COIL-TEENSY.{n}", color="blue",
           label=f"trigger {n}", teensy=f"Solenoid trigger {n}")
for n, pin in ((1, 36), (2, 35), (3, 34)):
    MB.net(f"SENSE{n}", f"A1.{pin}", f"J-SENSE-TEENSY.{n}", color="green",
           label=f"sense {n}", teensy=f"Solenoid sense {n}")

# -- break beam ---------------------------------------------------------------------------------
MB.net("FEED", "R2.2", "J-MB-EMITTER.1", color="red", label="feed")
MB.net("RECEIVER", "A1.31", "R3.1", "J-MB-RECEIVER.1", color="green", label="receiver",
       teensy="Receiver output of the ball drain gate")

# -- toggle switch ------------------------------------------------------------------------------
MB.net("SWITCH", "A1.32", "R1.1", color="green", label="switch", teensy="Toggle switch")
MB.net("SWITCH-R1", "R1.2", "J-MB-SWITCH.1", color="green", label="switch behind R1")
