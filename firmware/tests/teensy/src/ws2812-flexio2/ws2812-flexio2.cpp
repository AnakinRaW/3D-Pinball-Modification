// Tests on the Teensy whether the FlexIO2 output sends every lane's bits with the encoder's
// timing, on the lanes and pins it is given, and refuses what it cannot send.
//
//   wave       six lanes on the lighting's pins send their LEDs' bits with the encoder's highs,
//              read back from the pins themselves, and rest low before and after the frame
//   two-lanes  the same with two lanes on two of those pins, the later pin first
//   busy       a second frame right after the first is refused at once, and one after the frame
//              and its latch goes out
//   refusals   no lane, more lanes than shifters, a pin without FlexIO2 and a pin taken twice
//              are each refused
//   long       a lane as long as the lighting allows sends every bit of its frame
//   load       frame after frame of that lane comes out whole while a second DMA channel copies
//              memory without a pause
//   latch      a frame sent the moment the last one is done starts only after the LEDs' reset time
//   clock      a clock root at another rate is refused while FlexIO3 is enabled on it, and set to
//              120 MHz while FlexIO3 is not
//   shared     while one output's frame goes out, another output is busy and refuses to send
//
// Wiring: none. LEDs on chain A may stay connected. Its first three show the wave's colors for a
// moment and go dark again. long, load and latch send on chain F, where no LED is wired.

#include <Arduino.h>
#include <DMAChannel.h>
#include <lighting.h>
#include <ws2812-flexio2.h>
#include <ws2812-color-encoder.h>
#include "test-harness.h"

using Lane = Ws2812Flexio2::Lane;

constexpr uint8_t COLORS = 3;   // red, green, blue, as FastLED's CRGB stores them
constexpr uint8_t SIX = 6;
constexpr uint8_t TWO = 2;
constexpr uint16_t MOST_LEDS = 3;
constexpr uint8_t PINS[SIX] = {32, 6, 7, 8, 9, 10};   // pin-assignment.md: LED chain A to F
// ten LEDs over six lanes, chain A's three the bench's
constexpr uint8_t SIX_OF[SIX] = {0, 1, 2, 3, 4, 5};
constexpr uint16_t SIX_LEDS[SIX] = {3, 2, 2, 1, 1, 1};
// two-lanes: chain F's pin first, then chain A's, two LEDs each
constexpr uint8_t TWO_OF[TWO] = {5, 0};
constexpr uint16_t TWO_LEDS[TWO] = {2, 2};
constexpr uint8_t NOT_FLEXIO2 = 5;   // pin-assignment.md: Toggle switch
constexpr uint8_t FULL = 255;        // a brightness that scales nothing
constexpr uint32_t CPU_MHZ = 600;    // F_CPU_ACTUAL, the cycle counter's rate
constexpr uint32_t NS_PER_US = 1000;
constexpr uint32_t US_PER_MS = 1000;
constexpr uint32_t WATCH_US = 600;     // longer than the frame of three LEDs and its latch
constexpr uint32_t WAVE_EDGES = 512;   // three edges a bit at most, for the 72 bits of chain A
constexpr uint32_t SETTLE_MS = 10;     // longer than any frame of these tests
constexpr uint32_t AT_ONCE_US = 50;    // what a refused send() may take
constexpr uint32_t LIMIT_S = 10;       // the longest each test takes, with room to spare
// the highs the encoder builds, whose place in the datasheets the computer tests check
constexpr uint32_t T0H_NS = ws2812::ZERO_HIGHS * ws2812::SYMBOL_NS;
constexpr uint32_t T1H_NS = ws2812::ONE_HIGHS * ws2812::SYMBOL_NS;
constexpr uint32_t ONE_FROM_NS = (T0H_NS + T1H_NS) / 2;   // a high past this reads as a 1
// How far a reading may lie from the high it reads: one pass of the reading loop, which reads
// FLEXIO2_PIN and the cycle counter, took up to about 40 ns in the bench runs, and either edge
// may land anywhere in a pass.
constexpr uint32_t READ_TOL_NS = 80;

// long, load and latch: one lane on chain F's pin
constexpr uint8_t CHAIN_F = 5;
constexpr uint16_t LONG_LEDS = Lighting::MAX_CHAIN_LEDS;   // the longest chain the lighting allows
constexpr uint16_t LATCH_LEDS = 2;
// the frame of the long lane and, beyond it, room for its zero words
constexpr uint32_t LONG_FRAME_US = uint32_t(LONG_LEDS) * ws2812::BITS_PER_LED *
                                   ws2812::SYMBOL_BITS * ws2812::SYMBOL_NS / NS_PER_US;
constexpr uint32_t LATCH_ROOM_US = 400;   // longer than the driver's zero words
constexpr uint32_t LONG_WATCH_US = LONG_FRAME_US + LATCH_ROOM_US;
constexpr uint32_t LONG_LIMIT_S = 30;
// load: frames a run sends, one every FRAME_MS, the lamps' frame period
constexpr uint32_t FRAME_MS = 11;
constexpr uint32_t LOAD_QUICK_MS = 1100;
constexpr uint32_t LOAD_SOAK_MS = 11000;
constexpr uint32_t LOAD_WORDS = 4096;   // each of the two buffers the second DMA copies between
// WS2812B datasheet Ver. No. V5: RES, the low time that ends a frame, more than 280 µs
constexpr uint32_t RESET_US = 280;
constexpr uint32_t LATCH_WATCH_US = 1000;   // two short frames and the zero words between them
// clock: RM CCM, CS1CDR: FLEXIO2_CLK_PODF 3 divides by 4, which puts the root at 60 MHz
constexpr uint32_t PODF_MASK = CCM_CS1CDR_FLEXIO2_CLK_PODF(7);
constexpr uint32_t PODF_SLOW = CCM_CS1CDR_FLEXIO2_CLK_PODF(3);
constexpr uint32_t PODF_120 = CCM_CS1CDR_FLEXIO2_CLK_PODF(1);

struct Edge {
    uint32_t pins;
    uint32_t cycles;
};

// what reading one lane found
struct Reading {
    uint32_t bits;
    uint32_t errors;
    uint32_t firstError;
    uint32_t zeroMin;
    uint32_t zeroMax;
    uint32_t oneMin;
    uint32_t oneMax;
    bool lowAfter;
};

// one set of lanes under test: the output, its lanes and the colors it sends
struct Rig {
    Ws2812Flexio2 output;
    uint8_t count;
    Lane lanes[SIX];
    uint8_t signals[SIX];
    uint8_t colors[SIX][MOST_LEDS * COLORS];
    const uint8_t* rgb[SIX];
    bool begun;
};

static Rig six;
static Rig two;
static Edge edges[WAVE_EDGES];
static Ws2812Flexio2 longOutput;
static bool longBegun = false;
static uint8_t longColors[LONG_LEDS * COLORS];
static uint8_t longSent[LONG_LEDS * COLORS];   // the same bytes in the order they go out
static Ws2812Flexio2 latchOutput;
static bool latchBegun = false;
static uint8_t latchColors[LATCH_LEDS * COLORS];
static DMAMEM uint32_t loadFrom[LOAD_WORDS];
static DMAMEM uint32_t loadTo[LOAD_WORDS];
static DMAChannel load(false);
static Ws2812Flexio2 slowWithFlexio3;
static Ws2812Flexio2 slowAlone;

// The wave's pattern, color 0 to 2 for red, green and blue of LED i on lane l: each LED differs,
// so a shifted or swapped lane shows.
static uint8_t waveColor(uint8_t l, uint8_t i, uint8_t color) {
    const uint8_t rgb[COLORS] = {uint8_t(0xA5 ^ (l * 0x11 + i)),
                                 uint8_t(0x5A + l * 0x07 + i * 0x13),
                                 uint8_t(0x0F << ((l + i) % 4))};
    return rgb[color];
}

// the bit lane l sends as its bit number bit: green, red, blue, the most significant bit first
static bool waveBit(uint8_t l, uint32_t bit) {
    const uint8_t i = uint8_t(bit / ws2812::BITS_PER_LED);
    const uint32_t k = bit % ws2812::BITS_PER_LED;
    const uint8_t sent[] = {waveColor(l, i, 1), waveColor(l, i, 0), waveColor(l, i, 2)};
    const uint8_t byte = sent[k / ws2812::BITS_PER_COLOR];
    return (byte >> (ws2812::BITS_PER_COLOR - 1 - k % ws2812::BITS_PER_COLOR)) & 1;
}

// Gives a rig lanes on the lighting's chains, in the order given.
static void assign(Rig& rig, const uint8_t* chains, const uint16_t* leds, uint8_t count) {
    rig.count = count;
    for (uint8_t l = 0; l < count; ++l) {
        rig.lanes[l] = {PINS[chains[l]], leds[l]};
        rig.signals[l] = Ws2812Flexio2::signalOf(PINS[chains[l]]);
        rig.rgb[l] = rig.colors[l];
    }
}

// Starts the rig's output once. Reports a failure and returns false where begin() fails.
static bool started(Rig& rig) {
    if (!rig.begun) {
        rig.begun = rig.output.begin(rig.lanes, rig.count);
        if (!rig.begun) {
            test::fail("begin() false", "begin() true");
        }
    }
    return rig.begun;
}

// Fills the rig's LEDs with the wave's pattern, or makes them dark.
static void fill(Rig& rig, bool pattern) {
    for (uint8_t l = 0; l < rig.count; ++l) {
        for (uint8_t i = 0; i < rig.lanes[l].leds; ++i) {
            for (uint8_t color = 0; color < COLORS; ++color) {
                rig.colors[l][i * COLORS + color] = pattern ? waveColor(l, i, color) : 0;
            }
        }
    }
}

// One frame of the wave on the rig, read back from FLEXIO2_PIN, which reads the pads while FlexIO2
// drives them. Every lane carries exactly its LEDs' bits, each high lies within the reading's
// tolerance of the encoder's T0H or T1H, and the lines rest low before and after the frame. The
// first high of each lane may start before the reading does, so its length is left out.
static void sendAndRead(Rig& rig) {
    if (!started(rig)) {
        return;
    }
    uint32_t mask = 0;
    for (uint8_t l = 0; l < rig.count; ++l) {
        mask |= 1u << rig.signals[l];
    }
    fill(rig, true);
    char seen[80];
    char wanted[64];
    delay(SETTLE_MS);
    const uint32_t before = FLEXIO2_PIN & mask;
    uint32_t n = 0;
    uint32_t level = 0;
    __disable_irq();
    const uint32_t c0 = ARM_DWT_CYCCNT;
    const bool sent = rig.output.send(rig.rgb, FULL);
    while (ARM_DWT_CYCCNT - c0 < WATCH_US * CPU_MHZ) {
        const uint32_t v = FLEXIO2_PIN & mask;
        if (v != level && n < WAVE_EDGES) {
            edges[n++] = {v, ARM_DWT_CYCCNT};
        }
        level = v;
    }
    __enable_irq();
    delay(SETTLE_MS);
    fill(rig, false);
    rig.output.send(rig.rgb, FULL);
    if (!sent) {
        test::fail("send() false", "send() true");
        return;
    }
    if (before != 0 || level != 0) {
        snprintf(seen, sizeof seen, "pins %08lx before, %08lx after", before, level);
        test::fail(seen, "every lane low");
        return;
    }
    uint32_t zeroMin = UINT32_MAX;
    uint32_t zeroMax = 0;
    uint32_t oneMin = UINT32_MAX;
    uint32_t oneMax = 0;
    for (uint8_t l = 0; l < rig.count; ++l) {
        const uint32_t pin = 1u << rig.signals[l];
        const uint32_t bits = uint32_t(rig.lanes[l].leds) * ws2812::BITS_PER_LED;
        uint32_t got = 0;
        uint32_t rise = c0;
        bool high = false;
        for (uint32_t e = 0; e < n; ++e) {
            const bool now = (edges[e].pins & pin) != 0;
            if (now && !high) {
                rise = edges[e].cycles;
            } else if (!now && high) {
                const uint32_t ns = (edges[e].cycles - rise) * NS_PER_US / CPU_MHZ;
                const bool one = ns > ONE_FROM_NS;
                if (got < bits && one != waveBit(l, got)) {
                    snprintf(seen, sizeof seen, "lane %u bit %lu a %d, high %lu ns", l, got, one,
                             ns);
                    snprintf(wanted, sizeof wanted, "a %d", waveBit(l, got));
                    test::fail(seen, wanted);
                    return;
                }
                if (got > 0) {
                    uint32_t& lo = one ? oneMin : zeroMin;
                    uint32_t& hi = one ? oneMax : zeroMax;
                    lo = ns < lo ? ns : lo;
                    hi = ns > hi ? ns : hi;
                }
                ++got;
            }
            high = now;
        }
        if (got != bits) {
            snprintf(seen, sizeof seen, "lane %u sent %lu bits", l, got);
            snprintf(wanted, sizeof wanted, "%lu", bits);
            test::fail(seen, wanted);
            return;
        }
    }
    Serial.printf("0s high %lu to %lu ns, 1s high %lu to %lu ns, %lu edges\n", zeroMin, zeroMax,
                  oneMin, oneMax, n);
    if (zeroMin + READ_TOL_NS < T0H_NS || zeroMax > T0H_NS + READ_TOL_NS ||
        oneMin + READ_TOL_NS < T1H_NS || oneMax > T1H_NS + READ_TOL_NS) {
        snprintf(seen, sizeof seen, "0s %lu to %lu ns, 1s %lu to %lu ns", zeroMin, zeroMax, oneMin,
                 oneMax);
        snprintf(wanted, sizeof wanted, "%lu and %lu ns, each within %lu ns", T0H_NS, T1H_NS,
                 READ_TOL_NS);
        test::fail(seen, wanted);
        return;
    }
    test::pass();
}

// wave: six lanes on the lighting's pins, in the order of chains A to F.
static void testWave() {
    assign(six, SIX_OF, SIX_LEDS, SIX);
    sendAndRead(six);
}

// two-lanes: two lanes, chain F's pin first and chain A's second.
static void testTwoLanes() {
    assign(two, TWO_OF, TWO_LEDS, TWO);
    sendAndRead(two);
}

// busy: a dark frame goes out, busy() holds while it does, and a second send() right after it is
// refused at once. Once the frame and its latch have gone out, busy() clears and send() takes the
// next frame.
static void testBusy() {
    assign(six, SIX_OF, SIX_LEDS, SIX);
    if (!started(six)) {
        return;
    }
    fill(six, false);
    delay(SETTLE_MS);
    char seen[64];
    if (six.output.busy()) {
        test::fail("busy() true after a pause", "false");
        return;
    }
    const bool first = six.output.send(six.rgb, FULL);
    const bool during = six.output.busy();
    const uint32_t t0Us = micros();
    const bool second = six.output.send(six.rgb, FULL);
    const uint32_t secondUs = micros() - t0Us;
    delay(SETTLE_MS);
    const bool after = six.output.busy();
    const bool third = six.output.send(six.rgb, FULL);
    delay(SETTLE_MS);
    if (!first || !during || second || after || !third) {
        snprintf(seen, sizeof seen, "send %d, busy %d, send %d, busy %d, send %d", first, during,
                 second, after, third);
        test::fail(seen, "send 1, busy 1, send 0, busy 0, send 1");
        return;
    }
    if (secondUs >= AT_ONCE_US) {
        snprintf(seen, sizeof seen, "the refused send() took %lu us", secondUs);
        test::fail(seen, "under 50 us");
        return;
    }
    test::pass();
}

// refusals: begin() refuses no lane, one lane more than FlexIO2 has shifters, a pin that does not
// reach FlexIO2 and a pin given to two lanes, and send() on a refused output sends nothing.
static void testRefusals() {
    static Ws2812Flexio2 none;
    static Ws2812Flexio2 tooMany;
    static Ws2812Flexio2 notFlexio;
    static Ws2812Flexio2 twice;
    Lane lanes[Ws2812Flexio2::MAX_LANES + 1];
    for (uint8_t l = 0; l <= Ws2812Flexio2::MAX_LANES; ++l) {
        lanes[l] = {PINS[l % SIX], 1};
    }
    const Lane wrong[TWO] = {{PINS[0], 1}, {NOT_FLEXIO2, 1}};
    const Lane same[TWO] = {{PINS[0], 1}, {PINS[0], 1}};
    const bool a = none.begin(lanes, 0);
    const bool b = tooMany.begin(lanes, Ws2812Flexio2::MAX_LANES + 1);
    const bool c = notFlexio.begin(wrong, TWO);
    const bool d = twice.begin(same, TWO);
    const uint8_t dark[COLORS] = {};
    const uint8_t* rgb[TWO] = {dark, dark};
    const bool e = notFlexio.send(rgb, FULL);
    const bool f = Ws2812Flexio2::signalOf(NOT_FLEXIO2) != Ws2812Flexio2::NO_SIGNAL;
    if (a || b || c || d || e || f) {
        char seen[64];
        snprintf(seen, sizeof seen, "begin %d %d %d %d, send %d, signal %d", a, b, c, d, e, f);
        test::fail(seen, "begin 0 0 0 0, send 0, signal 0");
        return;
    }
    test::pass();
}

// Fills the long lane with a pattern that changes with each frame and notes the bytes in the order
// green, red, blue, in which they go out.
static void fillLong(uint32_t frame) {
    for (uint16_t i = 0; i < LONG_LEDS; ++i) {
        uint8_t* led = longColors + i * COLORS;
        led[0] = uint8_t(i * 7 + frame);
        led[1] = uint8_t(i * 13 + 5 + 2 * frame);
        led[2] = uint8_t(255 - i * 3 + frame);
        uint8_t* sent = longSent + i * COLORS;
        sent[0] = led[1];
        sent[1] = led[0];
        sent[2] = led[2];
    }
}

// Starts the long lane once. Reports a failure and returns false where begin() fails.
static bool longStarted() {
    if (!longBegun) {
        const Lane lane = {PINS[CHAIN_F], LONG_LEDS};
        longBegun = longOutput.begin(&lane, 1);
        if (!longBegun) {
            test::fail("begin() false", "begin() true");
        }
    }
    return longBegun;
}

// Sends one frame of the long lane and checks each bit as it comes back on FLEXIO2_PIN, so no
// edge has to be stored. The first high may start before the reading does, so its length is left
// out of the extremes.
static Reading readLong() {
    Reading r = {0, 0, 0, UINT32_MAX, 0, UINT32_MAX, 0, false};
    const uint32_t pin = 1u << Ws2812Flexio2::signalOf(PINS[CHAIN_F]);
    const uint32_t bits = uint32_t(LONG_LEDS) * ws2812::BITS_PER_LED;
    const uint8_t* rgb[1] = {longColors};
    bool high = false;
    uint32_t rise = 0;
    __disable_irq();
    const uint32_t c0 = ARM_DWT_CYCCNT;
    rise = c0;
    if (!longOutput.send(rgb, FULL)) {
        __enable_irq();
        r.errors = UINT32_MAX;
        return r;
    }
    while (ARM_DWT_CYCCNT - c0 < LONG_WATCH_US * CPU_MHZ) {
        const bool now = (FLEXIO2_PIN & pin) != 0;
        if (now && !high) {
            rise = ARM_DWT_CYCCNT;
        } else if (!now && high) {
            const uint32_t ns = (ARM_DWT_CYCCNT - rise) * NS_PER_US / CPU_MHZ;
            const bool one = ns > ONE_FROM_NS;
            if (r.bits < bits) {
                const uint8_t byte = longSent[r.bits / ws2812::BITS_PER_COLOR];
                const bool wanted =
                    (byte >> (ws2812::BITS_PER_COLOR - 1 - r.bits % ws2812::BITS_PER_COLOR)) & 1;
                if (one != wanted && r.errors++ == 0) {
                    r.firstError = r.bits;
                }
            }
            if (r.bits > 0) {
                uint32_t& lo = one ? r.oneMin : r.zeroMin;
                uint32_t& hi = one ? r.oneMax : r.zeroMax;
                lo = ns < lo ? ns : lo;
                hi = ns > hi ? ns : hi;
            }
            ++r.bits;
        }
        high = now;
    }
    __enable_irq();
    r.lowAfter = !high;
    return r;
}

// Reports a failure for a reading that is not the long lane's frame, and returns whether it was.
static bool wholeFrame(const Reading& r, uint32_t frame) {
    const uint32_t bits = uint32_t(LONG_LEDS) * ws2812::BITS_PER_LED;
    char seen[96];
    char wanted[64];
    if (r.errors == UINT32_MAX) {
        test::fail("send() false", "send() true");
        return false;
    }
    if (r.bits != bits || r.errors != 0 || !r.lowAfter) {
        snprintf(seen, sizeof seen, "frame %lu: %lu bits, %lu wrong from bit %lu, low after %d",
                 frame, r.bits, r.errors, r.firstError, r.lowAfter);
        snprintf(wanted, sizeof wanted, "%lu bits, none wrong, low after", bits);
        test::fail(seen, wanted);
        return false;
    }
    if (r.zeroMin + READ_TOL_NS < T0H_NS || r.zeroMax > T0H_NS + READ_TOL_NS ||
        r.oneMin + READ_TOL_NS < T1H_NS || r.oneMax > T1H_NS + READ_TOL_NS) {
        snprintf(seen, sizeof seen, "frame %lu: 0s %lu to %lu ns, 1s %lu to %lu ns", frame,
                 r.zeroMin, r.zeroMax, r.oneMin, r.oneMax);
        snprintf(wanted, sizeof wanted, "%lu and %lu ns, each within %lu ns", T0H_NS, T1H_NS,
                 READ_TOL_NS);
        test::fail(seen, wanted);
        return false;
    }
    return true;
}

// long: one frame of the longest chain the lighting allows, every bit read back and checked.
static void testLong() {
    if (!longStarted()) {
        return;
    }
    delay(SETTLE_MS);
    fillLong(0);
    const Reading r = readLong();
    Serial.printf("%lu bits, 0s high %lu to %lu ns, 1s high %lu to %lu ns\n", r.bits, r.zeroMin,
                  r.zeroMax, r.oneMin, r.oneMax);
    if (wholeFrame(r, 0)) {
        test::pass();
    }
}

// load: the long lane's frames, one every frame period, each read back whole while a second DMA
// channel copies between two buffers in RAM2, where the frame's words lie, without a pause. The
// second channel is allocated later, so it has the higher fixed priority.
static void testLoad() {
    if (!longStarted()) {
        return;
    }
    // DMAChannel releases a channel only in its destructor, so the static channel is taken once
    if (load.TCD == nullptr) {
        load.begin(true);
    }
    if (load.TCD == nullptr) {
        test::skip("no second DMA channel free");
        return;
    }
    load.sourceBuffer(loadFrom, sizeof loadFrom);
    load.destinationBuffer(loadTo, sizeof loadTo);
    load.triggerContinuously();
    load.enable();
    const uint32_t frames = test::durationMs(LOAD_QUICK_MS, LOAD_SOAK_MS) / FRAME_MS;
    uint32_t zeroMin = UINT32_MAX;
    uint32_t zeroMax = 0;
    uint32_t oneMin = UINT32_MAX;
    uint32_t oneMax = 0;
    bool whole = true;
    for (uint32_t f = 0; f < frames && whole; ++f) {
        fillLong(f);
        const Reading r = readLong();
        whole = wholeFrame(r, f);
        zeroMin = r.zeroMin < zeroMin ? r.zeroMin : zeroMin;
        zeroMax = r.zeroMax > zeroMax ? r.zeroMax : zeroMax;
        oneMin = r.oneMin < oneMin ? r.oneMin : oneMin;
        oneMax = r.oneMax > oneMax ? r.oneMax : oneMax;
        delay(FRAME_MS - LONG_WATCH_US / US_PER_MS);
    }
    load.disable();
    Serial.printf("%lu frames under load, 0s high %lu to %lu ns, 1s high %lu to %lu ns\n", frames,
                  zeroMin, zeroMax, oneMin, oneMax);
    if (whole) {
        test::pass();
    }
}

// latch: two short frames, the second sent the moment busy() clears. The line stays low from the
// first frame's last bit to the second frame's first for longer than the LEDs' reset time.
static void testLatch() {
    if (!latchBegun) {
        const Lane lane = {PINS[CHAIN_F], LATCH_LEDS};
        latchBegun = latchOutput.begin(&lane, 1);
        if (!latchBegun) {
            test::fail("begin() false", "begin() true");
            return;
        }
    }
    for (uint8_t& v : latchColors) {
        v = FULL;
    }
    const uint8_t* rgb[1] = {latchColors};
    const uint32_t pin = 1u << Ws2812Flexio2::signalOf(PINS[CHAIN_F]);
    delay(SETTLE_MS);
    bool high = false;
    bool second = false;
    uint32_t lastFall = 0;
    uint32_t nextRise = 0;
    __disable_irq();
    const uint32_t c0 = ARM_DWT_CYCCNT;
    const bool first = latchOutput.send(rgb, FULL);
    while (ARM_DWT_CYCCNT - c0 < LATCH_WATCH_US * CPU_MHZ) {
        const bool now = (FLEXIO2_PIN & pin) != 0;
        const uint32_t t = ARM_DWT_CYCCNT;
        if (!now && high && !second) {
            lastFall = t;
        }
        if (now && !high && second && nextRise == 0) {
            nextRise = t;
        }
        high = now;
        if (!second && !latchOutput.busy()) {
            second = latchOutput.send(rgb, FULL);
        }
    }
    __enable_irq();
    delay(SETTLE_MS);
    const uint32_t gapUs = (nextRise - lastFall) / CPU_MHZ;
    Serial.printf("low between the two frames: %lu us\n", gapUs);
    char seen[64];
    char wanted[48];
    if (!first || !second || lastFall == 0 || nextRise == 0) {
        snprintf(seen, sizeof seen, "first %d, second %d, fall %d, rise %d", first, second,
                 lastFall != 0, nextRise != 0);
        test::fail(seen, "both frames read");
        return;
    }
    if (gapUs <= RESET_US) {
        snprintf(seen, sizeof seen, "%lu us low", gapUs);
        snprintf(wanted, sizeof wanted, "more than %lu us", RESET_US);
        test::fail(seen, wanted);
        return;
    }
    test::pass();
}

// clock: the shared root moved to 60 MHz. With FlexIO3 enabled, begin() refuses and leaves the
// root alone. With FlexIO3 off, begin() sets the root back to 120 MHz and starts, and a frame
// still goes out afterwards. The registers
// stand as they did before the test at its end. PJRC's start-up has FlexIO3's clock gate on.
static void testClock() {
    const uint32_t flexio3 = FLEXIO3_CTRL;
    const uint32_t divider = CCM_CS1CDR;
    const Lane lane = {PINS[CHAIN_F], 1};
    delay(SETTLE_MS);
    FLEXIO3_CTRL = flexio3 | FLEXIO_CTRL_FLEXEN;
    CCM_CS1CDR = (divider & ~PODF_MASK) | PODF_SLOW;
    const bool refused = !slowWithFlexio3.begin(&lane, 1);
    const bool left = (CCM_CS1CDR & PODF_MASK) == PODF_SLOW;
    FLEXIO3_CTRL = flexio3 & ~FLEXIO_CTRL_FLEXEN;
    const bool started = slowAlone.begin(&lane, 1);
    const bool set = (CCM_CS1CDR & PODF_MASK) == PODF_120;
    CCM_CS1CDR = divider;
    FLEXIO3_CTRL = flexio3;
    // FlexIO2 still answers after the change: a dark frame goes out
    const uint8_t dark[COLORS] = {};
    const uint8_t* rgb[1] = {dark};
    const bool sent = slowAlone.send(rgb, FULL);
    delay(SETTLE_MS);
    const bool done = !slowAlone.busy();
    if (!refused || !left || !started || !set || !sent || !done) {
        char seen[96];
        snprintf(seen, sizeof seen,
                 "refused %d, root left %d, then started %d, root set %d, sent %d, done %d",
                 refused, left, started, set, sent, done);
        test::fail(seen, "every one 1");
        return;
    }
    test::pass();
}

// shared: the long lane's output sends a frame. While it goes out, the six lanes' output is busy
// as well and refuses its send at once. Once the frame is out, the six lanes' send goes through.
static void testShared() {
    assign(six, SIX_OF, SIX_LEDS, SIX);
    if (!longStarted() || !started(six)) {
        return;
    }
    fillLong(0);
    fill(six, false);
    delay(SETTLE_MS);
    const uint8_t* rgb[1] = {longColors};
    const bool first = longOutput.send(rgb, FULL);
    const bool busy = six.output.busy();
    const bool refused = !six.output.send(six.rgb, FULL);
    delay(SETTLE_MS);
    const bool after = six.output.send(six.rgb, FULL);
    delay(SETTLE_MS);
    if (!first || !busy || !refused || !after) {
        char seen[64];
        snprintf(seen, sizeof seen, "sent %d, other busy %d, refused %d, then sent %d", first, busy,
                 refused, after);
        test::fail(seen, "sent 1, other busy 1, refused 1, then sent 1");
        return;
    }
    test::pass();
}

static const Test TESTS[] = {
    {"ws2812-flexio2.wave", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testWave, nullptr},
    {"ws2812-flexio2.two-lanes", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testTwoLanes, nullptr},
    {"ws2812-flexio2.busy", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testBusy, nullptr},
    {"ws2812-flexio2.refusals", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testRefusals, nullptr},
    {"ws2812-flexio2.long", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testLong, nullptr},
    {"ws2812-flexio2.load", RunsOn::TEENSY, LIMIT_S, LONG_LIMIT_S, testLoad, nullptr},
    {"ws2812-flexio2.latch", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testLatch, nullptr},
    {"ws2812-flexio2.clock", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testClock, nullptr},
    {"ws2812-flexio2.shared", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testShared, nullptr},
};

extern const Program WS2812_FLEXIO2 = {TESTS, sizeof TESTS / sizeof TESTS[0]};
