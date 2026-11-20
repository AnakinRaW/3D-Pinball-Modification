// Carries the events the drivers detect to the main loop, oldest first. Every driver publishes
// into its queue, each event stamped with nowUs() at detection. The main loop reads all queues as
// one stream.
//
// Each queue has one writer, the driver's interrupt, and one reader, the main loop. It therefore
// needs no lock. No interrupt is ever disabled for it. events is the firmware's one queue.
//
// Design: docs/firmware/input-handling.md.
#pragma once
#include <Arduino.h>
#include <atomic>
#include "core/clock.h"
#include <string.h>
#include <type_traits>

enum class DriverEventType : uint16_t {
    // A part of a device has failed. The source packs device and part, as faultSource() of
    // core/device-monitor.h does, and the payload is the driver's error code.
    DEVICE_FAILED,
    // That part works again. The source as for DEVICE_FAILED, no payload.
    DEVICE_RECOVERED,
};

struct DriverEvent {
    uint32_t time;   // nowUs() at detection
    DriverEventType type;
    uint8_t source;     // channel, switch or target inside its subsystem
    uint32_t payload;   // interpreted according to the type
};
// model/firmware/figures.py, b_event: the size the device monitor's queue in RAM2 is counted with
constexpr size_t EVENT_BYTES = 12;
static_assert(sizeof(DriverEvent) == EVENT_BYTES, "the model counts RAM2 with this size");

// the payload of a type without one
struct NoPayload {};

// one payload per type
template <DriverEventType TAG> struct DriverPayloadOf;
template <> struct DriverPayloadOf<DriverEventType::DEVICE_FAILED> {
    using type = uint32_t;   // the driver's error code
};
template <> struct DriverPayloadOf<DriverEventType::DEVICE_RECOVERED> {
    using type = NoPayload;
};

// Gets the payload of an event of type TAG.
template <DriverEventType TAG> typename DriverPayloadOf<TAG>::type payload(const DriverEvent& e) {
    typename DriverPayloadOf<TAG>::type value{};
    if constexpr (!std::is_empty_v<decltype(value)>) {
        memcpy(&value, &e.payload, sizeof(value));
    }
    return value;
}

class EventQueue {
public:
    // model/firmware/figures.py, n_producers: the queues attach() takes at most
    static constexpr uint8_t PRODUCERS = 11;

    // One driver's queue. Its interrupt writes it, the main loop reads it.
    class Producer {
    public:
        // Puts the event at the end of the queue. Safe in an interrupt. Returns false and counts
        // the event in dropped() when the queue is full.
        bool publish(const DriverEvent& e) {
            const uint32_t head = writtenCount.load(std::memory_order_relaxed);
            if (head - readCount.load(std::memory_order_acquire) == depth) {
                droppedCount = droppedCount + 1;
                return false;
            }
            storage[head & (depth - 1)] = e;
            // The event is whole before the reader sees it.
            writtenCount.store(head + 1, std::memory_order_release);
            return true;
        }

        // Publishes an event of type TAG with its payload packed into the 32 bits of the field.
        template <DriverEventType TAG>
        bool publish(uint32_t now, uint8_t source, typename DriverPayloadOf<TAG>::type value) {
            using T = typename DriverPayloadOf<TAG>::type;
            static_assert(sizeof(T) <= sizeof(uint32_t) && std::is_trivially_copyable_v<T>,
                          "a payload fits the 32 bits of the field");
            uint32_t raw = 0;
            // An empty type such as NoPayload still takes a byte, whose value is undefined.
            if constexpr (!std::is_empty_v<T>) {
                memcpy(&raw, &value, sizeof(T));
            }
            return publish(DriverEvent{now, TAG, source, raw});
        }

        // Gets how many events found the queue full.
        uint32_t dropped() const { return droppedCount; }

    private:
        friend class EventQueue;

        // Gets the oldest event of the queue, nullptr when it is empty.
        const DriverEvent* front() const {
            const uint32_t tail = readCount.load(std::memory_order_relaxed);
            if (writtenCount.load(std::memory_order_acquire) == tail) {
                return nullptr;
            }
            return &storage[tail & (depth - 1)];
        }

        // Frees the oldest event's slot once read() has copied it.
        void pop() {
            readCount.store(readCount.load(std::memory_order_relaxed) + 1,
                            std::memory_order_release);
        }

        DriverEvent* storage = nullptr;
        uint32_t depth = 0;   // a power of two; 0 drops every event
        std::atomic<uint32_t> writtenCount{
            0};   // the events written so far, moved by the writer alone
        std::atomic<uint32_t> readCount{0};   // the events read so far, moved by the reader alone
        volatile uint32_t droppedCount = 0;
        uint32_t reportedDrops = 0;   // the drops read() has written into the log, moved by read()
    };

    // Attaches a queue of depth events in storage and points out at it. Before the main loop only.
    // Returns false for a queue past the last of PRODUCERS, for no storage and for a depth that
    // is no power of two. out then points at a queue that drops every event and counts it in
    // dropped(), so an interrupt that publishes needs no check.
    bool attach(DriverEvent* storage, uint32_t depth, Producer*& out);

    // Copies up to max events into out, oldest first across all queues, and returns how many. The
    // times are compared with before(), so the order holds across the wrap of nowUs(). A queue
    // that dropped events since the last read() gets a warning in the log. A debug build notes
    // every event it copies in the debug trace, with the type above the source and the payload. The
    // main loop calls it.
    uint32_t read(DriverEvent* out, uint32_t max);

private:
    Producer producers[PRODUCERS];
    Producer refused;   // the queue attach() points at when it refuses one
    uint8_t count = 0;
};

extern EventQueue events;
