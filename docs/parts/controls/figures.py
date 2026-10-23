"""The figures of the toggle switch and its series resistor, as inputs plus formulas."""
from figcheck import *

MODEL = Model.at(__file__, "controls",
                 drawings=["toggle-switch-schematic.svg",
                           "../motherboard/motherboard-schematic.svg"],
                 documents=["../../firmware/drivers/controls.md", "../motherboard/design.md"])
fig, ds, gr, msr, asm, dec = MODEL.declarers()
MODEL.uses("v_rail", of="lighting")
dec("t_bounce", 50, "ms", src="the time after a report in which the driver drops an edge as bounce, docs/firmware/drivers/controls.md")

R1 = "Series resistor"

# -- a pin driven high into the closed switch ---------------------------------
with MODEL.block("I_FAULT", section=R1):
    MODEL.uses("v_3v3", of="teensy-4.1")
    MODEL.uses("v_3v3_acc", of="teensy-4.1", stated=False)
    MODEL.uses("v_3v3_max", of="teensy-4.1")
    MODEL.uses("v_3v3_min", of="teensy-4.1", stated=False)
    dec("r_s", 1.2, "kΩ", src="R1, in series between the switch pin and the switch: the 1.2 kΩ of "
        "the resistor kit")
    ds("kit_tol", 5, "%", src="the justPi resistor kit JUS-19729 every resistor comes from, "
       "Botland product page: tolerance 5 %")
    ds("p_kit", 0.25, "W", src="the same kit and page: power rating 0.25 W", stated=False)
    dec("p_derate", 50, "%", src="the derating target for resistor power in the design-review "
        "skill, at most half the rating", stated=False)
    MODEL.uses("i_pin_max", of="teensy-4.1")
    fig("r_s_low", "kΩ", "r_s * (1 - kit_tol)", moves="+r_s -kit_tol")

    # The firmware sets the pin as an output and drives it high while the switch
    # holds it at ground, so R1 alone carries the rail, taken at its high end.
    fig("i_fault", "mA", "v_3v3_max / r_s_low", "up", moves="+v_3v3_max +kit_tol -r_s")

    # The least R1 whose low end still holds that current inside what the pin allows.
    fig("r_s_min", "kΩ", "v_3v3_max / (i_pin_max * (1 - kit_tol))", "up",
        moves="+v_3v3_max +kit_tol -i_pin_max")

fig("p_r_s", "mW", "v_3v3_max * i_fault", moves="+v_3v3_max +kit_tol -r_s", stated=False)

# -- the closed switch, read through R1 ---------------------------------------
with MODEL.block("V_LOW", section=R1):
    MODEL.uses("i_pu_max", of="teensy-4.1")
    MODEL.uses("vil_frac", of="teensy-4.1", stated=False)
    fig("r_s_high", "kΩ", "r_s * (1 + kit_tol)", moves="+r_s +kit_tol")

    # The pull-up's current flows through R1 into the closed switch. It is largest
    # with the pin at ground, so the datasheet's figure there bounds it.
    fig("v_low_max", "V", "i_pu_max * r_s_high", "up", moves="+i_pu_max +r_s +kit_tol")

    # The input's low level follows the rail, so its lowest sits at the rail's low end.
    fig("v_il", "V", "vil_frac * v_3v3_min", "down", moves="+vil_frac +v_3v3_min")

    # The most R1 whose high end still reads the closed switch as a low.
    fig("r_s_max", "kΩ", "v_il / (i_pu_max * (1 + kit_tol))", "down",
        moves="+vil_frac +v_3v3_min -i_pu_max -kit_tol")

# -- requirements -------------------------------------------------------------
_I = MODEL.invariant
_I("a pin driven high into the closed switch stays inside its 4 mA", "i_fault < i_pin_max")
_I("the closed switch still reads low through R1", "v_low_max < v_il")
_I("R1 sits between the least value that holds the pin's current and the most that still reads low",
   "r_s_min <= r_s <= r_s_max")
_I("R1 carries a pin driven into the closed switch at no more than half its rating",
   "p_r_s <= p_kit * p_derate")

# -- wiring -------------------------------------------------------------------
# the switch closes the pin to ground through R1, and the Teensy's internal pull-up holds it high
MODEL.net("Toggle switch",
          contact("P26 through R1", src="design.md, the switch closes the pin to ground through R1"),
          pull("the Teensy's internal pull-up", TEENSY_RAIL,
               src="docs/firmware/drivers/controls.md, the pin runs with INPUT_PULLUP"),
          teensy="Toggle switch")
