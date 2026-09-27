"""The figures of the toggle switch and its series resistor, as inputs plus formulas.

Run with:  python tools/figcheck.py docs/parts/controls/figures.py

Every input names where it comes from. Every derived quantity is a function
whose parameter names are its dependencies, so `--graph <key>` answers what a
value feeds and what it rests on.
"""
import pathlib

from figcheck import Model

HERE = pathlib.Path(__file__).resolve().parent
MODEL = Model("controls", HERE / "design.md", section=None, until="## Sources",
              drawings=[HERE / "toggle-switch-schematic.svg"])

ds = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="datasheet", **kw)
dec = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="decision", **kw)
fig = MODEL.derived

MCU = "IMXRT1060CEC.pdf"
R1 = "The series resistor"

# ===========================================================================
# a pin driven high into the closed switch
# ===========================================================================
dec("v_3v3", 3.3, "V", src="the Teensy's 3.3 V rail, the highest level a pin drives, "
    "PJRC pin assignment card 11a rev4", group="I_FAULT", section=R1, stated=True)
dec("r_s", 1.1, "kΩ", src="R1, in series between the switch pin and the switch, a value "
    "on hand", group="I_FAULT", section=R1, stated=True)
dec("r_tol", 5, "%", src="R1's tolerance, 5 % or better as the part list asks",
    group="I_FAULT", section=R1, stated=True)
dec("i_pin_max", 4, "mA", src="the recommended maximum output current per pin, "
    "Teensy 4.1 product page; the comparison table's 10 mA spans older generations",
    group="I_FAULT", section=R1, stated=True)


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
ds("i_pu_max", 212, "µA", sheet=MCU, src="Table 22, Pull-up resistor (22 kΩ PU), "
   "RPU_22K at Vin = 0 V, maximum; the pull-up INPUT_PULLUP selects, IOMUXC_PAD_PUS(3) "
   "in PJRC's cores/teensy4/digital.c", group="V_LOW", section=R1, stated=True)
ds("vil_frac", 0.3, "", sheet=MCU, stated=False, src="Table 22, Low-Level input voltage "
   "VIL, maximum 0.3 x NVCC_XXXX, written into the label of the line it scales")


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
