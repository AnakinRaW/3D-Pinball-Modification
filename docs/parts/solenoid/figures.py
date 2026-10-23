"""The figures of the solenoid board, as inputs plus formulas."""
from math import inf

from figcheck import *

MODEL = Model.at(__file__, "solenoid", drawings=["board-schematic.svg"],
                 documents=["../../firmware/drivers/solenoid.md"])
fig, ds, gr, msr, asm, dec = MODEL.declarers()

DIODE = "1N5817-1N5819-TSC.pdf"
IRL = "IRL540N-Infineon.pdf"

DRIVE = "The drive channel"
SENSE = "The sense channel"
COIL = "The coil"
SUPPLY = "The supply"

# -- what the stock machine brings, all of it measured on the machine and -----
with MODEL.block("V_DRAIN", section=DRIVE):
    # recorded in research/Rokr/3_bumper-control.md
    msr("v_5v", 5.0, "V", src="the machine's rail, measured in operation at the coil")
    msr("v_coil_min", 4.4, "V", src="the least rail at which a stock coil still kicks, measured on "
        "the stock machine as 4.3 to 4.4 V; the higher end is the floor")
msr("i_coil", 0.68, "A", src="measured in series at the coil while running, and 5.0 V / 7.35 Ω "
    "gives the same", group="I_CH", section=DRIVE, stated=True)
ds("i_coil_scoop", 0.8, "A", src="Adafruit 3992 product page, current draw at 5 V; the scoop's "
   "coil, larger than the three stock coils and the one that sets the per-channel worst case, "
   "since all four drive channels are built identical", group="I_CH", section=DRIVE, stated=True)
dec("n_bumpers_at_once", 2, "", src="the most bumpers one ball can trigger within one pull-in, "
    "however fast it is; a second ball may sit in the scoop meanwhile")
dec("n_stock_drive", 3, "", src="the three drive channels wired to stock coils, the fourth to the "
    "scoop's")
msr("r_contact", 30, "Ω", src="the ball's path from foil to shell, measured at various points as "
    "7 to 30 Ω; the upper end is the one that costs level", group="V_SENSE", section=SENSE,
    stated=True)

# -- what the Teensy and the firmware bring -----------------------------------
with MODEL.block("V_GATE", section=DRIVE):
    MODEL.uses("v_3v3", of="teensy-4.1")
    MODEL.uses("v_3v3_acc", of="teensy-4.1")
    MODEL.uses("v_3v3_min", of="teensy-4.1", stated=False)
    MODEL.uses("v_3v3_max", of="teensy-4.1", stated=False)
    ds("kit_tol", 5, "%", src="the justPi resistor kit JUS-19729 every resistor comes from, "
       "Botland product page: tolerance 5 %")
ds("p_kit", 0.25, "W", src="the same kit and page: power rating 0.25 W", group="I_FOIL",
   section=SENSE, stated=True)
dec("p_derate", 50, "%", src="the derating target for resistor power in the design-review skill, "
    "at most half the rating")
MODEL.uses("v_jack_max", of="power", group="R_S", section=SENSE, stated=True)
MODEL.uses("v_not_tolerant", of="teensy-4.1")
MODEL.uses("i_pin_max", of="teensy-4.1", group="I_PIN", section=DRIVE, stated=True)
with MODEL.block(section=SENSE):
    MODEL.uses("i_teensy_3v3", of="teensy-4.1", group="I_FOIL")
    MODEL.uses("i_pin_fault_max", of="teensy-4.1", group="R_S")
    MODEL.uses("v_clamp_over", of="teensy-4.1", group="R_S")
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

# -- what this design chooses -------------------------------------------------
dec("n_drive", 4, "", src="the three sensed solenoids plus the one unsensed solenoid")
dec("n_sense", 3, "", src="one sense line per sensed solenoid's shell")
dec("r_gate", 1.2, "kΩ", src="chosen so the gate still clears the voltage R_DS(on) is read at "
    "against the power ground's rise, while the pin stays under its 4 mA at the switching moment: "
    "the 1.2 kΩ of the resistor kit", group="I_PIN", section=DRIVE, stated=True)
dec("r_gate_pd", 10, "kΩ", src="chosen to hold the gate under the lowest turn-on threshold against "
    "the pin's keeper after a reset, while the divider it forms with R_gate still leaves the gate "
    "above the voltage R_DS(on) is specified at; it is the value R31 to R33 and R41 to R43 carry, "
    "so the board holds one resistor value fewer: the 10 kΩ of the resistor kit", group="V_GATE",
    section=DRIVE, stated=True)
with MODEL.block(section=SENSE):
    dec("r_pulldown", 10, "kΩ", src="chosen to hold the sense node at ground with no ball on the "
        "shell, at a fraction of a milliamp when one is: the 10 kΩ of the resistor kit",
        group="V_SENSE")
    dec("r_series", 10, "kΩ", src="chosen so a sense wire meeting a coil wire drives under a "
        "milliamp into the pin even with the Teensy unpowered, where the ceiling is 0.3 V rather "
        "than the rail plus that; the pin draws nothing, so the value costs no sense level and "
        "nothing bounds it from above: the 10 kΩ of the resistor kit", group="R_S")
    dec("r_foil", 330, "Ω", src="chosen to bound what the foil can draw from the Teensy's 3.3 V "
        "rail if it reaches ground, at a value whose own fault dissipation stays well inside half "
        "the kit's rating and which still leaves level with all three contacts closed: the 330 Ω "
        "of the resistor kit", group="V_SENSE")
C91 = "KEMET ESL107M016AC3AA"
with MODEL.block("C_BULK", section=SUPPLY):
    dec("c_c91", 100, "µF", src=f"C91, {C91}, 100 µF 16 V: the smallest standard value above the "
        "67 µF the stock ratio below asks for")
    ds("z_c91", 230, "mΩ", src=f"{C91}, ESR at 100 kHz, as DigiKey lists it from the ESL sheet")
    ds("v_c91_rating", 16, "V", src=f"{C91}, rated voltage")
dec("c_filter", 100, "nF", src="chosen with r_pulldown for a settling time under how long a ball "
    "rests on a shell, at the value every other ceramic of the build has", group="T_SENSE",
    section=SENSE, stated=True)
dec("t_contact_fail", 2, "s", src="how long a sensed solenoid's contact may stay closed before the "
    "driver reports DeviceFailed, far longer than a ball touches a shell")
with MODEL.block("DUTY", section=COIL):
    dec("t_on_max", 30, "ms", src="the ceiling the driver enforces on one pull-in, at the top of "
        "the 18 to 30 ms the Mission Pinball Framework's coil examples use, and above the stock "
        "machine's visibly short pull: long enough to kick the ball away, short enough that a ball "
        "cannot be fired back and forth between the three top bumpers, and far below the seconds "
        "at which a mini solenoid overheats")
    dec("t_late", 1, "ms", src="how far past its pull-in a coil may still be on before the main "
        "loop counts it overdue and stops feeding the watchdog; the driver tick ends a pull-in by "
        "t_on_max, late only by the other functions on the same tick, far inside this")
    dec("t_rearm", 10, "ms", src="the cool-down after a release, during which the coil may not "
        "fire again, and how long a sensed solenoid's contact has to stay open before that channel "
        "is armed; decided above the time the plunger takes to stroke and return, which no mass or "
        "spring figure lets us compute")
dec("t_hold_max", 5, "s", src="the longest any coil may stay energised, from the requirements in "
    "design.md, set because the solenoids' vendor and specifications are unknown", group="HOLD",
    section=COIL, stated=True)
with MODEL.block("C_BULK", section=SUPPLY):
    asm("l_feed", 0.3, "µH", src="the 5 V feed from the distribution, 30 cm of loose pair at "
        "roughly 1 µH per meter, estimated from the conductor and not measured")
    asm("c_stray", 2, "nF", src="what the board's own copper offers the feed current with C91 "
        "unfitted, an order-of-magnitude estimate that C91 is there to make irrelevant")
with MODEL.block("FAULT", section=SUPPLY):
    asm("r_wire_short", 0.2, "Ω", src="a coil pair shorted against itself in the playfield loom, "
        "estimated from the conductor and its length, not measured")
    dec("derate_contact", 80, "%", src="the share of a connector's per-contact rating practice "
        "leaves unused, from the derating targets of the design-review skill")

asm("place_gate", 10, "mm", src="how close R11 to R14 and R21 to R24 sit to their gate, against "
    "gate loop pickup; no datasheet states one")
asm("place_filter", 15, "mm", src="how close R31 to R33 and C1 to C3 sit to J-M, so the wire is "
    "filtered where it lands; no datasheet states one")
asm("place_flyback", 20, "mm", src="how close D1 to D4 sit to their coil connector, which keeps "
    "the flyback loop short; no datasheet states one")
asm("r_lead", 0.1, "Ω", src="one wire of the build's cables with its two contacts; every wire is "
    "the same, so the ground paths differ only in how many wires they run through; estimated from "
    "the conductor and not measured", group="GND", section=SUPPLY, stated=True)

# -- the ball, the one speed every subsystem assumes --------------------------
MODEL.uses("v_ball", of="ir-reflective", group="T_SENSE", section=SENSE, stated=True)

# -- IRL540NPbF, Infineon, Rev. 2.1 -------------------------------------------
with MODEL.block(section=DRIVE):
    ds("v_ds_max", 100, "V", sheet=IRL, src="Electrical Characteristics, drain-to-source breakdown "
       "voltage minimum at V_GS = 0 V and I_D = 250 µA", group="V_DRAIN")
    ds("v_gs_max", 16, "V", sheet=IRL, src="Absolute Maximum Ratings, gate-to-source voltage, "
       "±16 V", group="V_GATE")
    ds("i_d_max", 26, "A", sheet=IRL, src="Absolute Maximum Ratings, continuous drain current at "
       "T_C = 100 °C", group="I_CH")
ds("v_gs_spec_min", 4.0, "V", sheet=IRL, src="Electrical Characteristics, the lowest gate voltage "
   "the sheet specifies R_DS(on) at, 0.063 Ω", group="Drive", stated="loose")
with MODEL.block(section=DRIVE):
    gr("v_gs_rdson", 2.5, "V", sheet=IRL, src="Fig. 1, typical output characteristics at T_J = "
       "25 °C, its lowest curve", group="V_GATE")
    gr("rdson", 120, "mΩ", sheet=IRL, src="Fig. 1, the 2.5 V curve passes I_D = 1 A at V_DS = "
       "0.12 V; a typical figure, since the sheet guarantees none below 4.0 V", group="P_Q")
    ds("vgsth_min", 1.0, "V", sheet=IRL, src="Electrical Characteristics, gate threshold voltage "
       "minimum at V_DS = V_GS and I_D = 250 µA", group="V_KICK")
    ds("vgsth_max", 2.0, "V", sheet=IRL, src="the same row, maximum", group="V_GATE")
with MODEL.block("V_KICK", section=DRIVE):
    ds("c_iss", 1800, "pF", sheet=IRL, src="Electrical Characteristics, input capacitance typical "
       "at V_GS = 0 V, V_DS = 25 V and f = 1.0 MHz")
    ds("c_rss", 170, "pF", sheet=IRL, src="the same table, reverse transfer capacitance")
with MODEL.block("P_Q", section=DRIVE):
    ds("t_j_max", 175, "°C", sheet=IRL, src="Absolute Maximum Ratings, operating junction "
       "temperature, maximum")
    ds("r_th_ja", 62, "°C/W", sheet=IRL, src="Thermal Resistance, junction-to-ambient, maximum")
    asm("t_amb", 40, "°C", src="the highest air temperature inside the machine, the 40 °C the IR "
        "design assumes for its sensors")

# -- 1N5819, Taiwan Semiconductor, Version H2104, the 1N5819 column -----------
ds("v_rrm", 40, "V", sheet=DIODE, src="Absolute Maximum Ratings, repetitive peak reverse voltage, "
   "1N5819 column", group="V_DRAIN", section=DRIVE, stated=True)
with MODEL.block("I_CH", section=DRIVE):
    ds("i_f_av", 1, "A", sheet=DIODE, src="Absolute Maximum Ratings, forward current")
    ds("i_fsm", 30, "A", sheet=DIODE, src="Absolute Maximum Ratings, surge peak forward current, "
       "8.3 ms single half sine wave")
ds("v_f_diode", 0.60, "V", sheet=DIODE, src="Electrical Specifications, forward voltage maximum at "
   "I_F = 1 A and T_J = 25 °C, 1N5819 column", group="V_DRAIN", section=DRIVE, stated=True)

# -- the stock machine's bulk at its power entry ------------------------------
msr("c_stock", 200, "µF", src="two radial electrolytics marked 100 10V VT in parallel at the stock "
    "power entry, read from their markings in research/Rokr/1_power-supply.md", group="C_BULK",
    section=SUPPLY, stated=True)
dec("n_stock_coils", 3, "", src="the three stock coils, which the machine energises together at "
    "power-on")

# -- derived: the drive channel -----------------------------------------------
fig("r_gate_total", "kΩ", "r_gate + r_gate_pd", moves="+r_gate +r_gate_pd", group="V_GATE",
    section=DRIVE, stated=False)

# Each resistor at the end of its tolerance that hurts the figure it enters.
fig("r_gate_lo", "kΩ", "r_gate * (1 - kit_tol)", moves="+r_gate -kit_tol", stated=False)
fig("r_gate_hi", "kΩ", "r_gate * (1 + kit_tol)", moves="+r_gate +kit_tol", stated=False)
fig("r_gate_pd_lo", "kΩ", "r_gate_pd * (1 - kit_tol)", moves="+r_gate_pd -kit_tol", stated=False)
fig("r_gate_pd_hi", "kΩ", "r_gate_pd * (1 + kit_tol)", moves="+r_gate_pd +kit_tol", stated=False)

with MODEL.block("V_GATE", section=DRIVE):
    # The gate while it is on, at its lowest: the rail at its low end, R_gate at its high end and
    # R_gate to ground at its low end.
    fig("v_gate", "V", "v_3v3_min * r_gate_pd_lo / (r_gate_hi + r_gate_pd_lo)", "down",
        moves="+v_3v3_min +r_gate_pd -r_gate -kit_tol")
    fig("v_gate_margin", "mV", "v_gate - v_gs_rdson", "down",
        moves="+v_3v3_min +r_gate_pd -r_gate -kit_tol -v_gs_rdson")

    # A reset during a pull-in leaves the keeper holding the pin high against the
    # two resistors to ground.
    fig("v_pin_keeper", "V",
        "v_3v3_max * (r_gate_hi + r_gate_pd_hi) / (r_keeper_min + r_gate_hi + r_gate_pd_hi)", "up",
        moves="+v_3v3_max +r_gate +r_gate_pd +kit_tol -r_keeper_min")
    fig("v_gate_keeper", "V",
        "v_3v3_max * r_gate_pd_hi / (r_keeper_min + r_gate_lo + r_gate_pd_hi)", "up",
        moves="+v_3v3_max +r_gate_pd -r_keeper_min -r_gate")
    fig("v_il", "V", "v_3v3_min * vil_frac", "down", moves="+v_3v3_min +vil_frac")

    # The most R_gate to ground whose high end still holds the gate under the lowest turn-on
    # threshold against the keeper, with R_gate at its low end.
    fig("r_gate_pd_max", "kΩ",
        "vgsth_min * (r_keeper_min + r_gate_lo) / ((v_3v3_max - vgsth_min) * (1 + kit_tol))",
        "down", moves="+vgsth_min +r_keeper_min +r_gate -v_3v3_max -kit_tol")
with MODEL.block("I_PIN", section=DRIVE):
    fig("i_pin_peak", "mA", "v_3v3_max / r_gate_lo", moves="+v_3v3_max +kit_tol -r_gate")
    fig("i_pin_hold", "µA", "v_3v3_max / (r_gate_lo + r_gate_pd_lo)",
        moves="+v_3v3_max +kit_tol -r_gate -r_gate_pd")

    # The least R_gate whose low end holds the pin inside what PJRC allows at the switching moment.
    fig("r_gate_min", "kΩ", "v_3v3_max / (i_pin_max * (1 - kit_tol))", "up",
        moves="+v_3v3_max +kit_tol -i_pin_max")
with MODEL.block(section=DRIVE):
    fig("v_kick", "V", "v_drain_off * c_rss / c_iss", moves="+v_5v +v_f_diode +c_rss -c_iss",
        group="V_KICK")
    fig("t_gate", "µs", "r_gate_hi * c_iss", moves="+r_gate +kit_tol +c_iss", group="I_PIN")
    fig("t_gate_decay", "µs", "r_gate_pd * c_iss", moves="+r_gate_pd +c_iss", group="V_KICK")
    fig("v_drain_off", "V", "v_5v + v_f_diode", moves="+v_5v +v_f_diode", group="V_DRAIN")
with MODEL.block("P_Q", section=DRIVE):
    fig("v_ds_on", "mV", "i_coil_scoop * rdson", moves="+i_coil_scoop +rdson")
    fig("p_mosfet", "mW", "i_coil_scoop * i_coil_scoop * rdson", moves="+i_coil_scoop +rdson")
    fig("t_j_q", "°C", "t_amb + p_mosfet * r_th_ja", "up",
        moves="+t_amb +i_coil_scoop +rdson +r_th_ja")

# The most that pulls at once in play: two bumpers off one ball, and the scoop ejecting
# the other ball at the same moment.
fig("i_coils_at_once", "A", "n_bumpers_at_once * i_coil + i_coil_scoop",
    moves="+n_bumpers_at_once +i_coil +i_coil_scoop", group="I_FEED", section=DRIVE)
fig("i_feed_max", "A", "n_stock_drive * i_coil + i_coil_scoop",
    moves="+i_coil +i_coil_scoop +n_stock_drive", group="I_FEED", section=DRIVE)

# -- derived: the sense channel -----------------------------------------------
with MODEL.block("V_SENSE", section=SENSE):
    fig("v_ih", "V", "v_3v3 * vih_frac", moves="+v_3v3 +vih_frac")
    fig("v_sense_high", "V", "v_3v3 * r_pulldown / (r_pulldown + r_contact + r_foil)",
        moves="+v_3v3 +r_pulldown -r_contact -r_foil")
    fig("v_sense_all", "V", "v_3v3 * r_pulldown / (r_pulldown + r_contact + n_sense * r_foil)",
        moves="+v_3v3 +r_pulldown -r_contact -r_foil -n_sense")
with MODEL.block("I_FOIL", section=SENSE):
    fig("i_sense", "mA", "v_3v3 / (r_pulldown + r_contact + r_foil)",
        moves="+v_3v3 -r_pulldown -r_contact -r_foil")
    fig("i_foil", "mA", "v_3v3 * n_sense / (n_sense * r_foil + r_contact + r_pulldown)",
        moves="+n_sense +v_3v3 -r_pulldown -r_contact -r_foil")
    fig("i_foil_short", "mA", "v_3v3 / r_foil", moves="+v_3v3 -r_foil")
    fig("p_foil_short", "mW", "v_3v3 * v_3v3 / r_foil", moves="+v_3v3 -r_foil")
with MODEL.block("R_S", section=SENSE):
    fig("v_pin_clamp", "V", "v_3v3 + v_clamp_over", moves="+v_3v3 +v_clamp_over")
    fig("i_pin_fault", "mA", "(v_5v - v_clamp_over) / r_series",
        moves="+v_5v -r_series -v_clamp_over")

fig("i_foil_fault", "mA", "(v_5v - v_clamp_over) / r_foil", moves="+v_5v -r_foil -v_clamp_over",
    group="I_FOIL", section=SENSE)
with MODEL.block("T_SENSE", section=SENSE):
    fig("t_open", "µs", "r_pulldown * c_filter", moves="+r_pulldown +c_filter")
    fig("t_close", "µs", "(r_contact + r_foil) * c_filter", moves="+r_contact +r_foil +c_filter")
    fig("t_contact", "ms", "ball_diameter / v_ball", moves="+ball_diameter -v_ball")

with MODEL.block("V_SENSE", section=SENSE):
    # The least R_pulldown whose low end keeps the pin above its high level with all three
    # contacts closed, R91 at its high end; the rail scales both sides and drops out.
    fig("r_pulldown_min", "kΩ", "vih_frac * (r_contact + n_sense * r_foil * (1 + kit_tol)) / ((1 - "
        "vih_frac) * (1 - kit_tol))", "up", moves="+vih_frac +r_contact +n_sense +r_foil +kit_tol")

    # The most R91 whose high end still does that, with R_pulldown at its low end.
    fig("r_foil_max", "kΩ", "(r_pulldown * (1 - kit_tol) * (1 - vih_frac) / vih_frac - r_contact) "
        "/ (n_sense * (1 + kit_tol))", "down",
        moves="+r_pulldown -vih_frac -r_contact -n_sense -kit_tol")

with MODEL.block(section=SENSE):
    # The least R91 whose low end keeps a foil held at ground, the rail at its high end, under
    # the share of the kit's rating every resistor is held to.
    fig("r_foil_min", "Ω", "v_3v3_max * v_3v3_max / (p_kit * p_derate * (1 - kit_tol))", "up",
        moves="+v_3v3_max +kit_tol -p_kit -p_derate", group="I_FOIL")

    # The least series resistor whose low end holds a sense wire on a coil wire under PJRC's
    # milliamp into an unpowered pin, with the rail where the adapter puts it with no load.
    fig("r_series_min", "kΩ", "(v_jack_max - v_clamp_over) / (i_pin_fault_max * (1 - kit_tol))",
        "up", moves="+v_jack_max +kit_tol -v_clamp_over -i_pin_fault_max", group="R_S")

    # The most R_pulldown whose high end still lets the node fall inside one contact.
    fig("r_pulldown_max", "kΩ", "t_contact / (c_filter * (1 + kit_tol))", "down",
        moves="+ball_diameter -v_ball -c_filter -kit_tol", group="T_SENSE")

    # The largest filter whose node still falls inside one contact, the ceiling on C_filter.
    fig("c_filter_max", "nF", "t_contact / r_pulldown", "down",
        moves="+ball_diameter -v_ball -r_pulldown", group="T_SENSE")

# -- derived: the coil --------------------------------------------------------
with MODEL.block("DUTY", section=COIL):
    fig("p_coil", "W", "v_5v * i_coil", moves="+v_5v +i_coil")

    # The driver tick ends a pull on the first tick after it has been on this long,
    # so a pull lasts from t_on_min up to t_on_max.
    fig("t_on_min", "ms", "t_on_max - t_tick", moves="+t_on_max -t_tick", stated=False)
    fig("t_on_actual", "ms", "t_on_max + t_late", moves="+t_on_max +t_late")
    fig("t_cycle_min", "ms", "t_on_actual + t_rearm", moves="+t_on_max +t_late +t_rearm")
    fig("duty_max", "%", "t_on_actual / t_cycle_min", moves="+t_on_max +t_late -t_rearm")
    fig("p_coil_avg", "W", "p_coil * duty_max", moves="+v_5v +i_coil +t_on_max +t_late -t_rearm")

# The worst case is a coil the driver tick never releases: the loop feeds the watchdog
# until the coil turns overdue, and the restart comes one timeout later.
fig("t_hold_fault", "s", "t_wdt + t_on_actual", "up", moves="+t_wdt +t_on_max +t_late",
    group="HOLD", section=COIL)

# -- derived: the supply ------------------------------------------------------
with MODEL.block("C_BULK", section=SUPPLY):
    # The Teensy's VIN hangs on the same rail, so the rise at switch-off stays under
    # what VIN takes.
    fig("dv_allowed", "V", "v_in_max - v_5v", moves="+v_in_max -v_5v")
    fig("i_stock", "A", "n_stock_coils * i_coil", moves="+n_stock_coils +i_coil")

    # At switch-off the feed's inductance drives its current on into the bulk: half
    # L I squared into half C dV squared, solved for C.
    def _c_bulk_min(l_feed, i_coil_scoop, dv_allowed, z_c91):
        room = dv_allowed - i_coil_scoop * z_c91   # what the part's impedance leaves the capacitance
        if room.v <= 0:                    # a rail already at VIN's limit leaves no rise to absorb
            return Q.of(inf, "µF")
        return l_feed * (i_coil_scoop / room) ** 2
    fig("c_bulk_min", "µF", _c_bulk_min, "up", moves="+l_feed +i_coil_scoop +v_5v +z_c91 -v_in_max")

    # The rise C91 lets through: the feed's current into its capacitance, plus the same
    # current across its impedance.
    fig("v_spike", "mV", "i_coil_scoop * (l_feed / c_c91) ** (1 / 2) + i_coil_scoop * z_c91",
        moves="+i_coil_scoop +l_feed +z_c91 -c_c91")
    fig("v_spike_bare", "V", "i_coil_scoop * (l_feed / c_stray) ** (1 / 2)",
        moves="+i_coil_scoop +l_feed -c_stray")
    fig("v_drain_spike", "V", "v_drain_off + v_spike",
        moves="+v_5v +v_f_diode +i_coil_scoop +l_feed +z_c91 -c_c91")
with MODEL.block("FAULT", section=SUPPLY):
    fig("i_coil_short", "A", "v_5v / (r_wire_short + rdson)", moves="+v_5v -r_wire_short -rdson")
    fig("p_q_short", "W", "i_coil_short * i_coil_short * rdson", moves="+v_5v +rdson -r_wire_short")

with MODEL.block("GND", section=SUPPLY):
    # The power ground reaches the distribution over J-COIL-PWR alone, so all four coils
    # at once lift it by their whole current across that one wire. The signal
    # ground and the Teensy carry none of it.
    fig("v_power_rise", "mV", "i_feed_max * r_lead", "up",
        moves="+i_coil +i_coil_scoop +n_stock_drive +r_lead")

    # The gate is driven from the Teensy's ground and measured against the power
    # ground, so the rise comes off the pin's level before the divider.
    fig("v_gate_shifted", "V",
        "(v_3v3_min - v_power_rise) * r_gate_pd_lo / (r_gate_hi + r_gate_pd_lo)", "down",
        moves="+v_3v3_min +r_gate_pd -r_gate -kit_tol -r_lead -i_coil")
    fig("v_gate_margin_shifted", "mV", "v_gate_shifted - v_gs_rdson", "down",
        moves="+v_3v3_min +r_gate_pd -r_gate -kit_tol -r_lead -v_gs_rdson")

    # The two ends of the gate divider's window: the most R_gate, and the least R_gate to ground,
    # that still leave the gate above the voltage R_DS(on) is read at, with the power
    # ground's rise against it and each at the end of its tolerance that hurts.
    fig("r_gate_max", "kΩ",
        "r_gate_pd_lo * ((v_3v3_min - v_power_rise) / v_gs_rdson - 1) / (1 + kit_tol)", "down",
        moves="+v_3v3_min +r_gate_pd -kit_tol -r_lead -i_coil -v_gs_rdson")
    fig("r_gate_pd_min", "kΩ",
        "r_gate_hi * v_gs_rdson / ((v_3v3_min - v_power_rise - v_gs_rdson) * (1 - kit_tol))", "up",
        moves="+r_gate +kit_tol +r_lead +i_coil +v_gs_rdson -v_3v3_min")

fig("v_sense_margin", "mV", "v_sense_all - v_ih",
    moves="+v_3v3 +r_pulldown -r_contact -r_foil -n_sense -vih_frac", group="GND", section=SUPPLY,
    stated=False)

# -- what the design requires, stated over the quantities rather than over any
# formula above
_I = MODEL.invariant

_I("the gate clears the 2.5 V of the typical curve the drop across the switch is read from",
   "v_gate > v_gs_rdson")
_I("and clears the worst-case turn-on threshold well before that", "v_gate > vgsth_max")
_I("the gate stays inside its rating", "v_gate < v_gs_max")
_I("the Teensy pin is not loaded past what PJRC allows, transient included",
   "i_pin_peak < i_pin_max")
_I("the drain step at power-up cannot turn the part on", "v_kick < vgsth_min")
_I("the clamped drain stays inside the MOSFET's drain-source rating", "v_drain_off < v_ds_max")
_I("the clamped drain stays inside the diode's reverse rating", "v_drain_off < v_rrm")
_I("the coil fits the diode, which is the tightest part of a channel, taken at the scoop's coil, "
   "the largest of the four", "i_coil_scoop < i_f_av")
_I("the channel ceiling the diode sets fits the MOSFET", "i_f_av < i_d_max")
_I("switch-off is far inside the diode's surge rating", "i_coil_scoop < i_fsm")
_I("the MOSFET's junction stays under its rating while the scoop's coil is energised",
   "t_j_q < t_j_max")
_I("after a reset during a pull-in the keeper, at its strongest, holds the gate under the lowest "
   "turn-on threshold", "v_gate_keeper < vgsth_min")
_I("and the pin then reads as a low", "v_pin_keeper < v_il")
_I("a closed contact clears the input's high level, one at a time", "v_sense_high > v_ih")
_I("and with all three closed at once, which shares the foil resistor", "v_sense_all > v_ih")
_I("a sense wire meeting a coil wire drives under a milliamp into the pin's clamp",
   "i_pin_fault < i_pin_fault_max")
_I("the foil resistor holds a foil at ground inside half its rating",
   "p_foil_short <= p_kit * p_derate")
_I("the bulk per ampere of solenoid stays above the stock machine's, taken at the one coil the "
   "firmware allows; the unarbitrated case is not one a capacitor rescues, since its demand "
   "already exceeds the supply", "c_c91 * i_stock > c_stock * i_coil")
_I("the bulk capacitor is bought at twice the rail it sits across", "v_c91_rating >= 2 * v_5v")
_I("the gate still clears the curve's 2.5 V, with the power ground's rise against it",
   "v_gate_shifted > v_gs_rdson")
_I("the drain stays inside its rating through the switch-off spike C91 leaves",
   "v_drain_spike < v_ds_max")
_I("C91 holds the rise at switch-off under what the Teensy's VIN takes", "c_c91 >= c_bulk_min")
_I("the foil fits the Teensy's 3V3 pin", "i_foil < i_teensy_3v3")
_I("the sense filter settles well inside how long a ball rests on a shell",
   "t_open < t_contact and t_close < t_open")
_I("the sense filter stays under the largest that still settles", "c_filter <= c_filter_max")
_I("the window a contact has to stay open outlasts the contact one hit makes",
   "t_rearm > t_contact")
_I("the overdue margin sits far inside the pull-in it is added to", "t_late < t_on_max")
_I("a coil the driver tick never releases is released by the restart inside the limit the "
   "requirements set", "t_hold_fault < t_hold_max")
_I("the gate settles far inside the shortest pull-in the driver can command", "t_gate < t_on_min")
_I("R11 to R14 sit inside their window", "r_gate_min <= r_gate <= r_gate_max")
_I("R21 to R24 sit inside their window", "r_gate_pd_min <= r_gate_pd <= r_gate_pd_max")
_I("R31 to R33 sit inside their window", "r_pulldown_min <= r_pulldown <= r_pulldown_max")
_I("R41 to R43 sit above their floor", "r_series_min <= r_series")
_I("R91 sits inside its window", "r_foil_min <= r_foil <= r_foil_max")

# the driver's constants, as docs/firmware/drivers/solenoid.md writes them
MODEL.as_written("t_on_max_us", of="t_on_max", unit="µs")
MODEL.as_written("t_rearm_us", of="t_rearm", unit="µs")
MODEL.as_written("t_late_us", of="t_late", unit="µs")
MODEL.as_written("t_contact_fail_ms", of="t_contact_fail", unit="ms")

# -- what the documents state that the model does not compute -----------------
for _text, _why in [
    ("2", "a coil connector's pin count, and the second pin of one"),
]:
    MODEL.aside(_text, _why)

# -- wiring -------------------------------------------------------------------
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
