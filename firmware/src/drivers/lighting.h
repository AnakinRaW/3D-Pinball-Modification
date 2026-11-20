// The lighting driver sends the colors of six LED chains under the cap of the adapter's profile.
// FastLED provides the colors and the strip's measured power model, by which the driver dims a
// frame that would draw past the cap. drivers/ws2812-flexio2.h sends the frames, all six chains
// at once, each on its pin.
//
// Driver comes from drivers/driver-base.h. The main task hands the adapter's rating and the LEDs
// of each chain from the config to configure() before begin(). A bench program configures the
// LEDs it has. A chain of no LEDs leaves its pin as it is.
//
// Design: docs/firmware/drivers/lighting.md.
#pragma once
#include <Arduino.h>
// FastLED 3.10.9's fl/gfx/colorutils.h turns warnings off twice and back on once, so without
// the push and pop -Wunused-parameter and -Wreturn-type stay off for all code after it.
#pragma GCC diagnostic push
#include <FastLED.h>
#pragma GCC diagnostic pop
#include "core/debug-log.h"
#include "drivers/driver-base.h"
#include "drivers/ws2812-flexio2.h"

// what the lighting may light and how bright, for one adapter rating
struct Profile {
    uint8_t perimeterStep;   // 2 lights every second perimeter LED, 1 lights every one
    uint8_t ambientMax;      // the brightness the perimeters and the ceiling may reach
    uint8_t playfieldMax;    // the brightness the playfield lights may reach
    uint32_t capMa;          // the lighting's cap, in mA at 5 V
};

class Lighting : public Driver {
public:
    static constexpr uint8_t CHAINS = 6;   // design.md: chains A to F
    static_assert(CHAINS <= Ws2812Flexio2::MAX_LANES, "every chain takes a FlexIO2 shifter");
    // the lighting model's n_chain_limit: one meter of the 160 LEDs/m strip
    static constexpr uint16_t MAX_CHAIN_LEDS = 160;

    // The two profiles of design.md, Current ceiling: the playfield at full brightness, the
    // ambient lights at what the cap leaves in FastLED's sum with the whole layout at full white.
    static constexpr Profile PROFILE_3A = {2, 102, 255, 2000};
    static constexpr Profile PROFILE_6A = {1, 128, 255, 2920};

    // What one LED of the strip draws at full value, in mW at 5 V and rounded up, from the
    // currents measured in docs/research/Custom/sezo-ws2812b-strip.md.
    static constexpr uint8_t RED_MW = 28;
    static constexpr uint8_t GREEN_MW = 31;
    static constexpr uint8_t BLUE_MW = 31;
    static constexpr uint8_t DARK_MW = 3;
    static constexpr uint8_t VOLTS = 5;   // the machine's rail

    // lighting.adapter_a's range, the two adapters the design derives
    static constexpr uint8_t ADAPTER_MIN_A = 3;
    static constexpr uint8_t ADAPTER_MAX_A = 6;

    // the config's lighting.adapter_a, 3 A by default, and each chain's LEDs, which the main task
    // takes from the lamp groups of the config
    struct Settings {
        uint8_t adapterA = ADAPTER_MIN_A;
        uint16_t leds[CHAINS] = {};
    };

    // Takes the adapter's rating and the LEDs of each chain. A value outside its range counts as
    // the nearer end. The main task calls it before begin(). Without it, begin() takes the 3 A
    // profile and no chain.
    void configure(const Settings& settings);

    // Selects the profile for the adapter, sets FastLED's power model, takes the colors from the
    // heap and starts the output on the chains with LEDs. Returns false when the heap has no room
    // or the output does not start. A later call keeps the chains of the first.
    bool begin() override;

    // Gets the profile the lighting runs with.
    const Profile& profile() const { return current; }

    // Gets how many LEDs chain c, 0 to 5, holds. Gets 0 for any other c.
    uint16_t leds(uint8_t c) const;

    // Gets the LEDs of chain c, 0 to 5, whose colors the next show() sends. Gets nullptr for any
    // other c and for a chain without LEDs.
    CRGB* chain(uint8_t c);

    // Scales the colors of every LED under the profile's cap and starts sending them. Returns at
    // once. Returns false, and sends nothing, before begin(), without LEDs and while the last
    // frame still goes out.
    bool show();

    // Gets the brightness the last show() scaled every color by, 255 where the cap dimmed nothing.
    uint8_t brightness() const { return shown; }

    // Gets the brightness of 0 to 255 at which the LEDs stay under a cap, by FastLED's power
    // model: 255 where they stay under it at full value.
    static uint8_t brightnessUnder(const CRGB* leds, uint16_t count, uint32_t capMa);

private:
    Settings settings;   // what configure() took
    Profile current = PROFILE_3A;
    bool allocated = false;
    uint16_t counts[CHAINS] = {};
    CRGB* chains[CHAINS] = {};
    CRGB* colors = nullptr;   // the LEDs of every chain, chain after chain
    uint16_t total = 0;       // the LEDs of every chain together
    uint8_t shown = 255;
    Ws2812Flexio2 output;
};
