// Draws what the machine reports: the warning lights with a tile per device, and the list of the
// warnings the logger holds.
//
// Design: docs/firmware/host-screens.md, Machine and service.
#pragma once
#include "controls.h"
#include "view.h"

/// The warning lights: a tile per device, three across, each with its bulb.
class WarningLightsView : public View {
public:
    static constexpr uint8_t TILES_MAX = 12;   // host-screens.md: twelve tiles

    /// What the player can press. TILE comes with the tile's index.
    enum Action : uint8_t { BACK, TILE };

    /// A tile: a device and what its bulb shows, or the warnings in amber.
    struct Tile {
        const char* name = "";
        controls::Light light = controls::Light::WORKS;   // WORKS, FAILED or WARNING
    };

    /// @param count  The tiles, up to TILES_MAX.
    void build(lv_obj_t* parent, const Tile* tiles, uint8_t count, Pressed pressed, void* context);
};

/// The warnings since start, equal lines counted, in a list that scrolls.
class WarningsView : public View {
public:
    /// What the player can press.
    enum Action : uint8_t { BACK };

    /// A warning: where it came from, what it says and how often it came.
    struct Warning {
        const char* source = "";   // such as "Config"
        const char* text = "";
        uint16_t count = 1;
    };

    void build(lv_obj_t* parent, const Warning* warnings, uint8_t count, Pressed pressed,
               void* context);
};
