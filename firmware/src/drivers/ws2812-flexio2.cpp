#include "ws2812-flexio2.h"
#include "logging/logger.h"
#include <DMAChannel.h>
#include <new>
#include "drivers/ws2812-color-encoder.h"

namespace {

constexpr uint8_t MAX_LANES = Ws2812Flexio2::MAX_LANES;

/// The pins that reach FlexIO2 on each board with the i.MX RT1062, and the FlexIO2 signal each
/// carries in ALT4. The pads GPIO_B0_00 to 15 carry signals 0 to 15 and GPIO_B1_00 to 15 signals
/// 16 to 31, i.MX RT1060 reference manual, IOMUXC. PJRC's core_pins.h names the pad behind each
/// pin of each board.
struct FlexioPin {
    uint8_t pin;
    uint8_t signal;
};
#if defined(ARDUINO_TEENSY41)
constexpr FlexioPin FLEXIO_PINS[] = {{6, 10},  {7, 17},  {8, 16}, {9, 11},  {10, 0},
                                     {11, 2},  {12, 1},  {13, 3}, {32, 12}, {34, 29},
                                     {35, 28}, {36, 18}, {37, 19}};
#elif defined(ARDUINO_TEENSY40)
constexpr FlexioPin FLEXIO_PINS[] = {{6, 10}, {7, 17}, {8, 16}, {9, 11}, {10, 0},
                                     {11, 2}, {12, 1}, {13, 3}, {32, 12}};
#elif defined(ARDUINO_TEENSY_MICROMOD)
constexpr FlexioPin FLEXIO_PINS[] = {{6, 10}, {7, 17}, {8, 16}, {9, 11},  {10, 0},
                                     {11, 2}, {12, 1}, {13, 3}, {32, 12}, {40, 4},
                                     {41, 5}, {42, 6}, {43, 7}, {44, 8},  {45, 9}};
#else
#error "ws2812-flexio2 knows the FlexIO2 pins of the Teensy 4.0, 4.1 and MicroMod only"
#endif
constexpr uint8_t DCHPRI_REGISTER = 0x1C;   // the bits of a channel that pick its register
constexpr uint8_t DCHPRI_BYTE = 0x03;       // the bits that pick its byte in the register
constexpr uint32_t ALT_FLEXIO2 = 4;   // RM IOMUXC, SW_MUX_CTL_PAD_GPIO_B0 and B1: ALT4 is FlexIO2
/// The pad's drive strength as pinMode(OUTPUT) sets it in PJRC's core, digital.c.
constexpr uint32_t PAD_OUTPUT = IOMUXC_PAD_DSE(7);

/// RM CCM, CSCMR2 and CS1CDR: FLEXIO2_CLK_ROOT from PLL3 at 480 MHz, divided by 2 and by 2, which
/// gives 120 MHz. The root clocks FlexIO3 as well, which the display drives at the same 120 MHz.
constexpr uint32_t CLK_SEL_PLL3 = 3;
constexpr uint32_t CLK_PRED_2 = 1;
constexpr uint32_t CLK_PODF_2 = 1;
constexpr uint32_t CLK_SEL_MASK = CCM_CSCMR2_FLEXIO2_CLK_SEL(3);
constexpr uint32_t CLK_DIV_MASK = CCM_CS1CDR_FLEXIO2_CLK_PRED(7) | CCM_CS1CDR_FLEXIO2_CLK_PODF(7);
constexpr uint32_t CLK_SEL = CCM_CSCMR2_FLEXIO2_CLK_SEL(CLK_SEL_PLL3);
constexpr uint32_t CLK_DIV =
    CCM_CS1CDR_FLEXIO2_CLK_PRED(CLK_PRED_2) | CCM_CS1CDR_FLEXIO2_CLK_PODF(CLK_PODF_2);

/// RM FlexIO, TIMCMP in dual 8-bit baud mode: the shift clock is the 120 MHz root divided by
/// 2 * (5 + 1), 10 MHz, one symbol bit of ws2812::SYMBOL_NS. The upper byte counts the edges of
/// one word, twice its bits less one.
constexpr uint32_t BAUD_DIV = 5;
constexpr uint32_t TIMER_COMPARE = ((ws2812::WORD_BITS * 2 - 1) << 8) | BAUD_DIV;
/// RM FlexIO, TIMCFG: the timer stops at its compare, after each word, and starts again on its
/// trigger, so it never shifts a word that is not there.
constexpr uint32_t TIMER_CFG = FLEXIO_TIMCFG_TIMDIS(2) | FLEXIO_TIMCFG_TIMENA(2);
/// RM FlexIO, TIMCTL: triggered by shifter 0's status flag, active low, so the timer runs while
/// shifter 0 holds a word, in dual 8-bit baud mode.
constexpr uint32_t SHIFTER0_STATUS = 1;   // TRGSEL 4 * N + 1 selects shifter N's status flag
constexpr uint32_t TIMER_CTL = FLEXIO_TIMCTL_TRGSEL(SHIFTER0_STATUS) | FLEXIO_TIMCTL_TRGPOL |
                               FLEXIO_TIMCTL_TRGSRC | FLEXIO_TIMCTL_TIMOD(1);
/// RM FlexIO, SHIFTCTL: transmit mode on timer 0, driving its pin as an output
constexpr uint32_t SHIFTER_TRANSMIT = 2;
constexpr uint32_t SHIFTER_OUTPUT = 3;
constexpr uint32_t SHIFTER0_DMA = 1;   // SHIFTSDEN: shifter 0's status requests the DMA
/// RM FlexIO, CTRL: FlexIO takes no register write until its software reset has cleared in the
/// FlexIO clock. The manual gives no time, so the output holds the reset this long and waits this
/// long after it, as the display driver does at FlexIO3, where the first write behind a shorter
/// reset got lost on the bench.
constexpr uint32_t FLEXIO_RESET_NS = 1000;

/// WS2812B datasheet Ver. No. V5: RES, the low time that ends a frame, more than 280 µs
constexpr uint32_t RESET_NS = 280000;
constexpr uint32_t WORD_NS = ws2812::SYMBOLS_PER_WORD * ws2812::SYMBOL_BITS * ws2812::SYMBOL_NS;
/// The words a shifter still holds when the DMA has written its last word: the one in its buffer
/// and the one it shifts out, RM FlexIO. A next send() cuts them off.
constexpr uint32_t WORDS_QUEUED = 2;
/// The zero words after the longest lane: more than RES even after the queued words are cut off.
constexpr uint32_t LATCH_WORDS = RESET_NS / WORD_NS + 1 + WORDS_QUEUED;
/// ARM Cortex-M7: the data cache's line, which a DMA buffer aligns to
constexpr size_t CACHE_LINE = 32;

/// RM eDMA, DCHPRI: under fixed-priority arbitration the channel with the highest CHPRI is served
/// first, and the channels of a group each need a different CHPRI. PJRC's DMAChannel uses group 0,
/// channels 0 to 15, and leaves CHPRI at the channel's number.
constexpr uint8_t TOP_PRIORITY = 15;
constexpr uint8_t PRIORITY_MASK = 0x0F;
constexpr uint8_t GROUP_CHANNELS = 16;

/// One FlexIO2 and one DMA channel serve every object, as the hardware is one.
DMAChannel dma(false);
bool flexioReady = false;
bool sending = false;

volatile uint32_t& shiftbuf(uint8_t n) {
    return (&FLEXIO2_SHIFTBUF0)[n];
}

volatile uint32_t& shiftctl(uint8_t n) {
    return (&FLEXIO2_SHIFTCTL0)[n];
}

volatile uint32_t& shiftcfg(uint8_t n) {
    return (&FLEXIO2_SHIFTCFG0)[n];
}

uint32_t shifterCtl(uint8_t signal) {
    return FLEXIO_SHIFTCTL_TIMSEL(0) | FLEXIO_SHIFTCTL_PINCFG(SHIFTER_OUTPUT) |
           FLEXIO_SHIFTCTL_PINSEL(signal) | FLEXIO_SHIFTCTL_SMOD(SHIFTER_TRANSMIT);
}

/// Gets the DCHPRI byte of a channel, laid out as PJRC's DMAChannel.cpp addresses it: four
/// channels to a register, the lowest channel in the register's highest byte.
///
/// @param channel  The DMA channel.
volatile uint8_t& dchpri(uint8_t channel) {
    return (&DMA_DCHPRI3)[(channel & DCHPRI_REGISTER) | (DCHPRI_BYTE - (channel & DCHPRI_BYTE))];
}

/// Gives the channel the highest priority, so no other channel keeps it from refilling the
/// shifters in time, and lets it preempt a lower channel's minor loop.
///
/// The channel that held the highest priority takes this channel's old one. RM eDMA, CR: HALT
/// keeps any channel from starting while two channels share a priority for a moment.
///
/// @param channel  The DMA channel.
void raisePriority(uint8_t channel) {
    const uint8_t own = dchpri(channel) & PRIORITY_MASK;
    DMA_CR |= DMA_CR_HALT;
    for (uint8_t other = 0; other < GROUP_CHANNELS; ++other) {
        if (other != channel && (dchpri(other) & PRIORITY_MASK) == TOP_PRIORITY) {
            dchpri(other) = uint8_t((dchpri(other) & ~PRIORITY_MASK) | own);
        }
    }
    dchpri(channel) = uint8_t((dchpri(channel) & ~(PRIORITY_MASK | DMA_DCHPRI_DPA)) | TOP_PRIORITY);
    DMA_CR &= ~DMA_CR_HALT;
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

/// Brings FlexIO2's clock root to 120 MHz, which every step of BAUD_DIV is computed for.
///
/// PJRC's start-up turns every clock gate on, so FlexIO3's FLEXEN bit says whether it is in use,
/// and its gate only whether that bit can be read.
///
/// @return  False, with nothing changed, when PLL3 does not run at 480 MHz, or when the root runs
///          at another rate while FlexIO3 is enabled, since FlexIO3 shares the root and a change
///          would move its rate.
bool clockAt120() {
    if (!pll3At480()) {
        logWarning("leds", "PLL3 does not run at 480 MHz");
        return false;
    }
    if (rootAt120()) {
        return true;
    }
    const bool flexio3Clocked = (CCM_CCGR7 & CCM_CCGR7_FLEXIO3(CCM_CCGR_ON)) != 0;
    if (flexio3Clocked && (FLEXIO3_CTRL & FLEXIO_CTRL_FLEXEN) != 0) {
        logWarning("leds", "FlexIO3 runs on the shared clock root at another rate");
        return false;
    }
    // RM CCM: the gate stays off while the root changes, and comes back on for a FlexIO2 that
    // runs already
    CCM_CCGR3 &= ~CCM_CCGR3_FLEXIO2(CCM_CCGR_ON);
    CCM_CSCMR2 = (CCM_CSCMR2 & ~CLK_SEL_MASK) | CLK_SEL;
    CCM_CS1CDR = (CCM_CS1CDR & ~CLK_DIV_MASK) | CLK_DIV;
    (void)CCM_CS1CDR;
    CCM_CCGR3 |= CCM_CCGR3_FLEXIO2(CCM_CCGR_ON);
    return rootAt120();
}

/// Sets up FlexIO2 on its clock: one timer clocks the shifters, which send() sets up.
bool startFlexio() {
    CCM_CCGR3 |= CCM_CCGR3_FLEXIO2(CCM_CCGR_ON);
    FLEXIO2_CTRL = FLEXIO_CTRL_SWRST;
    delayNanoseconds(FLEXIO_RESET_NS);
    FLEXIO2_CTRL = 0;
    delayNanoseconds(FLEXIO_RESET_NS);
    for (uint8_t shifter = 0; shifter < MAX_LANES; ++shifter) {
        shiftcfg(shifter) = 0;
        shiftctl(shifter) = 0;
    }
    FLEXIO2_TIMCMP0 = TIMER_COMPARE;
    FLEXIO2_TIMCFG0 = TIMER_CFG;
    FLEXIO2_TIMCTL0 = 0;
    FLEXIO2_SHIFTSDEN = SHIFTER0_DMA;
    FLEXIO2_CTRL = FLEXIO_CTRL_FLEXEN;
    dma.begin(true);
    // DMAChannel.cpp leaves TCD null when every channel is taken
    if (dma.TCD == nullptr) {
        logWarning("leds", "no DMA channel free");
        return false;
    }
    dma.triggerAtHardwareEvent(DMAMUX_SOURCE_FLEXIO2_REQUEST0);
    raisePriority(dma.channel);
    return true;
}

}   // namespace

bool Ws2812Flexio2::begin(const Lane* lanes, uint8_t count) {
    // The lanes and their words come from the heap, once, during the start, so a later begin()
    // keeps the lanes of the first.
    if (!allocated) {
        if (count == 0 || count > MAX_LANES) {
            logWarning("leds", "%u lanes, where 1 to %u fit", count, MAX_LANES);
            return false;
        }
        uint16_t longest = 0;
        for (uint8_t lane = 0; lane < count; ++lane) {
            const uint8_t signal = Ws2812Flexio2::signalOf(lanes[lane].pin);
            if (signal == NO_SIGNAL) {
                logWarning("leds", "pin %u does not reach FlexIO2", lanes[lane].pin);
                return false;
            }
            for (uint8_t other = 0; other < lane; ++other) {
                if (lanes[other].pin == lanes[lane].pin) {
                    logWarning("leds", "pin %u carries two lanes", lanes[lane].pin);
                    return false;
                }
            }
            signals[lane] = signal;
            longest = lanes[lane].leds > longest ? lanes[lane].leds : longest;
        }
        if (longest > 0) {
            wordCount = uint32_t(longest) * ws2812::WORDS_PER_LED + LATCH_WORDS;
            const size_t bytes = size_t(wordCount) * count * sizeof(uint32_t) + CACHE_LINE;
            uint8_t* raw = new (std::nothrow) uint8_t[bytes];
            if (raw == nullptr) {
                logWarning("leds", "no room for %lu words", wordCount * count);
                wordCount = 0;
                return false;
            }
            const uintptr_t aligned =
                (uintptr_t(raw) + CACHE_LINE - 1) & ~uintptr_t(CACHE_LINE - 1);
            words = reinterpret_cast<uint32_t*>(aligned);
        }
        for (uint8_t lane = 0; lane < count; ++lane) {
            pins[lane] = lanes[lane].pin;
            counts[lane] = lanes[lane].leds;
        }
        laneCount = count;
        allocated = true;
    }
    if (words == nullptr) {
        return true;
    }
    if (!clockAt120()) {
        return false;
    }
    if (!flexioReady) {
        if (!startFlexio()) {
            return false;
        }
        flexioReady = true;
    }
    for (uint8_t lane = 0; lane < laneCount; ++lane) {
        if (counts[lane] > 0) {
            *portConfigRegister(pins[lane]) = ALT_FLEXIO2;
            *portControlRegister(pins[lane]) = PAD_OUTPUT;
        }
    }
    return true;
}

bool Ws2812Flexio2::busy() const {
    return sending && !dma.complete();
}

bool Ws2812Flexio2::send(const uint8_t* const* rgb, uint8_t brightness) {
    if (words == nullptr || !flexioReady) {
        DEBUG_LOG("Ws2812Flexio2::send: no LEDs to send\n");
        return false;
    }
    if (busy()) {
        DEBUG_LOG("Ws2812Flexio2::send: the last frame still goes out\n");
        return false;
    }
    // Each DMA request hands every shifter one word, so the buffer holds the lanes' words
    // interleaved: one word of every lane, then the next word of every lane. Within a request the
    // last lane's word goes first and shifter 0's last, since shifter 0's status starts the timer.
    // So lane l's words start at laneCount - 1 - l and lie laneCount words apart.
    for (uint8_t lane = 0; lane < laneCount; ++lane) {
        ws2812::encodeLane(rgb[lane], counts[lane], brightness, words + (laneCount - 1 - lane),
                           laneCount, wordCount);
    }
    const size_t bytes = size_t(wordCount) * laneCount * sizeof(uint32_t);
    arm_dcache_flush(words, bytes);
    // Stopping the timer and switching each shifter off and on drops whatever a cut-off latch
    // left in them. Shifter l drives lane l's pin, and the shifters past the lanes stay off.
    FLEXIO2_TIMCTL0 = 0;
    for (uint8_t shifter = 0; shifter < MAX_LANES; ++shifter) {
        shiftctl(shifter) = 0;
        if (shifter < laneCount) {
            shiftctl(shifter) = shifterCtl(signals[shifter]);
        }
    }
    dma.TCD->SADDR = words;
    dma.TCD->SOFF = sizeof(uint32_t);
    dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(2) | DMA_TCD_ATTR_DSIZE(2);
    // RM eDMA, NBYTES with minor loop offsets: each request writes one word a lane downward from
    // the last lane's shifter buffer, then the destination moves back up by as much
    dma.TCD->NBYTES_MLOFFYES = DMA_TCD_NBYTES_DMLOE |
                               DMA_TCD_NBYTES_MLOFFYES_MLOFF(sizeof(uint32_t) * laneCount) |
                               DMA_TCD_NBYTES_MLOFFYES_NBYTES(sizeof(uint32_t) * laneCount);
    dma.TCD->SLAST = 0;
    dma.TCD->DADDR = &shiftbuf(laneCount - 1);
    dma.TCD->DOFF = -int16_t(sizeof(uint32_t));
    dma.TCD->CITER_ELINKNO = wordCount;
    dma.TCD->BITER_ELINKNO = wordCount;
    dma.TCD->DLASTSGA = 0;
    dma.TCD->CSR = DMA_TCD_CSR_DREQ;
    dma.clearComplete();
    dma.enable();
    FLEXIO2_TIMCTL0 = TIMER_CTL;
    sending = true;
    return true;
}

uint8_t Ws2812Flexio2::signalOf(uint8_t pin) {
    for (const FlexioPin& p : FLEXIO_PINS) {
        if (p.pin == pin) {
            return p.signal;
        }
    }
    return NO_SIGNAL;
}
