#include "Watchdog_t4.h"         // the WDT_T4 library, github.com/tonton81/WDT_T4

extern "C" void watchdogWarned(const uint32_t* frame);

// the i.MX RT's WDOG1, which restarts the Teensy unless it is fed within 2 s. It notes why it went
// unfed in CrashReport's breadcrumbs, #1 the cause and #2 its detail
class Watchdog {
public:
    // starts the watchdog and its warning; the main task calls it right after the storage
    // driver's mount and before the solenoids' begin(), so it guards the coils from their first pull
    void begin();

    // feeds the watchdog while every guard holds, and notes the cause where one fails; at the end
    // of every pass of the main loop, and from FreeRTOS's idle task until the main loop runs
    void check();

    // writes the report of the last restart into the log; the main task, right after the logger's begin()
    void report(Logger& logger);

private:
    friend void watchdogWarned(const uint32_t* frame);

    static constexpr unsigned kCauseNone = 0;    // a note the restart did not follow
    static constexpr unsigned kCauseCoil = 1;    // a coil stays on too long; #2 the coils, one bit each
    static constexpr unsigned kCauseHang = 2;    // the firmware hangs; #2 the address of the interrupted code

    // notes why the watchdog goes unfed; the first note stands until the restart
    void note(unsigned cause, unsigned detail);

    static Watchdog* self_;                      // the watchdog the warning interrupt notes in
    WDT_T4<WDT1>     wdt_;                       // the i.MX RT's WDOG1
    volatile bool    noted_ = false;             // a note waits for its restart
};

extern Watchdog watchdog;

Watchdog* Watchdog::self_ = nullptr;

// finds the interrupted code's saved registers, on a task's stack or on the interrupts' stack
__attribute__((naked)) static void watchdogWarning() {
    asm volatile(
        "tst lr, #4          \n"
        "ite eq              \n"
        "mrseq r0, msp       \n"
        "mrsne r0, psp       \n"
        "b watchdogWarned    \n");
}

// the warning, half a second before the restart: notes where the interrupted code stood
extern "C" void watchdogWarned(const uint32_t* frame) {
    Watchdog::self_->note(Watchdog::kCauseHang, frame[6]);   // the program counter the interrupt saved
    WDOG1_WICR |= WDOG_WICR_WTIS;                // clears the warning; the restart follows
}

void Watchdog::begin() {
    self_ = this;
    WDT_timings_t config;
    config.timeout  = 2;         // in seconds
    config.trigger  = 0.5;       // the warning, in seconds before the restart
    config.callback = [] {};     // WDT_T4 enables the warning only with a callback
    wdt_.begin(config);
    attachInterruptVector(IRQ_WDOG1, watchdogWarning);   // replaces WDT_T4's handler, which cannot see the interrupted code
    NVIC_SET_PRIORITY(IRQ_WDOG1, 32);                     // the watchdog's row in general-design.md
}

void Watchdog::check() {
    if (const uint8_t coils = solenoids.overdue()) {   // each guarded component adds its check
        note(kCauseCoil, coils);
        return;
    }
    if (noted_) {                                // fed after all, so the note is void
        CrashReport.breadcrumb(1, kCauseNone);
        noted_ = false;
    }
    wdt_.feed();
}

void Watchdog::note(unsigned cause, unsigned detail) {
    if (noted_) return;
    CrashReport.breadcrumb(1, cause);
    CrashReport.breadcrumb(2, detail);
    noted_ = true;
}

// prints CrashReport's report line by line where the chip records a restart other than a
// power-up; CrashReport clears it once printed, the reset status included. Breadcrumbs beyond #2
// come from firmware flashed earlier and appear in the first report only
void Watchdog::report(Logger& logger) {
    if (!CrashReport && !(SRC_SRSR & ~SRC_SRSR_IPP_RESET_B)) return;
    struct Lines : Print {
        Logger& log;
        char    line[LOG_LINE_BYTES];
        size_t  n = 0;
        explicit Lines(Logger& l) : log(l) {}
        size_t write(uint8_t c) override {
            if (c == '\r') return 1;
            if (c != '\n') {
                if (n < sizeof(line) - 1) line[n++] = c;   // a line longer than the log takes is cut
                return 1;
            }
            line[n] = 0;
            if (n) log.write("restart", "%s", line);
            n = 0;
            return 1;
        }
    } lines{logger};
    lines.print(CrashReport);
}
