// The events a game gets from the components, with the fields of a driver event: the moment of
// detection, a type, a source inside the component and a payload. A component hands the events it
// made of one driver event as a PinballEvents, which points into the component's list.
//
// Design: docs/firmware/game-abstraction.md, Playfield components.
#pragma once
#include <stdint.h>
#include <string.h>
#include <type_traits>

enum class EventType : uint16_t {
    /// A mode's track of music has played to its end. The source is the key of the track's mode,
    /// and the payload is the number the audio driver gave the track.
    MUSIC_ENDED,
    /// A video has played to its end. The source is the key of the mode that holds the video's
    /// slide, and the payload is 0.
    MOVIE_ENDED,
};

struct PinballEvent {
    uint32_t time;   // nowUs() at detection, taken over from the driver event
    EventType type;
    uint8_t source;     // lane, channel or key inside its component
    uint32_t payload;   // interpreted according to the type
};

/// The pinball events a component made of one driver event, valid until its next translate().
struct PinballEvents {
    const PinballEvent* events = nullptr;   // points into the component's list
    uint8_t count = 0;
};

/// Names the payload of each type, one per type.
template <EventType TAG> struct PayloadOf;
template <> struct PayloadOf<EventType::MUSIC_ENDED> {
    using type = uint32_t;   // the number the audio driver gave the track
};
template <> struct PayloadOf<EventType::MOVIE_ENDED> {
    using type = uint32_t;   // always 0
};

/// Gets the payload of an event of type TAG.
template <EventType TAG> typename PayloadOf<TAG>::type payload(const PinballEvent& e) {
    typename PayloadOf<TAG>::type value{};
    static_assert(sizeof(value) <= sizeof(e.payload) &&
                      std::is_trivially_copyable_v<decltype(value)>,
                  "a payload fits the 32 bits of the field");
    memcpy(&value, &e.payload, sizeof(value));
    return value;
}
