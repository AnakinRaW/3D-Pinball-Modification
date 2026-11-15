// Servo trial for the Teensy 4.1 with the DFRobot SER0049: ServoDriver as the gist
// firmware/gist/drivers/servo.cpp designs it, on the driver tick, with PWMServoT4 from
// firmware/lib/pwm-servo-t4.
//
// The program starts once the serial monitor is open. It runs begin(), which sends the servo to its
// start position, and then moves three times to the end position and back.
//
// A wire from pin 0 to pin 1 lets the program time every pulse, the first one included, as the
// first-pulse test of docs/firmware/testing.md asks. Without the wire, pin 1 sees no edge, and the
// program reports the pulse width the timer's registers hold instead.

#include <Arduino.h>
#include "servo-driver.h"

// the core's pwm.c: each pin's timer, read here to print the pulse the registers hold
extern const struct pwm_pin_info_struct { uint8_t type, module, channel, muxval; } pwm_pin_info[];

constexpr uint8_t  SERVO_PIN   = 0;          // ServoDriver's kPin in servo-driver.h
constexpr uint8_t  CAPTURE_PIN = 1;          // the wire from the servo pin lands here
constexpr uint32_t EDGES       = 1024;       // edges kept between two reports
constexpr uint32_t SAME_US     = 3;          // pulses this close count as one width
constexpr uint32_t SETTLE_MS   = 1000;       // added to a move's travel time before the report

DriverTick    driverTick;
DeviceMonitor deviceMonitor;
Config        config;

static ServoDriver servo;

// The capture interrupt keeps each edge's cycle count with the pin's level in bit 0.
static volatile uint32_t edges[EDGES];
static volatile uint32_t edgeHead = 0;
static uint32_t          edgeTail = 0;
static bool              captured = false;

static void edgeIsr() {
    const uint32_t now = (ARM_DWT_CYCCNT & ~1u) | digitalReadFast(CAPTURE_PIN);
    edges[edgeHead % EDGES] = now;
    edgeHead = edgeHead + 1;
}

static uint32_t cyclesToUs(uint32_t cycles) { return (cycles + F_CPU_ACTUAL / 2000000) / (F_CPU_ACTUAL / 1000000); }

// Reports every pulse since the last report, consecutive pulses of the same width as one run.
static void reportPulses(const char* label) {
    const uint32_t head = edgeHead;
    Serial.printf("[%s]\n", label);
    if (head - edgeTail > EDGES) {
        Serial.printf("  %lu edges lost\n", head - edgeTail - EDGES);
        edgeTail = head - EDGES;
    }
    uint32_t rise = 0, lastRise = 0, pulses = 0, runUs = 0, runCount = 0;
    uint32_t minPeriodUs = UINT32_MAX, maxPeriodUs = 0;
    bool haveRise = false, haveLastRise = false;
    for (; edgeTail != head; ++edgeTail) {
        const uint32_t e = edges[edgeTail % EDGES];
        captured = true;
        if (e & 1) {
            rise = e, haveRise = true;
            if (haveLastRise) {
                const uint32_t p = cyclesToUs(e - lastRise);
                minPeriodUs = min(minPeriodUs, p), maxPeriodUs = max(maxPeriodUs, p);
            }
            lastRise = e, haveLastRise = true;
            continue;
        }
        if (!haveRise) continue;
        haveRise = false;
        const uint32_t us = cyclesToUs(e - rise);
        ++pulses;
        if (us < 490 || us > 2510) Serial.printf("  pulse %lu: %lu us, outside 500 to 2500 us\n", pulses, us);
        if (runCount && (us > runUs + SAME_US || us + SAME_US < runUs)) {
            Serial.printf("  %3lu x %4lu us\n", runCount, runUs);
            runCount = 0;
        }
        if (runCount == 0) runUs = us;
        ++runCount;
    }
    if (runCount) Serial.printf("  %3lu x %4lu us\n", runCount, runUs);
    if (!captured) {
        Serial.println("  no edge on pin 1 (wire from pin 0 not fitted)");
    } else if (pulses) {
        Serial.printf("  %lu pulses, frame %lu to %lu us\n", pulses, minPeriodUs, maxPeriodUs);
    } else {
        Serial.println("  no pulse");
    }
}

// Reports the pulse the servo pin's FlexPWM submodule holds. Channel X is high from VAL0 to VAL1,
// channel A from VAL2 to VAL3 and channel B from VAL4 to VAL5, as the core's flexpwmWrite() sets.
static void reportRegisters(const char* label) {
    static IMXRT_FLEXPWM_t* const modules[] = {&IMXRT_FLEXPWM1, &IMXRT_FLEXPWM2, &IMXRT_FLEXPWM3,
                                               &IMXRT_FLEXPWM4};
    const auto& info = pwm_pin_info[SERVO_PIN];
    IMXRT_FLEXPWM_t& pwm = *modules[info.module >> 4];
    const uint8_t sub = info.module & 0x03;
    const uint32_t val0 = pwm.SM[sub].VAL0, val1 = pwm.SM[sub].VAL1;
    uint32_t on = val0, off = val1, outen = pwm.OUTEN & FLEXPWM_OUTEN_PWMX_EN(1 << sub);
    if (info.channel == 1) {
        on = pwm.SM[sub].VAL2, off = pwm.SM[sub].VAL3;
        outen = pwm.OUTEN & FLEXPWM_OUTEN_PWMA_EN(1 << sub);
    } else if (info.channel == 2) {
        on = pwm.SM[sub].VAL4, off = pwm.SM[sub].VAL5;
        outen = pwm.OUTEN & FLEXPWM_OUTEN_PWMB_EN(1 << sub);
    }
    const uint32_t prescale = 1u << ((pwm.SM[sub].CTRL >> 4) & 7);
    const uint32_t countsPerMs = F_BUS_ACTUAL / prescale / 1000;
    const uint32_t widthUs = on >= off ? 0 : (off - on) * 1000 / countsPerMs;
    Serial.printf("  registers %s: FlexPWM%u.%u %c, VAL0 %lu, VAL1 %lu, on %lu, off %lu, "
                  "prescale %lu, frame %lu us, pulse %lu us, pin mux %lu, output %s\n",
                  label, (info.module >> 4) + 1, sub, "XAB"[info.channel], val0, val1, on, off,
                  prescale, (val1 + 1) * 1000 / countsPerMs, widthUs,
                  *portConfigRegister(SERVO_PIN) & 7, outen ? "on" : "off");
}

static void waitAndReport(uint32_t ms, const char* label) {
    delay(ms);
    reportPulses(label);
}

void setup() {
    Serial.begin(115200);
    while (!Serial) {}
    delay(200);
    Serial.println("\nServo trial: ServoDriver from the gist, PWMServoT4 from firmware/lib, "
                   "SER0049 on pin 0");

    ARM_DEMCR |= ARM_DEMCR_TRCENA;
    ARM_DWT_CTRL |= ARM_DWT_CTRL_CYCCNTENA;
    pinMode(CAPTURE_PIN, INPUT_PULLDOWN);
    attachInterrupt(CAPTURE_PIN, edgeIsr, CHANGE);

    waitAndReport(500, "before begin()");
    reportRegisters("before attach");

    const uint32_t t0 = micros();
    const bool ok = servo.begin();
    const uint32_t beginUs = micros() - t0;
    Serial.printf("begin() %s after %lu us, failed parts 0x%X, travel %lu ms\n",
                  ok ? "true" : "false", beginUs, servo.failed().parts, servo.travelMs());
    reportRegisters("after begin");
    if (!ok) {
        Serial.println("stopped: begin() failed");
        return;
    }

    waitAndReport(SETTLE_MS, "begin(): start position");
    reportRegisters("at start");
    for (int i = 1; i <= 3; ++i) {
        Serial.printf("\nround %d\n", i);
        servo.moveTo(ServoDriver::Position::End);
        waitAndReport(servo.travelMs() + SETTLE_MS, "moveTo(End)");
        reportRegisters("at end");
        servo.moveTo(ServoDriver::Position::Start);
        waitAndReport(servo.travelMs() + SETTLE_MS, "moveTo(Start)");
        reportRegisters("at start");
    }
    Serial.println("\ndone, the servo holds its start position");
}

void loop() {}
