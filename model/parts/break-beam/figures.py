"""The figures of the ball drain break beam, as inputs plus formulas."""
from figcheck import *

MODEL = Model.at(__file__, "break-beam", drawings=["break-beam-schematic.svg"],
                 documents=["../../firmware/drivers/break-beam.md"])
fig, ds, gr, msr, asm, dec = MODEL.declarers()

EMITTER = "AM4457F3C-Kingbright.pdf"
RECEIVER = "TEKT5400S-Vishay.pdf"
AM = "Kingbright AM4457F3C, DSAQ0403 Rev. V.3A"
TK = "Vishay TEKT5400S, 81569 Rev. 1.6"
USB = "Universal Serial Bus Specification Revision 2.0"

EM = "Emitter"
RX = "Receiver"
BALL = "Ball"

# -- the emitter, behind R2 from the supply behind D1 -------------------------
with MODEL.block("EMITTER", section=EM):
    # VIN sits behind D1 in the machine and behind D2 on USB alone. The machine gives the
    # lower floor, and the adapter's ceiling bounds both.
    MODEL.uses("v_teensy_6a", of="power")
    MODEL.uses("v_teensy_usb", of="power", stated=False)
    # the adapter the machine's floor is taken at
    MODEL.uses("i_supply_6a", of="lighting", stated=False)
    MODEL.uses("v_jack_max", of="power")
    ds("v_usb_max", 5.25, "V", src=f"{USB}, Table 7-7: VBUS at a high-power port, maximum",
       stated=False)
    ds("vf_max", 1.6, "V", sheet=EMITTER, src=f"{AM}, page 2, Electrical / Optical Characteristics, "
       "forward voltage maximum at I_F = 20 mA")
    ds("i_f_test", 20, "mA", sheet=EMITTER, src=f"{AM}, pages 1 and 2, the forward current the "
       "radiant intensity and the forward voltage are given at", stated=False)
    dec("r_e", 220, "Ω", src="R2, from the supply behind D1 to the emitter's anode, the kit value "
        "that keeps the emitter inside its derating while the receiver keeps more than twice the "
        "current a LOW needs")
    dec("r_tol", 5, "%", src="the tolerance of R2 and R3, 5 % or better as the part list asks")
    MODEL.uses("t_a", of="ir-reflective")
_DERATING = f"{AM}, page 3, Forward Current Derating Curve"
gr("i_f_knee", 50, "mA", sheet=EMITTER, src=f"{_DERATING}, the permissible current up to its knee")
gr("t_knee", 25, "°C", sheet=EMITTER, src=f"{_DERATING}, the ambient at its knee")
gr("i_f_end", 10, "mA", sheet=EMITTER, src=f"{_DERATING}, the permissible current where the slope "
   "ends")
gr("t_end", 85, "°C", sheet=EMITTER, src=f"{_DERATING}, the ambient where the slope ends")
with MODEL.block("RATINGS", section=EM):
    ds("p_d_led", 90, "mW", sheet=EMITTER, src=f"{AM}, page 2, Absolute Maximum Ratings, power "
       "dissipation")
    dec("p_r_e_rating", 0.25, "W", src="R2's power rating, a 0.25 W part from the kit")
with MODEL.block("EMITTER", section=EM):
    fig("u_re_min", "V", "v_teensy_6a - vf_max", "down", moves="+v_teensy_6a -vf_max")
    fig("r_e_max", "Ω", "r_e * (1 + r_tol)", moves="+r_e +r_tol")
    fig("i_f_min", "mA", "u_re_min / r_e_max", "down", moves="+v_teensy_6a -vf_max -r_e -r_tol")
    fig("r_e_min", "Ω", "r_e * (1 - r_tol)", moves="+r_e -r_tol")

    # The sheet gives no least forward voltage, so the most current is taken with none left.
    fig("i_f_max", "mA", "v_jack_max / r_e_min", "up", moves="+v_jack_max +r_tol -r_e")
    fig("i_f_derated", "mA",
        "i_f_knee - (i_f_knee - i_f_end) * ((t_a - t_knee) / (t_end - t_knee))", "down",
        moves="+i_f_knee +i_f_end +t_knee +t_end -t_a")
with MODEL.block("RATINGS", section=EM):
    fig("p_r_e_max", "mW", "v_jack_max ** 2 / r_e_min", "up", moves="+v_jack_max +r_tol -r_e")

    # Whatever the emitter's forward voltage, it dissipates most where that voltage
    # takes half of VIN.
    fig("p_led_max", "mW", "(v_jack_max / 2) ** 2 / r_e_min", "up", moves="+v_jack_max +r_tol -r_e")

# -- the light reaching the receiver, a Vishay TEKT5400S ----------------------
with MODEL.block("LIT", section=RX):
    ds("ie_min", 2, "mW/sr", sheet=EMITTER, src=f"{AM}, page 1, Selection Guide, radiant intensity "
       "minimum at I_F = 20 mA, the value after CIE127-2007")
    msr("d_gap", 3.9, "cm", src="the gap between the two niches across the drain, measured on the "
        "stock machine")
    ds("ica_min", 2, "mA", sheet=RECEIVER, src=f"{TK}, Basic Characteristics, collector light "
       "current minimum at Ee = 1 mW/cm², λ = 950 nm, V_CE = 5 V")
    ds("ica_typ", 4, "mA", sheet=RECEIVER, src=f"{TK}, Basic Characteristics, the same row, "
       "typical")
    ds("ee_test", 1, "mW/cm²", sheet=RECEIVER, src=f"{TK}, Basic Characteristics, the irradiance "
       "the light current is given at", stated=False)
    ds("v_ce_test", 5, "V", sheet=RECEIVER, src=f"{TK}, Basic Characteristics, the "
       "collector-emitter voltage the light current is given at", stated=False)
    gr("ica_01", 0.21, "mA", sheet=RECEIVER, src=f"{TK}, Figure 4, the 0.1 mW/cm² curve read at "
       "V_CE = 1 V")
    gr("ica_02", 0.53, "mA", sheet=RECEIVER, src=f"{TK}, Figure 4, the 0.2 mW/cm² curve read at "
       "V_CE = 1 V")
    gr("ee_01", 0.1, "mW/cm²", sheet=RECEIVER, src=f"{TK}, Figure 4, the irradiance of its lowest "
       "curve", stated=False)
    gr("ee_02", 0.2, "mW/cm²", sheet=RECEIVER, src=f"{TK}, Figure 4, the irradiance of the curve "
       "above it", stated=False)
    dec("v_ce_read", 1, "V", src="the collector-emitter voltage Figure 4 is read at, on the "
        "curves' flat part next to the 1.00 V the pin reads LOW at", stated=False)
    ds("lam_p", 940, "nm", sheet=EMITTER, src=f"{AM}, page 2, wavelength of peak emission, typical")
    ds("lam_test", 950, "nm", sheet=RECEIVER, src=f"{TK}, Basic Characteristics, the wavelength "
       "the light current is given at")
    gr("s_940", 89, "%", sheet=RECEIVER, src=f"{TK}, Figure 7, relative spectral sensitivity at "
       "the emitter's peak")
    gr("s_950", 80, "%", sheet=RECEIVER, src=f"{TK}, Figure 7, relative spectral sensitivity at "
       "the wavelength the light current is given at")

    # The sheet's intensity against current is a straight line through the origin.
    fig("ie_least", "mW/sr", "ie_min * (i_f_min / i_f_test)", "down",
        moves="+ie_min +v_teensy_6a -i_f_test -vf_max -r_e -r_tol")
    fig("ee_least", "mW/cm²", "ie_least / d_gap ** 2", "down",
        moves="+ie_min +v_teensy_6a -d_gap -vf_max -r_e")

    # Figure 4's curves at 1 V, scaled to the least part by its table's least and
    # typical current, and carried below the lowest curve with the slope between the
    # two lowest curves.
    def _ica_least(ica_min, ica_typ, ica_01, ica_02, ee_01, ee_02, ee_least):
        slope = ln(ica_02 / ica_01) / ln(ee_02 / ee_01)
        return ica_min / ica_typ * ica_01 * exp(slope * ln(ee_least / ee_01))
    fig("ica_least", "mA", _ica_least, "down",
        moves="+ica_min +ica_01 +ee_02 +ie_min +v_teensy_6a -ica_typ -ica_02 -d_gap -vf_max -r_e")

# -- the pin reading LOW while the beam is clear ------------------------------
with MODEL.block("LOW", section=RX):
    MODEL.uses("v_3v3", of="teensy-4.1")
    MODEL.uses("v_3v3_acc", of="teensy-4.1")
    MODEL.uses("vil_frac", of="teensy-4.1", stated=False)
    MODEL.uses("vih_frac", of="teensy-4.1", stated=False)
    dec("r_pu", 100, "kΩ", src="R3, from the Teensy's 3V3 to the pin, the kit value that leaves "
        "the receiver's least current more than twice what a LOW needs")
    fig("v_3v3_hi", "V", "v_3v3 * (1 + v_3v3_acc)", moves="+v_3v3 +v_3v3_acc")
    fig("v_il_hi", "V", "v_3v3_hi * vil_frac", moves="+v_3v3 +v_3v3_acc +vil_frac")
    fig("u_low", "V", "v_3v3_hi - v_il_hi", "up", moves="+v_3v3 +v_3v3_acc -vil_frac")
    fig("r_pu_min", "kΩ", "r_pu * (1 - r_tol)", moves="+r_pu -r_tol")

    # what the least light current would drop across R3, were the receiver not to saturate
    fig("u_lit", "V", "ica_least * r_pu_min", "down", moves="+ica_min +r_pu -ica_typ -d_gap -r_tol")
    fig("margin_lit", "×", "u_lit / u_low", "down", moves="+ica_min +r_pu -d_gap -v_3v3")

# -- the pin reading HIGH while the ball darkens the receiver -----------------
with MODEL.block("BLOCKED", section=RX):
    ds("i_ceo_max", 100, "nA", sheet=RECEIVER, src=f"{TK}, Basic Characteristics, collector dark "
       "current maximum at V_CE = 20 V, E = 0")
    MODEL.uses("i_in_max", of="teensy-4.1")
    ds("r_l_sw", 100, "Ω", sheet=RECEIVER, src=f"{TK}, Basic Characteristics, the load resistor "
       "the turn-on and turn-off times are given at", stated=False)
    fig("r_pu_max", "kΩ", "r_pu * (1 + r_tol)", moves="+r_pu +r_tol")
    fig("u_dark", "V", "(i_ceo_max + i_in_max) * r_pu_max", "up",
        moves="+i_ceo_max +i_in_max +r_pu +r_tol")
    fig("v_3v3_lo", "V", "v_3v3 * (1 - v_3v3_acc)", moves="+v_3v3 -v_3v3_acc")
    fig("v_ih_lo", "V", "v_3v3_lo * vih_frac", moves="+v_3v3 +vih_frac -v_3v3_acc")
    fig("u_high", "V", "v_3v3_lo - v_ih_lo", "down", moves="+v_3v3 -v_3v3_acc -vih_frac")
    fig("i_shadow_max", "µA", "u_high / r_pu_max - i_in_max - i_ceo_max", "down",
        moves="+v_3v3 -r_pu -r_tol -i_in_max -i_ceo_max -vih_frac -v_3v3_acc")
    fig("i_r3_max", "µA", "v_3v3_hi / r_pu_min", "up", moves="+v_3v3 +v_3v3_acc +r_tol -r_pu")

# -- the ball -----------------------------------------------------------------
with MODEL.block("BALL", section=BALL):
    MODEL.uses("ball_diameter", of="ir-reflective")
    MODEL.uses("v_ball", of="ir-reflective")
    MODEL.uses("t_monitor", of="firmware", stated=False)
    dec("t_blocked_fail", 5, "s", stated=False, src="how long the beam may stay broken before the "
        "driver reports DeviceFailed, far longer than any passing ball keeps it broken")
    dec("t_dead", 10, "ms", stated=False, src="the driver takes edges inside this window as one "
        "crossing, decided: an estimate above the few milliseconds the ball's edge takes to cross "
        "the beam and far below the gap between two drains, to be replaced by a scope reading of "
        "the output during a crossing")
    fig("t_block", "ms", "ball_diameter / v_ball", moves="+ball_diameter -v_ball")

# -- what the design requires, stated over the quantities rather than over any
# formula above
_I = MODEL.invariant
_I("the forward voltage taken at 20 mA bounds it at the emitter's least current",
   "i_f_min <= i_f_test")
_I("VIN on USB alone tops out where the adapter does", "v_usb_max <= v_jack_max")
_I("the machine's floor lies under the floor on USB alone", "v_teensy_6a <= v_teensy_usb")
_I("the emitter's current stays inside its derating at the ambient the build assumes",
   "i_f_max <= i_f_derated")
_I("the emitter dissipates less than it allows", "p_led_max < p_d_led")
_I("R2 stays inside its rating with the whole supply across it", "p_r_e_max <= p_r_e_rating")
_I("the receiver is at least as sensitive at the emitter's peak as where its current is given",
   "s_940 >= s_950")
_I("the clear beam pulls the pin LOW with more than twice the current it needs",
   "margin_lit > Q.of(2, '')")
_I("the dark current and the pin's input current leave the pin HIGH", "u_dark < u_high")

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

# -- wiring -------------------------------------------------------------------
# the receiver pulls the line low while it is lit, and R3 holds it high otherwise
MODEL.net("Receiver output",
          drives("the receiver's collector", "open-drain",
                 src="design.md, the receiver pulls the pin to GND while it is lit"),
          pull("R3", TEENSY_RAIL, src="design.md, R3 from the Teensy's 3V3 to the pin"),
          teensy="Receiver output of the ball drain gate")

MODEL.draws("i_r3_max", pool=TEENSY_RAIL)   # R3, fed from the Teensy's 3V3
MODEL.draws(pool="device monitor")
