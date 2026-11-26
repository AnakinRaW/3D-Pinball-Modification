// Break beam test for the Teensy 4.1, wired as docs/parts/break-beam/design.md draws it: the
// receiver from pin 31 to GND, R3 with 100 kOhm from 3V3 to pin 31, the emitter behind R2 with
// 220 Ohm from VIN to GND.
//
// The pin is read the way firmware/src/drivers/break-beam.cpp reads it: an input without the
// Teensy's pull-up, with hysteresis, and an interrupt on both edges. The interrupt notes each
// edge with its time in a ring buffer, and the loop prints it. HIGH means blocked.
//
// Each line gives the time since the start, the new state and how long the state before it held.
// A break after the beam was clear for at least 10 ms counts as a crossing, as in the driver, and
// a shorter clear is marked as flicker. A beam broken for 5 s is reported as failed, once.
// Without edges a status line follows every 2 s.
//
// It starts once the serial monitor is open.

#include <Arduino.h>

constexpr uint8_t PIN = 31;             // pin-assignment.md: Receiver output of the ball drain gate
constexpr uint32_t DEAD_US = 10000;     // the driver's 10 ms: a shorter clear is the same crossing
constexpr uint32_t FAIL_MS = 5000;      // the driver's 5 s: a beam broken this long has failed
constexpr uint32_t STATUS_MS = 2000;    // a status line after this long without an edge
constexpr uint32_t RING = 256;          // edges the loop may fall behind by

struct Edge {
    uint32_t us;
    uint8_t level;
};

static volatile Edge ring[RING];
static volatile uint32_t head = 0;      // written by the interrupt
static volatile uint32_t lost = 0;      // edges the full ring dropped
static uint32_t tail = 0;               // read by the loop

static void edge() {
    const uint32_t now = micros();
    const uint32_t next = head + 1;
    if (next - tail > RING) {
        lost = lost + 1;
        return;
    }
    ring[head % RING].us = now;
    ring[head % RING].level = digitalReadFast(PIN);
    head = next;
}

static const char* name(uint8_t level) { return level ? "BLOCKED" : "CLEAR  "; }

static void printTime(uint32_t us) { Serial.printf("%9.6f s", us / 1e6); }

void setup() {
    Serial.begin(115200);
    while (!Serial) {
    }

    pinMode(PIN, INPUT);                                // R3 pulls the pin up
    *portControlRegister(PIN) |= IOMUXC_PAD_HYS;        // hysteresis, which INPUT leaves off
    attachInterrupt(digitalPinToInterrupt(PIN), edge, CHANGE);

    Serial.println("break beam test: pin 31, HIGH = blocked, LOW = clear");
    Serial.printf("at the start the beam is %s\n", name(digitalReadFast(PIN)));
}

void loop() {
    static uint8_t level = digitalReadFast(PIN);
    static uint32_t sinceUs = micros();
    static uint32_t clearedUs = micros() - DEAD_US;
    static uint32_t crossings = 0;
    static uint32_t flickers = 0;
    static uint32_t reportedLost = 0;
    static uint32_t lastPrintMs = millis();
    static bool failed = false;

    while (tail != head) {
        const uint32_t us = ring[tail % RING].us;
        const uint8_t now = ring[tail % RING].level;
        tail = tail + 1;
        if (now == level) {
            // two edges inside one read: the pin went and came back before the interrupt read it
            Serial.print("  ");
            printTime(us);
            Serial.printf("  glitch, still %s\n", name(now));
            continue;
        }

        const float heldMs = (us - sinceUs) / 1000.0f;
        Serial.print("  ");
        printTime(us);
        if (now) {
            const bool crossing = us - clearedUs >= DEAD_US;
            if (crossing) {
                crossings = crossings + 1;
            } else {
                flickers = flickers + 1;
            }
            Serial.printf("  %s after %10.3f ms clear   %s %lu\n", name(now), heldMs,
                          crossing ? "crossing" : "flicker ", crossing ? crossings : flickers);
        } else {
            clearedUs = us;
            failed = false;
            Serial.printf("  %s after %10.3f ms blocked\n", name(now), heldMs);
        }
        level = now;
        sinceUs = us;
        lastPrintMs = millis();
    }

    if (lost != reportedLost) {
        Serial.printf("  %lu edges lost, the ring was full\n", lost - reportedLost);
        reportedLost = lost;
    }

    if (level && !failed && micros() - sinceUs >= FAIL_MS * 1000) {
        failed = true;
        Serial.println("  FAILED: blocked for 5 s (no light at the receiver)");
    }

    if (millis() - lastPrintMs >= STATUS_MS) {
        lastPrintMs = millis();
        Serial.printf("status: %s for %.1f s, pin reads %d, crossings %lu, flickers %lu\n",
                      name(level), (micros() - sinceUs) / 1e6, digitalReadFast(PIN), crossings,
                      flickers);
    }
}
