// Tests on the computer whether the controls driver turns the edges of the toggle switch's pin into
// one TOGGLE_SWITCHED per flip and always ends on the position the switch rests in. The fake pin
// keeps what the driver set up, and a test sets its level and calls the function the driver
// attached to it, as the pin interrupt would. A fake IntervalTimer stands in for the driver tick's,
// and the tests step it one tick at a time.
//
//   queue-full     begin() fails while the event queue takes no further queue, with a warning
//   tick-full      begin() fails while the driver tick has no free slot, with a warning
//   at-start       begin() sets the pin up and reports the resting position without an event
//   flip           an edge to a new position gives one TOGGLE_SWITCHED with the time of the edge
//   bounce         edges within 50 ms of a report publish nothing, and a bounce that ends in the
//                  reported position stays silent after the window
//   flip-back      a switch flipped back inside the window gets its resting position from the tick
//   wrap           the window holds across the wrap of nowUs(), and an edge a whole wrap after a
//                  report counts
//   second-driver  a second driver's begin() fails with a warning, and it answers with position 0
//
// One driver holds the pin's interrupt for the whole program, and at-start begins it, so
// queue-full and tick-full run before it and the others after it. A driver refused for the pin
// never reaches the queue or the tick.
//
// Run: pio test -e computer, which runs in CI as well.

#include "drivers/toggle-switch.h"
#include "logging/logger.h"
#include <string>
#include <unity.h>

constexpr uint8_t SWITCH_PIN = 5;        // pin-assignment.md: Toggle switch
constexpr uint8_t CLOSED = LOW;          // controls design: the switch closes the pin to ground
constexpr uint8_t OPEN = HIGH;           // the pull-up holds the open pin HIGH
constexpr uint8_t POSITION_CLOSED = 1;   // controls design: the contact closes in position 1
constexpr uint8_t POSITION_OPEN = 0;
constexpr uint32_t T0_US = 5000000;   // begin(), some time after the start
constexpr uint32_t QUIET_US = ToggleSwitch::QUIET_US;
constexpr uint32_t TICK_US = DriverTick::PERIOD_US;
constexpr uint32_t BOUNCE_US = 300;     // a few edges of a contact's bounce, well inside the window
constexpr uint32_t LONG_US = 1000000;   // a switch resting this long has closed every window
constexpr uint32_t EVENTS = 8;

// A log file whose writes end at once.
struct LogFile : AppendFile {
    std::string content;
    AppendStatus state = AppendStatus::IDLE;

    void write(uint32_t at, const uint8_t* data, size_t n) override {
        content.resize(at);
        content.append(reinterpret_cast<const char*>(data), n);
        state = AppendStatus::SUCCEEDED;
    }

    void rewrite(const uint8_t* data, size_t n) override {
        content.clear();
        write(0, data, n);
    }

    AppendStatus status() const override { return state; }
};

// The firmware's log, begun before the first test. It lives as long as the program, since
// logWarning() keeps writing to the logger that began last.
static LogFile logA;
static LogFile logB;
static uint8_t logBuffer[1024];
static Logger logger(logA, logB, logBuffer, sizeof(logBuffer));

static EventQueue queue;
static DriverTick ticker;
static ToggleSwitch toggle(queue, ticker);
static DriverEvent out[EVENTS];
static IntervalTimer* timer = nullptr;   // the timer ticker started at begin()

// Sets the pin's level at timeUs and calls the interrupt the driver attached.
static void edge(uint32_t timeUs, uint8_t level) {
    GPT2_CNT = timeUs;
    fakePins[SWITCH_PIN].level = level;
    TEST_ASSERT_NOT_NULL(fakePins[SWITCH_PIN].isr);
    fakePins[SWITCH_PIN].isr();
}

// Runs the driver tick once at timeUs.
static void tickAt(uint32_t timeUs) {
    GPT2_CNT = timeUs;
    TEST_ASSERT_NOT_NULL(timer);
    timer->fire();
}

// Runs the driver tick on every tick from fromUs up to and including toUs.
static void ticksUntil(uint32_t fromUs, uint32_t toUs) {
    for (uint32_t t = fromUs; t - fromUs <= toUs - fromUs; t += TICK_US) {
        tickAt(t);
    }
}

// Gets how often the log holds a warning of the controls driver with the text given.
static uint32_t warnings(const char* text) {
    logger.flush();
    logger.update();
    logger.update();
    const std::string line = std::string(" W controls ") + text + "\n";
    uint32_t count = 0;
    for (size_t at = logA.content.find(line); at != std::string::npos;
         at = logA.content.find(line, at + 1)) {
        ++count;
    }
    return count;
}

// Gets the events published since the last call.
static uint32_t taken() {
    return queue.read(out, EVENTS);
}

// Checks that event i is a TOGGLE_SWITCHED from the switch with the time and the position given.
static void assertSwitched(uint32_t i, uint32_t timeUs, uint8_t position) {
    TEST_ASSERT_EQUAL_UINT16(uint16_t(DriverEventType::TOGGLE_SWITCHED), uint16_t(out[i].type));
    TEST_ASSERT_EQUAL_UINT8(ToggleSwitch::SWITCH, out[i].source);
    TEST_ASSERT_EQUAL_UINT32(timeUs, out[i].time);
    TEST_ASSERT_EQUAL_UINT8(position, payload<DriverEventType::TOGGLE_SWITCHED>(out[i]));
}

// Lets the switch rest at timeUs long enough that every window has closed, and drops its events.
// Returns the time it rests until.
static uint32_t rest(uint32_t timeUs) {
    tickAt(timeUs + QUIET_US);
    taken();
    return timeUs + LONG_US;
}

static void testQueueFull() {
    EventQueue full;
    static DriverEvent taken[EventQueue::PRODUCERS];
    EventQueue::Producer* producer = nullptr;
    for (uint8_t i = 0; i < EventQueue::PRODUCERS; ++i) {
        TEST_ASSERT_TRUE(full.attach(&taken[i], 1, producer));
    }
    DriverTick otherTicker;
    ToggleSwitch refused(full, otherTicker);
    TEST_ASSERT_FALSE(refused.begin());
    TEST_ASSERT_FALSE(refused.begin());
    TEST_ASSERT_EQUAL_UINT8(0, refused.position());
    TEST_ASSERT_NULL(fakePins[SWITCH_PIN].isr);
    // The second call answers without a second warning.
    TEST_ASSERT_EQUAL_UINT32(1, warnings("the event queue takes no further queue"));
}

static void testTickFull() {
    EventQueue otherQueue;
    DriverTick full;
    for (uint8_t i = 0; i < DriverTick::SLOTS; ++i) {
        TEST_ASSERT_TRUE(full.attach([] {}, DriverTick::PERIOD_US));
    }
    ToggleSwitch refused(otherQueue, full);
    fakePins[SWITCH_PIN].level = CLOSED;
    TEST_ASSERT_FALSE(refused.begin());
    TEST_ASSERT_FALSE(refused.begin());
    TEST_ASSERT_EQUAL_UINT8(0, refused.position());
    TEST_ASSERT_NULL(fakePins[SWITCH_PIN].isr);
    TEST_ASSERT_EQUAL_UINT32(1, warnings("the driver tick has no free slot"));
}

static void testAtStart() {
    GPT2_CNT = T0_US;
    fakePins[SWITCH_PIN].level = CLOSED;
    TEST_ASSERT_EQUAL_UINT8(0, toggle.position());
    TEST_ASSERT_TRUE(toggle.begin());
    timer = fakeTimers.running;
    TEST_ASSERT_EQUAL_INT(INPUT_PULLUP, fakePins[SWITCH_PIN].mode);
    TEST_ASSERT_EQUAL_INT(CHANGE, fakePins[SWITCH_PIN].interruptMode);
    // A second call answers as the first.
    TEST_ASSERT_TRUE(toggle.begin());
    TEST_ASSERT_EQUAL_UINT8(POSITION_CLOSED, toggle.position());
    ticksUntil(T0_US, T0_US + 2 * QUIET_US);
    TEST_ASSERT_EQUAL_UINT32(0, taken());
}

static void testFlip() {
    const uint32_t t = rest(T0_US + 2 * QUIET_US);
    edge(t, OPEN);
    TEST_ASSERT_EQUAL_UINT8(POSITION_OPEN, toggle.position());
    TEST_ASSERT_EQUAL_UINT32(1, taken());
    assertSwitched(0, t, POSITION_OPEN);
    ticksUntil(t, t + 2 * QUIET_US);
    TEST_ASSERT_EQUAL_UINT32(0, taken());
    // An edge that reads the position already reported publishes nothing.
    edge(t + 2 * QUIET_US, OPEN);
    TEST_ASSERT_EQUAL_UINT32(0, taken());
    // The flip back to 1 gives the second event.
    const uint32_t back = t + 3 * QUIET_US;
    edge(back, CLOSED);
    TEST_ASSERT_EQUAL_UINT32(1, taken());
    assertSwitched(0, back, POSITION_CLOSED);
    TEST_ASSERT_EQUAL_UINT8(POSITION_CLOSED, toggle.position());
}

static void testBounce() {
    const uint32_t t = rest(T0_US + 100 * QUIET_US);
    edge(t, OPEN);
    TEST_ASSERT_EQUAL_UINT32(1, taken());
    // The contact bounces and comes to rest in the reported position.
    edge(t + BOUNCE_US, CLOSED);
    edge(t + 2 * BOUNCE_US, OPEN);
    edge(t + QUIET_US - 1, CLOSED);
    edge(t + QUIET_US - 1, OPEN);
    ticksUntil(t, t + QUIET_US - 1);
    TEST_ASSERT_EQUAL_UINT32(0, taken());
    // The window closes on the tick at QUIET_US, and the switch rests where it was reported.
    ticksUntil(t + QUIET_US, t + 2 * QUIET_US);
    TEST_ASSERT_EQUAL_UINT32(0, taken());
    // With the window closed, an edge to a new position counts again.
    const uint32_t next = t + 2 * QUIET_US + 1;
    edge(next, CLOSED);
    TEST_ASSERT_EQUAL_UINT32(1, taken());
    assertSwitched(0, next, POSITION_CLOSED);
}

static void testFlipBack() {
    const uint32_t t = rest(T0_US + 200 * QUIET_US);
    edge(t, OPEN);
    TEST_ASSERT_EQUAL_UINT32(1, taken());
    // The switch is flipped back inside the window, and no further edge comes.
    const uint32_t back = t + QUIET_US / 2;
    edge(back, CLOSED);
    edge(back + BOUNCE_US, OPEN);
    edge(back + 2 * BOUNCE_US, CLOSED);
    ticksUntil(t, t + QUIET_US - 1);
    TEST_ASSERT_EQUAL_UINT32(0, taken());
    // The tick that closes the window publishes the resting position with the last edge's time.
    tickAt(t + QUIET_US);
    TEST_ASSERT_EQUAL_UINT32(1, taken());
    assertSwitched(0, back + 2 * BOUNCE_US, POSITION_CLOSED);
    // That report opens a window, and edges in it are bounce.
    edge(t + QUIET_US + BOUNCE_US, OPEN);
    edge(t + QUIET_US + 2 * BOUNCE_US, CLOSED);
    ticksUntil(t + QUIET_US + TICK_US, t + 3 * QUIET_US);
    TEST_ASSERT_EQUAL_UINT32(0, taken());
    TEST_ASSERT_EQUAL_UINT8(POSITION_CLOSED, toggle.position());
}

static void testWrap() {
    // The report comes just before the wrap of nowUs(), and the window ends after it.
    const uint32_t beforeWrap = UINT32_MAX - QUIET_US / 2;
    rest(beforeWrap - LONG_US);
    edge(beforeWrap, OPEN);
    TEST_ASSERT_EQUAL_UINT32(1, taken());
    edge(beforeWrap + QUIET_US / 4, CLOSED);
    ticksUntil(beforeWrap, beforeWrap + QUIET_US - 1);
    TEST_ASSERT_EQUAL_UINT32(0, taken());
    tickAt(beforeWrap + QUIET_US);
    TEST_ASSERT_EQUAL_UINT32(1, taken());
    assertSwitched(0, beforeWrap + QUIET_US / 4, POSITION_CLOSED);
    // A whole wrap of nowUs() after that report, nowUs() reads the report's time again. The tick
    // has closed the window long before, so the edge counts.
    const uint32_t reportUs = beforeWrap + QUIET_US;
    tickAt(reportUs + QUIET_US);
    edge(reportUs, OPEN);
    TEST_ASSERT_EQUAL_UINT32(1, taken());
    assertSwitched(0, reportUs, POSITION_OPEN);
    rest(reportUs);
    edge(reportUs + LONG_US, CLOSED);
    taken();
}

static void testSecondDriver() {
    EventQueue otherQueue;
    DriverTick otherTicker;
    ToggleSwitch second(otherQueue, otherTicker);
    fakePins[SWITCH_PIN].level = CLOSED;
    TEST_ASSERT_EQUAL_UINT8(0, second.position());
    TEST_ASSERT_FALSE(second.begin());
    TEST_ASSERT_FALSE(second.begin());
    TEST_ASSERT_EQUAL_UINT8(0, second.position());
    TEST_ASSERT_EQUAL_UINT32(1, warnings("another toggle switch driver holds the pin"));
    // The first driver keeps the interrupt and its events.
    const uint32_t t = rest(UINT32_MAX / 2);
    edge(t, OPEN);
    TEST_ASSERT_EQUAL_UINT32(1, taken());
    DriverEvent none[1];
    TEST_ASSERT_EQUAL_UINT32(0, otherQueue.read(none, 1));
    TEST_ASSERT_EQUAL_UINT8(POSITION_OPEN, toggle.position());
}

void setUp() {}
void tearDown() {}

int main() {
    logger.begin(LogFileStart{}, LogFileStart{});
    UNITY_BEGIN();
    RUN_TEST(testQueueFull);
    RUN_TEST(testTickFull);
    RUN_TEST(testAtStart);
    RUN_TEST(testFlip);
    RUN_TEST(testBounce);
    RUN_TEST(testFlipBack);
    RUN_TEST(testWrap);
    RUN_TEST(testSecondDriver);
    return UNITY_END();
}
