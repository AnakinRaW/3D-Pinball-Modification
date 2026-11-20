#include "event-queue.h"
#include "logging/logger.h"
#include "logging/debug-log.h"
#include <inttypes.h>

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
    p.storage = storage;
    p.depth = depth;
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
        DEBUG_LOG("event %u from source %u, payload %" PRIu32 ", at %" PRIu32 " us\n",
                  unsigned(oldest->type), oldest->source, oldest->payload, oldest->time);
        from->pop();
        n = n + 1;
    }
    return n;
}
