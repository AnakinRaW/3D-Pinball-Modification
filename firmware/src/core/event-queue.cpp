#include "event-queue.h"
#include "logging/debug-log.h"
#include <inttypes.h>

EventQueue events;

bool EventQueue::attach(DriverEvent* storage, uint32_t depth, Producer*& out) {
    out = &refused;
    if (count == PRODUCERS) {
        DEBUG_LOG("EventQueue::attach: all %u queues taken\n", PRODUCERS);
        return false;
    }
    if (storage == nullptr) {
        DEBUG_LOG("EventQueue::attach: no storage\n");
        return false;
    }
    if (depth == 0 || (depth & (depth - 1)) != 0) {
        DEBUG_LOG("EventQueue::attach: a depth of %" PRIu32 " is no power of two\n", depth);
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
        from->pop();
        n = n + 1;
    }
    return n;
}
