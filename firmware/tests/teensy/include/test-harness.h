// The harness of the test image: a program's table of tests, and the PASS, FAIL and SKIP lines a
// test prints. The Results section of docs/firmware/testing.md holds their format. Every check is
// written into the test as a condition, and the test reports PASS, FAIL or SKIP exactly once.
#pragma once
#include <Arduino.h>

// the "Runs on" column of testing.md
enum class RunsOn : uint8_t { TEENSY, BENCH };

struct Test {
    // the program and the row, such as "servo.pulses"
    const char* name;
    RunsOn runsOn;
    // the longest the test takes in a quick run and in a soak run; the runner gives up after it
    uint32_t quickLimitS;
    uint32_t soakLimitS;
    void (*run)();
    // runs once the Teensy is back from the restart the test asked for with test::restart(),
    // nullptr for a test that asks for none
    void (*resume)();
};

// one section of testing.md: its tests in the order of its table
struct Program {
    const Test* tests;
    uint8_t count;
};

namespace test {

enum class Answer : uint8_t { YES, NO, NONE };

// Gets whether this run takes the durations testing.md states rather than the quick ones.
bool soak();

// Gets quickMs in a quick run and soakMs in a soak run.
uint32_t durationMs(uint32_t quickMs, uint32_t soakMs);

void pass();

// FAIL with what the test saw and what it expected.
void fail(const char* seen, const char* wanted);

// SKIP with the reason, such as a part that is not connected.
void skip(const char* reason);

// Prints the DO line of a step a person carries out. The test watches for the step itself.
void step(const char* text);

// Prints the ASK line and waits up to timeoutMs for y or n over the serial port.
Answer ask(const char* question, uint32_t timeoutMs);

// Prints the RESTART line and restarts the Teensy. The runner then sends "resume <test>", which
// calls the test's resume function, so the Teensy keeps no state across the restart.
[[noreturn]] void restart();

}   // namespace test
