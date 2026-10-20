"""The figures of the solenoid board, as inputs plus formulas.

Run with:  python tools/figcheck.py docs/parts/solenoid/figures.py --sheets

Every input names where it comes from. Every derived quantity is a function
whose parameter names are its dependencies, so `--graph <key>` answers what a
value feeds and what it rests on.
"""
import pathlib
from math import inf

from figcheck import Model, Q, TEENSY_RAIL, contact, pull, reads

HERE = pathlib.Path(__file__).resolve().parent
MODEL = Model("solenoid", HERE / "design.md", section=None, until="## Sources",
              drawings=[HERE / "board-schematic.svg"],
              documents=[HERE.parents[1] / "firmware" / "drivers" / "solenoid.md"])

ds = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="datasheet", **kw)
dec = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="decision", **kw)
asm = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="assumed", **kw)
msr = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="measured", **kw)
gr = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="graph", **kw)
fig = MODEL.derived

DIODE = "1N5817-1N5819-TSC.pdf"
IRL = "IRL540N-Infineon.pdf"

DRIVE = "The drive channel"
SENSE = "The sense channel"
COIL = "The coil"
SUPPLY = "The supply"

# ===========================================================================
# what the stock machine brings, all of it measured on the machine and
# recorded in research/Rokr/3_bumper-control.md
# ===========================================================================
msr("v_5v", 5.0, "V", src="the machine's rail, measured in operation at the coil",
    group="V_DRAIN", section=DRIVE, stated=True)
msr("v_coil_min", 4.4, "V", src="the least rail at which a stock coil still kicks, "
    "measured on the stock machine as 4.3 to 4.4 V; the higher end is the floor",
    group="V_DRAIN", section=DRIVE, stated=True)
msr("i_coil", 0.68, "A", src="measured in series at the coil while running, and "
    "5.0 V / 7.35 Ω gives the same",
    group="I_CH", section=DRIVE, stated=True)
ds("i_coil_scoop", 0.8, "A", src="Adafruit 3992 product page, current draw at 5 V; "
   "the scoop's coil, larger than the three stock coils and the one that sets the "
   "per-channel worst case, since all four drive channels are built identical",
   group="I_CH", section=DRIVE, stated=True)
dec("n_bumpers_at_once", 2, "", src="the most bumpers one ball can trigger within one "
    "pull-in, however fast it is; a second ball may sit in the scoop meanwhile", stated=False)
dec("n_stock_drive", 3, "", stated=False,
    src="the three drive channels wired to stock coils, the fourth to the scoop's")
msr("r_contact", 30, "Ω", src="the ball's path from foil to shell, measured at "
    "various points as 7 to 30 Ω; the upper end is the one that costs level",
    group="V_SENSE", section=SENSE, stated=True)

# ===========================================================================
# what the Teensy and the firmware bring
# ===========================================================================
MODEL.uses("v_3v3", of="teensy-4.1", group="V_GATE", section=DRIVE, stated=True)
MODEL.uses("v_3v3_acc", of="teensy-4.1", group="V_GATE", section=DRIVE, stated=True)
MODEL.uses("v_3v3_min", of="teensy-4.1")
MODEL.uses("v_3v3_max", of="teensy-4.1")
MODEL.input("kit_tol", 5, "%", kind="datasheet", src="the justPi resistor kit JUS-19729 every resistor comes from, Botland product page: tolerance 5 %", group="V_GATE", section=DRIVE, stated=True)
MODEL.input("p_kit", 0.25, "W", kind="datasheet", src="the same kit and page: power rating 0.25 W", group="I_FOIL", section=SENSE, stated=True)
MODEL.input("p_derate", 50, "%", kind="decision", src="the derating target for resistor power in the design-review skill, at most half the rating", stated=False)
MODEL.uses("v_jack_max", of="power", group="R_S", section=SENSE, stated=True)
MODEL.uses("v_not_tolerant", of="teensy-4.1")
MODEL.uses("i_pin_max", of="teensy-4.1", group="I_PIN", section=DRIVE, stated=True)
MODEL.uses("i_teensy_3v3", of="teensy-4.1", group="I_FOIL", section=SENSE, stated=True)
MODEL.uses("i_pin_fault_max", of="teensy-4.1", group="R_S", section=SENSE, stated=True)
MODEL.uses("v_clamp_over", of="teensy-4.1", group="R_S", section=SENSE, stated=True)
MODEL.uses("vih_frac", of="teensy-4.1")
MODEL.uses("vil_frac", of="teensy-4.1")
# Table 86 of IMXRT1060CEC gives every edge pin a keeper on reset, so a trigger pin has
# one wherever it sits. The keeper then holds the level the pin last drove, and the
# minimum is the strongest it can be.
MODEL.uses("r_keeper_min", of="teensy-4.1", group="V_GATE", section=DRIVE, stated=True)
MODEL.uses("t_tick", of="firmware")
MODEL.uses("t_monitor", of="firmware")
MODEL.uses("t_wdt", of="firmware", group="HOLD", section=COIL, stated=True)
MODEL.uses("v_in_max", of="teensy-4.1", group="C_BULK", section=SUPPLY, stated=True)
MODEL.uses("ball_diameter", of="ir-reflective", group="T_SENSE", section=SENSE, stated=True)

# ===========================================================================
# what this design chooses
# ===========================================================================
dec("n_drive", 4, "", stated=False,
    src="the three sensed solenoids plus the one unsensed solenoid")
dec("n_sense", 3, "", stated=False, src="one sense line per sensed solenoid's shell")
dec("r_gate", 1.2, "kΩ", src="chosen so the gate still clears the voltage R_DS(on) is "
    "read at against the power ground's rise, while the pin stays under its 4 mA at "
    "the switching moment: the 1.2 kΩ of the resistor kit",
    group="I_PIN", section=DRIVE, stated=True)
dec("r_gate_pd", 10, "kΩ", src="chosen to hold the gate under the lowest turn-on "
    "threshold against the pin's keeper after a reset, while the divider it forms with "
    "R_gate still leaves the gate above the voltage R_DS(on) is specified at; it is "
    "the value R31 to R33 and R41 to R43 carry, so the board holds one resistor value "
    "fewer: the 10 kΩ of the resistor kit", group="V_GATE", section=DRIVE, stated=True)
dec("r_pulldown", 10, "kΩ", src="chosen to hold the sense node at ground with no "
    "ball on the shell, at a fraction of a milliamp when one is: the 10 kΩ of the "
    "resistor kit", group="V_SENSE", section=SENSE, stated=True)
dec("r_series", 10, "kΩ", src="chosen so a sense wire meeting a coil wire drives "
    "under a milliamp into the pin even with the Teensy unpowered, where the ceiling "
    "is 0.31 V rather than the rail plus that; the pin draws nothing, so the value "
    "costs no sense level and nothing bounds it from above: the 10 kΩ of the resistor kit",
    group="R_S", section=SENSE, stated=True)
dec("r_foil", 330, "Ω", src="chosen to bound what the foil can draw from the Teensy's "
    "3.3 V rail if it reaches ground, at a value whose own fault dissipation stays "
    "well inside half the kit's rating and which still leaves level with all three contacts closed: "
    "the 330 Ω of the resistor kit", group="V_SENSE", section=SENSE, stated=True)
C91 = "KEMET ESL107M016AC3AA"
dec("c_c91", 100, "µF", src=f"C91, {C91}, 100 µF 16 V: the smallest standard value "
    "above the 67 µF the stock ratio below asks for", group="C_BULK", section=SUPPLY,
    stated=True)
ds("z_c91", 230, "mΩ", src=f"{C91}, ESR at 100 kHz, as DigiKey lists it from the ESL "
   "sheet", group="C_BULK", section=SUPPLY, stated=True)
ds("v_c91_rating", 16, "V", src=f"{C91}, rated voltage", group="C_BULK", section=SUPPLY,
   stated=True)
dec("c_filter", 100, "nF", src="chosen with r_pulldown for a settling time under how "
    "long a ball rests on a shell, at the value every other ceramic of the build has",
    group="T_SENSE", section=SENSE, stated=True)
dec("t_contact_fail", 2, "s", stated=False, src="how long a sensed solenoid's contact may stay closed before the driver "
    "reports DeviceFailed, far longer than a ball touches a shell")
dec("t_on_max", 30, "ms", src="the ceiling the driver enforces on one pull-in, "
    "at the top of the 18 to 30 ms the Mission Pinball Framework's coil examples use, "
    "and above the stock machine's visibly short pull: long enough to kick the ball "
    "away, short enough that a ball cannot be fired back and forth between the three top "
    "bumpers, and far below the seconds at which a mini solenoid overheats",
    group="DUTY", section=COIL, stated=True)
dec("t_late", 1, "ms", src="how far past its pull-in a coil may still be on before the "
    "main loop counts it overdue and stops feeding the watchdog; the driver tick ends a "
    "pull-in by t_on_max, late only by the other functions on the same tick, far inside this",
    group="DUTY", section=COIL, stated=True)
dec("t_rearm", 10, "ms", src="the cool-down after a release, during which the coil "
    "may not fire again, and how long a sensed solenoid's contact has to stay open before "
    "that channel is armed; decided above the time the plunger takes to stroke and "
    "return, which no mass or spring figure lets us compute",
    group="DUTY", section=COIL, stated=True)
dec("t_hold_max", 5, "s", src="the longest any coil may stay energised, from the "
    "requirements in design.md, set because the solenoids' vendor and specifications "
    "are unknown",
    group="HOLD", section=COIL, stated=True)

asm("l_feed", 0.3, "µH", src="the 5 V feed from the distribution, 30 cm of loose "
    "pair at roughly 1 µH per meter, estimated from the conductor and not measured",
    group="C_BULK", section=SUPPLY, stated=True)
asm("c_stray", 2, "nF", src="what the board's own copper offers the feed current with "
    "C91 unfitted, an order-of-magnitude estimate that C91 is there to make "
    "irrelevant", group="C_BULK", section=SUPPLY, stated=True)

asm("r_wire_short", 0.2, "Ω", src="a coil pair shorted against itself in the playfield "
    "loom, estimated from the conductor and its length, not measured",
    group="FAULT", section=SUPPLY, stated=True)
dec("derate_contact", 80, "%", src="the share of a connector's per-contact rating "
    "practice leaves unused, from the derating targets of the design-review skill",
    group="FAULT", section=SUPPLY, stated=True)

asm("place_gate", 10, "mm", stated=False, src="how close R11 to R14 and R21 to R24 "
    "sit to their gate, against gate loop pickup; no datasheet states one")
asm("place_filter", 15, "mm", stated=False, src="how close R31 to R33 and C1 to C3 "
    "sit to J-M, so the wire is filtered where it lands; no datasheet states one")
asm("place_flyback", 20, "mm", stated=False, src="how close D1 to D4 sit to their "
    "coil connector, which keeps the flyback loop short; no datasheet states one")

asm("r_lead", 0.1, "Ω", src="one wire of the build's cables with its two contacts; every "
    "wire is the same, so the ground paths differ only in how many wires they run "
    "through; estimated from the conductor and not measured",
    group="GND", section=SUPPLY, stated=True)

# ===========================================================================
# the ball, the one speed every subsystem assumes
# ===========================================================================
MODEL.uses("v_ball", of="ir-reflective", group="T_SENSE", section=SENSE, stated=True)

# ===========================================================================
# IRL540NPbF, Infineon, Rev. 2.1
# ===========================================================================
ds("v_ds_max", 100, "V", sheet=IRL, src="Electrical Characteristics, drain-to-source "
   "breakdown voltage minimum at V_GS = 0 V and I_D = 250 µA",
   group="V_DRAIN", section=DRIVE, stated=True)
ds("v_gs_max", 16, "V", sheet=IRL, src="Absolute Maximum Ratings, gate-to-source "
   "voltage, ±16 V", group="V_GATE", section=DRIVE, stated=True)
ds("i_d_max", 26, "A", sheet=IRL, src="Absolute Maximum Ratings, continuous drain "
   "current at T_C = 100 °C", group="I_CH", section=DRIVE, stated=True)
ds("v_gs_spec_min", 4.0, "V", sheet=IRL, src="Electrical Characteristics, the lowest "
   "gate voltage the sheet specifies R_DS(on) at, 0.063 Ω", group="Drive", stated="loose")
gr("v_gs_rdson", 2.5, "V", sheet=IRL, src="Fig. 1, typical output characteristics at "
   "T_J = 25 °C, its lowest curve", group="V_GATE", section=DRIVE, stated=True)
gr("rdson", 120, "mΩ", sheet=IRL, src="Fig. 1, the 2.5 V curve passes I_D = 1 A at "
   "V_DS = 0.12 V; a typical figure, since the sheet guarantees none below 4.0 V",
   group="P_Q", section=DRIVE, stated=True)
ds("vgsth_min", 1.0, "V", sheet=IRL, src="Electrical Characteristics, gate threshold "
   "voltage minimum at V_DS = V_GS and I_D = 250 µA",
   group="V_KICK", section=DRIVE, stated=True)
ds("vgsth_max", 2.0, "V", sheet=IRL, src="the same row, maximum",
   group="V_GATE", section=DRIVE, stated=True)
ds("c_iss", 1800, "pF", sheet=IRL, src="Electrical Characteristics, input capacitance "
   "typical at V_GS = 0 V, V_DS = 25 V and f = 1.0 MHz",
   group="V_KICK", section=DRIVE, stated=True)
ds("c_rss", 170, "pF", sheet=IRL, src="the same table, reverse transfer capacitance",
   group="V_KICK", section=DRIVE, stated=True)
ds("t_j_max", 175, "°C", sheet=IRL, src="Absolute Maximum Ratings, operating junction "
   "temperature, maximum", group="P_Q", section=DRIVE, stated=True)
ds("r_th_ja", 62, "°C/W", sheet=IRL, src="Thermal Resistance, junction-to-ambient, maximum",
   group="P_Q", section=DRIVE, stated=True)
asm("t_amb", 40, "°C", src="the highest air temperature inside the machine, the 40 °C the "
    "IR design assumes for its sensors", group="P_Q", section=DRIVE, stated=True)

# ===========================================================================
# 1N5819, Taiwan Semiconductor, Version H2104, the 1N5819 column
# ===========================================================================
ds("v_rrm", 40, "V", sheet=DIODE, src="Absolute Maximum Ratings, repetitive peak "
   "reverse voltage, 1N5819 column", group="V_DRAIN", section=DRIVE, stated=True)
ds("i_f_av", 1, "A", sheet=DIODE, src="Absolute Maximum Ratings, forward current", group="I_CH", section=DRIVE, stated=True)
ds("i_fsm", 30, "A", sheet=DIODE, src="Absolute Maximum Ratings, surge peak "
   "forward current, 8.3 ms single half sine wave",
   group="I_CH", section=DRIVE, stated=True)
ds("v_f_diode", 0.60, "V", sheet=DIODE, src="Electrical Specifications, "
   "forward voltage maximum at I_F = 1 A and T_J = 25 °C, "
   "1N5819 column", group="V_DRAIN", section=DRIVE, stated=True)


# ===========================================================================
# the stock machine's bulk at its power entry
# ===========================================================================
msr("c_stock", 200, "µF", src="two radial electrolytics marked 100 10V VT in parallel "
    "at the stock power entry, read from their markings in research/Rokr/1_power-supply.md",
    group="C_BULK", section=SUPPLY, stated=True)
dec("n_stock_coils", 3, "", stated=False,
    src="the three stock coils, which the machine energises together at power-on")

# ===========================================================================
# derived: the drive channel
# ===========================================================================
@fig("r_gate_total", "kΩ", group="V_GATE", section=DRIVE, stated=False,
     rises_with=["r_gate", "r_gate_pd"])
def _(r_gate, r_gate_pd):
    return r_gate + r_gate_pd


# Each resistor at the end of its tolerance that hurts the figure it enters.
@fig("r_gate_lo", "kΩ", stated=False, rises_with=["r_gate"], falls_with=["kit_tol"])
def _(r_gate, kit_tol):
    return r_gate * (1 - kit_tol)


@fig("r_gate_hi", "kΩ", stated=False, rises_with=["r_gate", "kit_tol"])
def _(r_gate, kit_tol):
    return r_gate * (1 + kit_tol)


@fig("r_gate_pd_lo", "kΩ", stated=False, rises_with=["r_gate_pd"], falls_with=["kit_tol"])
def _(r_gate_pd, kit_tol):
    return r_gate_pd * (1 - kit_tol)


@fig("r_gate_pd_hi", "kΩ", stated=False, rises_with=["r_gate_pd", "kit_tol"])
def _(r_gate_pd, kit_tol):
    return r_gate_pd * (1 + kit_tol)


# The gate while it is on, at its lowest: the rail at its low end, R_gate at its high end and
# R_gate to ground at its low end.
@fig("v_gate", "V", group="V_GATE", section=DRIVE, prints="down",
     rises_with=["v_3v3_min", "r_gate_pd"], falls_with=["r_gate", "kit_tol"])
def _(v_3v3_min, r_gate_pd_lo, r_gate_hi):
    return v_3v3_min * r_gate_pd_lo / (r_gate_hi + r_gate_pd_lo)


@fig("v_gate_margin", "mV", group="V_GATE", section=DRIVE, prints="down",
     rises_with=["v_3v3_min", "r_gate_pd"], falls_with=["r_gate", "kit_tol", "v_gs_rdson"])
def _(v_gate, v_gs_rdson):
    return v_gate - v_gs_rdson


# A reset during a pull-in leaves the keeper holding the pin high against the
# two resistors to ground.
@fig("v_pin_keeper", "V", group="V_GATE", section=DRIVE, prints="up",
     rises_with=["v_3v3_max", "r_gate", "r_gate_pd", "kit_tol"], falls_with=["r_keeper_min"])
def _(v_3v3_max, r_gate_hi, r_gate_pd_hi, r_keeper_min):
    return v_3v3_max * (r_gate_hi + r_gate_pd_hi) / (r_keeper_min + r_gate_hi + r_gate_pd_hi)


@fig("v_gate_keeper", "V", group="V_GATE", section=DRIVE, prints="up",
     rises_with=["v_3v3_max", "r_gate_pd"], falls_with=["r_keeper_min", "r_gate"])
def _(v_3v3_max, r_gate_lo, r_gate_pd_hi, r_keeper_min):
    return v_3v3_max * r_gate_pd_hi / (r_keeper_min + r_gate_lo + r_gate_pd_hi)


@fig("v_il", "V", group="V_GATE", section=DRIVE, prints="down",
     rises_with=["v_3v3_min", "vil_frac"])
def _(v_3v3_min, vil_frac):
    return v_3v3_min * vil_frac


# The most R_gate to ground whose high end still holds the gate under the lowest turn-on
# threshold against the keeper, with R_gate at its low end.
@fig("r_gate_pd_max", "kΩ", group="V_GATE", section=DRIVE, prints="down",
     rises_with=["vgsth_min", "r_keeper_min", "r_gate"], falls_with=["v_3v3_max", "kit_tol"])
def _(vgsth_min, r_keeper_min, r_gate_lo, v_3v3_max, kit_tol):
    return vgsth_min * (r_keeper_min + r_gate_lo) / ((v_3v3_max - vgsth_min) * (1 + kit_tol))


@fig("i_pin_peak", "mA", group="I_PIN", section=DRIVE,
     rises_with=["v_3v3_max", "kit_tol"], falls_with=["r_gate"])
def _(v_3v3_max, r_gate_lo):
    return v_3v3_max / r_gate_lo


@fig("i_pin_hold", "µA", group="I_PIN", section=DRIVE,
     rises_with=["v_3v3_max", "kit_tol"], falls_with=["r_gate", "r_gate_pd"])
def _(v_3v3_max, r_gate_lo, r_gate_pd_lo):
    return v_3v3_max / (r_gate_lo + r_gate_pd_lo)


# The least R_gate whose low end holds the pin inside what PJRC allows at the switching moment.
@fig("r_gate_min", "kΩ", group="I_PIN", section=DRIVE, prints="up",
     rises_with=["v_3v3_max", "kit_tol"], falls_with=["i_pin_max"])
def _(v_3v3_max, i_pin_max, kit_tol):
    return v_3v3_max / (i_pin_max * (1 - kit_tol))


@fig("v_kick", "V", group="V_KICK", section=DRIVE,
     rises_with=["v_5v", "v_f_diode", "c_rss"], falls_with=["c_iss"])
def _(v_drain_off, c_rss, c_iss):
    return v_drain_off * c_rss / c_iss


@fig("t_gate", "µs", group="I_PIN", section=DRIVE,
     rises_with=["r_gate", "kit_tol", "c_iss"])
def _(r_gate_hi, c_iss):
    return r_gate_hi * c_iss


@fig("t_gate_decay", "µs", group="V_KICK", section=DRIVE,
     rises_with=["r_gate_pd", "c_iss"])
def _(r_gate_pd, c_iss):
    return r_gate_pd * c_iss


@fig("v_drain_off", "V", group="V_DRAIN", section=DRIVE,
     rises_with=["v_5v", "v_f_diode"])
def _(v_5v, v_f_diode):
    return v_5v + v_f_diode


@fig("v_ds_on", "mV", group="P_Q", section=DRIVE,
     rises_with=["i_coil_scoop", "rdson"])
def _(i_coil_scoop, rdson):
    return i_coil_scoop * rdson


@fig("p_mosfet", "mW", group="P_Q", section=DRIVE,
     rises_with=["i_coil_scoop", "rdson"])
def _(i_coil_scoop, rdson):
    return i_coil_scoop * i_coil_scoop * rdson


@fig("t_j_q", "°C", group="P_Q", section=DRIVE, prints="up",
     rises_with=["t_amb", "i_coil_scoop", "rdson", "r_th_ja"])
def _(t_amb, p_mosfet, r_th_ja):
    return t_amb + p_mosfet * r_th_ja


# The most that pulls at once in play: two bumpers off one ball, and the scoop ejecting
# the other ball at the same moment.
@fig("i_coils_at_once", "A", group="I_FEED", section=DRIVE,
     rises_with=["n_bumpers_at_once", "i_coil", "i_coil_scoop"])
def _(n_bumpers_at_once, i_coil, i_coil_scoop):
    return n_bumpers_at_once * i_coil + i_coil_scoop


@fig("i_feed_max", "A", group="I_FEED", section=DRIVE,
     rises_with=["i_coil", "i_coil_scoop", "n_stock_drive"])
def _(i_coil, i_coil_scoop, n_stock_drive):
    return n_stock_drive * i_coil + i_coil_scoop


# ===========================================================================
# derived: the sense channel
# ===========================================================================
@fig("v_ih", "V", group="V_SENSE", section=SENSE,
     rises_with=["v_3v3", "vih_frac"])
def _(v_3v3, vih_frac):
    return v_3v3 * vih_frac


@fig("v_sense_high", "V", group="V_SENSE", section=SENSE,
     rises_with=["v_3v3", "r_pulldown"], falls_with=["r_contact", "r_foil"])
def _(v_3v3, r_pulldown, r_contact, r_foil):
    return v_3v3 * r_pulldown / (r_pulldown + r_contact + r_foil)


@fig("v_sense_all", "V", group="V_SENSE", section=SENSE,
     rises_with=["v_3v3", "r_pulldown"],
     falls_with=["r_contact", "r_foil", "n_sense"])
def _(v_3v3, r_pulldown, r_contact, r_foil, n_sense):
    return v_3v3 * r_pulldown / (r_pulldown + r_contact + n_sense * r_foil)


@fig("i_sense", "mA", group="I_FOIL", section=SENSE,
     rises_with=["v_3v3"], falls_with=["r_pulldown", "r_contact", "r_foil"])
def _(v_3v3, r_pulldown, r_contact, r_foil):
    return v_3v3 / (r_pulldown + r_contact + r_foil)


@fig("i_foil", "mA", group="I_FOIL", section=SENSE,
     rises_with=["n_sense", "v_3v3"],
     falls_with=["r_pulldown", "r_contact", "r_foil"])
def _(v_3v3, n_sense, r_pulldown, r_contact, r_foil):
    return v_3v3 * n_sense / (n_sense * r_foil + r_contact + r_pulldown)


@fig("i_foil_short", "mA", group="I_FOIL", section=SENSE,
     rises_with=["v_3v3"], falls_with=["r_foil"])
def _(v_3v3, r_foil):
    return v_3v3 / r_foil


@fig("p_foil_short", "mW", group="I_FOIL", section=SENSE,
     rises_with=["v_3v3"], falls_with=["r_foil"])
def _(v_3v3, r_foil):
    return v_3v3 * v_3v3 / r_foil


@fig("v_pin_clamp", "V", group="R_S", section=SENSE,
     rises_with=["v_3v3", "v_clamp_over"])
def _(v_3v3, v_clamp_over):
    return v_3v3 + v_clamp_over


@fig("i_pin_fault", "mA", group="R_S", section=SENSE,
     rises_with=["v_5v"], falls_with=["r_series", "v_clamp_over"])
def _(v_5v, v_clamp_over, r_series):
    return (v_5v - v_clamp_over) / r_series


@fig("i_foil_fault", "mA", group="I_FOIL", section=SENSE,
     rises_with=["v_5v"], falls_with=["r_foil", "v_clamp_over"])
def _(v_5v, v_clamp_over, r_foil):
    return (v_5v - v_clamp_over) / r_foil


@fig("t_open", "µs", group="T_SENSE", section=SENSE,
     rises_with=["r_pulldown", "c_filter"])
def _(r_pulldown, c_filter):
    return r_pulldown * c_filter


@fig("t_close", "µs", group="T_SENSE", section=SENSE,
     rises_with=["r_contact", "r_foil", "c_filter"])
def _(r_contact, r_foil, c_filter):
    return (r_contact + r_foil) * c_filter


@fig("t_contact", "ms", group="T_SENSE", section=SENSE,
     rises_with=["ball_diameter"], falls_with=["v_ball"])
def _(ball_diameter, v_ball):
    return ball_diameter / v_ball


# The least R_pulldown whose low end keeps the pin above its high level with all three
# contacts closed, R91 at its high end; the rail scales both sides and drops out.
@fig("r_pulldown_min", "kΩ", group="V_SENSE", section=SENSE, prints="up",
     rises_with=["vih_frac", "r_contact", "n_sense", "r_foil", "kit_tol"])
def _(vih_frac, r_contact, n_sense, r_foil, kit_tol):
    return (vih_frac * (r_contact + n_sense * r_foil * (1 + kit_tol))
            / ((1 - vih_frac) * (1 - kit_tol)))


# The most R91 whose high end still does that, with R_pulldown at its low end.
@fig("r_foil_max", "kΩ", group="V_SENSE", section=SENSE, prints="down",
     rises_with=["r_pulldown"], falls_with=["vih_frac", "r_contact", "n_sense", "kit_tol"])
def _(r_pulldown, vih_frac, r_contact, n_sense, kit_tol):
    return ((r_pulldown * (1 - kit_tol) * (1 - vih_frac) / vih_frac - r_contact)
            / (n_sense * (1 + kit_tol)))


# The least R91 whose low end keeps a foil held at ground, the rail at its high end, under
# the share of the kit's rating every resistor is held to.
@fig("r_foil_min", "Ω", group="I_FOIL", section=SENSE, prints="up",
     rises_with=["v_3v3_max", "kit_tol"], falls_with=["p_kit", "p_derate"])
def _(v_3v3_max, p_kit, p_derate, kit_tol):
    return v_3v3_max * v_3v3_max / (p_kit * p_derate * (1 - kit_tol))


# The least series resistor whose low end holds a sense wire on a coil wire under PJRC's
# milliamp into an unpowered pin, with the rail where the adapter puts it with no load.
@fig("r_series_min", "kΩ", group="R_S", section=SENSE, prints="up",
     rises_with=["v_jack_max", "kit_tol"], falls_with=["v_clamp_over", "i_pin_fault_max"])
def _(v_jack_max, v_clamp_over, i_pin_fault_max, kit_tol):
    return (v_jack_max - v_clamp_over) / (i_pin_fault_max * (1 - kit_tol))


# The most R_pulldown whose high end still lets the node fall inside one contact.
@fig("r_pulldown_max", "kΩ", group="T_SENSE", section=SENSE, prints="down",
     rises_with=["ball_diameter"], falls_with=["v_ball", "c_filter", "kit_tol"])
def _(t_contact, c_filter, kit_tol):
    return t_contact / (c_filter * (1 + kit_tol))


# The largest filter whose node still falls inside one contact, the ceiling on C_filter.
@fig("c_filter_max", "nF", group="T_SENSE", section=SENSE, prints="down",
     rises_with=["ball_diameter"], falls_with=["v_ball", "r_pulldown"])
def _(t_contact, r_pulldown):
    return t_contact / r_pulldown


# ===========================================================================
# derived: the coil
# ===========================================================================
@fig("p_coil", "W", group="DUTY", section=COIL,
     rises_with=["v_5v", "i_coil"])
def _(v_5v, i_coil):
    return v_5v * i_coil


# The driver tick ends a pull on the first tick after it has been on this long,
# so a pull lasts from t_on_min up to t_on_max.
@fig("t_on_min", "ms", stated=False, rises_with=["t_on_max"], falls_with=["t_tick"])
def _(t_on_max, t_tick):
    return t_on_max - t_tick


@fig("t_on_actual", "ms", group="DUTY", section=COIL,
     rises_with=["t_on_max", "t_late"])
def _(t_on_max, t_late):
    return t_on_max + t_late


@fig("t_cycle_min", "ms", group="DUTY", section=COIL,
     rises_with=["t_on_max", "t_late", "t_rearm"])
def _(t_on_actual, t_rearm):
    return t_on_actual + t_rearm


@fig("duty_max", "%", group="DUTY", section=COIL,
     rises_with=["t_on_max", "t_late"], falls_with=["t_rearm"])
def _(t_on_actual, t_cycle_min):
    return t_on_actual / t_cycle_min


@fig("p_coil_avg", "W", group="DUTY", section=COIL,
     rises_with=["v_5v", "i_coil", "t_on_max", "t_late"], falls_with=["t_rearm"])
def _(p_coil, duty_max):
    return p_coil * duty_max


# The worst case is a coil the driver tick never releases: the loop feeds the watchdog
# until the coil turns overdue, and the restart comes one timeout later.
@fig("t_hold_fault", "s", group="HOLD", section=COIL, prints="up",
     rises_with=["t_wdt", "t_on_max", "t_late"])
def _(t_wdt, t_on_actual):
    return t_wdt + t_on_actual


# ===========================================================================
# derived: the supply
# ===========================================================================
# The Teensy's VIN hangs on the same rail, so the rise at switch-off stays under
# what VIN takes.
@fig("dv_allowed", "V", group="C_BULK", section=SUPPLY,
     rises_with=["v_in_max"], falls_with=["v_5v"])
def _(v_in_max, v_5v):
    return v_in_max - v_5v


@fig("i_stock", "A", group="C_BULK", section=SUPPLY,
     rises_with=["n_stock_coils", "i_coil"])
def _(n_stock_coils, i_coil):
    return n_stock_coils * i_coil


# At switch-off the feed's inductance drives its current on into the bulk: half
# L I squared into half C dV squared, solved for C.
@fig("c_bulk_min", "µF", group="C_BULK", section=SUPPLY, prints="up",
     rises_with=["l_feed", "i_coil_scoop", "v_5v", "z_c91"], falls_with=["v_in_max"])
def _(l_feed, i_coil_scoop, dv_allowed, z_c91):
    room = dv_allowed - i_coil_scoop * z_c91   # what the part's impedance leaves the capacitance
    if room.v <= 0:                    # a rail already at VIN's limit leaves no rise to absorb
        return Q.of(inf, "µF")
    return l_feed * (i_coil_scoop / room) ** 2


# The rise C91 lets through: the feed's current into its capacitance, plus the same
# current across its impedance.
@fig("v_spike", "mV", group="C_BULK", section=SUPPLY,
     rises_with=["i_coil_scoop", "l_feed", "z_c91"], falls_with=["c_c91"])
def _(i_coil_scoop, l_feed, c_c91, z_c91):
    return i_coil_scoop * (l_feed / c_c91) ** (1 / 2) + i_coil_scoop * z_c91


@fig("v_spike_bare", "V", group="C_BULK", section=SUPPLY,
     rises_with=["i_coil_scoop", "l_feed"], falls_with=["c_stray"])
def _(i_coil_scoop, l_feed, c_stray):
    return i_coil_scoop * (l_feed / c_stray) ** (1 / 2)


@fig("v_drain_spike", "V", group="C_BULK", section=SUPPLY,
     rises_with=["v_5v", "v_f_diode", "i_coil_scoop", "l_feed", "z_c91"], falls_with=["c_c91"])
def _(v_drain_off, v_spike):
    return v_drain_off + v_spike


@fig("i_coil_short", "A", group="FAULT", section=SUPPLY,
     rises_with=["v_5v"], falls_with=["r_wire_short", "rdson"])
def _(v_5v, r_wire_short, rdson):
    return v_5v / (r_wire_short + rdson)


@fig("p_q_short", "W", group="FAULT", section=SUPPLY,
     rises_with=["v_5v", "rdson"], falls_with=["r_wire_short"])
def _(i_coil_short, rdson):
    return i_coil_short * i_coil_short * rdson


# The power ground reaches the distribution over J-PWR alone, so all four coils
# at once lift it by their whole current across that one wire. The signal
# ground and the Teensy carry none of it.
@fig("v_power_rise", "mV", group="GND", section=SUPPLY, prints="up",
     rises_with=["i_coil", "i_coil_scoop", "n_stock_drive", "r_lead"])
def _(i_feed_max, r_lead):
    return i_feed_max * r_lead


# The gate is driven from the Teensy's ground and measured against the power
# ground, so the rise comes off the pin's level before the divider.
@fig("v_gate_shifted", "V", group="GND", section=SUPPLY, prints="down",
     rises_with=["v_3v3_min", "r_gate_pd"], falls_with=["r_gate", "kit_tol", "r_lead", "i_coil"])
def _(v_3v3_min, v_power_rise, r_gate_pd_lo, r_gate_hi):
    return (v_3v3_min - v_power_rise) * r_gate_pd_lo / (r_gate_hi + r_gate_pd_lo)


@fig("v_gate_margin_shifted", "mV", group="GND", section=SUPPLY, prints="down",
     rises_with=["v_3v3_min", "r_gate_pd"],
     falls_with=["r_gate", "kit_tol", "r_lead", "v_gs_rdson"])
def _(v_gate_shifted, v_gs_rdson):
    return v_gate_shifted - v_gs_rdson


# The two ends of the gate divider's window: the most R_gate, and the least R_gate to ground,
# that still leave the gate above the voltage R_DS(on) is read at, with the power
# ground's rise against it and each at the end of its tolerance that hurts.
@fig("r_gate_max", "kΩ", group="GND", section=SUPPLY, prints="down",
     rises_with=["v_3v3_min", "r_gate_pd"],
     falls_with=["kit_tol", "r_lead", "i_coil", "v_gs_rdson"])
def _(v_3v3_min, v_power_rise, v_gs_rdson, r_gate_pd_lo, kit_tol):
    return r_gate_pd_lo * ((v_3v3_min - v_power_rise) / v_gs_rdson - 1) / (1 + kit_tol)


@fig("r_gate_pd_min", "kΩ", group="GND", section=SUPPLY, prints="up",
     rises_with=["r_gate", "kit_tol", "r_lead", "i_coil", "v_gs_rdson"],
     falls_with=["v_3v3_min"])
def _(r_gate_hi, v_gs_rdson, v_3v3_min, v_power_rise, kit_tol):
    return r_gate_hi * v_gs_rdson / ((v_3v3_min - v_power_rise - v_gs_rdson) * (1 - kit_tol))


@fig("v_sense_margin", "mV", group="GND", section=SUPPLY, stated=False,
     rises_with=["v_3v3", "r_pulldown"],
     falls_with=["r_contact", "r_foil", "n_sense", "vih_frac"])
def _(v_sense_all, v_ih):
    return v_sense_all - v_ih


# ===========================================================================
# what the design requires, stated over the quantities rather than over any
# formula above
# ===========================================================================
_I = MODEL.invariant

_I("the gate clears the 2.5 V of the typical curve the drop across the switch is read "
   "from",
   lambda v: v.v_gate > v.v_gs_rdson)
_I("and clears the worst-case turn-on threshold well before that",
   lambda v: v.v_gate > v.vgsth_max)
_I("the gate stays inside its rating",
   lambda v: v.v_gate < v.v_gs_max)
_I("the Teensy pin is not loaded past what PJRC allows, transient included",
   lambda v: v.i_pin_peak < v.i_pin_max)
_I("the drain step at power-up cannot turn the part on",
   lambda v: v.v_kick < v.vgsth_min)
_I("the clamped drain stays inside the MOSFET's drain-source rating",
   lambda v: v.v_drain_off < v.v_ds_max)
_I("the clamped drain stays inside the diode's reverse rating",
   lambda v: v.v_drain_off < v.v_rrm)
_I("the coil fits the diode, which is the tightest part of a channel, "
   "taken at the scoop's coil, the largest of the four",
   lambda v: v.i_coil_scoop < v.i_f_av)
_I("the channel ceiling the diode sets fits the MOSFET",
   lambda v: v.i_f_av < v.i_d_max)
_I("switch-off is far inside the diode's surge rating",
   lambda v: v.i_coil_scoop < v.i_fsm)
_I("the MOSFET's junction stays under its rating while the scoop's coil is energised",
   lambda v: v.t_j_q < v.t_j_max)
_I("after a reset during a pull-in the keeper, at its strongest, holds the gate under "
   "the lowest turn-on threshold",
   lambda v: v.v_gate_keeper < v.vgsth_min)
_I("and the pin then reads as a low",
   lambda v: v.v_pin_keeper < v.v_il)
_I("a closed contact clears the input's high level, one at a time",
   lambda v: v.v_sense_high > v.v_ih)
_I("and with all three closed at once, which shares the foil resistor",
   lambda v: v.v_sense_all > v.v_ih)
_I("a sense wire meeting a coil wire drives under a milliamp into the pin's clamp",
   lambda v: v.i_pin_fault < v.i_pin_fault_max)
_I("the foil resistor holds a foil at ground inside half its rating",
   lambda v: v.p_foil_short <= v.p_kit * v.p_derate)
_I("the bulk per ampere of solenoid stays above the stock machine's, taken at the "
   "one coil the firmware allows; the unarbitrated case is not one a capacitor "
   "rescues, since its demand already exceeds the supply",
   lambda v: v.c_c91 * v.i_stock > v.c_stock * v.i_coil)
_I("the bulk capacitor is bought at twice the rail it sits across",
   lambda v: v.v_c91_rating >= 2 * v.v_5v)
_I("the gate still clears the curve's 2.5 V, with the power ground's rise against it",
   lambda v: v.v_gate_shifted > v.v_gs_rdson)
_I("the drain stays inside its rating through the switch-off spike C91 leaves",
   lambda v: v.v_drain_spike < v.v_ds_max)
_I("C91 holds the rise at switch-off under what the Teensy's VIN takes",
   lambda v: v.c_c91 >= v.c_bulk_min)
_I("the foil fits the Teensy's 3V3 pin",
   lambda v: v.i_foil < v.i_teensy_3v3)
_I("the sense filter settles well inside how long a ball rests on a shell",
   lambda v: v.t_open < v.t_contact and v.t_close < v.t_open)
_I("the sense filter stays under the largest that still settles",
   lambda v: v.c_filter <= v.c_filter_max)
_I("the window a contact has to stay open outlasts the contact one hit makes",
   lambda v: v.t_rearm > v.t_contact)
_I("the overdue margin sits far inside the pull-in it is added to",
   lambda v: v.t_late < v.t_on_max)
_I("a coil the driver tick never releases is released by the restart inside the limit "
   "the requirements set",
   lambda v: v.t_hold_fault < v.t_hold_max)
_I("the gate settles far inside the shortest pull-in the driver can command",
   lambda v: v.t_gate < v.t_on_min)
_I("R11 to R14 sit inside their window", lambda v: v.r_gate_min <= v.r_gate <= v.r_gate_max)
_I("R21 to R24 sit inside their window",
   lambda v: v.r_gate_pd_min <= v.r_gate_pd <= v.r_gate_pd_max)
_I("R31 to R33 sit inside their window",
   lambda v: v.r_pulldown_min <= v.r_pulldown <= v.r_pulldown_max)
_I("R41 to R43 sit above their floor", lambda v: v.r_series_min <= v.r_series)
_I("R91 sits inside its window", lambda v: v.r_foil_min <= v.r_foil <= v.r_foil_max)


# the driver's constants, as docs/firmware/drivers/solenoid.md writes them
MODEL.as_written("t_on_max_us", of="t_on_max", unit="µs")
MODEL.as_written("t_rearm_us", of="t_rearm", unit="µs")
MODEL.as_written("t_late_us", of="t_late", unit="µs")
MODEL.as_written("t_contact_fail_ms", of="t_contact_fail", unit="ms")


# ===========================================================================
# what the documents state that the model does not compute
# ===========================================================================
for _text, _why in [
    ("2", "a coil connector's pin count, and the second pin of one"),
]:
    MODEL.aside(_text, _why)


# ===========================================================================
# wiring
# ===========================================================================
# each trigger drives its MOSFET's gate through R11 to R14, and R21 to R24 pull the gate
# to power ground whenever the Teensy does not drive the pin
for _n in range(1, 5):
    MODEL.net(f"Trigger {_n}, Teensy side",
              reads(f"Q{_n} gate through R1{_n}", src="design.md, R11 to R14 in series with the gates"),
              pull(f"R2{_n} behind R1{_n}", "gnd", src="design.md, R21 to R24, the gate pull-downs on power ground"),
              teensy=f"Solenoid trigger {_n}")

# a ball bridges the foil and a solenoid's shell, which lifts that sense node to the
# Teensy's 3V3 through R91; R31 to R33 hold it at signal ground otherwise
for _n in range(1, 4):
    MODEL.net(f"Sense {_n}, Teensy side",
              contact(f"the ball between the foil and shell {_n}, through R91 and R4{_n}", to=TEENSY_RAIL,
                      src="design.md, the foil is fed from the Teensy's 3V3 through R91"),
              pull(f"R3{_n} behind R4{_n}", "gnd", src="design.md, R31 to R33, the sense pull-downs on signal ground"),
              teensy=f"Solenoid sense {_n}")

MODEL.draws("i_foil", pool=TEENSY_RAIL)   # the foil, fed from the Teensy's 3V3
MODEL.draws(pool="driver tick")      # release() ends a pull-in on the tick after it is due
MODEL.draws(pool="device monitor")
