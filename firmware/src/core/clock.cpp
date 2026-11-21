#include "clock.h"

// The computer tests set the fake GPT2_CNT themselves and start no clock.
#if defined(__IMXRT1062__)

/// i.MX RT1060 reference manual, Rev. 3, General Purpose Timer: the 24 MHz crystal divided by 8 in
/// the 24 MHz prescaler and by 3 in the main one gives 1 MHz
constexpr uint32_t PRESCALE_24M = 8;
constexpr uint32_t PRESCALE = 3;
constexpr uint32_t CLKSRC_24M = 5;   // GPT_CR CLKSRC: the crystal through its prescaler

/// GPT2's clock gate is off after a reset, and a register access before the gate is on stops the
/// Teensy. ENMOD clears the counter as GPT2 starts, and FRR lets it run on across 2^32.
void startClock() {
    CCM_CCGR0 |= CCM_CCGR0_GPT2_BUS(CCM_CCGR_ON) | CCM_CCGR0_GPT2_SERIAL(CCM_CCGR_ON);
    GPT2_CR = 0;
    GPT2_PR = GPT_PR_PRESCALER24M(PRESCALE_24M - 1) | GPT_PR_PRESCALER(PRESCALE - 1);
    GPT2_CR = GPT_CR_EN_24M | GPT_CR_CLKSRC(CLKSRC_24M) | GPT_CR_FRR | GPT_CR_WAITEN | GPT_CR_ENMOD;
    GPT2_CR |= GPT_CR_EN;
}

#endif
