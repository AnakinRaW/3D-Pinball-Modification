"""The IR board and its isolator board, pin by pin. figures.py loads this file, and
the perfboard layouts, the KiCad export and the checks of docs/parts/ir-reflective/ read it from
there.

Channel n of sixteen takes Rn into the LED bus, R(16 + n) as its pull-down and J-Sn for its
sensor board. U1 reads channels 1 to 8 on CH0 to CH7, U2 channels 9 to 16. The three sensor
boards from the stock machine sit on channels 1 to 3, so R1 to R3 carry 100 Ω.

The isolator U3 sits on a board of its own beside the IR board, with J-IR-TEENSY, R34, R36, C3
and C4. J-IR-ISO joins the two boards. SCK, MOSI and ADC-DOUT run in two leads each, one per
converter, and the isolator board joins each pair, so the IR board never joins U1's control
lines to U2's.

Connector pin orders: J-IR-TEENSY as design.md's J-IR-TEENSY table gives it. J-IR-ISO carries
U1's SCK, MOSI, CS-A and DOUT on pins 1 to 4, U2's SCK, MOSI, CS-B and DOUT on pins 5 to 8,
CLOCK on pin 9, 3V3 on pin 10 and GND on pin 11. Each J-Sn carries 3V3 on pin 1, the channel on
pin 2 and the LED cathode on pin 3, the sensor board's pin order. J-D24V5F3, the module's
socket, carries SHDN, VIN, GND and VOUT on pins 1 to 4, the module's own row. The power board's
5 V cable plugs onto the module's VIN and GND from above, through a stacking header, so the
socket's VIN contact carries no copper.

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

MAIN = SCHEMATIC.board("main", "IR board")
MAIN.part("J-IR-ISO", "header_male", pins=11)
MAIN.part("J-D24V5F3", "header_female", pins=4, title="D24V5F3 socket")
MAIN.part("U1", "mcp3008_p", type="MCP3008")
MAIN.part("U2", "mcp3008_p", type="MCP3008")
MAIN.part("Q1", "to220", type="IRL540N")
for ref, value in (("R33", "r_gate"), ("R35", "r_gate_pd"), ("R37", "r_dout"), ("R38", "r_dout"),
                   ("R39", "r_ref")):
    MAIN.part(ref, "resistor", value=value)
for ref in ("C1", "C2"):
    MAIN.part(ref, "ceramic_k15", type="K104K15X7RF5TL2", value="c_decoupling")
MAIN.part("C5", "ceramic_fg11", type="FG11X7R1C226MRT06", value="c_entry")
MAIN.part("C6", "ceramic_fg11", type="FG11X7R1C226MRT06", value="c_ref")

# the isolator board's outputs over J-IR-ISO, one lead per converter
for sig, pin, adc in (("SCK-U1", 1, "U1.13"), ("MOSI-U1", 2, "U1.11"), ("CS-A-2", 3, "U1.10"),
                      ("SCK-U2", 5, "U2.13"), ("MOSI-U2", 6, "U2.11"), ("CS-B-2", 7, "U2.10")):
    MAIN.net(sig, f"J-IR-ISO.{pin}", adc, color="blue", label=sig)
MAIN.net("CLOCK-2", "J-IR-ISO.9", "R33.1", color="blue", label="CLOCK (board)")
MAIN.net("GATE", "R33.2", "R35.1", "Q1.G", color="blue")
MAIN.net("DOUT-A", "J-IR-ISO.4", "R37.1", color="green", label="ADC-DOUT")
MAIN.net("DOUT-B", "J-IR-ISO.8", "R38.1", color="green", label="ADC-DOUT")
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
MAIN.unconnected("J-D24V5F3.2", why="VIN takes the power board's 5 V on the module itself, the "
                 "cable plugged onto the module's stacking header from above, design.md")
MAIN.net("3V3", "J-D24V5F3.4", "C5.1", "R39.1", "J-IR-ISO.10",
         *(f"J-S{n}.1" for n in range(1, 17)), color="yellow", label="3V3", rail="3V3")
MAIN.net("3V3_ADC", "R39.2", "C6.1", "C1.1", "C2.1", "U1.15", "U1.16", "U2.15", "U2.16",
         color="yellow", label="3V3_ADC", rail="3V3_ADC")
MAIN.net("GND", "J-D24V5F3.3", "J-IR-ISO.11", "Q1.S", "R35.2",
         "C1.2", "C2.2", "C5.2", "C6.2", "U1.9", "U1.14", "U2.9", "U2.14",
         *(f"R{16 + n}.2" for n in range(1, 17)), color="black", label="GND", rail="gnd")

ISO = SCHEMATIC.board("iso", "IR isolator board")
ISO.part("J-IR-TEENSY", "header_male", pins=8)
ISO.part("J-IR-ISO", "header_male", pins=11)
ISO.part("U3", "iso7761f", type="ISO7761F", pins=ISO_PINS, roles=ISO_ROLES)
for ref, value in (("R34", "r_miso"), ("R36", "r_dout_pu")):
    ISO.part(ref, "resistor", value=value)
for ref in ("C3", "C4"):
    ISO.part(ref, "ceramic_k15", type="K104K15X7RF5TL2", value="c_decoupling")

# side 2's outputs run from VCC2, which the roles tuple cannot tell from VCC1
for pin in ("OUTA", "OUTB", "OUTC", "OUTD", "OUTE"):
    ISO.role(f"U3.{pin}", drives(f"U3 {pin}", "push-pull", rail="3V3", src=ISO_S2))

# the Teensy's side, J-IR-TEENSY to U3's side 1
for n, (sig, pin) in enumerate((("SCK", "INA"), ("MOSI", "INB"), ("CS-A", "INC"),
                                ("CS-B", "IND"), ("CLOCK", "INE")), start=1):
    ISO.net(sig, f"J-IR-TEENSY.{n}", f"U3.{pin}", color="blue", label=sig, teensy=sig)
ISO.net("MISO", "J-IR-TEENSY.6", "R34.1", color="green", label="MISO", teensy="MISO")
ISO.net("MISO-OUTF", "R34.2", "U3.OUTF", color="green")
ISO.net("3V3-TEENSY", "J-IR-TEENSY.7", "U3.VCC1", "C3.1", color="yellow", label="3V3",
        rail=TEENSY_RAIL)
ISO.net("GND1", "J-IR-TEENSY.8", "U3.GND1", "C3.2", color="black", label="GND", rail="gnd")

# the board's side, U3's side 2 to J-IR-ISO, SCK, MOSI and ADC-DOUT joined from their two leads
ISO.net("SCK-2", "U3.OUTA", "J-IR-ISO.1", "J-IR-ISO.5", color="blue", label="SCK (board)")
ISO.net("MOSI-2", "U3.OUTB", "J-IR-ISO.2", "J-IR-ISO.6", color="blue", label="MOSI (board)")
ISO.net("CS-A-2", "U3.OUTC", "J-IR-ISO.3", color="blue", label="CS-A (board)")
ISO.net("CS-B-2", "U3.OUTD", "J-IR-ISO.7", color="blue", label="CS-B (board)")
ISO.net("CLOCK-2", "U3.OUTE", "J-IR-ISO.9", color="blue", label="CLOCK (board)")
ISO.net("ADC-DOUT", "U3.INF", "R36.2", "J-IR-ISO.4", "J-IR-ISO.8", color="green",
        label="ADC-DOUT")
ISO.net("3V3", "J-IR-ISO.10", "U3.VCC2", "C4.1", "R36.1", color="yellow", label="3V3",
        rail="3V3")
ISO.net("GND", "J-IR-ISO.11", "U3.GND2", "C4.2", color="black", label="GND", rail="gnd")
