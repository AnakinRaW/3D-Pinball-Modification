"""The figures of the rotary sensor on the seal rod, as inputs plus formulas.

Run with:  python tools/figcheck.py docs/parts/magnetic-rotary/figures.py

Every input names where it comes from. Every derived quantity is a function
whose parameter names are its dependencies, so `--graph <key>` answers what a
value feeds and what it rests on.
"""
import pathlib

from figcheck import Model, Q, TEENSY_RAIL, drives, ln, pull, reads

HERE = pathlib.Path(__file__).resolve().parent
MODEL = Model("magnetic-rotary", HERE / "design.md", section=None, until="## Sources",
              drawings=[HERE / "magnetic-rotary-schematic.svg", HERE / "magnetic-rotary-assembly.svg"],
              documents=[HERE.parents[2] / "firmware" / "drivers" / "magnetic-rotary.md"])

ds = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="datasheet", **kw)
dec = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="decision", **kw)
asm = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="assumed", **kw)
fig = MODEL.derived

SENSOR = "AS5600-ams.pdf"
BUS = "The bus"
READ = "The read"
SUPPLY = "The supply"

# ===========================================================================
# the bus
# ===========================================================================
dec("r_pu", 4.7, "kΩ", src="R4 and R5 on the Grove module, SDA and SCL to the module's VCC, "
    "from Seeed's Eagle schematic of the 101020692; the weakest pull-up on the bus, the "
    "Teensy's own being stronger in parallel", group="BUS", section=BUS, stated=True)
MODEL.uses("r_pullup", of="teensy-4.1")
asm("c_bus", 200, "pF", src="the capacitance of each bus line, taken for the module's cable with "
    "margin; no measurement",
    group="BUS", section=BUS, stated=True)
ds("vil_frac", 0.3, "", sheet=SENSOR, src="I²C electrical specification, logic low input "
   "voltage VIL, maximum 0.3 x VDD", group="BUS", section=BUS, stated=True)
ds("vih_frac", 0.7, "", sheet=SENSOR, src="I²C electrical specification, logic high input "
   "voltage VIH, minimum 0.7 x VDD", group="BUS", section=BUS, stated=True)
ds("t_r_std", 1000, "ns", sheet="UM10204-NXP.pdf", src="Table 10, rise time of both SDA "
   "and SCL signals, Standard-mode, maximum; the specification the AS5600 datasheet names for "
   "sizing the pull-up", group="BUS", section=BUS, stated=True)


# The rising edge between the input thresholds, with the module's pull-up alone:
# the Teensy's own pull-up in parallel only makes it faster.
@fig("t_r", "ns", group="BUS", section=BUS, prints="up",
     rises_with=["r_pu", "c_bus", "vih_frac"], falls_with=["vil_frac"])
def _(r_pu, c_bus, vil_frac, vih_frac):
    return r_pu * c_bus * ln((1 - vil_frac) / (1 - vih_frac))


# ===========================================================================
# the read
# ===========================================================================
dec("f_scl", 100, "kHz", src="the bus clock, Standard-mode, the rate Wire's begin() sets "
    "with setClock(100000)", group="READ", section=READ, stated=True)
dec("n_bytes", 3, "", src="the bytes on the bus in one read: the address, and the two bytes "
    "of RAW ANGLE, whose pointer the AS5600 keeps between reads", stated=False)
dec("n_bits_byte", 9, "bits", src="the clock periods one byte takes on the bus, eight data "
    "bits and the acknowledge, UM10204", group="READ", section=READ, stated=True)
MODEL.uses("t_tick", of="firmware", group="READ", section=READ, stated=True)
ds("n_steps", 4096, "steps", sheet=SENSOR, src="the 12-bit resolution, 4096 positions per "
   "turn", group="READ", section=READ, stated=True)
dec("n_bytes_ptr", 2, "", src="the bytes a library puts on the bus to set the register pointer "
    "before each read, the address and the register, as readReg2() in RobTillaart/AS5600 sends them", stated=False)
dec("t_fail", 1, "s", src="how long no read may work before the sensor counts as failed, "
    "long enough for its own recovery attempts and short enough for a game to react", stated=False)
MODEL.uses("t_monitor", of="firmware")
MODEL.as_written("t_fail_ms", of="t_fail", unit="ms")
dec("n_still", 4, "", src="the ticks the position has to stay within n_dead after a movement "
    "before the driver reports the rod still, an estimate that keeps a seal coasting out of a "
    "spin from counting as stopped", stated=False)
dec("n_dead", 12, "steps", src="how far the position has to lie from the last published one "
    "before the driver reports again, chosen at about one degree to ride out the rod's play "
    "and the machine's vibration, which nothing in the repository measures", stated=False)


# START and STOP take about one clock period each, hence the two.
@fig("t_read", "µs", group="READ", section=READ, prints="up",
     rises_with=["n_bytes", "n_bits_byte"], falls_with=["f_scl"])
def _(n_bytes, n_bits_byte, f_scl):
    return (n_bytes * n_bits_byte + 2) / f_scl


@fig("bus_share", "%", group="READ", section=READ, prints="up",
     rises_with=["n_bytes", "n_bits_byte"], falls_with=["f_scl", "t_tick"])
def _(t_read, t_tick):
    return t_read / t_tick


# A library read: the pointer write and then the read, each with its own START and STOP,
# with the processor waiting in Wire for both.
@fig("t_lib_read", "µs", stated=False, prints="up",
     rises_with=["n_bytes_ptr", "n_bytes", "n_bits_byte"], falls_with=["f_scl"])
def _(n_bytes_ptr, n_bits_byte, f_scl, t_read):
    return (n_bytes_ptr * n_bits_byte + 2) / f_scl + t_read


@fig("lib_share", "%", stated=False, prints="up",
     rises_with=["n_bytes_ptr", "n_bytes", "n_bits_byte"], falls_with=["f_scl", "t_tick"])
def _(t_lib_read, t_tick):
    return t_lib_read / t_tick


# Two readings a tick apart are unwrapped modulo one turn, which holds while the
# rod turns less than half a turn between them.
@fig("f_turn_max", "Hz", group="READ", section=READ, prints="down", falls_with=["t_tick"])
def _(t_tick):
    return 1 / (2 * t_tick)




# How long the rod has to rest before the driver reports it still.
@fig("t_still", "ms", stated=False, rises_with=["n_still", "t_tick"])
def _(n_still, t_tick):
    return n_still * t_tick


# ===========================================================================
# the magnet
# ===========================================================================
dec("gap_min", 0.5, "mm", src="the near end of the gap between magnet and AS5600 that the "
    "Seeed wiki gives for the 101020692, adopted", stated=False)
dec("gap_max", 3, "mm", src="the far end of that gap, adopted from the Seeed wiki", stated=False)
dec("d_magnet", 6, "mm", src="the magnet diameter the AS5600 datasheet quotes its axis "
    "displacement for, adopted", stated=False)


# ===========================================================================
# the supply
# ===========================================================================
MODEL.uses("v_3v3", of="teensy-4.1", group="SUPPLY", section=SUPPLY, stated=True)
dec("v_5v", 5, "V", src="the machine's 5 V, which must not feed the module's VCC because "
    "its pull-ups follow VCC", stated=False)
ds("vdd_min", 3.0, "V", sheet=SENSOR, src="Operating conditions, VDD3V3 in 3.3 V mode, "
   "minimum", group="SUPPLY", section=SUPPLY, stated=True)
ds("vdd_max", 3.6, "V", sheet=SENSOR, src="Operating conditions, VDD3V3 in 3.3 V mode, "
   "maximum", group="SUPPLY", section=SUPPLY, stated=True)
ds("i_dd", 6.5, "mA", sheet=SENSOR, src="Operating conditions, supply current in NOM, "
   "PM = 00, always on", group="SUPPLY", section=SUPPLY, stated=True)


# ===========================================================================
# requirements
# ===========================================================================
_I = MODEL.invariant
_I("a rising edge on the bus stays inside the Standard-mode limit",
   lambda v: v.t_r < v.t_r_std)
_I("a read ends well inside its tick",
   lambda v: v.t_read < v.t_tick)
_I("the Teensy's rail sits in the AS5600's 3.3 V window",
   lambda v: v.vdd_min < v.v_3v3 < v.vdd_max)
_I("the report threshold stays a small part of a turn",
   lambda v: v.n_dead < v.n_steps)

for _text, _why in [
    ("0.25 mm", "how far the rod's axis may sit from the package centre with a 6 mm magnet, "
                "AS5600 datasheet, a mounting figure"),
    ("12 bits", "the AS5600's resolution, RES in the datasheet's system specifications, "
                "which gives the 4096 steps"),
    ("5", "the GPIO function in a pin's mux register, which recover() writes as Wire's "
          "force_clock() does in PJRC's WireIMXRT.cpp"),
]:
    MODEL.aside(_text, _why)


# ===========================================================================
# wiring
# ===========================================================================
# the module's only supply is VCC, which this build feeds from the Teensy's 3V3, so its
# pull-ups sit at or below the Teensy's rail
_PU = ("design.md, the module's pull-ups R4 and R5, 4.7 kΩ; the module runs from VCC alone, "
       "which comes from the Teensy's 3V3")
_WIRE = "firmware/drivers/magnetic-rotary.md, Wire.begin() sets up the pins and their pull-ups"
MODEL.net("SDA",
          drives("AS5600 SDA", "open-drain", src="UM10204, every I²C device drives SDA open-drain"),
          pull("the module's pull-up on SDA", TEENSY_RAIL, src=_PU),
          pull("the Teensy's internal pull-up", TEENSY_RAIL, src=_WIRE),
          teensy="SDA to the rotary sensors")
MODEL.net("SCL",
          reads("AS5600 SCL", src="design.md, the AS5600's input levels on the bus"),
          pull("the module's pull-up on SCL", TEENSY_RAIL, src=_PU),
          pull("the Teensy's internal pull-up", TEENSY_RAIL, src=_WIRE),
          teensy="SCL to the rotary sensors")

MODEL.owns("Wire", "the driver runs the LPI2C1 controller itself after begin(), "
                   "firmware/drivers/magnetic-rotary.md")
MODEL.draws("i_dd", pool=TEENSY_RAIL)   # the module, fed from the Teensy's 3V3
MODEL.draws(pool="driver tick")      # one read collected and the next started on every tick
MODEL.draws(pool="device monitor")
