#include "warning-list.h"
#include "core/task-lock.h"
#include <stdio.h>
#include <string.h>

constexpr uint16_t COUNT_MAX = UINT16_MAX;

WarningList warningList;

void WarningList::add(const char* who, uint32_t ms, const char* format, va_list args) {
    if (inInterrupt()) {
        // An interrupt may stop the task that holds the lock, so it never takes it.
        return;
    }
    // The text is formatted before the lock, so the lock is held only for the search.
    char text[WarningEntry::TEXT_BYTES];
    vsnprintf(text, sizeof(text), format, args);

    const TaskLock lock;
    const uint8_t n = taken.load(std::memory_order_relaxed);
    for (uint8_t i = 0; i < n; ++i) {
        WarningEntry& e = entries[i];
        if (strcmp(e.who, who) == 0 && strcmp(e.text, text) == 0) {
            if (e.count < COUNT_MAX) {
                e.count = static_cast<uint16_t>(e.count + 1);
                changes.fetch_add(1, std::memory_order_release);
            }
            return;
        }
    }
    if (n == ENTRIES) {
        droppedTotal.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    WarningEntry& e = entries[n];
    e.who = who;
    memcpy(e.text, text, sizeof(text));
    e.count = 1;
    e.firstMs = ms;
    taken.store(static_cast<uint8_t>(n + 1), std::memory_order_release);
    changes.fetch_add(1, std::memory_order_release);
}

bool WarningList::read(uint8_t i, WarningEntry& out) const {
    const TaskLock lock;
    if (i >= taken.load(std::memory_order_relaxed)) {
        return false;
    }
    out = entries[i];
    return true;
}
