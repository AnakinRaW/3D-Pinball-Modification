"""The figures of the servo, its buffer and its supply, as inputs plus formulas.

Run with:  python tools/figcheck.py docs/parts/servo/figures.py

Every input names where it comes from. Every derived quantity is a function
whose parameter names are its dependencies, so `--graph <key>` answers what a
value feeds and what it rests on.
"""
import pathlib

from figcheck import Model, drives, ln, pull, reads

HERE = pathlib.Path(__file__).resolve().parent
MODEL = Model("servo", HERE / "design.md", section=None, until="## Sources",
              drawings=[HERE / "servo-schematic.svg"],
              documents=[HERE.parents[2] / "firmware" / "drivers" / "servo.md"])

ds = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="datasheet", **kw)
msr = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="measured", **kw)
dec = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="decision", **kw)
asm = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="assumed", **kw)
fig = MODEL.derived

SERVO = "SER0049-DFRobot.pdf"
BUFFER = "TI CD74HCT125 datasheet, SCHS415A"

PULSE = "The pulse"
LEVELS = "The signal levels"
OUTPUT = "The output"
SUPPLY = "The supply"

# ===========================================================================
# the pulse
# ===========================================================================
dec("t_frame", 20, "ms", src="the frame a servo expects its pulse in: REFRESH_INTERVAL, "
    "20000 µs, in the Arduino Servo library the SER0049 sheet points to, and the 50 Hz "
    "PJRC's PWMServo sets with analogWriteFrequency()",
    group="PULSE", section=PULSE, stated=True)
ds("t_min", 500, "µs", sheet=SERVO, src="Pulse Width Range, 500~2500 µs, the short end, "
   "one end of the travel", group="PULSE", section=PULSE, stated=True)
ds("t_max", 2500, "µs", sheet=SERVO, src="Pulse Width Range, the long end, the other end "
   "of the travel", group="PULSE", section=PULSE, stated=True)
dec("n_duty", 4096, "steps", src="PWMServo's write() sets the pulse through analogWrite() "
    "at 12-bit resolution, 4096 steps to the frame, PJRC's PWMServo.cpp",
    group="PULSE", section=PULSE, stated=True)
ds("t_min_default", 544, "µs", stated=False, src="PWMServo.h, the pulse for 0 degrees "
   "attach(pin) takes when no range is given")
ds("t_max_default", 2400, "µs", stated=False, src="PWMServo.h, the pulse for 180 degrees "
   "attach(pin) takes when no range is given")


@fig("f_frame", "Hz", group="PULSE", section=PULSE, falls_with=["t_frame"])
def _(t_frame):
    return 1 / t_frame


@fig("t_step", "µs", group="PULSE", section=PULSE,
     rises_with=["t_frame"], falls_with=["n_duty"])
def _(t_frame, n_duty):
    return t_frame / n_duty


# How finely PWMServo sets the pulse across the servo's whole travel.
@fig("n_travel", "steps", group="PULSE", section=PULSE,
     rises_with=["t_max", "n_duty"], falls_with=["t_min", "t_frame"])
def _(t_max, t_min, t_step):
    return (t_max - t_min) / t_step


# ===========================================================================
# the signal levels: the Teensy's pin through R3 into U1
# ===========================================================================
MODEL.uses("v_3v3", of="teensy-4.1", group="LEVELS", section=LEVELS, stated=True)
MODEL.uses("v_oh_drop", of="teensy-4.1")
MODEL.uses("v_ol_max", of="teensy-4.1", group="LEVELS", section=LEVELS, stated=True)
MODEL.uses("i_oh_test", of="teensy-4.1")
MODEL.uses("r_keeper_min", of="teensy-4.1", group="LEVELS", section=LEVELS, stated=True)
dec("r_pd", 10, "kΩ", src="R1, from the pin to ground, chosen to hold U1's input under its "
    "low level against the pin's keeper after a reset, at a third of a milliamp while the "
    "pin drives high", group="LEVELS", section=LEVELS, stated=True)
ds("vih_u1", 2, "V", src=f"{BUFFER}, 5.3 Recommended Operating Conditions, high-level input "
   "voltage VIH minimum at VCC = 4.5 V to 5.5 V", group="LEVELS", section=LEVELS, stated=True)
ds("vil_u1", 0.8, "V", src=f"{BUFFER}, 5.3 Recommended Operating Conditions, low-level input "
   "voltage VIL maximum at VCC = 4.5 V to 5.5 V", group="LEVELS", section=LEVELS, stated=True)
ds("ii_u1", 1, "µA", src=f"{BUFFER}, 5.5 Electrical Characteristics, input leakage current II "
   "maximum at VI = VCC or GND and VCC = 5.5 V, over the whole temperature range",
   group="LEVELS", section=LEVELS, stated=True)
dec("r_in", 2.2, "kΩ", src="R3, from the pin to U1's input, chosen so the pin's current into "
    "U1's clamp diode stays far under its 4 mA while the machine's 5 V is off",
    group="LEVELS", section=LEVELS, stated=True)
MODEL.uses("i_pin_max", of="teensy-4.1")
MODEL.uses("v_clamp_over", of="teensy-4.1", group="LEVELS", section=LEVELS, stated=True)
MODEL.uses("r_lead", of="solenoid", group="LEVELS", section=LEVELS, stated=True)
ds("iik_u1", 20, "mA", src=f"{BUFFER}, 5.1 Absolute Maximum Ratings, input clamp current IIK "
   "at VI > VCC + 0.5 V; 7.3.3 places a clamp diode from each input to VCC",
   group="OFF", section=LEVELS, stated=True)
ds("c_in_u1", 10, "pF", src=f"{BUFFER}, 5.5 Electrical Characteristics, input capacitance Ci "
   "maximum", group="EDGE", section=LEVELS, stated=True)
asm("c_stray", 15, "pF", src="the wiring from R3 to U1's input, estimated generously and not "
    "measured", group="EDGE", section=LEVELS, stated=True)
ds("t_tran_max", 400, "ns", src=f"{BUFFER}, 5.3 Recommended Operating Conditions, input "
   "transition time tt maximum at VCC = 5.5 V, the shorter of its two figures",
   group="EDGE", section=LEVELS, stated=True)
asm("edge_low", 10, "%", stated=False, src="the lower mark TI's logic sheets time an edge "
    "from; SCHS415A draws its marks in a figure the text search cannot read")
asm("edge_high", 90, "%", stated=False, src="the upper mark, taken as edge_low is")


@fig("v_oh_min", "V", group="LEVELS", section=LEVELS, prints="down",
     rises_with=["v_3v3"], falls_with=["v_oh_drop"])
def _(v_3v3, v_oh_drop):
    return v_3v3 - v_oh_drop


# U1's input current runs through R3 in either direction, so U1's input sits
# that far off the pin's level, against whichever margin it eats into.
@fig("v_r_in", "mV", group="LEVELS", section=LEVELS, prints="up",
     rises_with=["ii_u1", "r_in"])
def _(ii_u1, r_in):
    return ii_u1 * r_in


@fig("m_high", "V", group="LEVELS", section=LEVELS, prints="down",
     rises_with=["v_3v3"], falls_with=["v_oh_drop", "vih_u1", "ii_u1", "r_in"])
def _(v_oh_min, v_r_in, vih_u1):
    return v_oh_min - v_r_in - vih_u1


@fig("m_low", "V", group="LEVELS", section=LEVELS, prints="down",
     rises_with=["vil_u1"], falls_with=["v_ol_max", "ii_u1", "r_in"])
def _(vil_u1, v_ol_max, v_r_in):
    return vil_u1 - v_ol_max - v_r_in


@fig("i_pd", "mA", group="LEVELS", section=LEVELS, prints="up",
     rises_with=["v_3v3"], falls_with=["r_pd"])
def _(v_3v3, r_pd):
    return v_3v3 / r_pd


# After a reset the keeper holds the level the pin last drove, a high at worst,
# and R1 pulls against it at the pin, so the pin sits at the divider between
# the two. U1's input current adds its drop across the two in parallel and
# across R3.
@fig("v_keeper", "V", group="LEVELS", section=LEVELS, prints="up",
     rises_with=["v_3v3", "r_pd", "ii_u1", "r_in"], falls_with=["r_keeper_min"])
def _(v_3v3, r_pd, r_keeper_min, ii_u1, r_in):
    return (v_3v3 * r_pd / (r_pd + r_keeper_min)
            + ii_u1 * (r_pd * r_keeper_min / (r_pd + r_keeper_min) + r_in))


# With the 5 V on and the Teensy unpowered nothing drives the pin: U1's input
# current is all that reaches its net, and R1 carries it to ground.
@fig("v_pin_off", "mV", group="LEVELS", section=LEVELS, prints="up",
     rises_with=["ii_u1", "r_pd"])
def _(ii_u1, r_pd):
    return ii_u1 * r_pd


# With the machine's 5 V off, U1's clamp diode holds its input at the dead
# rail. The pin's high then drives R3 into that diode, both taken at 0 V, and
# R1 beside it.
@fig("i_clamp", "mA", group="OFF", section=LEVELS, prints="up",
     rises_with=["v_3v3"], falls_with=["r_in"])
def _(v_3v3, r_in):
    return v_3v3 / r_in


@fig("i_pin_off5", "mA", group="OFF", section=LEVELS, prints="up",
     rises_with=["v_3v3"], falls_with=["r_in", "r_pd"])
def _(i_clamp, i_pd):
    return i_clamp + i_pd


# R3 into U1's input and the wiring beside it is a single pole, whose step
# crosses the two marks ln((1 - low) / (1 - high)) time constants apart.
@fig("t_edge_in", "ns", group="EDGE", section=LEVELS, prints="up",
     rises_with=["r_in", "c_in_u1", "c_stray", "edge_high"], falls_with=["edge_low"])
def _(r_in, c_in_u1, c_stray, edge_low, edge_high):
    return ln((1 - edge_low.raw) / (1 - edge_high.raw)) * r_in * (c_in_u1 + c_stray)


# U1 takes its ground at J-PWR, so the servo's stall current through that one
# wire lifts U1's ground over the distribution, and the Teensy's high arrives
# that much lower.
@fig("v_u1_rise", "mV", group="LEVELS", section=LEVELS, prints="up",
     rises_with=["i_stall", "r_lead"])
def _(i_stall, r_lead):
    return i_stall * r_lead


# ===========================================================================
# the output: U1 into the servo's signal wire
# ===========================================================================
ds("vcc_u1_min", 4.5, "V", src=f"{BUFFER}, Recommended Operating Conditions, supply "
   "voltage VCC minimum", group="OUTPUT", section=OUTPUT, stated=True)
ds("vcc_u1_max", 5.5, "V", src=f"{BUFFER}, Recommended Operating Conditions, supply "
   "voltage VCC maximum; the highest rail U1 is specified on, and the one a fault on the "
   "signal wire is taken at", group="OUTPUT", section=OUTPUT, stated=True)
ds("v_oh_u1", 4.4, "V", src=f"{BUFFER}, 5.5 Electrical Characteristics, VOH minimum at "
   "IOH = -20 µA and VCC = 4.5 V", group="OUTPUT", section=OUTPUT, stated=True)
ds("i_oh_light", 20, "µA", stated=False, src=f"{BUFFER}, 5.5 Electrical Characteristics, the "
   "light-load current the 4.4 V VOH is specified at")
dec("r_series", 1, "kΩ", src="R2, chosen so a signal wire that meets 5 V or ground holds "
    "U1's output inside its continuous output current",
    group="OUTPUT", section=OUTPUT, stated=True)
ds("i_out_u1", 35, "mA", src=f"{BUFFER}, 5.1 Absolute Maximum Ratings, continuous output "
   "current IO; 5.3 states no output current", group="OUTPUT", section=OUTPUT, stated=True)


@fig("i_fault", "mA", group="OUTPUT", section=OUTPUT, prints="up",
     rises_with=["vcc_u1_max"], falls_with=["r_series"])
def _(vcc_u1_max, r_series):
    return vcc_u1_max / r_series


# ===========================================================================
# the supply
# ===========================================================================
MODEL.uses("v_5v", of="solenoid", group="SUPPLY", section=SUPPLY, stated=True)
ds("v_servo_min", 4.8, "V", sheet=SERVO, src="Operating Voltage, 4.8-6 V DC, minimum",
   group="SUPPLY", section=SUPPLY, stated=True)
ds("v_servo_max", 6, "V", sheet=SERVO, src="Operating Voltage, maximum",
   group="SUPPLY", section=SUPPLY, stated=True)
ds("i_rest", 8, "mA", sheet=SERVO, src="SPECIFICATION list, static current at most 8 mA "
   "at 6.0 V, which the selection guide's 9 g 180° column repeats as its quiescent current",
   group="SUPPLY", section=SUPPLY, stated=True)
ds("i_free", 120, "mA", sheet=SERVO, src="SPECIFICATION list, no-load current at most "
   "120 mA at 6.0 V; the selection guide's 9 g 180° column gives 60 mA, and the larger "
   "bounds both", group="SUPPLY", section=SUPPLY, stated=True)
ds("i_stall_table", 650, "mA", sheet=SERVO, src="Selection Guide for Clutch Servo, the "
   "9 g 180° column, SER0049, stall current at most 650 mA at 6.0 V",
   group="SUPPLY", section=SUPPLY, stated=True)
ds("i_stall", 800, "mA", sheet=SERVO, src="SPECIFICATION list, stall current at most "
   "800 mA at 6.0 V, the larger of the sheet's two figures and the one the design takes",
   group="SUPPLY", section=SUPPLY, stated=True)
ds("t_block", 5, "s", sheet=SERVO, stated=False, src="Electronic Protection: after being "
   "blocked for 5 seconds the servo turns off its power")
MODEL.uses("l_feed", of="solenoid", group="SUPPLY", section=SUPPLY, stated=True)
dec("c_bulk", 100, "µF", src="C1, the 100 µF per servo that Adafruit's PCA9685 guide "
    "starts from", group="SUPPLY", section=SUPPLY, stated=True)
dec("v_cap_rating", 10, "V", src="the working voltage C1 is bought at, twice the rail it "
    "sits across", group="SUPPLY", section=SUPPLY, stated=True)
# the floor the solenoid design lets the rail sag to while a coil pulls, the minimum
# supply of the LED controllers on the same rail, and the pull-in its driver commands
MODEL.uses("v_led_min", of="solenoid", group="SUPPLY", section=SUPPLY, stated=True)
MODEL.uses("t_on_max", of="solenoid")
ds("c_bypass", 100, "nF", src=f"{BUFFER}, 8.3 Power Supply Recommendations, a 0.1 µF "
   "capacitor at each VCC terminal, as close to it as possible",
   group="SUPPLY", section=SUPPLY, stated=True)
asm("place_bypass", 10, "mm", stated=False, src="how close C2 sits to U1's supply pin; "
    "the sheet asks for it as close as the layout allows and states no figure")
asm("place_bulk", 20, "mm", stated=False, src="how close C1 sits to where the feed meets "
    "the servo's cable; no datasheet states one")


dec("t_load_timeout", 40, "ms", stated=False, src="how long begin() waits for the timer to "
    "take the parked pulse before it resets the Teensy: two frames, while the next reload "
    "after attach() comes within one")


# The feed's inductance keeps its current flowing when the servo stops drawing,
# and C1 takes it: half L I squared into half C dV squared.
@fig("v_kick", "mV", group="SUPPLY", section=SUPPLY, prints="up",
     rises_with=["i_stall", "l_feed"], falls_with=["c_bulk"])
def _(i_stall, l_feed, c_bulk):
    return i_stall * (l_feed / c_bulk) ** (1 / 2)


@fig("v_short", "V", group="SUPPLY", section=SUPPLY, prints="up",
     rises_with=["v_servo_min"], falls_with=["v_led_min"])
def _(v_servo_min, v_led_min):
    return v_servo_min - v_led_min


# ===========================================================================
# requirements
# ===========================================================================
_I = MODEL.invariant
_I("begin() waits longer than the one frame the timer's next reload can take",
   lambda v: v.t_load_timeout > v.t_frame)
_I("PWMServo's default range sits inside the SER0049's, so attach() takes the servo's own",
   lambda v: v.t_min < v.t_min_default and v.t_max_default < v.t_max)
_I("the Teensy's high, less R3's drop, clears U1's high input level",
   lambda v: v.v_oh_min - v.v_r_in > v.vih_u1)
_I("the Teensy's low, plus R3's drop, stays under U1's low input level",
   lambda v: v.v_ol_max + v.v_r_in < v.vil_u1)
_I("the pin drives R1 and U1's input inside the current its output levels are specified at",
   lambda v: v.i_pd + v.ii_u1 <= v.i_oh_test)
_I("the pin drives R1 and U1's input inside its 4 mA",
   lambda v: v.i_pd + v.ii_u1 < v.i_pin_max)
_I("after a reset U1's input reads low against the keeper",
   lambda v: v.v_keeper < v.vil_u1)
_I("with the Teensy unpowered, U1 leaves the pin under what an unpowered pin may take",
   lambda v: v.v_pin_off < v.v_clamp_over)
_I("with the machine's 5 V off, the pin drives R1 and U1's clamp diode inside its 4 mA",
   lambda v: v.i_pin_off5 < v.i_pin_max)
_I("with the machine's 5 V off, R3 holds U1's clamp diode inside its rating",
   lambda v: v.i_clamp < v.iik_u1)
_I("the edge through R3 stays inside the longest input edge U1 allows",
   lambda v: v.t_edge_in < v.t_tran_max)
_I("U1's ground, lifted by the servo's stall, leaves the Teensy's high clear of U1's threshold",
   lambda v: v.v_u1_rise < v.m_high)
_I("a signal wire that meets 5 V or ground holds U1's output inside its rating",
   lambda v: v.i_fault <= v.i_out_u1)
_I("the machine's rail sits in U1's supply range",
   lambda v: v.vcc_u1_min <= v.v_5v <= v.vcc_u1_max)
_I("U1 still runs at the rail's floor while a coil pulls",
   lambda v: v.v_led_min >= v.vcc_u1_min)
_I("the machine's rail sits in the servo's supply range",
   lambda v: v.v_servo_min <= v.v_5v <= v.v_servo_max)
_I("the rise at C1 keeps the servo under its maximum",
   lambda v: v.v_5v + v.v_kick < v.v_servo_max)
_I("the stall current the design takes bounds the sheet's other figure",
   lambda v: v.i_stall >= v.i_stall_table)
_I("C1 is bought at twice the rail",
   lambda v: v.v_cap_rating >= 2 * v.v_5v)

for _text, _why in [
    ("1", "FlexPWM1's submodule 1, the one the servo pin sits on, as the driver names it, "
          "and the step past the frame's last count, as the driver writes it"),
    ("40", "t_load_timeout in milliseconds, as the driver writes it"),
]:
    MODEL.aside(_text, _why)


# ===========================================================================
# wiring
# ===========================================================================
# the pin feeds U1 through R3, and R1 holds it low while nothing drives it; U1's input
# clamp diode points to its own 5 V, so it can sink the pin's current but never raises the pin
MODEL.net("Servo signal, Teensy side",
          pull("R1", "gnd", src="design.md, R1 pulls the pin to ground through 10 kΩ"),
          reads("U1 input A through R3", src="design.md, R3 between the Teensy and U1"),
          teensy="Servo signal")
MODEL.net("Servo signal, servo side",
          drives("U1 output Y", "push-pull", rail="the machine's 5 V",
                 src="design.md, U1 takes its 5 V at J-PWR, and OE on ground keeps its output on"),
          reads("the SER0049's signal input through R2", src="design.md, R2 between U1 and the servo"))

MODEL.owns("FlexPWM1.1", "PWMServo sets the submodule to a 20 ms frame, and the driver writes its "
                         "VAL0 itself, firmware/drivers/servo.md")
