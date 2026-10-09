#include <atomic>
#include <cstring>
#include <type_traits>

enum class DriverEventType : uint16_t {
    DeviceFailed,       // a part of a device has failed; the source packs device and part, the payload is the driver's error code
    DeviceRecovered,    // that part works again; the source as for DeviceFailed, no payload
    // List of events ...
};

struct DriverEvent {
    uint32_t        time;      // nowUs() at detection
    DriverEventType type;
    uint8_t         source;    // channel, switch or target inside its subsystem
    uint32_t        payload;   // interpreted according to the type
};

// one payload per type
template <DriverEventType Tag> struct DriverPayloadOf;

// gets the payload of an event of type Tag
template <DriverEventType Tag>
typename DriverPayloadOf<Tag>::type payload(const DriverEvent& e) {
    typename DriverPayloadOf<Tag>::type value;
    memcpy(&value, &e.payload, sizeof(value));
    return value;
}

// every driver's events in one stream, oldest first
class EventQueue {
public:
    // one driver's queue: its interrupt writes it, the main loop reads it, so it needs no lock
    class Producer {
    public:
        // ISR-safe, false when full
        bool publish(const DriverEvent& e) {
            const uint32_t head = head_.load(std::memory_order_relaxed);
            if (head - tail_.load(std::memory_order_acquire) == depth_) {
                dropped_ = dropped_ + 1;
                return false;
            }
            storage_[head & (depth_ - 1)] = e;
            head_.store(head + 1, std::memory_order_release);   // the event is whole before the reader sees it
            return true;
        }

        // publishes an event of type Tag with its payload packed into the 32 bits of the field
        template <DriverEventType Tag>
        bool publish(uint32_t now, uint8_t source, typename DriverPayloadOf<Tag>::type value) {
            using T = typename DriverPayloadOf<Tag>::type;
            static_assert(sizeof(T) <= sizeof(uint32_t) && std::is_trivially_copyable_v<T>,
                          "a payload fits the 32 bits of the field");
            uint32_t raw = 0;
            memcpy(&raw, &value, sizeof(T));
            return publish(DriverEvent{now, Tag, source, raw});
        }

        uint32_t dropped() const { return dropped_; }   // gets how many events found the queue full

    private:
        friend class EventQueue;

        // gets the oldest event of the queue, nullptr when it is empty
        const DriverEvent* front() const {
            const uint32_t tail = tail_.load(std::memory_order_relaxed);
            if (head_.load(std::memory_order_acquire) == tail) return nullptr;
            return &storage_[tail & (depth_ - 1)];
        }

        // frees the oldest event's slot once read() has copied it
        void pop() { tail_.store(tail_.load(std::memory_order_relaxed) + 1, std::memory_order_release); }

        DriverEvent*          storage_ = nullptr;
        uint32_t              depth_   = 0;     // a power of two; 0 drops every event
        std::atomic<uint32_t> head_{0};         // the events written so far, which only the writer moves
        std::atomic<uint32_t> tail_{0};         // the events read so far, which only the reader moves
        volatile uint32_t     dropped_ = 0;
    };

    // before the main loop only, depth a power of two. A queue past the tenth, or one of another
    // depth, drops every event, and its dropped() counts them
    Producer& attach(DriverEvent* storage, size_t depth) {
        if (count_ == kProducers || depth == 0 || (depth & (depth - 1)) != 0) return refused_;
        Producer& p = producers_[count_];
        p.storage_ = storage;
        p.depth_   = depth;
        count_ = count_ + 1;
        return p;
    }

    // up to max, oldest first; compares the times with before(), so the order holds across the
    // wrap of nowUs()
    size_t read(DriverEvent* out, size_t max) {
        size_t n = 0;
        while (n < max) {
            Producer*          from  = nullptr;
            const DriverEvent* first = nullptr;
            for (size_t i = 0; i < count_; ++i) {
                const DriverEvent* e = producers_[i].front();
                if (e && (!first || before(e->time, first->time))) {
                    from  = &producers_[i];
                    first = e;
                }
            }
            if (!first) break;                   // every queue is empty
            out[n] = *first;
            from->pop();
            n = n + 1;
        }
        return n;
    }

private:
    static constexpr size_t kProducers = 10;     // the producers input-handling.md estimates at most

    Producer producers_[kProducers];
    Producer refused_;                           // the queue attach() hands out when it refuses one
    size_t   count_ = 0;
};

extern EventQueue events;
