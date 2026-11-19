// The event-queue program of docs/firmware/testing.md. It tests on the Teensy whether the event
// queue carries events from interrupts to the main loop whole, once and in order.
//
//   from-interrupts  two interrupts of different priorities publish numbered events while the
//                    test task reads them, for ten minutes in a soak run
//
// Wiring: none, the Teensy alone. GPT1 raises the more urgent interrupt and an IntervalTimer on
// the PIT the other, since the PIT's four channels share one interrupt and so one priority. Each
// interrupt numbers its events. The payload carries the number, and the source carries the
// queue and the number's low seven bits as a second copy, so an event written in part shows.

#include <Arduino.h>
#include <IntervalTimer.h>
#include "core/clock.h"
#include "core/event-queue.h"
#include "test-harness.h"

constexpr uint32_t QUICK_RUN_MS = 5000;
constexpr uint32_t SOAK_RUN_MS = 600000;   // testing.md, event-queue from-interrupts: ten minutes
constexpr uint32_t QUICK_LIMIT_S = 15;
constexpr uint32_t SOAK_LIMIT_S = 615;

constexpr uint8_t QUEUES = 2;
constexpr uint8_t URGENT = 0;   // the queue GPT1's interrupt writes
constexpr uint8_t CALM = 1;     // the queue the PIT's interrupt writes
// general-design.md, Interrupts: the driver tick's row and the DMA refills' row, so the urgent
// interrupt preempts the calm one in the middle of its publish
constexpr uint8_t URGENT_PRIORITY = 96;
constexpr uint8_t CALM_PRIORITY = 128;
// Periods that share no factor, so the two interrupts meet at every phase over the run.
constexpr uint32_t URGENT_PERIOD_US = 10;
constexpr uint32_t CALM_PERIOD_US = 13;
// Each queue holds 10 ms of its events, far more than the test task ever stays away.
constexpr uint32_t DEPTH = 1024;
constexpr uint32_t READ_MAX = 64;         // the events one read() takes at most
constexpr uint32_t PROGRESS_MS = 60000;   // a soak run prints its progress once a minute
constexpr uint32_t US_PER_MS = 1000;
constexpr uint32_t MS_PER_S = 1000;

// i.MX RT1060 reference manual, Rev. 3, General Purpose Timer: CLKSRC 1 is the peripheral clock,
// the 24 MHz crystal in the Teensy core's start-up, which a prescaler of 24 turns into
// microseconds. ENMOD clears the counter as GPT1 starts, and without FRR the counter restarts at
// the compare value, so OCR1 sets the period.
constexpr uint32_t GPT_CLKSRC_PERIPHERAL = 1;
constexpr uint32_t GPT_PRESCALE = 24;
constexpr uint32_t GPT_STATUS_ALL = 0x3F;   // GPT_SR: writing a 1 clears each of its six flags

constexpr uint8_t NUMBER_BITS = 7;   // the number's low bits the source carries beside the queue
constexpr uint8_t NUMBER_MASK = (1u << NUMBER_BITS) - 1;

static DMAMEM DriverEvent storage[QUEUES][DEPTH];
static EventQueue queue;
static EventQueue::Producer* producers[QUEUES];
static volatile uint32_t numbered[QUEUES];   // the events each interrupt has numbered so far
static IntervalTimer pit;

// Gets the source of event number of queue q: the queue in bit 0, the number's low bits above.
static uint8_t sourceOf(uint8_t q, uint32_t number) {
    return uint8_t(q | (number & NUMBER_MASK) << 1);
}

// Publishes the interrupt's next numbered event.
static void publishNext(uint8_t q) {
    const uint32_t number = numbered[q];
    producers[q]->publish(
        DriverEvent{nowUs(), DriverEventType::DEVICE_FAILED, sourceOf(q, number), number});
    numbered[q] = number + 1;
}

static void urgentIsr() {
    GPT1_SR = GPT_SR_OF1;
    publishNext(URGENT);
    // The flag's clear has to reach GPT1 before the interrupt returns, or it fires twice.
    asm volatile("dsb");
}

static void calmIsr() {
    publishNext(CALM);
}

static void startGpt1() {
    CCM_CCGR1 |= CCM_CCGR1_GPT1_BUS(CCM_CCGR_ON) | CCM_CCGR1_GPT1_SERIAL(CCM_CCGR_ON);
    GPT1_CR = 0;
    GPT1_PR = GPT_PRESCALE - 1;
    GPT1_OCR1 = URGENT_PERIOD_US - 1;
    GPT1_SR = GPT_STATUS_ALL;
    GPT1_IR = GPT_IR_OF1IE;
    attachInterruptVector(IRQ_GPT1, urgentIsr);
    NVIC_SET_PRIORITY(IRQ_GPT1, URGENT_PRIORITY);
    NVIC_ENABLE_IRQ(IRQ_GPT1);
    GPT1_CR = GPT_CR_EN | GPT_CR_ENMOD | GPT_CR_CLKSRC(GPT_CLKSRC_PERIPHERAL);
}

static void stopGpt1() {
    GPT1_CR = 0;
    NVIC_DISABLE_IRQ(IRQ_GPT1);
}

// what the test task has seen of one queue
struct Seen {
    uint32_t next;     // the number the queue's next event has to carry
    uint32_t lastUs;   // the time of its last event
};

// Checks one event against what its queue sent before. Reports a failure and returns false where
// the event is not whole, comes twice or comes out of its queue's order.
static bool checkEvent(const DriverEvent& e, Seen* seen) {
    char text[96];
    char wanted[48];
    const uint8_t q = e.source & 1;
    Seen& s = seen[q];
    if (e.type != DriverEventType::DEVICE_FAILED || e.source != sourceOf(q, e.payload)) {
        snprintf(text, sizeof text, "queue %u event %lu with source %u type %u", q, e.payload,
                 e.source, unsigned(e.type));
        test::fail(text, "a whole event");
        return false;
    }
    if (e.payload != s.next) {
        snprintf(text, sizeof text, "queue %u event %lu", q, e.payload);
        snprintf(wanted, sizeof wanted, "event %lu", s.next);
        test::fail(text, wanted);
        return false;
    }
    if (s.next > 0 && before(e.time, s.lastUs)) {
        snprintf(text, sizeof text, "queue %u event %lu at %lu us after %lu us", q, e.payload,
                 e.time, s.lastUs);
        test::fail(text, "the queue's times in order");
        return false;
    }
    s.next = s.next + 1;
    s.lastUs = e.time;
    return true;
}

// Reads every waiting event and checks each. Returns false once a check has failed.
static bool drain(Seen* seen) {
    DriverEvent out[READ_MAX];
    for (uint32_t n = queue.read(out, READ_MAX); n > 0; n = queue.read(out, READ_MAX)) {
        for (uint32_t i = 0; i < n; ++i) {
            if (!checkEvent(out[i], seen)) {
                return false;
            }
        }
    }
    return true;
}

static void testFromInterrupts() {
    for (uint8_t q = 0; q < QUEUES; ++q) {
        if (!queue.attach(storage[q], DEPTH, producers[q])) {
            test::fail("attach() false", "both queues attached");
            return;
        }
        numbered[q] = 0;
    }
    Seen seen[QUEUES] = {};
    pit.priority(CALM_PRIORITY);
    if (!pit.begin(calmIsr, CALM_PERIOD_US)) {
        test::fail("no IntervalTimer free", "the PIT's interrupt running");
        return;
    }
    startGpt1();
    const uint32_t startUs = nowUs();
    const uint32_t runMs = test::durationMs(QUICK_RUN_MS, SOAK_RUN_MS);
    bool whole = true;
    uint32_t reportedUs = startUs;
    uint32_t lagMax = 0;
    while (whole && !elapsedMs(startUs, runMs)) {
        // The events waiting before a drain, which the queue's depth has to hold.
        for (uint8_t q = 0; q < QUEUES; ++q) {
            const uint32_t lag = numbered[q] - seen[q].next;
            lagMax = lag > lagMax ? lag : lagMax;
        }
        whole = drain(seen);
        if (elapsedMs(reportedUs, PROGRESS_MS)) {
            reportedUs += PROGRESS_MS * US_PER_MS;
            Serial.printf("%lu s: %lu and %lu events read, at most %lu waiting\n",
                          (reportedUs - startUs) / (US_PER_MS * MS_PER_S), seen[URGENT].next,
                          seen[CALM].next, lagMax);
        }
    }
    stopGpt1();
    pit.end();
    if (!whole || !drain(seen)) {
        return;
    }
    for (uint8_t q = 0; q < QUEUES; ++q) {
        char text[64];
        char wanted[64];
        if (producers[q]->dropped() != 0) {
            snprintf(text, sizeof text, "queue %u dropped %lu", q, producers[q]->dropped());
            test::fail(text, "dropped 0");
            return;
        }
        if (seen[q].next != numbered[q]) {
            snprintf(text, sizeof text, "queue %u read %lu", q, seen[q].next);
            snprintf(wanted, sizeof wanted, "%lu published", numbered[q]);
            test::fail(text, wanted);
            return;
        }
    }
    Serial.printf("%lu events from GPT1 at %u, %lu from the PIT at %u, all read\n",
                  numbered[URGENT], URGENT_PRIORITY, numbered[CALM], CALM_PRIORITY);
    test::pass();
}

static const Test TESTS[] = {
    {"event-queue.from-interrupts", RunsOn::TEENSY, QUICK_LIMIT_S, SOAK_LIMIT_S, testFromInterrupts,
     nullptr},
};

extern const Program EVENT_QUEUE = {TESTS, sizeof TESTS / sizeof TESTS[0]};
