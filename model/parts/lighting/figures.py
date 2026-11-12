"""The figures of the lighting's level shifter, data lines, supply and frames, as inputs plus
formulas."""
from math import floor, inf

from figcheck import *

MODEL = Model.at(__file__, "lighting",
                 drawings=["lighting-schematic.svg",
                           "../../firmware/components/lamps-architecture.svg"],
                 documents=["../../firmware/components/lamps.md",
                            "../../firmware/drivers/lighting.md",
                            "../../research/Custom/sezo-ws2812b-strip.md"])
fig, ds, gr, msr, asm, dec = MODEL.declarers()
MODEL.skips("../../research/Custom/sezo-ws2812b-strip.md", "Test Code",
            "the bench sketch the strip's currents were measured with")

BUFFER = "TI CD74HCT125 datasheet, SCHS415A"

SHIFT = "Level shifting"
BUFFERS = "One CD74HCT125E"
SERIES = "A series resistor sits in each data line"
SUPPLY = "Supply"
CEILING = "Current ceiling"

# -- the strip, the layout and what the lights draw ---------------------------
ds("strip_len", 2, "m", src="the SEZO WS2812B strip's listing, the reel's length")
ds("strip_width", 2.7, "mm", src="the SEZO WS2812B strip's listing, its width")
asm("len_sides_ceiling", 115.5, "cm", src="the strip the side panels and the diorama ceiling take")
asm("len_other_stock", 45, "cm", src="the strip the other stock positions take, a rough length")
dec("hole_min", 6, "mm", src="the narrowest hole a new playfield light gets")
dec("hole_max", 10, "mm", src="the widest hole a new playfield light gets")
dec("n_leds", 240, "", src="every LED the light layout table plans")
dec("n_perimeter", 158, "", src="the LEDs of the two perimeter chains, A and B, in the light layout table")
dec("n_perimeter_lit_3a", 80, "", src="the perimeter LEDs the 3 A adapter lights, every second of A and "
    "B, 40 each in the light layout table")
dec("n_ceiling", 26, "", src="the LEDs of the ceiling chain, C, in the light layout table", stated=False)
# What the strip draws, measured on a piece cut from it. Each reading takes the whole piece, its
# dark controllers included, so the dark reading comes off every other one.
_BENCH = ("bench: a piece of two LEDs cut from the SEZO strip on a bench supply at 5.1 V, the "
          "multimeter's current range in series with the supply, FastLED at brightness 255 setting "
          "both LEDs to")
dec("n_bench", 2, "", src="the LEDs of the piece the strip's currents were measured on", stated=False)
msr("v_bench", 5.1, "V", src="bench: the bench supply's output the strip ran on, read with the "
    "multimeter", stated=False)
msr("i_bench_green_half", 9.8, "mA", src=f"{_BENCH} CRGB(0, 128, 0), which is FastLED's "
    "CRGB::Green", stated=False)
asm("i_color_usual", 20, "mA", src="the per-color current WS2812B strips are usually budgeted "
    "with, which the measurement replaces", stated=False)
msr("i_bench_dark", 3.5, "mA", src=f"{_BENCH} black", stated=False)
msr("i_bench_red", 14.6, "mA", src=f"{_BENCH} CRGB(255, 0, 0)", stated=False)
msr("i_bench_green", 15.8, "mA", src=f"{_BENCH} CRGB(0, 255, 0)", stated=False)
msr("i_bench_blue", 15.7, "mA", src=f"{_BENCH} CRGB(0, 0, 255)", stated=False)
msr("i_bench_white", 37.5, "mA", src=f"{_BENCH} CRGB(255, 255, 255)", stated=False)
fig("i_ctrl", "mA", "i_bench_dark / n_bench", moves="+i_bench_dark -n_bench", stated=False)
fig("i_red", "mA", "(i_bench_red - i_bench_dark) / n_bench",
    moves="+i_bench_red -i_bench_dark -n_bench", stated=False)
fig("i_green", "mA", "(i_bench_green - i_bench_dark) / n_bench",
    moves="+i_bench_green -i_bench_dark -n_bench", stated=False)
fig("i_blue", "mA", "(i_bench_blue - i_bench_dark) / n_bench",
    moves="+i_bench_blue -i_bench_dark -n_bench", stated=False)
fig("i_green_half", "mA", "(i_bench_green_half - i_bench_dark) / n_bench",
    moves="+i_bench_green_half -i_bench_dark -n_bench", stated=False)

# Full white as the three colors summed, which lies above the white reading and so bounds it.
fig("i_led_white", "mA", "i_red + i_green + i_blue", "up",
    moves="+i_bench_red +i_bench_green +i_bench_blue -i_bench_dark", stated=False)
fig("i_led_white_read", "mA", "(i_bench_white - i_bench_dark) / n_bench",
    moves="+i_bench_white -i_bench_dark -n_bench", stated=False)
fig("i_idle", "A", "n_leds.raw * i_ctrl", "up", stated=False)

# The ambient lights are the perimeters and the ceiling, the playfield lights the rest. The 3 A
# adapter lights every second perimeter LED, the 6 A adapter all of them.
fig("n_ambient", "", "n_perimeter + n_ceiling", stated=False)
fig("n_playfield", "", "n_leds - n_ambient", stated=False)
fig("n_ambient_lit_3a", "", "n_perimeter_lit_3a + n_ceiling", stated=False)

# FastLED's power model takes whole milliwatts per color at full brightness. The rail's 5 V
# turns the currents into them, rounded up so that the cap never lets more through than it states.
dec("mw_grain", 1, "mW", src="FastLED's PowerModelRGB, which takes each color's draw in whole "
    "milliwatts, FastLED 3.10.4 src/power_mgt.h", stated=False)
MODEL.uses("i_permanent", of="power")
MODEL.uses("v_c_rating", of="power")
fig("i_light_left_3a", "A", "i_supply_3a - i_permanent", "down", stated=False)

# -- the Teensy's pin and the buffer's supply ---------------------------------
MODEL.uses("v_3v3", of="teensy-4.1", group=SHIFT, stated="loose")
MODEL.uses("v_oh_drop", of="teensy-4.1")
MODEL.uses("v_ol_max", of="teensy-4.1")
MODEL.uses("i_pin_max", of="teensy-4.1")
MODEL.uses("v_3v3_max", of="teensy-4.1")
MODEL.uses("v_3v3_min", of="teensy-4.1")
ds("kit_tol", 5, "%", src="the justPi resistor kit JUS-19729 every resistor comes from, Botland "
   "product page: tolerance 5 %")
dec("v_rail", 5, "V", src="the machine's rail, which feeds the buffers and, through J-LA to J-LF, "
    "the segments", group=SHIFT, stated="loose")
ds("vih_led_frac", 70, "%", src="XINGLIGHT XL-0807RGBC-WS2812B datasheet, electrical "
   "characteristics, input signal threshold VIH at least 0.7 × VDD at VDD = 5 V; the older WS2812B "
   "sheets state the same")
ds("vcc_min", 4.5, "V", src=f"{BUFFER}, 5.3 Recommended Operating Conditions, supply voltage VCC "
   "minimum", group="VCC, pin 14", section=BUFFERS, stated=True)
ds("vcc_max", 5.5, "V", src=f"{BUFFER}, 5.3 Recommended Operating Conditions, supply voltage VCC "
   "maximum", group="VCC, pin 14", section=BUFFERS, stated=True)
with MODEL.block("Decoupling", section=BUFFERS):
    ds("c_bypass", 100, "nF", src=f"{BUFFER}, 8.3 Power Supply Recommendations, a 0.1 µF capacitor "
       "at each VCC terminal, as close to it as possible")
    asm("place_bypass", 10, "mm", src="how close each 100 nF sits to its buffer's supply pin, "
        "which keeps the loop through the capacitor short; no datasheet states one")

# The LED reads a high only above a fraction of its own supply.
fig("vih_led", "V", "vih_led_frac * v_rail", "up", moves="+vih_led_frac +v_rail", group=SHIFT,
    stated="loose")

# -- the buffer's output ------------------------------------------------------
ds("i_oh_test", 4, "mA", src=f"{BUFFER}, 5.5 Electrical Characteristics, the output current VOH is "
   "specified at for the bus-driver load", group=BUFFERS)
ds("v_oh_out", 3.7, "V", src=f"{BUFFER}, 5.5 Electrical Characteristics, VOH minimum at IOH = "
   "-4 mA and VCC = 4.5 V, -55 °C to 125 °C", group=BUFFERS, stated="loose")
dec("r_out", 330, "Ω", src="R4 to R6, the series resistor at each buffer output, chosen so a data "
    "wire that meets 5 V or ground holds the buffer's output inside its continuous output current "
    "and the resistor inside half its rating: the 330 Ω of the resistor kit, the smallest of its "
    "values that does both", group=SERIES, stated="loose")

# -- the series resistor in each data line ------------------------------------
with MODEL.block(SERIES, stated="loose"):
    ds("i_out_max", 35, "mA", src=f"{BUFFER}, 5.1 Absolute Maximum Ratings, continuous output "
       "current IO; 5.3 states no output current")
    ds("p_kit", 0.25, "W", src="the same kit and page: power rating 0.25 W")
    ds("r_out_best_min", 47, "Ω", src="PJRC, OctoWS2811 page, Signal Quality: values of 47 to "
       "220 Ω are probably best for the series resistor, depending on the wire")
    ds("r_out_best_max", 220, "Ω", src="the same page, the upper end of that range")
    dec("p_derate", 50, "%", src="the derating target for resistor power in the design-review "
        "skill, at most half the rating", stated=False)
    MODEL.uses("i_coil", of="solenoid", stated=False)

    # A data wire that meets ground while the buffer drives high, or 5 V while it drives
    # low, leaves the resistor alone between the rail and the output. The rail is taken
    # at the highest the buffer accepts, the resistor at its low end, and the output's own
    # resistance is not counted.
    fig("i_fault", "mA", "vcc_max / (r_out * (1 - kit_tol))", "up",
        moves="+vcc_max +kit_tol -r_out")

fig("p_fault", "W", "vcc_max * i_fault", "up", moves="+vcc_max +kit_tol -r_out", stated=False)

# The resistor's floor: the least value whose low end holds the fault inside the output's
# rating, and the least whose low end carries it at the share of its rating every
# resistor is held to.
fig("r_out_min", "Ω", "vcc_max / (i_out_max * (1 - kit_tol))", "up",
    moves="+vcc_max +kit_tol -i_out_max", stated=False)
fig("r_out_min_p", "Ω", "vcc_max * vcc_max / (p_kit * p_derate * (1 - kit_tol))", "up",
    moves="+vcc_max +kit_tol -p_kit -p_derate", stated=False)

# -- the input resistor -------------------------------------------------------
with MODEL.block("Used inputs", section=BUFFERS):
    dec("r_in", 2.2, "kΩ", src="the resistor from each Teensy pin to its buffer input, chosen so "
        "the pin's current into the input's clamp diode stays far under its 4 mA while the "
        "machine's 5 V is off, and so the edge it slows leaves the data timing near its own: the "
        "2.2 kΩ of the resistor kit")
    asm("place_in", 10, "mm", src="how close the resistor sits to its buffer input, which keeps "
        "the wiring between them short; no datasheet states one")
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
       "transition time tt maximum at VCC = 5.5 V, the shorter of its two figures", group=BUFFERS,
       stated=False)
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
    fig("i_clamp", "mA", "v_3v3_max / r_in", "up", moves="+v_3v3_max -r_in")

fig("v_oh_min", "V", "v_3v3_min - v_oh_drop", "down", moves="+v_3v3_min -v_oh_drop", stated=False)

# The resistor into the input and the wiring beside it is a single pole.
fig("tau_in", "ns", "r_in * (c_in + c_stray)", moves="+r_in +c_in +c_stray", stated=False)

# A single-pole step crosses the two marks ln((1 - low) / (1 - high)) time
# constants apart.
fig("t_edge", "ns", "ln((1 - edge_low.raw) / (1 - edge_high.raw)) * tau_in", "up",
    moves="+r_in +c_in +c_stray +edge_high -edge_low", group=BUFFERS, stated=False)

# The buffer switches where its input crosses its threshold, somewhere between
# VIL and VIH. A high from the level H after a low at L crosses a threshold T
# tau ln((H - L) / (H - T)) after the rising edge and tau ln((H - L) / (T - L))
# after the falling one, so the buffer's high lasts tau ln((H - T) / (T - L))
# longer than the pin's. It grows most with the highest high, the lowest
# threshold and the highest low.
fig("t_longer", "ns", "tau_in * ln((v_3v3_max - vil) / (vil - v_ol_max))", "up",
    moves="+v_3v3_max +v_ol_max +r_in +c_in +c_stray -vil", group=BUFFERS, stated=False)

# It shrinks most with the lowest high, the highest threshold and a low at
# ground.
fig("t_shorter", "ns", "tau_in * ln(vih / (v_oh_min - vih))", "up",
    moves="+vih +v_oh_drop +r_in +c_in +c_stray -v_3v3_min", group=BUFFERS, stated=False)

# R1 to R3's window. The least value whose low end holds the pin inside its 4 mA with the
# 5 V off, the rail at its high end, and the most whose high end still stretches a high by
# less than the LED's timing tolerance, the edge the window rests on.
fig("r_in_min", "kΩ", "v_3v3_max / (i_pin_max * (1 - kit_tol))", "up",
    moves="+v_3v3_max +kit_tol -i_pin_max", stated=False)
fig("r_in_max", "kΩ",
    "t_bit_tol / ((c_in + c_stray) * ln((v_3v3_max - vil) / (vil - v_ol_max)) * (1 + kit_tol))",
    "down", moves="+t_bit_tol +vil -c_in -c_stray -v_3v3_max -v_ol_max -kit_tol", stated=False)
asm("c_data", 150, "pF", src="the data wire from the board to a chain's first LED together with "
    "that LED's input, estimated generously and not measured", group=SERIES, stated="loose")
ds("vil_led_frac", 30, "%", src="XINGLIGHT XL-0807RGBC-WS2812B datasheet, electrical "
   "characteristics, input signal threshold VIL at most 0.3 × VDD at VDD = 5 V; the older WS2812B "
   "sheets state the same")
fig("vil_led", "V", "vil_led_frac * v_rail", "down", moves="+vil_led_frac +v_rail", stated=False)

# The resistor into the data wire and the first LED's input is a single pole too. The LED
# reads its input against one threshold, which its sheet bounds only by VIH, so a high
# arrives shorter by tau ln(VIH / (VOH - VIH)) at most. The buffer and the first LED share the
# board's rail, so VIH is taken at the supply the buffer's VOH minimum is specified at, with
# the resistor at its high end.
ds("vcc_voh", 4.5, "V", src=f"{BUFFER}, 5.5 Electrical Characteristics, the VCC the VOH minimum "
   "is specified at")
fig("vih_led_voh", "V", "vih_led_frac * vcc_voh", "up", moves="+vih_led_frac +vcc_voh",
    stated=False)
def _t_out_shorter(r_out, kit_tol, c_data, v_oh_out, vih_led_voh):
    if (v_oh_out - vih_led_voh).v <= 0:    # a high that never reaches the LED's high level
        return Q.of(inf, "ns")
    return r_out * (1 + kit_tol) * c_data * ln(vih_led_voh / (v_oh_out - vih_led_voh))
fig("t_out_shorter", "ns", _t_out_shorter, "up",
    moves="+r_out +kit_tol +c_data +vih_led_frac +vcc_voh -v_oh_out", group=SERIES,
    stated="loose")

# The most the resistor may be: at its high end the two edges together still shorten a high
# by less than the LED's timing tolerance.
def _r_out_max(t_bit_tol, t_shorter, c_data, v_oh_out, vih_led_voh, kit_tol):
    if (v_oh_out - vih_led_voh).v <= 0:    # no resistor passes a high the LED cannot read
        return Q.of(0, "Ω")
    return ((t_bit_tol - t_shorter)
            / (c_data * ln(vih_led_voh / (v_oh_out - vih_led_voh)) * (1 + kit_tol)))
fig("r_out_max", "Ω", _r_out_max, "down",
    moves="+t_bit_tol +v_oh_out -c_data -vih_led_frac -vcc_voh -kit_tol -r_in", stated=False)

# -- the supply through the boards' power input -------------------------------------------------
with MODEL.block(SUPPLY, stated="loose"):
    dec("i_supply_3a", 3, "A", src="the smaller supply the machine is designed for; design.md, "
        "Current ceiling")
    dec("i_supply_6a", 6, "A", src="the larger supply the machine is designed for; design.md, "
        "Current ceiling")
    dec("i_budget", 2.0, "A", src="the lighting's cap with the 3 A supply, which the firmware "
        "holds with FastLED's setMaxPowerInVoltsAndMilliamps; design.md, Current ceiling",
        group="Cap", section=CEILING, stated=True)
    # With the 6 A supply the cap leaves room for the permanent consumers and the coil case, two
    # bumpers and the scoop pulling at once.
    MODEL.uses("i_coils_at_once", of="solenoid", stated=False)
    dec("cap_grain", 0.01, "A", src="the step the lighting's cap is set in, 10 mA", stated=False)
    fig("i_budget_6a", "A", "floor_to(i_supply_6a - i_permanent - i_coils_at_once, 'A', "
        "cap_grain.to('A'))", "down", moves="+i_supply_6a -i_coils_at_once -i_permanent",
        group="Cap", section=CEILING, stated=True)
    dec("len_jpwr", 30, "cm", src="the longest the boards' power input cable, the length the feed's inductance is "
        "estimated for")
    ds("r_awg22", 52.96, "mΩ/m", src="ASTM B258, 22 AWG annealed copper at 20 °C", stated=False)
    MODEL.uses("l_feed", of="solenoid", stated=False)
    MODEL.uses("c_one", of="power")  # C2 on each board, the power board's bulk part
    dec("n_bulk_light", 2, "", src="C2, one on each of the two lighting boards at the boards' power input",
        stated=False)
    MODEL.uses("z_c", of="power")
    ds("f_z_bulk", 100, "kHz", src="the frequency the listing states C2's impedance at")
    asm("place_bulk", 20, "mm", src="how close C2 sits to the boards' power input, where the feed arrives; no "
        "datasheet states one")

    # One wire of the boards' power input, soldered at both ends: its copper alone.
    fig("r_jpwr", "mΩ", "len_jpwr * r_awg22", moves="+len_jpwr +r_awg22", stated=False)

    # Both wires of the boards' power input carry the lighting's whole current. The cap holds for all chains
    # together, so either board can draw all of it, and each board's figures take the whole cap.
    fig("v_jpwr", "V", "2 * r_jpwr * i_budget", "up", moves="+i_budget +len_jpwr +r_awg22",
        stated=False)
    fig("v_jpwr_6a", "V", "2 * r_jpwr * i_budget_6a", "up", moves="+i_budget_6a +len_jpwr +r_awg22",
        stated=False)

    # When the LED current steps, the feed's inductance holds its current for a moment,
    # and C2 carries the step. The worst step is the whole cap at once. The rail then
    # falls by the step across C2's impedance and the feed's characteristic impedance.
    fig("z_step", "mΩ", "z_c + (l_feed / c_one) ** (1 / 2)", moves="+z_c +l_feed -c_one",
        stated=False)
    fig("v_dip", "V", "i_budget * z_step", "up", moves="+i_budget +z_c +l_feed -c_one",
        stated=False)
    fig("v_dip_6a", "V", "i_budget_6a * z_step", "up", moves="+i_budget_6a +z_c +l_feed -c_one",
        stated=False)
    fig("v_buffer_min", "V", "v_rail - v_jpwr - v_dip", "down",
        moves="+v_rail -i_budget -len_jpwr -r_awg22 -z_c -l_feed")
    fig("v_buffer_min_6a", "V", "v_rail - v_jpwr_6a - v_dip_6a", "down",
        moves="+v_rail -i_budget_6a -len_jpwr -r_awg22 -z_c -l_feed")

# -- the two adapters' profiles: how bright each zone may be at full white everywhere --------------
dec("scale_steps", 256, "", src="FastLED's scale8, which multiplies a value by (scale + 1) / 256 while "
    "FASTLED_SCALE8_FIXED is 1, src/platforms/shared/scale8.h of FastLED 3.10.8", stated=False)
dec("playfield_max", 255, "", src="the playfield lights' brightness at most, full, since they stand "
    "for the game", group="Playfield lights, brightness at most", section=CEILING)
dec("ambient_ceiling", 128, "", src="the most brightness the ambient lights need, a decision")
fig("frac_playfield", "%", "(playfield_max + 1) / scale_steps", stated=False)


# The ambient lights take what is left of the cap once the controllers and the playfield at its
# brightness are paid, as the FastLED brightness value whose scale stays under it.
def _ambient_max(budget, i_idle, n_playfield, i_led_white, frac_playfield, n_lit, scale_steps):
    left = (budget - i_idle - n_playfield.raw * i_led_white * frac_playfield.raw) / (n_lit.raw * i_led_white)
    return Q.of(floor(left.raw * scale_steps.raw) - 1, "")


fig("ambient_max_3a", "", lambda i_budget, i_idle, n_playfield, i_led_white, frac_playfield,
    n_ambient_lit_3a, scale_steps: _ambient_max(i_budget, i_idle, n_playfield, i_led_white,
                                               frac_playfield, n_ambient_lit_3a, scale_steps), "down",
    group="Ambient lights, brightness at most", section=CEILING)
fig("ambient_max_6a", "", lambda i_budget_6a, i_idle, n_playfield, i_led_white, frac_playfield,
    n_ambient, scale_steps: _ambient_max(i_budget_6a, i_idle, n_playfield, i_led_white,
                                        frac_playfield, n_ambient, scale_steps), "down",
    group="Ambient lights, brightness at most", section=CEILING)

# what the whole layout draws at full white with each profile
fig("i_white_3a", "A", "i_idle + i_led_white * (n_playfield.raw * frac_playfield.raw + "
    "n_ambient_lit_3a.raw * (ambient_max_3a.raw + 1) / scale_steps.raw)", "up", stated=False)
fig("i_white_6a", "A", "i_idle + i_led_white * (n_playfield.raw * frac_playfield.raw + "
    "n_ambient.raw * (ambient_max_6a.raw + 1) / scale_steps.raw)", "up", stated=False)

# -- FastLED's power model ------------------------------------------------------
fig("pm_red", "mW", "ceil_to(i_red * v_rail, 'mW', mw_grain.to('mW'))", "up",
    moves="+i_bench_red -i_bench_dark +v_rail", stated=False)
fig("pm_green", "mW", "ceil_to(i_green * v_rail, 'mW', mw_grain.to('mW'))", "up",
    moves="+i_bench_green -i_bench_dark +v_rail", stated=False)
fig("pm_blue", "mW", "ceil_to(i_blue * v_rail, 'mW', mw_grain.to('mW'))", "up",
    moves="+i_bench_blue -i_bench_dark +v_rail", stated=False)
fig("pm_dark", "mW", "ceil_to(i_ctrl * v_rail, 'mW', mw_grain.to('mW'))", "up",
    moves="+i_bench_dark +v_rail", stated=False)

# -- the frame ----------------------------------------------------------------
OBJECTFLED = "ObjectFLED.h in FastLED"
MODEL.uses("n_chain_max", of="firmware")
ds("n_bits_led", 24, "", src="WS2812B datasheet, older revision: composition of 24 bit data, "
   "8 bits each of green, red and blue")
ds("n_bits_color", 8, "", src="WS2812B datasheet, older revision: composition of 24 bit data, G7 "
   "to G0, R7 to R0 and B7 to B0; the lamps component's Color holds them as they are")
MODEL.aside("30 %", "an example of the brightness Color::dim() takes, in lamps.md")
ds("t_bit", 1.25, "µs", src=f"{OBJECTFLED}: TH_TL, the period of one bit, 1250 ns by default; the "
   "WS2812B datasheet's TH+TL")
ds("t_latch", 300, "µs", src=f"{OBJECTFLED}: LATCH_DELAY, how long it holds the outputs low after "
   "a frame, 300 µs by default")
ds("t_reset", 280, "µs", src="WS2812B datasheet Ver. No. V5: RES, the low time that ends a frame, "
   "more than 280 µs; the older revision asks for more than 50 µs, and the strip's own sheet gives "
   "none")
dec("t_frame", 10, "ms", src="the lamps component's frame period, "
    "docs/firmware/components/lamps.md; 100 frames a second, the gist's kFrameUs")
fig("t_chain_out", "ms", "n_chain_max * n_bits_led * t_bit",
    moves="+n_chain_max +n_bits_led +t_bit", stated=False)
fig("t_frame_out", "ms", "t_chain_out + t_latch", moves="+t_chain_out +t_latch", stated=False)

# -- requirements -------------------------------------------------------------
_I = MODEL.invariant
_I("the machine's rail sits in the buffer's supply range", "vcc_min <= v_rail <= vcc_max")
_I("the Teensy's pin alone cannot reach the LED's high input level", "v_3v3 < vih_led")
_I("the buffer's lowest high clears the LED's high input level", "v_oh_out >= vih_led")
_I("the Teensy's lowest high clears the buffer's high input level", "v_oh_min > vih")
_I("the Teensy's highest low stays under the buffer's low input level", "v_ol_max < vil")
_I("with the machine's 5 V off, the resistor holds each pin inside its 4 mA", "i_clamp < i_pin_max")
_I("with the machine's 5 V off, the resistor holds the clamp diode inside its rating",
   "i_clamp < iik")
_I("the resistor stretches each high by less than the LED's timing tolerance",
   "t_longer < t_bit_tol")
_I("the resistor shortens each high by less than the LED's timing tolerance",
   "t_shorter < t_bit_tol")
_I("the edge through the resistor stays inside the longest input edge the buffer allows",
   "t_edge < t_tran_max")
_I("a data wire that meets 5 V or ground holds the buffer's output inside its rating",
   "i_fault <= i_out_max")
_I("the series resistor carries that fault at no more than half its rating",
   "p_fault <= p_kit * p_derate")
_I("R1 to R3 sit inside their window", "r_in_min <= r_in <= r_in_max")
_I("the data-line resistor holds a shorted wire inside the buffer's rating, at its low end",
   "r_out_min <= r_out")
_I("and carries it at no more than half its rating", "r_out_min_p <= r_out")
_I("and at its high end leaves the first LED's high inside the timing tolerance",
   "r_out <= r_out_max and t_shorter + t_out_shorter < t_bit_tol")

_I("with the 3 A supply, a step of the whole cap leaves the buffers inside their supply range",
   "v_buffer_min >= vcc_min")
_I("with the 6 A supply, a step of the whole cap leaves the buffers inside their supply range",
   "v_buffer_min_6a >= vcc_min")
_I("the three colors summed bound what an LED draws at full white",
   "i_led_white >= i_led_white_read")
_I("the controllers alone stay inside the smaller cap, so the colors keep a share of it",
   "i_idle < i_budget")
_I("with the 3 A adapter the whole layout at full white stays inside the cap", "i_white_3a <= i_budget")
_I("with the 6 A adapter the whole layout at full white stays inside the cap",
   "i_white_6a <= i_budget_6a")
_I("the ambient lights stay at or under the brightness they need",
   "ambient_max_3a <= ambient_ceiling and ambient_max_6a <= ambient_ceiling")
_I("the ambient lights keep some brightness with either adapter", "ambient_max_3a > Q.of(0, '')")
_I("the 6 A adapter carries the lighting at its cap, the permanent consumers and two bumpers with "
   "the scoop", "i_budget_6a + i_coils_at_once + i_permanent <= i_supply_6a")
_I("the strip draws less per color than the usual budget", "i_red < i_color_usual and "
   "i_green < i_color_usual and i_blue < i_color_usual")
_I("green at half its value draws less than at full and more than nothing",
   "Q.of(0, 'mA') < i_green_half < i_green")
_I("ObjectFLED holds the lines low long enough for the LEDs to take over a frame",
   "t_latch > t_reset")
_I("a frame has gone out before the next one is handed on, so show() never waits",
   "t_frame_out < t_frame")

# -- wiring -------------------------------------------------------------------
# each chain's pin feeds one buffer input through 2.2 kΩ; the input's clamp diode points to
# the buffer's VCC, so it can sink the pin's current but never raises the pin
for _n in "ABCDEF":
    MODEL.net(f"LED chain {_n}, Teensy side",
              reads("a CD74HCT125E input through 2.2 kΩ",
                    src="design.md, Level shifting: 2.2 kΩ from the Teensy pin to each used input"),
              teensy=f"LED chain {_n}")

MODEL.owns("QuadTimer4", "FastLED's ObjectFLED driver owns channels 0 to 2 of QuadTimer4, "
                         "ObjectFLED.h in FastLED")
