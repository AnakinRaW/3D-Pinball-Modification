// Tests on the Teensy whether the display driver sends its rectangles and restarts the panel.
//
//   picture        color bars, ramps and the words TOP LEFT look right (asks at the bench)
//   rectangles     rectangles go out in their bus time and the function handed in runs once; a
//                  rectangle off the steps, beyond the screen or during another is refused
//   refills        full screens keep the bus at its speed and cost the processor no more than the
//                  estimate
//   restart-steps  a restart holds a rectangle back until the panel is back, and lets the one on
//                  its way finish
//   with-lighting  the lighting's frames on FlexIO2, which shares FlexIO3's clock, go out every
//                  lamps' frame period while full screens go out, and the screens keep their speed
//
// Wiring: the Adafruit 2090 on its 8-bit bus, as pin-assignment.md gives it. The panel cannot be
// read back, so a test judges what the Teensy sees: the times and the calls of the function each
// rectangle hands in. The screen shows the test's colors meanwhile. with-lighting sends dark
// frames on chain F, which has no LEDs at the bench, so no LED lights.

#include <Arduino.h>
#include <string.h>
// FastLED's fl/gfx/colorutils.h turns warnings off twice and back on once, so without
// the push and pop -Wunused-parameter and -Wreturn-type stay off for all code after it.
#pragma GCC diagnostic push
#include <FastLED.h>
#pragma GCC diagnostic pop
#include "core/clock.h"
#include "drivers/display.h"
#include "drivers/lighting.h"
#include "test-harness.h"

constexpr uint32_t W = Display::WIDTH;
constexpr uint32_t H = Display::HEIGHT;
constexpr uint32_t BYTES_PER_PIXEL = 2;
constexpr uint32_t FRAME_BYTES = W * H * BYTES_PER_PIXEL;
constexpr uint32_t NS_PER_US = 1000;
constexpr uint32_t US_PER_MS = 1000;
// RGB565 colors, high byte first on the bus
constexpr uint16_t BLACK = 0x0000;
constexpr uint16_t WHITE = 0xFFFF;
constexpr uint16_t RED = 0xF800;
constexpr uint16_t GREEN = 0x07E0;
constexpr uint16_t BLUE = 0x001F;
constexpr uint16_t YELLOW = 0xFFE0;
constexpr uint16_t CYAN = 0x07FF;
constexpr uint16_t MAGENTA = 0xF81F;
constexpr uint16_t RED_MAX = 31;   // RGB565: five bits of red and of blue, six of green
constexpr uint16_t GREEN_MAX = 63;
constexpr uint16_t BLUE_MAX = 31;
constexpr uint8_t RED_SHIFT = 11;
constexpr uint8_t GREEN_SHIFT = 5;

// rectangles: a rectangle's bus time is its bytes at 83.3 ns, the whole screen's FRAME_BUS_US
// spread over its bytes. The function runs no earlier than that and no later than the start of a
// rectangle, a few microseconds, the 3 µs wait after the last burst and the interrupts' delay.
constexpr uint32_t LATE_US = 200;
constexpr uint32_t DONE_TIMEOUT_MS = 100;   // far longer than the whole screen's bus time
struct Rect {
    uint16_t x, y, width, height;
};
constexpr Rect RECTS[] = {{0, 0, 320, 240}, {0, 0, 16, 1},        {304, 239, 16, 1},
                          {16, 32, 64, 48}, {160, 120, 160, 120}, {48, 200, 32, 40}};
// refills: how long each half runs, and how much slower than the bus a frame may arrive
constexpr uint32_t REFILLS_QUICK_MS = 3000;
constexpr uint32_t REFILLS_SOAK_MS = 30000;
constexpr uint32_t FRAME_SLACK_PERMILLE = 50;
constexpr uint32_t TURNS = 10;        // the idle and sending halves of refills, in turn
constexpr uint32_t TIMED_MS = 1000;   // the run that counts the handler's clocks
constexpr uint32_t PASS_WORK = 600;   // the nops of one pass of the counting loop
constexpr uint32_t BAND_ROWS = 48;    // five bands make the screen
constexpr uint32_t BANDS = H / BAND_ROWS;
constexpr uint32_t PERMILLE = 1000;
// restart-steps: the panel's 120 ms after the reset and 120 ms after Sleep Out, plus up to two
// display ticks of 40 ms each before a step is taken, and a reserve
constexpr uint32_t RESTART_MIN_MS = 240;
constexpr uint32_t RESTART_MAX_MS = 400;
constexpr uint32_t RESTART_TIMEOUT_MS = 2000;
// with-lighting: chain F, which has no LEDs at the bench, at its longest, one frame every lamps'
// frame period
constexpr uint8_t BENCH_CHAIN = Lighting::CHAINS - 1;
constexpr uint32_t LAMPS_FRAME_MS = 11;
constexpr uint32_t LIGHTING_QUICK_MS = 3000;
constexpr uint32_t LIGHTING_SOAK_MS = 30000;
// picture
constexpr uint32_t ASK_TIMEOUT_MS = 120000;
constexpr uint8_t GLYPH_W = 5;   // the letters of TOP LEFT, five columns of seven rows each
constexpr uint8_t GLYPH_H = 7;
constexpr uint8_t GLYPH_SCALE = 4;
constexpr uint8_t TEXT_MARGIN = 8;
constexpr uint32_t BAR_ROWS = 120;
// the longest each test takes, with room to spare
constexpr uint32_t QUICK_LIMIT_S = 20;
constexpr uint32_t REFILLS_SOAK_LIMIT_S = 90;
constexpr uint32_t ASK_LIMIT_S = 150;

static Display display;
static bool begun = false;
// the programs' shared PSRAM
static_assert(2 * FRAME_BYTES <= test::SCRATCH_BYTES, "both screens fit");
static uint8_t* const frame = test::scratch();
// refills: a band of the screen in RAM2, where the screen component keeps LVGL's buffers
DMAMEM static uint8_t band[BAND_ROWS * W * BYTES_PER_PIXEL] __attribute__((aligned(32)));
static uint8_t* const second = test::scratch() + FRAME_BYTES;

static volatile uint32_t doneCount = 0;
static volatile uint32_t doneCycles = 0;
static volatile uint32_t secondDone = 0;
static volatile uint32_t secondCycles = 0;

static uint32_t cyclesToUs(uint32_t cycles) {
    return uint32_t(uint64_t(cycles) * 1000000u / F_CPU_ACTUAL);
}

// Gets the bus time of n bytes in nanoseconds.
static uint32_t busNs(uint32_t bytes) {
    return uint32_t(uint64_t(Display::FRAME_BUS_US) * NS_PER_US * bytes / FRAME_BYTES);
}

// Begins the display once after the start. Reports a failure and returns false where it fails.
static bool ready() {
    if (!begun) {
        begun = display.begin();
        if (!begun) {
            test::fail("begin() false", "true");
        }
    }
    return begun;
}

static void put(uint8_t* buffer, uint32_t x, uint32_t y, uint16_t color) {
    uint8_t* p = buffer + (y * W + x) * BYTES_PER_PIXEL;
    p[0] = uint8_t(color >> 8);
    p[1] = uint8_t(color);
}

static void fill(uint8_t* buffer, uint32_t bytes, uint16_t color) {
    for (uint32_t i = 0; i < bytes; i += BYTES_PER_PIXEL) {
        buffer[i] = uint8_t(color >> 8);
        buffer[i + 1] = uint8_t(color);
    }
}

static void onDone() {
    doneCycles = ARM_DWT_CYCCNT;
    doneCount = doneCount + 1;
}

static void onSecondDone() {
    secondCycles = ARM_DWT_CYCCNT;
    secondDone = secondDone + 1;
}

// Waits until the function of the rectangle sent last has run count times.
static bool waitDone(uint32_t count) {
    const uint32_t t0 = millis();
    while (doneCount < count) {
        if (millis() - t0 > DONE_TIMEOUT_MS) {
            return false;
        }
    }
    return true;
}

// Sends a whole screen of one color and waits until it is out.
static bool showColor(uint16_t color) {
    fill(frame, FRAME_BYTES, color);
    const uint32_t before = doneCount;
    return display.send(0, 0, W, H, frame, onDone) && waitDone(before + 1);
}

// ---- picture

// the letters T, O, P, L, E and F, one byte a row, the leftmost column in bit 4
struct Glyph {
    char letter;
    uint8_t rows[GLYPH_H];
};
constexpr Glyph GLYPHS[] = {
    {'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'P', {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}},
    {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}},
    {'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}},
    {'F', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}},
};

static void drawText(const char* text, uint32_t x0, uint32_t y0) {
    uint32_t x = x0;
    for (const char* c = text; *c != '\0'; ++c) {
        for (const Glyph& g : GLYPHS) {
            if (g.letter != *c) {
                continue;
            }
            for (uint32_t row = 0; row < GLYPH_H * GLYPH_SCALE; ++row) {
                for (uint32_t col = 0; col < GLYPH_W * GLYPH_SCALE; ++col) {
                    const bool on =
                        (g.rows[row / GLYPH_SCALE] >> (GLYPH_W - 1 - col / GLYPH_SCALE)) & 1;
                    if (on) {
                        put(frame, x + col, y0 + row, WHITE);
                    }
                }
            }
        }
        x += (GLYPH_W + 1) * GLYPH_SCALE;
    }
}

// Color bars over the upper half, ramps of red, green and blue below, and TOP LEFT at the top left.
static void testPicture() {
    if (!ready()) {
        return;
    }
    constexpr uint16_t BARS[] = {WHITE, YELLOW, CYAN, GREEN, MAGENTA, RED, BLUE, BLACK};
    constexpr uint32_t BAR_COUNT = sizeof BARS / sizeof BARS[0];
    const uint32_t rampRows = (H - BAR_ROWS) / 3;
    for (uint32_t y = 0; y < H; ++y) {
        for (uint32_t x = 0; x < W; ++x) {
            uint16_t c = BLACK;
            if (y < BAR_ROWS) {
                c = BARS[x * BAR_COUNT / W];
            } else if (y < BAR_ROWS + rampRows) {
                c = uint16_t(x * (RED_MAX + 1) / W << RED_SHIFT);
            } else if (y < BAR_ROWS + 2 * rampRows) {
                c = uint16_t(x * (GREEN_MAX + 1) / W << GREEN_SHIFT);
            } else {
                c = uint16_t(x * (BLUE_MAX + 1) / W);
            }
            put(frame, x, y, c);
        }
    }
    // The words stand on a black field, so they read on the white bar as well.
    for (uint32_t y = 0; y < GLYPH_H * GLYPH_SCALE + 2 * TEXT_MARGIN; ++y) {
        for (uint32_t x = 0; x < 9 * (GLYPH_W + 1) * GLYPH_SCALE; ++x) {
            put(frame, x, y, BLACK);
        }
    }
    drawText("TOP LEFT", TEXT_MARGIN, TEXT_MARGIN);
    const uint32_t before = doneCount;
    if (!display.send(0, 0, W, H, frame, onDone) || !waitDone(before + 1)) {
        test::fail("the picture did not go out", "sent");
        return;
    }
    const test::Answer a = test::ask(
        "Does the screen show eight bars white yellow cyan green magenta red blue black from the "
        "left, below them smooth ramps of red, green and blue from dark at the left, and TOP LEFT "
        "upright at the top left?",
        ASK_TIMEOUT_MS);
    if (a == test::Answer::YES) {
        test::pass();
    } else {
        test::fail(a == test::Answer::NO ? "answered no" : "no answer", "yes");
    }
}

// ---- rectangles

static void testRectangles() {
    char seen[96];
    char wanted[64];
    if (!ready()) {
        return;
    }
    fill(frame, FRAME_BYTES, BLUE);
    for (const Rect& r : RECTS) {
        const uint32_t before = doneCount;
        const uint32_t t0 = ARM_DWT_CYCCNT;
        if (!display.send(r.x, r.y, r.width, r.height, frame, onDone)) {
            snprintf(seen, sizeof seen, "%u, %u, %u by %u refused", r.x, r.y, r.width, r.height);
            test::fail(seen, "sent");
            return;
        }
        if (!waitDone(before + 1)) {
            snprintf(seen, sizeof seen, "%u by %u: no call", r.width, r.height);
            test::fail(seen, "one call");
            return;
        }
        // A second call would arrive meanwhile.
        delay(1);
        const uint32_t tookUs = cyclesToUs(doneCycles - t0);
        const uint32_t wantUs = busNs(uint32_t(r.width) * r.height * BYTES_PER_PIXEL) / NS_PER_US;
        Serial.printf("%u, %u, %u by %u: %lu us, its bus time %lu us\n", r.x, r.y, r.width,
                      r.height, tookUs, wantUs);
        if (doneCount != before + 1) {
            snprintf(seen, sizeof seen, "%lu calls", doneCount - before);
            test::fail(seen, "one call");
            return;
        }
        if (tookUs < wantUs || tookUs > wantUs + LATE_US) {
            snprintf(seen, sizeof seen, "%u by %u: the call after %lu us", r.width, r.height,
                     tookUs);
            snprintf(wanted, sizeof wanted, "%lu to %lu us", wantUs, wantUs + LATE_US);
            test::fail(seen, wanted);
            return;
        }
    }
    const bool offSteps = display.send(8, 0, 16, 16, frame, onDone);
    const bool beyond =
        display.send(W - Display::STEP_PX, 0, 2 * Display::STEP_PX, 1, frame, onDone) ||
        display.send(0, H - 1, Display::STEP_PX, 2, frame, onDone);
    const uint32_t before = doneCount;
    const bool first = display.send(0, 0, W, H, frame, onDone);
    const bool during = display.send(0, 0, Display::STEP_PX, 1, frame, onDone);
    waitDone(before + 1);
    if (offSteps || beyond || !first || during) {
        snprintf(seen, sizeof seen, "off the steps %d, beyond %d, the screen %d, during it %d",
                 offSteps, beyond, first, during);
        test::fail(seen, "0, 0, 1, 0");
        return;
    }
    test::pass();
}

// ---- refills

// The driver's FlexIO3 handler, wrapped by one that counts the processor clocks of each run.
static void (*driverHandler)() = nullptr;
static volatile uint32_t handlerRuns = 0;
static volatile uint64_t handlerClocks = 0;
static volatile uint32_t handlerMaxClocks = 0;

static void timedHandler() {
    const uint32_t t0 = ARM_DWT_CYCCNT;
    driverHandler();
    const uint32_t clocks = ARM_DWT_CYCCNT - t0;
    handlerClocks = handlerClocks + clocks;
    if (clocks > handlerMaxClocks) {
        handlerMaxClocks = clocks;
    }
    handlerRuns = handlerRuns + 1;
}

// Counts the passes of a loop that sends the next band of the screen whenever the last one is out,
// or only asks whether it is out.
static uint32_t countPasses(bool sending, uint32_t ms, uint32_t& frames) {
    uint32_t passes = 0;
    uint32_t sentBands = 0;
    const uint32_t start = doneCount;
    const uint32_t t0 = millis();
    while (millis() - t0 < ms) {
        ++passes;
        // A fixed piece of work, about a microsecond, outweighs the check below, which costs more
        // while sending than while idle.
        for (uint32_t i = 0; i < PASS_WORK; ++i) {
            asm volatile("nop");
        }
        if (sending && doneCount - start == sentBands &&
            display.send(0, uint16_t(sentBands % BANDS * BAND_ROWS), W, BAND_ROWS, band, onDone)) {
            ++sentBands;
        }
    }
    waitDone(start + sentBands);
    frames = (doneCount - start) / BANDS;
    return passes;
}

static void testRefills() {
    char seen[96];
    char wanted[64];
    if (!ready()) {
        return;
    }
    fill(band, sizeof band, GREEN);
    const uint32_t ms = test::durationMs(REFILLS_QUICK_MS, REFILLS_SOAK_MS);
    // Idle and sending halves take turns, so a disturbance that comes and goes, such as USB,
    // falls on both alike.
    uint32_t frames = 0;
    uint32_t idle = 0;
    uint32_t busy = 0;
    for (uint32_t i = 0; i < TURNS; ++i) {
        uint32_t turnFrames = 0;
        idle += countPasses(false, ms / TURNS, turnFrames);
        busy += countPasses(true, ms / TURNS, turnFrames);
        frames += turnFrames;
    }
    // The wrapper's own clocks would count against the share, so it runs only afterwards.
    uint32_t timedFrames = 0;
    driverHandler = _VectorsRam[IRQ_FLEXIO3 + 16];
    attachInterruptVector(IRQ_FLEXIO3, timedHandler);
    countPasses(true, TIMED_MS, timedFrames);
    attachInterruptVector(IRQ_FLEXIO3, driverHandler);
    const uint32_t sharePermille =
        idle > busy ? uint32_t(uint64_t(idle - busy) * PERMILLE / idle) : 0;
    Serial.printf("the handler ran %lu times, %lu clocks on average, %lu at most\n", handlerRuns,
                  handlerRuns > 0 ? uint32_t(handlerClocks / handlerRuns) : 0, handlerMaxClocks);
    // The bound rounds up, so a share that equals the figure passes.
    const uint32_t estimatePermille =
        (Display::REFILL_CLOCKS * PERMILLE + Display::BURST_CLOCKS - 1) / Display::BURST_CLOCKS;
    const uint32_t frameUs = frames > 0 ? ms * US_PER_MS / frames : 0;
    const uint32_t frameMaxUs =
        Display::FRAME_BUS_US * (PERMILLE + FRAME_SLACK_PERMILLE) / PERMILLE;
    Serial.printf("passes idle %lu, sending %lu: %lu per mille of the processor; %lu frames, "
                  "%lu us each\n",
                  idle, busy, sharePermille, frames, frameUs);
    if (sharePermille > estimatePermille) {
        snprintf(seen, sizeof seen, "%lu per mille", sharePermille);
        snprintf(wanted, sizeof wanted, "%lu per mille at most", estimatePermille);
        test::fail(seen, wanted);
        return;
    }
    if (frames == 0 || frameUs > frameMaxUs) {
        snprintf(seen, sizeof seen, "%lu us a frame", frameUs);
        snprintf(wanted, sizeof wanted, "%lu us at most", frameMaxUs);
        test::fail(seen, wanted);
        return;
    }
    test::pass();
}

// ---- restart-steps

static void testRestartSteps() {
    char seen[96];
    char wanted[64];
    if (!ready() || !showColor(BLUE)) {
        if (begun) {
            test::fail("the blue screen did not go out", "sent");
        }
        return;
    }
    fill(frame, FRAME_BYTES, RED);
    fill(second, FRAME_BYTES, GREEN);
    const uint32_t before = doneCount;
    const uint32_t t0 = ARM_DWT_CYCCNT;
    if (!display.send(0, 0, W, H, frame, onDone)) {
        test::fail("the red screen refused", "sent");
        return;
    }
    display.restart();
    const bool heldAtOnce = display.restarting();
    if (!waitDone(before + 1)) {
        test::fail("the red screen on its way did not finish", "it finishes");
        return;
    }
    const uint32_t redUs = cyclesToUs(doneCycles - t0);
    const uint32_t secondBefore = secondDone;
    const bool accepted = display.send(0, 0, W, H, second, onSecondDone);
    const uint32_t restartStart = millis();
    while (display.restarting() && millis() - restartStart < RESTART_TIMEOUT_MS) {
        if (secondDone != secondBefore) {
            break;
        }
    }
    const uint32_t restartMs = millis() - restartStart;
    const bool doneDuring = secondDone != secondBefore && display.restarting();
    const uint32_t backCycles = ARM_DWT_CYCCNT;
    const uint32_t t1 = millis();
    while (secondDone == secondBefore && millis() - t1 < DONE_TIMEOUT_MS) {
    }
    const int32_t afterBackUs = int32_t(cyclesToUs(secondCycles - backCycles));
    Serial.printf("restarting at once %d, red out after %lu us, the restart %lu ms, green out "
                  "%ld us after it\n",
                  heldAtOnce, redUs, restartMs, afterBackUs);
    if (!heldAtOnce || !accepted || doneDuring) {
        snprintf(seen, sizeof seen, "restarting %d, green accepted %d, green out during it %d",
                 heldAtOnce, accepted, doneDuring);
        test::fail(seen, "1, 1, 0");
        return;
    }
    if (restartMs < RESTART_MIN_MS || restartMs > RESTART_MAX_MS) {
        snprintf(seen, sizeof seen, "a restart of %lu ms", restartMs);
        snprintf(wanted, sizeof wanted, "%lu to %lu ms", RESTART_MIN_MS, RESTART_MAX_MS);
        test::fail(seen, wanted);
        return;
    }
    if (secondDone != secondBefore + 1) {
        test::fail("green did not go out after the restart", "one call");
        return;
    }
    test::pass();
}

// ---- with-lighting

static Lighting lighting;

// Sends a lighting frame every lamps' frame period for ms while the display sends the screen in
// bands without a pause, and counts the frames the lighting refused because the last was not out.
static uint32_t countRefusals(uint32_t ms, uint32_t& shows, uint32_t& frames) {
    uint32_t refused = 0;
    uint32_t sentBands = 0;
    const uint32_t start = doneCount;
    const uint32_t t0 = millis();
    uint32_t nextShow = t0;
    shows = 0;
    while (millis() - t0 < ms) {
        if (int32_t(millis() - nextShow) >= 0) {
            nextShow += LAMPS_FRAME_MS;
            ++shows;
            if (!lighting.show()) {
                ++refused;
            }
        }
        if (doneCount - start == sentBands &&
            display.send(0, uint16_t(sentBands % BANDS * BAND_ROWS), W, BAND_ROWS, band, onDone)) {
            ++sentBands;
        }
    }
    waitDone(start + sentBands);
    frames = (doneCount - start) / BANDS;
    return refused;
}

static void testWithLighting() {
    char seen[96];
    char wanted[64];
    if (!ready()) {
        return;
    }
    Lighting::Settings settings;
    settings.leds[BENCH_CHAIN] = Lighting::MAX_CHAIN_LEDS;
    lighting.configure(settings);
    if (!lighting.begin()) {
        test::fail("the lighting's begin() false beside the display", "true");
        return;
    }
    fill_solid(lighting.chain(BENCH_CHAIN), lighting.leds(BENCH_CHAIN), CRGB::Black);
    fill(band, sizeof band, CYAN);
    uint32_t shows = 0;
    uint32_t frames = 0;
    const uint32_t ms = test::durationMs(LIGHTING_QUICK_MS, LIGHTING_SOAK_MS);
    const uint32_t refused = countRefusals(ms, shows, frames);
    const uint32_t frameUs = frames > 0 ? ms * US_PER_MS / frames : 0;
    const uint32_t frameMaxUs =
        Display::FRAME_BUS_US * (PERMILLE + FRAME_SLACK_PERMILLE) / PERMILLE;
    Serial.printf("%lu lighting frames, %lu refused; %lu screens, %lu us each\n", shows, refused,
                  frames, frameUs);
    if (refused != 0) {
        snprintf(seen, sizeof seen, "%lu of %lu lighting frames refused", refused, shows);
        test::fail(seen, "none");
        return;
    }
    if (frames == 0 || frameUs > frameMaxUs) {
        snprintf(seen, sizeof seen, "%lu us a screen beside the lighting", frameUs);
        snprintf(wanted, sizeof wanted, "%lu us at most", frameMaxUs);
        test::fail(seen, wanted);
        return;
    }
    test::pass();
}

static const Test TESTS[] = {
    {"display.picture", RunsOn::BENCH, ASK_LIMIT_S, ASK_LIMIT_S, testPicture, nullptr},
    {"display.rectangles", RunsOn::TEENSY, QUICK_LIMIT_S, QUICK_LIMIT_S, testRectangles, nullptr},
    {"display.refills", RunsOn::TEENSY, QUICK_LIMIT_S, REFILLS_SOAK_LIMIT_S, testRefills, nullptr},
    {"display.restart-steps", RunsOn::TEENSY, QUICK_LIMIT_S, QUICK_LIMIT_S, testRestartSteps,
     nullptr},
    {"display.with-lighting", RunsOn::TEENSY, QUICK_LIMIT_S, REFILLS_SOAK_LIMIT_S, testWithLighting,
     nullptr},
};

extern const Program DISPLAY_PROGRAM = {TESTS, sizeof TESTS / sizeof TESTS[0]};
