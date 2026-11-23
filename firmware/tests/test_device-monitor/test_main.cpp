// Tests on the computer whether the device monitor reports every change of a driver's failed
// parts once. A fake driver answers failed() as the test sets it, a fake IntervalTimer stands in
// for the driver tick's, and the tests step it one check at a time.
//
//   changes     each part that fails or recovers gives one event, an unchanged answer none
//   full-queue  a part that changes while the queue is full is reported once it has room
//   source      faultDevice() and faultPart() give back what faultSource() packed
//   log-line    logFault() writes a failed part as a warning with its device, part and code, a
//               recovered part as a line of the level INFO, and nothing for another event
//
// Run: pio test -e computer, which runs in CI as well.

#include "core/device-monitor.h"
#include "logging/logger.h"
#include <string>
#include <unity.h>

constexpr uint32_t CHECK_TICKS = DeviceMonitor::CHECK_US / DriverTick::PERIOD_US;
constexpr uint8_t PARTS = 16;   // the parts a PartMask holds
constexpr PartMask ALL_PARTS = 0xFFFF;
constexpr uint32_t FIRST_CODE = 7;
constexpr uint32_t SECOND_CODE = 0xDEADBEEF;
constexpr uint32_t T0_US = 1000;   // the time of the first check
constexpr uint32_t ROOM = 2;       // full-queue: the events read to make room

// a driver whose failed() answers what the test sets
class FakeDriver : public Driver {
public:
    bool begin() override { return true; }
    Fault failed() const override { return answer; }

    Fault answer = {};
};

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

// a device monitor on a queue and a driver tick that only this test uses
struct Rig {
    EventQueue queue;
    DriverTick tick;
    DriverEvent storage[DeviceMonitor::DEPTH];
    DeviceMonitor monitor{queue, tick, storage};
    DriverEvent out[DeviceMonitor::DEPTH + 1];

    // Lets the driver tick run up to the next check, with nowUs() at nowUs.
    void check(uint32_t nowUs) {
        GPT2_CNT = nowUs;
        for (uint32_t i = 0; i < CHECK_TICKS; ++i) {
            fakeTimers.running->fire();
        }
    }

    // Reads up to max events into out.
    uint32_t read(uint32_t max = DeviceMonitor::DEPTH + 1) { return queue.read(out, max); }
};

static void testLogLine() {
    static LogFile first;
    static LogFile second;
    static uint8_t buffer[1024];
    static Logger logger(first, second, buffer, sizeof buffer);
    logger.begin(LogFileStart{}, LogFileStart{});
    const DriverEvent failed = {0, DriverEventType::DEVICE_FAILED, faultSource(Device::SD_CARD, 0),
                                FIRST_CODE};
    const DriverEvent recovered = {0, DriverEventType::DEVICE_RECOVERED,
                                   faultSource(Device::SOLENOIDS, 3), 0};
    const DriverEvent touch = {0, DriverEventType::TOUCH_DOWN, 0, 0};
    TEST_ASSERT_TRUE(logFault(failed));
    TEST_ASSERT_TRUE(logFault(recovered));
    TEST_ASSERT_FALSE(logFault(touch));
    logger.flush();
    logger.update();
    logger.update();
    const std::string& log = first.content;
    TEST_ASSERT_TRUE(log.find(" W monitor SD card part 0 failed, error code 7\n") !=
                     std::string::npos);
    TEST_ASSERT_TRUE(log.find(" I monitor solenoids part 3 works again\n") != std::string::npos);
    // The banner and the two lines.
    size_t lines = 0;
    for (char c : log) {
        lines += c == '\n' ? 1 : 0;
    }
    TEST_ASSERT_EQUAL_UINT32(1 + 4 + 2, lines);
    TEST_ASSERT_EQUAL_STRING("device", deviceName(Device::COUNT));
}

void setUp() {
    fakeTimers = FakeTimers{};
}

void tearDown() {}

static void assertEvent(const DriverEvent& e, DriverEventType type, Device d, uint8_t part,
                        uint32_t code, uint32_t timeUs) {
    TEST_ASSERT_EQUAL_UINT16(uint16_t(type), uint16_t(e.type));
    TEST_ASSERT_EQUAL_UINT8(faultSource(d, part), e.source);
    TEST_ASSERT_EQUAL_HEX32(code, e.payload);
    TEST_ASSERT_EQUAL_UINT32(timeUs, e.time);
}

// changes: each change gives one DEVICE_FAILED with the device, the part and the code, or one
// DEVICE_RECOVERED, and an unchanged answer gives nothing.
static void testChanges() {
    Rig rig;
    FakeDriver sensor;
    FakeDriver servo;
    TEST_ASSERT_TRUE(rig.monitor.watch(Device::IR_SENSING, sensor));
    TEST_ASSERT_TRUE(rig.monitor.watch(Device::SERVO, servo));
    uint32_t t = T0_US;

    rig.check(t);
    TEST_ASSERT_EQUAL_UINT32(0, rig.read());

    sensor.answer = Fault{PartMask(1u << 0 | 1u << 3), FIRST_CODE};
    servo.answer = Fault{1u << 0, 0};
    rig.check(++t);
    TEST_ASSERT_EQUAL_UINT32(3, rig.read());
    assertEvent(rig.out[0], DriverEventType::DEVICE_FAILED, Device::IR_SENSING, 0, FIRST_CODE, t);
    assertEvent(rig.out[1], DriverEventType::DEVICE_FAILED, Device::IR_SENSING, 3, FIRST_CODE, t);
    assertEvent(rig.out[2], DriverEventType::DEVICE_FAILED, Device::SERVO, 0, 0, t);

    rig.check(++t);
    TEST_ASSERT_EQUAL_UINT32(0, rig.read());

    sensor.answer = Fault{PartMask(1u << 3 | 1u << 15), SECOND_CODE};
    servo.answer = Fault{};
    rig.check(++t);
    TEST_ASSERT_EQUAL_UINT32(3, rig.read());
    assertEvent(rig.out[0], DriverEventType::DEVICE_RECOVERED, Device::IR_SENSING, 0, 0, t);
    assertEvent(rig.out[1], DriverEventType::DEVICE_FAILED, Device::IR_SENSING, 15, SECOND_CODE, t);
    assertEvent(rig.out[2], DriverEventType::DEVICE_RECOVERED, Device::SERVO, 0, 0, t);

    rig.check(++t);
    TEST_ASSERT_EQUAL_UINT32(0, rig.read());
}

// full-queue: each part is reported at the first check after the queue has room.
static void testFullQueue() {
    Rig rig;
    FakeDriver sensor;
    TEST_ASSERT_TRUE(rig.monitor.watch(Device::IR_SENSING, sensor));
    uint32_t t = T0_US;

    // Every part failing and recovering fills the queue to its depth.
    static_assert(2 * PARTS == DeviceMonitor::DEPTH, "two changes of every part fill the queue");
    sensor.answer = Fault{ALL_PARTS, FIRST_CODE};
    rig.check(++t);
    sensor.answer = Fault{};
    rig.check(++t);
    const uint32_t filledUs = t;

    // Four parts fail while the queue is full, and stay unreported over several checks.
    sensor.answer = Fault{0x000F, SECOND_CODE};
    rig.check(++t);
    rig.check(++t);

    // Two events read make room for two parts.
    TEST_ASSERT_EQUAL_UINT32(ROOM, rig.read(ROOM));
    rig.check(++t);
    const uint32_t roomUs = t;
    TEST_ASSERT_EQUAL_UINT32(DeviceMonitor::DEPTH, rig.read());
    const uint32_t old = DeviceMonitor::DEPTH - ROOM;
    assertEvent(rig.out[old - 1], DriverEventType::DEVICE_RECOVERED, Device::IR_SENSING, PARTS - 1,
                0, filledUs);
    assertEvent(rig.out[old], DriverEventType::DEVICE_FAILED, Device::IR_SENSING, 0, SECOND_CODE,
                roomUs);
    assertEvent(rig.out[old + 1], DriverEventType::DEVICE_FAILED, Device::IR_SENSING, 1,
                SECOND_CODE, roomUs);

    // The queue is empty now, so the next check reports the other two.
    rig.check(++t);
    TEST_ASSERT_EQUAL_UINT32(2, rig.read());
    assertEvent(rig.out[0], DriverEventType::DEVICE_FAILED, Device::IR_SENSING, 2, SECOND_CODE, t);
    assertEvent(rig.out[1], DriverEventType::DEVICE_FAILED, Device::IR_SENSING, 3, SECOND_CODE, t);

    rig.check(++t);
    TEST_ASSERT_EQUAL_UINT32(0, rig.read());
}

// source: faultDevice() and faultPart() give back what faultSource() packed.
static void testSource() {
    for (uint8_t d = 0; d < uint8_t(Device::COUNT); ++d) {
        for (uint8_t part = 0; part < PARTS; ++part) {
            const uint8_t source = faultSource(Device(d), part);
            TEST_ASSERT_EQUAL_UINT8(d, uint8_t(faultDevice(source)));
            TEST_ASSERT_EQUAL_UINT8(part, faultPart(source));
        }
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(testChanges);
    RUN_TEST(testFullQueue);
    RUN_TEST(testSource);
    RUN_TEST(testLogLine);
    return UNITY_END();
}
