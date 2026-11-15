// Watchdog test for the Teensy 4.1 under FreeRTOS, with the watchdog of firmware/gist/error-handling.cpp:
// WDT_T4 on WDOG1 with a 2 s timeout, its warning 0.5 s before the restart at priority 32, and a
// warning handler that notes the interrupted program counter in CrashReport's breadcrumbs.
//
// The test runs over three starts and keeps its state in RAM2, which a watchdog restart leaves:
// 1. The main task feeds the watchdog for 5 s, then hangs in a loop with interrupts on.
// 2. After that restart it prints the restart's cause and CrashReport, then hangs again with
//    every interrupt off, where the warning cannot run.
// 3. After that restart it prints the cause again and ends.
// It starts once the serial monitor is open, after every restart.

#define USE_ARDUINO_DEFINES   // keeps Arduino's macros, which WDT_T4 uses (constrain)
#include "arduino_freertos.h"
#include "Watchdog_t4.h"

constexpr uint32_t kMagic     = 0x57444F47;   // "WDOG"
constexpr unsigned kCauseNone = 0;
constexpr unsigned kCauseHang = 2;

struct State {
    uint32_t magic;
    uint32_t phase;          // which hang the last start entered
    uint32_t warnedMs;       // from the last feed to the warning, 0 where no warning came
    uint32_t warnedPc;       // the program counter the warning found
    uint32_t hangPc;         // where the hang loop lies
};
DMAMEM static State state __attribute__((aligned(32)));

static WDT_T4<WDT1> wdt;
static volatile uint32_t lastFeed = 0;

static void save() { arm_dcache_flush((void*)&state, sizeof state); }

extern "C" void watchdogWarned(const uint32_t* frame) {
    state.warnedMs = millis() - lastFeed;
    state.warnedPc = frame[6];
    save();
    CrashReport.breadcrumb(1, kCauseHang);
    CrashReport.breadcrumb(2, frame[6]);
    WDOG1_WICR |= WDOG_WICR_WTIS;
}

__attribute__((naked)) static void watchdogWarning() {
    asm volatile(
        "tst lr, #4          \n"
        "ite eq              \n"
        "mrseq r0, msp       \n"
        "mrsne r0, psp       \n"
        "b watchdogWarned    \n");
}

static void startWatchdog() {
    WDT_timings_t config;
    config.timeout  = 2;
    config.trigger  = 0.5;
    config.callback = [] {};
    wdt.begin(config);
    attachInterruptVector(IRQ_WDOG1, watchdogWarning);
    NVIC_SET_PRIORITY(IRQ_WDOG1, 32);
}

static void feed() { wdt.feed(); lastFeed = millis(); }

static void printCause(uint32_t srsr) {
    Serial.printf("  reset status 0x%08lX:%s%s%s\n", srsr,
                  (srsr & SRC_SRSR_WDOG_RST_B) ? " WDOG1" : "",
                  (srsr & SRC_SRSR_IPP_RESET_B) ? " power-up" : "",
                  (srsr & SRC_SRSR_LOCKUP_SYSRESETREQ) ? " software or lockup" : "");
}

__attribute__((noinline)) static void hangWithInterrupts() {
    for (;;) asm volatile("nop");
}

__attribute__((noinline)) static void hangWithoutInterrupts() {
    __disable_irq();
    for (;;) asm volatile("nop");
}

static void mainMain(void*) {
    const uint32_t srsr = SRC_SRSR;
    startWatchdog();
    while (!Serial) { feed(); vTaskDelay(10); }
    arm_dcache_delete((void*)&state, sizeof state);
    if (state.magic != kMagic) { state = {kMagic, 0, 0, 0, 0}; save(); }

    Serial.printf("\nWatchdog test, start after phase %lu\n", state.phase);
    printCause(srsr);
    if (state.phase == 1) {
        Serial.printf("  hang with interrupts on: warning %lu ms after the last feed, at 0x%08lX; the hang loop lies at 0x%08lX\n",
                      state.warnedMs, state.warnedPc, state.hangPc);
        Serial.print(CrashReport);
    } else if (state.phase == 2) {
        Serial.printf("  hang with interrupts off: %s\n", state.warnedMs ? "a warning came" : "no warning, as expected");
        Serial.print(CrashReport);
        state.magic = 0; save();
        Serial.println("\ndone");
        for (;;) { feed(); vTaskDelay(100); }
    }

    // feeding keeps the Teensy running
    const uint32_t t0 = millis();
    uint32_t feeds = 0;
    while (millis() - t0 < 5000) { feed(); feeds++; vTaskDelay(5); }
    Serial.printf("  fed %lu times over 5 s without a restart\n", feeds);

    state.warnedMs = 0; state.warnedPc = 0;
    if (state.phase == 0) {
        state.phase  = 1;
        state.hangPc = (uint32_t)&hangWithInterrupts;
        save();
        Serial.println("  hanging with interrupts on ...");
        Serial.flush(); vTaskDelay(50);
        hangWithInterrupts();
    } else {
        state.phase  = 2;
        state.hangPc = (uint32_t)&hangWithoutInterrupts;
        save();
        Serial.println("  hanging with interrupts off ...");
        Serial.flush(); vTaskDelay(50);
        hangWithoutInterrupts();
    }
}

void setup() {
    Serial.begin(115200);
    xTaskCreate(mainMain, "main", 8192 / sizeof(StackType_t), nullptr, 1, nullptr);
    vTaskStartScheduler();
}

void loop() {}
