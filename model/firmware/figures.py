"""The figures of the firmware: its periods, its watchdog, its interrupts and its slots."""
import math

from figcheck import *

MODEL = Model.at(__file__, "firmware", "general-design.md",
                 drawings=["architecture.svg"],
                 documents=["driver-design.md", "error-handling.md", "logging.md", "io/i2c-bus.md",
                            "components/file-system.md", "game-abstraction.md",
                            "input-handling.md", "components/config.md", "host-screens.md"])
fig, ds, gr, msr, asm, dec = MODEL.declarers()
MODEL.ignores("README.md", "the index of the firmware documents for a visitor, which states no figure")
MODEL.ignores("game-files.md", "the format of a game file, whose numbers are the values of its example game")
dec("n_balls_example", 3, "", src="the balls the example game in game-abstraction.md starts with")
dec("points_lane_example", 100, "", src="the points the example game gives for a lane it passes")
dec("t_ball_save_example", 10000, "", src="kSaveMs, the milliseconds the example game's ball save holds after a launch")

CORE = "PJRC's cores/teensy4"
IRQ = "Interrupts"

# -- the periods --------------------------------------------------------------
dec("t_tick", 5, "ms", src="the driver tick's period, driver-design.md, on which the rotary "
    "sensor's driver requests one read; model/parts/magnetic-rotary/figures.py requires the read to "
    "fit inside it")
dec("t_monitor", 100, "ms", src="how often the device monitor of error-handling.md asks every "
    "driver which parts of its device have failed")
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
dec("t_clock_step", 1, "µs", src="GPT2 counts the 24 MHz crystal divided by 24, its two prescalers "
    "in startClock() of firmware/src/core/clock.cpp")
fig("t_clock_wrap", "minutes", "n_clock_range * t_clock_step", moves="+n_clock_range +t_clock_step",
    stated=False)

MODEL.owns("GPT2", "nowUs() reads its counter, which setup() starts, general-design.md")

# -- the I²C buses on Wire and Wire2, docs/firmware/io/i2c-bus.md -------------
dec("f_scl", 100, "kHz", src="the bus clock on Wire and on Wire2, Standard-mode, the rate a Wire "
    "object's begin() sets with setClock(100000); each I²C bus calls it at its first attach()")

MODEL.owns("Wire", "the I²C bus wireBus runs LPI2C1 and every transfer on the bus, "
           "docs/firmware/io/i2c-bus.md", serves=("magnetic-rotary", "port-expander"))
MODEL.owns("Wire2", "the I²C bus wire2Bus runs LPI2C4 and every transfer of the touch "
           "driver, docs/firmware/io/i2c-bus.md", serves=("display",))

# the one guard the watchdog keeps, as error-handling.md tabulates it
MODEL.uses("t_on_actual", of="solenoid", group="Solenoid Driver", section="Watchdog", stated=True)
MODEL.uses("t_hold_max", of="solenoid", group="Solenoid Driver", section="Watchdog", stated=True)

# -- the slots ----------------------------------------------------------------
dec("n_tick_slots", 8, "", src="the callbacks the driver tick holds, kSlots in driver-design.md")
dec("n_monitor_slots", 12, "", src="the drivers the device monitor can watch, one for each device "
    "of the machine, Device::Count in driver-design.md")
dec("n_queue", 32, "", src="the depth of the device monitor's event queue, error-handling.md; a "
    "converter that stops answering fails eight parts at once")
ds("n_capture", 16, "bytes", src="what a driver tick callback may capture, the size "
   f"teensy::inplace_function takes in {CORE}/IntervalTimer.h")

dec("n_producers", 11, "", src="the queues the event queue takes at most, PRODUCERS in "
    "firmware/src/core/event-queue.h: one for each driver that publishes events, as input-handling.md "
    "estimates them")
dec("b_event", 12, "bytes", src="sizeof(DriverEvent) in firmware/src/core/event-queue.h: its time, "
    "type, source and payload, the payload aligned to four bytes", stated=False)
# the device monitor's queue, DMAMEM in firmware/src/core/device-monitor.cpp
fig("ram_monitor_queue", "bytes", "n_queue.raw * b_event", moves="+n_queue +b_event",
    stated=False)
MODEL.draws("ram_monitor_queue", pool="RAM2")

MODEL.supplies("n_tick_slots", pool="driver tick")
MODEL.supplies("n_monitor_slots", pool="device monitor")
MODEL.draws(pool="driver tick")   # the device monitor's own check

# -- the interrupts, a lower number preempting a higher one -------------------
with MODEL.block(section=IRQ):
    dec("prio_wdt_warn", 32, "", src="the watchdog's warning interrupt, which notes where the "
        "firmware hung; it fires only on the way to a restart",
        group="Watchdog, warning before a restart")
    ds("prio_default", 128, "", src=f"{CORE}/startup.c, which sets every interrupt to 128 before "
       "setup() runs", stated=False)
    dec("prio_ir", 64, "", src="the IR driver's two interrupts, whose read block has to end inside "
        "its phase", group="IR ball sensing, starting a sensor reading")
    dec("prio_pins", 96, "", src="the one IRQ every pin interrupt shares, which the main task sets "
        "once",
        group="Solenoids, break beam, controls and port expander, reacting to a contact, the beam, the switch or INT")
    dec("prio_tick", 96, "", src="the driver tick, kPriority in driver-design.md",
        group="Rotary sensor, port expander, solenoids, servo, device monitor, display and touch, their periodic work")
    dec("prio_i2c", 160, "", src="the I²C buses' LPI2C1 and LPI2C4 interrupts, which have no "
        "deadline: below the audio DMA refill, which has one, and above FreeRTOS's limit, since they "
        "never call the kernel; docs/firmware/io/i2c-bus.md",
        group="I²C buses, running every transfer on Wire and Wire2")
    ds("prio_update", 208, "", src=f"{CORE}/AudioStream.cpp, NVIC_SET_PRIORITY(IRQ_SOFTWARE, 208), "
       "the Audio library's update", group="Audio, computing the next block of samples")
    dec("prio_card", 240, "", src="the SD controller's interrupt, in which SdFat ends a transfer, "
        "SD_PRIORITY in firmware/src/drivers/storage.cpp",
        group="Storage, SdFat ending a transfer")
    ds("prio_kernel_irq", 240, "", src="freertos-teensy 11.2.0-4, src/portable/port.c: PendSV and "
       "SysTick at portMIN_INTERRUPT_PRIORITY, 255, of which the Teensy's four priority bits keep "
       "240", group="FreeRTOS, its tick and its task switch")
    ds("stack_irq_port", 4, "KB", src="configMAIN_STACK_DEPTH in freertos-teensy's "
       "FreeRTOSConfig.h, 4096 bytes: the stack the port keeps for the interrupts at the top of "
       "RAM1", stated=False)
    MODEL.as_written("stack_irq_port_bytes", of="stack_irq_port", unit="bytes")
    dec("stack_irq", 8, "KB", src="configMAIN_STACK_DEPTH in the firmware's FreeRTOSConfig.h: the "
        "interrupts' stack at the top of RAM1, doubled from the port's value since six interrupt "
        "levels can nest; a test build measures about 2.1 KB for the deepest nesting, with the IR, "
        "pin and tick handlers estimated; the main task's stack takes the rest of RAM1 below it, "
        "behind 32 bytes the MPU forbids, so an overflowing interrupt stack faults at once",
        stated=False)
    MODEL.as_written("stack_irq_bytes", of="stack_irq", unit="bytes")
    ds("t_kernel_tick", 1, "ms", src="the kernel's tick, configTICK_RATE_HZ 1000 in "
       "freertos-teensy's FreeRTOSConfig.h; its hook counts millis() and micros()", stated=False)
    dec("n_kernel_level", 13, "", src="configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY in the "
        "firmware's copy of freertos-teensy's FreeRTOSConfig.h, raised from the port's 2, so the "
        "kernel's critical sections mask only the interrupts that call it: the audio update, "
        "FlexIO3 and the SD controller", stated=False)
    ds("n_prio_step", 16, "", src="the Teensy 4.1's NVIC keeps the upper four of a priority's "
       "eight bits, so priorities step by 16; freertos-teensy's FreeRTOSConfig.h shifts the "
       "configuration value by 8 minus configPRIO_BITS, 4", stated=False)

    # configMAX_SYSCALL_INTERRUPT_PRIORITY: the most urgent priority whose interrupt may call the
    # kernel, and the one the kernel's critical sections mask from
    fig("prio_kernel", "", "n_kernel_level * n_prio_step", moves="+n_kernel_level +n_prio_step",
        group="FreeRTOS's limit, from which on an interrupt may call FreeRTOS")
    dec("prio_display", 224, "", src="the display driver's FlexIO3 interrupt, which refills the "
        "bus and has no deadline, since a late refill only pauses the write strobe; "
        "docs/firmware/drivers/display.md", group="Display, sending a rectangle")
    fig("prio_ir_spi", "", "prio_ir", group="IR ball sensing, taking each conversion's result")

    # output_i2s2.cpp attaches its DMA interrupt without a priority, so it keeps the default
    fig("prio_dma", "", "prio_default",
        group="Audio, refilling the half of the output buffer that has played")

# -- the log, docs/firmware/drivers/storage.md and docs/firmware/logging.md ---
dec("log_size", 1, "MB", src="the size a log file reaches before the logger turns to its other "
    "file, Logger::FILE_LIMIT_BYTES in firmware/src/logging/logger.h")
asm("log_line", 64, "bytes", src="an average line with its time, its writer and a short message, "
    "such as 52310 I host game started")
dec("t_log_flush", 1, "s", src="the time from one write of the waiting log lines to the next, "
    "CardAppender::WRITE_PERIOD_MS in firmware/src/io/card-appender.h")
MODEL.as_written("t_log_flush_us", of="t_log_flush", unit="µs")
dec("log_buffer", 64, "KB", src="the lines the logger holds in the PSRAM until they are on the "
    "card, Logger::BUFFER_BYTES in firmware/src/logging/logger.h, a decision: a burst of about a "
    "thousand lines, or a card that stalls for several seconds, loses none")
dec("log_line_max", 160, "bytes", src="the longest line with its line break, LOG_LINE_BYTES in "
    "firmware/src/logging/log-line.h; a longer line is cut")
dec("n_trace_records", 256, "", src="the notes the debug trace holds in a debug build, "
    "TRACE_RECORDS in firmware/src/logging/debug-trace.cpp, a burst of interrupts while the main "
    "loop is away")
dec("debug_buffer", 16, "KB", src="the debug lines waiting for USB in a debug build, "
    "DEBUG_BUFFER_BYTES in firmware/src/logging/debug-log.cpp, in the PSRAM")
fig("n_buffer_lines", "", "log_buffer / log_line", "down", moves="+log_buffer -log_line",
    stated=False)
MODEL.draws("log_buffer", pool="PSRAM")
MODEL.draws("debug_buffer", pool="PSRAM")
MODEL.uses("f_s", of="audio")
MODEL.uses("t_wr_block", of="audio")
MODEL.uses("t_sdfat", of="audio")
MODEL.uses("i_supply_6a", of="lighting")
dec("config_max", 4, "KB", src="the longest /config.yml the config component reads, kFileMax in "
    "firmware/gist/components/config.cpp, a decision: the whole file with every IR channel is "
    "well under it")
fig("n_log_lines", "", "log_size / log_line", moves="+log_size -log_line", stated=False)

# -- RAM2 that the libraries take, beside the buffers the drivers declare -----
ds("ram_usb_tx", 8, "KB", src=f"{CORE}/usb_serial.c: txbuffer, TX_SIZE 2048 times TX_NUM 4 bytes "
   "in DMAMEM, with the USB type Serial")
ds("ram_usb_rx", 4, "KB", src=f"{CORE}/usb_serial.c and usb_desc.h: rx_buffer, RX_NUM 8 times "
   "CDC_RX_SIZE_480 512 bytes in DMAMEM, with the USB type Serial")
dec("n_chain_max", 79, "", src="the longest LED chain, a perimeter segment, in the light layout "
    "table of docs/parts/lighting/design.md; the lighting driver sizes the frame's words by it")
fig("ram_usb", "KB", "ram_usb_tx + ram_usb_rx", moves="+ram_usb_tx +ram_usb_rx", stated=False)
# The lighting driver takes from the heap, which lies in RAM2 by PJRC's Teensy 4.1 page, the
# colors of every LED and the frame's words: each chain's words for the longest chain and the zero
# words after it, firmware/src/drivers/lighting.h and firmware/src/drivers/ws2812-flexio2.h. The budget takes every
# chain at the bound the config allows, so no config can leave the lighting without memory.
MODEL.uses("n_bits_led", of="lighting")
MODEL.uses("n_symbols_word", of="lighting")
MODEL.uses("n_latch_words", of="lighting")
MODEL.uses("n_chain_limit", of="lighting")
dec("n_led_chains", 6, "", src="Lighting::CHAINS in firmware/src/drivers/lighting.h, the chains "
    "whose words alternate in the frame")
dec("b_word", 4, "bytes", src="one word of a FlexIO shifter's 32-bit buffer, RM FlexIO SHIFTBUF")
dec("b_crgb", 3, "bytes", src="FastLED's CRGB, one byte each of red, green and blue")
fig("ram_lighting", "KB", "(n_chain_limit * n_bits_led / n_symbols_word + n_latch_words) * "
    "n_led_chains.raw * b_word + n_led_chains.raw * n_chain_limit.raw * b_crgb",
    moves="+n_chain_limit +n_bits_led -n_symbols_word +n_latch_words +n_led_chains +b_word +b_crgb",
    stated=False)

MODEL.draws("ram_usb", pool="RAM2")
MODEL.draws("ram_lighting", pool="RAM2")

# -- the host's screens, host-screens.md ----------------------------------------
_HOST = "decided with the host screens' storyboard"
dec("t_menu_snap", 150, "ms", src=f"how long the menu takes to snap open or back once the finger lets go, {_HOST}")
dec("t_page_slide", 150, "ms", src=f"how long a page of the host slides sideways, {_HOST}")
dec("t_toast_drop", 200, "ms", src=f"how long a toast takes to drop in from the top, {_HOST}")
dec("t_curtain_close", 250, "ms", src=f"how long the curtain takes to close, {_HOST}")
dec("t_curtain_hold", 200, "ms", src=f"how long the curtain stays closed with its plaque, {_HOST}")
dec("t_curtain_open", 300, "ms", src=f"how long the curtain takes to open, {_HOST}")
dec("t_launch_card", 1.5, "s", src=f"about how long the launch card stays before the game's screen, {_HOST}")
dec("t_menu_idle", 20, "s", src=f"how long the menu stays open without a touch, {_HOST}")
dec("t_hint_in", 150, "ms", src=f"how long the hint at the right edge takes to fade in, {_HOST}")
dec("t_hint_stay", 1, "s", src=f"how long the hint stands once the finger lifts or rests, {_HOST}")
dec("t_hint_fade", 300, "ms", src=f"how long the hint then takes to fade, {_HOST}")
dec("n_menu_width", 208, "", src=f"the menu's width in pixels, {_HOST}")
_EDGE_MENU_H = "firmware/src/host/views/edge-menu-view.h"
for _key, _name, _unit in (("t_menu_snap", "SNAP_MS", "ms"),
                           ("t_hint_in", "HINT_IN_MS", "ms"),
                           ("t_hint_stay", "HINT_STAY_MS", "ms"),
                           ("t_hint_fade", "HINT_FADE_MS", "ms"),
                           ("n_menu_width", "WIDTH_PX", "")):
    MODEL.in_code(_key, file=_EDGE_MENU_H, name=_name, unit=_unit)
dec("t_toast_fault", 4, "s", src=f"how long the toast of a failed device shows, {_HOST}")
dec("t_toast_recovered", 3, "s", src=f"how long the toast of a device that works again shows, {_HOST}")
dec("t_game_over", 3, "s", src=f"how long Game over shows before the first name entry, {_HOST}")
dec("t_name_idle", 30, "s", src=f"how long the name entry waits for a touch before it counts as Skip, {_HOST}")
dec("t_play_again", 15, "s", src=f"the countdown on the high scores that starts the same game again, {_HOST}")
dec("step_slider", 5, "%", src=f"the step of the minus and the plus beside a slider, {_HOST}")

# -- the bench, firmware/bench/load and firmware/bench/watchdog ----------------
_LOAD = "firmware/bench/load on the Teensy 4.1 under FreeRTOS with every load running at once: the IR timer on a PIT, the driver tick on GPT1, a movie read and drawn at 25 frames a second, four audio streams, saves and log lines"
msr("t_ir_jitter_bench", 0.35, "µs", src=f"{_LOAD}: the spread of the IR timer's interrupt, the "
    "largest of six runs of about 33000 interrupts each")
msr("t_tick_jitter_bench", 0.30, "µs", src=f"{_LOAD}: the spread of the driver tick's interrupt, "
    "the largest of six runs of about 4000 interrupts each")
MODEL.uses("t_jitter", of="ir-reflective", stated=False)
msr("t_wdt_warn_bench", 1.5, "s", src="firmware/bench/watchdog on the Teensy 4.1 under FreeRTOS, a "
    "hang with interrupts on: the time from the last feed to the warning interrupt, 1500 ms")

# -- what the firmware requires -----------------------------------------------
_I = MODEL.invariant

_I("the IR timer fired on the bench well inside the reserve its phase keeps for a late trigger",
   "t_ir_jitter_bench < t_jitter")
_I("the watchdog warned on the bench no later than it is set to", "t_wdt_warn_bench <= t_wdt - t_wdt_warn")

_I("the IR driver's interrupts preempt the pin interrupts and the driver tick, whose deadlines are "
   "milliseconds", "prio_ir < prio_pins and prio_ir < prio_tick")
_I("the Audio library's two interrupts sit below the IR, pin and driver-tick interrupts",
   "min(prio_dma, prio_update) > max(prio_ir, prio_pins, prio_tick)")
_I("the SD controller's interrupt sits beneath both, the lowest of all",
   "prio_card > max(prio_dma, prio_update, prio_ir, prio_pins, prio_tick, prio_display, prio_i2c)")
_I("the I²C bus's interrupt, which has no deadline, sits below the audio DMA refill, which has one",
   "prio_i2c > prio_dma")
_I("the display's FlexIO3 interrupt sits below the Audio library's two, so a refill never delays a "
   "block", "prio_display > max(prio_dma, prio_update)")
_I("the kernel never masks the IR driver's interrupts",
   "prio_ir < prio_kernel and prio_ir_spi < prio_kernel")
_I("the kernel never masks an interrupt that does not call it: the pins, the driver tick, the DMA "
   "interrupts and the I²C bus",
   "max(prio_pins, prio_tick, prio_dma, prio_i2c) < prio_kernel")
_I("every interrupt that calls the kernel lies at its threshold or below it: the audio update and "
   "FlexIO3 start reads", "prio_kernel <= min(prio_update, prio_display)")
_I("the kernel's tick and task switch wait for every other interrupt", "prio_kernel_irq >= "
   "max(prio_ir, prio_pins, prio_tick, prio_dma, prio_i2c, prio_update, prio_display, "
   "prio_card)")
_I("the device monitor checks on whole driver ticks", "(t_monitor / t_tick).raw.is_integer()")
_I("the device monitor's queue has a depth the event queue accepts, a power of two",
   "math.log2(n_queue.raw).is_integer()")
_I("an average log line fits the logger's buffer", "log_line < log_buffer")
_I("SdFat's longest wait ends before the watchdog restarts the Teensy", "t_sdfat < t_wdt")
_I("SdFat's longest wait ends before the watchdog warns, so a wait the main loop survives leaves "
   "no note", "t_sdfat < t_wdt - t_wdt_warn")
_I("the watchdog's warning preempts every driver interrupt, so a hang in any of them is noted",
   "prio_wdt_warn < min(prio_ir, prio_ir_spi, prio_pins, prio_tick, prio_dma, prio_i2c, "
   "prio_update, prio_display, prio_card)")
_I("FreeRTOS never masks the watchdog's warning, which does not call it",
   "prio_wdt_warn < prio_kernel")

for _text, _why in [
    ("0", "a loop's start, an empty count or mask, and a pure virtual function in the listings"),
    ("73021 W", "the time of an example log line in logging.md, where W is the level, no unit"),
    ("80211 W", "the time of an example log line in logging.md, where W is the level, no unit"),
]:
    MODEL.aside(_text, _why)
