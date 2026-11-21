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
#include "core/clock.h"
#include "core/ring.h"
#include <string.h>
#include <type_traits>

enum class DriverEventType : uint16_t {
    /// A part of a device has failed. The source packs device and part, as faultSource() of
    /// core/device-monitor.h does, and the payload is the driver's error code.
    DEVICE_FAILED,
    /// That part works again. The source as for DEVICE_FAILED, no payload.
    DEVICE_RECOVERED,
    /// A finger touches the screen. The source is 0, and the payload is a TouchPoint.
    TOUCH_DOWN,
    /// The finger has moved far enough from the position last reported. Payload as for TOUCH_DOWN.
    TOUCH_MOVED,
    /// The finger has rested where it touched down for long enough. Payload as for TOUCH_DOWN.
    TOUCH_HELD,
    /// The finger has left the screen. The payload is where it was last.
    TOUCH_UP,
};

struct DriverEvent {
    uint32_t time;   // nowUs() at detection
    DriverEventType type;
    uint8_t source;     // channel, switch or target inside its subsystem
    uint32_t payload;   // interpreted according to the type
};
/// model/firmware/figures.py, b_event: the size the device monitor's queue in RAM2 is counted with
constexpr size_t EVENT_BYTES = 12;
static_assert(sizeof(DriverEvent) == EVENT_BYTES, "the model counts RAM2 with this size");

/// The payload of a type without one.
struct NoPayload {};

/// A position on the screen in the picture's pixels, 0 at the top left. Packed into a payload, x
/// takes the lower 16 bits and y the upper 16.
struct TouchPoint {
    uint16_t x;
    uint16_t y;
};

/// Names the payload of each type, one per type.
template <DriverEventType TAG> struct DriverPayloadOf;
template <> struct DriverPayloadOf<DriverEventType::DEVICE_FAILED> {
    using type = uint32_t;   // the driver's error code
};
template <> struct DriverPayloadOf<DriverEventType::DEVICE_RECOVERED> {
    using type = NoPayload;
};
template <> struct DriverPayloadOf<DriverEventType::TOUCH_DOWN> {
    using type = TouchPoint;
};
template <> struct DriverPayloadOf<DriverEventType::TOUCH_MOVED> {
    using type = TouchPoint;
};
template <> struct DriverPayloadOf<DriverEventType::TOUCH_HELD> {
    using type = TouchPoint;
};
template <> struct DriverPayloadOf<DriverEventType::TOUCH_UP> {
    using type = TouchPoint;
};

/// Gets the payload of an event of type TAG.
///
template <DriverEventType TAG> typename DriverPayloadOf<TAG>::type payload(const DriverEvent& e) {
    typename DriverPayloadOf<TAG>::type value{};
    if constexpr (!std::is_empty_v<decltype(value)>) {
        memcpy(&value, &e.payload, sizeof(value));
    }
    return value;
}

class EventQueue {
public:
    /// model/firmware/figures.py, n_producers: the queues attach() takes at most
    static constexpr uint8_t PRODUCERS = 11;

    /// One driver's queue. Its interrupt writes it, the main loop reads it.
    class Producer {
    public:
        /// Puts the event at the end of the queue.
        ///
        /// @return   False when the queue is full. The event is then counted in dropped().
        /// @note     Safe in an interrupt.
        bool publish(const DriverEvent& e) {
            if (!ring.append(&e, 1)) {
                droppedCount = droppedCount + 1;
                return false;
            }
            return true;
        }

        /// Publishes an event of type TAG with its payload packed into the 32 bits of the field.
        ///
        /// @param now     The time stamp of the event, nowUs() at detection.
        /// @param source  The event's source.
        /// @param value   The event's payload.
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

        /// Gets how many events found the queue full.
        uint32_t dropped() const { return droppedCount; }

    private:
        friend class EventQueue;

        /// Gets the oldest event of the queue, nullptr when it is empty.
        const DriverEvent* front() const {
            size_t n = 0;
            const DriverEvent* e = ring.peek(n);
            return n == 0 ? nullptr : e;
        }

        /// Frees the oldest event's slot once read() has copied it.
        void pop() { ring.consume(1); }

        /// The queue's events. The driver's interrupt adds them and read() takes them. attach()
        /// hands the ring the storage.
        Ring<DriverEvent> ring;
        volatile uint32_t droppedCount = 0;
        uint32_t reportedDrops = 0;   // the drops read() has written into the log, moved by read()
    };

    /// Attaches a queue of @p depth events in @p storage and points @p out at it.
    ///
    /// @param storage  The memory of the queue's events.
    /// @param depth    The number of events the queue holds.
    /// @param out      Points at the queue. After a refusal it points at a queue that drops every
    ///                 event and counts it in dropped(), so an interrupt that publishes needs no
    ///                 check.
    /// @return         False for a queue past the last of PRODUCERS, for no storage and for a
    ///                 depth that is no power of two.
    /// @note           Before the main loop only.
    bool attach(DriverEvent* storage, uint32_t depth, Producer*& out);

    /// Copies up to @p max events into @p out, oldest first across all queues.
    ///
    /// The times are compared with before(), so the order holds across the wrap of nowUs(). A queue
    /// that dropped events since the last read() gets a warning in the log. A debug build notes
    /// every event it copies in the debug trace, with the type above the source and the payload.
    ///
    /// @param out  Receives the events.
    /// @param max  The most events to copy.
    /// @return     How many events were copied.
    /// @note       The main loop calls it.
    uint32_t read(DriverEvent* out, uint32_t max);

private:
    Producer producers[PRODUCERS];
    Producer refused;   // the queue attach() points at when it refuses one
    uint8_t count = 0;
};

extern EventQueue events;
