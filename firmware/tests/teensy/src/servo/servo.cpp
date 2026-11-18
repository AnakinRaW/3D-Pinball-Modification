// Tests on the bench whether the servo driver sends the right pulses and moves the seesaw.
//
//   first-pulse  from before begin() on, every pulse lies inside the SER0049's range
//   pulses       angles handed to configure() give the pulses of those angles
//   ends         the seesaw reaches both end positions without straining (asks at the bench)
//   defaults     without configure(), the end positions take the default angles
//   speed        a slow and a fast setting move the pulse at the speed they set
//
// Wiring: the SER0049 on pin 0, and a wire from pin 0 to pin 1, where the program times each
// pulse edge. Without the wire, the timed tests report SKIP.
//
// Each test that hands the driver other settings uses a ServoDriver of its own. The ones before it
// stay at their target and write no pulse, so only the newest one moves the servo. first-pulse
// times the first pulse since the start, so it runs first after a restart.

#include <Arduino.h>
#include "test-harness.h"
#include <servo-driver.h>

constexpr uint8_t CAPTURE_PIN = 1;   // the wire from the servo pin lands here
constexpr uint32_t EDGES = 1024;     // edges kept between two drains
constexpr uint32_t MAX_PULSES = 512;

// servo.md: attach() takes the SER0049's range, 500 µs for 0 degrees and 2500 µs for 180
constexpr uint32_t PULSE_0_NS = 500000;
constexpr uint32_t PULSE_180_NS = 2500000;
constexpr uint32_t SPAN_DEG = 180;
constexpr uint32_t FRAME_NS = 20000000;   // PWMServo's frame
constexpr uint32_t FRAME_MS = 20;
constexpr uint32_t STEP_NS = FRAME_NS / 4096;    // one step of the pulse at 12 bits, design.md
constexpr uint32_t FRAME_TOLERANCE_NS = 10000;   // the capture's own jitter, well under a step
constexpr uint32_t SETTLE_MS = 1000;             // added to a move's travel time
constexpr uint32_t WINDOW_FRAMES = 10;           // frames timed at an end position
constexpr uint32_t ENDS_ROUNDS = 3;
constexpr uint32_t ASK_TIMEOUT_MS = 30000;
// the longest each test takes, its settle and travel times summed with room to spare
constexpr uint32_t TEENSY_LIMIT_S = 15;
constexpr uint32_t ENDS_LIMIT_S = 60;

// servo.md, Config: the defaults of servo.start_deg and servo.end_deg
constexpr int32_t DEFAULT_START_DEG = 0;
constexpr int32_t DEFAULT_END_DEG = 40;
// pulses: two angles other than the defaults; speed: below and above one degree per frame
constexpr int32_t SET_START_DEG = 10;
constexpr int32_t SET_END_DEG = 50;
constexpr int32_t SLOW_DEG_S = 25;
constexpr int32_t FAST_DEG_S = 100;

static ServoDriver firstServo;
static ServoDriver configuredServo;
static ServoDriver endsServo;
static ServoDriver defaultServo;
static ServoDriver slowServo;
static ServoDriver fastServo;

// The capture interrupt keeps each edge's cycle count with the pin's level in bit 0.
static volatile uint32_t edges[EDGES];
static volatile uint32_t edgeHead = 0;
static uint32_t edgeTail = 0;
static bool anyEdge = false;

struct Pulse {
    uint32_t widthNs;
    uint32_t fallCycles;
};
static Pulse pulses[MAX_PULSES];
static uint32_t pulseCount = 0;
static bool lost = false;
static uint32_t riseCycles = 0;
static bool haveRise = false;

static bool captureOn = false;

static void edgeIsr() {
    edges[edgeHead % EDGES] = (ARM_DWT_CYCCNT & ~1u) | digitalReadFast(CAPTURE_PIN);
    edgeHead = edgeHead + 1;
}

// Starts noting the edges on the capture pin, once after the start.
static void startCapture() {
    if (captureOn) {
        return;
    }
    pinMode(CAPTURE_PIN, INPUT_PULLDOWN);
    attachInterrupt(CAPTURE_PIN, edgeIsr, CHANGE);
    captureOn = true;
}

static uint32_t cyclesToNs(uint32_t cycles) {
    return uint32_t(uint64_t(cycles) * 1000000000u / F_CPU_ACTUAL);
}

// Turns the edges noted since the last drain into pulses, a rising edge and the falling one after.
static void drain() {
    const uint32_t head = edgeHead;
    if (head - edgeTail > EDGES) {
        lost = true;
        edgeTail = head - EDGES;
        haveRise = false;
    }
    for (; edgeTail != head; ++edgeTail) {
        const uint32_t e = edges[edgeTail % EDGES];
        anyEdge = true;
        if (e & 1) {
            riseCycles = e;
            haveRise = true;
            continue;
        }
        if (!haveRise) {
            continue;
        }
        haveRise = false;
        if (pulseCount == MAX_PULSES) {
            lost = true;
            continue;
        }
        pulses[pulseCount++] = {cyclesToNs(e - riseCycles), e};
    }
}

// Forgets the pulses timed so far.
static void forget() {
    drain();
    pulseCount = 0;
    lost = false;
}

// Times pulses for a while. A test bench, so the program may wait.
static void collect(uint32_t ms) {
    const uint32_t t0 = millis();
    while (millis() - t0 < ms) {
        drain();
    }
    drain();
}

static uint32_t widthOfNs(int32_t deg) {
    return PULSE_0_NS + (PULSE_180_NS - PULSE_0_NS) / SPAN_DEG * uint32_t(deg);
}

static uint32_t differenceNs(uint32_t a, uint32_t b) {
    return a > b ? a - b : b - a;
}

// FAIL with the seen pulse width in µs to two decimals.
static void failWidth(const char* what, uint32_t seenNs, uint32_t wantedNs) {
    char seen[64];
    char wanted[48];
    snprintf(seen, sizeof seen, "%s %lu.%02lu us", what, seenNs / 1000, seenNs % 1000 / 10);
    snprintf(wanted, sizeof wanted, "%lu.%02lu us +- %lu.%02lu us", wantedNs / 1000,
             wantedNs % 1000 / 10, STEP_NS / 1000, STEP_NS % 1000 / 10);
    test::fail(seen, wanted);
}

static bool wireMissing() {
    if (anyEdge) {
        return false;
    }
    test::skip("no edge on pin 1, the wire from pin 0 is not fitted");
    return true;
}

// Moves to a target and lets the servo arrive.
static void moveAndSettle(ServoDriver& servo, ServoDriver::Position target) {
    servo.moveTo(target);
    collect(servo.travelMs() + SETTLE_MS);
}

// Checks that the servo gets one pulse of the wanted width in every frame. Reports a failure and
// returns false where it does not.
static bool checkSteady(const char* where, uint32_t wantedNs) {
    forget();
    collect(WINDOW_FRAMES * FRAME_MS + FRAME_MS / 2);
    char seen[64];
    if (lost) {
        test::fail("edges lost", "every edge timed");
        return false;
    }
    if (pulseCount + 1 < WINDOW_FRAMES || pulseCount > WINDOW_FRAMES + 1) {
        snprintf(seen, sizeof seen, "%lu pulses in %lu frames at %s", pulseCount, WINDOW_FRAMES,
                 where);
        test::fail(seen, "one pulse each frame");
        return false;
    }
    for (uint32_t i = 0; i < pulseCount; ++i) {
        if (differenceNs(pulses[i].widthNs, wantedNs) > STEP_NS) {
            snprintf(seen, sizeof seen, "pulse at %s", where);
            failWidth(seen, pulses[i].widthNs, wantedNs);
            return false;
        }
        const uint32_t periodNs =
            i ? cyclesToNs(pulses[i].fallCycles - pulses[i - 1].fallCycles) : FRAME_NS;
        if (differenceNs(periodNs, FRAME_NS) > FRAME_TOLERANCE_NS) {
            snprintf(seen, sizeof seen, "frame of %lu ns at %s", periodNs, where);
            test::fail(seen, "20000000 ns");
            return false;
        }
    }
    return true;
}

// first-pulse: every pulse from before begin() until the servo stands at its start position lies
// in the SER0049's range, give or take one step of the pulse.
static void testFirstPulse() {
    startCapture();
    forget();
    collect(FRAME_MS * 10);
    if (!firstServo.begin()) {
        test::fail("begin() false", "begin() true");
        return;
    }
    collect(firstServo.travelMs() + SETTLE_MS);
    if (wireMissing()) {
        return;
    }
    if (pulseCount == 0) {
        test::fail("no pulse", "pulses at the start position");
        return;
    }
    for (uint32_t i = 0; i < pulseCount; ++i) {
        const uint32_t w = pulses[i].widthNs;
        if (w + STEP_NS < PULSE_0_NS || w > PULSE_180_NS + STEP_NS) {
            char seen[48];
            snprintf(seen, sizeof seen, "pulse %lu of %lu.%02lu us", i + 1, w / 1000,
                     w % 1000 / 10);
            test::fail(seen, "500 to 2500 us +- 4.88 us");
            return;
        }
    }
    test::pass();
}

// pulses: with both angles handed to configure(), each end position gets the pulse of its angle.
static void testPulses() {
    startCapture();
    ServoDriver::Settings settings;
    settings.startDeg = uint8_t(SET_START_DEG);
    settings.endDeg = uint8_t(SET_END_DEG);
    configuredServo.configure(settings);
    if (!configuredServo.begin()) {
        test::fail("begin() false", "begin() true");
        return;
    }
    collect(SETTLE_MS);
    if (wireMissing()) {
        return;
    }
    moveAndSettle(configuredServo, ServoDriver::Position::END);
    if (!checkSteady("the end", widthOfNs(SET_END_DEG))) {
        return;
    }
    moveAndSettle(configuredServo, ServoDriver::Position::START);
    if (!checkSteady("the start", widthOfNs(SET_START_DEG))) {
        return;
    }
    test::pass();
}

// ends: a person watches the seesaw over several moves between the two default end positions,
// the ones the machine uses.
static void testEnds() {
    startCapture();
    if (!endsServo.begin()) {
        test::fail("begin() false", "begin() true");
        return;
    }
    collect(SETTLE_MS);
    for (uint32_t i = 0; i < ENDS_ROUNDS; ++i) {
        moveAndSettle(endsServo, ServoDriver::Position::END);
        moveAndSettle(endsServo, ServoDriver::Position::START);
    }
    switch (test::ask("Did the seesaw reach each extreme without the servo straining or humming "
                      "against it?",
                      ASK_TIMEOUT_MS)) {
    case test::Answer::YES:
        test::pass();
        break;
    case test::Answer::NO:
        test::fail("n", "y");
        break;
    case test::Answer::NONE:
        test::skip("no answer");
        break;
    }
}

// defaults: without configure(), the end positions get the pulses of the defaults.
static void testDefaults() {
    startCapture();
    if (!defaultServo.begin()) {
        test::fail("begin() false", "begin() true");
        return;
    }
    collect(SETTLE_MS);
    if (wireMissing()) {
        return;
    }
    moveAndSettle(defaultServo, ServoDriver::Position::END);
    if (!checkSteady("the end", widthOfNs(DEFAULT_END_DEG))) {
        return;
    }
    moveAndSettle(defaultServo, ServoDriver::Position::START);
    if (!checkSteady("the start", widthOfNs(DEFAULT_START_DEG))) {
        return;
    }
    test::pass();
}

// One speed: the pulse walks from the start to the end position in the frames the speed asks for,
// never back, and stops on the end position's pulse.
static bool checkSpeed(ServoDriver& servo, int32_t speedDegS) {
    ServoDriver::Settings settings;
    settings.speedDegS = uint32_t(speedDegS);
    servo.configure(settings);
    if (!servo.begin()) {
        test::fail("begin() false", "begin() true");
        return false;
    }
    collect(SETTLE_MS);
    if (wireMissing()) {
        return false;
    }
    forget();
    servo.moveTo(ServoDriver::Position::END);
    collect(servo.travelMs() + SETTLE_MS);
    char seen[64];
    char wanted[48];
    if (lost || pulseCount < 2) {
        test::fail("edges lost or no pulse", "every pulse timed");
        return false;
    }
    // A change of a whole degree moves the pulse by 11 µs, more than two steps.
    const uint32_t changeNs = STEP_NS;
    uint32_t first = 0;
    uint32_t last = 0;
    for (uint32_t i = 1; i < pulseCount; ++i) {
        const uint32_t a = pulses[i - 1].widthNs;
        const uint32_t b = pulses[i].widthNs;
        if (b + changeNs < a) {
            snprintf(seen, sizeof seen, "pulse %lu shorter than the one before at %ld deg/s", i + 1,
                     speedDegS);
            test::fail(seen, "a pulse that only grows");
            return false;
        }
        if (b > a + changeNs) {
            first = first ? first : i;
            last = i;
        }
    }
    const uint32_t spanDeg = uint32_t(DEFAULT_END_DEG - DEFAULT_START_DEG);
    const uint32_t mdegPerFrame = uint32_t(speedDegS) * FRAME_MS;
    const uint32_t wantedFrames = (spanDeg * 1000 + mdegPerFrame - 1) / mdegPerFrame;
    // The first change comes once a whole degree has gathered, so the measured run is short by
    // up to the frames one degree takes.
    const uint32_t slack = (1000 + mdegPerFrame - 1) / mdegPerFrame;
    const uint32_t frames = first ? last - first + 1 : 0;
    if (frames + slack < wantedFrames || frames > wantedFrames + 1) {
        snprintf(seen, sizeof seen, "%lu frames at %ld deg/s", frames, speedDegS);
        snprintf(wanted, sizeof wanted, "%lu frames, %lu fewer at most", wantedFrames, slack);
        test::fail(seen, wanted);
        return false;
    }
    const uint32_t endNs = widthOfNs(DEFAULT_END_DEG);
    const uint32_t lastNs = pulses[pulseCount - 1].widthNs;
    if (differenceNs(lastNs, endNs) > STEP_NS) {
        snprintf(seen, sizeof seen, "last pulse at %ld deg/s", speedDegS);
        failWidth(seen, lastNs, endNs);
        return false;
    }
    return true;
}

// speed: below and above one degree per frame.
static void testSpeed() {
    startCapture();
    if (checkSpeed(slowServo, SLOW_DEG_S) && checkSpeed(fastServo, FAST_DEG_S)) {
        test::pass();
    }
}

static const Test TESTS[] = {
    {"servo.first-pulse", RunsOn::TEENSY, TEENSY_LIMIT_S, TEENSY_LIMIT_S, testFirstPulse, nullptr},
    {"servo.pulses", RunsOn::TEENSY, TEENSY_LIMIT_S, TEENSY_LIMIT_S, testPulses, nullptr},
    {"servo.ends", RunsOn::BENCH, ENDS_LIMIT_S, ENDS_LIMIT_S, testEnds, nullptr},
    {"servo.defaults", RunsOn::TEENSY, TEENSY_LIMIT_S, TEENSY_LIMIT_S, testDefaults, nullptr},
    {"servo.speed", RunsOn::TEENSY, TEENSY_LIMIT_S, TEENSY_LIMIT_S, testSpeed, nullptr},
};

extern const Program SERVO = {TESTS, sizeof TESTS / sizeof TESTS[0]};
