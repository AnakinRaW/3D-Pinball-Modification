"""The figures of the hit targets, in this build the stock machine's two bells, as inputs plus formulas.

Run with:  python tools/figcheck.py docs/parts/targets/figures.py

Every input names where it comes from. Every derived quantity is a function
whose parameter names are its dependencies, so `--graph <key>` answers what a
value feeds and what it rests on.
"""
import pathlib

from figcheck import Model, TEENSY_RAIL, contact, pull, reads

HERE = pathlib.Path(__file__).resolve().parent
MODEL = Model("targets", HERE / "design.md", section=None, until="## Sources",
              drawings=[HERE / "targets-schematic.svg"],
              documents=[HERE.parents[1] / "firmware" / "drivers" / "contacts.md"])

ds = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="datasheet", **kw)
dec = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="decision", **kw)
fig = MODEL.derived

EXP = "MCP23008-MCP23S08-Microchip.pdf"
LEVELS = "Levels"

# ===========================================================================
# U1's inputs, the rail behind them and the ball's contact
# ===========================================================================
MODEL.uses("v_3v3_min", of="teensy-4.1", group="LEVELS", section=LEVELS, stated=True)
MODEL.uses("r_contact", of="solenoid", group="LEVELS", section=LEVELS, stated=True)
MODEL.uses("i_pu_gp", of="port-expander", group="LEVELS", section=LEVELS, stated=True)
ds("vil_frac_exp", 20, "%", sheet=EXP, src="DS20001919F, D031: GPIO input low voltage V_IL "
   "maximum, 0.2 VDD, Schmitt trigger", group="LEVELS", section=LEVELS, stated=True)


# ===========================================================================
# derived: a touched bell's input
# ===========================================================================
# U1's pull-up drives its whole current through the ball into the bells' foil at GND.
@fig("v_touched", "mV", group="LEVELS", section=LEVELS, prints="up",
     rises_with=["i_pu_gp", "r_contact"])
def _(i_pu_gp, r_contact):
    return i_pu_gp * r_contact


@fig("v_il_exp", "V", group="LEVELS", section=LEVELS, prints="down",
     rises_with=["v_3v3_min", "vil_frac_exp"])
def _(v_3v3_min, vil_frac_exp):
    return v_3v3_min * vil_frac_exp.raw


# The most the ball's contact with a bell may have before a touched bell stays over U1's
# low threshold.
@fig("r_contact_max", "kΩ", group="LEVELS", section=LEVELS, prints="down",
     rises_with=["v_3v3_min", "vil_frac_exp"], falls_with=["i_pu_gp"])
def _(v_il_exp, i_pu_gp):
    return v_il_exp / i_pu_gp


# ===========================================================================
# the contact driver, docs/firmware/drivers/contacts.md
# ===========================================================================
dec("t_quiet", 10, "ms", src="how long a contact has to have been open before the driver publishes "
    "its closure, decided: an estimate above the few milliseconds a contact bounces and far below "
    "the time between two hits of one target", stated=False)
dec("t_fail_contacts", 1, "s", src="how long no read of the port expander may work before the "
    "contacts count as failed: long enough for several of the driver tick's reads, short enough "
    "for a game to react", stated=False)
MODEL.uses("t_poll", of="port-expander")
MODEL.draws(pool="device monitor")   # the contact driver watched by the device monitor


# ===========================================================================
# requirements
# ===========================================================================
_I = MODEL.invariant
_I("a touched bell pulls its input under U1's low threshold", lambda v: v.v_touched <= v.v_il_exp)
_I("the ball's contact, taken from the shells' measurement, stays under what a bell tolerates",
   lambda v: v.r_contact <= v.r_contact_max)
_I("several of the driver tick's reads of the port expander fail before the contacts count as "
   "failed", lambda v: v.t_fail_contacts > 2 * v.t_poll)


# ===========================================================================
# wiring
# ===========================================================================
for _n, _e in ((1, "E-1"), (2, "E-2")):
    MODEL.net(f"Bell {_n}",
              contact(f"the ball between bell {_n} and the bells' foil at GND",
                      src="design.md, the bells' foil takes its ground from the Teensy motherboard"),
              pull(f"U1's pull-up at {_e}", TEENSY_RAIL,
                   src="port-expander/design.md, the driver switches every input's pull-up on"),
              reads(f"U1 at {_e}", src="DS20001919F, Table 1-1: GP0 and GP1 are U1's inputs"))
