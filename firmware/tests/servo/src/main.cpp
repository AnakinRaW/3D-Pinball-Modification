// The servo test program of docs/firmware/testing.md: ServoDriver as the gist
// firmware/gist/drivers/servo.cpp designs it, on the driver tick, with PWMServoT4 from
// firmware/lib/pwm-servo-t4, and the SER0049 on pin 0.
//
// A wire from pin 0 to pin 1 lets the Teensy tests time every pulse: an interrupt on pin 1 notes
// the cycle count of each edge. Without the wire, pin 1 sees no edge, and those tests report SKIP.
// The program starts once the serial monitor is open and prints its run in the lines of the
// testing document's Results section.
//
// Each test that starts the driver with another config uses a ServoDriver of its own. The ones
// before it stay at their target and write no pulse, so only the newest one moves the servo.

#include <Arduino.h>
#include "servo-driver.h"

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

// servo.md, Config: the defaults of servo.start_deg and servo.end_deg
constexpr int32_t DEFAULT_START_DEG = 0;
constexpr int32_t DEFAULT_END_DEG = 40;
// pulses: two angles other than the defaults; speed: below and above one degree per frame
constexpr int32_t SET_START_DEG = 10;
constexpr int32_t SET_END_DEG = 50;
constexpr int32_t SLOW_DEG_S = 25;
constexpr int32_t FAST_DEG_S = 100;

DriverTick driverTick;
DeviceMonitor deviceMonitor;
Config config;

static ServoDriver firstServo;
static ServoDriver configuredServo;
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

static uint32_t passed = 0;
static uint32_t failed = 0;
static uint32_t skipped = 0;

static void edgeIsr() {
    edges[edgeHead % EDGES] = (ARM_DWT_CYCCNT & ~1u) | digitalReadFast(CAPTURE_PIN);
    edgeHead = edgeHead + 1;
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
static void restart() {
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

static void run(const char* test) {
    Serial.printf("RUN servo.%s\n", test);
}

static void pass(const char* test) {
    Serial.printf("PASS servo.%s\n", test);
    ++passed;
}

static void skip(const char* test, const char* reason) {
    Serial.printf("SKIP servo.%s %s\n", test, reason);
    ++skipped;
}

// FAIL with the seen pulse width in µs to two decimals.
static void failWidth(const char* test, const char* what, uint32_t seenNs, uint32_t wantedNs) {
    Serial.printf("FAIL servo.%s %s %lu.%02lu us expected %lu.%02lu us +- %lu.%02lu us\n", test,
                  what, seenNs / 1000, seenNs % 1000 / 10, wantedNs / 1000, wantedNs % 1000 / 10,
                  STEP_NS / 1000, STEP_NS % 1000 / 10);
    ++failed;
}

static void fail(const char* test, const char* seen, const char* wanted) {
    Serial.printf("FAIL servo.%s %s expected %s\n", test, seen, wanted);
    ++failed;
}

static bool wireMissing(const char* test) {
    if (anyEdge) {
        return false;
    }
    skip(test, "no edge on pin 1, the wire from pin 0 is not fitted");
    return true;
}

// Moves to a target and lets the servo arrive.
static void moveAndSettle(ServoDriver& servo, ServoDriver::Position target) {
    servo.moveTo(target);
    collect(servo.travelMs() + SETTLE_MS);
}

// Checks that the servo gets one pulse of the wanted width in every frame. Reports a failure and
// returns false where it does not.
static bool checkSteady(const char* test, const char* where, uint32_t wantedNs) {
    restart();
    collect(WINDOW_FRAMES * FRAME_MS + FRAME_MS / 2);
    char seen[64];
    if (lost) {
        fail(test, "edges lost", "every edge timed");
        return false;
    }
    if (pulseCount + 1 < WINDOW_FRAMES || pulseCount > WINDOW_FRAMES + 1) {
        snprintf(seen, sizeof seen, "%lu pulses in %lu frames at %s", pulseCount, WINDOW_FRAMES,
                 where);
        fail(test, seen, "one pulse each frame");
        return false;
    }
    for (uint32_t i = 0; i < pulseCount; ++i) {
        if (differenceNs(pulses[i].widthNs, wantedNs) > STEP_NS) {
            snprintf(seen, sizeof seen, "pulse at %s", where);
            failWidth(test, seen, pulses[i].widthNs, wantedNs);
            return false;
        }
        const uint32_t periodNs =
            i ? cyclesToNs(pulses[i].fallCycles - pulses[i - 1].fallCycles) : FRAME_NS;
        if (differenceNs(periodNs, FRAME_NS) > FRAME_TOLERANCE_NS) {
            snprintf(seen, sizeof seen, "frame of %lu ns at %s", periodNs, where);
            fail(test, seen, "20000000 ns");
            return false;
        }
    }
    return true;
}

// first-pulse: every pulse from before begin() until the servo stands at its start position lies
// in the SER0049's range, give or take one step of the pulse.
static void testFirstPulse() {
    const char* test = "first-pulse";
    run(test);
    restart();
    collect(FRAME_MS * 10);
    config.clear();
    if (!firstServo.begin()) {
        fail(test, "begin() false", "begin() true");
        return;
    }
    collect(firstServo.travelMs() + SETTLE_MS);
    if (wireMissing(test)) {
        return;
    }
    if (pulseCount == 0) {
        fail(test, "no pulse", "pulses at the start position");
        return;
    }
    for (uint32_t i = 0; i < pulseCount; ++i) {
        const uint32_t w = pulses[i].widthNs;
        if (w + STEP_NS < PULSE_0_NS || w > PULSE_180_NS + STEP_NS) {
            char seen[48];
            snprintf(seen, sizeof seen, "pulse %lu of %lu.%02lu us", i + 1, w / 1000,
                     w % 1000 / 10);
            fail(test, seen, "500 to 2500 us +- 4.88 us");
            return;
        }
    }
    pass(test);
}

// pulses: with both angles in the config, each end position gets the pulse of its angle.
static void testPulses() {
    const char* test = "pulses";
    run(test);
    config.clear();
    config.set(ConfigKey::kServoStart, SET_START_DEG);
    config.set(ConfigKey::kServoEnd, SET_END_DEG);
    if (!configuredServo.begin()) {
        fail(test, "begin() false", "begin() true");
        return;
    }
    collect(SETTLE_MS);
    if (wireMissing(test)) {
        return;
    }
    moveAndSettle(configuredServo, ServoDriver::Position::End);
    if (!checkSteady(test, "the end", widthOfNs(SET_END_DEG))) {
        return;
    }
    moveAndSettle(configuredServo, ServoDriver::Position::Start);
    if (!checkSteady(test, "the start", widthOfNs(SET_START_DEG))) {
        return;
    }
    pass(test);
}

// ends: a person watches the seesaw over several moves between the two end positions.
static void testEnds() {
    const char* test = "ends";
    run(test);
    for (uint32_t i = 0; i < ENDS_ROUNDS; ++i) {
        moveAndSettle(configuredServo, ServoDriver::Position::End);
        moveAndSettle(configuredServo, ServoDriver::Position::Start);
    }
    while (Serial.available()) {
        Serial.read();
    }
    Serial.printf("ASK servo.%s Did the seesaw reach each extreme without the servo straining or "
                  "humming against it?\n",
                  test);
    const uint32_t t0 = millis();
    while (millis() - t0 < ASK_TIMEOUT_MS) {
        const int c = Serial.read();
        if (c == 'y') {
            pass(test);
            return;
        }
        if (c == 'n') {
            fail(test, "n", "y");
            return;
        }
    }
    skip(test, "no answer");
}

// defaults: with neither angle in the config, the end positions get the pulses of the defaults.
static void testDefaults() {
    const char* test = "defaults";
    run(test);
    config.clear();
    if (!defaultServo.begin()) {
        fail(test, "begin() false", "begin() true");
        return;
    }
    collect(SETTLE_MS);
    if (wireMissing(test)) {
        return;
    }
    moveAndSettle(defaultServo, ServoDriver::Position::End);
    if (!checkSteady(test, "the end", widthOfNs(DEFAULT_END_DEG))) {
        return;
    }
    moveAndSettle(defaultServo, ServoDriver::Position::Start);
    if (!checkSteady(test, "the start", widthOfNs(DEFAULT_START_DEG))) {
        return;
    }
    pass(test);
}

// One speed: the pulse walks from the start to the end position in the frames the speed asks for,
// never back, and stops on the end position's pulse.
static bool checkSpeed(const char* test, ServoDriver& servo, int32_t speedDegS) {
    config.clear();
    config.set(ConfigKey::kServoSpeed, speedDegS);
    if (!servo.begin()) {
        fail(test, "begin() false", "begin() true");
        return false;
    }
    collect(SETTLE_MS);
    restart();
    servo.moveTo(ServoDriver::Position::End);
    collect(servo.travelMs() + SETTLE_MS);
    char seen[64];
    char wanted[48];
    if (lost || pulseCount < 2) {
        fail(test, "edges lost or no pulse", "every pulse timed");
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
            fail(test, seen, "a pulse that only grows");
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
        fail(test, seen, wanted);
        return false;
    }
    const uint32_t endNs = widthOfNs(DEFAULT_END_DEG);
    const uint32_t lastNs = pulses[pulseCount - 1].widthNs;
    if (differenceNs(lastNs, endNs) > STEP_NS) {
        snprintf(seen, sizeof seen, "last pulse at %ld deg/s", speedDegS);
        failWidth(test, seen, lastNs, endNs);
        return false;
    }
    return true;
}

// speed: below and above one degree per frame.
static void testSpeed() {
    const char* test = "speed";
    run(test);
    if (wireMissing(test)) {
        return;
    }
    if (checkSpeed(test, slowServo, SLOW_DEG_S) && checkSpeed(test, fastServo, FAST_DEG_S)) {
        pass(test);
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial) {
    }
    delay(200);
    ARM_DEMCR |= ARM_DEMCR_TRCENA;
    ARM_DWT_CTRL |= ARM_DWT_CTRL_CYCCNTENA;
    pinMode(CAPTURE_PIN, INPUT_PULLDOWN);
    attachInterrupt(CAPTURE_PIN, edgeIsr, CHANGE);

    testFirstPulse();
    testPulses();
    testEnds();
    testDefaults();
    testSpeed();
    Serial.printf("END %lu %lu %lu\n", passed, failed, skipped);
}

void loop() {}
