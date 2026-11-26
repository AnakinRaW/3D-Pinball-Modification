// The test image: every Teensy test of every program in one firmware. The host picks the tests
// with a command line over USB serial, and the image prints each run in the lines of the Results
// section of docs/firmware/testing.md. firmware/tools/teensy-tests.py is that host.
//
// Commands, one to a line:
//   list                       TEST <name> <runs on> <quick s> <soak s> for every test, then
//                              READY <tests run since the start>
//   run <pattern>... [soak]    runs every test whose name matches a pattern, in the order of
//                              testing.md, then END. A * in a pattern stands for any text
//   resume <test> [soak]       runs the second half of a test that restarted the Teensy, then END
//   reset                      restarts the Teensy
//   blank                      sets the display up, which leaves it black, then BLANK
//
// The image keeps no state across a restart. A crash report left by the run before is printed as
// CRASH lines once the host opens the port. The tests run in a FreeRTOS task, as the firmware's
// main loop does, and setup() starts the clock before it, as the firmware's setup() does.

#include <Arduino.h>
#include <FreeRTOS.h>
#include "core/clock.h"
#include "drivers/display.h"
#include "logging/debug-log.h"
#include <string.h>
#include <task.h>
#include "test-drivers.h"
#include "test-harness.h"

extern const Program CLOCK;
extern const Program TASK_LOCK;
extern const Program DEBUG_OUTPUT;
extern const Program DEBUG_TRACE_PROGRAM;
extern const Program CARD_APPENDER;
extern const Program EVENT_QUEUE;
extern const Program DRIVER_TICK;
extern const Program PSRAM;
extern const Program STORAGE;
extern const Program FILE_CHANNEL;
extern const Program WS2812_FLEXIO2;
extern const Program LIGHTING;
extern const Program BREAK_BEAM;
extern const Program I2C_BUS;
extern const Program SERVO;
extern const Program AUDIO;
extern const Program DISPLAY_PROGRAM;
extern const Program TOUCH_PROGRAM;
extern const Program EVERY_DRIVER;
extern const Program LAMPS;
extern const Program SOUNDS;
extern const Program SCREEN;
extern const Program VIDEO_PLAYER;
extern const Program LOGGER;
extern const Program START_UP;

// the programs in the order their sections stand in testing.md
static const Program* const PROGRAMS[] = {
    &CLOCK,           &TASK_LOCK,     &DEBUG_OUTPUT,   &DEBUG_TRACE_PROGRAM,
    &CARD_APPENDER,   &EVENT_QUEUE,   &DRIVER_TICK,    &PSRAM,
    &STORAGE,         &FILE_CHANNEL,  &WS2812_FLEXIO2, &LIGHTING,
    &BREAK_BEAM,      &I2C_BUS,       &SERVO,          &AUDIO,
    &DISPLAY_PROGRAM, &TOUCH_PROGRAM, &EVERY_DRIVER,   &LAMPS,
    &SOUNDS,          &SCREEN,        &VIDEO_PLAYER,   &LOGGER,
    &START_UP};

constexpr uint32_t LINE_BYTES = 256;         // the longest command line
constexpr uint32_t MAX_WORDS = 16;           // words in one command line
constexpr uint32_t RESTART_FLUSH_MS = 100;   // lets the last line leave over USB before a restart
constexpr uint32_t DRAIN_IDLE_PASSES = 3;    // ms in a row without a sent debug byte
// the stack of the task the tests run in, enough for the largest test's local variables
constexpr uint32_t TEST_STACK_BYTES = 32768;

// ARMv7-M Architecture Reference Manual, B3.2.6: AIRCR takes a write only with the key 0x05FA in
// its upper half, and SYSRESETREQ, bit 2, restarts the processor
constexpr uint32_t AIRCR_VECTKEY = 0x05FA0000;
constexpr uint32_t AIRCR_SYSRESETREQ = 1u << 2;

static char line[LINE_BYTES];
static uint32_t lineLength = 0;
static bool lineTooLong = false;

static bool soakRun = false;
static const Test* current = nullptr;
static bool reported = false;
static uint32_t testsRun = 0;
static uint32_t passed = 0;
static uint32_t failed = 0;
static uint32_t skipped = 0;

// Prints a text with a word in front of each of its lines, for the crash report.
class LinePrefix : public Print {
public:
    explicit LinePrefix(const char* prefix) : prefix(prefix) {}

    size_t write(uint8_t c) override {
        if (c == '\r') {
            return 1;
        }
        if (lineStart) {
            Serial.print(prefix);
            lineStart = false;
        }
        Serial.write(c);
        lineStart = c == '\n';
        return 1;
    }

private:
    const char* prefix;
    bool lineStart = true;
};

static void drainDebugLog();

[[noreturn]] static void restartNow() {
    drainDebugLog();
    Serial.flush();
    delay(RESTART_FLUSH_MS);
    SCB_AIRCR = AIRCR_VECTKEY | AIRCR_SYSRESETREQ;
    for (;;) {
    }
}

// Sends the waiting debug lines, so they stand ahead of the line printed next. It waits until
// USB serial has taken them or has stopped taking any.
static void drainDebugLog() {
    uint32_t idle = 0;
    while (idle < DRAIN_IDLE_PASSES) {
        idle = sendDebugLog() == 0 ? idle + 1 : 0;
        delay(1);
    }
}

// Notes that the current test has reported. A second result is a fault of the test.
static bool firstResult() {
    drainDebugLog();
    if (current == nullptr) {
        Serial.printf("ERROR a result outside a test\n");
        return false;
    }
    if (reported) {
        Serial.printf("ERROR %s reported a second result\n", current->name);
        ++failed;
        return false;
    }
    reported = true;
    return true;
}

EXTMEM static uint8_t scratchBytes[test::SCRATCH_BYTES] __attribute__((aligned(32)));

namespace test {

uint8_t* scratch() {
    return scratchBytes;
}

bool soak() {
    return soakRun;
}

uint32_t durationMs(uint32_t quickMs, uint32_t soakMs) {
    return soakRun ? soakMs : quickMs;
}

void pass() {
    if (firstResult()) {
        Serial.printf("PASS %s\n", current->name);
        ++passed;
    }
}

void fail(const char* seen, const char* wanted) {
    if (firstResult()) {
        Serial.printf("FAIL %s %s expected %s\n", current->name, seen, wanted);
        ++failed;
    }
}

void skip(const char* reason) {
    if (firstResult()) {
        Serial.printf("SKIP %s %s\n", current->name, reason);
        ++skipped;
    }
}

void step(const char* text) {
    Serial.printf("DO %s %s\n", current->name, text);
}

Answer ask(const char* question, uint32_t timeoutMs) {
    while (Serial.available()) {
        Serial.read();
    }
    Serial.printf("ASK %s %s\n", current->name, question);
    const uint32_t t0 = millis();
    while (millis() - t0 < timeoutMs) {
        const int c = Serial.read();
        if (c == 'y') {
            return Answer::YES;
        }
        if (c == 'n') {
            return Answer::NO;
        }
    }
    return Answer::NONE;
}

bool check(const char* tag, uint32_t count, uint32_t& good, uint32_t& total, uint32_t& lost,
           uint32_t timeoutMs) {
    drainDebugLog();
    while (Serial.available()) {
        Serial.read();
    }
    Serial.printf("CHECK %s %s %lu\n", current->name, tag, count);
    char answer[LINE_BYTES];
    uint32_t length = 0;
    const uint32_t t0 = millis();
    while (millis() - t0 < timeoutMs) {
        const int c = Serial.read();
        if (c < 0 || c == '\r') {
            continue;
        }
        if (c != '\n') {
            if (length + 1 < LINE_BYTES) {
                answer[length++] = char(c);
            }
            continue;
        }
        answer[length] = '\0';
        unsigned long g = 0;
        unsigned long t = 0;
        unsigned long l = 0;
        if (sscanf(answer, "checked %lu %lu %lu", &g, &t, &l) == 3) {
            good = g;
            total = t;
            lost = l;
            return true;
        }
        length = 0;
    }
    return false;
}

void restart() {
    Serial.printf("RESTART %s\n", current->name);
    restartNow();
}

}   // namespace test

// Matches a name against a pattern in which * stands for any text. Iterative: on a mismatch after
// a *, the * takes one more character of the name.
static bool matches(const char* pattern, const char* name) {
    const char* star = nullptr;
    const char* afterStar = nullptr;
    while (*name != '\0') {
        if (*pattern == '*') {
            star = pattern++;
            afterStar = name;
        } else if (*pattern == *name) {
            ++pattern;
            ++name;
        } else if (star != nullptr) {
            pattern = star + 1;
            name = ++afterStar;
        } else {
            return false;
        }
    }
    while (*pattern == '*') {
        ++pattern;
    }
    return *pattern == '\0';
}

static void runTest(const Test& t, bool resume) {
    current = &t;
    reported = false;
    ++testsRun;
    Serial.printf("RUN %s\n", t.name);
    if (resume) {
        t.resume();
    } else {
        t.run();
    }
    if (!reported) {
        test::fail("no result", "PASS, FAIL or SKIP");
    }
    current = nullptr;
}

static void startCount() {
    passed = 0;
    failed = 0;
    skipped = 0;
}

static void endCount() {
    drainDebugLog();
    Serial.printf("END %lu %lu %lu\n", passed, failed, skipped);
}

static void list() {
    for (const Program* p : PROGRAMS) {
        for (uint8_t i = 0; i < p->count; ++i) {
            const Test& t = p->tests[i];
            Serial.printf("TEST %s %s %lu %lu\n", t.name,
                          t.runsOn == RunsOn::TEENSY ? "Teensy" : "bench", t.quickLimitS,
                          t.soakLimitS);
        }
    }
    Serial.printf("READY %lu\n", testsRun);
}

// Runs every test that matches one of the patterns, each once, in the order of the programs.
static void run(char* const* patterns, uint32_t count) {
    startCount();
    for (const Program* p : PROGRAMS) {
        for (uint8_t i = 0; i < p->count; ++i) {
            const Test& t = p->tests[i];
            for (uint32_t k = 0; k < count; ++k) {
                if (matches(patterns[k], t.name)) {
                    runTest(t, false);
                    break;
                }
            }
        }
    }
    endCount();
}

static void resume(const char* name) {
    startCount();
    for (const Program* p : PROGRAMS) {
        for (uint8_t i = 0; i < p->count; ++i) {
            const Test& t = p->tests[i];
            if (strcmp(t.name, name) != 0) {
                continue;
            }
            if (t.resume == nullptr) {
                Serial.printf("ERROR %s has no second half\n", name);
                return;
            }
            runTest(t, true);
            endCount();
            return;
        }
    }
    Serial.printf("ERROR no test %s\n", name);
}

// Sets the display up, which leaves it black. The panel stays awake through a restart of the
// Teensy, and the bus lines float meanwhile, so a restart can leave a stripe of stray bytes on it.
static void blank() {
    if (test::display.begin()) {
        Serial.printf("BLANK\n");
    } else {
        Serial.printf("ERROR the display does not begin\n");
    }
}

// Splits the line into words, takes soak out of them and carries out the command.
static void command() {
    char* words[MAX_WORDS];
    uint32_t count = 0;
    soakRun = false;
    for (char* w = strtok(line, " \t"); w != nullptr; w = strtok(nullptr, " \t")) {
        if (strcmp(w, "soak") == 0) {
            soakRun = true;
        } else if (count < MAX_WORDS) {
            words[count++] = w;
        }
    }
    if (count == 0) {
        return;
    }
    if (strcmp(words[0], "list") == 0 && count == 1) {
        list();
    } else if (strcmp(words[0], "run") == 0 && count >= 2) {
        run(words + 1, count - 1);
    } else if (strcmp(words[0], "resume") == 0 && count == 2) {
        resume(words[1]);
    } else if (strcmp(words[0], "reset") == 0 && count == 1) {
        restartNow();
    } else if (strcmp(words[0], "blank") == 0 && count == 1) {
        blank();
    } else {
        Serial.printf("ERROR unknown command %s\n", words[0]);
    }
}

// Reads the command lines and carries them out, for good.
static void testTask(void*) {
    for (;;) {
        sendDebugLog();
        while (Serial.available()) {
            const int c = Serial.read();
            if (c == '\r') {
                continue;
            }
            if (c != '\n') {
                if (lineLength + 1 < LINE_BYTES) {
                    line[lineLength++] = char(c);
                } else {
                    lineTooLong = true;
                }
                continue;
            }
            line[lineLength] = '\0';
            if (lineTooLong) {
                Serial.printf("ERROR a line longer than %lu bytes\n", LINE_BYTES - 1);
            } else {
                command();
            }
            lineLength = 0;
            lineTooLong = false;
        }
    }
}

void setup() {
    startClock();
    Serial.begin(115200);
    while (!Serial) {
    }
    // The cycle counter times the tests' edges.
    ARM_DEMCR |= ARM_DEMCR_TRCENA;
    ARM_DWT_CTRL |= ARM_DWT_CTRL_CYCCNTENA;
    if (CrashReport) {
        LinePrefix crash("CRASH ");
        crash.print(CrashReport);
    }
    if (xTaskCreate(testTask, "tests", TEST_STACK_BYTES / sizeof(StackType_t), nullptr, 1,
                    nullptr) != pdPASS) {
        Serial.printf("ERROR no memory for the test task\n");
        return;
    }
    vTaskStartScheduler();
}

// never runs once FreeRTOS has started
void loop() {}
