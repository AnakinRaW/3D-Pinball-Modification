"""The figures of the servo, its buffer and its supply, as inputs plus formulas."""
from figcheck import *

MODEL = Model.at(__file__, "servo", drawings=["servo-schematic.svg"],
                 documents=["../../firmware/drivers/servo.md"])
fig, ds, gr, msr, asm, dec = MODEL.declarers()

SERVO = "SER0049-DFRobot.pdf"
BUFFER = "TI CD74HCT125 datasheet, SCHS415A"

PULSE = "Pulse"
LEVELS = "Signal levels"
OUTPUT = "Output"
SUPPLY = "Supply"

# -- the pulse ----------------------------------------------------------------
with MODEL.block("PULSE", section=PULSE):
    dec("t_frame", 20, "ms", src="the frame a servo expects its pulse in: REFRESH_INTERVAL, "
        "20000 µs, in the Arduino Servo library the SER0049 sheet points to, and the 50 Hz PJRC's "
        "PWMServo sets with analogWriteFrequency()")
    ds("t_min", 500, "µs", sheet=SERVO, src="Pulse Width Range, 500~2500 µs, the short end, one "
       "end of the travel")
    ds("t_max", 2500, "µs", sheet=SERVO, src="Pulse Width Range, the long end, the other end of "
       "the travel")
    dec("n_duty", 4096, "steps", src="PWMServo's write() sets the pulse through analogWrite() at "
        "12-bit resolution, 4096 steps to the frame, PJRC's PWMServo.cpp")
    ds("t_min_default", 544, "µs", stated=False, src="PWMServo.h, the pulse for 0 degrees "
       "attach(pin) takes when no range is given")
    ds("t_max_default", 2400, "µs", stated=False, src="PWMServo.h, the pulse for 180 degrees "
       "attach(pin) takes when no range is given")
    fig("f_frame", "Hz", "1 / t_frame", moves="-t_frame")
    fig("t_step", "µs", "t_frame / n_duty", moves="+t_frame -n_duty")

    # How finely PWMServo sets the pulse across the servo's whole travel.
    fig("n_travel", "steps", "(t_max - t_min) / t_step", moves="+t_max +n_duty -t_min -t_frame")

# -- the signal levels: the Teensy's pin through R3 into U1 -------------------
with MODEL.block("LEVELS", section=LEVELS):
    MODEL.uses("v_3v3", of="teensy-4.1")
    MODEL.uses("v_oh_drop", of="teensy-4.1", stated=False)
    MODEL.uses("v_ol_max", of="teensy-4.1")
    MODEL.uses("i_oh_test", of="teensy-4.1", stated=False)
    MODEL.uses("r_keeper_min", of="teensy-4.1")
MODEL.uses("v_3v3_max", of="teensy-4.1")
MODEL.uses("v_3v3_min", of="teensy-4.1")
ds("kit_tol", 5, "%", src="the justPi resistor kit JUS-19729 every resistor comes from, Botland "
   "product page: tolerance 5 %", group="OUTPUT", section=OUTPUT, stated=True)
ds("p_kit", 0.25, "W", src="the same kit and page: power rating 0.25 W")
dec("p_derate", 50, "%", src="the derating target for resistor power in the design-review skill, "
    "at most half the rating")
with MODEL.block("LEVELS", section=LEVELS):
    dec("r_pd", 10, "kΩ", src="R1, from the pin to ground, chosen to hold U1's input under its low "
        "level against the pin's keeper after a reset, at a third of a milliamp while the pin "
        "drives high: the 10 kΩ of the resistor kit")
    ds("vih_u1", 2, "V", src=f"{BUFFER}, 5.3 Recommended Operating Conditions, high-level input "
       "voltage VIH minimum at VCC = 4.5 V to 5.5 V")
    ds("vil_u1", 0.8, "V", src=f"{BUFFER}, 5.3 Recommended Operating Conditions, low-level input "
       "voltage VIL maximum at VCC = 4.5 V to 5.5 V")
    ds("ii_u1", 1, "µA", src=f"{BUFFER}, 5.5 Electrical Characteristics, input leakage current II "
       "maximum at VI = VCC or GND and VCC = 5.5 V, over the whole temperature range")
    dec("r_in", 2.2, "kΩ", src="R3, from the pin to U1's input, chosen so the pin's current into "
        "U1's clamp diode stays far under its 4 mA while the machine's 5 V is off: the 2.2 kΩ of "
        "the resistor kit")
    MODEL.uses("i_pin_max", of="teensy-4.1", stated=False)
    MODEL.uses("v_clamp_over", of="teensy-4.1")
    MODEL.uses("r_lead", of="solenoid")
ds("iik_u1", 20, "mA", src=f"{BUFFER}, 5.1 Absolute Maximum Ratings, input clamp current IIK at VI "
   "> VCC + 0.5 V; 7.3.3 places a clamp diode from each input to VCC", group="OFF", section=LEVELS,
   stated=True)
with MODEL.block("EDGE", section=LEVELS):
    ds("c_in_u1", 10, "pF", src=f"{BUFFER}, 5.5 Electrical Characteristics, input capacitance Ci "
       "maximum")
    asm("c_stray", 15, "pF", src="the wiring from R3 to U1's input, estimated generously and not "
        "measured")
    ds("t_tran_max", 400, "ns", src=f"{BUFFER}, 5.3 Recommended Operating Conditions, input "
       "transition time tt maximum at VCC = 5.5 V, the shorter of its two figures")
asm("edge_low", 10, "%", src="the lower mark TI's logic sheets time an edge from; SCHS415A draws "
    "its marks in a figure the text search cannot read")
asm("edge_high", 90, "%", src="the upper mark, taken as edge_low is")
with MODEL.block("LEVELS", section=LEVELS):
    fig("v_oh_min", "V", "v_3v3_min - v_oh_drop", "down", moves="+v_3v3_min -v_oh_drop")

    # U1's input current runs through R3 in either direction, so U1's input sits
    # that far off the pin's level, against whichever margin it eats into.
    fig("v_r_in", "mV", "ii_u1 * r_in", "up", moves="+ii_u1 +r_in")
    fig("m_high", "V", "v_oh_min - v_r_in - vih_u1", "down",
        moves="+v_3v3_min -v_oh_drop -vih_u1 -ii_u1 -r_in")
    fig("m_low", "V", "vil_u1 - v_ol_max - v_r_in", "down", moves="+vil_u1 -v_ol_max -ii_u1 -r_in")
    fig("i_pd", "mA", "v_3v3_max / r_pd", "up", moves="+v_3v3_max -r_pd")

    # After a reset the keeper holds the level the pin last drove, a high at worst,
    # and R1 pulls against it at the pin, so the pin sits at the divider between
    # the two. U1's input current adds its drop across the two in parallel and
    # across R3.
    fig("v_keeper", "V", "v_3v3_max * r_pd / (r_pd + r_keeper_min) + ii_u1 * (r_pd * r_keeper_min / "
        "(r_pd + r_keeper_min) + r_in)", "up", moves="+v_3v3_max +r_pd +ii_u1 +r_in -r_keeper_min")

    # With the 5 V on and the Teensy unpowered nothing drives the pin: U1's input
    # current is all that reaches its net, and R1 carries it to ground.
    fig("v_pin_off", "mV", "ii_u1 * r_pd", "up", moves="+ii_u1 +r_pd")

with MODEL.block("OFF", section=LEVELS):
    # With the machine's 5 V off, U1's clamp diode holds its input at the dead
    # rail. The pin's high then drives R3 into that diode, both taken at 0 V, and
    # R1 beside it.
    fig("i_clamp", "mA", "v_3v3_max / r_in", "up", moves="+v_3v3_max -r_in")
    fig("i_pin_off5", "mA", "i_clamp + i_pd", "up", moves="+v_3v3_max -r_in -r_pd")

with MODEL.block(section=LEVELS):
    # R3 into U1's input and the wiring beside it is a single pole, whose step
    # crosses the two marks ln((1 - low) / (1 - high)) time constants apart.
    fig("t_edge_in", "ns",
        "ln((1 - edge_low.raw) / (1 - edge_high.raw)) * r_in * (c_in_u1 + c_stray)", "up",
        moves="+r_in +c_in_u1 +c_stray +edge_high -edge_low", group="EDGE")

    # The windows of R1 and R3, each at its own tolerance and at the rail's high end.
    # R1 at its low end, and the pin's high, have to stay inside the current the pin's output
    # levels are specified at.
    fig("r_pd_min", "kΩ", "v_3v3_max / ((i_oh_test - ii_u1) * (1 - kit_tol))", "up",
        moves="+v_3v3_max +kit_tol +ii_u1 -i_oh_test", group="LEVELS")

    # R1 at its high end against the keeper at its strongest still holds U1's input under its
    # low level: v R / (R + K) + i (R K / (R + K) + R3) < VIL, solved for R.
    def _r_pd_max(vil_u1, ii_u1, r_in, kit_tol, v_3v3_max, r_keeper_min):
        room = vil_u1 - ii_u1 * r_in * (1 + kit_tol)
        return room * r_keeper_min / ((v_3v3_max + ii_u1 * r_keeper_min - room) * (1 + kit_tol))
    fig("r_pd_max", "kΩ", _r_pd_max, "down",
        moves="+vil_u1 +r_keeper_min -v_3v3_max -ii_u1 -r_in -kit_tol", group="LEVELS")

    # With the machine's 5 V off, R3 at its low end and R1 at its low end together still hold
    # the pin inside its 4 mA.
    fig("r_in_min", "kΩ",
        "v_3v3_max / ((i_pin_max - v_3v3_max / (r_pd * (1 - kit_tol))) * (1 - kit_tol))", "up",
        moves="+v_3v3_max +kit_tol -i_pin_max -r_pd", group="OFF")

    # R3 at its high end still gives an edge inside the longest U1 allows.
    fig("r_in_max", "kΩ", "t_tran_max / (ln((1 - edge_low.raw) / (1 - edge_high.raw)) * (c_in_u1 + "
        "c_stray) * (1 + kit_tol))", "down",
        moves="+t_tran_max +edge_low -c_in_u1 -c_stray -kit_tol -edge_high", group="EDGE")

    # U1 takes its ground at J-SERVO-PWR, so the servo's stall current through that one
    # wire lifts U1's ground over the distribution, and the Teensy's high arrives
    # that much lower.
    fig("v_u1_rise", "mV", "i_stall * r_lead", "up", moves="+i_stall +r_lead", group="LEVELS")

# -- the output: U1 into the servo's signal wire ------------------------------
with MODEL.block("OUTPUT", section=OUTPUT):
    ds("vcc_u1_min", 4.5, "V", src=f"{BUFFER}, Recommended Operating Conditions, supply voltage "
       "VCC minimum")
    ds("vcc_u1_max", 5.5, "V", src=f"{BUFFER}, Recommended Operating Conditions, supply voltage "
       "VCC maximum; the highest rail U1 is specified on, and the one a fault on the signal wire "
       "is taken at")
    ds("v_oh_u1", 4.4, "V", src=f"{BUFFER}, 5.5 Electrical Characteristics, VOH minimum at IOH = "
       "-20 µA and VCC = 4.5 V")
    ds("i_oh_light", 20, "µA", stated=False, src=f"{BUFFER}, 5.5 Electrical Characteristics, the "
       "light-load current the 4.4 V VOH is specified at")
    dec("r_series", 1.2, "kΩ", src="R2, chosen so a signal wire that meets 5 V or ground holds "
        "U1's output inside its continuous output current: the 1.2 kΩ of the resistor kit")
    asm("c_servo_in", 100, "pF", src="the servo's signal wire together with the servo's own input, "
        "estimated generously and not measured; the SER0049 sheet states no input figure")
    ds("i_out_u1", 35, "mA", src=f"{BUFFER}, 5.1 Absolute Maximum Ratings, continuous output "
       "current IO; 5.3 states no output current")
    fig("i_fault", "mA", "vcc_u1_max / (r_series * (1 - kit_tol))", "up",
        moves="+vcc_u1_max +kit_tol -r_series")
    fig("p_r_series", "mW", "vcc_u1_max * i_fault", moves="+vcc_u1_max +kit_tol -r_series",
        stated=False)

    # The least R2 whose low end still holds that fault inside U1's continuous output current.
    fig("r_series_min", "kΩ", "vcc_u1_max / (i_out_u1 * (1 - kit_tol))", "up",
        moves="+vcc_u1_max +kit_tol -i_out_u1")

    # R2 into the wire and the servo's input is a single pole. The most R2 is the one whose edge
    # at its high end still takes less than one step of the pulse, so R2 cannot move a pulse by
    # a step.
    fig("r_series_max", "kΩ",
        "t_step / (ln((1 - edge_low.raw) / (1 - edge_high.raw)) * c_servo_in * (1 + kit_tol))",
        "down", moves="+t_frame +edge_low -n_duty -c_servo_in -kit_tol -edge_high")

# -- the supply ---------------------------------------------------------------
with MODEL.block("SUPPLY", section=SUPPLY):
    MODEL.uses("v_5v", of="solenoid")
    ds("v_servo_min", 4.8, "V", sheet=SERVO, src="Operating Voltage, 4.8-6 V DC, minimum")
    ds("v_servo_max", 6, "V", sheet=SERVO, src="Operating Voltage, maximum")
    ds("i_rest", 8, "mA", sheet=SERVO, src="SPECIFICATION list, static current at most 8 mA at "
       "6.0 V, which the selection guide's 9 g 180° column repeats as its quiescent current")
    ds("i_free", 120, "mA", sheet=SERVO, src="SPECIFICATION list, no-load current at most 120 mA "
       "at 6.0 V; the selection guide's 9 g 180° column gives 60 mA, and the larger bounds both")
    ds("i_stall_table", 650, "mA", sheet=SERVO, src="Selection Guide for Clutch Servo, the 9 g "
       "180° column, SER0049, stall current at most 650 mA at 6.0 V")
    ds("i_stall", 800, "mA", sheet=SERVO, src="SPECIFICATION list, stall current at most 800 mA at "
       "6.0 V, the larger of the sheet's two figures and the one the design takes")
    ds("t_block", 5, "s", sheet=SERVO, stated=False, src="Electronic Protection: after being "
       "blocked for 5 seconds the servo turns off its power")
    MODEL.uses("l_feed", of="solenoid")
    C1 = "KEMET ESL107M016AC3AA"
    dec("c_c1", 100, "µF", src=f"C1, {C1}, 100 µF 16 V, the 100 µF per servo that Adafruit's "
        "PCA9685 guide starts from; the same part as the solenoid board's C91")
    ds("z_c1", 230, "mΩ", src=f"{C1}, ESR at 100 kHz, as DigiKey lists it from the ESL sheet")
    ds("v_c1_rating", 16, "V", src=f"{C1}, rated voltage")

    # the pull-in the solenoid driver commands
    MODEL.uses("t_on_max", of="solenoid", stated=False)
    ds("c_bypass", 100, "nF", src=f"{BUFFER}, 8.3 Power Supply Recommendations, a 0.1 µF capacitor "
       "at each VCC terminal, as close to it as possible")
    asm("place_bypass", 10, "mm", stated=False, src="how close C2 sits to U1's supply pin; the "
        "sheet asks for it as close as the layout allows and states no figure")
    asm("place_bulk", 20, "mm", stated=False, src="how close C1 sits to where the feed meets the "
        "servo's cable; no datasheet states one")
    dec("t_load_timeout", 40, "ms", stated=False, src="how long begin() waits for the timer to "
        "take the parked pulse before it fails: two frames, while the next reload after attach() "
        "comes within one")

    # The feed's inductance keeps its current flowing when the servo stops drawing,
    # and C1 takes it: half L I squared into half C dV squared, plus the same current
    # across C1's impedance.
    fig("v_kick", "mV", "i_stall * (l_feed / c_c1) ** (1 / 2) + i_stall * z_c1", "up",
        moves="+i_stall +l_feed +z_c1 -c_c1")

    # The least C1 that keeps the servo under its maximum at a stop, once the part's
    # impedance has taken its share of the room.
    fig("c_bulk_min", "µF", "l_feed * (i_stall / (v_servo_max - v_5v - i_stall * z_c1)) ** 2", "up",
        moves="+i_stall +l_feed +v_5v +z_c1 -v_servo_max")

    # The rail may sag as far as U1 still runs; the servo then sits under its own floor.
    fig("v_short", "V", "v_servo_min - vcc_u1_min", "up", moves="+v_servo_min -vcc_u1_min")

# -- requirements -------------------------------------------------------------
_I = MODEL.invariant
_I("begin() waits longer than the one frame the timer's next reload can take",
   "t_load_timeout > t_frame")
_I("PWMServo's default range sits inside the SER0049's, so attach() takes the servo's own",
   "t_min < t_min_default and t_max_default < t_max")
_I("the Teensy's high, less R3's drop, clears U1's high input level", "v_oh_min - v_r_in > vih_u1")
_I("the Teensy's low, plus R3's drop, stays under U1's low input level",
   "v_ol_max + v_r_in < vil_u1")
_I("the pin drives R1 and U1's input inside the current its output levels are specified at",
   "i_pd + ii_u1 <= i_oh_test")
_I("the pin drives R1 and U1's input inside its 4 mA", "i_pd + ii_u1 < i_pin_max")
_I("after a reset U1's input reads low against the keeper", "v_keeper < vil_u1")
_I("with the Teensy unpowered, U1 leaves the pin under what an unpowered pin may take",
   "v_pin_off < v_clamp_over")
_I("with the machine's 5 V off, the pin drives R1 and U1's clamp diode inside its 4 mA",
   "i_pin_off5 < i_pin_max")
_I("with the machine's 5 V off, R3 holds U1's clamp diode inside its rating", "i_clamp < iik_u1")
_I("the edge through R3 stays inside the longest input edge U1 allows", "t_edge_in < t_tran_max")
_I("U1's ground, lifted by the servo's stall, leaves the Teensy's high clear of U1's threshold",
   "v_u1_rise < m_high")
_I("a signal wire that meets 5 V or ground holds U1's output inside its rating",
   "i_fault <= i_out_u1")
_I("R1 sits inside its window", "r_pd_min <= r_pd <= r_pd_max")
_I("R2 sits inside its window", "r_series_min <= r_series <= r_series_max")
_I("R3 sits inside its window", "r_in_min <= r_in <= r_in_max")
_I("R2 carries a shorted signal wire at no more than half its rating",
   "p_r_series <= p_kit * p_derate")
_I("the machine's rail sits in U1's supply range", "vcc_u1_min <= v_5v <= vcc_u1_max")
_I("the machine's rail sits in the servo's supply range", "v_servo_min <= v_5v <= v_servo_max")
_I("the rise at C1 keeps the servo under its maximum", "v_5v + v_kick < v_servo_max")
_I("the rise at C1 keeps U1 under its supply maximum", "v_5v + v_kick < vcc_u1_max")
_I("the stall current the design takes bounds the sheet's other figure", "i_stall >= i_stall_table")
_I("C1 is bought at twice the rail", "v_c1_rating >= 2 * v_5v")
_I("C1 is at least what a stop needs", "c_c1 >= c_bulk_min")

# -- wiring -------------------------------------------------------------------
# the pin feeds U1 through R3, and R1 holds it low while nothing drives it; U1's input
# clamp diode points to its own 5 V, so it can sink the pin's current but never raises the pin
MODEL.net("Servo signal, Teensy side",
          pull("R1", "gnd", src="design.md, R1 pulls the pin to ground through 10 kΩ"),
          reads("U1 input A through R3", src="design.md, R3 between the Teensy and U1"),
          teensy="Servo signal")
MODEL.net("Servo signal, servo side",
          drives("U1 output Y", "push-pull", rail="the machine's 5 V",
                 src="design.md, U1 takes its 5 V at J-SERVO-PWR, and OE on ground keeps its output on"),
          reads("the SER0049's signal input through R2", src="design.md, R2 between U1 and the servo"))

MODEL.owns("FlexPWM1.1", "PWMServo sets the submodule to a 20 ms frame, and the driver writes its "
                         "VAL0 itself, docs/firmware/drivers/servo.md")
MODEL.draws(pool="device monitor")   # a timer that has not taken the parked pulse in begin()
