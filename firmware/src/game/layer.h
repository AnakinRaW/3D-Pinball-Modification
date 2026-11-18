// The layer a mode's lamps, slide and music lie on: the key the mode stack gave the mode, which
// no other running mode has, and the mode's priority, which places the layer above those of lower
// priority. Lamps, screen and sounds take a layer, so they need no knowledge of modes.
//
// Design: docs/firmware/game-abstraction.md, Modes.
#pragma once
#include <stdint.h>

struct Layer {
    uint8_t key;
    uint8_t priority;
};
