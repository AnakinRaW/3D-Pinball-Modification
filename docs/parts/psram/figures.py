"""The figures of the PSRAM on the Teensy's underside, as inputs plus formulas."""
from figcheck import *

MODEL = Model.at(__file__, "psram")
fig, ds, gr, msr, asm, dec = MODEL.declarers()

PSRAM_SHEET = "APS6404L-3SQR-APMemory.pdf"
CORE = "PJRC's cores/teensy4"
CHIP = "Chip"

# -- the chip on the smaller memory pads, an AP Memory APS6404L-3SQR ----------
with MODEL.block("PSRAM", section=CHIP):
    # tools/pdftext.py cannot map this sheet's subset fonts, so --sheets cannot look these
    # readings up. They are read from the rendered page and cite the sheet in their source.
    ds("psram_size", 8, "MB", src=f"{PSRAM_SHEET}, Rev. 2.3, page 1: 64 Mb, organized as 8M x "
       "8 bits; read from the rendered page")
    ds("i_psram_cc", 7, "mA", src=f"{PSRAM_SHEET}, Rev. 2.3, DC characteristics: I_CC read/write "
       "maximum, which its note 1 gives without the output load current; read from the rendered "
       "page")
    ds("f_psram_max", 109, "MHz", src=f"{PSRAM_SHEET}, Rev. 2.3, page 1: the clock rate for "
       "32-byte wrapped bursts at VDD = 3.3 V ±10 %; read from the rendered page. Its lower "
       "rate for a linear burst holds only for a burst that crosses a 1 KB page, and FlexSPI2 "
       "sends none with the prefetch startup.c switches off: a cache line fills in one aligned "
       "32-byte burst")
    ds("f_psram", 105.6, "MHz", src=f"{CORE}/startup.c, configure_external_ram(): "
       "CCM_CBCMR_FLEXSPI2_PODF(4) with CLK_SEL(3), the FlexSPI2 clock from Teensyduino 1.60 on")
    ds("n_psram_lines", 4, "", src=f"{CORE}/startup.c, configure_external_ram(): the read and the "
       "write sequence carry their data on four lines, PINS4", stated=False)
    ds("c_psram_line", 15, "pF", src=f"{PSRAM_SHEET}, Rev. 2.3, Table 7: load capacitance CL "
       "maximum, the system load the chip is specified for on each pin; read from the rendered "
       "page")
    MODEL.uses("v_3v3_max", of="teensy-4.1")
    MODEL.uses("i_teensy_3v3", of="teensy-4.1", stated=False)

    # A data line that flips on every clock swings at half the clock, and each swing charges
    # the line's load once. The datasheet's I_CC leaves this current out.
    fig("i_psram_lines", "mA", "n_psram_lines * c_psram_line * v_3v3_max * f_psram / 2", "up",
        moves="+n_psram_lines +c_psram_line +v_3v3_max +f_psram")
    # The clock line flips twice every period and charges its load once a period.
    fig("i_psram_clk", "mA", "c_psram_line * v_3v3_max * f_psram", "up",
        moves="+c_psram_line +v_3v3_max +f_psram")
    fig("i_psram", "mA", "i_psram_cc + i_psram_lines + i_psram_clk", "up",
        moves="+i_psram_cc +n_psram_lines +c_psram_line +v_3v3_max +f_psram")

MODEL.draws("i_psram", pool=TEENSY_RAIL)   # the PSRAM on the underside, fed from the Teensy's 3V3
MODEL.supplies("psram_size", pool="PSRAM")
MODEL.invariant("FlexSPI2 clocks the PSRAM inside the rate its datasheet gives at 3.3 V",
                "f_psram <= f_psram_max")

# -- wiring -------------------------------------------------------------------
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
