// Tests on the Teensy whether the break beam driver sets its pin up and reports the balls that a
// person passes through the beam.
//
//   pad     begin() leaves the pin an input with hysteresis, without pull-up or keeper, and with
//           an interrupt on both edges
//   pass    ten passes of a ball give ten BALL_DRAINED (asks at the bench)
//   failed  a ball held in the beam is reported failed after 5 s, and recovered once it is out
//           (asks at the bench)
//
// Wiring: pad needs none. pass and failed need the emitter and the receiver of the break beam
// design on the motherboard, and a ball. The driver publishes into the firmware's event queue,
// which this program reads, and a monitor of this program watches it.

#include <Arduino.h>
#include "core/clock.h"
#include "core/device-monitor.h"
#include "core/event-queue.h"
#include "drivers/break-beam.h"
#include "test-harness.h"

constexpr uint8_t BEAM_PIN = 31;   // pin-assignment.md: Receiver output of the ball drain gate
constexpr uint32_t PASSES = 10;
constexpr uint32_t AFTER_MS = 2000;   // pass: the wait for an extra event after the last pass
constexpr uint32_t STEP_TIMEOUT_MS = 120000;
// failed: the monitor's 100 ms check and a ball that flickers as it settles in the beam
constexpr uint32_t FAULT_SLACK_MS = 1000;
constexpr uint32_t HOLD_S = 6;   // failed: longer than BreakBeamDriver::FAIL_MS
constexpr uint32_t PAD_LIMIT_S = 5;
constexpr uint32_t BENCH_LIMIT_S = 300;
constexpr size_t TEXT = 96;

static BreakBeamDriver beam(events);
static bool begun = false;
static EventQueue monitorEvents;
static DriverEvent monitorStorage[DeviceMonitor::DEPTH];
static DeviceMonitor monitor(monitorEvents, driverTick, monitorStorage);
static bool watched = false;

// Begins the driver once after the start, and fails the test where begin() fails.
static bool ready() {
    if (!begun) {
        begun = beam.begin();
        if (!begun) {
            test::fail("begin() false", "true");
        }
    }
    return begun;
}

// Waits up to ms for the next BALL_DRAINED, and gets whether it came.
static bool nextBall(DriverEvent& e, uint32_t ms) {
    const uint32_t t0 = millis();
    while (millis() - t0 < ms) {
        if (events.read(&e, 1) == 1 && e.type == DriverEventType::BALL_DRAINED) {
            return true;
        }
        delay(1);
    }
    return false;
}

// Waits up to ms for the monitor's next event about the break beam, and gets whether it came.
static bool nextFault(DriverEvent& e, uint32_t ms) {
    const uint32_t t0 = millis();
    while (millis() - t0 < ms) {
        if (monitorEvents.read(&e, 1) == 1 && faultDevice(e.source) == Device::BREAK_BEAM) {
            return true;
        }
        delay(1);
    }
    return false;
}

static void testPad() {
    if (!ready()) {
        return;
    }
    const uint32_t pad = *portControlRegister(BEAM_PIN);
    if ((pad & IOMUXC_PAD_HYS) == 0 || (pad & (IOMUXC_PAD_PKE | IOMUXC_PAD_PUE)) != 0) {
        char seen[TEXT];
        snprintf(seen, sizeof seen, "pad 0x%08lx", pad);
        test::fail(seen, "HYS set, PKE and PUE clear");
        return;
    }
    // The pin's GPIO: IMR enables the pin's interrupt, and EDGE_SEL makes it fire on both edges.
    const auto* gpio = reinterpret_cast<const volatile IMXRT_GPIO_t*>(portOutputRegister(BEAM_PIN));
    const uint32_t mask = digitalPinToBitMask(BEAM_PIN);
    if ((gpio->IMR & mask) == 0 || (gpio->EDGE_SEL & mask) == 0) {
        char seen[TEXT];
        snprintf(seen, sizeof seen, "IMR 0x%08lx, EDGE_SEL 0x%08lx", gpio->IMR, gpio->EDGE_SEL);
        test::fail(seen, "the pin's bit set in both");
        return;
    }
    if (beam.blocked() != (digitalReadFast(BEAM_PIN) == HIGH)) {
        test::fail("blocked() differs from the pin", "blocked() true where the pin reads HIGH");
        return;
    }
    test::pass();
}

static void testPass() {
    if (!ready()) {
        return;
    }
    if (beam.blocked()) {
        test::skip("the beam is broken: no emitter, no receiver or a ball in the gate");
        return;
    }
    DriverEvent e;
    while (events.read(&e, 1) == 1) {
    }
    char text[TEXT];
    snprintf(text, sizeof text, "pass a ball through the beam %lu times, some slowly, some fast",
             PASSES);
    test::step(text);
    for (uint32_t i = 0; i < PASSES; ++i) {
        if (!nextBall(e, STEP_TIMEOUT_MS)) {
            snprintf(text, sizeof text, "%lu BALL_DRAINED", i);
            char wanted[TEXT];
            snprintf(wanted, sizeof wanted, "%lu", PASSES);
            test::fail(text, wanted);
            return;
        }
        Serial.printf("pass %lu at %lu us\n", i + 1, e.time);
    }
    if (nextBall(e, AFTER_MS)) {
        snprintf(text, sizeof text, "a BALL_DRAINED after the %lu passes", PASSES);
        test::fail(text, "none");
        return;
    }
    if (beam.blocked()) {
        test::fail("blocked() true with the ball out", "false");
        return;
    }
    test::pass();
}

static void testFailed() {
    if (!ready()) {
        return;
    }
    if (!watched) {
        watched = monitor.watch(Device::BREAK_BEAM, beam);
        if (!watched) {
            test::fail("the monitor refused the driver", "taken");
            return;
        }
    }
    if (beam.blocked()) {
        test::skip("the beam is broken: no emitter, no receiver or a ball in the gate");
        return;
    }
    char text[TEXT];
    snprintf(text, sizeof text, "hold a ball in the beam for %lu s, then take it out", HOLD_S);
    test::step(text);
    DriverEvent e;
    if (!nextBall(e, STEP_TIMEOUT_MS)) {
        test::fail("no BALL_DRAINED", "one as the ball breaks the beam");
        return;
    }
    const uint32_t brokeUs = e.time;
    if (!nextFault(e, BreakBeamDriver::FAIL_MS + FAULT_SLACK_MS) ||
        e.type != DriverEventType::DEVICE_FAILED) {
        test::fail("no DEVICE_FAILED", "DEVICE_FAILED");
        return;
    }
    const uint32_t afterMs = (e.time - brokeUs) / 1000;
    if (afterMs < BreakBeamDriver::FAIL_MS || afterMs > BreakBeamDriver::FAIL_MS + FAULT_SLACK_MS) {
        snprintf(text, sizeof text, "DEVICE_FAILED %lu ms after the break", afterMs);
        char wanted[TEXT];
        snprintf(wanted, sizeof wanted, "%lu to %lu ms", BreakBeamDriver::FAIL_MS,
                 BreakBeamDriver::FAIL_MS + FAULT_SLACK_MS);
        test::fail(text, wanted);
        return;
    }
    if (!nextFault(e, STEP_TIMEOUT_MS) || e.type != DriverEventType::DEVICE_RECOVERED) {
        test::fail("no DEVICE_RECOVERED", "DEVICE_RECOVERED once the ball is out");
        return;
    }
    if (beam.blocked()) {
        test::fail("blocked() true after DEVICE_RECOVERED", "false");
        return;
    }
    test::pass();
}

static const Test TESTS[] = {
    {"break-beam.pad", RunsOn::TEENSY, PAD_LIMIT_S, PAD_LIMIT_S, testPad, nullptr},
    {"break-beam.pass", RunsOn::BENCH, BENCH_LIMIT_S, BENCH_LIMIT_S, testPass, nullptr},
    {"break-beam.failed", RunsOn::BENCH, BENCH_LIMIT_S, BENCH_LIMIT_S, testFailed, nullptr},
};

extern const Program BREAK_BEAM = {TESTS, sizeof TESTS / sizeof TESTS[0]};
