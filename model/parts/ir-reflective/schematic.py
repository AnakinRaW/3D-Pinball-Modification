"""The IR sensor mainboard, pin by pin. figures.py loads this file, and the perfboard layouts, the
KiCad export and the checks of docs/parts/ir-reflective/ read it from there.

Channel n of sixteen takes Rn into the LED bus, R(16 + n) as its pull-down and J-Sn for its
sensor board. U1 reads channels 1 to 8 on CH0 to CH7, U2 channels 9 to 16. The three sensor
boards from the stock machine sit on channels 1 to 3, so R1 to R3 carry 100 Ω.

Connector pin orders: J-IR-TEENSY as design.md's J-IR-TEENSY table gives it. Each J-Sn carries
3V3 on pin 1, the channel on pin 2 and the LED cathode on pin 3, the sensor board's pin order.
J-D24V5F3, the module's socket, carries SHDN, VIN, GND and VOUT on pins 1 to 4, the module's own
row. J-IR-PWR, the power board's cable soldered into the edge pads, carries 5 V on pin 1 and GND
on pin 2.

A resistor drawn lying has pin 1 on its left, one drawn standing pin 1 at its top; a capacitor
has pin 1 at its supply plate.
"""
from figcheck import TEENSY_RAIL, Schematic, drives

SCHEMATIC = Schematic()

# U3, the ISO7761F, is not in the perfboard inventory: SLLSER1H page 5, Table 4-1, pin functions
ISO_PINS = ("VCC1", "INA", "INB", "INC", "IND", "INE", "OUTF", "GND1",
            "GND2", "INF", "OUTE", "OUTD", "OUTC", "OUTB", "OUTA", "VCC2")
ISO_ROLES = ("supply", "in", "in", "in", "in", "in", "out", "gnd",
             "gnd", "in", "out", "out", "out", "out", "out", "supply")
ISO_S2 = ("ISO7761-TI.pdf (SLLSER1H) page 5, Table 4-1: OUTA to OUTE are outputs of side 2, "
          "powered from VCC2")

MAIN = SCHEMATIC.board("main", "IR sensor mainboard")
MAIN.part("J-IR-TEENSY", "header_male", pins=8)
MAIN.part("J-IR-PWR", "pads", pins=2, title="the power board's 5 V, soldered into the edge pads")
MAIN.part("J-D24V5F3", "header_female", pins=4, title="D24V5F3 socket")
MAIN.part("U1", "mcp3008_p", type="MCP3008")
MAIN.part("U2", "mcp3008_p", type="MCP3008")
MAIN.part("U3", "iso7761f", type="ISO7761F", pins=ISO_PINS, roles=ISO_ROLES)
MAIN.part("Q1", "to220", type="IRL540N")
for ref, value in (("R33", "r_gate"), ("R34", "r_miso"), ("R35", "r_gate_pd"),
                   ("R36", "r_dout_pu"), ("R37", "r_dout"), ("R38", "r_dout"), ("R39", "r_ref")):
    MAIN.part(ref, "resistor", value=value)
for ref in ("C1", "C2", "C3", "C4"):
    MAIN.part(ref, "ceramic_k15", type="K104K15X7RF5TL2", value="c_decoupling")
MAIN.part("C5", "ceramic_fg11", type="FG11X7R1C226MRT06", value="c_entry")
MAIN.part("C6", "ceramic_fg11", type="FG11X7R1C226MRT06", value="c_ref")

# side 2's outputs run from VCC2, which the roles tuple cannot tell from VCC1
for pin in ("OUTA", "OUTB", "OUTC", "OUTD", "OUTE"):
    MAIN.role(f"U3.{pin}", drives(f"U3 {pin}", "push-pull", rail="3V3", src=ISO_S2))

# the Teensy's side, J-IR-TEENSY to U3's side 1
for n, (sig, pin) in enumerate((("SCK", "INA"), ("MOSI", "INB"), ("CS-A", "INC"),
                                ("CS-B", "IND"), ("CLOCK", "INE")), start=1):
    MAIN.net(sig, f"J-IR-TEENSY.{n}", f"U3.{pin}", color="blue", label=sig, teensy=sig)
MAIN.net("MISO", "J-IR-TEENSY.6", "R34.1", color="green", label="MISO", teensy="MISO")
MAIN.net("MISO-OUTF", "R34.2", "U3.OUTF", color="green")
MAIN.net("3V3-TEENSY", "J-IR-TEENSY.7", "U3.VCC1", "C3.1", color="yellow", label="3V3",
         rail=TEENSY_RAIL)
MAIN.net("GND1", "J-IR-TEENSY.8", "U3.GND1", "C3.2", color="black", label="GND", rail="gnd")

# the board's side, U3's side 2 to the converters and Q1
MAIN.net("SCK-2", "U3.OUTA", "U1.13", "U2.13", color="blue", label="SCK (board)")
MAIN.net("MOSI-2", "U3.OUTB", "U1.11", "U2.11", color="blue", label="MOSI (board)")
MAIN.net("CS-A-2", "U3.OUTC", "U1.10", color="blue", label="CS-A (board)")
MAIN.net("CS-B-2", "U3.OUTD", "U2.10", color="blue", label="CS-B (board)")
MAIN.net("CLOCK-2", "U3.OUTE", "R33.1", color="blue", label="CLOCK (board)")
MAIN.net("GATE", "R33.2", "R35.1", "Q1.G", color="blue")
MAIN.net("ADC-DOUT", "U3.INF", "R36.2", "R37.1", "R38.1", color="green", label="ADC-DOUT")
MAIN.net("DOUT-U1", "R37.2", "U1.12", color="green")
MAIN.net("DOUT-U2", "R38.2", "U2.12", color="green")

# sixteen channel positions, built alike
for n in range(1, 17):
    MAIN.part(f"J-S{n}", "header_male", pins=3)
    MAIN.part(f"R{n}", "resistor", value="r_led_sharp" if n <= 3 else "r_led")
    MAIN.part(f"R{16 + n}", "resistor", value="r_pd")
    adc, ch = ("U1", n) if n <= 8 else ("U2", n - 8)
    MAIN.net(f"CH{n}", f"{adc}.{ch}", f"R{16 + n}.1", f"J-S{n}.2", color="green", label=f"CH{n}")
    MAIN.net(f"LED{n}", f"J-S{n}.3", f"R{n}.2", color="white")
MAIN.net("LED-BUS", *(f"R{n}.1" for n in range(1, 17)), "Q1.D", color="white", label="LED BUS")

# supply
MAIN.unconnected("J-D24V5F3.1", why="SHDN stays open, since the module pulls it to VIN, design.md")
MAIN.net("5V", "J-IR-PWR.1", "J-D24V5F3.2", color="red", label="5 V", rail="5V")
MAIN.net("3V3", "J-D24V5F3.4", "C5.1", "R39.1", "U3.VCC2", "C4.1", "R36.1",
         *(f"J-S{n}.1" for n in range(1, 17)), color="yellow", label="3V3", rail="3V3")
MAIN.net("3V3_ADC", "R39.2", "C6.1", "C1.1", "C2.1", "U1.15", "U1.16", "U2.15", "U2.16",
         color="yellow", label="3V3_ADC", rail="3V3_ADC")
MAIN.net("GND", "J-IR-PWR.2", "J-D24V5F3.3", "U3.GND2", "Q1.S", "R35.2",
         "C1.2", "C2.2", "C4.2", "C5.2", "C6.2", "U1.9", "U1.14", "U2.9", "U2.14",
         *(f"R{16 + n}.2" for n in range(1, 17)), color="black", label="GND", rail="gnd")
