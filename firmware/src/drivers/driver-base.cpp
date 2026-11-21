#include "driver-base.h"
#include "logging/logger.h"
#include "logging/debug-log.h"
#include <inttypes.h>
#include <utility>

DriverTick driverTick;

bool DriverTick::attach(Callback fn, uint32_t periodUs) {
    const uint32_t every = periodUs / PERIOD_US;
    if (count == SLOTS) {
        logWarning("tick", "all %u slots taken", SLOTS);
        return false;
    }
    if (every == 0 || periodUs % PERIOD_US != 0) {
        logWarning("tick", "%" PRIu32 " us is no multiple of %" PRIu32 " us", periodUs, PERIOD_US);
        return false;
    }
    if (count == 0) {
        timer.priority(PRIORITY);
        if (!timer.begin([this] { tick(); }, PERIOD_US)) {
            logWarning("tick", "no IntervalTimer free");
            return false;
        }
    }
    // The tick must not see a half-written slot.
    noInterrupts();
    slots[count] = Slot{std::move(fn), every, every};
    count = count + 1;
    interrupts();
    return true;
}

void DriverTick::tick() {
    for (uint8_t i = 0; i < count; ++i) {
        if (--slots[i].left != 0) {
            continue;
        }
        slots[i].left = slots[i].every;
        slots[i].fn();
    }
}
