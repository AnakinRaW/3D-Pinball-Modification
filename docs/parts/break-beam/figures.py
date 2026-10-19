"""The figures of the ball drain break beam, as inputs plus formulas.

Run with:  python tools/figcheck.py docs/parts/break-beam/figures.py

Every input names where it comes from. Every derived quantity is a function
whose parameter names are its dependencies, so `--graph <key>` answers what a
value feeds and what it rests on.
"""
import pathlib

from figcheck import Model, Q, TEENSY_RAIL, drives, exp, ln, pull

HERE = pathlib.Path(__file__).resolve().parent
MODEL = Model("break-beam", HERE / "design.md",
              section=None, until="## Sources",
              drawings=[HERE / "break-beam-schematic.svg"],
              documents=[HERE.parents[1] / "firmware" / "drivers" / "break-beam.md"])

ds = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="datasheet", **kw)
gr = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="graph", **kw)
dec = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="decision", **kw)
msr = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="measured", **kw)
fig = MODEL.derived

EMITTER = "XTNI04W-SunLED.pdf"
RECEIVER = "TEKT5400S-Vishay.pdf"
XT = "SunLED XTNI04W, XDSA2670 V8-X"
TK = "Vishay TEKT5400S, 81569 Rev. 1.6"
USB = "Universal Serial Bus Specification Revision 2.0"

EM = "Emitter"
RX = "Receiver"
BALL = "Ball"

# ===========================================================================
# the emitter, behind R2 from the supply behind D1
# ===========================================================================
# VIN sits behind D1 in the machine and behind D2 on USB alone. The machine gives the
# lower floor, and the adapter's ceiling bounds both.
MODEL.uses("v_teensy_6a", of="power", group="EMITTER", section=EM, stated=True)
MODEL.uses("v_teensy_usb", of="power")
MODEL.uses("i_supply_6a", of="lighting")   # the adapter the machine's floor is taken at
MODEL.uses("v_jack_max", of="power", group="EMITTER", section=EM, stated=True)
ds("v_usb_max", 5.25, "V", src=f"{USB}, Table 7-7: VBUS at a high-power port, maximum")

ds("vf_max", 1.6, "V", sheet=EMITTER, src=f"{XT}, page 1, Operating Characteristics, "
   "forward voltage maximum at I_F = 20 mA", group="EMITTER", section=EM, stated=True)
ds("i_f_test", 20, "mA", sheet=EMITTER, src=f"{XT}, page 1, the forward current the forward "
   "voltage and the radiant intensity are given at")
dec("r_e", 220, "Ω", src="R2, from the supply behind D1 to the emitter's anode, the kit value that keeps the "
    "emitter inside its derating while the receiver keeps more than twice the current a LOW "
    "needs", group="EMITTER", section=EM, stated=True)
dec("r_tol", 5, "%", src="the tolerance of R2 and R3, 5 % or better as the part list asks",
    group="EMITTER", section=EM, stated=True)

MODEL.uses("t_a", of="ir-reflective", group="EMITTER", section=EM, stated=True)
_DERATING = f"{XT}, page 2, Forward Current Derating Curve"
gr("i_f_knee", 50, "mA", sheet=EMITTER, src=f"{_DERATING}, the permissible current up to "
   "its knee")
gr("t_knee", 25, "°C", sheet=EMITTER, src=f"{_DERATING}, the ambient at its knee")
gr("i_f_end", 10, "mA", sheet=EMITTER, src=f"{_DERATING}, the permissible current where the "
   "slope ends")
gr("t_end", 85, "°C", sheet=EMITTER, src=f"{_DERATING}, the ambient where the slope ends")

ds("p_d_led", 90, "mW", sheet=EMITTER, src=f"{XT}, page 1, Absolute Maximum Ratings, power "
   "dissipation", group="RATINGS", section=EM, stated=True)
dec("p_r_e_rating", 0.25, "W", src="R2's power rating, a 0.25 W part from the kit", group="RATINGS",
    section=EM, stated=True)


@fig("u_re_min", "V", group="EMITTER", section=EM, prints="down",
     rises_with=["v_teensy_6a"], falls_with=["vf_max"])
def _(v_teensy_6a, vf_max):
    return v_teensy_6a - vf_max


@fig("r_e_max", "Ω", group="EMITTER", section=EM, rises_with=["r_e", "r_tol"])
def _(r_e, r_tol):
    return r_e * (1 + r_tol)


@fig("i_f_min", "mA", group="EMITTER", section=EM, prints="down",
     rises_with=["v_teensy_6a"], falls_with=["vf_max", "r_e", "r_tol"])
def _(u_re_min, r_e_max):
    return u_re_min / r_e_max


@fig("r_e_min", "Ω", group="EMITTER", section=EM, rises_with=["r_e"], falls_with=["r_tol"])
def _(r_e, r_tol):
    return r_e * (1 - r_tol)


# The sheet gives no least forward voltage, so the most current is taken with none left.
@fig("i_f_max", "mA", group="EMITTER", section=EM, prints="up",
     rises_with=["v_jack_max", "r_tol"], falls_with=["r_e"])
def _(v_jack_max, r_e_min):
    return v_jack_max / r_e_min


@fig("i_f_derated", "mA", group="EMITTER", section=EM, prints="down",
     rises_with=["i_f_knee", "i_f_end", "t_knee", "t_end"], falls_with=["t_a"])
def _(i_f_knee, i_f_end, t_knee, t_end, t_a):
    return i_f_knee - (i_f_knee - i_f_end) * ((t_a - t_knee) / (t_end - t_knee))


@fig("p_r_e_max", "mW", group="RATINGS", section=EM, prints="up",
     rises_with=["v_jack_max", "r_tol"], falls_with=["r_e"])
def _(v_jack_max, r_e_min):
    return v_jack_max ** 2 / r_e_min


# Whatever the emitter's forward voltage, it dissipates most where that voltage
# takes half of VIN.
@fig("p_led_max", "mW", group="RATINGS", section=EM, prints="up",
     rises_with=["v_jack_max", "r_tol"], falls_with=["r_e"])
def _(v_jack_max, r_e_min):
    return (v_jack_max / 2) ** 2 / r_e_min


# ===========================================================================
# the light reaching the receiver, a Vishay TEKT5400S
# ===========================================================================
ds("ie_min", 2, "mW/sr", sheet=EMITTER, src=f"{XT}, page 1, radiant intensity minimum at "
   "I_F = 20 mA, the value after CIE127-2007", group="LIT", section=RX, stated=True)
msr("d_gap", 3.9, "cm", src="the gap between the two niches across the drain, measured on the "
    "stock machine", group="LIT", section=RX, stated=True)

ds("ica_min", 2, "mA", sheet=RECEIVER, src=f"{TK}, Basic Characteristics, collector light "
   "current minimum at Ee = 1 mW/cm², λ = 950 nm, V_CE = 5 V", group="LIT", section=RX,
   stated=True)
ds("ica_typ", 4, "mA", sheet=RECEIVER, src=f"{TK}, Basic Characteristics, the same row, "
   "typical", group="LIT", section=RX, stated=True)
ds("ee_test", 1, "mW/cm²", sheet=RECEIVER, src=f"{TK}, Basic Characteristics, the irradiance "
   "the light current is given at")
ds("v_ce_test", 5, "V", sheet=RECEIVER, src=f"{TK}, Basic Characteristics, the "
   "collector-emitter voltage the light current is given at")
gr("ica_01", 0.21, "mA", sheet=RECEIVER, src=f"{TK}, Figure 4, the 0.1 mW/cm² curve read at "
   "V_CE = 1 V", group="LIT", section=RX, stated=True)
gr("ica_02", 0.53, "mA", sheet=RECEIVER, src=f"{TK}, Figure 4, the 0.2 mW/cm² curve read at "
   "V_CE = 1 V", group="LIT", section=RX, stated=True)
gr("ee_01", 0.1, "mW/cm²", sheet=RECEIVER, src=f"{TK}, Figure 4, the irradiance of its "
   "lowest curve")
gr("ee_02", 0.2, "mW/cm²", sheet=RECEIVER, src=f"{TK}, Figure 4, the irradiance of the curve "
   "above it")
dec("v_ce_read", 1, "V", src="the collector-emitter voltage Figure 4 is read at, on the "
    "curves' flat part next to the 1.00 V the pin reads LOW at")
ds("lam_p", 940, "nm", sheet=EMITTER, src=f"{XT}, page 1, wavelength of peak emission, "
   "typical", group="LIT", section=RX, stated=True)
ds("lam_test", 950, "nm", sheet=RECEIVER, src=f"{TK}, Basic Characteristics, the wavelength "
   "the light current is given at", group="LIT", section=RX, stated=True)
gr("s_940", 89, "%", sheet=RECEIVER, src=f"{TK}, Figure 7, relative spectral sensitivity at "
   "the emitter's peak", group="LIT", section=RX, stated=True)
gr("s_950", 80, "%", sheet=RECEIVER, src=f"{TK}, Figure 7, relative spectral sensitivity at "
   "the wavelength the light current is given at", group="LIT", section=RX, stated=True)


# The sheet's intensity against current is a straight line through the origin.
@fig("ie_least", "mW/sr", group="LIT", section=RX, prints="down",
     rises_with=["ie_min", "v_teensy_6a"], falls_with=["i_f_test", "vf_max", "r_e", "r_tol"])
def _(ie_min, i_f_min, i_f_test):
    return ie_min * (i_f_min / i_f_test)


@fig("ee_least", "mW/cm²", group="LIT", section=RX, prints="down",
     rises_with=["ie_min", "v_teensy_6a"], falls_with=["d_gap", "vf_max", "r_e"])
def _(ie_least, d_gap):
    return ie_least / d_gap ** 2


# Figure 4's curves at 1 V, scaled to the least part by its table's least and
# typical current, and carried below the lowest curve with the slope between the
# two lowest curves.
@fig("ica_least", "mA", group="LIT", section=RX, prints="down",
     rises_with=["ica_min", "ica_01", "ee_02", "ie_min", "v_teensy_6a"],
     falls_with=["ica_typ", "ica_02", "d_gap", "vf_max", "r_e"])
def _(ica_min, ica_typ, ica_01, ica_02, ee_01, ee_02, ee_least):
    slope = ln(ica_02 / ica_01) / ln(ee_02 / ee_01)
    return ica_min / ica_typ * ica_01 * exp(slope * ln(ee_least / ee_01))


# ===========================================================================
# the pin reading LOW while the beam is clear
# ===========================================================================
MODEL.uses("v_3v3", of="teensy-4.1", group="LOW", section=RX, stated=True)
MODEL.uses("v_3v3_acc", of="teensy-4.1", group="LOW", section=RX, stated=True)
MODEL.uses("vil_frac", of="teensy-4.1")
MODEL.uses("vih_frac", of="teensy-4.1")
dec("r_pu", 100, "kΩ", src="R3, from the Teensy's 3V3 to the pin, the kit value that leaves "
    "the receiver's least current more than twice what a LOW needs", group="LOW", section=RX,
    stated=True)


@fig("v_3v3_hi", "V", group="LOW", section=RX, rises_with=["v_3v3", "v_3v3_acc"])
def _(v_3v3, v_3v3_acc):
    return v_3v3 * (1 + v_3v3_acc)


@fig("v_il_hi", "V", group="LOW", section=RX, rises_with=["v_3v3", "v_3v3_acc", "vil_frac"])
def _(v_3v3_hi, vil_frac):
    return v_3v3_hi * vil_frac


@fig("u_low", "V", group="LOW", section=RX, prints="up",
     rises_with=["v_3v3", "v_3v3_acc"], falls_with=["vil_frac"])
def _(v_3v3_hi, v_il_hi):
    return v_3v3_hi - v_il_hi


@fig("r_pu_min", "kΩ", group="LOW", section=RX, rises_with=["r_pu"], falls_with=["r_tol"])
def _(r_pu, r_tol):
    return r_pu * (1 - r_tol)


# what the least light current would drop across R3, were the receiver not to saturate
@fig("u_lit", "V", group="LOW", section=RX, prints="down",
     rises_with=["ica_min", "r_pu"], falls_with=["ica_typ", "d_gap", "r_tol"])
def _(ica_least, r_pu_min):
    return ica_least * r_pu_min


@fig("margin_lit", "×", group="LOW", section=RX, prints="down",
     rises_with=["ica_min", "r_pu"], falls_with=["d_gap", "v_3v3"])
def _(u_lit, u_low):
    return u_lit / u_low


# ===========================================================================
# the pin reading HIGH while the ball darkens the receiver
# ===========================================================================
ds("i_ceo_max", 100, "nA", sheet=RECEIVER, src=f"{TK}, Basic Characteristics, collector dark "
   "current maximum at V_CE = 20 V, E = 0", group="BLOCKED", section=RX, stated=True)
MODEL.uses("i_in_max", of="teensy-4.1", group="BLOCKED", section=RX, stated=True)
ds("r_l_sw", 100, "Ω", sheet=RECEIVER, src=f"{TK}, Basic Characteristics, the load resistor "
   "the turn-on and turn-off times are given at")


@fig("r_pu_max", "kΩ", group="BLOCKED", section=RX, rises_with=["r_pu", "r_tol"])
def _(r_pu, r_tol):
    return r_pu * (1 + r_tol)


@fig("u_dark", "V", group="BLOCKED", section=RX, prints="up",
     rises_with=["i_ceo_max", "i_in_max", "r_pu", "r_tol"])
def _(i_ceo_max, i_in_max, r_pu_max):
    return (i_ceo_max + i_in_max) * r_pu_max


@fig("v_3v3_lo", "V", group="BLOCKED", section=RX, rises_with=["v_3v3"],
     falls_with=["v_3v3_acc"])
def _(v_3v3, v_3v3_acc):
    return v_3v3 * (1 - v_3v3_acc)


@fig("v_ih_lo", "V", group="BLOCKED", section=RX, rises_with=["v_3v3", "vih_frac"],
     falls_with=["v_3v3_acc"])
def _(v_3v3_lo, vih_frac):
    return v_3v3_lo * vih_frac


@fig("u_high", "V", group="BLOCKED", section=RX, prints="down", rises_with=["v_3v3"],
     falls_with=["v_3v3_acc", "vih_frac"])
def _(v_3v3_lo, v_ih_lo):
    return v_3v3_lo - v_ih_lo


@fig("i_shadow_max", "µA", group="BLOCKED", section=RX, prints="down",
     rises_with=["v_3v3"],
     falls_with=["r_pu", "r_tol", "i_in_max", "i_ceo_max", "vih_frac", "v_3v3_acc"])
def _(u_high, r_pu_max, i_in_max, i_ceo_max):
    return u_high / r_pu_max - i_in_max - i_ceo_max


@fig("i_r3_max", "µA", group="BLOCKED", section=RX, prints="up",
     rises_with=["v_3v3", "v_3v3_acc", "r_tol"], falls_with=["r_pu"])
def _(v_3v3_hi, r_pu_min):
    return v_3v3_hi / r_pu_min


# ===========================================================================
# the ball
# ===========================================================================
MODEL.uses("ball_diameter", of="ir-reflective", group="BALL", section=BALL, stated=True)
MODEL.uses("v_ball", of="ir-reflective", group="BALL", section=BALL, stated=True)
MODEL.uses("t_monitor", of="firmware")
dec("t_blocked_fail", 5, "s", stated=False, src="how long the beam may stay broken before the "
    "driver reports DeviceFailed, far longer than any passing ball keeps it broken")
dec("t_dead", 10, "ms", stated=False,
    src="the driver takes edges inside this window as one crossing, decided: an "
    "estimate above the few milliseconds the ball's edge takes to cross the beam "
    "and far below the gap between two drains, to be replaced by a scope reading "
    "of the output during a crossing")


@fig("t_block", "ms", group="BALL", section=BALL,
     rises_with=["ball_diameter"], falls_with=["v_ball"])
def _(ball_diameter, v_ball):
    return ball_diameter / v_ball


# ===========================================================================
# what the design requires, stated over the quantities rather than over any
# formula above
# ===========================================================================
_I = MODEL.invariant
_I("the forward voltage taken at 20 mA bounds it at the emitter's least current",
   lambda v: v.i_f_min <= v.i_f_test)
_I("VIN on USB alone tops out where the adapter does",
   lambda v: v.v_usb_max <= v.v_jack_max)
_I("the machine's floor lies under the floor on USB alone",
   lambda v: v.v_teensy_6a <= v.v_teensy_usb)
_I("the emitter's current stays inside its derating at the ambient the build assumes",
   lambda v: v.i_f_max <= v.i_f_derated)
_I("the emitter dissipates less than it allows",
   lambda v: v.p_led_max < v.p_d_led)
_I("R2 stays inside its rating with the whole supply across it",
   lambda v: v.p_r_e_max <= v.p_r_e_rating)
_I("the receiver is at least as sensitive at the emitter's peak as where its current is given",
   lambda v: v.s_940 >= v.s_950)
_I("the clear beam pulls the pin LOW with more than twice the current it needs",
   lambda v: v.margin_lit > Q.of(2, ""))
_I("the dark current and the pin's input current leave the pin HIGH",
   lambda v: v.u_dark < v.u_high)

MODEL.curve("Forward Current Derating Curve",
            [("t_knee", "i_f_knee"), ("t_end", "i_f_end")],
            rises=False,
            on=[("t_a", "i_f_derated")])
MODEL.curve("Figure 4 at 1 V, light current against irradiance",
            [("ee_01", "ica_01"), ("ee_02", "ica_02")],
            rises=True)
MODEL.curve("Figure 7, relative spectral sensitivity past its peak",
            [("lam_p", "s_940"), ("lam_test", "s_950")],
            rises=False)


# ===========================================================================
# wiring
# ===========================================================================
# the receiver pulls the line low while it is lit, and R3 holds it high otherwise
MODEL.net("Receiver output",
          drives("the receiver's collector", "open-drain",
                 src="design.md, the receiver pulls the pin to GND while it is lit"),
          pull("R3", TEENSY_RAIL, src="design.md, R3 from the Teensy's 3V3 to the pin"),
          teensy="Receiver output of the ball drain gate")

MODEL.draws("i_r3_max", pool=TEENSY_RAIL)   # R3, fed from the Teensy's 3V3
MODEL.draws(pool="device monitor")
