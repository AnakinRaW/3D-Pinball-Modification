"""The figures of the ball drain break beam, as inputs plus formulas.

Run with:  python tools/figcheck.py docs/parts/break-beam/figures.py

Every input names where it comes from. Every derived quantity is a function
whose parameter names are its dependencies, so `--graph <key>` answers what a
value feeds and what it rests on.
"""
import pathlib

from figcheck import Model, Q, TEENSY_RAIL, drives, pull

HERE = pathlib.Path(__file__).resolve().parent
MODEL = Model("break-beam", HERE / "design.md",
              section=None, until="## Sources",
              documents=[HERE.parents[1] / "firmware" / "drivers" / "break-beam.md"])

ds = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="datasheet", **kw)
dec = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="decision", **kw)
asm = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="assumed", **kw)
msr = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="measured", **kw)
fig = MODEL.derived

SENSOR = "HD-DS25CM-3MM-Adafruit.pdf"
GATE = "The gate"

# ===========================================================================
# what the module brings
# ===========================================================================
ds("v_supply_min", 3.0, "V", sheet=SENSOR, src="point 3, the supply range",
   group="V_3V3", section=GATE, stated=True)
ds("i_emitter", 10, "mA", sheet=SENSOR, src="point 4, the working current",
   group="I_SUP", section=GATE, stated=True)
ds("i_sink_max", 100, "mA", sheet=SENSOR, src="point 6, what the output can sink",
   group="I_PU", section=GATE, stated=True)
ds("t_response", 2, "ms", sheet=SENSOR, src="point 9, the response time",
   group="V_MAX", section=GATE, stated=True)

asm("i_receiver", 10, "mA", src="the receiver's own draw, which no source states, "
    "bounded at the emitter's current",
    group="I_SUP", section=GATE, stated=True)

msr("v_ol", 11, "mV", src="the white lead against ground, beam blocked, at a 3.3 V "
    "supply with 10 kΩ to it; the sweep read 10 to 11 mV and the higher one stands",
    group="V_OL", section=GATE, stated=True)

# ===========================================================================
# what the controller brings
# ===========================================================================
MODEL.uses("v_3v3", of="teensy-4.1", group="V_3V3", section=GATE, stated=True)
MODEL.uses("i_teensy_3v3", of="teensy-4.1", group="I_SUP", section=GATE, stated=True)
MODEL.uses("r_pullup", of="teensy-4.1", group="R_PU", section=GATE, stated=True)
MODEL.uses("t_monitor", of="firmware")

# ===========================================================================
# what the machine brings
# ===========================================================================
MODEL.uses("ball_diameter", of="ir-reflective", group="D_BALL", section=GATE, stated=True)
dec("t_blocked_fail", 5, "s", stated=False, src="how long the beam may stay broken before the driver reports "
    "DeviceFailed, far longer than any passing ball keeps it broken")
dec("t_dead", 10, "ms", stated=False,
    src="the driver takes edges inside this window as one crossing, decided: an "
    "estimate above the few milliseconds the ball's edge takes to cross the beam "
    "and far below the gap between two drains, to be replaced by a scope reading "
    "of the output during a crossing")
MODEL.uses("v_ball", of="ir-reflective", group="V_MAX", section=GATE, stated=True)

# ===========================================================================
# derived
# ===========================================================================
@fig("i_pullup", "µA", group="I_PU", section=GATE,
     rises_with=["v_3v3"], falls_with=["r_pullup"])
def _(v_3v3, r_pullup):
    return v_3v3 / r_pullup


@fig("i_supply", "mA", group="I_SUP", section=GATE,
     rises_with=["i_emitter", "i_receiver"])
def _(i_emitter, i_receiver):
    return i_emitter + i_receiver


@fig("v_ball_max", "m/s", group="V_MAX", section=GATE,
     rises_with=["ball_diameter"], falls_with=["t_response"])
def _(ball_diameter, t_response):
    return ball_diameter / t_response


@fig("t_block", "ms", group="T_BLOCK", section=GATE,
     rises_with=["ball_diameter"], falls_with=["v_ball"])
def _(ball_diameter, v_ball):
    return ball_diameter / v_ball



# ===========================================================================
# what the design requires, stated over the quantities rather than over any
# formula above
# ===========================================================================
_I = MODEL.invariant
_I("the supply the module runs at is inside its range",
   lambda v: v.v_supply_min < v.v_3v3)
_I("the pair fits the Teensy's 3V3 pin",
   lambda v: v.i_supply < v.i_teensy_3v3)
_I("the output sinks far less than it is rated for",
   lambda v: v.i_pullup < v.i_sink_max)
_I("the low level sits at the rail rather than near a threshold",
   lambda v: v.v_ol < v.v_3v3 / Q.of(10, ""))
_I("the gate reports the fastest ball the build assumes",
   lambda v: v.v_ball_max > v.v_ball and v.t_block > v.t_response)

for _text, _why in [
    ("2 cm", "the gap between the two bodies, a mounting dimension"),
    ("3 mm", "the LED package the two bodies carry"),
]:
    MODEL.aside(_text, _why)


# ===========================================================================
# wiring
# ===========================================================================
# the receiver's open collector pulls the line low while the beam is blocked, and the
# Teensy's internal pull-up holds it high otherwise
MODEL.net("Receiver output",
          drives("receiver OUT", "open-drain", src="design.md, the receiver's output is an open collector"),
          pull("the Teensy's internal pull-up", TEENSY_RAIL,
               src="docs/firmware/drivers/break-beam.md, the pin runs with INPUT_PULLUP"),
          teensy="Receiver output of the ball drain gate")

MODEL.draws("i_supply", pool=TEENSY_RAIL)   # both bodies, fed from the Teensy's 3V3
MODEL.draws(pool="device monitor")
