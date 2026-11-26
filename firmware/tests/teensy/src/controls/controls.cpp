// Tests on the Teensy whether the controls driver sets the toggle switch's pin up and reports the
// flips a person makes.
//
//   pad       begin() leaves the pin an input with the 22 kΩ pull-up and hysteresis, and with an
//             interrupt on both edges
//   position  position() gives the position printed on the switch (asks at the bench)
//   flips     slow flips give one TOGGLE_SWITCHED each, and fast flips end on the position the
//             switch rests in (asks at the bench)
//
// Wiring: pad needs none. position and flips need the stock toggle switch on its pin through R1,
// as the controls design wires it. The shared driver publishes into the firmware's events, which
// this program reads.

#include <Arduino.h>
#include "core/clock.h"
#include "core/event-queue.h"
#include "drivers/toggle-switch.h"
#include "test-drivers.h"
#include "test-harness.h"

constexpr uint8_t SWITCH_PIN = 5;   // pin-assignment.md: Toggle switch
// general-design.md: the pin interrupt runs at the driver tick's priority, as the firmware's main
// task sets it
constexpr uint8_t PIN_PRIORITY = 96;
constexpr uint32_t PULL_UP_22K = 3;   // PJRC digital.c: INPUT_PULLUP selects PUS(3), 22 kΩ up
constexpr uint32_t SLOW_FLIPS = 5;
constexpr uint32_t FAST_FLIPS = 10;
constexpr uint32_t QUIET_AFTER_MS = 3000;   // flips: the switch counts as left alone after this
constexpr uint32_t STEP_TIMEOUT_MS = 60000;
constexpr uint32_t PAD_LIMIT_S = 5;
constexpr uint32_t BENCH_LIMIT_S = 300;
constexpr size_t TEXT = 96;

using test::toggleSwitch;
static bool begun = false;

// Begins the driver once after the start, and fails the test where begin() fails.
static bool ready() {
    if (!begun) {
        NVIC_SET_PRIORITY(IRQ_GPIO6789, PIN_PRIORITY);
        begun = toggleSwitch.begin();
        if (!begun) {
            test::fail("begin() false", "true");
        }
    }
    return begun;
}

// Drops every event the firmware's events hold.
static void drain() {
    DriverEvent e;
    while (events.read(&e, 1) == 1) {
    }
}

// Waits up to ms for the next TOGGLE_SWITCHED, and gets whether it came.
static bool nextFlip(DriverEvent& e, uint32_t ms) {
    const uint32_t t0 = millis();
    while (millis() - t0 < ms) {
        if (events.read(&e, 1) == 1 && e.type == DriverEventType::TOGGLE_SWITCHED) {
            return true;
        }
        delay(1);
    }
    return false;
}

// Asks a person to set the switch to a position and waits until position() gives it.
static bool setTo(uint8_t wanted) {
    char text[TEXT];
    snprintf(text, sizeof text, "set the switch to the position printed %u", wanted);
    test::step(text);
    const uint32_t t0 = millis();
    while (toggleSwitch.position() != wanted) {
        if (millis() - t0 >= STEP_TIMEOUT_MS) {
            snprintf(text, sizeof text, "position() %u", toggleSwitch.position());
            char expected[TEXT];
            snprintf(expected, sizeof expected, "%u", wanted);
            test::fail(text, expected);
            return false;
        }
        delay(1);
    }
    return true;
}

static void testPad() {
    if (!ready()) {
        return;
    }
    const uint32_t pad = *portControlRegister(SWITCH_PIN);
    const uint32_t wanted =
        IOMUXC_PAD_PKE | IOMUXC_PAD_PUE | IOMUXC_PAD_PUS(PULL_UP_22K) | IOMUXC_PAD_HYS;
    if ((pad & wanted) != wanted) {
        char seen[TEXT];
        snprintf(seen, sizeof seen, "pad 0x%08lx", pad);
        test::fail(seen, "PKE, PUE, PUS 3 and HYS set");
        return;
    }
    // The pin's GPIO: IMR enables the pin's interrupt, and EDGE_SEL makes it fire on both edges.
    const auto* gpio =
        reinterpret_cast<const volatile IMXRT_GPIO_t*>(portOutputRegister(SWITCH_PIN));
    const uint32_t mask = digitalPinToBitMask(SWITCH_PIN);
    if ((gpio->IMR & mask) == 0 || (gpio->EDGE_SEL & mask) == 0) {
        char seen[TEXT];
        snprintf(seen, sizeof seen, "IMR 0x%08lx, EDGE_SEL 0x%08lx", gpio->IMR, gpio->EDGE_SEL);
        test::fail(seen, "the pin's bit set in both");
        return;
    }
    // The switch closes the pin to ground in position 1.
    const uint8_t fromPin = digitalReadFast(SWITCH_PIN) == LOW ? 1 : 0;
    if (toggleSwitch.position() != fromPin) {
        test::fail("position() differs from the pin", "1 where the pin reads LOW, else 0");
        return;
    }
    test::pass();
}

static void testPosition() {
    if (!ready()) {
        return;
    }
    if (!setTo(1)) {
        return;
    }
    drain();
    if (!setTo(0)) {
        return;
    }
    DriverEvent e;
    if (!nextFlip(e, STEP_TIMEOUT_MS) || payload<DriverEventType::TOGGLE_SWITCHED>(e) != 0) {
        test::fail("no TOGGLE_SWITCHED with position 0", "one as the switch reaches 0");
        return;
    }
    test::pass();
}

static void testFlips() {
    if (!ready()) {
        return;
    }
    drain();
    char text[TEXT];
    char expected[TEXT];
    snprintf(text, sizeof text, "flip the switch %lu times, a second or more between flips",
             SLOW_FLIPS);
    test::step(text);
    uint8_t last = toggleSwitch.position();
    DriverEvent e;
    for (uint32_t i = 0; i < SLOW_FLIPS; ++i) {
        if (!nextFlip(e, STEP_TIMEOUT_MS)) {
            snprintf(text, sizeof text, "%lu TOGGLE_SWITCHED", i);
            snprintf(expected, sizeof expected, "%lu", SLOW_FLIPS);
            test::fail(text, expected);
            return;
        }
        const uint8_t moved = payload<DriverEventType::TOGGLE_SWITCHED>(e);
        if (moved == last) {
            snprintf(text, sizeof text, "flip %lu reports %u again", i + 1, moved);
            test::fail(text, "the other position");
            return;
        }
        last = moved;
        Serial.printf("flip %lu to %u at %lu us\n", i + 1, moved, e.time);
    }
    // The fast flips follow at once, so a person who goes on without a pause stays inside the
    // test.
    snprintf(text, sizeof text, "flip the switch %lu times as fast as possible, then leave it",
             FAST_FLIPS);
    test::step(text);
    if (!nextFlip(e, STEP_TIMEOUT_MS)) {
        test::fail("no TOGGLE_SWITCHED", "one for the first flip");
        return;
    }
    uint32_t count = 0;
    do {
        const uint8_t moved = payload<DriverEventType::TOGGLE_SWITCHED>(e);
        if (moved == last) {
            snprintf(text, sizeof text, "fast flip event %lu reports %u again", count + 1, moved);
            test::fail(text, "the other position");
            return;
        }
        last = moved;
        ++count;
    } while (nextFlip(e, QUIET_AFTER_MS));
    Serial.printf("%lu events for the fast flips\n", count);
    // A hand flips more often than asked, so the count is printed and not judged. Bounce shows as
    // a position reported twice in a row, which the loop above fails.
    if (last != toggleSwitch.position()) {
        snprintf(text, sizeof text, "the last event %u, position() %u", last,
                 toggleSwitch.position());
        test::fail(text, "the last event the position()");
        return;
    }
    test::pass();
}

static const Test TESTS[] = {
    {"controls.pad", RunsOn::TEENSY, PAD_LIMIT_S, PAD_LIMIT_S, testPad, nullptr},
    {"controls.position", RunsOn::BENCH, BENCH_LIMIT_S, BENCH_LIMIT_S, testPosition, nullptr},
    {"controls.flips", RunsOn::BENCH, BENCH_LIMIT_S, BENCH_LIMIT_S, testFlips, nullptr},
};

extern const Program CONTROLS = {TESTS, sizeof TESTS / sizeof TESTS[0]};
