// Tests on the computer whether the event queue serves the events of several queues as one
// stream, oldest first, and keeps every event whole.
//
//   oldest-first  read() gives the events of three queues once each, oldest first across them
//   across-wrap   the order holds with times on both sides of the wrap of nowUs()
//   full          a publish into a full queue returns false and counts, the queued events stay
//   payload       payload<TAG>() gives back what publish<TAG>() packed, for every type
//   refused       attach() turns down a bad depth and a queue past the last, and the queue it
//                 points at drops every event
//
// Run: pio test -e computer, which runs in CI as well.

#include <event-queue.h>
#include <unity.h>

constexpr uint32_t DEPTH = 8;
constexpr uint8_t QUEUES = 3;
constexpr uint8_t PER_QUEUE = 3;
constexpr uint32_t EVENTS = QUEUES * PER_QUEUE;
// The times each queue publishes, each queue in time order. Published in turn, one event of each
// queue after the other, so the order of publishing differs from the order of the times.
constexpr uint32_t TIMES_US[QUEUES][PER_QUEUE] = {{10, 11, 40}, {5, 30, 31}, {20, 21, 22}};
// The times sorted, the order read() has to give.
constexpr uint32_t SORTED_US[EVENTS] = {5, 10, 11, 20, 21, 22, 30, 31, 40};
constexpr uint32_t READ_CHUNK = 4;            // read() is asked for fewer events than wait, twice
constexpr uint32_t WRAP_OFFSET_US = 0 - 20;   // moves the times so they lie across the wrap
constexpr uint32_t FULL_DEPTH = 4;
constexpr uint32_t CODE = 0xDEADBEEF;   // an error code that uses every byte of the payload
constexpr uint8_t SOURCE = 0xA5;
constexpr uint32_t REFUSED_DEPTH = 2;
constexpr uint32_t ODD_DEPTH = 3;   // a depth that is no power of two

void setUp() {}

void tearDown() {}

// A payload that names the queue and the event's place in it, so read() shows which event it gave.
static uint32_t tag(uint8_t queue, uint8_t k) {
    return uint32_t(queue) << 8 | k;
}

// Publishes TIMES_US moved by offsetUs into three queues in turn, reads them back in chunks and
// checks that every event comes once, oldest first.
static void publishAndRead(uint32_t offsetUs) {
    EventQueue queue;
    DriverEvent storage[QUEUES][DEPTH];
    EventQueue::Producer* producers[QUEUES];
    for (uint8_t q = 0; q < QUEUES; ++q) {
        TEST_ASSERT_TRUE(queue.attach(storage[q], DEPTH, producers[q]));
    }
    for (uint8_t k = 0; k < PER_QUEUE; ++k) {
        for (uint8_t q = 0; q < QUEUES; ++q) {
            const DriverEvent e{TIMES_US[q][k] + offsetUs, DriverEventType::DEVICE_FAILED, q,
                                tag(q, k)};
            TEST_ASSERT_TRUE(producers[q]->publish(e));
        }
    }
    DriverEvent out[EVENTS];
    uint32_t n = 0;
    for (uint32_t got = queue.read(out, READ_CHUNK); got > 0;
         got = queue.read(out + n, READ_CHUNK)) {
        TEST_ASSERT_LESS_OR_EQUAL_UINT32(READ_CHUNK, got);
        n += got;
        TEST_ASSERT_LESS_OR_EQUAL_UINT32(EVENTS, n);
    }
    TEST_ASSERT_EQUAL_UINT32(EVENTS, n);
    for (uint32_t i = 0; i < EVENTS; ++i) {
        TEST_ASSERT_EQUAL_UINT32(SORTED_US[i] + offsetUs, out[i].time);
        // The event's payload names the queue and place whose time this is.
        const uint8_t q = out[i].source;
        const uint8_t k = uint8_t(out[i].payload & 0xFF);
        TEST_ASSERT_EQUAL_UINT32(tag(q, k), out[i].payload);
        TEST_ASSERT_EQUAL_UINT32(TIMES_US[q][k] + offsetUs, out[i].time);
    }
    TEST_ASSERT_EQUAL_UINT32(0, queue.read(out, EVENTS));
}

// oldest-first: read() gives every event once, oldest first across all queues.
static void testOldestFirst() {
    publishAndRead(0);
}

// across-wrap: the order holds across the wrap of nowUs().
static void testAcrossWrap() {
    publishAndRead(WRAP_OFFSET_US);
}

// full: the publish past the depth returns false and counts in dropped(), and the queued events
// come out unchanged.
static void testFull() {
    EventQueue queue;
    DriverEvent storage[FULL_DEPTH];
    EventQueue::Producer* attached = nullptr;
    TEST_ASSERT_TRUE(queue.attach(storage, FULL_DEPTH, attached));
    EventQueue::Producer& p = *attached;
    for (uint32_t i = 0; i < FULL_DEPTH; ++i) {
        TEST_ASSERT_TRUE(p.publish(DriverEvent{i, DriverEventType::DEVICE_FAILED, SOURCE, i}));
    }
    TEST_ASSERT_EQUAL_UINT32(0, p.dropped());
    TEST_ASSERT_FALSE(
        p.publish(DriverEvent{FULL_DEPTH, DriverEventType::DEVICE_RECOVERED, SOURCE, 0}));
    TEST_ASSERT_EQUAL_UINT32(1, p.dropped());
    DriverEvent out[FULL_DEPTH + 1];
    TEST_ASSERT_EQUAL_UINT32(FULL_DEPTH, queue.read(out, FULL_DEPTH + 1));
    for (uint32_t i = 0; i < FULL_DEPTH; ++i) {
        TEST_ASSERT_EQUAL_UINT32(i, out[i].time);
        TEST_ASSERT_EQUAL_UINT16(uint16_t(DriverEventType::DEVICE_FAILED), uint16_t(out[i].type));
        TEST_ASSERT_EQUAL_UINT8(SOURCE, out[i].source);
        TEST_ASSERT_EQUAL_UINT32(i, out[i].payload);
    }
}

// payload: payload<TAG>() gives every payload back unchanged.
static void testPayload() {
    EventQueue queue;
    DriverEvent storage[DEPTH];
    EventQueue::Producer* attached = nullptr;
    TEST_ASSERT_TRUE(queue.attach(storage, DEPTH, attached));
    EventQueue::Producer& p = *attached;
    TEST_ASSERT_TRUE(p.publish<DriverEventType::DEVICE_FAILED>(1, SOURCE, CODE));
    TEST_ASSERT_TRUE(p.publish<DriverEventType::DEVICE_RECOVERED>(2, SOURCE, NoPayload{}));
    DriverEvent out[2];
    TEST_ASSERT_EQUAL_UINT32(2, queue.read(out, 2));
    TEST_ASSERT_EQUAL_UINT16(uint16_t(DriverEventType::DEVICE_FAILED), uint16_t(out[0].type));
    TEST_ASSERT_EQUAL_UINT8(SOURCE, out[0].source);
    TEST_ASSERT_EQUAL_UINT32(1, out[0].time);
    TEST_ASSERT_EQUAL_HEX32(CODE, payload<DriverEventType::DEVICE_FAILED>(out[0]));
    TEST_ASSERT_EQUAL_UINT16(uint16_t(DriverEventType::DEVICE_RECOVERED), uint16_t(out[1].type));
    TEST_ASSERT_EQUAL_UINT8(SOURCE, out[1].source);
    TEST_ASSERT_EQUAL_UINT32(2, out[1].time);
    // A type without a payload leaves the field 0.
    TEST_ASSERT_EQUAL_HEX32(0, out[1].payload);
    (void)payload<DriverEventType::DEVICE_RECOVERED>(out[1]);
}

// Publishes one event through p and checks that p drops it and that read() finds nothing.
static void checkDrops(EventQueue& queue, EventQueue::Producer* p) {
    TEST_ASSERT_NOT_NULL(p);
    const uint32_t dropped = p->dropped();
    TEST_ASSERT_FALSE(p->publish(DriverEvent{0, DriverEventType::DEVICE_FAILED, SOURCE, 0}));
    TEST_ASSERT_EQUAL_UINT32(dropped + 1, p->dropped());
    DriverEvent out[1];
    TEST_ASSERT_EQUAL_UINT32(0, queue.read(out, 1));
}

// refused: attach() returns false for a depth of no power of two, for no storage and for a queue
// past the last of PRODUCERS, and points at a queue that drops every event.
static void testRefused() {
    EventQueue queue;
    DriverEvent storage[EventQueue::PRODUCERS][REFUSED_DEPTH];
    EventQueue::Producer* p = nullptr;
    TEST_ASSERT_FALSE(queue.attach(storage[0], ODD_DEPTH, p));
    checkDrops(queue, p);
    TEST_ASSERT_FALSE(queue.attach(storage[0], 0, p));
    checkDrops(queue, p);
    TEST_ASSERT_FALSE(queue.attach(nullptr, REFUSED_DEPTH, p));
    checkDrops(queue, p);
    for (uint8_t i = 0; i < EventQueue::PRODUCERS; ++i) {
        TEST_ASSERT_TRUE(queue.attach(storage[i], REFUSED_DEPTH, p));
    }
    TEST_ASSERT_FALSE(queue.attach(storage[0], REFUSED_DEPTH, p));
    checkDrops(queue, p);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(testOldestFirst);
    RUN_TEST(testAcrossWrap);
    RUN_TEST(testFull);
    RUN_TEST(testPayload);
    RUN_TEST(testRefused);
    return UNITY_END();
}
