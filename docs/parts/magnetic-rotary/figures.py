"""The figures of the rotary sensor on the seal rod, as inputs plus formulas.

Run with:  python tools/figcheck.py docs/parts/magnetic-rotary/figures.py

Every input names where it comes from. Every derived quantity is a function
whose parameter names are its dependencies, so `--graph <key>` answers what a
value feeds and what it rests on.
"""
import pathlib

from figcheck import Model, Q, ln

HERE = pathlib.Path(__file__).resolve().parent
MODEL = Model("magnetic-rotary", HERE / "design.md", section=None, until="## Sources",
              drawings=[HERE / "magnetic-rotary-schematic.svg", HERE / "magnetic-rotary-assembly.svg"],
              documents=[HERE.parents[2] / "firmware" / "magnetic-rotary.md"])

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
dec("r_pu_teensy", 22, "kΩ", src="the pad pull-up Wire switches on, IOMUXC_PAD_PUS(3) in "
    "PJRC's WireIMXRT.cpp", stated=False)
asm("c_bus", 200, "pF", src="the capacitance of each bus line, taken for the module's lead "
    "and a lead to the display's touch controller together; no measurement",
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
dec("t_tick", 5, "ms", src="the period of the driver's IntervalTimer, which collects one "
    "read and starts the next", group="READ", section=READ, stated=True)
ds("n_steps", 4096, "steps", sheet=SENSOR, src="the 12-bit resolution, 4096 positions per "
   "turn", group="READ", section=READ, stated=True)
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
dec("v_3v3", 3.3, "V", src="the Teensy's 3.3 V rail, which feeds the module's VCC, PJRC pin "
    "assignment card 11a rev4", group="SUPPLY", section=SUPPLY, stated=True)
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
    ("5000", "the tick in microseconds, as the driver writes it"),
    ("12 bits", "the AS5600's resolution, RES in the datasheet's system specifications, "
                "which gives the 4096 steps"),
    ("5", "the GPIO function in a pin's mux register, which recover() writes as Wire's "
          "force_clock() does in PJRC's WireIMXRT.cpp"),
]:
    MODEL.aside(_text, _why)
