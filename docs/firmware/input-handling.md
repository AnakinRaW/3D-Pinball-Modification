# Input Handling System

## Requirements

The machine has several input subsystems: IR ball sensing, a break beam sensor, hit targets, sensed solenoids.

These subsystems have in common that they are sensing events driven by the ball interacting with elements on the playfield. Their drivers are responsible for correct sensing, processing as well as notifying the event. The subsystems shall not communicate with each other directly, but only publish their events.

The main loop hands those events to the game host, and the [playfield components](game-abstraction.md#playfield-components) turn them into the pinball events a game gets. The game handles those by calling other subsystems to create things like visual effects or point scoring. The **Input Handling System** is the place to organise these events so that a game can use them.

In order to be able to realise complex game mechanics the input system needs to fulfil a couple of requirements:

- The game logic can tell the order in which events happened, whichever subsystem detected which.
- The game logic can tell how much time passed between two events, so a timing window can be judged.
- An event can additionally carry a payload that the game logic consumes.
- No event is lost while the loop is busy with a display redraw, an LED frame or anything else, as long as its queue has space left.
- Events can be read out and dispatched without an interrupt, so the game logic runs in the main loop.

## Concept

Every subsystem writes its events to a separate queue as it detects them, each stamped with the moment of detection. The input handling system serves the oldest event across those queues on request. This should happen at a single point in the main loop, so the game logic sees one ordered stream and never runs inside an interrupt handler.

A driver event carries its type, its source inside the subsystem, its timestamp and one payload field of 32 bits, interpreted according to the type. A read only looks at the timestamp.

```cpp
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
template <DriverEventType Tag> typename DriverPayloadOf<Tag>::type payload(const DriverEvent&);
```

Each queue has one writer and one reader, so none needs a lock and no interrupt is ever disabled for one. A read searches the front of every queue for the oldest event and compares their times with [`before()`](general-design.md#time-measurement). At an estimated size of ten producers maximum the lookup cost is acceptable. A queue with no room left drops the newest event and counts it, which is what `dropped()` reports.

> A real-time event is handled by the subsystem that detects it, and the message that it happened is reported afterwards. A sensed solenoid triggering is such an event.

Device faults reach the queue through the device monitor, which [error-handling](error-handling.md#device-faults) describes. The `Device` list stands in [driver-design](driver-design.md#devices).

## API

```cpp
class EventQueue {
public:
    class Producer {
    public:
        // ISR-safe, false when full
        bool publish(const DriverEvent& e);
        template <DriverEventType Tag>
        bool publish(uint32_t now, uint8_t source, typename DriverPayloadOf<Tag>::type value);
        uint32_t dropped() const;
    };

    // before the main loop only, depth a power of two
    Producer& attach(DriverEvent* storage, size_t depth);

    // up to max, oldest first
    size_t    read(DriverEvent* out, size_t max);
};

extern EventQueue events;
```

## What else was considered

| Approach | Outcome |
|---|---|
| A callback per subsystem, called where the event is detected | The game logic runs wherever detection happened, an interrupt included. Each subsystem reaches it by a path of a different length, so the order is whatever those paths make it |
| The game polls every subsystem once per loop pass | The order becomes the polling order, which has nothing to do with real time, and anything happening twice between two passes collapses into one |
| The game diffs a driver's state each pass | As above, and every change between two passes disappears. That state stays as something a rule can query, rather than as the way an event is found |
| The payload as a type parameter, `Event<TPayload>` | Every payload type gives a separate event type, and one queue carries one type, so the one ordered stream falls apart |
| FreeRTOS's message queues | Its queue neither stamps an event nor brings several subsystems into one order, so the timestamp and the ordering would be written on top of it anyway. FreeRTOS also briefly blocks interrupts for every event a driver publishes to such a queue, so each event would cost more than in the lock-free queues and gain nothing |