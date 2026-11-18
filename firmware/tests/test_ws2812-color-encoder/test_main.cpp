// Tests on the computer whether the encoder turns LED colors into the shifter words the WS2812B
// takes.
//
//   symbols  a 0 and a 1 are 13 shifter bits with 3 and 7 high bits first, inside the highs, lows
//            and bit period of both datasheets
//   order    an LED goes out green, red, blue, each most significant bit first
//   lanes    the words of a lane land every stride words, and the other lanes' words stay
//   padding  after the last LED the lane holds zero words, which keep the line low
//   scale    a brightness scales every color as FastLED's scale8() does
//
// Run: pio test -e computer, which runs in CI as well.

#include <unity.h>
#include <ws2812-color-encoder.h>

using namespace ws2812;

// WS2812B datasheet, older revision: T0H 0.4 µs, T1H 0.8 µs, T0L 0.85 µs, T1L 0.45 µs, each
// ±150 ns, and TH+TL 1.25 µs ±600 ns
constexpr uint32_t T0H_NS = 400;
constexpr uint32_t T1H_NS = 800;
constexpr uint32_t T0L_NS = 850;
constexpr uint32_t T1L_NS = 450;
constexpr uint32_t TOL_NS = 150;
constexpr uint32_t PERIOD_NS = 1250;
constexpr uint32_t PERIOD_TOL_NS = 600;
// WS2812B-2020 datasheet V6: T0H 220 to 380 ns, T1H, T0L and T1L 580 ns to 1 µs, and a bit at
// least 1.25 µs
constexpr uint32_t V6_T0H_MIN_NS = 220;
constexpr uint32_t V6_T0H_MAX_NS = 380;
constexpr uint32_t V6_MIN_NS = 580;
constexpr uint32_t V6_MAX_NS = 1000;
constexpr uint32_t V6_PERIOD_MIN_NS = 1250;
constexpr uint32_t SYMBOL_MASK = (1u << SYMBOL_BITS) - 1;
constexpr uint8_t COLORS = 3;
constexpr uint32_t MARK = 0xDEADBEEF;   // a word no encoding writes

void setUp() {}

void tearDown() {}

// the high bits at the start of a symbol, and whether every bit after them is low
static uint32_t leadingHighs(uint32_t symbol, bool* lowAfter) {
    uint32_t highs = 0;
    while (highs < SYMBOL_BITS && ((symbol >> highs) & 1)) {
        ++highs;
    }
    *lowAfter = (symbol >> highs) == 0;
    return highs;
}

// the LED bits a lane's words carry, most significant first, decoded from the symbols
static void decode(const uint32_t* words, size_t stride, uint32_t count, uint8_t* bits) {
    uint32_t b = 0;
    for (uint32_t w = 0; w < count; ++w) {
        for (uint32_t k = 0; k < SYMBOLS_PER_WORD; ++k) {
            const uint32_t symbol = (words[w * stride] >> (SYMBOL_BITS * k)) & SYMBOL_MASK;
            bits[b++] = symbol == ONE ? 1 : (symbol == ZERO ? 0 : 2);
        }
    }
}

static uint8_t byteAt(const uint8_t* bits) {
    uint8_t v = 0;
    for (uint32_t i = 0; i < BITS_PER_COLOR; ++i) {
        v = uint8_t((v << 1) | bits[i]);
    }
    return v;
}

static void testSymbols() {
    bool lowAfter = false;
    const uint32_t zeroHighs = leadingHighs(ZERO, &lowAfter);
    TEST_ASSERT_EQUAL_UINT32(3, zeroHighs);
    TEST_ASSERT_TRUE(lowAfter);
    const uint32_t oneHighs = leadingHighs(ONE, &lowAfter);
    TEST_ASSERT_EQUAL_UINT32(7, oneHighs);
    TEST_ASSERT_TRUE(lowAfter);
    const uint32_t periodNs = SYMBOL_BITS * SYMBOL_NS;
    const uint32_t t0h = zeroHighs * SYMBOL_NS;
    const uint32_t t1h = oneHighs * SYMBOL_NS;
    const uint32_t t0l = periodNs - t0h;
    const uint32_t t1l = periodNs - t1h;
    TEST_ASSERT_UINT32_WITHIN(TOL_NS, T0H_NS, t0h);
    TEST_ASSERT_UINT32_WITHIN(TOL_NS, T1H_NS, t1h);
    TEST_ASSERT_UINT32_WITHIN(TOL_NS, T0L_NS, t0l);
    TEST_ASSERT_UINT32_WITHIN(TOL_NS, T1L_NS, t1l);
    TEST_ASSERT_UINT32_WITHIN(PERIOD_TOL_NS, PERIOD_NS, periodNs);
    TEST_ASSERT_TRUE(t0h >= V6_T0H_MIN_NS && t0h <= V6_T0H_MAX_NS);
    TEST_ASSERT_TRUE(t1h >= V6_MIN_NS && t1h <= V6_MAX_NS);
    TEST_ASSERT_TRUE(t0l >= V6_MIN_NS && t0l <= V6_MAX_NS);
    TEST_ASSERT_TRUE(t1l >= V6_MIN_NS && t1l <= V6_MAX_NS);
    TEST_ASSERT_TRUE(periodNs >= V6_PERIOD_MIN_NS);
    TEST_ASSERT_TRUE(WORD_BITS <= 32);
    TEST_ASSERT_EQUAL_UINT32(BITS_PER_LED, WORDS_PER_LED * SYMBOLS_PER_WORD);
}

static void testOrder() {
    // stored red, green, blue: red 0xA5, green 0x3C, blue 0x01
    const uint8_t led[COLORS] = {0xA5, 0x3C, 0x01};
    uint32_t words[WORDS_PER_LED];
    encodeLane(led, 1, 255, words, 1, WORDS_PER_LED);
    uint8_t bits[BITS_PER_LED];
    decode(words, 1, WORDS_PER_LED, bits);
    TEST_ASSERT_EQUAL_HEX8(0x3C, byteAt(bits));
    TEST_ASSERT_EQUAL_HEX8(0xA5, byteAt(bits + BITS_PER_COLOR));
    TEST_ASSERT_EQUAL_HEX8(0x01, byteAt(bits + 2 * BITS_PER_COLOR));
}

static void testLanes() {
    constexpr size_t LANES = 3;
    constexpr uint32_t COUNT = 2 * WORDS_PER_LED;
    const uint8_t leds[2 * COLORS] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
    uint32_t words[COUNT * LANES];
    for (uint32_t& w : words) {
        w = MARK;
    }
    encodeLane(leds, 2, 255, words + 1, LANES, COUNT);
    for (uint32_t w = 0; w < COUNT; ++w) {
        TEST_ASSERT_EQUAL_HEX32(MARK, words[w * LANES]);
        TEST_ASSERT_EQUAL_HEX32(MARK, words[w * LANES + 2]);
    }
    uint8_t bits[2 * BITS_PER_LED];
    decode(words + 1, LANES, COUNT, bits);
    const uint8_t sent[] = {0x34, 0x12, 0x56, 0x9A, 0x78, 0xBC};
    for (uint32_t i = 0; i < sizeof sent; ++i) {
        TEST_ASSERT_EQUAL_HEX8(sent[i], byteAt(bits + i * BITS_PER_COLOR));
    }
}

static void testPadding() {
    constexpr uint32_t COUNT = 3 * WORDS_PER_LED;
    const uint8_t led[COLORS] = {0xFF, 0xFF, 0xFF};
    uint32_t words[COUNT];
    for (uint32_t& w : words) {
        w = MARK;
    }
    encodeLane(led, 1, 255, words, 1, COUNT);
    TEST_ASSERT_EQUAL_HEX32(pairWord(PAIRS - 1), words[0]);
    for (uint32_t w = WORDS_PER_LED; w < COUNT; ++w) {
        TEST_ASSERT_EQUAL_HEX32(0, words[w]);
    }
    // a lane shorter than its LEDs stops at count, whole LEDs only
    uint32_t short_[WORDS_PER_LED + 1];
    short_[WORDS_PER_LED] = MARK;
    const uint8_t two[2 * COLORS] = {};
    encodeLane(two, 2, 255, short_, 1, WORDS_PER_LED);
    TEST_ASSERT_EQUAL_HEX32(MARK, short_[WORDS_PER_LED]);
}

static void testScale() {
    TEST_ASSERT_EQUAL_UINT8(255, scale(255, 255));
    TEST_ASSERT_EQUAL_UINT8(200, scale(200, 255));
    TEST_ASSERT_EQUAL_UINT8(0, scale(255, 0));
    TEST_ASSERT_EQUAL_UINT8(127, scale(255, 127));
    TEST_ASSERT_EQUAL_UINT8(50, scale(100, 127));
    const uint8_t led[COLORS] = {255, 100, 0};
    uint32_t words[WORDS_PER_LED];
    encodeLane(led, 1, 127, words, 1, WORDS_PER_LED);
    uint8_t bits[BITS_PER_LED];
    decode(words, 1, WORDS_PER_LED, bits);
    TEST_ASSERT_EQUAL_HEX8(50, byteAt(bits));
    TEST_ASSERT_EQUAL_HEX8(127, byteAt(bits + BITS_PER_COLOR));
    TEST_ASSERT_EQUAL_HEX8(0, byteAt(bits + 2 * BITS_PER_COLOR));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(testSymbols);
    RUN_TEST(testOrder);
    RUN_TEST(testLanes);
    RUN_TEST(testPadding);
    RUN_TEST(testScale);
    return UNITY_END();
}
