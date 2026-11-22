// One element of the playfield, or a service such as the sounds, between the drivers and the game.
// The host hands every driver event to the components one after the other until one takes it, and
// passes the pinball events the component made of it to the game.
//
// Design: docs/firmware/game-abstraction.md, Playfield components.
#pragma once
#include "core/event-queue.h"
#include "game/pinball-event.h"

class Component {
public:
    virtual ~Component() = default;

    /// Takes a driver event of the component's parts and points at the pinball events it made.
    ///
    /// @param in   The driver event.
    /// @param out  Points at the pinball events made of @p in, none when it made none.
    /// @return     False for an event of another part, which goes on to the next component.
    /// @note       The host calls it in the main loop.
    virtual bool translate(const DriverEvent& in, PinballEvents& out) = 0;

    /// Runs the component's timed work and points at the pinball events it made.
    ///
    /// The default makes none.
    ///
    /// @param now  The time of this pass, as nowUs() gives it.
    /// @param out  Points at the pinball events made, none when it made none.
    /// @return     False when it made none.
    /// @note       The host calls it once per pass of the main loop, before the game's update().
    virtual bool update(uint32_t now, PinballEvents& out) {
        (void)now;
        (void)out;
        return false;
    }
};
