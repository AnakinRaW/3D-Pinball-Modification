#include "interrupt-priorities.h"
#include "logging/logger.h"

namespace {

/// An interrupt and the priority its row of the interrupt table gives.
struct Row {
    const char* name;
    IRQ_NUMBER_t irq;
    uint8_t priority;
};

// general-design.md, Interrupts: the rows whose interrupt has a fixed number. The audio output's
// DMA channel is taken at run time. The watchdog and the IR ball sensing have no driver.
constexpr Row ROWS[] = {
    {"pin interrupts", IRQ_GPIO6789, 96}, {"driver tick", IRQ_PIT, 96},
    {"I2C bus Wire", IRQ_LPI2C1, 160},    {"I2C bus Wire2", IRQ_LPI2C4, 160},
    {"audio update", IRQ_SOFTWARE, 208},  {"display", IRQ_FLEXIO3, 224},
    {"SD controller", IRQ_SDHC1, 240},
};

// general-design.md, Interrupts: FreeRTOS's tick and its task switch
constexpr uint8_t FREERTOS_PRIORITY = 240;
// ARMv7-M architecture reference manual, B3.2.12 SHPR3: PRI_15 in bits 31 to 24 for SysTick,
// PRI_14 in bits 23 to 16 for PendSV
constexpr uint32_t SYSTICK_SHIFT = 24;
constexpr uint32_t PENDSV_SHIFT = 16;
constexpr uint32_t PRIORITY_MASK = 0xFF;

}   // namespace

uint32_t checkInterruptPriorities() {
    uint32_t differ = 0;
    const char* first = nullptr;
    uint8_t firstSeen = 0;
    uint8_t firstWanted = 0;
    const auto compare = [&](const char* name, uint8_t seen, uint8_t wanted) {
        if (seen == wanted) {
            return;
        }
        if (differ == 0) {
            first = name;
            firstSeen = seen;
            firstWanted = wanted;
        }
        ++differ;
    };
    for (const Row& row : ROWS) {
        if (NVIC_IS_ENABLED(row.irq)) {
            compare(row.name, NVIC_GET_PRIORITY(row.irq), row.priority);
        }
    }
    compare("SysTick", uint8_t((SCB_SHPR3 >> SYSTICK_SHIFT) & PRIORITY_MASK), FREERTOS_PRIORITY);
    compare("PendSV", uint8_t((SCB_SHPR3 >> PENDSV_SHIFT) & PRIORITY_MASK), FREERTOS_PRIORITY);
    if (differ != 0) {
        logWarning("main", "%" PRIu32 " interrupts off their priority, first %s at %u, not %u",
                   differ, first, firstSeen, firstWanted);
    }
    return differ;
}
