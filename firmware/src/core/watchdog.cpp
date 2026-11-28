#include "watchdog.h"
#include "logging/debug-log.h"
#include "logging/debug-trace.h"
#include "logging/log-line.h"
#include "logging/logger.h"
#include <Watchdog_t4.h>

/// model/firmware/figures.py, t_wdt_warn: how long before the restart the warning comes
constexpr uint32_t WARNING_MS = 500;
constexpr uint8_t WARNING_PRIORITY = 32;   // general-design.md: the watchdog's row
constexpr double MS_PER_S = 1000.0;
constexpr uint32_t VECTOR_OFFSET = 16;   // the processor's exceptions ahead of the first IRQ
constexpr uint32_t FRAME_PC = 6;         // the program counter's word in the interrupt's frame
constexpr size_t CACHE_ROW_BYTES = 32;   // the Cortex-M7's data cache works in rows of 32 bytes

/// The watchdog that has begun, which the idle task and the warning interrupt reach.
static Watchdog* active = nullptr;
/// WDOG1, which WDT_T4 runs.
static WDT_T4<WDT1> wdog;
/// WDT_T4's handler of the warning, which clears the warning once the note is written.
static void (*libraryWarning)() = nullptr;
/// The note of the restart's cause. RAM2 keeps it across a restart, and its seal tells it from
/// what power-up leaves there. It fills one cache row, so cleaning the cache over it touches
/// nothing else.
struct alignas(CACHE_ROW_BYTES) CacheRowNote {
    RestartNote note;
};
static_assert(sizeof(CacheRowNote) == CACHE_ROW_BYTES, "the note fills one cache row");
DMAMEM static CacheRowNote kept;
static RestartNote& restartNote = kept.note;

Watchdog::Watchdog() : guard(write) {}

/// Finds the frame the warning interrupt saved, on a task's stack or on the interrupts' stack, and
/// hands it to watchdogWarned().
__attribute__((naked)) static void onWarning() {
    asm volatile("tst lr, #4\n"
                 "ite eq\n"
                 "mrseq r0, msp\n"
                 "mrsne r0, psp\n"
                 "b watchdogWarned\n");
}

/// Notes a hang at the code the warning interrupted, then lets WDT_T4 clear the warning.
extern "C" void watchdogWarned(const uint32_t* frame) {
    DEBUG_TRACE("watchdog hang", int32_t(frame[FRAME_PC]), 0);
    active->guard.note(RestartCause::HANG, frame[FRAME_PC]);
    libraryWarning();
}

/// FreeRTOS's idle task runs it whenever every other task waits.
extern "C" void vApplicationIdleHook() {
    if (active != nullptr && active->idleFeeds) {
        active->check();
    }
}

void Watchdog::begin() {
    if (begun) {
        DEBUG_LOG("watchdog: begin() has run before\n");
        return;
    }
    begun = true;
    // A note counts only after a restart by WDOG1. Any other start keeps the cause at NONE.
    arm_dcache_delete(&kept, sizeof kept);
    if ((SRC_SRSR & SRC_SRSR_WDOG_RST_B) != 0) {
        previousCause = readNote(restartNote);
        previousDetail = previousCause == RestartCause::NONE ? 0 : restartNote.detail;
    }
    // CrashReport keeps its breadcrumbs for report(), and only the note starts afresh.
    sealNote(restartNote, RestartCause::NONE, 0);
    arm_dcache_flush(&kept, sizeof kept);
    active = this;
    idleFeeds = true;

    WDT_timings_t config;
    config.timeout = TIMEOUT_S;
    config.trigger = WARNING_MS / MS_PER_S;
    // WDT_T4 turns the warning on only for a handler.
    config.callback = [] {};
    wdog.begin(config);
    // The warning comes through onWarning(), which sees the interrupted code, and then through
    // WDT_T4's handler, which ends the warning.
    libraryWarning = _VectorsRam[VECTOR_OFFSET + IRQ_WDOG1];
    attachInterruptVector(IRQ_WDOG1, onWarning);
    NVIC_SET_PRIORITY(IRQ_WDOG1, WARNING_PRIORITY);
}

void Watchdog::loopStarts() {
    idleFeeds = false;
}

void Watchdog::check() {
    if (!begun) {
        if (!unbegunNoted) {
            unbegunNoted = true;
            DEBUG_LOG("watchdog: check() before begin() feeds nothing\n");
        }
        return;
    }
    // The idle task and the main loop both feed until the main loop runs, and the warning notes
    // through the guard as well.
    noInterrupts();
    const bool fed = guard.check();
    if (fed) {
        wdog.feed();
    }
    interrupts();
    // A coil that stays on fails every check until the restart, so only the change is logged.
    if (fed == starving) {
        starving = !fed;
        DEBUG_LOG(starving ? "watchdog: a coil stays on, so the watchdog goes unfed\n"
                           : "watchdog: fed again\n");
    }
}

void Watchdog::write(RestartCause cause, uint32_t detail) {
    CrashReport.breadcrumb(1, uint32_t(cause));
    CrashReport.breadcrumb(2, detail);
    sealNote(restartNote, cause, detail);
    arm_dcache_flush(&kept, sizeof kept);
}

/// Hands CrashReport's text to the log, one line at a time.
class LogLines : public Print {
public:
    size_t write(uint8_t c) override {
        if (c == '\r') {
            return 1;
        }
        if (c != '\n') {
            // A line longer than the log takes is cut.
            if (n < sizeof line - 1) {
                line[n++] = char(c);
            }
            return 1;
        }
        line[n] = '\0';
        if (n != 0) {
            logInfo("restart", "%s", line);
        }
        n = 0;
        return 1;
    }

private:
    char line[LOG_LINE_BYTES] = {};
    size_t n = 0;
};

void Watchdog::report() {
    if (previousCause == RestartCause::COIL) {
        logWarning("restart", "the watchdog restarted the Teensy: coils 0x%02lx stayed on too long",
                   previousDetail);
    } else if (previousCause == RestartCause::HANG) {
        logWarning("restart", "the watchdog restarted the Teensy: the firmware hung at 0x%08lx",
                   previousDetail);
    }
    if (!CrashReport && (SRC_SRSR & ~SRC_SRSR_IPP_RESET_B) == 0) {
        return;
    }
    // CrashReport names the reset and a crash's place, and clears itself once printed.
    LogLines lines;
    lines.print(CrashReport);
}
