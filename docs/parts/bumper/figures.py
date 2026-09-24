"""The figures of the bumper board, as inputs plus formulas.

Run with:  python tools/figcheck.py docs/parts/bumper/figures.py --sheets

Every input names where it comes from. Every derived quantity is a function
whose parameter names are its dependencies, so `--graph <key>` answers what a
value feeds and what it rests on.
"""
import pathlib

from figcheck import Model, Q

HERE = pathlib.Path(__file__).resolve().parent
MODEL = Model("bumper", HERE / "design.md", section=None, until="## Sources",
              documents=[HERE.parents[2] / "firmware" / "bumper.md"])

ds = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="datasheet", **kw)
dec = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="decision", **kw)
asm = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="assumed", **kw)
msr = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="measured", **kw)
fig = MODEL.derived

MOSFET = "AO3400A-AOS.pdf"
DIODE = "1n5817.pdf"
RT1062 = "IMXRT1060CEC.pdf"
LED = "WS2811-Worldsemi.pdf"
IRL = "IRL540N.PDF"

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
msr("i_coil", 0.68, "A", src="measured in series at the coil while running, and "
    "5.0 V / 7.35 Ω gives the same",
    group="I_CH", section=DRIVE, stated=True)
ds("i_coil_scoop", 0.8, "A", src="Adafruit 3992 product page, current draw at 5 V; "
   "the scoop's coil, larger than the three stock coils and the one that sets the "
   "per-channel worst case, since all four drive channels are built identical",
   group="I_CH", section=DRIVE, stated=True)
dec("n_stock_drive", 3, "", stated=False,
    src="the three drive channels wired to stock coils, the fourth to the scoop's")
msr("r_contact", 30, "Ω", src="the ball's path from foil to shell, measured at "
    "various points as 7 to 30 Ω; the upper end is the one that costs level",
    group="V_SENSE", section=SENSE, stated=True)

# ===========================================================================
# what the Teensy brings
# ===========================================================================
dec("v_3v3", 3.3, "V", src="the Teensy's 3.3 V rail, PJRC pin assignment card 11a rev4",
    group="V_GATE", section=DRIVE, stated=True)
dec("i_pin_max", 4, "mA", src="the recommended maximum output current per pin, "
    "Teensy 4.1 product page; the comparison table's 10 mA spans older generations",
    group="I_PIN", section=DRIVE, stated=True)
dec("i_3v3_max", 250, "mA", src="PJRC pin assignment card 11a rev4, the 3.3 V rail "
    "available to external circuits, the total across both 3V3 header pins",
    group="I_FOIL", section=SENSE, stated=True)

# ===========================================================================
# what this design chooses
# ===========================================================================
dec("n_drive", 4, "", stated=False,
    src="three bumper coils plus one spare solenoid channel without sense")
dec("n_sense", 3, "", stated=False, src="one sense line per bumper shell")
dec("r_gate", 2.2, "kΩ", src="chosen so the Teensy pin keeps a wide margin under its "
    "4 mA at the switching moment, which costs only gate settling time; it is also "
    "the value R41 to R43 carry, so the board holds one resistor value fewer",
    group="I_PIN", section=DRIVE, stated=True)
dec("r_gate_pd", 100, "kΩ", src="chosen well above R_gate so the divider costs the "
    "gate little, and bounded above only by the gate leakage it has to beat, which "
    "is a hundred nanoamperes", group="V_GATE", section=DRIVE, stated=True)
dec("r_pulldown", 10, "kΩ", src="chosen to hold the sense node at ground with no "
    "ball on the bumper, at a fraction of a milliamp when one is",
    group="V_SENSE", section=SENSE, stated=True)
dec("r_series", 10, "kΩ", src="chosen so a sense wire meeting a coil wire drives "
    "under a milliamp into the pin even with the Teensy unpowered, where the ceiling "
    "is 0.31 V rather than the rail plus that; the pin draws nothing, so the value "
    "costs no sense level", group="R_S", section=SENSE, stated=True)
dec("i_pin_fault_max", 1, "mA", src="PJRC calls under 1 mA into an out-of-range pin "
    "'very unlikely to cause harm', and states that even this does not follow NXP's "
    "guidance", group="R_S", section=SENSE, stated=True)
dec("r_foil", 330, "Ω", src="chosen to bound what the foil can draw from the Teensy's "
    "3.3 V rail if it reaches ground, at a value whose own fault dissipation stays "
    "well inside an 0805 and which still leaves level with all three contacts closed",
    group="V_SENSE", section=SENSE, stated=True)
dec("c_bulk", 100, "µF", src="chosen so the bulk per ampere of solenoid stays above "
    "what the stock machine runs on, which is the one working instance of these "
    "coils on this rail", group="C_BULK", section=SUPPLY, stated=True)
dec("v_cap_rating", 10, "V", src="the working voltage C91 is bought at, twice the "
    "rail it sits across", group="C_BULK", section=SUPPLY, stated=True)
dec("c_filter", 10, "nF", src="chosen with r_pulldown for a settling time far under "
    "how long a ball rests on a shell", group="T_SENSE", section=SENSE, stated=True)
dec("t_on_max", 50, "ms", src="the ceiling the driver enforces on one pull-in, "
    "decided above the stock machine's visibly short pull and far below the "
    "seconds at which a mini solenoid overheats; a scope reading of the stock "
    "pulse replaces it", group="DUTY", section=COIL, stated=True)
dec("t_tick", 1, "ms", src="the period of the driver's own timer, which is what ends "
    "a pull-in; chosen far under the pull-in and far over the interrupt it costs",
    group="DUTY", section=COIL, stated=True)
dec("t_rearm", 10, "ms", src="how long a channel's contact has to stay open before "
    "that channel may fire again; decided above the time the plunger takes to stroke "
    "and return, which no mass or spring figure lets us compute",
    group="DUTY", section=COIL, stated=True)

asm("l_feed", 0.3, "µH", src="the 5 V feed from the distribution, 30 cm of loose "
    "pair at roughly 1 µH per metre, estimated from the conductor and not measured",
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

asm("r_gnd_power", 50, "mΩ", src="the ground path over J-PWR and the distribution, "
    "estimated from a short thick conductor and not measured",
    group="GND", section=SUPPLY, stated=True)
asm("r_gnd_signal", 330, "mΩ", src="the ground path over J-T pin 9, estimated from a "
    "signal-cable conductor and not measured",
    group="GND", section=SUPPLY, stated=True)

# ===========================================================================
# the ball, shared with the break beam so both subsystems assume one speed
# ===========================================================================
asm("ball_diameter", 9, "mm", src="the steel ball the EG01 kit supplies, taken as 9 mm",
    group="T_SENSE", section=SENSE, stated=True)
dec("v_ball", 3, "m/s", src="the fastest ball this build assumes, "
    "docs/parts/ir-reflective/design.md", group="T_SENSE", section=SENSE, stated=True)

# ===========================================================================
# AO3400A, Alpha & Omega Semiconductor, Rev 3.1
# ===========================================================================
ds("v_ds_max", 30, "V", sheet=MOSFET, src="absolute maximum ratings, drain-source "
   "voltage", group="V_DRAIN", section=DRIVE, stated=True)
ds("v_gs_max", 12, "V", sheet=MOSFET, src="absolute maximum ratings, gate-source "
   "voltage, ±12 V", group="V_GATE", section=DRIVE, stated=True)
ds("i_d_max", 5.7, "A", sheet=MOSFET, src="absolute maximum ratings, continuous "
   "drain current at T_A = 25 °C", group="I_CH", section=DRIVE, stated=True)
ds("rdson", 48, "mΩ", sheet=MOSFET, src="static parameters, R_DS(on) maximum at "
   "V_GS = 2.5 V and I_D = 3 A, the lowest gate voltage the sheet specifies",
   group="P_Q", section=DRIVE, stated=True)
ds("v_gs_rdson", 2.5, "V", sheet=MOSFET, src="static parameters, the gate voltage "
   "the 48 mΩ row is specified at; clearing V_GS(th) means conducting, clearing "
   "this means conducting at a resistance the sheet guarantees",
   group="V_GATE", section=DRIVE, stated=True)
ds("vgsth_min", 0.65, "V", sheet=MOSFET, src="static parameters, gate threshold "
   "voltage minimum", group="V_KICK", section=DRIVE, stated=True)
ds("vgsth_max", 1.45, "V", sheet=MOSFET, src="static parameters, gate threshold "
   "voltage maximum", group="V_GATE", section=DRIVE, stated=True)
ds("c_iss", 630, "pF", sheet=MOSFET, src="dynamic parameters, input capacitance at "
   "V_GS = 0 V, V_DS = 15 V, f = 1 MHz", group="V_KICK", section=DRIVE, stated=True)
ds("c_rss", 50, "pF", sheet=MOSFET, src="dynamic parameters, reverse transfer "
   "capacitance at the same condition", group="V_KICK", section=DRIVE, stated=True)
# ---------------------------------------------------------------------------
# IRL540N, International Rectifier, the through-hole variant. The local copy is
# an image scan with no text layer, so --sheets cannot look these up and they
# are read from the rendered page instead.
# ---------------------------------------------------------------------------
ds("c_iss_th", 1800, "pF", sheet=IRL, src="Electrical Characteristics, input "
   "capacitance typical at V_GS = 0 V, V_DS = 25 V, f = 1.0 MHz",
   group="V_KICK", section=DRIVE, stated=True)
ds("c_rss_th", 170, "pF", sheet=IRL, src="the same table, reverse transfer "
   "capacitance", group="V_KICK", section=DRIVE, stated=True)
ds("vgsth_min_th", 1.0, "V", sheet=IRL, src="the same table, gate threshold voltage "
   "minimum at V_DS = V_GS, I_D = 250 µA",
   group="V_KICK", section=DRIVE, stated=True)

ds("p_d_mosfet", 1.4, "W", sheet=MOSFET, src="thermal characteristics, power "
   "dissipation at T_A = 25 °C", group="P_Q", section=DRIVE, stated=True)

# ===========================================================================
# 1N5819, STMicroelectronics, DocID 6262 Rev 5, the 1N5819 column
# ===========================================================================
ds("v_rrm", 40, "V", sheet=DIODE, src="Table 2 absolute ratings, repetitive peak "
   "reverse voltage, 1N5819 column", group="V_DRAIN", section=DRIVE, stated=True)
ds("i_f_av", 1, "A", sheet=DIODE, src="Table 2 absolute ratings, average forward "
   "current at T_L = 125 °C", group="I_CH", section=DRIVE, stated=True)
ds("i_fsm", 25, "A", sheet=DIODE, src="Table 2 absolute ratings, surge "
   "non-repetitive forward current, t_p = 10 ms sinusoidal",
   group="I_CH", section=DRIVE, stated=True)
ds("v_f_diode", 0.55, "V", sheet=DIODE, src="Table 4 static electrical "
   "characteristics, forward voltage drop at I_F = 1 A and T_j = 25 °C, "
   "1N5819 column", group="V_DRAIN", section=DRIVE, stated=True)


# ===========================================================================
# i.MX RT1062, NXP, IMXRT1060CEC Rev. 4
# ===========================================================================
ds("v_clamp_over", 0.31, "V", sheet=RT1062, src="Table 7, Vin/Vout maximum given "
   "as OVDD + 0.31 V, which is where the pin's protection diode takes over",
   group="R_S", section=SENSE, stated=True)
ds("vih_frac", 0.7, "", sheet=RT1062, stated=False,
   src="Table 10 GPIO DC parameters, high-level input voltage V_IH minimum, "
   "given as 0.7 x NVCC_XXXX")

# ===========================================================================
# what else hangs on the machine's 5 V rail
# ===========================================================================
ds("v_led_min", 4.5, "V", sheet=LED, src="supply voltage range, minimum; the "
   "WS2812B carries this same controller and is the tightest load on the rail",
   group="C_BULK", section=SUPPLY, stated=True)
ds("p_0805", 125, "mW", src="the 25 °C free-air rating of an 0805 thick-film resistor",
   group="I_FOIL", section=SENSE, stated=True)

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


@fig("v_gate", "V", group="V_GATE", section=DRIVE,
     rises_with=["v_3v3", "r_gate_pd"], falls_with=["r_gate"])
def _(v_3v3, r_gate_pd, r_gate):
    return v_3v3 * r_gate_pd / (r_gate + r_gate_pd)


@fig("v_gate_margin", "mV", group="V_GATE", section=DRIVE,
     rises_with=["v_3v3", "r_gate_pd"], falls_with=["r_gate", "v_gs_rdson"])
def _(v_gate, v_gs_rdson):
    return v_gate - v_gs_rdson


@fig("i_pin_peak", "mA", group="I_PIN", section=DRIVE,
     rises_with=["v_3v3"], falls_with=["r_gate"])
def _(v_3v3, r_gate):
    return v_3v3 / r_gate


@fig("i_pin_hold", "µA", group="I_PIN", section=DRIVE,
     rises_with=["v_3v3"], falls_with=["r_gate", "r_gate_pd"])
def _(v_3v3, r_gate, r_gate_pd):
    return v_3v3 / (r_gate + r_gate_pd)


@fig("v_kick", "V", group="V_KICK", section=DRIVE,
     rises_with=["v_5v", "v_f_diode", "c_rss"], falls_with=["c_iss"])
def _(v_drain_off, c_rss, c_iss):
    return v_drain_off * c_rss / c_iss


@fig("t_gate", "µs", group="I_PIN", section=DRIVE,
     rises_with=["r_gate", "c_iss"])
def _(r_gate, c_iss):
    return r_gate * c_iss


@fig("v_kick_th", "V", group="V_KICK", section=DRIVE,
     rises_with=["v_5v", "v_f_diode", "c_rss_th"], falls_with=["c_iss_th"])
def _(v_drain_off, c_rss_th, c_iss_th):
    return v_drain_off * c_rss_th / c_iss_th


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


# ===========================================================================
# derived: the coil
# ===========================================================================
@fig("p_coil", "W", group="DUTY", section=COIL,
     rises_with=["v_5v", "i_coil"])
def _(v_5v, i_coil):
    return v_5v * i_coil


@fig("t_on_actual", "ms", group="DUTY", section=COIL,
     rises_with=["t_on_max", "t_tick"])
def _(t_on_max, t_tick):
    return t_on_max + t_tick


@fig("t_cycle_min", "ms", group="DUTY", section=COIL,
     rises_with=["t_on_max", "t_tick", "t_rearm"])
def _(t_on_actual, t_rearm):
    return t_on_actual + t_rearm


@fig("duty_max", "%", group="DUTY", section=COIL,
     rises_with=["t_on_max", "t_tick"], falls_with=["t_rearm"])
def _(t_on_actual, t_cycle_min):
    return t_on_actual / t_cycle_min


@fig("p_coil_avg", "W", group="DUTY", section=COIL,
     rises_with=["v_5v", "i_coil", "t_on_max", "t_tick"], falls_with=["t_rearm"])
def _(p_coil, duty_max):
    return p_coil * duty_max


# ===========================================================================
# derived: the supply
# ===========================================================================
@fig("dv_allowed", "V", group="C_BULK", section=SUPPLY,
     rises_with=["v_5v"], falls_with=["v_led_min"])
def _(v_5v, v_led_min):
    return v_5v - v_led_min


@fig("i_stock", "A", group="C_BULK", section=SUPPLY,
     rises_with=["n_stock_coils", "i_coil"])
def _(n_stock_coils, i_coil):
    return n_stock_coils * i_coil


@fig("t_cover", "µs", group="C_BULK", section=SUPPLY,
     rises_with=["c_bulk", "v_5v"], falls_with=["v_led_min", "i_coil_scoop"])
def _(c_bulk, dv_allowed, i_coil_scoop):
    return c_bulk * dv_allowed / i_coil_scoop


@fig("v_spike", "mV", group="C_BULK", section=SUPPLY,
     rises_with=["i_coil_scoop", "l_feed"], falls_with=["c_bulk"])
def _(i_coil_scoop, l_feed, c_bulk):
    return i_coil_scoop * (l_feed / c_bulk) ** (1 / 2)


@fig("v_spike_bare", "V", group="C_BULK", section=SUPPLY,
     rises_with=["i_coil_scoop", "l_feed"], falls_with=["c_stray"])
def _(i_coil_scoop, l_feed, c_stray):
    return i_coil_scoop * (l_feed / c_stray) ** (1 / 2)


@fig("v_drain_spike", "V", group="C_BULK", section=SUPPLY,
     rises_with=["v_5v", "v_f_diode", "i_coil_scoop", "l_feed"], falls_with=["c_bulk"])
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


@fig("i_gnd_signal", "mA", group="GND", section=SUPPLY,
     rises_with=["i_coil_scoop", "r_gnd_power"], falls_with=["r_gnd_signal"])
def _(i_coil_scoop, r_gnd_power, r_gnd_signal):
    return i_coil_scoop * r_gnd_power / (r_gnd_power + r_gnd_signal)


@fig("v_gnd_shift", "mV", group="GND", section=SUPPLY,
     rises_with=["i_coil_scoop", "r_gnd_power", "r_gnd_signal"])
def _(i_gnd_signal, r_gnd_signal):
    return i_gnd_signal * r_gnd_signal


@fig("v_sense_margin", "mV", group="GND", section=SUPPLY,
     rises_with=["v_3v3", "r_pulldown"],
     falls_with=["r_contact", "r_foil", "n_sense", "vih_frac"])
def _(v_sense_all, v_ih):
    return v_sense_all - v_ih


# ===========================================================================
# what the design requires, stated over the quantities rather than over any
# formula above
# ===========================================================================
_I = MODEL.invariant

_I("the gate clears the voltage the sheet specifies R_DS(on) at, so the drop "
   "across the switch rests on a guaranteed maximum rather than on conduction",
   lambda v: v.v_gate > v.v_gs_rdson)
_I("and clears the worst-case turn-on threshold well before that",
   lambda v: v.v_gate > v.vgsth_max)
_I("the gate stays inside its rating",
   lambda v: v.v_gate < v.v_gs_max)
_I("the Teensy pin is not loaded past what PJRC allows, transient included",
   lambda v: v.i_pin_peak < v.i_pin_max)
_I("the drain step at power-up cannot turn the part on",
   lambda v: v.v_kick < v.vgsth_min)
_I("nor the through-hole variant, whose divider and threshold are its own",
   lambda v: v.v_kick_th < v.vgsth_min_th)
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
_I("the MOSFET stays inside its dissipation rating",
   lambda v: v.p_mosfet < v.p_d_mosfet)
_I("a closed contact clears the input's high level, one at a time",
   lambda v: v.v_sense_high > v.v_ih)
_I("and with all three closed at once, which shares the foil resistor",
   lambda v: v.v_sense_all > v.v_ih)
_I("a sense wire meeting a coil wire drives under a milliamp into the pin's clamp",
   lambda v: v.i_pin_fault < v.i_pin_fault_max)
_I("the foil resistor survives a foil held at ground inside an 0805",
   lambda v: v.p_foil_short < v.p_0805)
_I("the bulk per ampere of solenoid stays above the stock machine's, taken at the "
   "one coil the firmware allows; the unarbitrated case is not one a capacitor "
   "rescues, since its demand already exceeds the supply",
   lambda v: v.c_bulk * v.i_stock > v.c_stock * v.i_coil)
_I("the bulk capacitor is bought at twice the rail it sits across",
   lambda v: v.v_cap_rating >= 2 * v.v_5v)
_I("the ground loop shifts the sense reference by far less than the level holds",
   lambda v: v.v_gnd_shift < v.v_sense_margin)
_I("the drain stays inside its rating through the switch-off spike C91 leaves",
   lambda v: v.v_drain_spike < v.v_ds_max)
_I("the rail may not sag past what the tightest load on it accepts",
   lambda v: v.v_5v - v.dv_allowed >= v.v_led_min)
_I("the foil fits the Teensy's 3V3 pin",
   lambda v: v.i_foil < v.i_3v3_max)
_I("the sense filter settles well inside how long a ball rests on a shell",
   lambda v: v.t_open < v.t_contact and v.t_close < v.t_open)
_I("the window a contact has to stay open outlasts the contact one hit makes",
   lambda v: v.t_rearm > v.t_contact)
_I("the driver's timer ticks far inside the pull-in it has to end",
   lambda v: v.t_tick < v.t_on_max)
_I("the gate settles far inside the shortest pull-in the driver can command",
   lambda v: v.t_gate < v.t_on_max)


# ===========================================================================
# what the documents state that the model does not compute
# ===========================================================================
for _text, _why in [
    # quoted from a datasheet at a condition this design does not run
    ("3 A", "the drain current AOS specifies R_DS(on) at"),

    # measured on the stock machine, quoted rather than derived here
    ("7.35 Ω", "the coil's winding resistance, measured"),
    ("30 cm", "the length of the 5 V feed, from which its inductance is estimated"),
    ("7", "the lower end of the measured contact resistance range"),
    ("6 mm", "the plunger stroke, measured installed"),

    # Teensy pin numbers
    ("0", "the Teensy pin carrying trigger 4"),
    ("1", "the Teensy pin carrying sense 1"),
    ("14", "the Teensy pin carrying sense 2"),
    ("15", "the Teensy pin carrying sense 3"),
    ("32", "the Teensy pin carrying trigger 1"),
    ("34", "the Teensy pin carrying trigger 2"),
    ("35", "the Teensy pin carrying trigger 3"),

    # connector pin numbers and counts
    ("2", "a coil connector's pin count, and the second pin of one"),
    ("5", "a connector pin number"),
    ("6", "a connector pin number"),
    ("8", "a connector pin number"),
    ("9", "a connector pin number, and J-T's conductor count"),

    # a package, a pitch, a name rather than a value
    ("2.54 mm", "the connector pitch"),

    # constants as the driver writes them, without a unit
    ("50000", "the commanded pull-in in microseconds"),
    ("1000", "the driver's timer period in microseconds"),
    ("10000", "the open window a channel needs before it fires again, in microseconds"),
    ("250000", "the enforced pause in microseconds"),
    ("0805", "the resistor package, a name rather than a value"),
]:
    MODEL.aside(_text, _why)
