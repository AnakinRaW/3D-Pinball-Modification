"""The figures of the display, its bus, its touch controller and its supply, as inputs plus formulas.

Run with:  python tools/figcheck.py docs/parts/display/figures.py

Every input names where it comes from. Every derived quantity is a function
whose parameter names are its dependencies, so `--graph <key>` answers what a
value feeds and what it rests on.
"""
import pathlib

from figcheck import Model, TEENSY_RAIL, drives, ln, pull, reads

HERE = pathlib.Path(__file__).resolve().parent
FIRMWARE = HERE.parents[1] / "firmware"
MODEL = Model("display", HERE / "design.md", section=None, until="## Sources",
              drawings=[HERE / "display-schematic.svg"],
              documents=[FIRMWARE / "drivers" / "display.md",
                         FIRMWARE / "drivers" / "touch.md",
                         FIRMWARE / "components" / "screen.md",
                         HERE.parents[2] / "assets" / "media.md"])

ds = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="datasheet", **kw)
dec = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="decision", **kw)
asm = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="assumed", **kw)
fig = MODEL.derived

PANEL = "ILI9341-Ilitek.pdf"
GLASS = "CH280QV10-CT-ChengHao.pdf"
LDO = "AP2112-Diodes.pdf"
SUPERVISOR = "APX803-Diodes.pdf"
TOUCH_IC = "CST026-Hynitron.pdf"
BUFFER = "SN74LVC245A-TI.pdf"
LVL = "Adafruit 757, Adafruit FET 4-Channel Shifter.sch in github.com/adafruit/4-Channel-Level-Shifter-PCB"
BSS = "BSS138-onsemi.pdf"
MCU = "IMXRT1060CEC.pdf"
RM = "IMXRT1060RM Rev. 3"
SCH = ("Adafruit's EAGLE schematic of the 2090, rev D, "
       "github.com/adafruit/Adafruit-2.8-TFT-with-Capacitive-Touch-PCB")
SD = "SD Specifications Part 1, Physical Layer Simplified Specification, version 6.00"

BUS = "Bus"
FRAME = "Picture"
MOVIE = "Picture"
TOUCH = "Touch bus"
RESET = "Supply"
SUPPLY = "Supply"

# ===========================================================================
# the bus: FlexIO3 clocks a byte onto D0 to D7 with every WR strobe
# ===========================================================================
dec("f_flexio", 120, "MHz", src=f"the FlexIO3 clock, PLL3's 480 MHz divided by 2 and by 2, "
    f"the highest {RM} allows, Table 14-5", group="BUS", section=BUS, stated=True)
ds("f_flexio_max", 120, "MHz", src=f"{RM}, Table 14-5: FLEXIO2_CLK_ROOT, which also clocks "
   "FlexIO3 (p. 1011), at most 120 MHz in Full Speed and Overdrive Run")
ds("f_pll3", 480, "MHz", src=f"{RM}, CCM: PLL3, the USB PLL, at 480 MHz, which "
   "FLEXIO2_CLK_SEL 3 selects; PJRC's startup.c runs it there", stated=False)
dec("n_div", 10, "clocks", src="the FlexIO3 clocks per write, twice TIMCMP[7:0] + 1 of the "
    "timer that drives WR, chosen for a margin over the ILI9341's write cycle",
    group="BUS", section=BUS, stated=True)
ds("t_wc_min", 66, "ns", sheet=PANEL, src="18.3.1, 8080-I write timing, write cycle twc "
   "minimum, p. 238", group="BUS", section=BUS, stated=True)
ds("t_strobe_min", 15, "ns", sheet=PANEL, src="18.3.1, write control pulse H duration twrh "
   "and L duration twrl, each at least, p. 238", group="BUS", section=BUS, stated=True)
ds("t_sh_min", 10, "ns", sheet=PANEL, src="18.3.1, write data setup time tdst and hold time "
   "tdht, each at least, p. 238", group="BUS", section=BUS, stated=True)
ds("t_pd_min", 1.5, "ns", sheet=BUFFER, src="7.6 Switching Characteristics, tpd A to B at "
   "VCC = 3.3 V ± 0.3 V, minimum; the module's two buffers are 74LVC245, TI's figures "
   "stand for them", group="BUS", section=BUS, stated=True)
ds("t_pd_max", 6.3, "ns", sheet=BUFFER, src="7.6 Switching Characteristics, tpd A to B at "
   "VCC = 3.3 V ± 0.3 V, maximum from -40 °C to 85 °C", group="BUS", section=BUS, stated=True)


@fig("t_wr", "ns", group="BUS", section=BUS, rises_with=["n_div"], falls_with=["f_flexio"])
def _(n_div, f_flexio):
    return n_div / f_flexio


# WR is low for half a write and high for the other half; the data changes as WR falls
# and the panel latches it as WR rises
@fig("t_half", "ns", group="BUS", section=BUS, rises_with=["n_div"], falls_with=["f_flexio"])
def _(t_wr):
    return t_wr / 2


# WR passes one buffer and the data the other, so either can arrive first
@fig("t_skew", "ns", group="BUS", section=BUS, prints="up",
     rises_with=["t_pd_max"], falls_with=["t_pd_min"])
def _(t_pd_max, t_pd_min):
    return t_pd_max - t_pd_min


@fig("t_sh", "ns", group="BUS", section=BUS, prints="down",
     rises_with=["n_div", "t_pd_min"], falls_with=["f_flexio", "t_pd_max"])
def _(t_half, t_skew):
    return t_half - t_skew


# ---------------------------------------------------------------------------
# the edges: a bus pin at its weakest drive into the cable and a buffer input
# ---------------------------------------------------------------------------
ds("r_drv", 157, "Ω", sheet=MCU, src="Table 27, GPIO output buffer average impedance at "
   "NVCC 3.3 V, DSE 001, typical; the sheet gives no maximum", group="EDGE", section=BUS,
   stated=True)
dec("l_cable", 50, "cm", src="the longest cable from the Teensy to the module in the diorama",
    group="EDGE", section=BUS, stated=True)
asm("c_cable", 50, "pF", src="one wire of the cable of at most 50 cm against its neighbors, "
    "about twice what a flat cable of 28 AWG carries per half meter; no measurement",
    group="EDGE", section=BUS, stated=True)
ds("c_io", 5.5, "pF", sheet=BUFFER, src="7.5 Electrical Characteristics, Cio of the A or B "
   "ports at VCC = 3.3 V, typical", group="EDGE", section=BUS, stated=True)
asm("edge_low", 10, "%", stated=False, src="the lower mark TI's logic sheets time an edge "
    "between")
asm("edge_high", 90, "%", stated=False, src="the upper mark, taken as edge_low is")
ds("dt_dv_max", 10, "ns/V", sheet=BUFFER, src="7.3 Recommended Operating Conditions, "
   "input transition rise or fall rate Δt/Δv, maximum", group="EDGE", section=BUS,
   stated=True)
MODEL.uses("v_3v3", of="teensy-4.1", group="LEVELS", section=BUS, stated=True)


@fig("t_edge", "ns", group="EDGE", section=BUS, prints="up",
     rises_with=["r_drv", "c_cable", "c_io", "edge_high"], falls_with=["edge_low"])
def _(r_drv, c_cable, c_io, edge_low, edge_high):
    return ln((1 - edge_low.raw) / (1 - edge_high.raw)) * r_drv * (c_cable + c_io)


@fig("dt_dv", "ns/V", group="EDGE", section=BUS, prints="up",
     rises_with=["r_drv", "c_cable", "c_io"], falls_with=["v_3v3"])
def _(t_edge, edge_low, edge_high, v_3v3):
    return t_edge / ((edge_high - edge_low) * v_3v3)


# ---------------------------------------------------------------------------
# the levels: the Teensy's pins into the module's buffers
# ---------------------------------------------------------------------------
MODEL.uses("v_oh_drop", of="teensy-4.1")
MODEL.uses("v_ol_max", of="teensy-4.1", group="LEVELS", section=BUS, stated=True)
ds("vih_buf", 2, "V", sheet=BUFFER, src="7.3 Recommended Operating Conditions, VIH minimum "
   "at VCC = 2.7 V to 3.6 V", group="LEVELS", section=BUS, stated=True)
ds("vil_buf", 0.8, "V", sheet=BUFFER, src="7.3 Recommended Operating Conditions, VIL maximum "
   "at VCC = 2.7 V to 3.6 V", group="LEVELS", section=BUS, stated=True)


@fig("v_oh_min", "V", group="LEVELS", section=BUS, prints="down",
     rises_with=["v_3v3"], falls_with=["v_oh_drop"])
def _(v_3v3, v_oh_drop):
    return v_3v3 - v_oh_drop


@fig("m_high", "V", group="LEVELS", section=BUS, prints="down",
     rises_with=["v_3v3"], falls_with=["v_oh_drop", "vih_buf"])
def _(v_oh_min, vih_buf):
    return v_oh_min - vih_buf


@fig("m_low", "V", group="LEVELS", section=BUS, prints="down",
     rises_with=["vil_buf"], falls_with=["v_ol_max"])
def _(vil_buf, v_ol_max):
    return vil_buf - v_ol_max


# ===========================================================================
# the frame: three layers of palette indices and a movie, composited as they are sent
# ===========================================================================
ds("n_cols", 320, "", sheet=PANEL, src="the panel's 320 lines, which the landscape mounting "
   "makes its width; 240RGB x 320 in the title", group="FRAME", section=FRAME, stated=True)
ds("n_rows", 240, "", sheet=PANEL, src="the panel's 240 columns, the height in landscape",
   group="FRAME", section=FRAME, stated=True)
ds("b_pixel", 2, "bytes", sheet=PANEL, src="8-bit bus, 16 bits per pixel in RGB 5-6-5, two "
   "writes per pixel, p. 65", group="FRAME", section=FRAME, stated=True)
dec("b_index", 1, "bytes", src="one palette index per pixel in a layer, 256 colors",
    stated=False)
dec("n_layers", 3, "", src="the game's layer, the host's layer and the game's background",
    stated=False)
dec("n_colors", 256, "", src="the colors of a layer's palette, every value of its byte, "
    "index 0 transparent", stated=False)
ds("n_shifters", 8, "", src=f"{RM}, 50.6.1.3.3: FlexIO3 has eight shifters, chained into "
   "one buffer of a burst", stated=False)
ds("b_shifter", 4, "bytes", src=f"{RM}: each shifter buffer holds 32 bits, four writes of "
   "8 bits", stated=False)
MODEL.uses("f_cpu", of="teensy-4.1", group="FRAME", section=FRAME, stated=True)
asm("n_isr", 350, "clocks", src="the processor clocks one refill takes: 16 pixels looked up "
    "in two layers and in the movie or the background, eight writes to FlexIO3, whose "
    "registers answer at its own clock, and the interrupt's entry and exit; an estimate, no "
    "measurement, with the background's index and palette read for a pixel that falls "
    "through to it",
    group="FRAME", section=FRAME, stated=True)


@fig("b_frame", "KB", group="FRAME", section=FRAME,
     rises_with=["n_cols", "n_rows", "b_pixel"])
def _(n_cols, n_rows, b_pixel):
    return n_cols * n_rows * b_pixel


@fig("t_frame_bus", "ms", group="FRAME", section=FRAME, prints="up",
     rises_with=["n_cols", "n_rows", "b_pixel", "n_div"], falls_with=["f_flexio"])
def _(b_frame, t_wr):
    return b_frame * t_wr


@fig("f_screen_max", "Hz", group="FRAME", section=FRAME, prints="down",
     falls_with=["n_cols", "n_rows", "n_div"], rises_with=["f_flexio"])
def _(t_frame_bus):
    return 1 / t_frame_bus


@fig("n_burst", "bytes", group="FRAME", section=FRAME, rises_with=["n_shifters", "b_shifter"])
def _(n_shifters, b_shifter):
    return n_shifters * b_shifter


@fig("n_burst_px", "", stated=False, rises_with=["n_shifters", "b_shifter"],
     falls_with=["b_pixel"])
def _(n_burst, b_pixel):
    return n_burst / b_pixel


# a movie's viewport: its left edge and its width move in whole bursts, so no burst straddles its edge
@fig("n_viewport_step", "", stated=False, rises_with=["n_shifters", "b_shifter"],
     falls_with=["b_pixel"])
def _(n_burst_px):
    return n_burst_px


@fig("t_burst", "µs", group="FRAME", section=FRAME,
     rises_with=["n_shifters", "b_shifter", "n_div"], falls_with=["f_flexio"])
def _(n_burst, t_wr):
    return n_burst * t_wr


dec("t_end_wait", 3, "µs", src="how long the FlexIO3 interrupt waits after the last burst of a "
    "run has started before it ends the run, a decision: one burst and a reserve for the FlexIO3 "
    "clocks between shifter 7's flag and the burst's first write, which the reference manual does "
    "not give", stated=False)
MODEL.as_written("t_end_wait_ns", of="t_end_wait", unit="ns")


@fig("cpu_send", "%", group="FRAME", section=FRAME, prints="up",
     rises_with=["n_isr", "f_flexio"], falls_with=["n_div", "f_cpu", "n_shifters"])
def _(n_isr, t_burst, f_cpu):
    return n_isr / (t_burst * f_cpu)


# partly opaque colors: a palette entry carries an opacity, and the refill blends such a pixel
# with the color behind it
dec("n_alpha", 32, "", src="the opacity steps a palette entry holds, five bits, the steps the "
    "RGB565 blend multiplies in", stated=False)
asm("n_blend", 20, "clocks", src="the processor clocks one refill adds to blend one pixel of a "
    "partly opaque layer with the color behind it: the opacity test, the lookup of the layer "
    "behind, the RGB565 blend in 32 steps and the branch misses where opaque and partly opaque "
    "pixels alternate; an estimate from the instruction count, no measurement",
    group="FRAME", section=FRAME, stated=True)
dec("n_blend_layers", 2, "", src="the layers that blend at one pixel at most, the front and the "
    "middle layer; the back layer's opacity is folded into its palette color when it is set, "
    "since only black lies behind it", stated=False)


@fig("n_isr_blend", "clocks", group="FRAME", section=FRAME,
     rises_with=["n_isr", "n_blend", "n_blend_layers", "n_shifters"], falls_with=["b_pixel"])
def _(n_isr, n_burst_px, n_blend_layers, n_blend):
    return n_isr + n_burst_px * n_blend_layers * n_blend


@fig("n_isr_budget", "clocks", group="FRAME", section=FRAME,
     rises_with=["f_cpu", "n_div", "n_shifters"], falls_with=["f_flexio"])
def _(t_burst, f_cpu):
    return t_burst * f_cpu


@fig("cpu_send_blend", "%", group="FRAME", section=FRAME, prints="up",
     rises_with=["n_isr", "n_blend", "n_blend_layers", "f_flexio"], falls_with=["n_div", "f_cpu"])
def _(n_isr_blend, n_isr_budget):
    return n_isr_blend / n_isr_budget


@fig("b_layer", "KB", group="FRAME", section=FRAME, rises_with=["n_cols", "n_rows", "b_index"])
def _(n_cols, n_rows, b_index):
    return n_cols * n_rows * b_index


# ===========================================================================
# the movie: RGB565 frames from the card, read in chunks into a ring
# ===========================================================================
dec("f_movie", 25, "Hz", src="the frame rate of every movie, a whole number of the display's "
    "ticks per frame", group="MOVIE", section=MOVIE, stated=True)
dec("t_dtick", 40, "ms", src="the period the display driver attaches to the driver tick, one "
    "tick for each of the movie's frames", group="MOVIE", section=MOVIE, stated=True)
dec("ring", 64, "KB", src="the movie's ring buffer in RAM2, four chunks, which the card fills "
    "between two runs, so a run starts with the ring full", group="MOVIE", section=MOVIE, stated=True)
dec("movie_chunk", 16, "KB", src="what the movie's file channel reads from the card at a time, "
    "kChunk in docs/firmware/drivers/display.md; four of the audio's chunks, so a frame takes a "
    "quarter of the reads", group="MOVIE", section=MOVIE, stated=True)
MODEL.as_written("movie_chunk_bytes", of="movie_chunk", unit="bytes")
MODEL.uses("chunk", of="audio")
MODEL.uses("t_tick", of="firmware")
MODEL.as_written("t_dtick_us", of="t_dtick", unit="µs")


@fig("t_movie", "ms", group="MOVIE", section=MOVIE, falls_with=["f_movie"])
def _(f_movie):
    return 1 / f_movie


@fig("n_frame_ticks", "", stated=False, falls_with=["f_movie", "t_dtick"])
def _(t_movie, t_dtick):
    return t_movie / t_dtick


@fig("n_ring", "", stated=False, rises_with=["ring"], falls_with=["movie_chunk"])
def _(ring, movie_chunk):
    return ring / movie_chunk


@fig("n_frame_chunks", "", group="MOVIE", section=MOVIE,
     rises_with=["n_cols", "n_rows", "b_pixel"], falls_with=["movie_chunk"])
def _(b_frame, movie_chunk):
    return b_frame / movie_chunk



# ---------------------------------------------------------------------------
# the card: an estimate of how long a chunk takes, from the A1 class
# ---------------------------------------------------------------------------
ds("f_a1", 1500, "Hz", src=f"{SD}, 4.16.1.1: an A1 card reads at least 1500 random blocks "
   "of 4 KB a second, measured at the UHS-I clocks of Table 4-72 only", stated=False)
ds("f_sd", 50, "MHz", src=f"{SD}: the High Speed clock, which SdFat sets on the Teensy 4.1 "
   "and the storage driver keeps", group="CARD", section=MOVIE, stated=True)
ds("n_sd_lines", 4, "bits", src=f"{SD}: the four data lines of the SD bus, one bit each per "
   "clock", stated=False)
ds("b_bits", 8, "bits", src="the bits of one byte", stated=False)
MODEL.uses("n_streams", of="audio")
MODEL.uses("f_s", of="audio")
MODEL.uses("b_sample", of="audio")


@fig("t_a1", "ms", group="CARD", section=MOVIE, falls_with=["f_a1"])
def _(f_a1):
    return 1 / f_a1


# The A1 rate is measured on a faster bus; the whole chunk's time on the Teensy's
# High Speed bus is added on top of it, which counts the bus twice and keeps the
# estimate on the slow side.
@fig("t_chunk_bus", "ms", group="CARD", section=MOVIE,
     rises_with=["chunk", "b_bits"], falls_with=["f_sd", "n_sd_lines"])
def _(chunk, b_bits, n_sd_lines, f_sd):
    return chunk * b_bits / (n_sd_lines * f_sd)


@fig("t_chunk_est", "ms", group="CARD", section=MOVIE, prints="up",
     rises_with=["chunk"], falls_with=["f_a1", "f_sd"])
def _(t_a1, t_chunk_bus):
    return t_a1 + t_chunk_bus


@fig("t_movie_bus", "ms", group="CARD", section=MOVIE,
     rises_with=["movie_chunk", "b_bits"], falls_with=["f_sd", "n_sd_lines"])
def _(movie_chunk, b_bits, n_sd_lines, f_sd):
    return movie_chunk * b_bits / (n_sd_lines * f_sd)


asm("n_movie_access", 1, "", src="the A1 accesses one movie chunk costs: a read of 16 KB that "
    "follows on in one file costs the card one access, as a read of 4 KB does. The A1 class "
    "measures random reads of 4 KB only, so this is an estimate, and card_load_a1 holds without it")


@fig("t_movie_chunk_est", "ms", group="CARD", section=MOVIE, prints="up",
     rises_with=["movie_chunk", "n_movie_access"], falls_with=["f_a1", "f_sd"])
def _(n_movie_access, t_a1, t_movie_bus):
    return n_movie_access * t_a1 + t_movie_bus


@fig("t_card_frame", "ms", group="CARD", section=MOVIE, prints="up",
     rises_with=["n_cols", "n_rows", "b_pixel"], falls_with=["f_a1", "f_sd"])
def _(n_frame_chunks, t_movie_chunk_est):
    return n_frame_chunks * t_movie_chunk_est


# the four audio streams read their chunks from the same card meanwhile
@fig("t_card_audio", "ms", group="CARD", section=MOVIE, prints="up",
     rises_with=["n_streams", "f_s", "b_sample"], falls_with=["f_movie", "f_a1"])
def _(n_streams, f_s, b_sample, t_movie, chunk, t_chunk_est):
    return n_streams * f_s * b_sample * t_movie / chunk * t_chunk_est


@fig("card_load", "%", group="CARD", section=MOVIE, prints="up",
     rises_with=["f_movie", "n_cols", "n_rows"], falls_with=["f_a1", "f_sd"])
def _(t_card_frame, t_card_audio, t_movie):
    return (t_card_frame + t_card_audio) / t_movie


# on the A1 class alone: every 4 KB of a frame costs one random read, whatever the chunk
@fig("card_load_a1", "%", group="CARD", section=MOVIE, prints="up",
     rises_with=["f_movie", "n_cols", "n_rows"], falls_with=["f_a1", "f_sd"])
def _(b_frame, chunk, t_chunk_est, t_card_audio, t_movie):
    return (b_frame / chunk * t_chunk_est + t_card_audio) / t_movie



# ===========================================================================
# the touch bus: Wire2 to the touch controller, read on the touch driver's tick
# ===========================================================================
dec("f_scl", 100, "kHz", src="the bus clock, Standard-mode, the rate Wire2's begin() sets",
    group="TOUCH", section=TOUCH, stated=True)
dec("t_ttick", 20, "ms", src="the period the touch driver attaches to the driver tick, one "
    "read of the touch controller each", group="TOUCH", section=TOUCH, stated=True)
dec("n_touch_bytes", 8, "", src="the bytes on the bus in one read: the address to write, "
    "the register TD_STATUS, the address to read, and TD_STATUS with the four bytes of the "
    "first touch point", stated=False)
dec("n_touch_data", 5, "", src="the bytes that read brings, TD_STATUS and P1_XH to P1_YL, "
    "registers 0x02 to 0x06 of the FT6x06 map the CST026 keeps", stated=False)
dec("n_conditions", 3, "clocks", src="START, the repeated START and STOP, about one clock "
    "period each", stated=False)
MODEL.uses("n_bits_byte", of="magnetic-rotary", group="TOUCH", section=TOUCH, stated=True)
ds("r_pu_touch", 10, "kΩ", src=f"{LVL}: R1, R4, R5 and R8 "
   "from each LV line to LV, R2, R3, R6 and R7 from each HV line to HV; the Teensy's own "
   "22 kΩ in parallel only speeds the edge", group="TOUCH", section=TOUCH, stated=True)
asm("c_touch", 100, "pF", src="each bus line: the cable of at most 50 cm, the converter's "
    "BSS138 and the Teensy's pin, with margin; no measurement", group="TOUCH",
    section=TOUCH, stated=True)
MODEL.uses("vil_frac", of="teensy-4.1")
MODEL.uses("vih_frac", of="teensy-4.1")
MODEL.uses("t_r_std", of="magnetic-rotary", group="TOUCH", section=TOUCH, stated=True)
ds("f_scl_max", 400, "kHz", sheet=TOUCH_IC, src="I2C slave interface up to 400 kHz, p. 1 and "
   "the AC table, p. 9", stated=False)
dec("t_fail", 100, "ms", src="how long no touch read may work before the touch controller "
    "counts as failed: five reads in a row, so a single failed read does not count, and "
    "short enough that the silence of a supervisor reset reaches the device monitor, which "
    "then has the host restart the display", group="Device faults", stated="loose")
MODEL.as_written("t_fail_ms", of="t_fail", unit="ms")


@fig("t_touch_read", "µs", group="TOUCH", section=TOUCH, prints="up",
     rises_with=["n_touch_bytes", "n_bits_byte", "n_conditions"], falls_with=["f_scl"])
def _(n_touch_bytes, n_bits_byte, n_conditions, f_scl):
    return (n_touch_bytes * n_bits_byte + n_conditions) / f_scl


@fig("touch_share", "%", group="TOUCH", section=TOUCH, prints="up",
     rises_with=["n_touch_bytes"], falls_with=["f_scl", "t_ttick"])
def _(t_touch_read, t_ttick):
    return t_touch_read / t_ttick


# A library read: Adafruit_FT6206 takes the touch registers in one write_then_read(), with the
# processor waiting in Wire until it has ended.
dec("n_lib_touch_bytes", 19, "", src="the bytes Adafruit_FT6206 puts on the bus for one read: "
    "the address, register 0x00, the address again and the 16 registers readData() takes, "
    "github.com/adafruit/Adafruit_FT6206_Library, Adafruit_FT6206.cpp", stated=False)


@fig("t_lib_touch_read", "ms", stated=False, prints="up",
     rises_with=["n_lib_touch_bytes", "n_bits_byte", "n_conditions"], falls_with=["f_scl"])
def _(n_lib_touch_bytes, n_bits_byte, n_conditions, f_scl):
    return (n_lib_touch_bytes * n_bits_byte + n_conditions) / f_scl


@fig("lib_touch_share", "%", stated=False, prints="up",
     rises_with=["n_lib_touch_bytes"], falls_with=["f_scl", "t_ttick"])
def _(t_lib_touch_read, t_ttick):
    return t_lib_touch_read / t_ttick


# the rise between the input thresholds with the module's pull-up alone; the Teensy's
# own pull-up in parallel only makes it faster
@fig("t_r_touch", "ns", group="TOUCH", section=TOUCH, prints="up",
     rises_with=["r_pu_touch", "c_touch", "vih_frac"], falls_with=["vil_frac"])
def _(r_pu_touch, c_touch, vil_frac, vih_frac):
    return r_pu_touch * c_touch * ln((1 - vil_frac) / (1 - vih_frac))


# ---------------------------------------------------------------------------
# the gestures the touch driver reports
# ---------------------------------------------------------------------------
dec("n_slop", 10, "", src="the pixels a finger may move and still count as resting, which "
    "the touch driver also takes as the step between two TouchMoved", stated=False)
dec("t_hold", 1, "s", src="how long a finger has to rest before the touch driver reports "
    "TouchHeld", stated=False)
dec("t_tap", 500, "ms", src="the longest touch the screen component counts as a tap",
    stated=False)
MODEL.as_written("t_hold_ms", of="t_hold", unit="ms")
MODEL.as_written("t_tap_ms", of="t_tap", unit="ms")
MODEL.uses("prio_display", of="firmware")


@fig("n_hold_ticks", "", stated=False, rises_with=["t_hold"], falls_with=["t_ttick"])
def _(t_hold, t_ttick):
    return t_hold / t_ttick


# ===========================================================================
# the supply: the module's own regulator makes VCC from the power board's 5 V
# ===========================================================================
MODEL.uses("v_not_tolerant", of="teensy-4.1")
MODEL.uses("v_3v3_min", of="teensy-4.1")
MODEL.uses("v_rail", of="lighting", group="SUPPLY", section=SUPPLY, stated=True)
MODEL.uses("v_jack_max", of="power", group="SUPPLY", section=SUPPLY, stated=True)
MODEL.uses("v_release_6a", of="power", group="SUPPLY", section=SUPPLY, stated=True)
MODEL.uses("t_a", of="ir-reflective", group="SUPPLY", section=SUPPLY, stated=True)
ds("v_ldo_acc", 1.5, "%", sheet=LDO, src="Electrical Characteristics, output voltage accuracy",
   group="VCC", section=SUPPLY, stated=True)
ds("v_in_ldo_max", 6, "V", sheet=LDO, src="Recommended Operating Conditions, supply voltage "
   "maximum", group="SUPPLY", section=SUPPLY, stated=True, tol_units=0.5)
ds("rth_ldo", 184, "°C/W", sheet=LDO, src="thermal resistance junction to ambient of the SOT25 "
   "with no heatsink", group="SUPPLY", section=SUPPLY, stated=True)
ds("t_j_ldo_max", 150, "°C", sheet=LDO, src="Absolute Maximum Ratings, operating junction "
   "temperature", group="SUPPLY", section=SUPPLY, stated=True)
ds("v_gs_th_bss", 1.5, "V", sheet=BSS, src="BSS138 Rev C, On Characteristics, VGS(th) maximum "
   "at ID = 1 mA", group="TOUCH", section=TOUCH, stated=True)
ds("v_dss_bss", 50, "V", sheet=BSS, src="BSS138 Rev C, Absolute Maximum Ratings, drain-source "
   "voltage", stated=False)
ds("r_led", 10, "Ω", src=f"{SCH}: R1 to R4, one from VCC to each LED's anode",
   group="SUPPLY", section=SUPPLY, stated=True)
ds("n_led", 4, "", sheet=GLASS, src="8 Backlight Characteristics: 4 white LEDs in parallel, "
   "p. 11", stated=False)
ds("vf_min", 2.9, "V", sheet=GLASS, src="8 Backlight Characteristics, Vf minimum at "
   "If = 80 mA for the four, p. 11", group="SUPPLY", section=SUPPLY, stated=True)
asm("i_panel", 10, "mA", src="the ILI9341's own draw, which ILI9341_DS_V1.11 states only for "
    "standby, 100 µA at most; taken as a round ceiling", group="SUPPLY", section=SUPPLY,
    stated=True)
ds("i_touch", 5, "mA", sheet=TOUCH_IC, src="IDD at VDD = 3.3 V, maximum, p. 8",
   group="SUPPLY", section=SUPPLY, stated=True)
ds("r_pull", 10, "kΩ", src=f"{SCH}: R12 from RD to GND, and R13 to R16, the bus pull-ups",
   stated=False)
dec("n_pulls", 5, "", src="R12, which RD holds at Vin all the time, and the four pull-ups of "
    "the touch bus, counted as if their lines were low all the time", stated=False)
ds("i_buffer", 10, "µA", sheet=BUFFER, src="7.5 Electrical Characteristics, ICC maximum from "
   "-40 °C to 85 °C", stated=False)
ds("i_sup", 40, "µA", sheet=SUPERVISOR, src="Electrical Characteristics, ICC maximum, p. 4",
   stated=False)
ds("i_ldo_q", 80, "µA", sheet=LDO, src="Electrical Characteristics, quiescent current IQ "
   "maximum", stated=False)


# VCC is the module regulator's output, which runs from the 5 V rail; each LED takes the
# rest over its own resistor, and a forward voltage at its lowest takes the most
@fig("v_vcc_max", "V", group="SUPPLY", section=SUPPLY, prints="up",
     rises_with=["v_3v3", "v_ldo_acc"])
def _(v_3v3, v_ldo_acc):
    return v_3v3 * (1 + v_ldo_acc.raw)


@fig("v_vcc_min", "V", group="VCC", section=SUPPLY, prints="down",
     rises_with=["v_3v3"], falls_with=["v_ldo_acc"])
def _(v_3v3, v_ldo_acc):
    return v_3v3 * (1 - v_ldo_acc.raw)


@fig("i_led_max", "mA", group="SUPPLY", section=SUPPLY, prints="up",
     rises_with=["v_3v3", "v_ldo_acc"], falls_with=["vf_min", "r_led"])
def _(v_vcc_max, vf_min, r_led):
    return (v_vcc_max - vf_min) / r_led


@fig("i_backlight", "mA", group="SUPPLY", section=SUPPLY, prints="up",
     rises_with=["v_3v3", "n_led"], falls_with=["vf_min", "r_led"])
def _(i_led_max, n_led):
    return n_led * i_led_max


@fig("i_module", "mA", group="SUPPLY", section=SUPPLY, prints="up",
     rises_with=["v_3v3", "n_led", "i_panel", "i_touch", "n_pulls"],
     falls_with=["vf_min", "r_led", "r_pull"])
def _(i_backlight, i_panel, i_touch, n_pulls, v_vcc_max, r_pull, i_buffer, i_sup, i_ldo_q):
    return (i_backlight + i_panel + i_touch + n_pulls * v_vcc_max / r_pull
            + 2 * i_buffer + i_sup + i_ldo_q)


# the converter's pull-ups, both lines held low: the LV side out of the Teensy's 3V3, the
# HV side out of Vin
@fig("i_conv_lv", "mA", group="TOUCH", section=TOUCH, prints="up",
     rises_with=["v_3v3"], falls_with=["r_pu_touch"])
def _(v_3v3, r_pu_touch):
    return 2 * v_3v3 / r_pu_touch


@fig("i_conv_hv", "mA", group="SUPPLY", section=SUPPLY, prints="up",
     rises_with=["v_rail"], falls_with=["r_pu_touch"])
def _(v_rail, r_pu_touch):
    return 2 * v_rail / r_pu_touch


# J-DISP carries the module and the converter's HV pull-ups
@fig("i_disp", "mA", group="SUPPLY", section=SUPPLY, prints="up",
     rises_with=["v_3v3", "n_led", "v_rail"], falls_with=["vf_min", "r_led", "r_pu_touch"])
def _(i_module, i_conv_hv):
    return i_module + i_conv_hv


# the module's regulator drops the rail to VCC; the adapter at its no-load maximum and VCC
# at the bottom of its tolerance put the most across it
@fig("p_ldo", "W", group="SUPPLY", section=SUPPLY, prints="up",
     rises_with=["v_jack_max", "n_led"], falls_with=["r_led"])
def _(v_jack_max, v_vcc_min, i_module):
    return (v_jack_max - v_vcc_min) * i_module


@fig("t_j_ldo", "°C", group="SUPPLY", section=SUPPLY, prints="up",
     rises_with=["t_a", "v_jack_max", "rth_ldo"])
def _(t_a, p_ldo, rth_ldo):
    return t_a + p_ldo * rth_ldo


# the Teensy's rail at its lowest is what switches the converter's transistors on
# ---------------------------------------------------------------------------
# VCC: the module's regulator in dropout, between the ILI9341's range and the supervisor
# ---------------------------------------------------------------------------
ds("v_do_max", 200, "mV", sheet=LDO, src="Electrical Characteristics, dropout voltage at "
   "IOUT = 300 mA, maximum, 3.3 V version", stated=False)
ds("i_do", 300, "mA", sheet=LDO, src="the current that dropout figure is given at",
   stated=False)
ds("i_ldo_max", 600, "mA", sheet=LDO, src="Electrical Characteristics, output current, "
   "minimum guaranteed", stated=False)
ds("vci_min", 2.5, "V", sheet=PANEL, src="18.2.1, VCI operating voltage, minimum, p. 236",
   group="VCC", section=SUPPLY, stated=True)
ds("vci_max", 3.3, "V", sheet=PANEL, src="18.2.1, VCI and VDDI operating voltage, maximum, "
   "p. 236", group="VCC", section=SUPPLY, stated=True)
ds("vci_abs", 4.6, "V", sheet=PANEL, src="18.1 Absolute Maximum Ratings, VCI and VDDI, "
   "p. 235", group="VCC", section=SUPPLY, stated=True)
asm("v_sup_max", 3, "V", src="the supervisor's threshold: Adafruit's schematic names no "
    "variant of the APX803, and Adafruit rates Vin down to 3 V, under which the module's "
    "regulator leaves less than 3 V on VCC, so the fitted threshold lies below 3 V",
    group="VCC", section=SUPPLY, stated=True)
ds("v_sup_hyst", 40, "mV", sheet=SUPERVISOR, src="Electrical Characteristics, reset threshold "
   "hysteresis, typical, p. 4; the sheet gives no maximum", stated=False)


# the dropout grows with the current through the regulator's pass transistor
@fig("v_dropout", "mV", group="VCC", section=SUPPLY, prints="up",
     rises_with=["v_do_max", "v_3v3", "n_led", "i_panel"], falls_with=["i_do", "vf_min"])
def _(v_do_max, i_do, i_module):
    return v_do_max * i_module / i_do


# the least Vin the module's regulator keeps VCC at its top from
@fig("v_in_mod_min", "V", group="VCC", section=SUPPLY, prints="up",
     rises_with=["v_3v3", "v_ldo_acc", "v_do_max", "n_led"], falls_with=["i_do", "vf_min"])
def _(v_vcc_max, v_dropout):
    return v_vcc_max + v_dropout


# at the top of the regulator's tolerance VCC passes the ILI9341's operating maximum by this
@fig("v_over_vci", "mV", group="VCC", section=SUPPLY, prints="up",
     rises_with=["v_3v3", "v_ldo_acc"], falls_with=["vci_max"])
def _(v_vcc_max, vci_max):
    return v_vcc_max - vci_max


# ---------------------------------------------------------------------------
# start-up: the supervisor's reset, then the panel and the touch controller
# ---------------------------------------------------------------------------
ds("t_sup_min", 140, "ms", sheet=SUPERVISOR, src="Electrical Characteristics, reset active "
   "timeout period of APX803-XX, minimum, p. 4", stated=False)
ds("t_sup_max", 280, "ms", sheet=SUPERVISOR, src="Electrical Characteristics, reset active "
   "timeout period of APX803-XX, maximum, p. 4", group="START", section=RESET, stated=True)
ds("t_reset_wait", 5, "ms", sheet=PANEL, src="15.4 Reset Timing, note 7: 5 ms after RESX "
   "rises before a command, p. 230", group="START", section=RESET, stated=True)
ds("t_swreset_wait", 120, "ms", sheet=PANEL, src="Software Reset (01h): after a software reset in "
   "Sleep Out mode, 120 ms before the Sleep Out command, p. 90", stated=False)
MODEL.as_written("t_swreset_wait_ms", of="t_swreset_wait", unit="ms")
ds("t_sleep_out", 120, "ms", sheet=PANEL, src="Sleep Out (11h): 120 ms until Sleep Out mode "
   "is reached, p. 101", group="START", section=RESET, stated=True)
ds("t_touch_start", 300, "ms", src="FocalTech FT6x06 datasheet V0.1, Table 3-5, Trsi, the "
   "time from reset to the first I2C answer; the CST026's sheet states none",
   group="START", section=RESET, stated=True)
MODEL.as_written("t_reset_wait_ms", of="t_reset_wait", unit="ms")
MODEL.as_written("t_sleep_out_ms", of="t_sleep_out", unit="ms")


@fig("t_panel_ready", "ms", group="START", section=RESET, prints="up",
     rises_with=["t_sup_max", "t_reset_wait"])
def _(t_sup_max, t_reset_wait):
    return t_sup_max + t_reset_wait


@fig("t_touch_ready", "ms", group="START", section=RESET, prints="up",
     rises_with=["t_sup_max", "t_touch_start"])
def _(t_sup_max, t_touch_start):
    return t_sup_max + t_touch_start


MODEL.as_written("t_panel_ready_ms", of="t_panel_ready", unit="ms")
MODEL.as_written("t_touch_ready_ms", of="t_touch_ready", unit="ms")

# ===========================================================================
# RAM2: the three layers and the movie's ring
# ===========================================================================
MODEL.uses("ram2", of="teensy-4.1")


@fig("ram_display", "KB", group="FRAME", section=FRAME, prints="up",
     rises_with=["n_layers", "n_cols", "n_rows", "ring"])
def _(n_layers, b_layer, ring):
    return n_layers * b_layer + ring


# ===========================================================================
# requirements
# ===========================================================================
_I = MODEL.invariant
_I("a single failed touch read does not count as a failure",
   lambda v: v.t_fail > 2 * v.t_ttick)
_I("the shortest supervisor reset silences the touch controller longer than its failure time",
   lambda v: v.t_fail < v.t_sup_min)
_I("a run ends only once its last burst has left the bus, however late the interrupt comes",
   lambda v: v.t_end_wait > v.t_burst)
_I("FlexIO3 runs at a clock the reference manual allows",
   lambda v: v.f_flexio <= v.f_flexio_max)
_I("FlexIO3's clock is PLL3 divided by whole numbers",
   lambda v: (v.f_pll3 / v.f_flexio).raw.is_integer())
_I("the supervisor's shortest reset ends before its longest",
   lambda v: v.t_sup_min < v.t_sup_max)
_I("a write on the bus lasts at least the ILI9341's write cycle",
   lambda v: v.t_wr >= v.t_wc_min)
_I("WR stays low and high for the ILI9341's shortest pulse",
   lambda v: v.t_half >= v.t_strobe_min)
_I("the data is set up and held at the panel for at least what the ILI9341 asks",
   lambda v: v.t_sh >= v.t_sh_min)
_I("an edge from a bus pin stays inside the transition rate the module's buffers allow",
   lambda v: v.dt_dv <= v.dt_dv_max)
_I("the Teensy's high clears the buffers' high input level",
   lambda v: v.v_oh_min > v.vih_buf)
_I("the Teensy's low stays under the buffers' low input level",
   lambda v: v.v_ol_max < v.vil_buf)
_I("a refill that blends two layers at every pixel still ends inside its burst, so the bus "
   "never waits", lambda v: v.n_isr_blend < v.n_isr_budget)
_I("a burst is a whole number of pixels",
   lambda v: v.n_burst_px.raw.is_integer())
_I("the fullscreen viewport is a whole number of viewport steps wide",
   lambda v: (v.n_cols / v.n_viewport_step).raw.is_integer())
_I("a row is a whole number of bursts, so a job never ends inside a burst",
   lambda v: (v.n_cols * v.b_pixel / v.n_burst).raw.is_integer())
_I("a movie frame lasts a whole number of the display's ticks",
   lambda v: v.n_frame_ticks.raw.is_integer())
_I("the display's tick is a whole number of driver ticks, which attach() requires",
   lambda v: (v.t_dtick / v.t_tick).raw.is_integer())
_I("the touch driver's tick is a whole number of driver ticks, which attach() requires",
   lambda v: (v.t_ttick / v.t_tick).raw.is_integer())
_I("TouchHeld comes after a whole number of the touch driver's ticks",
   lambda v: v.n_hold_ticks.raw.is_integer())
_I("a full frame on the bus fits inside a movie frame",
   lambda v: v.t_frame_bus < v.t_movie)
_I("the ring holds a whole number of chunks, and more than one",
   lambda v: v.n_ring.raw.is_integer() and v.n_ring.raw > 1)
_I("the card carries a full-screen movie and the four audio streams, by the estimate",
   lambda v: v.card_load.raw < 1)
_I("and by the A1 class alone, which guarantees random reads of 4 KB",
   lambda v: v.card_load_a1.raw < 1)
_I("a touch read ends well inside the touch driver's tick",
   lambda v: v.t_touch_read < v.t_ttick)
_I("a rising edge on the touch bus stays inside the Standard-mode limit",
   lambda v: v.t_r_touch < v.t_r_std)
_I("the touch bus runs at a clock the touch controller takes",
   lambda v: v.f_scl <= v.f_scl_max)
_I("a tap is shorter than a hold",
   lambda v: v.t_tap < v.t_hold)
_I("the module stays inside what its regulator delivers",
   lambda v: v.i_module < v.i_ldo_max)
_I("VCC stays above the ILI9341's lowest supply",
   lambda v: v.v_vcc_min > v.vci_min)
_I("VCC stays above the supervisor's threshold and its hysteresis, so the panel leaves reset",
   lambda v: v.v_vcc_min > v.v_sup_max + v.v_sup_hyst)
_I("VCC stays under the ILI9341's absolute maximum",
   lambda v: v.v_vcc_max < v.vci_abs)
_I("the module's regulator stays under its junction rating with no heatsink",
   lambda v: v.t_j_ldo < v.t_j_ldo_max)
_I("the rail's highest peak stays inside the module regulator's input range",
   lambda v: v.v_release_6a <= v.v_in_ldo_max)
_I("the Teensy's rail at its lowest switches the converter's transistors on",
   lambda v: v.v_3v3_min > v.v_gs_th_bss)
_I("the converter's transistors take the rail on their HV side",
   lambda v: v.v_release_6a < v.v_dss_bss)
_I("the touch controller answers only after the panel's reset is over",
   lambda v: v.t_touch_ready > v.t_panel_ready)

for _text, _why in [
    ("25", "the frame rate in FFmpeg's fps filter, the movies' 25 frames a second"),
    ("81.3 mm", "the module's width, Adafruit's product page"),
    ("62.5 mm", "the module's height, Adafruit's product page"),
    ("76.2 mm", "the distance of the module's holes across, Adafruit's product page"),
    ("57.2 mm", "the distance of the module's holes down, Adafruit's product page"),
    ("3 mm", "the module's holes, Adafruit's EAGLE board, MOUNTINGHOLE_3.0"),
]:
    MODEL.aside(_text, _why)


# ===========================================================================
# wiring
# ===========================================================================
_LVC = (f"{SCH}: every bus line from the 8-bit header enters a 74LVC245 input on the module "
        "and drives nothing back while RD is high")
for _sig in ("D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7"):
    MODEL.net(_sig, reads(f"the module's data buffer, input {_sig}", src=_LVC),
              teensy=f"{_sig} to the display")
MODEL.net("WR", reads("the module's control buffer, input WR", src=_LVC),
          teensy="WR to the display")
MODEL.net("CS", reads("the module's control buffer, input CS", src=_LVC),
          teensy="CS to the display controller")
MODEL.net("D/C", reads("the module's control buffer, input C/D", src=_LVC),
          teensy="D/C to the display")

_I2C = (f"{LVL}: each line passes a BSS138 whose gate sits at LV, the Teensy's 3V3, with a "
        "10 kΩ pull-up to LV on the Teensy's side and one to HV, the display's Vin, on the "
        f"module's side; {SCH}: R13 and R14 pull SCL and SDA up to Vin on the module, Q2 and "
        "Q3, BSS138, pass them to the touch controller")
_WIRE = "docs/firmware/drivers/touch.md, Wire2.begin() sets up the pins and their pull-ups"
MODEL.net("SDA",
          drives("the converter's BSS138, which passes the touch controller's low",
                 "open-drain", src=_I2C),
          pull("the converter's LV pull-up", TEENSY_RAIL, src=_I2C),
          pull("the Teensy's internal pull-up", TEENSY_RAIL, src=_WIRE),
          teensy="SDA to the touch controller")
MODEL.net("SCL",
          drives("the converter's BSS138, which passes the touch controller's low",
                 "open-drain", src=_I2C),
          pull("the converter's LV pull-up", TEENSY_RAIL, src=_I2C),
          pull("the Teensy's internal pull-up", TEENSY_RAIL, src=_WIRE),
          teensy="SCL to the touch controller")

# the module's side of the touch bus, between the converter and the module
MODEL.net("SDA at the module",
          drives("the converter's BSS138, which passes the Teensy's low", "open-drain", src=_I2C),
          drives("Q3 on the module, for the touch controller", "open-drain", src=_I2C),
          pull("the converter's HV pull-up", "display Vin", src=_I2C),
          pull("R14 on the module", "display Vin", src=_I2C))
MODEL.net("SCL at the module",
          drives("the converter's BSS138, which passes the Teensy's low", "open-drain", src=_I2C),
          drives("Q2 on the module, for the touch controller", "open-drain", src=_I2C),
          pull("the converter's HV pull-up", "display Vin", src=_I2C),
          pull("R13 on the module", "display Vin", src=_I2C))

# the module's own lines that reach no Teensy pin
MODEL.net("RD",
          drives("the wire from RD to 3.3Vout on the header", "push-pull", rail="module 3.3Vout",
                 src="design.md, RD is tied to the module's 3.3Vout"),
          pull("R12 on the module", "gnd", src=f"{SCH}: R12 from RD to GND"),
          reads("the control buffer's input RD, and the data buffer's direction",
                src=f"{SCH}: RD drives U3's DIR"))
MODEL.net("CCS",
          drives("the wire from CCS to 3.3Vout on the header", "push-pull", rail="module 3.3Vout",
                 src="design.md, the card's chip select is tied to the module's 3.3Vout"),
          reads("the control buffer's input for the card's chip select", src=_LVC))
MODEL.net("MOSI",
          drives("the wire from MOSI to GND on the header", "push-pull", rail="gnd",
                 src="design.md, the unused buffer input MOSI is tied to GND"),
          reads("the control buffer's input MOSI", src=_LVC))

MODEL.owns("FlexIO3", "the driver programs its shifters and its timer to clock the bus, "
                      "docs/firmware/drivers/display.md")
MODEL.owns("Wire2", "the touch driver runs the LPI2C4 controller itself after begin(), "
                    "docs/firmware/drivers/touch.md")
MODEL.draws("i_conv_lv", pool=TEENSY_RAIL)   # the converter's LV pull-ups, fed from the Teensy's 3V3
MODEL.draws("ram_display", pool="RAM2")
MODEL.draws(pool="driver tick")      # the display driver: the movie's frames, every 40 ms
MODEL.draws(pool="driver tick")      # the touch driver: the touch read, every 20 ms
MODEL.draws(pool="device monitor")   # the touch driver: the touch controller
