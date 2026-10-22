"""The figures of the firmware: its periods, its watchdog, its interrupts and its slots.

Run with:  python tools/figcheck.py docs/firmware/figures.py

A subsystem takes a figure from here with MODEL.uses(key, of="firmware"). A driver
that attaches to the driver tick or the device monitor draws one of its slots, and
the run over the whole tree adds the draws of every model up against the slots
declared here.
"""
import math
import pathlib

from figcheck import Model, Q, floor_to

HERE = pathlib.Path(__file__).resolve().parent
MODEL = Model("firmware", HERE / "general-design.md", section=None, until="## Sources",
              documents=[HERE / "driver-design.md", HERE / "error-handling.md",
                         HERE / "logging.md", HERE / "io" / "i2c-bus.md",
                         HERE / "components" / "file-system.md"])

ds = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="datasheet", **kw)
dec = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="decision", **kw)
asm = lambda k, v, u, **kw: MODEL.input(k, v, u, kind="assumed", **kw)
fig = MODEL.derived

CORE = "PJRC's cores/teensy4"
IRQ = "Interrupts"

# ===========================================================================
# the periods
# ===========================================================================
dec("t_tick", 5, "ms", src="the driver tick's period, driver-design.md, on which the "
    "rotary sensor's driver requests one read; docs/parts/magnetic-rotary/figures.py requires "
    "the read to fit inside it")
dec("t_monitor", 100, "ms", src="how often the device monitor of error-handling.md asks "
    "every driver which parts of its device have failed")
dec("t_wdt", 2, "s", src="the watchdog timeout, error-handling.md; decided well inside the "
    "five-second limit of the solenoids' requirements, so a coil whose timer never fires is "
    "released by the restart")

MODEL.as_written("t_tick_us", of="t_tick", unit="µs")
MODEL.as_written("t_monitor_us", of="t_monitor", unit="µs")
dec("t_wdt_warn", 0.5, "s", src="how long before its restart the watchdog raises its warning, "
    "error-handling.md; WDOG1 counts it in steps of half a second, and the shortest step leaves "
    "the main loop the longest time to feed")

MODEL.as_written("t_wdt_s", of="t_wdt", unit="s")

# how long nowUs() counts before it wraps, which before() in general-design.md orders across
ds("n_clock_range", 4294967296, "", src="GPT2's counter, 32 bits, i.MX RT1060 reference manual "
   "Rev. 3, General Purpose Timer, GPT_CNT")
dec("t_clock_step", 1, "µs", src="GPT2 counts the 24 MHz crystal divided by 24, its two "
    "prescalers in general-design.cpp")


@fig("t_clock_wrap", "minutes", stated=False, rises_with=["n_clock_range", "t_clock_step"])
def _(n_clock_range, t_clock_step):
    return n_clock_range * t_clock_step

MODEL.owns("GPT2", "nowUs() reads its counter, which setup() starts, general-design.md")

# ===========================================================================
# the I²C buses on Wire and Wire2, docs/firmware/io/i2c-bus.md
# ===========================================================================
dec("f_scl", 100, "kHz", src="the bus clock on Wire and on Wire2, Standard-mode, the rate a Wire "
    "object's begin() sets with setClock(100000); each I²C bus calls it at its first attach()")

MODEL.owns("Wire", "the I²C bus wireBus runs LPI2C1 and every transfer on the bus, "
           "docs/firmware/io/i2c-bus.md", serves=("magnetic-rotary", "port-expander"))
MODEL.owns("Wire2", "the I²C bus wire2Bus runs LPI2C4 and every transfer of the touch "
           "driver, docs/firmware/io/i2c-bus.md", serves=("display",))


# the one guard the watchdog keeps, as error-handling.md tabulates it
MODEL.uses("t_on_actual", of="solenoid", group="Solenoid Driver", section="Watchdog",
           stated=True)
MODEL.uses("t_hold_max", of="solenoid", group="Solenoid Driver", section="Watchdog",
           stated=True)

# ===========================================================================
# the slots
# ===========================================================================
dec("n_tick_slots", 8, "", src="the callbacks the driver tick holds, kSlots in "
    "driver-design.md")
dec("n_monitor_slots", 12, "", src="the drivers the device monitor can watch, one for each "
    "device of the machine, Device::Count in driver-design.md")
dec("n_queue", 32, "", src="the depth of the device monitor's event queue, error-handling.md; "
    "a converter that stops answering fails eight parts at once")
ds("n_capture", 16, "bytes", src=f"what a driver tick callback may capture, the size "
   f"teensy::inplace_function takes in {CORE}/IntervalTimer.h")

MODEL.supplies("n_tick_slots", pool="driver tick")
MODEL.supplies("n_monitor_slots", pool="device monitor")
MODEL.draws(pool="driver tick")   # the device monitor's own check

# ===========================================================================
# the interrupts, a lower number preempting a higher one
# ===========================================================================
dec("prio_wdt_warn", 32, "", src="the watchdog's warning interrupt, which notes where the firmware "
    "hung; it fires only on the way to a restart", group="Watchdog, warning before a restart",
    section=IRQ, stated=True)
ds("prio_default", 128, "", src=f"{CORE}/startup.c, which sets every interrupt to 128 "
   "before setup() runs")
dec("prio_ir", 64, "", src="the IR driver's two interrupts, whose read block has to end "
    "inside its phase", group="IR ball sensing, starting a sensor reading", section=IRQ,
    stated=True)
dec("prio_pins", 96, "", src="the one IRQ every pin interrupt shares, which the main task sets "
    "once", group="Solenoids, break beam, controls and port expander, reacting to a contact, the beam, the switch or INT", section=IRQ, stated=True)
dec("prio_tick", 96, "", src="the driver tick, kPriority in driver-design.md",
    group="Rotary sensor, port expander, solenoids, device monitor, display and touch, their periodic work", section=IRQ, stated=True)
dec("prio_i2c", 160, "", src="the I²C buses' LPI2C1 and LPI2C4 interrupts, which have no "
    "deadline: below the DMA refills, which have one, and above FreeRTOS's limit, since they never "
    "call the kernel; docs/firmware/io/i2c-bus.md",
    group="I²C buses, running every transfer on Wire and Wire2", section=IRQ, stated=True)
ds("prio_update", 208, "", src=f"{CORE}/AudioStream.cpp, NVIC_SET_PRIORITY(IRQ_SOFTWARE, "
   "208), the Audio library's update", group="Audio, computing the next block of samples",
   section=IRQ, stated=True)
dec("prio_card", 240, "", src="the SD controller's interrupt, which wakes the storage task at the "
    "end of a transfer, NVIC_SET_PRIORITY(IRQ_SDHC1, 240) in docs/firmware/drivers/storage.md",
    group="Storage, waking the storage task at the end of a transfer",
    section=IRQ, stated=True)
ds("prio_kernel_irq", 240, "", src="freertos-teensy 11.2.0-4, src/portable/port.c: PendSV and "
   "SysTick at portMIN_INTERRUPT_PRIORITY, 255, of which the Teensy's four priority bits keep 240",
   group="FreeRTOS, its tick and its task switch", section=IRQ, stated=True)
ds("stack_irq_port", 4, "KB", src="configMAIN_STACK_DEPTH in freertos-teensy's FreeRTOSConfig.h, "
   "4096 bytes: the stack the port keeps for the interrupts at the top of RAM1")
MODEL.as_written("stack_irq_port_bytes", of="stack_irq_port", unit="bytes")
dec("stack_irq", 8, "KB", src="configMAIN_STACK_DEPTH in the firmware's FreeRTOSConfig.h: the "
    "interrupts' stack at the top of RAM1, doubled from the port's value since six interrupt "
    "levels can nest; a test build measures about 2.1 KB for the deepest nesting, with the IR, pin "
    "and tick handlers estimated; the main task's stack takes the rest of RAM1 below it, behind 32 "
    "bytes the MPU forbids, so an overflowing interrupt stack faults at once")
MODEL.as_written("stack_irq_bytes", of="stack_irq", unit="bytes")
ds("t_kernel_tick", 1, "ms", src="the kernel's tick, configTICK_RATE_HZ 1000 in freertos-teensy's "
   "FreeRTOSConfig.h; its hook counts millis() and micros()")
dec("n_kernel_level", 13, "", src="configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY in the firmware's "
    "copy of freertos-teensy's FreeRTOSConfig.h, raised from the port's 2, so the kernel's critical "
    "sections mask only the interrupts that call it: the audio update, FlexIO3 and the SD controller")
ds("n_prio_step", 16, "", src="the Teensy 4.1's NVIC keeps the upper four of a priority's eight "
   "bits, so priorities step by 16; freertos-teensy's FreeRTOSConfig.h shifts the configuration "
   "value by 8 minus configPRIO_BITS, 4")


# configMAX_SYSCALL_INTERRUPT_PRIORITY: the most urgent priority whose interrupt may call the
# kernel, and the one the kernel's critical sections mask from
@fig("prio_kernel", "", group="FreeRTOS's limit, from which on an interrupt may call FreeRTOS", section=IRQ, rises_with=["n_kernel_level", "n_prio_step"])
def _(n_kernel_level, n_prio_step):
    return n_kernel_level * n_prio_step
dec("prio_display", 224, "", src="the display driver's FlexIO3 interrupt, which refills the "
    "bus and has no deadline, since a late refill only pauses the write strobe; "
    "docs/firmware/drivers/display.md",
    group="Display, refilling its bus", section=IRQ, stated=True)


@fig("prio_ir_spi", "", group="IR ball sensing, taking each conversion's result", section=IRQ)
def _(prio_ir):
    return prio_ir


# output_i2s2.cpp attaches its DMA interrupt without a priority, so it keeps the default
@fig("prio_dma", "", group="Audio, refilling the half of the output buffer that has played",
     section=IRQ)
def _(prio_default):
    return prio_default


# FastLED's ObjectFLED driver, adapted from OctoWS2811, attaches its refill interrupt without a priority, so it
# keeps the default; OctoWS2811_imxrt.cpp sets none, and ObjectFLED's own source is read at bring-up
@fig("prio_led", "", group="Lighting, refilling the half of the LED bit buffer that has gone out", section=IRQ)
def _(prio_default):
    return prio_default


# ===========================================================================
# the log, docs/firmware/drivers/storage.md and docs/firmware/logging.md
# ===========================================================================
dec("log_size", 1, "MB", src="the size /log.txt reaches before the logger empties it and starts "
    "it over, kSize in docs/firmware/logging.md")


asm("log_line", 64, "bytes", src="an average line with its time, its writer and a short message, "
    "such as 52310 host game started")
dec("t_log_flush", 1, "s", src="the time from one write of the collected log lines to the next, "
    "kFlushUs in docs/firmware/logging.md")
MODEL.as_written("t_log_flush_us", of="t_log_flush", unit="µs")
dec("log_buffer", 2, "KB", src="the lines the logger collects in RAM between two writes, kBuffer "
    "in docs/firmware/logging.md")

MODEL.uses("f_s", of="audio")
MODEL.uses("t_wr_block", of="audio")
MODEL.uses("b_layer", of="display")
MODEL.uses("t_sdfat", of="audio")


@fig("n_log_lines", "", stated=False, rises_with=["log_size"], falls_with=["log_line"])
def _(log_size, log_line):
    return log_size / log_line


# ===========================================================================
# RAM2 that the libraries take, beside the buffers the drivers declare
# ===========================================================================
ds("ram_usb_tx", 8, "KB", src=f"{CORE}/usb_serial.c: txbuffer, TX_SIZE 2048 times TX_NUM 4 "
   "bytes in DMAMEM, with the USB type Serial")
ds("ram_usb_rx", 4, "KB", src=f"{CORE}/usb_serial.c and usb_desc.h: rx_buffer, RX_NUM 8 times "
   "CDC_RX_SIZE_480 512 bytes in DMAMEM, with the USB type Serial")
ds("ram_objectfled", 10, "KB", src="ObjectFLED.h in FastLED: bitdata, the bit buffer of 10 KB for "
   "80 LED bytes that all chains share")
asm("b_fastled_led", 24, "bytes", src="an upper bound for FastLED's color buffers and ObjectFLED's "
    "frame buffer per LED of the longest chain: eight lanes of 3 bytes each, from malloc(), whose "
    "heap lies in RAM2 by PJRC's Teensy 4.1 page; ObjectFLED's own figure is read at bring-up")
dec("n_chain_max", 79, "", src="the longest LED chain, a perimeter segment, in the light layout table of "
    "docs/parts/lighting/design.md; FastLED sizes its buffers by it")


@fig("ram_usb", "KB", stated=False, rises_with=["ram_usb_tx", "ram_usb_rx"])
def _(ram_usb_tx, ram_usb_rx):
    return ram_usb_tx + ram_usb_rx


@fig("ram_fastled", "KB", stated=False, rises_with=["b_fastled_led", "n_chain_max"])
def _(n_chain_max, b_fastled_led):
    return 2 * n_chain_max * b_fastled_led


MODEL.draws("ram_usb", pool="RAM2")
MODEL.draws("ram_objectfled", pool="RAM2")
MODEL.draws("ram_fastled", pool="RAM2")


# ===========================================================================
# what the firmware requires
# ===========================================================================
_I = MODEL.invariant

_I("the IR driver's interrupts preempt the pin interrupts and the driver tick, whose "
   "deadlines are milliseconds",
   lambda v: v.prio_ir < v.prio_pins and v.prio_ir < v.prio_tick)
_I("the Audio library's two interrupts sit below the IR, pin and driver-tick interrupts",
   lambda v: min(v.prio_dma, v.prio_update) > max(v.prio_ir, v.prio_pins, v.prio_tick))
_I("the SD controller's interrupt sits beneath both, the lowest of all",
   lambda v: v.prio_card > max(v.prio_dma, v.prio_update, v.prio_ir, v.prio_pins,
                               v.prio_tick, v.prio_display, v.prio_i2c))
_I("the I²C bus's interrupt, which has no deadline, sits below the DMA refills, which have one",
   lambda v: v.prio_i2c > max(v.prio_dma, v.prio_led))
_I("the display's FlexIO3 interrupt sits below the Audio library's two, so a refill never "
   "delays a block",
   lambda v: v.prio_display > max(v.prio_dma, v.prio_update))
_I("the kernel never masks the IR driver's interrupts",
   lambda v: v.prio_ir < v.prio_kernel and v.prio_ir_spi < v.prio_kernel)
_I("the kernel never masks an interrupt that does not call it: the pins, the driver tick, the "
   "DMA interrupts and the I²C bus",
   lambda v: max(v.prio_pins, v.prio_tick, v.prio_dma, v.prio_led, v.prio_i2c) < v.prio_kernel)
_I("every interrupt that calls the kernel lies at its threshold or below it: the audio update and "
   "FlexIO3 start reads, and the SD controller wakes the storage task",
   lambda v: v.prio_kernel <= min(v.prio_update, v.prio_display, v.prio_card))
_I("the kernel's tick and task switch wait for every other interrupt",
   lambda v: v.prio_kernel_irq >= max(v.prio_ir, v.prio_pins, v.prio_tick, v.prio_dma, v.prio_led,
                                      v.prio_i2c, v.prio_update, v.prio_display, v.prio_card))
_I("the device monitor checks on whole driver ticks",
   lambda v: (v.t_monitor / v.t_tick).raw.is_integer())
_I("the device monitor's queue has a depth the event queue accepts, a power of two",
   lambda v: math.log2(v.n_queue.raw).is_integer())
_I("an average log line fits the logger's buffer",
   lambda v: v.log_line < v.log_buffer)
_I("SdFat's longest wait, which stops the main loop in the cases storage.md's limitations name, "
   "ends before the watchdog restarts the Teensy",
   lambda v: v.t_sdfat < v.t_wdt)
_I("SdFat's longest wait ends before the watchdog warns, so a wait the main loop survives "
   "leaves no note",
   lambda v: v.t_sdfat < v.t_wdt - v.t_wdt_warn)
_I("the watchdog's warning preempts every driver interrupt, so a hang in any of them is noted",
   lambda v: v.prio_wdt_warn < min(v.prio_ir, v.prio_ir_spi, v.prio_pins, v.prio_tick, v.prio_dma,
                                   v.prio_led, v.prio_i2c, v.prio_update, v.prio_display,
                                   v.prio_card))
_I("FreeRTOS never masks the watchdog's warning, which does not call it",
   lambda v: v.prio_wdt_warn < v.prio_kernel)

for _text, _why in [
    ("0", "a loop's start, an empty count or mask, and a pure virtual function in the listings"),
    ("1000", "the microseconds in a millisecond, which elapsedMs() multiplies its span by"),
]:
    MODEL.aside(_text, _why)
