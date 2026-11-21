#include "event-queue.h"
#include "logging/logger.h"
#include "logging/debug-log.h"
#include "logging/debug-trace.h"
#include <inttypes.h>

// The note of an event read carries its type above its source, eight bits each.
constexpr uint32_t SOURCE_BITS = 8;

EventQueue events;

bool EventQueue::attach(DriverEvent* storage, uint32_t depth, Producer*& out) {
    out = &refused;
    if (count == PRODUCERS) {
        logWarning("events", "all %u queues taken", PRODUCERS);
        return false;
    }
    if (storage == nullptr) {
        logWarning("events", "a queue without storage");
        return false;
    }
    if (depth == 0 || (depth & (depth - 1)) != 0) {
        logWarning("events", "a depth of %" PRIu32 " is no power of two", depth);
        return false;
    }
    Producer& p = producers[count];
    p.ring.begin(storage, depth);
    count = count + 1;
    out = &p;
    return true;
}

uint32_t EventQueue::read(DriverEvent* out, uint32_t max) {
    for (uint8_t i = 0; i < count; ++i) {
        Producer& p = producers[i];
        const uint32_t dropped = p.dropped();
        if (dropped != p.reportedDrops) {
            logWarning("events", "queue %u dropped %" PRIu32 " events", i,
                       dropped - p.reportedDrops);
            p.reportedDrops = dropped;
        }
    }
    uint32_t n = 0;
    while (n < max) {
        Producer* from = nullptr;
        const DriverEvent* oldest = nullptr;
        for (uint8_t i = 0; i < count; ++i) {
            const DriverEvent* e = producers[i].front();
            if (e != nullptr && (oldest == nullptr || before(e->time, oldest->time))) {
                from = &producers[i];
                oldest = e;
            }
        }
        if (oldest == nullptr) {
            // Every queue is empty.
            break;
        }
        out[n] = *oldest;
        // A note takes a few cycles, where a formatted line would slow read() enough to let a
        // queue under a burst of interrupts run full.
        DEBUG_TRACE("event", int32_t(uint32_t(oldest->type) << SOURCE_BITS | oldest->source),
                    int32_t(oldest->payload));
        from->pop();
        n = n + 1;
    }
    return n;
}
