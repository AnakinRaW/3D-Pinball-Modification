"""The figures of the contacts on the port expander, in this build the drain ball store's foils
and the stock machine's two bells, as inputs plus formulas."""
from figcheck import *

MODEL = Model.at(__file__, "contacts", drawings=["contacts-schematic.svg"],
                 documents=["../../firmware/drivers/contacts.md"])
fig, ds, gr, msr, asm, dec = MODEL.declarers()

EXP = "MCP23008-MCP23S08-Microchip.pdf"
LEVELS = "Levels"

# -- U1's inputs, the rail behind them and the ball's contact -----------------
with MODEL.block("LEVELS", section=LEVELS):
    MODEL.uses("v_3v3_min", of="teensy-4.1")
    MODEL.uses("r_contact", of="solenoid")
    MODEL.uses("i_pu_gp", of="port-expander")
    ds("vil_frac_exp", 20, "%", sheet=EXP, src="DS20001919F, D031: GPIO input low voltage V_IL "
       "maximum, 0.2 VDD, Schmitt trigger")

# -- derived: a touched bell's input ------------------------------------------
with MODEL.block("LEVELS", section=LEVELS):
    # U1's pull-up drives its whole current through the ball into the bells' foil at GND.
    fig("v_touched", "mV", "i_pu_gp * r_contact", "up", moves="+i_pu_gp +r_contact")
    fig("v_il_exp", "V", "v_3v3_min * vil_frac_exp.raw", "down", moves="+v_3v3_min +vil_frac_exp")

    # The most the ball's contact with a bell may have before a touched bell stays over U1's
    # low threshold.
    fig("r_contact_max", "kΩ", "v_il_exp / i_pu_gp", "down",
        moves="+v_3v3_min +vil_frac_exp -i_pu_gp")

# -- derived: a stored ball's input ------------------------------------------
STORE = "Drain ball store"
msr("d_fingers", 9, "mm", src="the distance between the seesaw's two fingers, measured on the "
    "stock ball separator with a ruler")
with MODEL.block("STORE", section=STORE):
    # A stored ball closes two contacts in series, the floor foil and the wall foil, each taken
    # as the shells' measured contact.
    fig("r_store", "Ω", "2 * r_contact", "up", moves="+r_contact")
    fig("v_store", "mV", "i_pu_gp * r_store", "up", moves="+i_pu_gp +r_store")

# -- the contact driver, docs/firmware/drivers/contacts.md --------------------
dec("t_quiet", 10, "ms", src="how long a contact has to have been open before the driver publishes "
    "its closure, decided: an estimate above the few milliseconds a contact bounces and far below "
    "the time between two hits of one target")
dec("t_fail_contacts", 1, "s", src="how long no read of the port expander may work before the "
    "contacts count as failed: long enough for several of the driver tick's reads, short enough "
    "for a game to react")
MODEL.uses("t_poll", of="port-expander")
MODEL.names("D031", "D070", why="the MCP23008 datasheet's parameter numbers for the low threshold and the weak pull-up current, which the Sources cite")
MODEL.draws(pool="device monitor")   # the contact driver watched by the device monitor

# -- requirements -------------------------------------------------------------
_I = MODEL.invariant
_I("a touched bell pulls its input under U1's low threshold", "v_touched <= v_il_exp")
_I("the ball's contact, taken from the shells' measurement, stays under what a bell tolerates",
   "r_contact <= r_contact_max")
_I("a stored ball pulls its input under U1's low threshold", "v_store <= v_il_exp")
_I("a stored ball's two contacts stay under what an input tolerates", "r_store <= r_contact_max")
_I("several of the driver tick's reads of the port expander fail before the contacts count as "
   "failed", "t_fail_contacts > 2 * t_poll")

# -- wiring -------------------------------------------------------------------
for _n, _e in ((1, "E-3"), (2, "E-4")):
    MODEL.net(f"Bell {_n}",
              contact(f"the ball between bell {_n} and the bells' foil at GND",
                      src="design.md, the bells' foil takes its ground from the Teensy motherboard"),
              pull(f"U1's pull-up at {_e}", TEENSY_RAIL,
                   src="port-expander/design.md, the driver switches every input's pull-up on"),
              reads(f"U1 at {_e}", src="DS20001919F, Table 1-1: GP2 and GP3 are U1's inputs"))

for _slot, _e, _gp in (("front", "E-1", "GP0"), ("back", "E-2", "GP1")):
    MODEL.net(f"Ball store {_slot}",
              contact(f"a ball between the {_slot} floor foil and the wall foil at GND",
                      src="design.md, the wall foil takes its ground from the Teensy motherboard"),
              pull(f"U1's pull-up at {_e}", TEENSY_RAIL,
                   src="port-expander/design.md, the driver switches every input's pull-up on"),
              reads(f"U1 at {_e}", src=f"DS20001919F, Table 1-1: {_gp} is one of U1's inputs"))
