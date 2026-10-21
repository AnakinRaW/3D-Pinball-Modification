"""The figures of the PSRAM on the Teensy's underside, as inputs plus formulas.

Run with:  python tools/figcheck.py docs/parts/psram/figures.py

Every input names where it comes from. Every derived quantity is a function
whose parameter names are its dependencies, so `--graph <key>` answers what a
value feeds and what it rests on.
"""
import pathlib

from figcheck import Model, TEENSY_RAIL, drives, pull, reads

HERE = pathlib.Path(__file__).resolve().parent
MODEL = Model("psram", HERE / "design.md", section=None, until="## Sources")

ds = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="datasheet", **kw)
asm = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="assumed", **kw)
fig = MODEL.derived

PSRAM_SHEET = "APS6404L-3SQR-APMemory.pdf"
CORE = "PJRC's cores/teensy4"
CHIP = "Chip"

# ===========================================================================
# the chip on the smaller memory pads, an AP Memory APS6404L-3SQR
# ===========================================================================
# tools/pdftext.py cannot map this sheet's subset fonts, so --sheets cannot look these
# readings up. They are read from the rendered page and cite the sheet in their source.
ds("psram_size", 8, "MB", src=f"{PSRAM_SHEET}, Rev. 2.3, page 1: 64 Mb, organized as "
   "8M x 8 bits; read from the rendered page", group="PSRAM", section=CHIP, stated=True)
ds("i_psram_cc", 7, "mA", src=f"{PSRAM_SHEET}, Rev. 2.3, DC characteristics: I_CC "
   "read/write maximum, which its note 1 gives without the output load current; read "
   "from the rendered page", group="PSRAM", section=CHIP, stated=True)
ds("f_psram_max", 109, "MHz", src=f"{PSRAM_SHEET}, Rev. 2.3, page 1: the clock rate for "
   "32-byte wrapped bursts at VDD = 3.3 V ±10 %; read from the rendered page",
   group="PSRAM", section=CHIP, stated=True)
ds("f_psram", 105.6, "MHz", src=f"{CORE}/startup.c, configure_external_ram(): "
   "CCM_CBCMR_FLEXSPI2_PODF(4) with CLK_SEL(3), the FlexSPI2 clock from Teensyduino 1.60 on",
   group="PSRAM", section=CHIP, stated=True)
ds("n_psram_lines", 4, "", src=f"{CORE}/startup.c, configure_external_ram(): the read and "
   "the write sequence carry their data on four lines, PINS4")
ds("c_psram_line", 15, "pF", src=f"{PSRAM_SHEET}, Rev. 2.3, Table 7: load capacitance CL "
   "maximum, the system load the chip is specified for on each pin; read from the rendered "
   "page", group="PSRAM", section=CHIP, stated=True)
MODEL.uses("v_3v3_max", of="teensy-4.1", group="PSRAM", section=CHIP, stated=True)
MODEL.uses("i_teensy_3v3", of="teensy-4.1")


# A data line that flips on every clock swings at half the clock, and each swing charges
# the line's load once. The datasheet's I_CC leaves this current out.
@fig("i_psram_lines", "mA", group="PSRAM", section=CHIP, prints="up",
     rises_with=["n_psram_lines", "c_psram_line", "v_3v3_max", "f_psram"])
def _(n_psram_lines, c_psram_line, v_3v3_max, f_psram):
    return n_psram_lines * c_psram_line * v_3v3_max * f_psram / 2


@fig("i_psram", "mA", group="PSRAM", section=CHIP, prints="up",
     rises_with=["i_psram_cc", "n_psram_lines", "c_psram_line", "v_3v3_max", "f_psram"])
def _(i_psram_cc, i_psram_lines):
    return i_psram_cc + i_psram_lines


MODEL.draws("i_psram", pool=TEENSY_RAIL)   # the PSRAM on the underside, fed from the Teensy's 3V3
MODEL.invariant("FlexSPI2 clocks the PSRAM inside the rate its datasheet gives at 3.3 V",
                lambda v: v.f_psram <= v.f_psram_max)

# ===========================================================================
# wiring
# ===========================================================================
# configure_external_ram() sets the pads at every boot: a 100 kΩ pull-up on the chip select,
# a 47 kΩ pull-up on each data line and none on the clock
STARTUP = f"{CORE}/startup.c, configure_external_ram()"
MODEL.net("CE#",
          reads("APS6404L CE#", src=f"{PSRAM_SHEET}, the chip select input"),
          pull("the pad's 100 kΩ pull-up", TEENSY_RAIL,
               src=f"{STARTUP}, IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_24 = 0x1B0F9"),
          teensy="PSRAM chip select")
MODEL.net("CLK",
          reads("APS6404L CLK", src=f"{PSRAM_SHEET}, the clock input"),
          teensy="PSRAM clock")
for line, pad in ((0, 26), (1, 27), (2, 28), (3, 29)):
    MODEL.net(f"SIO{line}",
              drives(f"APS6404L SIO{line}", "tri-state", rail=TEENSY_RAIL,
                     select="CE# low in the data phase of a read",
                     src=f"{PSRAM_SHEET}, Figure 8: the chip drives SIO0 to SIO3 only while it "
                         "sends the data of a quad read, and holds them high-Z otherwise"),
              pull("the pad's 47 kΩ pull-up", TEENSY_RAIL,
                   src=f"{STARTUP}, IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_{pad} = 0x170F9"),
              teensy=f"PSRAM data {line}")

MODEL.owns("FlexSPI2", f"{STARTUP} sets the controller up at every start, and EXTMEM reaches "
                       "the PSRAM through it, docs/firmware/general-design.md")
