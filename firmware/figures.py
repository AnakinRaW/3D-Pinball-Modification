"""The figures of the firmware: its periods, its watchdog, its interrupts and its slots.

Run with:  python tools/figcheck.py firmware/figures.py

A subsystem takes a figure from here with MODEL.uses(key, of="firmware"). A driver
that attaches to the driver tick or the device monitor draws one of its slots, and
the run over the whole tree adds the draws of every model up against the slots
declared here.
"""
import math
import pathlib

from figcheck import Model

HERE = pathlib.Path(__file__).resolve().parent
MODEL = Model("firmware", HERE / "general-design.md", section=None, until="## Sources",
              documents=[HERE / "driver-design.md", HERE / "error-handling.md"])

ds = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="datasheet", **kw)
dec = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="decision", **kw)
fig = MODEL.derived

CORE = "PJRC's cores/teensy4"
IRQ = "Interrupts"

# ===========================================================================
# the periods
# ===========================================================================
dec("t_tick", 5, "ms", src="the driver tick's period, driver-design.md, on which the "
    "rotary sensor's driver collects one read and starts the next; "
    "docs/parts/magnetic-rotary/figures.py requires the read to fit inside it")
dec("t_monitor", 100, "ms", src="how often the device monitor of error-handling.md asks "
    "every driver which parts of its device have failed")
dec("t_wdt", 2, "s", src="the watchdog timeout, error-handling.md; decided above the usual "
    "length of a write through SdFat, which firmware/drivers/storage.md describes, and well "
    "inside the five-second limit of the solenoids' requirements, so a coil whose timer "
    "never fires is released by the restart")

MODEL.as_written("t_tick_us", of="t_tick", unit="µs")
MODEL.as_written("t_monitor_us", of="t_monitor", unit="µs")
MODEL.as_written("t_wdt_s", of="t_wdt", unit="s")

# the one guard the watchdog keeps, as error-handling.md tabulates it
MODEL.uses("t_on_actual", of="solenoid", group="Solenoid Driver", section="The watchdog",
           stated=True)
MODEL.uses("t_hold_max", of="solenoid", group="Solenoid Driver", section="The watchdog",
           stated=True)

# ===========================================================================
# the slots
# ===========================================================================
dec("n_tick_slots", 8, "", src="the callbacks the driver tick holds, kSlots in "
    "driver-design.md")
dec("n_monitor_slots", 8, "", src="the drivers the device monitor watches, kSlots in "
    "error-handling.md")
dec("n_queue", 32, "", src="the depth of the device monitor's event queue, error-handling.md; "
    "a converter that stops answering fails eight parts at once")
ds("n_capture", 16, "bytes", src=f"what a driver tick callback may capture, the size "
   f"teensy::inplace_function takes in {CORE}/IntervalTimer.h")

MODEL.supplies("n_tick_slots", pool="driver tick", call="driverTick.attach(")
MODEL.supplies("n_monitor_slots", pool="device monitor", call="deviceMonitor.watch(")
MODEL.draws(pool="driver tick")   # the device monitor's own check

# ===========================================================================
# the interrupts, a lower number preempting a higher one
# ===========================================================================
ds("prio_default", 128, "", src=f"{CORE}/startup.c, which sets every interrupt to 128 "
   "before setup() runs")
dec("prio_ir", 64, "", src="the IR driver's two interrupts, whose read block has to end "
    "inside its phase", group="QuadTimer3 compare, starting the read block", section=IRQ,
    stated=True)
dec("prio_pins", 96, "", src="the one IRQ every pin interrupt shares, which setup() sets "
    "once", group="Pin interrupts, one IRQ shared by every pin", section=IRQ, stated=True)
dec("prio_tick", 96, "", src="the driver tick, kPriority in driver-design.md",
    group="`IntervalTimer`, the driver tick", section=IRQ, stated=True)
ds("prio_update", 208, "", src=f"{CORE}/AudioStream.cpp, NVIC_SET_PRIORITY(IRQ_SOFTWARE, "
   "208), the Audio library's update", group="The Audio library's update, computing a block",
   section=IRQ, stated=True)
dec("prio_card", 240, "", src="the storage driver's card interrupt, NVIC_SET_PRIORITY("
    "IRQ_SDHC1, 240) in firmware/drivers/storage.md",
    group="The SD controller's interrupt, ending one card transfer and starting the next",
    section=IRQ, stated=True)


@fig("prio_ir_spi", "", group="The interrupt after each SPI conversion", section=IRQ)
def _(prio_ir):
    return prio_ir


# output_i2s2.cpp attaches its DMA interrupt without a priority, so it keeps the default
@fig("prio_dma", "", group="I²S2 DMA, handing a block to the amplifier", section=IRQ)
def _(prio_default):
    return prio_default


# ===========================================================================
# what the firmware requires
# ===========================================================================
_I = MODEL.invariant

_I("the IR driver's interrupts preempt the pin interrupts and the driver tick, whose "
   "deadlines are milliseconds",
   lambda v: v.prio_ir < v.prio_pins and v.prio_ir < v.prio_tick)
_I("the Audio library's two interrupts sit below every driver interrupt",
   lambda v: min(v.prio_dma, v.prio_update) > max(v.prio_ir, v.prio_pins, v.prio_tick))
_I("the storage driver's card interrupt sits beneath both, the lowest of all",
   lambda v: v.prio_card > max(v.prio_dma, v.prio_update, v.prio_ir, v.prio_pins,
                               v.prio_tick))
_I("the device monitor checks on whole driver ticks",
   lambda v: (v.t_monitor / v.t_tick).raw.is_integer())
_I("the device monitor's queue has a depth the event queue accepts, a power of two",
   lambda v: math.log2(v.n_queue.raw).is_integer())

for _text, _why in [
    ("0", "a loop's start, an empty count or mask, and a pure virtual function in the listings"),
    ("1", "the step a count or a mask moves by in the listings"),
]:
    MODEL.aside(_text, _why)
