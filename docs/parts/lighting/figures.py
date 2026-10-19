"""The figures of the lighting's level shifter, data lines, supply and frames, as inputs plus formulas.

Run with:  python tools/figcheck.py docs/parts/lighting/figures.py

It governs the Level shifting, Signal wiring and Supply sections of the lighting design,
the lighting schematic and the lamps component's document. Every input names where it
comes from. Every derived quantity is a function whose parameter names are its dependencies, so
`--graph <key>` answers what a value feeds and what it rests on.
"""
import pathlib

from math import inf

from figcheck import Model, Q, ln, reads

HERE = pathlib.Path(__file__).resolve().parent
MODEL = Model("lighting", HERE / "design.md", section="### Level shifting",
              until="## Teensy pins",
              drawings=[HERE / "lighting-schematic.svg"],
              documents=[HERE.parents[1] / "firmware" / "components" / "lamps.md"])

ds = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="datasheet", **kw)
dec = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="decision", **kw)
asm = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="assumed", **kw)
fig = MODEL.derived

BUFFER = "TI CD74HCT125 datasheet, SCHS415A"

SHIFT = "Level shifting"
BUFFERS = "One CD74HCT125E"
SERIES = "A series resistor sits in each data line"
SUPPLY = "Supply"

# ===========================================================================
# the Teensy's pin and the buffer's supply
# ===========================================================================
MODEL.uses("v_3v3", of="teensy-4.1", group=SHIFT, stated="loose")
MODEL.uses("v_oh_drop", of="teensy-4.1")
MODEL.uses("v_ol_max", of="teensy-4.1")
MODEL.uses("i_pin_max", of="teensy-4.1")
MODEL.uses("v_3v3_max", of="teensy-4.1")
MODEL.input("kit_tol", 5, "%", kind="datasheet", src="the justPi resistor kit JUS-19729 every resistor comes from, Botland product page: tolerance 5 %", stated=False)
dec("v_rail", 5, "V", src="the machine's rail, which feeds the buffers and, through J-LA to J-LF, the segments",
    group=SHIFT, stated="loose")
ds("vih_led_frac", 70, "%", stated=False, src="XINGLIGHT XL-0807RGBC-WS2812B datasheet, "
   "electrical characteristics, input signal threshold VIH at least 0.7 × VDD at VDD = 5 V; "
   "the older WS2812B sheets state the same")
ds("vcc_min", 4.5, "V", src=f"{BUFFER}, 5.3 Recommended Operating Conditions, supply "
   "voltage VCC minimum", group="VCC, pin 14", section=BUFFERS, stated=True)
ds("vcc_max", 5.5, "V", src=f"{BUFFER}, 5.3 Recommended Operating Conditions, supply "
   "voltage VCC maximum", group="VCC, pin 14", section=BUFFERS, stated=True)
ds("c_bypass", 100, "nF", src=f"{BUFFER}, 8.3 Power Supply Recommendations, a 0.1 µF "
   "capacitor at each VCC terminal, as close to it as possible",
   group="Decoupling", section=BUFFERS, stated=True)
asm("place_bypass", 10, "mm", src="how close each 100 nF sits to its buffer's supply pin, "
    "which keeps the loop through the capacitor short; no datasheet states one",
    group="Decoupling", section=BUFFERS, stated=True)

# The LED reads a high only above a fraction of its own supply.
@fig("vih_led", "V", group=SHIFT, stated="loose", prints="up",
     rises_with=["vih_led_frac", "v_rail"])
def _(vih_led_frac, v_rail):
    return vih_led_frac * v_rail


# ===========================================================================
# the buffer's output
# ===========================================================================
ds("i_oh_test", 4, "mA", src=f"{BUFFER}, 5.5 Electrical Characteristics, the output "
   "current VOH is specified at for the bus-driver load", group=BUFFERS, stated=False)
ds("v_oh_out", 3.7, "V", src=f"{BUFFER}, 5.5 Electrical Characteristics, VOH minimum at "
   "IOH = -4 mA and VCC = 4.5 V, -55 °C to 125 °C", group=BUFFERS, stated="loose")
dec("r_out", 330, "Ω", src="R4 to R6, the series resistor at each buffer output, chosen so "
    "a data wire that meets 5 V or ground holds the buffer's output inside its continuous output "
    "current and the resistor inside half its rating: the 330 Ω of the resistor kit, the "
    "smallest of its values that does both", group=SERIES, stated="loose")

# ===========================================================================
# the series resistor in each data line
# ===========================================================================
ds("i_out_max", 35, "mA", src=f"{BUFFER}, 5.1 Absolute Maximum Ratings, continuous output "
   "current IO; 5.3 states no output current", group=SERIES, stated="loose")
MODEL.input("p_kit", 0.25, "W", kind="datasheet", src="the same kit and page: power rating 0.25 W", group=SERIES, stated="loose")
ds("r_out_best_min", 47, "Ω", src="PJRC, OctoWS2811 page, Signal Quality: values of 47 to "
   "220 Ω are probably best for the series resistor, depending on the wire", group=SERIES,
   stated="loose")
ds("r_out_best_max", 220, "Ω", src="the same page, the upper end of that range", group=SERIES,
   stated="loose")
MODEL.input("p_derate", 50, "%", kind="decision", src="the derating target for resistor power in the design-review skill, at most half the rating", stated=False)
MODEL.uses("i_coil", of="solenoid")


# A data wire that meets ground while the buffer drives high, or 5 V while it drives
# low, leaves the resistor alone between the rail and the output. The rail is taken
# at the highest the buffer accepts, the resistor at its low end, and the output's own
# resistance is not counted.
@fig("i_fault", "mA", group=SERIES, stated="loose", prints="up",
     rises_with=["vcc_max", "kit_tol"], falls_with=["r_out"])
def _(vcc_max, r_out, kit_tol):
    return vcc_max / (r_out * (1 - kit_tol))


@fig("p_fault", "W", stated=False, prints="up",
     rises_with=["vcc_max", "kit_tol"], falls_with=["r_out"])
def _(vcc_max, i_fault):
    return vcc_max * i_fault


# The resistor's floor: the least value whose low end holds the fault inside the output's
# rating, and the least whose low end carries it at the share of its rating every
# resistor is held to.
@fig("r_out_min", "Ω", stated=False, prints="up",
     rises_with=["vcc_max", "kit_tol"], falls_with=["i_out_max"])
def _(vcc_max, i_out_max, kit_tol):
    return vcc_max / (i_out_max * (1 - kit_tol))


@fig("r_out_min_p", "Ω", stated=False, prints="up",
     rises_with=["vcc_max", "kit_tol"], falls_with=["p_kit", "p_derate"])
def _(vcc_max, p_kit, p_derate, kit_tol):
    return vcc_max * vcc_max / (p_kit * p_derate * (1 - kit_tol))


# ===========================================================================
# the input resistor
# ===========================================================================
dec("r_in", 2.2, "kΩ", src="the resistor from each Teensy pin to its buffer input, chosen "
    "so the pin's current into the input's clamp diode stays far under its 4 mA while the "
    "machine's 5 V is off, and so the edge it slows leaves the data timing near its own: "
    "the 2.2 kΩ of the resistor kit",
    group="Used inputs", section=BUFFERS, stated=True)
asm("place_in", 10, "mm", src="how close the resistor sits to its buffer input, which keeps "
    "the wiring between them short; no datasheet states one",
    group="Used inputs", section=BUFFERS, stated=True)
ds("iik", 20, "mA", stated=False, src=f"{BUFFER}, 5.1 Absolute Maximum Ratings, input clamp "
   "current IIK at VI > VCC + 0.5 V; 7.3.3 places a clamp diode from each input to VCC")
ds("vih", 2, "V", stated=False, src=f"{BUFFER}, 5.3 Recommended Operating Conditions, "
   "high-level input voltage VIH minimum at VCC = 4.5 V to 5.5 V")
ds("vil", 0.8, "V", stated=False, src=f"{BUFFER}, 5.3 Recommended Operating Conditions, "
   "low-level input voltage VIL maximum at VCC = 4.5 V to 5.5 V")
ds("c_in", 10, "pF", stated=False, src=f"{BUFFER}, 5.5 Electrical Characteristics, input "
   "capacitance Ci maximum")
asm("c_stray", 5, "pF", stated=False, src="the wiring from the resistor to its buffer input "
    "inside the 10 mm placement, estimated generously and not measured")
ds("t_tran_max", 400, "ns", src=f"{BUFFER}, 5.3 Recommended Operating Conditions, input "
   "transition time tt maximum at VCC = 5.5 V, the shorter of its two figures",
   group=BUFFERS, stated=False)
LED = "WS2812B datasheet, older revision, data transfer time; the strip's own sheet is an open point"
ds("t0h", 0.4, "µs", src=f"{LED}: T0H, the high of a 0", group=BUFFERS, stated=False)
ds("t1h", 0.8, "µs", src=f"{LED}: T1H, the high of a 1", group=BUFFERS, stated=False)
ds("t_bit_tol", 150, "ns", src=f"{LED}: the tolerance on T0H and T1H", group=BUFFERS,
   stated=False)
asm("edge_low", 10, "%", stated=False, src="the lower mark TI's logic sheets time an edge "
    "from; SCHS415A draws its marks in a figure the text search cannot read")
asm("edge_high", 90, "%", stated=False, src="the upper mark, taken as edge_low is")


# With the machine's 5 V off, the input's clamp diode holds the input at the
# dead rail. The pin's high then drives the resistor into that diode, both
# taken at 0 V.
@fig("i_clamp", "mA", group="Used inputs", section=BUFFERS, prints="up",
     rises_with=["v_3v3"], falls_with=["r_in"])
def _(v_3v3, r_in):
    return v_3v3 / r_in


@fig("v_oh_min", "V", stated=False, rises_with=["v_3v3"], falls_with=["v_oh_drop"])
def _(v_3v3, v_oh_drop):
    return v_3v3 - v_oh_drop


# The resistor into the input and the wiring beside it is a single pole.
@fig("tau_in", "ns", stated=False, rises_with=["r_in", "c_in", "c_stray"])
def _(r_in, c_in, c_stray):
    return r_in * (c_in + c_stray)


# A single-pole step crosses the two marks ln((1 - low) / (1 - high)) time
# constants apart.
@fig("t_edge", "ns", group=BUFFERS, stated=False, prints="up",
     rises_with=["r_in", "c_in", "c_stray", "edge_high"], falls_with=["edge_low"])
def _(tau_in, edge_low, edge_high):
    return ln((1 - edge_low.raw) / (1 - edge_high.raw)) * tau_in


# The buffer switches where its input crosses its threshold, somewhere between
# VIL and VIH. A high from the level H after a low at L crosses a threshold T
# tau ln((H - L) / (H - T)) after the rising edge and tau ln((H - L) / (T - L))
# after the falling one, so the buffer's high lasts tau ln((H - T) / (T - L))
# longer than the pin's. It grows most with the highest high, the lowest
# threshold and the highest low.
@fig("t_longer", "ns", group=BUFFERS, stated=False, prints="up",
     rises_with=["v_3v3", "v_ol_max", "r_in", "c_in", "c_stray"], falls_with=["vil"])
def _(tau_in, v_3v3, vil, v_ol_max):
    return tau_in * ln((v_3v3 - vil) / (vil - v_ol_max))


# It shrinks most with the lowest high, the highest threshold and a low at
# ground.
@fig("t_shorter", "ns", group=BUFFERS, stated=False, prints="up",
     rises_with=["vih", "v_oh_drop", "r_in", "c_in", "c_stray"], falls_with=["v_3v3"])
def _(tau_in, v_oh_min, vih):
    return tau_in * ln(vih / (v_oh_min - vih))


# R1 to R3's window. The least value whose low end holds the pin inside its 4 mA with the
# 5 V off, the rail at its high end, and the most whose high end still stretches a high by
# less than the LED's timing tolerance, the edge the window rests on.
@fig("r_in_min", "kΩ", stated=False, prints="up",
     rises_with=["v_3v3_max", "kit_tol"], falls_with=["i_pin_max"])
def _(v_3v3_max, i_pin_max, kit_tol):
    return v_3v3_max / (i_pin_max * (1 - kit_tol))


@fig("r_in_max", "kΩ", stated=False, prints="down",
     rises_with=["t_bit_tol", "vil"],
     falls_with=["c_in", "c_stray", "v_3v3_max", "v_ol_max", "kit_tol"])
def _(t_bit_tol, c_in, c_stray, v_3v3_max, vil, v_ol_max, kit_tol):
    return t_bit_tol / ((c_in + c_stray) * ln((v_3v3_max - vil) / (vil - v_ol_max))
                        * (1 + kit_tol))


asm("c_data", 150, "pF", src="the data wire from the board to a chain's first LED together with "
    "that LED's input, estimated generously and not measured", group=SERIES, stated="loose")
ds("vil_led_frac", 30, "%", stated=False, src="XINGLIGHT XL-0807RGBC-WS2812B datasheet, "
   "electrical characteristics, input signal threshold VIL at most 0.3 × VDD at VDD = 5 V; "
   "the older WS2812B sheets state the same")


@fig("vil_led", "V", stated=False, prints="down", rises_with=["vil_led_frac", "v_rail"])
def _(vil_led_frac, v_rail):
    return vil_led_frac * v_rail


# The resistor into the data wire and the first LED's input is a single pole too. The LED's
# high starts where the edge crosses its high level and ends where it falls through its low
# level, so the high arrives shorter by tau ln(VIL / (VOH - VIH)), taken at the buffer's lowest
# high and with the resistor at its high end.
@fig("t_out_shorter", "ns", group=SERIES, stated="loose", prints="up",
     rises_with=["r_out", "kit_tol", "c_data", "vil_led_frac", "vih_led_frac", "v_rail"],
     falls_with=["v_oh_out"])
def _(r_out, kit_tol, c_data, vil_led, v_oh_out, vih_led):
    if (v_oh_out - vih_led).v <= 0:    # a high that never reaches the LED's high level
        return Q.of(inf, "ns")
    return r_out * (1 + kit_tol) * c_data * ln(vil_led / (v_oh_out - vih_led))


# The most the resistor may be: at its high end the two edges together still shorten a high
# by less than the LED's timing tolerance.
@fig("r_out_max", "Ω", stated=False, prints="down",
     rises_with=["t_bit_tol", "v_oh_out"],
     falls_with=["c_data", "vil_led_frac", "kit_tol", "r_in"])
def _(t_bit_tol, t_shorter, c_data, vil_led, v_oh_out, vih_led, kit_tol):
    if (v_oh_out - vih_led).v <= 0:    # no resistor passes a high the LED cannot read
        return Q.of(0, "Ω")
    return ((t_bit_tol - t_shorter)
            / (c_data * ln(vil_led / (v_oh_out - vih_led)) * (1 + kit_tol)))


# ===========================================================================
# the supply through J-PWR
# ===========================================================================
dec("i_supply_3a", 3, "A", src="the smaller supply the machine is designed for; design.md, "
    "Current ceiling", group=SUPPLY, stated="loose")
dec("i_supply_6a", 6, "A", src="the larger supply the machine is designed for; design.md, "
    "Current ceiling", group=SUPPLY, stated="loose")
dec("i_budget", 2.0, "A", src="the lighting's cap with the 3 A supply, which the firmware holds "
    "with FastLED's setMaxPowerInVoltsAndMilliamps; design.md, Current ceiling",
    stated=False)
dec("i_budget_6a", 3.3, "A", src="the lighting's cap with the 6 A supply, kept at 3.3 A: with "
    "the permanent consumers of the power design and two bumpers with the scoop pulling at once "
    "the sum passes the adapter by 0.34 A for at most one pull-in, which the design accepts as it "
    "accepts the 3 A adapter's overrun; design.md, Current ceiling, and the power design's "
    "Adapter section",
    stated=False)
dec("len_jpwr", 30, "cm", src="the longest J-PWR cable, the length the feed's inductance "
    "is estimated for", group=SUPPLY, stated="loose")
ds("r_awg22", 52.96, "mΩ/m", src="ASTM B258, 22 AWG annealed copper at 20 °C", stated=False)
MODEL.uses("l_feed", of="solenoid")
MODEL.uses("c_one", of="power", group=SUPPLY, stated="loose")   # C2 on each board, the power board's bulk part
dec("n_bulk_light", 2, "", src="C2, one on each of the two lighting boards at J-PWR", stated=False)
MODEL.uses("z_c", of="power", group=SUPPLY, stated="loose")
ds("f_z_bulk", 100, "kHz", src="the frequency the listing states C2's impedance at",
   group=SUPPLY, stated="loose")
asm("place_bulk", 20, "mm", src="how close C2 sits to J-PWR, where the feed arrives; no "
    "datasheet states one", group=SUPPLY, stated="loose")


# One wire of J-PWR, soldered at both ends: its copper alone.
@fig("r_jpwr", "mΩ", stated=False, rises_with=["len_jpwr", "r_awg22"])
def _(len_jpwr, r_awg22):
    return len_jpwr * r_awg22


# Both wires of J-PWR carry the lighting's whole current. The cap holds for all chains
# together, so either board can draw all of it, and each board's figures take the whole cap.
@fig("v_jpwr", "V", stated=False, prints="up",
     rises_with=["i_budget", "len_jpwr", "r_awg22"])
def _(i_budget, r_jpwr):
    return 2 * r_jpwr * i_budget


@fig("v_jpwr_6a", "V", stated=False, prints="up",
     rises_with=["i_budget_6a", "len_jpwr", "r_awg22"])
def _(i_budget_6a, r_jpwr):
    return 2 * r_jpwr * i_budget_6a


# When the LED current steps, the feed's inductance holds its current for a moment,
# and C2 carries the step. The worst step is the whole cap at once. The rail then
# falls by the step across C2's impedance and the feed's characteristic impedance.
@fig("z_step", "mΩ", stated=False, rises_with=["z_c", "l_feed"], falls_with=["c_one"])
def _(z_c, l_feed, c_one):
    return z_c + (l_feed / c_one) ** (1 / 2)


@fig("v_dip", "V", stated=False, prints="up",
     rises_with=["i_budget", "z_c", "l_feed"], falls_with=["c_one"])
def _(i_budget, z_step):
    return i_budget * z_step


@fig("v_dip_6a", "V", stated=False, prints="up",
     rises_with=["i_budget_6a", "z_c", "l_feed"], falls_with=["c_one"])
def _(i_budget_6a, z_step):
    return i_budget_6a * z_step


@fig("v_buffer_min", "V", group=SUPPLY, stated="loose", prints="down",
     rises_with=["v_rail"], falls_with=["i_budget", "len_jpwr", "r_awg22", "z_c", "l_feed"])
def _(v_rail, v_jpwr, v_dip):
    return v_rail - v_jpwr - v_dip


@fig("v_buffer_min_6a", "V", group=SUPPLY, stated="loose", prints="down",
     rises_with=["v_rail"], falls_with=["i_budget_6a", "len_jpwr", "r_awg22", "z_c", "l_feed"])
def _(v_rail, v_jpwr_6a, v_dip_6a):
    return v_rail - v_jpwr_6a - v_dip_6a


# ===========================================================================
# the frame
# ===========================================================================
OBJECTFLED = "ObjectFLED.h in FastLED"
MODEL.uses("n_chain_max", of="firmware")
ds("n_bits_led", 24, "", src="WS2812B datasheet, older revision: composition of 24 bit data, "
   "8 bits each of green, red and blue", stated=False)
ds("n_bits_color", 8, "", src="WS2812B datasheet, older revision: composition of 24 bit data, "
   "G7 to G0, R7 to R0 and B7 to B0; the lamps component's Color holds them as they are",
   stated=False)
MODEL.aside("30 %", "an example of the brightness Color::dim() takes, in lamps.md")
ds("t_bit", 1.25, "µs", src=f"{OBJECTFLED}: TH_TL, the period of one bit, 1250 ns by default; "
   "the WS2812B datasheet's TH+TL", stated=False)
ds("t_latch", 300, "µs", src=f"{OBJECTFLED}: LATCH_DELAY, how long it holds the outputs low "
   "after a frame, 300 µs by default", stated=False)
ds("t_reset", 280, "µs", src="WS2812B datasheet Ver. No. V5: RES, the low time that ends a "
   "frame, more than 280 µs; the older revision asks for more than 50 µs, and the strip's own "
   "sheet gives none", stated=False)
dec("t_frame", 10, "ms", src="the lamps component's frame period, docs/firmware/components/"
    "lamps.md; 100 frames a second, the gist's kFrameUs", stated=False)


@fig("t_chain_out", "ms", stated=False, rises_with=["n_chain_max", "n_bits_led", "t_bit"])
def _(n_chain_max, n_bits_led, t_bit):
    return n_chain_max * n_bits_led * t_bit


@fig("t_frame_out", "ms", stated=False, rises_with=["t_chain_out", "t_latch"])
def _(t_chain_out, t_latch):
    return t_chain_out + t_latch


# ===========================================================================
# requirements
# ===========================================================================
_I = MODEL.invariant
_I("the machine's rail sits in the buffer's supply range",
   lambda v: v.vcc_min <= v.v_rail <= v.vcc_max)
_I("the Teensy's pin alone cannot reach the LED's high input level",
   lambda v: v.v_3v3 < v.vih_led)
_I("the buffer's lowest high clears the LED's high input level",
   lambda v: v.v_oh_out >= v.vih_led)
_I("the Teensy's lowest high clears the buffer's high input level",
   lambda v: v.v_oh_min > v.vih)
_I("the Teensy's highest low stays under the buffer's low input level",
   lambda v: v.v_ol_max < v.vil)
_I("with the machine's 5 V off, the resistor holds each pin inside its 4 mA",
   lambda v: v.i_clamp < v.i_pin_max)
_I("with the machine's 5 V off, the resistor holds the clamp diode inside its rating",
   lambda v: v.i_clamp < v.iik)
_I("the resistor stretches each high by less than the LED's timing tolerance",
   lambda v: v.t_longer < v.t_bit_tol)
_I("the resistor shortens each high by less than the LED's timing tolerance",
   lambda v: v.t_shorter < v.t_bit_tol)
_I("the edge through the resistor stays inside the longest input edge the buffer allows",
   lambda v: v.t_edge < v.t_tran_max)
_I("a data wire that meets 5 V or ground holds the buffer's output inside its rating",
   lambda v: v.i_fault <= v.i_out_max)
_I("the series resistor carries that fault at no more than half its rating",
   lambda v: v.p_fault <= v.p_kit * v.p_derate)
_I("R1 to R3 sit inside their window", lambda v: v.r_in_min <= v.r_in <= v.r_in_max)
_I("the data-line resistor holds a shorted wire inside the buffer's rating, at its low end",
   lambda v: v.r_out_min <= v.r_out)
_I("and carries it at no more than half its rating",
   lambda v: v.r_out_min_p <= v.r_out)
_I("and at its high end leaves the first LED's high inside the timing tolerance",
   lambda v: v.r_out <= v.r_out_max and v.t_shorter + v.t_out_shorter < v.t_bit_tol)

_I("with the 3 A supply, a step of the whole cap leaves the buffers inside their supply range",
   lambda v: v.v_buffer_min >= v.vcc_min)
_I("with the 6 A supply, a step of the whole cap leaves the buffers inside their supply range",
   lambda v: v.v_buffer_min_6a >= v.vcc_min)
_I("ObjectFLED holds the lines low long enough for the LEDs to take over a frame",
   lambda v: v.t_latch > v.t_reset)
_I("a frame has gone out before the next one is handed on, so show() never waits",
   lambda v: v.t_frame_out < v.t_frame)



# ===========================================================================
# wiring
# ===========================================================================
# each chain's pin feeds one buffer input through 2.2 kΩ; the input's clamp diode points to
# the buffer's VCC, so it can sink the pin's current but never raises the pin
for _n in "ABCDEF":
    MODEL.net(f"LED chain {_n}, Teensy side",
              reads("a CD74HCT125E input through 2.2 kΩ",
                    src="design.md, Level shifting: 2.2 kΩ from the Teensy pin to each used input"),
              teensy=f"LED chain {_n}")

MODEL.owns("QuadTimer4", "FastLED's ObjectFLED driver owns channels 0 to 2 of QuadTimer4, "
                         "ObjectFLED.h in FastLED")
