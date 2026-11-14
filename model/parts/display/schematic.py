"""The display board, the module and the cables between them and the Teensy, pin by pin.
figures.py loads this file, and the checks of docs/parts/display/ read it from there.

A1 is the Adafruit 2090, its pins those of the 8-bit header JP1, pins 1 to 20, then those of the
SPI header JP2, pins 1 to 20. Pins of one name are one line on the module, and the netlist keeps
them apart, since the module and not the wiring joins them. A2 is the Adafruit 757: SDA passes
its channel 1, SCL its channel 2.

Connector pin orders: J-DISP-PWR, where the cable from the power board's J-DISP is soldered,
carries 5 V on pin 1 and GND on pin 2. J-TOUCH carries SCL, SDA and the Teensy's 3V3 on pins 1
to 3, as J-MB-TOUCH does. J-DISP-MOD carries SDA, SCL, 5 V and GND on pins 1 to 4, as docs/connectors.md gives them.
J-DISP-BUS, the bus cable from the Teensy to the module's header, carries D0 to D7 on pins 1 to
8, WR on pin 9, D/C on pin 10, CS on pin 11 and GND on pin 12.
"""
from figcheck import TEENSY_RAIL, Schematic

# Adafruit's EAGLE schematic of the 2090, rev D, gives the module no pin table; every bus line
# from the 8-bit header enters a 74LVC245 input, and the other pins' roles do not reach a Teensy
# net
MODULE_PINS = (
    # JP1, the 8-bit header
    ("GND", "gnd"), ("Vin", "supply"), ("CS", "in"), ("C/D", "in"), ("WR", "in"), ("RD", "in"),
    ("RST", "in"), ("Lite", "in"), ("IRQ", "other"), ("SCL", "other"), ("SDA", "other"),
    ("GND", "gnd"), *((f"D{i}", "in") for i in range(8)),
    # JP2, the SPI header
    ("GND", "gnd"), ("Vin", "supply"), ("3Vo", "supply"), ("CLK", "in"), ("MISO", "other"),
    ("MOSI", "in"), ("CS", "in"), ("D/C", "in"), ("RST", "in"), ("Lite", "in"), ("GND", "gnd"),
    ("IRQ", "other"), ("SDA", "other"), ("SCL", "other"), ("IM3", "other"), ("IM2", "other"),
    ("IM1", "other"), ("IM0", "other"), ("CCS", "in"), ("CD", "other"))


SCHEMATIC = Schematic()

DISPLAY = SCHEMATIC.board("display", "Display")
DISPLAY.part("A1", "adafruit2090", type="Adafruit 2090", pins=tuple(n for n, _ in MODULE_PINS),
             roles=tuple(r for _, r in MODULE_PINS))
DISPLAY.part("A2", "adafruit757", type="Adafruit 757")
DISPLAY.part("J-DISP-PWR", "pads", pins=2, title="the cable from J-DISP on the edge pads")
DISPLAY.part("J-TOUCH", "header_male", pins=3)
DISPLAY.part("J-DISP-MOD", "header_male", pins=4)
DISPLAY.part("J-DISP-BUS", "header_male", pins=12)

# the bus, from the Teensy straight to the 8-bit header
for i in range(8):
    DISPLAY.net(f"D{i}", f"J-DISP-BUS.{i + 1}", f"A1.D{i}", color="blue", label=f"D{i}",
                teensy=f"D{i} to the display")
DISPLAY.net("WR", "J-DISP-BUS.9", "A1.WR", color="blue", label="WR", teensy="WR to the display")
DISPLAY.net("D/C", "J-DISP-BUS.10", "A1.C/D", color="blue", label="D/C",
            teensy="D/C to the display")
DISPLAY.net("CS", "J-DISP-BUS.11", "A1.CS", color="blue", label="CS",
            teensy="CS to the display controller")
DISPLAY.net("BUS-GND", "J-DISP-BUS.12", "A1.GND#2", color="black", label="GND", rail="gnd")

# the touch bus through the converter
DISPLAY.net("SDA", "J-TOUCH.2", "A2.A1", color="white", label="SDA",
            teensy="SDA to the touch controller")
DISPLAY.net("SCL", "J-TOUCH.1", "A2.A2", color="white", label="SCL",
            teensy="SCL to the touch controller")
DISPLAY.net("3V3", "J-TOUCH.3", "A2.LV", color="yellow", label="3V3", rail=TEENSY_RAIL)
DISPLAY.net("SDA-MOD", "A2.B1", "J-DISP-MOD.1", "A1.SDA", color="white", label="SDA")
DISPLAY.net("SCL-MOD", "A2.B2", "J-DISP-MOD.2", "A1.SCL", color="white", label="SCL")

# the supply, and the 5 V wire on Vin and RD at the header
DISPLAY.net("5V", "J-DISP-PWR.1", "A2.HV", "J-DISP-MOD.3", "A1.Vin", "A1.RD", color="red",
            label="5 V", rail="5V")
DISPLAY.net("GND", "J-DISP-PWR.2", "A2.GND#2", "J-DISP-MOD.4", "A1.GND", color="black",
            label="GND", rail="gnd")

# the SPI header's ties: CCS to 3.3Vout, MOSI to GND at pin 1
DISPLAY.net("3VO", "A1.3Vo", "A1.CCS", color="yellow", label="3Vo", rail="module 3.3Vout")
DISPLAY.net("MOSI-GND", "A1.MOSI", "A1.GND#3", color="black", label="GND", rail="gnd")

DISPLAY.unconnected("A1.RST", "A1.Lite", "A1.IRQ",
                    why="RST, Lite and the touch controller's IRQ stay unconnected")
DISPLAY.unconnected("A1.IM0", "A1.IM1", "A1.IM2", "A1.IM3", "A1.MISO", "A1.CD",
                    why="the module's jumpers set the 8-bit mode, and the card's MISO and its "
                        "card detect stay unconnected")
DISPLAY.unconnected("A1.CLK", "A1.D/C", "A1.CS#2",
                    why="the SPI header's lines of C/D, WR and CS, which the 8-bit header carries")
DISPLAY.unconnected("A1.Vin#2", "A1.GND#4", "A1.RST#2", "A1.Lite#2", "A1.IRQ#2", "A1.SDA#2",
                    "A1.SCL#2", why="the same lines as the 8-bit header's pins of that name")
DISPLAY.unconnected("A2.A3", "A2.A4", "A2.B3", "A2.B4",
                    why="channels 3 and 4 of the converter stay unused")
DISPLAY.unconnected("A2.GND", why="the converter joins it to its HV row's GND, which takes the "
                                  "board's ground")
