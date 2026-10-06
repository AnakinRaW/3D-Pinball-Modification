"""The figures of the toggle switch and its series resistor, as inputs plus formulas.

Run with:  python tools/figcheck.py docs/parts/controls/figures.py

Every input names where it comes from. Every derived quantity is a function
whose parameter names are its dependencies, so `--graph <key>` answers what a
value feeds and what it rests on.
"""
import pathlib

from figcheck import Model, TEENSY_RAIL, contact, pull

HERE = pathlib.Path(__file__).resolve().parent
MODEL = Model("controls", HERE / "design.md", section=None, until="## Sources",
              drawings=[HERE / "toggle-switch-schematic.svg"])

dec = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="decision", **kw)
fig = MODEL.derived

R1 = "The series resistor"

# ===========================================================================
# a pin driven high into the closed switch
# ===========================================================================
MODEL.uses("v_3v3", of="teensy-4.1", group="I_FAULT", section=R1, stated=True)
dec("r_s", 1.1, "kΩ", src="R1, in series between the switch pin and the switch, a value "
    "on hand", group="I_FAULT", section=R1, stated=True)
dec("r_tol", 5, "%", src="R1's tolerance, 5 % or better as the part list asks",
    group="I_FAULT", section=R1, stated=True)
MODEL.uses("i_pin_max", of="teensy-4.1", group="I_FAULT", section=R1, stated=True)


@fig("r_s_low", "kΩ", group="I_FAULT", section=R1,
     rises_with=["r_s"], falls_with=["r_tol"])
def _(r_s, r_tol):
    return r_s * (1 - r_tol)


# The firmware sets the pin as an output and drives it high while the switch
# holds it at ground, so R1 alone carries the rail.
@fig("i_fault", "mA", group="I_FAULT", section=R1, prints="up",
     rises_with=["v_3v3", "r_tol"], falls_with=["r_s"])
def _(v_3v3, r_s_low):
    return v_3v3 / r_s_low


# ===========================================================================
# the closed switch, read through R1
# ===========================================================================
MODEL.uses("i_pu_max", of="teensy-4.1", group="V_LOW", section=R1, stated=True)
MODEL.uses("vil_frac", of="teensy-4.1")


@fig("r_s_high", "kΩ", group="V_LOW", section=R1, rises_with=["r_s", "r_tol"])
def _(r_s, r_tol):
    return r_s * (1 + r_tol)


# The pull-up's current flows through R1 into the closed switch. It is largest
# with the pin at ground, so the datasheet's figure there bounds it.
@fig("v_low_max", "V", group="V_LOW", section=R1, prints="up",
     rises_with=["i_pu_max", "r_s", "r_tol"])
def _(i_pu_max, r_s_high):
    return i_pu_max * r_s_high


@fig("v_il", "V", group="V_LOW", section=R1, prints="down",
     rises_with=["vil_frac", "v_3v3"])
def _(vil_frac, v_3v3):
    return vil_frac * v_3v3


# ===========================================================================
# requirements
# ===========================================================================
_I = MODEL.invariant
_I("a pin driven high into the closed switch stays inside its 4 mA",
   lambda v: v.i_fault < v.i_pin_max)
_I("the closed switch still reads low through R1",
   lambda v: v.v_low_max < v.v_il)


# ===========================================================================
# wiring
# ===========================================================================
# the switch closes the pin to ground through R1, and the Teensy's internal pull-up holds it high
MODEL.net("Toggle switch",
          contact("P26 through R1", src="design.md, the switch closes the pin to ground through R1"),
          pull("the Teensy's internal pull-up", TEENSY_RAIL,
               src="docs/firmware/drivers/controls.md, the pin runs with INPUT_PULLUP"),
          teensy="Toggle switch")
