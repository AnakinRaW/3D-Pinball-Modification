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
              drawings=[HERE / "toggle-switch-schematic.svg",
                        HERE.parent / "motherboard" / "motherboard-schematic.svg"])

dec = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="decision", **kw)
fig = MODEL.derived

R1 = "Series resistor"

# ===========================================================================
# a pin driven high into the closed switch
# ===========================================================================
MODEL.uses("v_3v3", of="teensy-4.1", group="I_FAULT", section=R1, stated=True)
MODEL.uses("v_3v3_acc", of="teensy-4.1")
MODEL.uses("v_3v3_max", of="teensy-4.1", group="I_FAULT", section=R1, stated=True)
MODEL.uses("v_3v3_min", of="teensy-4.1")
dec("r_s", 1.2, "kΩ", src="R1, in series between the switch pin and the switch: the 1.2 kΩ "
    "of the resistor kit", group="I_FAULT", section=R1,
    stated=True)
MODEL.input("kit_tol", 5, "%", kind="datasheet", src="the justPi resistor kit JUS-19729 every resistor comes from, Botland product page: tolerance 5 %", group="I_FAULT", section=R1, stated=True)
MODEL.input("p_kit", 0.25, "W", kind="datasheet", src="the same kit and page: power rating 0.25 W", stated=False)
MODEL.input("p_derate", 50, "%", kind="decision", src="the derating target for resistor power in the design-review skill, at most half the rating", stated=False)
MODEL.uses("i_pin_max", of="teensy-4.1", group="I_FAULT", section=R1, stated=True)


@fig("r_s_low", "kΩ", group="I_FAULT", section=R1,
     rises_with=["r_s"], falls_with=["kit_tol"])
def _(r_s, kit_tol):
    return r_s * (1 - kit_tol)


# The firmware sets the pin as an output and drives it high while the switch
# holds it at ground, so R1 alone carries the rail, taken at its high end.
@fig("i_fault", "mA", group="I_FAULT", section=R1, prints="up",
     rises_with=["v_3v3_max", "kit_tol"], falls_with=["r_s"])
def _(v_3v3_max, r_s_low):
    return v_3v3_max / r_s_low


# The least R1 whose low end still holds that current inside what the pin allows.
@fig("r_s_min", "kΩ", group="I_FAULT", section=R1, prints="up",
     rises_with=["v_3v3_max", "kit_tol"], falls_with=["i_pin_max"])
def _(v_3v3_max, i_pin_max, kit_tol):
    return v_3v3_max / (i_pin_max * (1 - kit_tol))


@fig("p_r_s", "mW", stated=False,
     rises_with=["v_3v3_max", "kit_tol"], falls_with=["r_s"])
def _(v_3v3_max, i_fault):
    return v_3v3_max * i_fault


# ===========================================================================
# the closed switch, read through R1
# ===========================================================================
MODEL.uses("i_pu_max", of="teensy-4.1", group="V_LOW", section=R1, stated=True)
MODEL.uses("vil_frac", of="teensy-4.1")


@fig("r_s_high", "kΩ", group="V_LOW", section=R1, rises_with=["r_s", "kit_tol"])
def _(r_s, kit_tol):
    return r_s * (1 + kit_tol)


# The pull-up's current flows through R1 into the closed switch. It is largest
# with the pin at ground, so the datasheet's figure there bounds it.
@fig("v_low_max", "V", group="V_LOW", section=R1, prints="up",
     rises_with=["i_pu_max", "r_s", "kit_tol"])
def _(i_pu_max, r_s_high):
    return i_pu_max * r_s_high


# The input's low level follows the rail, so its lowest sits at the rail's low end.
@fig("v_il", "V", group="V_LOW", section=R1, prints="down",
     rises_with=["vil_frac", "v_3v3_min"])
def _(vil_frac, v_3v3_min):
    return vil_frac * v_3v3_min


# The most R1 whose high end still reads the closed switch as a low.
@fig("r_s_max", "kΩ", group="V_LOW", section=R1, prints="down",
     rises_with=["vil_frac", "v_3v3_min"], falls_with=["i_pu_max", "kit_tol"])
def _(v_il, i_pu_max, kit_tol):
    return v_il / (i_pu_max * (1 + kit_tol))


# ===========================================================================
# requirements
# ===========================================================================
_I = MODEL.invariant
_I("a pin driven high into the closed switch stays inside its 4 mA",
   lambda v: v.i_fault < v.i_pin_max)
_I("the closed switch still reads low through R1",
   lambda v: v.v_low_max < v.v_il)
_I("R1 sits between the least value that holds the pin's current and the most that still reads low",
   lambda v: v.r_s_min <= v.r_s <= v.r_s_max)
_I("R1 carries a pin driven into the closed switch at no more than half its rating",
   lambda v: v.p_r_s <= v.p_kit * v.p_derate)


# ===========================================================================
# wiring
# ===========================================================================
# the switch closes the pin to ground through R1, and the Teensy's internal pull-up holds it high
MODEL.net("Toggle switch",
          contact("P26 through R1", src="design.md, the switch closes the pin to ground through R1"),
          pull("the Teensy's internal pull-up", TEENSY_RAIL,
               src="docs/firmware/drivers/controls.md, the pin runs with INPUT_PULLUP"),
          teensy="Toggle switch")
