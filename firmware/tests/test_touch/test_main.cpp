// Tests on the computer whether the touch driver turns the touch controller's reads into the
// picture's positions and the touch events. A fake I²C bus stands in for the bus on Wire2, and a
// test ends each read with the five bytes it wants. A fake IntervalTimer stands in for the driver
// tick's, and the tests step it one tick at a time.
//
//   tick         a found controller is read once every 20 ms
//   orientation  the panel's portrait readings come out in the picture's landscape, mapped
//                between the readings measured at the picture's edges
//   events       TouchDown, TouchMoved, TouchHeld and TouchUp come as touch.md describes them
//   failed-read  failed reads report the controller failed after 100 ms, and release the finger
//   absent       a controller that does not answer at begin() is never read and never fails
//   calibration  other readings at the edges move the mapping, a reading above the range counts
//                as its top, and an axis whose readings lie too close keeps its defaults
//
// Run: pio test -e computer, which runs in CI as well.

#include "drivers/touch.h"
#include <unity.h>

constexpr uint32_t TICKS_PER_READ = Touch::TICK_US / DriverTick::PERIOD_US;
constexpr uint32_t READY_MS = 1000;   // later than the controller's first answer
constexpr uint32_t READ_US = Touch::TICK_US;
constexpr uint32_t US_PER_MS = 1000;
constexpr uint32_t T0_US = 5000000;   // the time of the first read
constexpr uint8_t ONE_TOUCH = 1;
constexpr uint8_t NO_TOUCH = 0;
constexpr uint8_t BYTE_BITS = 8;
constexpr uint8_t EVENT_FLAG = 0x80;    // FT6x06: P1_XH's upper bits hold the event flag
constexpr uint32_t NO_ANSWER = 0x400;   // RM LPI2C, MSR: NDF
constexpr uint32_t EVENTS = 16;
constexpr uint32_t READS = 40;   // tick: the reads stepped

// a touch driver on a queue, a driver tick and a bus that only this test uses
struct Rig {
    io::I2cBus bus;
    EventQueue queue;
    DriverTick tick;
    Touch touch{bus, queue, tick};
    DriverEvent out[EVENTS];

    // Begins the driver with the controller answering or not.
    void begin(bool answers) {
        bus.waitErrors = answers ? 0 : NO_ANSWER;
        TEST_ASSERT_TRUE(touch.begin());
    }

    // Ends one read at timeUs with a touch count and a position in the panel's portrait.
    void read(uint32_t timeUs, uint8_t touches, uint16_t panelX = 0, uint16_t panelY = 0) {
        GPT2_CNT = timeUs;
        const uint8_t bytes[] = {touches, uint8_t(EVENT_FLAG | panelX >> BYTE_BITS),
                                 uint8_t(panelX), uint8_t(panelY >> BYTE_BITS), uint8_t(panelY)};
        bus.end(*bus.reading, bytes, 0);
    }

    // Ends one read at timeUs with error flags.
    void fail(uint32_t timeUs, uint32_t errors) {
        GPT2_CNT = timeUs;
        bus.end(*bus.reading, nullptr, errors);
    }

    uint32_t events() { return queue.read(out, EVENTS); }
};

void setUp() {
    fakeTimers = FakeTimers{};
    fakeMillis = READY_MS;
    GPT2_CNT = 0;
}

void tearDown() {}

static void assertEvent(const DriverEvent& e, DriverEventType type, uint16_t x, uint16_t y,
                        uint32_t timeUs) {
    TEST_ASSERT_EQUAL_UINT16(uint16_t(type), uint16_t(e.type));
    TEST_ASSERT_EQUAL_UINT8(0, e.source);
    TEST_ASSERT_EQUAL_UINT32(timeUs, e.time);
    TEST_ASSERT_EQUAL_HEX32(uint32_t(y) << 16 | x, e.payload);
}

// tick: a found controller gets one read requested every TICK_US.
static void testTick() {
    Rig rig;
    rig.begin(true);
    TEST_ASSERT_TRUE(rig.touch.present());
    TEST_ASSERT_NOT_NULL(rig.bus.reading);
    for (uint32_t i = 0; i < READS * TICKS_PER_READ; ++i) {
        fakeTimers.running->fire();
    }
    TEST_ASSERT_EQUAL_UINT32(READS, rig.bus.reading->requests);
}

// orientation: the picture's x follows the panel's y backward from 327 at the left edge to 1 at the
// right edge, and its y follows the panel's x from 3 at the top edge to 254 at the bottom edge. A
// reading beyond an edge lands on that edge.
static void testOrientation() {
    struct Case {
        uint16_t panelX, panelY, x, y;
    };
    const Case cases[] = {{3, 327, 0, 0},    {254, 1, 319, 239}, {129, 164, 160, 120},
                          {3, 1, 319, 0},    {254, 327, 0, 239}, {0, 400, 0, 0},
                          {300, 0, 319, 239}};
    Rig rig;
    rig.begin(true);
    uint32_t t = T0_US;
    for (const Case& c : cases) {
        rig.read(t, ONE_TOUCH, c.panelX, c.panelY);
        const Finger f = rig.touch.finger();
        TEST_ASSERT_TRUE(f.down);
        TEST_ASSERT_EQUAL_UINT16(c.x, f.x);
        TEST_ASSERT_EQUAL_UINT16(c.y, f.y);
        TEST_ASSERT_EQUAL_UINT32(1, rig.events());
        assertEvent(rig.out[0], DriverEventType::TOUCH_DOWN, c.x, c.y, t);
        t += READ_US;
        rig.read(t, NO_TOUCH);
        TEST_ASSERT_EQUAL_UINT32(1, rig.events());
        assertEvent(rig.out[0], DriverEventType::TOUCH_UP, c.x, c.y, t);
        t += READ_US;
    }
}

// events: a drag gives TouchMoved for each step of SLOP_PX and no TouchHeld; a finger that rests
// within SLOP_PX gives one TouchHeld after HOLD_MS; TouchUp carries the last position.
static void testEvents() {
    Rig rig;
    rig.begin(true);
    uint32_t t = T0_US;
    // panel x 100 is picture y 92, panel y 200 is picture x 124
    rig.read(t, ONE_TOUCH, 100, 200);
    TEST_ASSERT_EQUAL_UINT32(1, rig.events());
    assertEvent(rig.out[0], DriverEventType::TOUCH_DOWN, 124, 92, t);

    // Panel y 191 is picture x 133, less than SLOP_PX from the position reported, and gives
    // nothing.
    t += READ_US;
    rig.read(t, ONE_TOUCH, 100, 191);
    TEST_ASSERT_EQUAL_UINT32(0, rig.events());
    // Panel y 190 is picture x 134, SLOP_PX from it, and gives TouchMoved.
    t += READ_US;
    rig.read(t, ONE_TOUCH, 100, 190);
    TEST_ASSERT_EQUAL_UINT32(1, rig.events());
    assertEvent(rig.out[0], DriverEventType::TOUCH_MOVED, 134, 92, t);
    // A finger that strayed brings no TouchHeld, however long it rests.
    const uint32_t restUntil = t + 2 * Touch::HOLD_MS * US_PER_MS;
    while (t < restUntil) {
        t += READ_US;
        rig.read(t, ONE_TOUCH, 100, 190);
    }
    TEST_ASSERT_EQUAL_UINT32(0, rig.events());
    t += READ_US;
    rig.read(t, NO_TOUCH);
    TEST_ASSERT_EQUAL_UINT32(1, rig.events());
    assertEvent(rig.out[0], DriverEventType::TOUCH_UP, 134, 92, t);
    TEST_ASSERT_FALSE(rig.touch.finger().down);

    // A finger that rests within SLOP_PX of where it came down gives one TouchHeld.
    t += READ_US;
    const uint32_t downUs = t;
    rig.read(t, ONE_TOUCH, 50, 50);
    TEST_ASSERT_EQUAL_UINT32(1, rig.events());
    uint32_t held = 0;
    uint32_t heldUs = 0;
    for (uint32_t i = 0; t - downUs < 3 * Touch::HOLD_MS * US_PER_MS; ++i) {
        t += READ_US;
        rig.read(t, ONE_TOUCH, uint16_t(50 + i % 3), uint16_t(50 + (Touch::SLOP_PX - 1) * (i % 2)));
        const uint32_t n = rig.events();
        for (uint32_t k = 0; k < n; ++k) {
            TEST_ASSERT_EQUAL_UINT16(uint16_t(DriverEventType::TOUCH_HELD),
                                     uint16_t(rig.out[k].type));
            ++held;
            heldUs = rig.out[k].time;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(1, held);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(Touch::HOLD_MS * US_PER_MS, heldUs - downUs);
    TEST_ASSERT_LESS_THAN_UINT32(Touch::HOLD_MS * US_PER_MS + READ_US, heldUs - downUs);
}

// failed-read: reads that fail for less than FAIL_MS change nothing; past it the controller counts
// as failed with the flags of the last failed read, a finger on the screen leaves it with
// TouchUp, and the first working read recovers it.
static void testFailedRead() {
    Rig rig;
    rig.begin(true);
    uint32_t t = T0_US;
    rig.read(t, ONE_TOUCH, 10, 20);
    TEST_ASSERT_EQUAL_UINT32(1, rig.events());
    const uint32_t lastGood = t;
    while (t + READ_US - lastGood < Touch::FAIL_MS * US_PER_MS) {
        t += READ_US;
        rig.fail(t, NO_ANSWER);
        TEST_ASSERT_EQUAL_UINT16(0, rig.touch.failed().parts);
        TEST_ASSERT_TRUE(rig.touch.finger().down);
    }
    TEST_ASSERT_EQUAL_UINT32(0, rig.events());
    t += READ_US;
    rig.fail(t, NO_ANSWER);
    const Fault f = rig.touch.failed();
    TEST_ASSERT_EQUAL_UINT16(1, f.parts);
    TEST_ASSERT_EQUAL_HEX32(NO_ANSWER, f.code);
    TEST_ASSERT_FALSE(rig.touch.finger().down);
    TEST_ASSERT_EQUAL_UINT32(1, rig.events());
    assertEvent(rig.out[0], DriverEventType::TOUCH_UP, 300, 7, t);
    // Further failed reads bring no second TouchUp.
    t += READ_US;
    rig.fail(t, NO_ANSWER);
    TEST_ASSERT_EQUAL_UINT32(0, rig.events());

    t += READ_US;
    rig.read(t, NO_TOUCH);
    TEST_ASSERT_EQUAL_UINT16(0, rig.touch.failed().parts);
    TEST_ASSERT_EQUAL_UINT32(0, rig.events());
}

// absent: no answer at begin() leaves present() false, requests no read and reports no fault.
static void testAbsent() {
    Rig rig;
    rig.begin(false);
    TEST_ASSERT_FALSE(rig.touch.present());
    for (uint32_t i = 0; i < READS * TICKS_PER_READ; ++i) {
        fakeTimers.running->fire();
    }
    TEST_ASSERT_EQUAL_UINT32(0, rig.bus.reading->requests);
    GPT2_CNT = T0_US;
    TEST_ASSERT_EQUAL_UINT16(0, rig.touch.failed().parts);
    TEST_ASSERT_FALSE(rig.touch.finger().down);
    TEST_ASSERT_EQUAL_UINT32(0, rig.events());
}

// calibration: configure() moves the mapping, holds a reading at RAW_MAX and keeps the defaults of
// an axis whose readings lie less than SPAN_MIN apart.
static void testCalibration() {
    {
        Rig rig;
        rig.touch.configure({400, 50, 10, 300});
        rig.begin(true);
        rig.read(T0_US, ONE_TOUCH, 10, 400);
        TEST_ASSERT_EQUAL_UINT16(0, rig.touch.finger().x);
        TEST_ASSERT_EQUAL_UINT16(0, rig.touch.finger().y);
        rig.read(T0_US + READ_US, ONE_TOUCH, 300, 50);
        TEST_ASSERT_EQUAL_UINT16(319, rig.touch.finger().x);
        TEST_ASSERT_EQUAL_UINT16(239, rig.touch.finger().y);
    }
    {
        Rig past;
        Rig nearer;
        past.touch.configure(
            {Touch::RAW_MAX + 1000, Touch::RAW_RIGHT, Touch::RAW_TOP, Touch::RAW_MAX + 1});
        nearer.touch.configure({Touch::RAW_MAX, Touch::RAW_RIGHT, Touch::RAW_TOP, Touch::RAW_MAX});
        TEST_ASSERT_EQUAL_UINT16(Touch::RAW_MAX, past.touch.settings().rawLeft);
        TEST_ASSERT_EQUAL_UINT16(Touch::RAW_MAX, past.touch.settings().rawBottom);
        past.begin(true);
        nearer.begin(true);
        for (const uint16_t raw : {uint16_t(0), uint16_t(2000), Touch::RAW_MAX}) {
            past.read(T0_US, ONE_TOUCH, raw, raw);
            nearer.read(T0_US, ONE_TOUCH, raw, raw);
            TEST_ASSERT_EQUAL_UINT16(nearer.touch.finger().x, past.touch.finger().x);
            TEST_ASSERT_EQUAL_UINT16(nearer.touch.finger().y, past.touch.finger().y);
        }
    }
    {
        Rig rig;
        const uint16_t close = Touch::SPAN_MIN - 1;
        rig.touch.configure({200, uint16_t(200 + close), 100, uint16_t(100 + close)});
        const Touch::Settings held = rig.touch.settings();
        TEST_ASSERT_EQUAL_UINT16(Touch::RAW_LEFT, held.rawLeft);
        TEST_ASSERT_EQUAL_UINT16(Touch::RAW_RIGHT, held.rawRight);
        TEST_ASSERT_EQUAL_UINT16(Touch::RAW_TOP, held.rawTop);
        TEST_ASSERT_EQUAL_UINT16(Touch::RAW_BOTTOM, held.rawBottom);
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(testTick);
    RUN_TEST(testOrientation);
    RUN_TEST(testEvents);
    RUN_TEST(testFailedRead);
    RUN_TEST(testAbsent);
    RUN_TEST(testCalibration);
    return UNITY_END();
}
