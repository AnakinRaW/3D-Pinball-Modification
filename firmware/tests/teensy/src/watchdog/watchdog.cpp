// Tests on the Teensy whether the watchdog restarts the Teensy when the firmware hangs or a coil
// stays on, notes why, reports a crash and leaves the Teensy running while FreeRTOS's idle task
// feeds it.
//
//   hang      a hang with interrupts on restarts the Teensy, and the note names an address inside
//             the hang's loop
//   coil      a coil that stays overdue restarts the Teensy, the note names that coil, and its
//             trigger pin reads low after the restart
//   start-up  a task that sleeps for longer than the watchdog's time is no restart, since the idle
//             task feeds the watchdog
//   crash     a trap restarts the Teensy, and the log holds the crash report with an address inside
//             the trap
//   power-up  after a person cuts and restores the power, the log holds no restart report
//
// Each test restarts the Teensy and checks the note in its second half. WDOG1 cannot be stopped
// once begun, so the second half begins the watchdog as well and leaves the feeds to the idle task,
// which runs while the image waits for the next command.
//
// The test's coils stand in for the solenoid driver: they drive one trigger pin high and report its
// coil overdue while the pin stays high.
//
// Wiring: the coil board on the trigger pins with the coils disconnected, whose pull-downs hold an
// undriven trigger pin low. coil keeps one trigger pin high until the restart.

#include <Arduino.h>
#include <FreeRTOS.h>
#include <stdlib.h>
#include <string.h>
#include <task.h>
#include "core/watchdog.h"
#include "logging/logger.h"
#include "test-harness.h"

constexpr uint8_t COIL = 3;           // the scoop's coil
constexpr uint8_t TRIGGER_PIN = 37;   // pin-assignment.md: Solenoid trigger 4
constexpr uint32_t MS_PER_S = 1000;
constexpr uint32_t FEED_MS = 500;          // hang: how long the loop feeds before it hangs
constexpr uint32_t FEED_STEP_MS = 10;      // hang: the loop's period while it feeds
constexpr uint32_t HANG_SPAN_BYTES = 32;   // hang: the loop's few instructions, with room
constexpr uint32_t SLEEP_MS = (Watchdog::TIMEOUT_S + 1) * MS_PER_S;   // start-up: past the timeout
constexpr uint32_t THUMB_BIT = 1;   // the lowest bit of a Thumb function's address
constexpr uint32_t CRASH_SPAN_BYTES = 32;
constexpr uint32_t LIMIT_S = 30;
constexpr size_t TEXT = 96;
constexpr size_t LOG_FILE_BYTES = 8192;

// A file in RAM whose writes end at once.
class LogFile : public AppendFile {
public:
    explicit LogFile(uint8_t* bytes) : bytes(bytes) {}

    void write(uint32_t at, const uint8_t* data, size_t n) override {
        if (uint64_t(at) + n > LOG_FILE_BYTES) {
            state = AppendStatus::FAILED;
            return;
        }
        memcpy(bytes + at, data, n);
        size = at + n > size ? at + n : size;
        state = AppendStatus::SUCCEEDED;
    }

    void rewrite(const uint8_t* data, size_t n) override {
        size = 0;
        write(0, data, n);
    }

    AppendStatus status() const override { return state; }

    uint8_t* const bytes;
    size_t size = 0;
    AppendStatus state = AppendStatus::IDLE;
};

EXTMEM static uint8_t firstLogBytes[LOG_FILE_BYTES];
EXTMEM static uint8_t secondLogBytes[LOG_FILE_BYTES];
EXTMEM static uint8_t logBuffer[Logger::BUFFER_BYTES];
static LogFile firstLog(firstLogBytes);
static LogFile secondLog(secondLogBytes);
static Logger logger(firstLog, secondLog, logBuffer, sizeof logBuffer);

// The scoop's coil, on while its trigger pin is driven high.
class TestCoils : public OverdueCoils {
public:
    // Drives the trigger pin high, so the coil stays on.
    void pull() {
        pinMode(TRIGGER_PIN, OUTPUT);
        digitalWriteFast(TRIGGER_PIN, HIGH);
    }

    uint8_t overdue() const override {
        const bool on = *portOutputRegister(TRIGGER_PIN) & digitalPinToBitMask(TRIGGER_PIN);
        return on ? 1u << COIL : 0;
    }
};

static TestCoils coils;
static Watchdog watchdog;

/// Loops with interrupts on until the watchdog restarts the Teensy.
__attribute__((noinline)) static void hangHere() {
    for (;;) {
        asm volatile("nop");
    }
}

__attribute__((noinline, noreturn)) static void crashHere() {
    __builtin_trap();
}

// Starts the in-memory log with two empty files.
static void beginLog() {
    logger.begin(LogFileStart{}, LogFileStart{});
}

// Writes every waiting line to the in-memory log and terminates the first file for string checks.
static void finishLog() {
    logger.flush();
    for (uint8_t i = 0; i < 8; ++i) {
        logger.update();
    }
    const size_t end = firstLog.size < LOG_FILE_BYTES ? firstLog.size : LOG_FILE_BYTES - 1;
    firstLog.bytes[end] = '\0';
}

// Gets whether the last restart came from WDOG1.
static bool watchdogReset() {
    return (SRC_SRSR & SRC_SRSR_WDOG_RST_B) != 0;
}

// Begins the watchdog after the restart and checks the note it read. The idle task feeds it from
// here on.
static bool noted(RestartCause cause) {
    watchdog.begin();
    if (!watchdogReset() || watchdog.lastCause() != cause) {
        char seen[TEXT];
        snprintf(seen, sizeof seen, "WDOG1 reset %d, cause %lu, detail 0x%08lx", watchdogReset(),
                 uint32_t(watchdog.lastCause()), watchdog.lastDetail());
        char wanted[TEXT];
        snprintf(wanted, sizeof wanted, "a WDOG1 reset with cause %lu", uint32_t(cause));
        test::fail(seen, wanted);
        return false;
    }
    return true;
}

static void testHang() {
    // The status of earlier restarts would stand beside this one's.
    CrashReport.clear();
    watchdog.begin();
    watchdog.loopStarts();
    const uint32_t t0 = millis();
    while (millis() - t0 < FEED_MS) {
        watchdog.check();
        delay(FEED_STEP_MS);
    }
    test::restartComing();
    hangHere();
}

static void resumeHang() {
    if (!noted(RestartCause::HANG)) {
        return;
    }
    const uint32_t loopAt = reinterpret_cast<uint32_t>(&hangHere) & ~THUMB_BIT;
    const uint32_t pc = watchdog.lastDetail();
    if (pc < loopAt || pc >= loopAt + HANG_SPAN_BYTES) {
        char seen[TEXT];
        snprintf(seen, sizeof seen, "the hang at 0x%08lx", pc);
        char wanted[TEXT];
        snprintf(wanted, sizeof wanted, "0x%08lx to 0x%08lx", loopAt, loopAt + HANG_SPAN_BYTES);
        test::fail(seen, wanted);
        return;
    }
    test::pass();
}

static void testCoil() {
    CrashReport.clear();
    watchdog.watch(coils);
    watchdog.begin();
    watchdog.loopStarts();
    coils.pull();
    test::restartComing();
    for (;;) {
        watchdog.check();
    }
}

static void resumeCoil() {
    // The pin is read before anything drives it: undriven, the coil board's pull-down holds it.
    pinMode(TRIGGER_PIN, INPUT);
    const bool high = digitalReadFast(TRIGGER_PIN) == HIGH;
    if (!noted(RestartCause::COIL)) {
        return;
    }
    if (watchdog.lastDetail() != (1u << COIL)) {
        char seen[TEXT];
        snprintf(seen, sizeof seen, "coils 0x%02lx", watchdog.lastDetail());
        char wanted[TEXT];
        snprintf(wanted, sizeof wanted, "coils 0x%02x", 1u << COIL);
        test::fail(seen, wanted);
        return;
    }
    if (high) {
        test::fail("the trigger pin high after the restart", "low");
        return;
    }
    test::pass();
}

static void testStartUp() {
    CrashReport.clear();
    // A driver's begin() that sleeps: the idle task feeds the watchdog meanwhile.
    watchdog.begin();
    vTaskDelay(pdMS_TO_TICKS(SLEEP_MS));
    test::restart();
}

static void resumeStartUp() {
    if (watchdogReset()) {
        test::fail("a WDOG1 reset", "a restart by the test alone");
        return;
    }
    watchdog.begin();
    if (watchdog.lastCause() != RestartCause::NONE) {
        test::fail("a cause noted", "none");
        return;
    }
    test::pass();
}

static void testCrash() {
    CrashReport.clear();
    test::crashComing();
    crashHere();
}

static void resumeCrash() {
    watchdog.begin();
    beginLog();
    watchdog.report();
    finishLog();

    const char* const log = reinterpret_cast<const char*>(firstLog.bytes);
    if (strstr(log, " I restart ") == nullptr) {
        test::fail("no crash report in the log", "a crash report");
        return;
    }
    constexpr char CODE_LINE[] = "Code was executing from address 0x";
    const char* const code = strstr(log, CODE_LINE);
    if (code == nullptr) {
        test::fail("no execution address in the crash report", "an address inside crashHere");
        return;
    }
    char* end = nullptr;
    const char* const address = code + sizeof CODE_LINE - 1;
    const uint32_t pc = static_cast<uint32_t>(strtoul(address, &end, 16));
    const uint32_t crashAt = reinterpret_cast<uint32_t>(&crashHere) & ~THUMB_BIT;
    if (end == address || pc < crashAt || pc >= crashAt + CRASH_SPAN_BYTES) {
        char seen[TEXT];
        snprintf(seen, sizeof seen, "the crash at 0x%08lx", pc);
        char wanted[TEXT];
        snprintf(wanted, sizeof wanted, "0x%08lx to 0x%08lx", crashAt, crashAt + CRASH_SPAN_BYTES);
        test::fail(seen, wanted);
        return;
    }
    if (CrashReport) {
        test::fail("CrashReport still present", "an empty CrashReport after reporting");
        return;
    }
    test::pass();
}

static void testPowerUp() {
    CrashReport.clear();
    test::step("cut the Teensy's power within 20 seconds, then restore it");
    test::restartComing();
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void resumePowerUp() {
    const uint32_t status = SRC_SRSR;
    if ((status & ~SRC_SRSR_IPP_RESET_B) != 0) {
        char seen[TEXT];
        snprintf(seen, sizeof seen, "reset status 0x%08lx", status);
        test::fail(seen, "only the power-up reset bit");
        return;
    }
    watchdog.begin();
    beginLog();
    watchdog.report();
    logInfo("watchdog", "power-up resumed");
    finishLog();

    const char* const log = reinterpret_cast<const char*>(firstLog.bytes);
    if (strstr(log, " restart ") != nullptr) {
        test::fail("a restart report in the log", "no restart report for power-up");
        return;
    }
    if (strstr(log, " I watchdog power-up resumed") == nullptr) {
        test::fail("no power-up control line in the log", "the control line");
        return;
    }
    if (watchdog.lastCause() != RestartCause::NONE) {
        test::fail("a watchdog cause was noted", "none");
        return;
    }
    test::pass();
}

static const Test TESTS[] = {
    {"watchdog.hang", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testHang, resumeHang},
    {"watchdog.coil", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testCoil, resumeCoil},
    {"watchdog.start-up", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testStartUp, resumeStartUp},
    {"watchdog.crash", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testCrash, resumeCrash},
    {"watchdog.power-up", RunsOn::BENCH, LIMIT_S, LIMIT_S, testPowerUp, resumePowerUp},
};

extern const Program WATCHDOG = {TESTS, sizeof TESTS / sizeof TESTS[0]};
