"""The figures of the port expander on the Teensy motherboard, as inputs plus formulas.

Run with:  python tools/figcheck.py docs/parts/port-expander/figures.py

Every input names where it comes from. Every derived quantity is a function
whose parameter names are its dependencies, so `--graph <key>` answers what a
value feeds and what it rests on.
"""
import pathlib

from figcheck import Model, TEENSY_RAIL, drives, ln, pull, reads

HERE = pathlib.Path(__file__).resolve().parent
MODEL = Model("port-expander", HERE / "design.md", section=None, until="## Sources",
              drawings=[HERE / "port-expander-schematic.svg"],
              documents=[HERE.parents[1] / "firmware" / "io" / "port-expander.md"])

ds = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="datasheet", **kw)
dec = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="decision", **kw)
asm = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="assumed", **kw)
fig = MODEL.derived

EXP = "MCP23008-MCP23S08-Microchip.pdf"
I2C = "UM10204-NXP.pdf"
BUS = "Bus"
READ = "Read"
SUPPLY = "Supply"

# ===========================================================================
# the bus, Wire, which U1 shares with the rotary sensor
# ===========================================================================
MODEL.uses("r_pu", of="magnetic-rotary", group="BUS", section=BUS, label="rotary module",
           stated=True)
dec("r_pu_board", 4.7, "kΩ", src="R5 and R6, SDA and SCL to the 3V3 rail on the Teensy "
    "motherboard, a value of the resistor kit; with them a rising edge stays inside "
    "Standard-mode on the motherboard's load alone", group="BUS", section=BUS,
    label="R5 and R6", stated=True)
MODEL.uses("c_bus", of="magnetic-rotary", group="BUS", section=BUS, stated=True)
asm("c_board", 100, "pF", src="each bus line's share of the motherboard's traces together with "
    "U1's pin, taken generously; no measurement", group="BUS", section=BUS, stated=True)
ds("c_b_max", 400, "pF", sheet=EXP, src="DS20001919F, D102: the capacitive loading SDA allows, "
   "CB maximum", group="BUS", section=BUS, stated=True)
MODEL.uses("vil_frac", of="magnetic-rotary")
MODEL.uses("vih_frac", of="magnetic-rotary")
MODEL.uses("t_r_std", of="magnetic-rotary", group="BUS", section=BUS, stated=True)
MODEL.uses("r_pullup", of="teensy-4.1", group="BUS", section=BUS, stated=True)
MODEL.uses("v_3v3_min", of="teensy-4.1")
MODEL.uses("v_3v3_max", of="teensy-4.1", group="BUS", section=BUS, stated=True)
ds("i_ol_std", 3, "mA", sheet=I2C, src="UM10204 Rev. 7.0, Table 10: LOW-level output current "
   "IOL at VOL = 0.4 V, Standard-mode, minimum", group="BUS", section=BUS, stated=True)


@fig("r_bus", "kΩ", group="BUS", section=BUS, rises_with=["r_pu", "r_pu_board"])
def _(r_pu, r_pu_board):
    return 1 / (1 / r_pu + 1 / r_pu_board)


@fig("c_line", "pF", group="BUS", section=BUS, prints="up", rises_with=["c_bus", "c_board"])
def _(c_bus, c_board):
    return c_bus + c_board


# The rising edge between the input levels, with the bus's pull-ups alone: the Teensy's
# own pull-up in parallel only makes it faster.
@fig("t_r_bus", "ns", group="BUS", section=BUS, prints="up",
     rises_with=["r_pu", "r_pu_board", "c_bus", "c_board", "vih_frac"], falls_with=["vil_frac"])
def _(r_bus, c_line, vil_frac, vih_frac):
    return r_bus * c_line * ln((1 - vil_frac) / (1 - vih_frac))


# The same edge with R5 and R6 as the only pull-ups and the motherboard as the whole load.
@fig("t_r_board", "ns", group="BUS", section=BUS, prints="up",
     rises_with=["r_pu_board", "c_board", "vih_frac"], falls_with=["vil_frac"])
def _(r_pu_board, c_board, vil_frac, vih_frac):
    return r_pu_board * c_board * ln((1 - vil_frac) / (1 - vih_frac))


# What a device sinks to hold a line low: every pull-up on the bus in parallel, the
# Teensy's own included, from the highest rail to zero volts.
@fig("r_sink", "kΩ", group="BUS", section=BUS, prints="down",
     rises_with=["r_pu", "r_pu_board", "r_pullup"])
def _(r_bus, r_pullup):
    return 1 / (1 / r_bus + 1 / r_pullup)


@fig("i_sink", "mA", group="BUS", section=BUS, prints="up", rises_with=["v_3v3_max"],
     falls_with=["r_pu", "r_pu_board", "r_pullup"])
def _(v_3v3_max, r_sink):
    return v_3v3_max / r_sink


# ===========================================================================
# the read, which the I²C bus runs on Wire, docs/firmware/io/port-expander.md
# ===========================================================================
MODEL.uses("f_scl", of="firmware", group="READ", section=READ, stated=True)
MODEL.uses("n_bits_byte", of="magnetic-rotary", group="READ", section=READ, stated=True)
dec("n_bytes_read", 6, "bytes", src="the bytes on the bus in one read: the address to write, "
    "INTF's register address, the address again to read, then INTF, INTCAP and GPIO, which "
    "follow one another while IOCON keeps SEQOP at its power-on 0, DS20001919F, Register 1-6",
    group="READ", section=READ, stated=True)
dec("n_conds", 3, "", src="the START, the repeated START and the STOP of one read, each taking "
    "about one clock period", stated=False)
MODEL.uses("t_read", of="magnetic-rotary", group="READ", section=READ, stated=True)
MODEL.uses("t_tick", of="firmware")
MODEL.uses("n_capture", of="firmware")   # what a Change callback may capture
dec("t_poll", 100, "ms", src="how often the driver tick requests a read without INT, which "
    "repeats a read that failed and so left INT low", group="READ", section=READ, stated=True)


# The bytes, and the START, repeated START and STOP, each about one clock period.
@fig("t_read_exp", "µs", group="READ", section=READ, prints="up",
     rises_with=["n_bytes_read", "n_bits_byte", "n_conds"], falls_with=["f_scl"])
def _(n_bytes_read, n_bits_byte, n_conds, f_scl):
    return (n_bytes_read * n_bits_byte + n_conds) / f_scl


@fig("poll_share", "%", group="READ", section=READ, prints="up",
     rises_with=["n_bytes_read", "n_bits_byte", "n_conds"], falls_with=["f_scl", "t_poll"])
def _(t_read_exp, t_poll):
    return t_read_exp / t_poll


# ===========================================================================
# the supply, from the Teensy's 3V3
# ===========================================================================
ds("vdd_min_exp", 1.8, "V", sheet=EXP, src="DS20001919F, page 1: operating voltage from 1.8 V "
   "for I²C at 100 kHz", group="SUPPLY", section=SUPPLY, stated=True)
ds("vdd_max_exp", 5.5, "V", sheet=EXP, src="DS20001919F, page 1: operating voltage up to 5.5 V",
   group="SUPPLY", section=SUPPLY, stated=True)
ds("i_dd_board", 1, "mA", sheet=EXP, src="DS20001919F, D004: supply current I_DD maximum, at "
   "SCL = 1 MHz", group="SUPPLY", section=SUPPLY, stated=True)
ds("i_pu_gp", 115, "µA", sheet=EXP, src="DS20001919F, D070: GPIO weak pull-up current IPU "
   "maximum, at VDD = 5 V with the pin at VSS; the sheet gives no figure at 3.3 V, where the "
   "current is smaller", group="SUPPLY", section=SUPPLY, stated=True)
ds("n_gp", 8, "", sheet=EXP, src="DS20001919F, Table 1-1: the inputs GP0 to GP7")
ds("r_gppu", 100, "kΩ", sheet=EXP, src="DS20001919F, 1.6.7: the internal pull-up a set GPPU "
   "bit switches on at an input", group="Signal", stated="loose")
dec("c1", 100, "nF", src="C1, the 100 nF ceramic the build decouples every IC with", group=SUPPLY,
    stated="loose")
dec("d_c1", 10, "mm", src="the most C1 may sit from VDD on perfboard, as C6 on the IR "
    "mainboard may", group=SUPPLY, stated="loose")


@fig("i_inputs", "mA", group="SUPPLY", section=SUPPLY, prints="up", rises_with=["i_pu_gp", "n_gp"])
def _(i_pu_gp, n_gp):
    return i_pu_gp * n_gp


# R5 and R6 while SDA and SCL are both low.
@fig("i_r_bus", "mA", group="SUPPLY", section=SUPPLY, prints="up",
     rises_with=["v_3v3_max"], falls_with=["r_pu_board"])
def _(v_3v3_max, r_pu_board):
    return 2 * v_3v3_max / r_pu_board


@fig("i_board", "mA", group="SUPPLY", section=SUPPLY, prints="up",
     rises_with=["i_dd_board", "i_pu_gp", "n_gp", "v_3v3_max"], falls_with=["r_pu_board"])
def _(i_dd_board, i_inputs, i_r_bus):
    return i_dd_board + i_inputs + i_r_bus


MODEL.draws("i_board", pool=TEENSY_RAIL)   # U1 and R5, R6, fed from the Teensy's 3V3

_I = MODEL.invariant
_I("a rising edge on the shared bus stays inside Standard-mode",
   lambda v: v.t_r_bus <= v.t_r_std)
_I("a rising edge stays inside Standard-mode with R5 and R6 alone on the motherboard's load",
   lambda v: v.t_r_board <= v.t_r_std)
_I("each bus line stays under the load U1 allows on SDA", lambda v: v.c_line <= v.c_b_max)
_I("a device holding a line low sinks no more than an I²C output has to",
   lambda v: v.i_sink <= v.i_ol_std)
_I("U1 runs inside its supply range on the Teensy's 3V3",
   lambda v: v.vdd_min_exp <= v.v_3v3_min and v.v_3v3_max <= v.vdd_max_exp)
_I("a read of the rotary sensor and a read of U1 fit into one tick together, so the rotary "
   "sensor's read ends before its next request even when it waits for U1's",
   lambda v: v.t_read + v.t_read_exp < v.t_tick)
_I("the driver tick's read comes on whole ticks", lambda v: (v.t_poll / v.t_tick).raw.is_integer())

# ===========================================================================
# wiring
# ===========================================================================
# SDA and SCL are Wire's, which docs/pin-assignment.md gives to the rotary sensor and to the
# port expander. The nets merge with the rotary sensor's, which bring the Teensy's own pull-up.
_R56 = "design.md, R5 on SDA and R6 on SCL, 4.7 kΩ to the motherboard's 3V3 rail, the Teensy's"
MODEL.net("SDA",
          drives("U1 SDA", "open-drain", src="UM10204, every I²C device drives SDA open-drain"),
          pull("R5", TEENSY_RAIL, src=_R56),
          teensy="SDA on Wire")
MODEL.net("SCL",
          reads("U1 SCL", src="DS20001919F, Table 1-1: SCL is U1's serial clock input"),
          pull("R6", TEENSY_RAIL, src=_R56),
          teensy="SCL on Wire")
MODEL.net("INT",
          drives("U1 INT", "push-pull", rail=TEENSY_RAIL,
                 src="DS20001919F, Register 1-6: IOCON at POR has ODR = 0 and INTPOL = 0, so "
                     "INT drives push-pull and active low"),
          teensy="Interrupt from the port expander")

MODEL.draws(pool="driver tick")      # a read requested every t_poll
