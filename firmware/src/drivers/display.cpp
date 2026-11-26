#include "display.h"
#include "core/clock.h"
#include "logging/debug-log.h"
#include "logging/debug-trace.h"
#include "logging/logger.h"
#include <atomic>
#include <string.h>
#include <utility>

namespace {

constexpr uint8_t DATA_PINS[] = {22, 23, 20, 21, 38, 39, 26, 27};   // pin-assignment.md: D0 to D7
constexpr uint8_t WR_PIN = 17;             // pin-assignment.md: WR to the display
constexpr uint8_t CS_PIN = 28;             // pin-assignment.md: CS to the display controller
constexpr uint8_t DC_PIN = 29;             // pin-assignment.md: D/C to the display
constexpr uint8_t FIRST_DATA_SIGNAL = 8;   // pin-assignment.md: D0 is FlexIO3's signal 8
constexpr uint8_t WR_SIGNAL = 6;           // pin-assignment.md: WR is FlexIO3's signal 6

/// RM IOMUXC, SW_MUX_CTL_PAD_GPIO_AD_B1: ALT9 is FlexIO3
constexpr uint32_t MUX_FLEXIO3 = 9;
/// design.md, Bus: the weakest drive, DSE 001, and the slow slew rate
constexpr uint32_t PAD_WEAK = IOMUXC_PAD_DSE(1) | IOMUXC_PAD_SPEED(0);

/// RM CCM, CSCMR2 and CS1CDR: FLEXIO2_CLK_ROOT, which clocks FlexIO3 too, from PLL3 at 480 MHz
/// divided by 2 and by 2, which gives 120 MHz
constexpr uint32_t CLK_SEL_PLL3 = 3;
constexpr uint32_t CLK_PRED_2 = 1;
constexpr uint32_t CLK_PODF_2 = 1;
constexpr uint32_t CLK_SEL_MASK = CCM_CSCMR2_FLEXIO2_CLK_SEL(3);
constexpr uint32_t CLK_DIV_MASK = CCM_CS1CDR_FLEXIO2_CLK_PRED(7) | CCM_CS1CDR_FLEXIO2_CLK_PODF(7);
constexpr uint32_t CLK_SEL = CCM_CSCMR2_FLEXIO2_CLK_SEL(CLK_SEL_PLL3);
constexpr uint32_t CLK_DIV =
    CCM_CS1CDR_FLEXIO2_CLK_PRED(CLK_PRED_2) | CCM_CS1CDR_FLEXIO2_CLK_PODF(CLK_PODF_2);

constexpr uint32_t DIVIDER = 10;   // design.md: the FlexIO3 clocks of one write, 83.3 ns
constexpr uint8_t SHIFTERS = 8;    // RM FlexIO: FlexIO3's shifters, chained into one burst
constexpr uint8_t LAST_SHIFTER = SHIFTERS - 1;
constexpr uint32_t LAST_FLAG = 1u << LAST_SHIFTER;
constexpr uint32_t BURST_BYTES = 32;   // design.md: one burst, eight shifters of four bytes
constexpr uint32_t BYTES_PER_PIXEL = 2;
constexpr uint8_t PIXELS_PER_WORD = sizeof(uint32_t) / BYTES_PER_PIXEL;   // two in a shifter
constexpr uint32_t DATA_LINES = 8;   // D0 to D7, one byte a write
/// RM FlexIO, TIMCMP in 8-bit baud mode: twice the writes less one in the upper byte, half the
/// divider less one in the lower
constexpr uint32_t BURST_COMPARE = (BURST_BYTES * 2 - 1) << 8 | (DIVIDER / 2 - 1);
constexpr uint32_t SINGLE_COMPARE = (1 * 2 - 1) << 8 | (DIVIDER / 2 - 1);
/// RM FlexIO, TIMCFG: a burst starts on its trigger and ends on the compare
constexpr uint32_t TIMER_CFG = FLEXIO_TIMCFG_TIMDIS(2) | FLEXIO_TIMCFG_TIMENA(2);
/// RM FlexIO, TIMCTL: TRGSEL 4 * N + 1 picks shifter N's status flag
constexpr uint32_t STATUS_TRIGGER = 1;
constexpr uint32_t PIN_OUT = 3;    // RM FlexIO, PINCFG: the pin is an output
constexpr uint32_t TRANSMIT = 2;   // RM FlexIO, SMOD: the shifter transmits
constexpr uint32_t TIMER0_FLAG = 1;

constexpr uint8_t PRIORITY = 224;          // general-design.md: the display's row
constexpr uint32_t TICK_US = 40000;        // design.md: the display's tick, which steps a restart
constexpr uint32_t PANEL_READY_MS = 285;   // design.md: the driver's first command after power-up
constexpr uint32_t RESET_MS = 120;   // ILI9341_DS_V1.11, 01h: after a software reset in Sleep Out
constexpr uint32_t SLEEP_OUT_MS = 120;   // design.md: from Sleep Out to running
/// design.md: from the start of the last burst until it has left the bus, with a reserve
constexpr uint32_t END_WAIT_NS = 3000;
/// RM FlexIO, CTRL: FlexIO3 takes no register write until its software reset has cleared in the
/// FlexIO3 clock. The manual gives no time, so the driver holds the reset this long and waits this
/// long after it.
constexpr uint32_t FLEXIO_RESET_NS = 1000;
/// ILI9341_DS_V1.11, 18.3.1: CS setup 15 ns and CS wait 10 ns at the most, D/C setup and hold 0 ns,
/// plus the 4.8 ns skew of the module's buffers; 50 ns leaves a reserve
constexpr uint32_t SETTLE_NS = 50;
/// The longest a write waits for FlexIO3, a hundred writes, so a FlexIO3 that never answers
/// cannot hang the processor.
constexpr uint32_t WAIT_LIMIT_NS = 10000;
constexpr uint32_t NS_PER_US = 1000;

// ILI9341_DS_V1.11, 8.2 and 8.3
constexpr uint8_t SOFTWARE_RESET = 0x01;
constexpr uint8_t SLEEP_OUT = 0x11;
constexpr uint8_t DISPLAY_ON = 0x29;
constexpr uint8_t COLUMN_ADDRESS = 0x2A;
constexpr uint8_t PAGE_ADDRESS = 0x2B;
constexpr uint8_t MEMORY_WRITE = 0x2C;
constexpr uint8_t MEMORY_ACCESS = 0x36;
constexpr uint8_t PIXEL_FORMAT = 0x3A;
constexpr uint8_t RGB565 = 0x55;   // 8.2.33: 16 bits a pixel
/// 8.2.29: a BGR panel, its memory addressed as the panel lies, 240 columns by 320 pages. The
/// panel builds its picture page by page, and a page is a column of the screen in landscape.
constexpr uint8_t PANEL_ORDER = 0x08;
constexpr uint8_t WORD_BYTES = 2;   // an address of CASET or PASET, high byte first
constexpr uint8_t BYTE_BITS = 8;

/// The power, frame rate and gamma setup the ILI9341 libraries send after a reset, each command
/// followed by its parameter count and its parameters, as Adafruit_ILI9341.cpp's initcmd holds it.
/// The panel keeps every value through a software reset, so the driver sends them after each one.
constexpr uint8_t SET_UP[] = {
    0xEF, 3,    0x03, 0x80, 0x02,                                 // undocumented, as the libraries
    0xCF, 3,    0x00, 0xC1, 0x30,                                 // power control B
    0xED, 4,    0x64, 0x03, 0x12, 0x81,                           // power on sequence control
    0xE8, 3,    0x85, 0x00, 0x78,                                 // driver timing control A
    0xCB, 5,    0x39, 0x2C, 0x00, 0x34, 0x02,                     // power control A
    0xF7, 1,    0x20,                                             // pump ratio control
    0xEA, 2,    0x00, 0x00,                                       // driver timing control B
    0xC0, 1,    0x23,                                             // power control 1
    0xC1, 1,    0x10,                                             // power control 2
    0xC5, 2,    0x3E, 0x28,                                       // VCOM control 1
    0xC7, 1,    0x86,                                             // VCOM control 2
    0x37, 1,    0x00,                                             // vertical scroll start 0
    0xB1, 2,    0x00, 0x18,                                       // frame rate 79 Hz
    0xB6, 3,    0x08, 0x82, 0x27,                                 // display function control
    0xF2, 1,    0x00,                                             // 3-gamma off
    0x26, 1,    0x01,                                             // gamma curve 1
    0xE0, 15,   0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1,   // positive gamma
    0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00,                     //
    0xE1, 15,   0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1,   // negative gamma
    0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F,                     //
};

IMXRT_FLEXIO_t& io() {
    return IMXRT_FLEXIO3_S;
}

/// Gets whether PLL3 runs at 480 MHz and reaches PLL3_SW_CLK, the source the root takes.
///
/// RM CCM_ANALOG, PLL_USB1: PLL3 runs at 20 times the 24 MHz crystal while it is powered,
/// enabled, locked and not bypassed, with DIV_SELECT 0. RM CCM, CCSR: PLL3_SW_CLK follows PLL3
/// while PLL3_SW_CLK_SEL is 0.
bool pll3At480() {
    constexpr uint32_t RUNNING =
        CCM_ANALOG_PLL_USB1_POWER | CCM_ANALOG_PLL_USB1_ENABLE | CCM_ANALOG_PLL_USB1_LOCK;
    const uint32_t pll = CCM_ANALOG_PLL_USB1;
    return (pll & RUNNING) == RUNNING && (pll & CCM_ANALOG_PLL_USB1_BYPASS) == 0 &&
           (pll & CCM_ANALOG_PLL_USB1_DIV_SELECT) == 0 &&
           (CCM_CCSR & CCM_CCSR_PLL3_SW_CLK_SEL) == 0;
}

bool rootAt120() {
    return (CCM_CSCMR2 & CLK_SEL_MASK) == CLK_SEL && (CCM_CS1CDR & CLK_DIV_MASK) == CLK_DIV;
}

/// Brings the clock root FlexIO3 shares with FlexIO2 to 120 MHz, which DIVIDER is computed for.
///
/// @return  False, with nothing changed, when PLL3 does not run at 480 MHz, or when the root runs
///          at another rate while FlexIO2 is enabled, since a change would move FlexIO2's rate.
bool clockAt120() {
    if (!pll3At480()) {
        logWarning("display", "PLL3 does not run at 480 MHz");
        return false;
    }
    if (rootAt120()) {
        return true;
    }
    const bool flexio2Clocked = (CCM_CCGR3 & CCM_CCGR3_FLEXIO2(CCM_CCGR_ON)) != 0;
    if (flexio2Clocked && (FLEXIO2_CTRL & FLEXIO_CTRL_FLEXEN) != 0) {
        logWarning("display", "FlexIO2 runs on the shared clock root at another rate");
        return false;
    }
    // RM CCM: the gate stays off while the root changes.
    CCM_CCGR7 &= ~CCM_CCGR7_FLEXIO3(CCM_CCGR_ON);
    CCM_CSCMR2 = (CCM_CSCMR2 & ~CLK_SEL_MASK) | CLK_SEL;
    CCM_CS1CDR = (CCM_CS1CDR & ~CLK_DIV_MASK) | CLK_DIV;
    (void)CCM_CS1CDR;
    CCM_CCGR7 |= CCM_CCGR7_FLEXIO3(CCM_CCGR_ON);
    return rootAt120();
}

/// Hands the bus pins to FlexIO3 with CS high, so the panel ignores what the pins do meanwhile.
void setUpPins() {
    pinMode(CS_PIN, OUTPUT);
    pinMode(DC_PIN, OUTPUT);
    digitalWriteFast(CS_PIN, HIGH);
    digitalWriteFast(DC_PIN, HIGH);
    *portControlRegister(CS_PIN) = PAD_WEAK;
    *portControlRegister(DC_PIN) = PAD_WEAK;
    for (const uint8_t pin : DATA_PINS) {
        *portControlRegister(pin) = PAD_WEAK;
        *portConfigRegister(pin) = MUX_FLEXIO3;
    }
    *portControlRegister(WR_PIN) = PAD_WEAK;
    *portConfigRegister(WR_PIN) = MUX_FLEXIO3;
}

void settle() {
    delayNanoseconds(SETTLE_NS);
}

/// Sets FlexIO3 up for single writes or for bursts of 32.
///
/// A single write takes shifter 0 alone and starts when its buffer is written. A burst chains the
/// eight shifters into one buffer of 32 bytes, shifter 0 first, and starts when shifter 7's buffer
/// is written. Timer 0 drives WR low for half a write and high for the other half, and the data
/// changes as WR falls. FlexIO3 lets go of the pins while it is set up, so CS has to be high.
///
/// @param burst  True for bursts, false for single writes.
void configure(bool burst) {
    IMXRT_FLEXIO_t& f = io();
    // With the two writes of the reset in a row and the setup right behind them, the panel missed
    // commands on the bench and stayed asleep after a restart.
    f.CTRL = FLEXIO_CTRL_SWRST;
    delayNanoseconds(FLEXIO_RESET_NS);
    f.CTRL = 0;
    delayNanoseconds(FLEXIO_RESET_NS);
    const uint8_t shifters = burst ? SHIFTERS : 1;
    for (uint8_t i = 0; i < shifters; ++i) {
        // RM FlexIO, SHIFTCFG: INSRC takes each shifter's input from the next one, and PWIDTH
        // shifts eight bits at a time onto D0 to D7.
        f.SHIFTCFG[i] = FLEXIO_SHIFTCFG_INSRC | FLEXIO_SHIFTCFG_PWIDTH(DATA_LINES - 1);
        f.SHIFTCTL[i] =
            FLEXIO_SHIFTCTL_TIMSEL(0) | FLEXIO_SHIFTCTL_SMOD(TRANSMIT) |
            (i == 0 ? FLEXIO_SHIFTCTL_PINCFG(PIN_OUT) | FLEXIO_SHIFTCTL_PINSEL(FIRST_DATA_SIGNAL)
                    : 0);
    }
    const uint32_t trigger = burst ? LAST_SHIFTER : 0;
    f.TIMCMP[0] = burst ? BURST_COMPARE : SINGLE_COMPARE;
    f.TIMCFG[0] = TIMER_CFG;
    // RM FlexIO, TIMCTL: the trigger is active while the shifter's buffer holds data, and WR is
    // active low.
    f.TIMCTL[0] = FLEXIO_TIMCTL_TRGSEL(trigger << 2 | STATUS_TRIGGER) | FLEXIO_TIMCTL_TRGPOL |
                  FLEXIO_TIMCTL_TRGSRC | FLEXIO_TIMCTL_PINCFG(PIN_OUT) |
                  FLEXIO_TIMCTL_PINSEL(WR_SIGNAL) | FLEXIO_TIMCTL_PINPOL | FLEXIO_TIMCTL_TIMOD(1);
    f.CTRL = FLEXIO_CTRL_FLEXEN;
}

/// The waits for FlexIO3 that ran past WAIT_LIMIT_NS since the start.
std::atomic<uint32_t> stalls{0};

/// Waits until a FlexIO3 flag is set.
///
/// A wait that runs past WAIT_LIMIT_NS is counted in stalls and noted in the debug trace.
///
/// @return  False when WAIT_LIMIT_NS passed without it.
bool waitFor(volatile uint32_t& flags, uint32_t mask) {
    const uint32_t t0 = ARM_DWT_CYCCNT;
    const uint32_t limit = F_CPU_ACTUAL / NS_PER_US * WAIT_LIMIT_NS / NS_PER_US;
    while ((flags & mask) == 0) {
        if (ARM_DWT_CYCCNT - t0 > limit) {
            stalls.fetch_add(1, std::memory_order_relaxed);
            DEBUG_TRACE("display stall", int32_t(mask), 0);
            return false;
        }
    }
    return true;
}

/// Writes one byte and waits until it has left the bus, so D/C may change afterwards.
void writeByte(uint8_t byte) {
    IMXRT_FLEXIO_t& f = io();
    f.TIMSTAT = TIMER0_FLAG;
    f.SHIFTBUF[0] = byte;
    waitFor(f.TIMSTAT, TIMER0_FLAG);
}

void command(uint8_t cmd, const uint8_t* params = nullptr, uint8_t count = 0) {
    digitalWriteFast(DC_PIN, LOW);
    settle();
    writeByte(cmd);
    digitalWriteFast(DC_PIN, HIGH);
    settle();
    for (uint8_t i = 0; i < count; ++i) {
        writeByte(params[i]);
    }
}

void select() {
    digitalWriteFast(CS_PIN, LOW);
    settle();
}

void deselect() {
    settle();
    digitalWriteFast(CS_PIN, HIGH);
}

/// Sets FlexIO3 up for single writes and selects the panel for a command.
void commandMode() {
    deselect();
    configure(false);
    select();
}

/// Sets FlexIO3 up for bursts and selects the panel for the pixels of a memory write.
void burstMode() {
    deselect();
    configure(true);
    digitalWriteFast(DC_PIN, HIGH);
    select();
}

/// Loads one burst into the shifters, shifter 7 last, since its buffer starts the burst.
///
/// @param words  The burst's 32 bytes, four to a shifter, in the order they go out.
void loadBurst(const uint32_t* words) {
    IMXRT_FLEXIO_t& f = io();
    for (uint8_t i = 0; i < SHIFTERS; ++i) {
        f.SHIFTBUF[i] = words[i];
    }
}

/// Sends an address range of CASET or PASET, first and last, each high byte first.
void addressRange(uint8_t cmd, uint16_t first, uint16_t last) {
    const uint8_t range[2 * WORD_BYTES] = {uint8_t(first >> BYTE_BITS), uint8_t(first),
                                           uint8_t(last >> BYTE_BITS), uint8_t(last)};
    command(cmd, range, sizeof range);
}

/// Opens the panel's memory for the rectangle and starts the memory write.
///
/// The screen's rows are the panel's columns and its columns the panel's pages, so the pixels that
/// follow run down one column of the screen after the other.
void window(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    addressRange(COLUMN_ADDRESS, y, uint16_t(y + height - 1));
    addressRange(PAGE_ADDRESS, x, uint16_t(x + width - 1));
    command(MEMORY_WRITE);
}

/// Sends what follows a software reset up to Sleep Out.
void setUpPanel() {
    for (size_t i = 0; i < sizeof SET_UP;) {
        const uint8_t count = SET_UP[i + 1];
        command(SET_UP[i], &SET_UP[i + 2], count);
        i += 2 + count;
    }
    command(PIXEL_FORMAT, &RGB565, 1);
    command(MEMORY_ACCESS, &PANEL_ORDER, 1);
    command(SLEEP_OUT);
}

/// Fills the whole panel with black, polling FlexIO3, since the panel's memory holds noise after
/// power-up and the driver keeps no buffer.
void blackScreen() {
    static const uint32_t black[SHIFTERS] = {};
    commandMode();
    window(0, 0, Display::WIDTH, Display::HEIGHT);
    burstMode();
    const uint32_t bursts =
        uint32_t(Display::WIDTH) * Display::HEIGHT * BYTES_PER_PIXEL / BURST_BYTES;
    for (uint32_t i = 0; i < bursts; ++i) {
        if (!waitFor(io().SHIFTSTAT, LAST_FLAG)) {
            break;
        }
        loadBurst(black);
        // The writes reach FlexIO3 only after a few of its clocks, and shifter 7's flag falls
        // then. Reading it earlier takes the burst before for this one.
        asm volatile("dsb" ::: "memory");
    }
    // The last burst waits in the buffers while the one before is on the bus. Shifter 7's flag
    // rises once it has started, and only then its time on the bus begins.
    waitFor(io().SHIFTSTAT, LAST_FLAG);
    delayNanoseconds(END_WAIT_NS);
    deselect();
}

}   // namespace

Display* Display::active = nullptr;

bool Display::begin() {
    if (begun) {
        return begunOk;
    }
    begun = true;
    if (!driverTick.attach([this] { tick(); }, TICK_US)) {
        logWarning("display", "the driver tick refused the display");
        return false;
    }
    if (!clockAt120()) {
        return false;
    }
    setUpPins();
    // The module's supervisor holds the panel in reset until then.
    while (millis() < PANEL_READY_MS) {
    }
    // A restart of the Teensy can find the panel awake, so a software reset comes first.
    commandMode();
    command(SOFTWARE_RESET);
    deselect();
    delay(RESET_MS);
    commandMode();
    setUpPanel();
    deselect();
    delay(SLEEP_OUT_MS);
    blackScreen();
    commandMode();
    command(DISPLAY_ON);
    deselect();

    active = this;
    attachInterruptVector(IRQ_FLEXIO3, serveActive);
    NVIC_SET_PRIORITY(IRQ_FLEXIO3, PRIORITY);
    NVIC_ENABLE_IRQ(IRQ_FLEXIO3);
    const uint32_t stalled = stalls.load(std::memory_order_relaxed);
    if (stalled != 0) {
        logWarning("display", "FlexIO3 stalled %lu times during the setup",
                   static_cast<unsigned long>(stalled));
    }
    begunOk = true;
    return true;
}

void Display::restart() {
    if (!begunOk) {
        DEBUG_LOG("Display::restart: no begin() yet\n");
        return;
    }
    DEBUG_LOG("display: the panel starts anew\n");
    restarts = restarts + 1;
    NVIC_SET_PENDING(IRQ_FLEXIO3);
}

bool Display::restarting() const {
    return begunOk && (restarts != restartsTaken || step != Step::IDLE);
}

bool Display::send(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t* pixels,
                   Sent sentFn) {
    if (!begunOk) {
        DEBUG_LOG("Display::send: no begin() yet\n");
        return false;
    }
    if (sending) {
        DEBUG_LOG("Display::send: a rectangle is still on its way\n");
        return false;
    }
    if (x % STEP_PX != 0 || width % STEP_PX != 0 || width == 0 || height == 0 ||
        uint32_t(x) + width > WIDTH || uint32_t(y) + height > HEIGHT || pixels == nullptr) {
        DEBUG_LOG("Display::send: %u, %u, %u by %u lies off the steps or the screen\n", x, y, width,
                  height);
        return false;
    }
    rectX = x;
    rectY = y;
    rectWidth = width;
    rectHeight = height;
    column = pixels;
    at = pixels;
    rowsLeft = height;
    // The width is whole bursts, so the rectangle is.
    left = uint32_t(width) * height * BYTES_PER_PIXEL / BURST_BYTES;
    sent = std::move(sentFn);
    // The interrupt reads the rectangle once it sees sending, so the rectangle is written first.
    std::atomic_signal_fence(std::memory_order_release);
    sending = true;
    NVIC_SET_PENDING(IRQ_FLEXIO3);
    return true;
}

void Display::serveActive() {
    active->serve();
}

/// Takes sixteen pixels down the rectangle's columns, which lie row by row in the caller's buffer,
/// and steps to the top of the next column at the end of one. A pixel's two bytes go out in the
/// order they lie, and a shifter sends its lowest byte first.
void Display::loadNext() {
    const uint32_t rowBytes = uint32_t(rectWidth) * BYTES_PER_PIXEL;
    const uint8_t* p = at;
    uint32_t rows = rowsLeft;
    uint32_t words[SHIFTERS];
    for (uint8_t i = 0; i < SHIFTERS; ++i) {
        uint32_t word = 0;
        for (uint8_t pixel = 0; pixel < PIXELS_PER_WORD; ++pixel) {
            uint16_t bytes = 0;
            memcpy(&bytes, p, sizeof bytes);
            word |= uint32_t(bytes) << (pixel * BYTES_PER_PIXEL * BYTE_BITS);
            p += rowBytes;
            rows = rows - 1;
            if (rows == 0) {
                column += BYTES_PER_PIXEL;
                p = column;
                rows = rectHeight;
            }
        }
        words[i] = word;
    }
    at = p;
    rowsLeft = rows;
    loadBurst(words);
    left = left - 1;
}

void Display::serve() {
    IMXRT_FLEXIO_t& f = io();
    if (!running) {
        // The panel's setup holds the bus until the panel is back.
        if (stepRestart() || !sending) {
            return;
        }
        commandMode();
        window(rectX, rectY, rectWidth, rectHeight);
        burstMode();
        running = true;
        loadNext();
        // Shifter 7's flag rises once a burst has moved from the buffers into the shifters, so the
        // next burst loads while this one is on the bus.
        f.SHIFTSIEN = LAST_FLAG;
        return;
    }
    if ((f.SHIFTSTAT & LAST_FLAG) == 0) {
        return;
    }
    if (left > 0) {
        loadNext();
        // The writes reach FlexIO3 only after a few of its clocks. Ending the interrupt earlier
        // finds shifter 7's flag still raised, which raises the interrupt a second time for
        // nothing.
        asm volatile("dsb" ::: "memory");
        return;
    }
    // The last burst has started. FlexIO3 raises no flag at its end that a late interrupt could
    // tell from the end of the burst before, so the interrupt waits out the burst.
    f.SHIFTSIEN = 0;
    delayNanoseconds(END_WAIT_NS);
    deselect();
    running = false;
    sending = false;
    if (sent) {
        sent();
    }
    if (restarts != restartsTaken) {
        NVIC_SET_PENDING(IRQ_FLEXIO3);
    }
}

/// Steps a restart between two rectangles: the software reset, then the setup once the panel's
/// 120 ms after the reset have passed, then Display On once its 120 ms after Sleep Out have passed.
/// The driver tick raises the interrupt while a step waits.
bool Display::stepRestart() {
    if (step == Step::IDLE) {
        if (restarts == restartsTaken) {
            return false;
        }
        restartsTaken = restarts;
        commandMode();
        command(SOFTWARE_RESET);
        deselect();
        stepUs = nowUs();
        step = Step::RESET;
        return true;
    }
    if (step == Step::RESET) {
        if (!elapsedMs(stepUs, RESET_MS)) {
            return true;
        }
        commandMode();
        setUpPanel();
        deselect();
        stepUs = nowUs();
        step = Step::SLEEP_OUT;
        return true;
    }
    if (!elapsedMs(stepUs, SLEEP_OUT_MS)) {
        return true;
    }
    commandMode();
    command(DISPLAY_ON);
    deselect();
    step = Step::IDLE;
    // A restart asked for meanwhile starts at once.
    if (restarts != restartsTaken) {
        NVIC_SET_PENDING(IRQ_FLEXIO3);
        return true;
    }
    return false;
}

void Display::tick() {
    if (step != Step::IDLE) {
        NVIC_SET_PENDING(IRQ_FLEXIO3);
    }
}
