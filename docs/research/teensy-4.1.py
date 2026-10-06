"""The figures of the Teensy 4.1 and its i.MX RT1062, as PJRC and NXP publish them.

Run with:  python tools/figcheck.py docs/research/teensy-4.1.py --sheets

A subsystem takes a figure from here with MODEL.uses(key, of="teensy-4.1"), so the
board's figures are typed once and every document that states one follows it.
"""
import pathlib

from figcheck import Model, TEENSY_RAIL

HERE = pathlib.Path(__file__).resolve().parent
MODEL = Model("teensy-4.1", HERE / "teensy-4.1.md", section=None, until="## Sources")

ds = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="datasheet", **kw)
dec = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="decision", **kw)
asm = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="assumed", **kw)
fig = MODEL.derived

MCU = "IMXRT1060CEC.pdf"
PAGE = "Teensy 4.1 product page"
CARD = "PJRC pin assignment card 11a rev4"
CORE = "PJRC's cores/teensy4"

SPEC = "Specifications"
POWER = "Power"
UNPOWERED = "The only quantified limit at a pin is a voltage"

# ===========================================================================
# the board, as PJRC publishes it
# ===========================================================================
ds("f_cpu", 600, "MHz", src=f"{PAGE}, the processor clock", group="Processor",
   section=SPEC, stated=True)
ds("flash", 7936, "KB", src=PAGE, group="Flash", section=SPEC, stated=True)
ds("ram", 1024, "KB", src=PAGE, group="RAM", section=SPEC, stated=True)
ds("ram1", 512, "KB", src=f"{PAGE}, the tightly coupled half, RAM1", group="RAM",
   section=SPEC, stated=True)
ds("eeprom", 4, "KB", src=f"{PAGE}, emulated in flash", group="EEPROM", section=SPEC,
   stated=True)
ds("n_io", 55, "", src=f"{PAGE}, the digital I/O pins")
ds("n_edge", 42, "", src=f"{PAGE}, the digital I/O pins a breadboard reaches")
ds("n_analog", 18, "", src=PAGE, group="Analog inputs", section=SPEC, stated=True)
ds("n_pwm", 35, "", src=PAGE, group="PWM outputs", section=SPEC, stated=True)
ds("v_3v3", 3.3, "V", src=f"{CARD}, the 3.3 V rail, which is the level every pin drives "
   "and, as the product page states, the most any pin may be driven to",
   group="Logic level", section=SPEC, stated=True)
ds("v_not_tolerant", 5, "V", src=f"the level the {PAGE} names in 'The pins are not 5V "
   "tolerant'")
ds("v_3v3_acc", 1, "%", src="TI TLV757P datasheet, SBVS322C, 5.5 Electrical Characteristics, "
   "output accuracy ±1 % from -40 °C to 85 °C at VOUT ≥ 1 V; the board's one 3.3 V "
   "regulator is a TLV75733P, PJRC's Teensy 4.1 schematic")

ds("v_in_min", 3.6, "V", src=f"{CARD}, VIN", group="VIN input range", section=POWER,
   stated=True)
ds("v_in_max", 5.5, "V", src=f"{CARD}, VIN", group="VIN input range", section=POWER,
   stated=True)
dec("i_pin_max", 4, "mA", src=f"the recommended maximum output current per pin, {PAGE}; "
    "the design holds every pin to it, since the comparison table's 10 mA spans every "
    "Teensy generation", group="Recommended maximum output current per pin",
    section=POWER, stated=True)
ds("i_pin_table", 10, "mA", src="PJRC technical specifications table, the output pin "
   "figure of a grid that spans every Teensy generation",
   group="Output pin figure in the comparison table", section=POWER, stated=True)
ds("i_teensy_3v3", 250, "mA", src=f"{CARD}, the 3.3 V rail available to external "
   "circuits, the total across both 3V3 header pins, which hang off the board's one "
   "regulator", group="3.3 V rail available to external circuits", section=POWER,
   stated=True)
ds("i_teensy_40", 100, "mA", src="PJRC's current draw at 600 MHz, published for the "
   "Teensy 4.0 and not for the 4.1", group="Current draw @ 600 MHz", section=POWER,
   stated=True)
dec("i_pin_fault_max", 1, "mA", src="PJRC calls under 1 mA into an out-of-range pin 'very "
    "unlikely to cause harm', and states that even this does not follow NXP's guidance; "
    "forum thread 60528, posts by Paul Stoffregen",
    group="A series resistor, 1 kΩ as the starting point", section=UNPOWERED, stated=True)

# ===========================================================================
# the pins, IMXRT1060CEC Rev. 4
# ===========================================================================
ds("v_in_below", 0.5, "V", sheet=MCU, src="Table 7, Vin/Vout minimum given as -0.5 V, how "
   "far below ground a pin may be driven")
ds("v_clamp_over", 0.31, "V", sheet=MCU, src="Table 7, Vin/Vout maximum given as "
   "OVDD + 0.31 V, which leaves 0.31 V on a pin while the Teensy is unpowered and is "
   "where the pin's protection diode takes over")
ds("v_oh_drop", 0.15, "V", sheet=MCU, src="Table 22 single voltage GPIO DC parameters, "
   "high-level output voltage VOH minimum, NVCC_XXXX - 0.15 V at Ioh = -1 mA for ipp_dse "
   "011 to 111; pinMode(OUTPUT) in PJRC's cores/teensy4/digital.c sets 7")
ds("v_ol_max", 0.15, "V", sheet=MCU, src="Table 22, low-level output voltage VOL maximum "
   "at Iol = 1 mA")
ds("i_oh_test", 1, "mA", sheet=MCU, src="Table 22, the current VOH and VOL are specified "
   "at for ipp_dse 011 to 111")
ds("vil_frac", 0.3, "", sheet=MCU, src="Table 22, low-level input voltage V_IL maximum, "
   "given as 0.3 x NVCC_XXXX")
ds("vih_frac", 0.7, "", sheet=MCU, src="Table 22, high-level input voltage V_IH minimum, "
   "given as 0.7 x NVCC_XXXX")
ds("r_keeper_min", 105, "kΩ", sheet=MCU, src="Table 22, keeper circuit resistance "
   "minimum, at V_I = 0.3 and 0.7 x NVCC_XXXX; Table 86 gives every edge pin a keeper "
   "on reset")
ds("i_pu_max", 212, "µA", sheet=MCU, src="Table 22, Pull-up resistor (22 kΩ PU), RPU_22K "
   "at Vin = 0 V, maximum")
asm("r_pullup", 22, "kΩ", src="the pad pull-up IOMUXC_PAD_PUS(3) selects, which "
    f"pinMode(INPUT_PULLUP) in {CORE}/digital.c and Wire in WireIMXRT.cpp both switch on; "
    "Table 22 names the setting 22 kΩ and bounds its current, i_pu_max, not its resistance")

ds("r_as_max", 1, "kΩ", sheet=MCU, src="Table 54, the maximum source resistance R_AS at "
   "12 bit, f_ADCK = 40 MHz and the 150 ns sample window")
ds("f_adck", 40, "MHz", sheet=MCU, src="Table 54, the ADC clock R_AS is given at")
ds("t_sample", 150, "ns", sheet=MCU, src="Table 54, the sample window R_AS is given at")
ds("r_as_chart", 10, "kΩ", sheet=MCU, src="Figures 36 to 38, the source resistance where "
   "the plotted sample times end")

# ===========================================================================
# the clocks and the memory
# ===========================================================================
ds("div_ipg", 4, "×", src=f"{CORE}/clockspeed.c, set_arm_clock(): the divider from the "
   "processor clock to the peripheral bus, the smallest that keeps the bus at or under "
   "150 MHz")
ds("qt_range", 65536, "steps", src="IMXRT1060RM Rev. 3, section 54.2: every QuadTimer "
   "channel counts in 16 bits")


@fig("f_bus", "MHz", stated=False, rises_with=["f_cpu"], falls_with=["div_ipg"])
def _(f_cpu, div_ipg):
    return f_cpu / div_ipg


# RAM2, the half of the RAM outside the tightly coupled memory, holds the DMA buffers
@fig("ram2", "KB", stated=False, rises_with=["ram"], falls_with=["ram1"])
def _(ram, ram1):
    return ram - ram1


MODEL.supplies("i_teensy_3v3", pool=TEENSY_RAIL)
MODEL.supplies("ram2", pool="RAM2")

# ===========================================================================
# the pins and what each can carry, which docs/pin-assignment.md tabulates
# ===========================================================================
CARD_PINS = "PJRC pin assignment card 11a and 11b rev4"
PWM_C = f"{CORE}/pwm.c, the pin table of the Teensy 4.1"

MODEL.pin_groups(edge=range(0, 42), sd=range(42, 48), pads=range(48, 55), src=CARD_PINS)

# A signal lists the pins that can carry it, alternatives in the order the card gives them.
# The roles set how a pin drives its net when it carries one, and a chip select can sit
# on any digital pin, so the port works with its CS pins taken.
MODEL.port_roles(inputs=("RX", "MISO", "data in", "in"), open_drain=("SDA", "SCL"),
                 optional=("CS",), src="the direction of each signal on the Teensy as master")
MODEL.port("Serial1", ("RX", 0), ("TX", 1), src=CARD_PINS)
MODEL.port("Serial2", ("RX", 7), ("TX", 8), src=CARD_PINS)
MODEL.port("Serial3", ("RX", 15), ("TX", 14), src=CARD_PINS)
MODEL.port("Serial4", ("RX", 16), ("TX", 17), src=CARD_PINS)
MODEL.port("Serial5", ("RX", 21), ("TX", 20), src=CARD_PINS)
MODEL.port("Serial6", ("RX", 25), ("TX", 24), src=CARD_PINS)
MODEL.port("Serial7", ("RX", 28), ("TX", 29), src=CARD_PINS)
MODEL.port("Serial8", ("RX", 34), ("TX", 35), src=CARD_PINS)
MODEL.port("SPI", ("MOSI", 11), ("MISO", 12), ("SCK", 13), ("CS", 10, 36, 37), src=CARD_PINS)
MODEL.port("SPI1", ("MOSI", 26), ("MISO", 1, 39), ("SCK", 27), ("CS", 0, 38), src=CARD_PINS)
MODEL.port("SPI2", ("MOSI", 43, 50), ("MISO", 42, 54), ("SCK", 45, 49), ("CS", 44),
           note="none on the edge headers", src=CARD_PINS)
MODEL.port("Wire", ("SDA", 18), ("SCL", 19), src=CARD_PINS)
MODEL.port("Wire1", ("SDA", 17), ("SCL", 16), src=CARD_PINS)
MODEL.port("Wire2", ("SDA", 25), ("SCL", 24), src=CARD_PINS)
MODEL.port("CAN1", ("RX", 23), ("TX", 22, 11), src=CARD_PINS)
MODEL.port("CAN2", ("RX", 0), ("TX", 1), src=CARD_PINS)
MODEL.port("CAN3", ("RX", 30), ("TX", 31), src=CARD_PINS)
MODEL.port("I²S1", ("MCLK", 23), ("BCLK", 21), ("LRCLK", 20), ("data out", 7, 32, 9, 6),
           ("data in", 8, 38), note="the Teensy Audio library's pin set", src=CARD_PINS)
MODEL.port("I²S2", ("MCLK", 33), ("BCLK", 4), ("LRCLK", 3), ("data out", 2), ("data in", 5),
           src=CARD_PINS)
MODEL.port("S/PDIF", ("out", 14), ("in", 15), src=CARD_PINS)

# A FlexPWM submodule drives its pins on channel A, B or X, a QuadTimer on channel 0 to 3.
MODEL.timer("FlexPWM1.0", (1, "X"), (44, "B"), (45, "A"), src=PWM_C)
MODEL.timer("FlexPWM1.1", (0, "X"), (42, "B"), (43, "A"), src=PWM_C)
MODEL.timer("FlexPWM1.2", (24, "X"), (46, "B"), (47, "A"), src=PWM_C)
MODEL.timer("FlexPWM1.3", (7, "B"), (8, "A"), (25, "X"), src=PWM_C)
MODEL.timer("FlexPWM2.0", (4, "A"), (33, "B"), src=PWM_C)
MODEL.timer("FlexPWM2.1", (5, "A"), src=PWM_C)
MODEL.timer("FlexPWM2.2", (6, "A"), (9, "B"), src=PWM_C)
MODEL.timer("FlexPWM2.3", (36, "A"), (37, "B"), src=PWM_C)
MODEL.timer("FlexPWM3.0", (54, "A"), src=PWM_C)
MODEL.timer("FlexPWM3.1", (28, "B"), (29, "A"), src=PWM_C)
MODEL.timer("FlexPWM3.3", (51, "B"), src=PWM_C)
MODEL.timer("FlexPWM4.0", (22, "A"), src=PWM_C)
MODEL.timer("FlexPWM4.1", (23, "A"), src=PWM_C)
MODEL.timer("FlexPWM4.2", (2, "A"), (3, "B"), src=PWM_C)
MODEL.timer("QuadTimer1", (10, "0"), (12, "1"), (11, "2"), src=PWM_C)
MODEL.timer("QuadTimer2", (13, "0"), src=PWM_C)
MODEL.timer("QuadTimer3", (19, "0"), (18, "1"), (14, "2"), (15, "3"), src=PWM_C)

# FlexIO3 carries signals 0 to 15 on the pads of GPIO1_IO16 to 31 and 16 to 31 on those of
# GPIO2_IO16 to 31, and the Teensy brings out the pins below
FLEXIO_RM = ("IMXRT1060RM Rev. 3, IOMUXC, FLEXIO3_FLEXIO00 to 31 on ALT9; the Teensy pin of "
             f"each pad from {CORE}/core_pins.h")
MODEL.flexio("FlexIO3", (19, 0), (18, 1), (14, 2), (15, 3), (40, 4), (41, 5), (17, 6), (16, 7),
             (22, 8), (23, 9), (20, 10), (21, 11), (38, 12), (39, 13), (26, 14), (27, 15),
             (8, 16), (7, 17), (36, 18), (37, 19), (35, 28), (34, 29), src=FLEXIO_RM)

# the pins of A0 to A17, in order
MODEL.analog(*range(14, 28), 38, 39, 40, 41, src=CARD_PINS)

# ===========================================================================
# what the pin data has to agree with
# ===========================================================================
_PINS = MODEL.pin_data
_I = MODEL.invariant
_I("the pin groups hold every digital pin the product page counts, each once",
   lambda v: sorted(p for g in _PINS.groups.values() for p in g) == list(range(int(v.n_io.raw))))
_I("and the edge headers the pins a breadboard reaches",
   lambda v: len(_PINS.groups["edge"]) == v.n_edge.raw)
_I("the timers reach every PWM pin the product page counts, each pin from one channel",
   lambda v: len(_PINS.pwm()) == len(set(_PINS.pwm())) == v.n_pwm.raw)
_I("the analog inputs are the ones the product page counts, each on its own pin",
   lambda v: len(_PINS.analog) == len(set(_PINS.analog)) == v.n_analog.raw)
_I("every pin a port, a timer or a FlexIO names is one of the board's pins",
   lambda v: {p for sigs in _PINS.ports.values() for _, pins in sigs for p in pins}
   | set(_PINS.pwm()) | set(_PINS.analog)
   | {p for pins in _PINS.flexio.values() for p, _ in pins} <= set(range(int(v.n_io.raw))))

# ===========================================================================
# what the document states that the model does not compute
# ===========================================================================
for _text, _why in [
    ("3", "the mux mode config_i2s() writes into each pin's config register"),
    ("1", "the SAI instance the code's comments name"),
]:
    MODEL.aside(_text, _why)
