"""The rotary sensor and its cable, pin by pin. figures.py loads this file, and the checks of
docs/parts/magnetic-rotary/ read it from there.

A1 is the Grove AS5600 module. Its pins are those of its Grove socket J1 in order. J-ROTARY is
the cable from the Teensy, in the Grove socket's order: SCL on pin 1, SDA on pin 2, 3V3 on pin 3
and GND on pin 4.
"""
from figcheck import TEENSY_RAIL, Schematic

GROVE = ("Grove-AS5600-Seeed-schematic.pdf, page 2: J1, the Grove socket, carries SCL on pin 1, "
         "SDA on pin 2, VCC on pin 3 and GND on pin 4. Q1 and Q2, 2N7002, join SCL and SDA to "
         "the AS5600; an I2C device drives SDA open-drain and reads SCL")

SCHEMATIC = Schematic()

SENSOR = SCHEMATIC.board("sensor", "Rotary sensor")
SENSOR.part("A1", "seeed101020692", type="Seeed Grove AS5600 module, 101020692",
            pins=("SCL", "SDA", "VCC", "GND"), roles=("in", "od", "supply", "gnd"))
SENSOR.part("J-ROTARY", "header_male", pins=4)
SENSOR.net("SCL", "J-ROTARY.1", "A1.SCL", color="white", label="SCL", teensy="SCL on Wire")
SENSOR.net("SDA", "J-ROTARY.2", "A1.SDA", color="white", label="SDA", teensy="SDA on Wire")
SENSOR.net("3V3", "J-ROTARY.3", "A1.VCC", color="yellow", label="3V3", rail=TEENSY_RAIL)
SENSOR.net("GND", "J-ROTARY.4", "A1.GND", color="black", label="GND", rail="gnd")
