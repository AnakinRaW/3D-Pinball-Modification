"""The figures of the rotary sensor on the seal rod, as inputs plus formulas."""
from figcheck import *

MODEL = Model.at(__file__, "magnetic-rotary",
                 drawings=["magnetic-rotary-schematic.svg", "magnetic-rotary-assembly.svg"],
                 documents=["../../firmware/drivers/magnetic-rotary.md"])
fig, ds, gr, msr, asm, dec = MODEL.declarers()

SENSOR = "AS5600-ams.pdf"
BUS = "Bus"
READ = "Read"
SUPPLY = "Supply"

# -- the bus ------------------------------------------------------------------
with MODEL.block("BUS", section=BUS):
    dec("r_pu", 4.7, "kΩ", src="R4 and R5 on the Grove module, SDA and SCL to the module's VCC, "
        "from Seeed's Eagle schematic of the 101020692; the weakest pull-up on the bus, the "
        "Teensy's own being stronger in parallel")
    MODEL.uses("r_pullup", of="teensy-4.1", stated=False)
    dec("l_cable", 20, "cm", src="the cable from the motherboard to the module, at most 20 cm of "
        "26 AWG jumper wire")
    ds("c_cable_m", 42.98, "pF/m", src="each line of a 26 AWG cable to its neighbors: 3M "
       "TS-0342-E, 3770 series flat cable, 26 AWG on 0.050 inch, capacitance unbalanced, between a "
       "conductor and its grounded neighbors on both sides, 13.1 pF/ft [42.98 pF/m]")
    ds("c_pin", 10, "pF", sheet="UM10204-NXP.pdf", src="UM10204 Rev. 7.0, Table 10: Ci, "
       "capacitance for each I/O pin, maximum")
    asm("c_module", 10, "pF", src="the Grove module's traces and connector between the cable and "
        "the AS5600, taken generously; no measurement")
    ds("vil_frac", 0.3, "", sheet=SENSOR, src="I²C electrical specification, logic low input "
       "voltage VIL, maximum 0.3 x VDD")
    ds("vih_frac", 0.7, "", sheet=SENSOR, src="I²C electrical specification, logic high input "
       "voltage VIH, minimum 0.7 x VDD")
    ds("t_r_std", 1000, "ns", sheet="UM10204-NXP.pdf", src="Table 10, rise time of both SDA and "
       "SCL signals, Standard-mode, maximum; the specification the AS5600 datasheet names for "
       "sizing the pull-up")

    # Each bus line on the rotary sensor's side: the cable, the AS5600's pin and the module.
    fig("c_bus", "pF", "l_cable * c_cable_m + c_pin + c_module", "up",
        moves="+l_cable +c_cable_m +c_pin +c_module")

    # The rising edge between the input thresholds, with the module's pull-up alone:
    # the Teensy's own pull-up in parallel only makes it faster.
    fig("t_r", "ns", "r_pu * c_bus * ln((1 - vil_frac) / (1 - vih_frac))", "up",
        moves="+r_pu +l_cable +c_cable_m +c_pin +c_module +vih_frac -vil_frac")

# -- the read -----------------------------------------------------------------
with MODEL.block("READ", section=READ):
    MODEL.uses("f_scl", of="firmware")
    dec("n_bytes", 3, "", src="the bytes on the bus in one read: the address, and the two bytes of "
        "RAW ANGLE, whose pointer the AS5600 keeps between reads", stated=False)
    dec("n_bits_byte", 9, "bits", src="the clock periods one byte takes on the bus, eight data "
        "bits and the acknowledge, UM10204")
    MODEL.uses("t_tick", of="firmware")
    ds("n_steps", 4096, "steps", sheet=SENSOR, src="the 12-bit resolution, 4096 positions per turn")
    dec("n_bytes_ptr", 2, "", src="the bytes a library puts on the bus to set the register pointer "
        "before each read, the address and the register, as readReg2() in RobTillaart/AS5600 sends "
        "them", stated=False)
    dec("t_fail", 1, "s", src="how long no read may work before the sensor counts as failed, long "
        "enough for its own recovery attempts and short enough for a game to react", stated=False)
    MODEL.uses("t_monitor", of="firmware", stated=False)
    MODEL.as_written("t_fail_ms", of="t_fail", unit="ms")
    dec("n_still", 4, "", src="the ticks the position has to stay within n_dead after a movement "
        "before the driver reports the rod still, an estimate that keeps a seal coasting out of a "
        "spin from counting as stopped", stated=False)
    dec("n_dead", 12, "steps", src="how far the position has to lie from the last published one "
        "before the driver reports again, chosen at about one degree to ride out the rod's play "
        "and the machine's vibration, which nothing in the repository measures", stated=False)

    # START and STOP take about one clock period each, hence the two.
    fig("t_read", "µs", "(n_bytes * n_bits_byte + 2) / f_scl", "up",
        moves="+n_bytes +n_bits_byte -f_scl")
    fig("bus_share", "%", "t_read / t_tick", "up", moves="+n_bytes +n_bits_byte -f_scl -t_tick")

    # A library read: the pointer write and then the read, each with its own START and STOP,
    # with the processor waiting in Wire for both.
    fig("t_lib_read", "µs", "(n_bytes_ptr * n_bits_byte + 2) / f_scl + t_read", "up",
        moves="+n_bytes_ptr +n_bytes +n_bits_byte -f_scl", stated=False)
    fig("lib_share", "%", "t_lib_read / t_tick", "up",
        moves="+n_bytes_ptr +n_bytes +n_bits_byte -f_scl -t_tick", stated=False)
    MODEL.uses("t_read_exp", of="port-expander")

    # Two readings are unwrapped modulo one turn, which holds while the rod turns less than half
    # a turn between them. A read of the port expander on the same bus can hold a read back, so
    # two readings lie at most a tick and one expander read apart.
    fig("f_turn_max", "Hz", "1 / (2 * (t_tick + t_read_exp))", "down", moves="-t_tick -t_read_exp")

# How long the rod has to rest before the driver reports it still.
fig("t_still", "ms", "n_still * t_tick", moves="+n_still +t_tick", stated=False)

# -- the magnet ---------------------------------------------------------------
dec("gap_min", 0.5, "mm", src="the near end of the gap between magnet and AS5600 that the Seeed "
    "wiki gives for the 101020692, adopted")
dec("gap_max", 3, "mm", src="the far end of that gap, adopted from the Seeed wiki")
dec("d_magnet", 6, "mm", src="the magnet diameter the AS5600 datasheet quotes its axis "
    "displacement for, adopted")

# -- the supply ---------------------------------------------------------------
with MODEL.block("SUPPLY", section=SUPPLY):
    MODEL.uses("v_3v3", of="teensy-4.1")
    dec("v_5v", 5, "V", src="the machine's 5 V, which must not feed the module's VCC because its "
        "pull-ups follow VCC", stated=False)
    ds("vdd_min", 3.0, "V", sheet=SENSOR, src="Operating conditions, VDD3V3 in 3.3 V mode, minimum")
    ds("vdd_max", 3.6, "V", sheet=SENSOR, src="Operating conditions, VDD3V3 in 3.3 V mode, maximum")
    ds("i_dd", 6.5, "mA", sheet=SENSOR, src="Operating conditions, supply current in NOM, PM = "
       "00, always on")

# -- requirements -------------------------------------------------------------
_I = MODEL.invariant
_I("a rising edge on the bus stays inside the Standard-mode limit", "t_r < t_r_std")
_I("a read ends well inside its tick", "t_read < t_tick")
_I("the Teensy's rail sits in the AS5600's 3.3 V window", "vdd_min < v_3v3 < vdd_max")
_I("the report threshold stays a small part of a turn", "n_dead < n_steps")

for _text, _why in [
    ("0.25 mm", "how far the rod's axis may sit from the package center with a 6 mm magnet, "
                "AS5600 datasheet, a mounting figure"),
    ("12 bits", "the AS5600's resolution, RES in the datasheet's system specifications, "
                "which gives the 4096 steps"),
]:
    MODEL.aside(_text, _why)

# -- wiring -------------------------------------------------------------------
# the module's only supply is VCC, which this build feeds from the Teensy's 3V3, so its
# pull-ups sit at or below the Teensy's rail
_PU = ("design.md, the module's pull-ups R4 and R5, 4.7 kΩ; the module runs from VCC alone, "
       "which comes from the Teensy's 3V3")
_WIRE = "docs/firmware/io/i2c-bus.md, Wire.begin() sets up the pins and their pull-ups"
MODEL.net("SDA",
          drives("AS5600 SDA", "open-drain", src="UM10204, every I²C device drives SDA open-drain"),
          pull("the module's pull-up on SDA", TEENSY_RAIL, src=_PU),
          pull("the Teensy's internal pull-up", TEENSY_RAIL, src=_WIRE),
          teensy="SDA on Wire")
MODEL.net("SCL",
          reads("AS5600 SCL", src="design.md, the AS5600's input levels on the bus"),
          pull("the module's pull-up on SCL", TEENSY_RAIL, src=_PU),
          pull("the Teensy's internal pull-up", TEENSY_RAIL, src=_WIRE),
          teensy="SCL on Wire")

MODEL.draws("i_dd", pool=TEENSY_RAIL)   # the module, fed from the Teensy's 3V3
MODEL.draws(pool="driver tick")      # one read requested on every tick
MODEL.draws(pool="device monitor")
