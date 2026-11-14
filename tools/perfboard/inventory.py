"""What the perfboard layouts are built from: the boards, the parts and the ways to connect.

Every size is read from the datasheet named beside it, in millimeters, as (nominal, max) where the
sheet gives a tolerance. A layout uses the maximum, so a part that is the largest the sheet allows
still fits. An entry the sheet does not cover says so in `unverified`, with the measurement that
would settle it.
"""

PITCH = 2.54

# ---- boards ---------------------------------------------------------------------------------
# Edge pads sit along both short ends, one per hole column, in line with the grid. The two outer
# columns carry the corner mounting holes instead, so a short end of n holes has n - 2 pads.
# Each pad stands on its own, connected to nothing.
BOARDS = {
    # name: (short side holes, long side holes, count in stock)
    "6x28": (6, 28, 3),
    "10x24": (10, 24, 5),
    "14x20": (14, 20, 3),
    "18x24": (18, 24, 3),
    "26x31": (26, 31, 1),
    "30x42": (30, 42, 2),
}

# how a board's edges are printed, where it differs from letters down the short side and numbers
# along the long side: the short side's labels, the long side's labels, and which comes first in a
# hole's name. The 30 x 42 board prints A to Z and then A to P again along its long side, which a
# drawing writes A' to P'.
_AZ = [chr(65 + i) for i in range(26)]
LABELS = {
    "18x24": ([str(i) for i in range(1, 19)], _AZ[:24], "long"),
    "14x20": ([str(i) for i in range(1, 15)], _AZ[:20], "long"),
    "30x42": (_AZ + ["1", "2", "3", "4"], _AZ + [ch + "'" for ch in _AZ[:16]], "both"),
}

# screw terminals in stock, by number of poles: Same Sky TB002-500-02BE and -03BE
TERMINALS = {2: 8, 3: 7}


def unique_names(names):
    """Pin names as a netlist addresses them: a name that repeats, such as a module's GND, takes
    #2, #3 and on from its second occurrence."""
    seen, out = {}, []
    for n in names:
        seen[n] = seen.get(n, 0) + 1
        out.append(n if seen[n] == 1 else f"{n}#{seen[n]}")
    return tuple(out)


def edge_pads(short):
    return short - 2


# ---- connections ---------------------------------------------------------------------------
CONNECTIONS = {
    "tin": "solder only, between two holes that are horizontal or vertical neighbors",
    "bare": "uninsulated wire flat along one row or column, soldered in every hole it touches",
    "link": "uninsulated wire through two holes like a lead, nothing in between it may touch",
    "insulated": "insulated wire from one hole or pad to one other, no branch",
}
INSULATED_MIN = 10.0          # mm; shorter, the insulation melts back while soldering

# ---- parts ----------------------------------------------------------------------------------
# roles: what each pin, in pin order, does to the line it sits on, which the wiring check reads.
#   in        reads the line and drives nothing, a load included
#   out       drives it high and low from the part's supply pin
#   tri:N     drives it only while pin N enables the output
#   od        only pulls it low, an open drain or an open collector
#   odpull:P  only pulls it low, and a resistor inside the part pulls it up to the supply on pin P
#   supply, gnd, nc   a supply pin, a ground pin, a pin connected to nothing inside
#   config    set by the firmware; the schematic declares the role it takes
#   other     does none of these to a logic line
# kind: axial, radial, to220, header, socket, terminal
PARTS = {
    "resistor": dict(
        kind="axial", body_l=(6.3, 6.8), body_d=(2.4, 2.6), lead_d=(0.55, 0.60),
        color="#d8c49a",
        source="CFR-Yageo.pdf, Dimensions table, style CFR-25 (normal size, 0.25 W); the justPi "
               "kit JUS-19729 names no size, so the larger of the two 0.25 W bodies is taken, "
               "at its maximum"),
    "diode_do41": dict(
        kind="axial", body_l=(4.7, 5.2), body_d=(2.35, 2.70), lead_d=(0.79, 0.86),
        color="#1a1a1a", band="#c8c8c8",
        source="1N5817-1N5819-TSC.pdf, page 5, Package outline DO-204AL: B 4.20 to 5.20, "
               "D 2.00 to 2.70, C 0.71 to 0.86",
        pins=("K", "A"), pin_note="the band marks the cathode"),
    "ceramic_k15": dict(
        kind="radial", body_w=(4.0, 4.0), body_t=(2.6, 2.6), lead_d=(0.5, 0.5),
        lead_spacing=(2.5, 3.3), color="#1a1a1a",
        source="K-series-Vishay.pdf, page 2, Lead configuration and dimensions, size code 15: "
               "Wb max 4.0, T max 2.6; page 1: lead wires 0.5 mm, black epoxy coating; "
               "ordering code digit 15 = 2 for 2.5 mm"),
    "electrolytic_esl_c3": dict(
        kind="radial_round", body_d=(5.0, 5.5), body_h=(11.0, 12.5), lead_d=(0.5, 0.5),
        lead_spacing=(2.0, 2.5), color="#1a1a1a", stripe="#c8c8c8",
        source="ESL-KEMET.pdf, page 3, Dimensions, size code C3: D 5.0 +-0.5, L 11.0 +1.5/-0, "
               "P 2.0 +-0.5, d 0.5; page 21: black sleeve, polarity stripe on the minus side",
        pins=("+", "-"), pin_note="the stripe marks minus; the plus lead is the longer one"),
    "to220": dict(
        roles=("in", "od", "other"),
        roles_source="IRL540N-Infineon.pdf, page 1 pin assignment: pin 1 gate, the input; an N-channel drain only sinks current, an open drain",
        kind="to220", body_e=(10.16, 10.67), body_a=(4.2, 4.83), lead_to_back=(2.5, 2.92),
        lead_w=(0.8, 1.01), color="#1a1a1a", tab="#bfbfbf",
        source="IRL540N-Infineon.pdf, page 8, PG-TO220-3-U05: E 9.65 to 10.67, A 3.56 to 4.83, "
               "A2 2.03 to 2.92, b 0.38 to 1.01; page 1: pin 1 gate, pin 2 drain and tab, pin 3 "
               "source",
        pins=("G", "D", "S"),
        pin_note="seen on the printed face with the leads down, pin 1 is on the left; the metal "
                 "tab at the back carries the drain"),
    "header_male": dict(
        kind="header", body_w=(2.54, 2.54), body_h=(2.50, 2.50), pin=(0.64, 0.64),
        color="#1a1a1a",
        source="Header-male-Sullins.pdf, drawing 11635: insulator 2.50 high, pins 0.64 square, "
               "pitch 2.54, black PBT; recommended hole 1.00",
        unverified="reference part; the headers in stock carry no maker"),
    "header_female": dict(
        kind="socket", body_w=(2.54, 2.54), body_h=(8.50, 8.50), color="#1a1a1a",
        source="Header-female-Sullins.pdf, page 1, LFB straight: A 8.50, B 2.54, tail 3.20",
        unverified="reference part; the socket headers in stock carry no maker"),
    "terminal_5mm": dict(
        kind="terminal", pitch=5.00, body_depth=(7.60, 7.85), body_h=(10.0, 10.0),
        pin_d=(1.00, 1.00), hole=1.30, pins_from_back=4.10, color="#2a6fcf",
        source="TB002-500-SameSky.pdf, page 2: pitch 5.00, length 5 x poles, depth 7.60, "
               "height 10.00, pins 1.00, pin row 4.10 from the back; the pins enter the "
               "perfboard's holes, tried on the board"),
}

PARTS.update({
    # ---- DIP ICs: body length and width over the plastic, row spacing E at the shoulders ----
    "cd74hct125e": dict(
        roles=("in", "in", "tri:1", "in", "in", "tri:4", "gnd", "tri:10", "in", "in", "tri:13", "in", "in", "supply"),
        roles_source="CD74HCT125-TI.pdf (SCHS415A), page 3 pin table: OE and A inputs, Y outputs, each Y 3-state under its OE",
        kind="dip", pins=14, body_l=(19.30, 19.69), body_w=(6.35, 6.60), row=(7.62, 8.26),
        lead_w=(0.46, 0.53), color="#1a1a1a",
        pinout=("1OE", "1A", "1Y", "2OE", "2A", "2Y", "GND", "3Y", "3A", "3OE", "4Y", "4A", "4OE",
                "VCC"),
        source="cd74hct125.pdf (TI SCHS415A), page 1 Device information, page 22 N (R-PDIP-T**) "
               "4040049/E, page 3 pin functions; notch at the pin 1 end",
        datasheet_url="https://www.ti.com/lit/ds/symlink/cd74hct125.pdf"),
    "mcp3008_p": dict(
        roles=("in",) * 8 + ("gnd", "in", "in", "tri:10", "in", "gnd", "supply", "supply"),
        roles_source="MCP3004-3008-Microchip.pdf (DS21295D), page 15 Table 3-1; DOUT is disabled while CS/SHDN is high, the tDIS test figure",
        kind="dip", pins=16, body_l=(19.18, 19.69), body_w=(6.35, 7.11), row=(7.87, 8.26),
        lead_w=(0.46, 0.56), color="#1a1a1a",
        pinout=("CH0", "CH1", "CH2", "CH3", "CH4", "CH5", "CH6", "CH7", "DGND", "CS/SHDN", "DIN",
                "DOUT", "CLK", "AGND", "VREF", "VDD"),
        source="MCP3004-3008-Microchip.pdf, page 31, 16-lead PDIP C04-017B: D, E1, E, b; page 1 "
               "package types"),
    "mcp23008_p": dict(
        roles=("in", "od", "in", "in", "in", "in", "nc", "config", "gnd") + ("config",) * 8 + ("supply",),
        roles_source="MCP23008-MCP23S08-Microchip.pdf (DS20001919F), page 4 Table 1-1: INT configurable as push-pull or open-drain, GP0 to GP7 set as input or output by the firmware; SDA is the I2C bus's open-drain data line",
        kind="dip", pins=18, body_l=(22.86, 23.37), body_w=(6.35, 7.11), row=(7.87, 8.26),
        lead_w=(0.46, 0.56), color="#1a1a1a",
        pinout=("SCL", "SDA", "A2", "A1", "A0", "RESET", "NC", "INT", "VSS", "GP0", "GP1", "GP2",
                "GP3", "GP4", "GP5", "GP6", "GP7", "VDD"),
        source="MCP23008-MCP23S08-Microchip.pdf, page 31, 18-lead PDIP C04-007B: D, E1, E, b; "
               "page 2 package types"),
    # ---- the second TO-220 ----
    "stp160n3ll": dict(
        roles=("in", "od", "other"),
        roles_source="STP160N3LL-ST.pdf, page 1 Figure 1: pin 1 gate, the input; an N-channel drain only sinks current, an open drain",
        kind="to220", body_e=(10.20, 10.40), body_a=(4.50, 4.60), lead_to_back=(2.56, 2.72),
        lead_w=(0.75, 0.88), color="#1a1a1a", tab="#bfbfbf",
        source="STP160N3LL-ST.pdf (DocID025073 Rev 3), page 11 Table 8, TO-220 type A: E 10.00 to "
               "10.40, A 4.40 to 4.60, J1 2.40 to 2.72, b 0.61 to 0.88, e 2.40 to 2.70; page 1 "
               "Figure 1: pin 1 gate, pin 2 drain and tab, pin 3 source",
        pins=("G", "D", "S")),
    # ---- optical parts ----
    "am4457f3c": dict(
        roles=("in", "in"),
        roles_source="an LED is a load and drives no line",
        kind="side_led", body_l=(5.72, 5.97), body_w=(4.45, 4.70), body_t=(1.55, 1.80),
        lens_d=1.5, lens_from_top=1.22, pitch=2.54, lead_w=(0.5, 0.75), color="#e8f2f8",
        pins=("A", "K"),
        pin_note="the cathode is the shorter lead; the lens looks across the line through the two "
                 "leads, sideways off the board",
        source="AM4457F3C-Kingbright.pdf (DSAQ0403 V.3A), page 1 Package dimensions, general "
               "tolerance 0.25; water clear"),
    "tekt5400s": dict(
        roles=("other", "od"),
        roles_source="TEKT5400S-Vishay.pdf, page 1: a phototransistor's collector only sinks current, an open drain",
        kind="side_led", body_l=(4.9, 5.0), body_w=(4.9, 5.0), body_t=(2.5, 2.65),
        lens_d=1.5, lens_from_top=1.8, lead_offset_to_lens=1.4, pitch=2.54, lead_w=(0.45, 0.55),
        color="#1a1a1a", pins=("E", "C"),
        pin_note="facing the lens with the leads down, E is left and C right; no polarity mark",
        source="TEKT5400S-Vishay.pdf (doc 81569 rev 1.6), page 4 Package dimensions 6.544-5347.01-4;"
               " body color from the page 1 photo"),
    "qre1113": dict(
        roles=("in", "in", "od", "other"),
        roles_source="qre1113-d.pdf, page 1 pinout: the emitter's LED is a load, the phototransistor's collector only sinks current",
        kind="reflective", body_a=(2.70, 2.90), body_b=(3.40, 3.60), body_h=(1.60, 1.70),
        pitch_same_side=1.80, span_outer=(4.0, 4.2), lead_w=(0.5, 0.6), color="#1a1a1a",
        pinout=("anode", "cathode", "collector", "emitter"),
        pin_note="pins 1 and 4 on one side, 2 and 3 on the other; chamfered corner at pin 1; "
                 "looks away from the board along the leads",
        source="qre1113-d.pdf (onsemi QRE1113/D rev 9), page 7 Case 100AQ, page 1 pinout",
        datasheet_url="https://www.onsemi.com/download/data-sheet/pdf/qre1113-d.pdf",
        fanout=(2, 3),
        fanout_note="the leads bend out to a field of 2 x 3 holes or larger, so each pin lands in "
                    "any hole of that field; two pins of one net can sit side by side and take a "
                    "tin bridge (tried on the part)",
        unverified="body color not stated in the sheet"),
    "opb606a": dict(
        roles=("od", "other", "in", "in"),
        roles_source="OPB606-607.pdf, page 1 pin table: the phototransistor's collector only sinks current, the LED is a load",
        kind="reflective_grid", body_a=(4.39, 4.52), body_b=(6.10, 6.10), body_h=(4.50, 4.70),
        pitch_x=(2.21, 2.34), pitch_y=2.54, lead_w=(0.51, 0.51), color="#1a1a1a",
        pinout=("collector", "emitter", "anode", "cathode"),
        pin_note="lead view: 4 top left, 1 top right, 3 bottom left, 2 bottom right; chamfered "
                 "corner at pin 1; looks away from the board along the leads",
        source="OPB606-607.pdf (TT Electronics rev E), page 1 drawing and pin table, black opaque "
               "housing",
        datasheet_url="https://www.ttelectronics.com/TTElectronics/media/ProductFiles/Datasheet/"
                      "OPB606-607.pdf",
        unverified="the 2.21 pitch is off the 2.54 grid; one lead pair has to be bent onto the grid"),
    # ---- passives of the other boards ----
    "electrolytic_zlh_8x20": dict(
        kind="radial_round", body_d=(8.0, 8.5), body_h=(20.0, 22.0), lead_d=(0.6, 0.6),
        lead_spacing=(3.5, 4.0), color="#1a1a1a", stripe="#c8c8c8", pins=("+", "-"),
        pin_note="the plus lead is the longer one",
        source="ZLH-Rubycon.pdf, page 2 Dimensions: D + 0.5 max, L + 2.0 max for L >= 20, "
               "F 3.5 +-0.5, d 0.6 for D 8; PET sleeve, black on the page 1 photo",
        unverified="F 3.5 lands between one and two grid steps; the leads are bent onto 2 holes"),
    "ceramic_fg11": dict(
        kind="radial", body_w=(5.5, 5.5), body_t=(4.0, 4.0), body_h=(7.0, 7.0), lead_d=(0.5, 0.6),
        lead_spacing=(2.5, 3.3), color="#2457c5",
        source="FG-series-TDK.pdf (TDK leadmlcc_halogenfree_fg 20241125), page 3 part number, "
               "page 40 FG11 shape and dimensions: 5.5 x 7.0 x 4.0 max, pitch 2.5 +-0.8, lead 0.5 "
               "+0.10; blue coating from the page 40 drawing"),
    # ---- mechanics ----
    "fuse_holder_5x20": dict(
        kind="fuse_holder", pitch=22.5, body_l=(22.5, 22.5), body_w=(9.6, 9.6),
        body_h=(11.5, 12.7), pin_hole=1.3, color="#1a1a1a",
        source="OGN-Schurter.pdf, page 3 THT variant and drilling diagram, 0031.8201",
        unverified="reference part, no holder is chosen yet; its 22.5 mm pin distance is 0.36 mm "
                   "off the grid's 22.86, so the holes need drilling out to 1.3 mm"),
    "header_male_90": dict(
        kind="header_90", body_w=(2.50, 2.70), pin=(0.64, 0.64), row_to_face=(4.02, 4.22),
        pin_out=(5.84, 6.04), color="#1a1a1a",
        source="Header-male-RA-Sullins.pdf, drawing 11637 rev B: insulator 2.50, pins 0.64, "
               "hole row 4.02 behind the insulator's front face, head 5.84 (code BA)",
        placement="only at the board edge, the pins pointing off the board; the insulator "
                  "stands beside the hole row toward the edge",
        unverified="reference part; the angled headers in stock carry no maker"),
    "dip_socket": dict(
        kind="dip_socket", row=7.62, body_w=(10.11, 10.11),
        body_l={14: 17.75, 16: 20.27, 18: 22.81}, color="#1a1a1a",
        source="DIP-socket-MillMax.pdf (Mill-Max DIP insulator information): .699/.798/.898 x .398 "
               "in for 14/16/18 pins",
        placement="every DIP IC sits in a socket",
        unverified="reference part; the sockets in stock carry no maker"),
})


# ---- breakout boards, always plugged as daughter boards onto socket headers -----------------
# size is the board outline (x along its length, y across), rows are the pin rows in board
# millimeters: (x of the first pin, y, step along x or y, names in order).
def _row(x0, y, names, step=PITCH, along="x"):
    if along == "x":
        return [(x0 + i * step, y, n) for i, n in enumerate(names)]
    return [(y, x0 + i * step, n) for i, n in enumerate(names)]


MODULES = {
    "teensy41": dict(
        size=(60.96, 17.78), color="#1f6b3a",
        pins=_row(1.27, 1.27, ("Vin", "GND", "3.3V", "23", "22", "21", "20", "19", "18", "17",
                               "16", "15", "14", "13", "GND", "41", "40", "39", "38", "37", "36",
                               "35", "34", "33"))
             + _row(1.27, 16.51, ("GND", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10",
                                  "11", "12", "3.3V", "24", "25", "26", "27", "28", "29", "30",
                                  "31", "32")),
        source="pjrc.com dimensions_teensy41.png and the rev 4 pin card: 60.96 x 17.78, rows "
               "15.24 apart and 1.27 in from the long edges, top view with USB at x = 0",
        note="USB connector at x = 0, 0.7 past the edge, 2.5 high; extra pads: VUSB, USB host "
             "(5), ethernet (2 x 3 on 2.0 mm, off the grid), side pads at x = 45.72"),
    "d24v5f3": dict(
        roles=("in", "supply", "gnd", "supply"),
        roles_source="D24V5Fx-Pololu-schematic.pdf: SHDN an enable input, VIN and VOUT the supplies",
        size=(10.2, 12.7), color="#1f6b3a",
        pins=_row(1.27, 1.27, ("SHDN", "VIN", "GND", "VOUT")),
        source="pololu.com d24v5fx-step-down-voltage-regulator-dimensions.pdf and product 2842: "
               "10.2 x 12.7, one row of 4 on 2.54, 1.27 from the edge; JP1 pin 1 SHDN"),
    "adafruit757": dict(
        roles=("gnd", "odpull:LV", "odpull:LV", "odpull:LV", "odpull:LV", "supply",
               "gnd", "odpull:HV", "odpull:HV", "odpull:HV", "odpull:HV", "supply"),
        roles_source="github.com/adafruit/4-Channel-Level-Shifter-PCB, Adafruit FET 4-Channel "
                     "Shifter.sch: each channel a BSS138 with its gate on LV, 10K from each A pin "
                     "to LV and from each B pin to HV, so either side only pulls low",
        size=(20.447, 15.24), color="#1d4f91",
        pins=_row(3.81, 2.54, ("GND", "A4", "A3", "A2", "A1", "LV"))
             + _row(3.81, 12.70, ("GND", "B4", "B3", "B2", "B1", "HV")),
        source="Adafruit 4-Channel-Level-Shifter-PCB Eagle board file: 15.24 x 20.447, rows "
               "10.16 apart, 2.54 in from the long edges"),
    "adafruit1207": dict(
        size=(20.32, 17.78), color="#1d4f91",
        pins=_row(1.27, 16.51, tuple(str(i) for i in range(1, 9)))
             + _row(1.27, 1.27, tuple(str(i) for i in range(16, 8, -1))),
        source="adafruit.com product 1207 (rows 0.6 in apart, 0.1 in pitch) and the DigiKey size "
               "0.800 x 0.700 in; DIP numbering counterclockwise from the chip side",
        unverified="Adafruit publishes no drawing; the outline is the distributor's"),
    "dfr0954": dict(
        size=(18.0, 18.0), color="#1a1a1a",
        pins=_row(2.65, 1.38, ("VCC", "GND", "LRC", "BCLK", "DIN", "SPK+"))
             + _row(2.65, 16.62, ("GAIN", "SD", "NC", "GND", "VCC", "SPK-")),
        source="DFR0954_max98357a-i2s_dimension_1.0.pdf and DFR0954-DFRobot-schematic.pdf page "
               "12: 18 x 18, 12 holes 0.9 in two rows 15.24 apart; black solder mask",
        note="holes are 0.9 mm; a 0.64 mm square header pin measures 0.9 mm across its corners"),
}


DIGIT = ("black", "brown", "red", "orange", "yellow", "green", "blue", "violet", "gray", "white")


def bands(ohms):
    """The four color bands of a 5 % resistor (IEC 60062): two digits, multiplier, gold."""
    if ohms <= 0:
        raise ValueError(ohms)
    exp = 0
    v = float(ohms)
    while v >= 100 - 1e-9:
        v /= 10
        exp += 1
    while v < 10 - 1e-9:
        v *= 10
        exp -= 1
    digits = round(v)
    if digits == 100:
        digits, exp = 10, exp + 1
    if abs(digits * 10 ** exp - ohms) > 1e-6 * ohms:
        raise ValueError(f"{ohms} ohm needs more than two significant digits")
    mult = {-2: "silver", -1: "gold"}.get(exp, DIGIT[exp] if 0 <= exp <= 9 else None)
    if mult is None:
        raise ValueError(ohms)
    return (DIGIT[digits // 10], DIGIT[digits % 10], mult, "gold")


BAND_COLOR = {"black": "#1a1a1a", "brown": "#7b3f00", "red": "#d22b2b", "orange": "#f08a24",
              "yellow": "#f2d40f", "green": "#2e9e44", "blue": "#2457c5", "violet": "#7d3cb5",
              "gray": "#8a8a8a", "white": "#ffffff", "gold": "#c9a227", "silver": "#c0c0c0"}


def span_range(name, standing=False):
    """Hole steps a part's two leads may land apart, from its body and lead size."""
    p = PARTS[name]
    if p["kind"] == "axial":
        if standing:
            return (1, 2)
        lo = int(-(-(p["body_l"][1] + 2 * 0.8) // PITCH))   # 0.8 mm straight lead before the bend
        return (lo, 6)
    if p["kind"] in ("radial", "radial_round"):
        return (1, 2)
    raise ValueError(name)
