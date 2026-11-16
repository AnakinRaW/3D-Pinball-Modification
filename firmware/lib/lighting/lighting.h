// The lighting driver of docs/firmware/drivers/lighting.md. FastLED sends the six LED chains in
// parallel by DMA through ObjectFLED, its only driver, and holds the lighting under the cap of the
// adapter's profile with the strip's measured power model.
//
// Driver comes from firmware/lib/driver-base. The main task hands the adapter's rating from the
// config to configure() before begin().
//
// LightingChains takes the length of each chain, so a bench program can drive the LEDs it has.
// A chain of no LEDs stays out of FastLED, and its pin is left as it is. Lighting is the driver
// with the chains of the light layout.
#pragma once
#include <Arduino.h>
// FastLED 3.10.9's fl/gfx/colorutils.h turns warnings off twice and back on once, so without
// the push and pop -Wunused-parameter and -Wreturn-type stay off for all code after it.
#pragma GCC diagnostic push
#include <FastLED.h>   // FastLED 3.10.6 or newer, which names ObjectFLED slot 0 of FLEX_IO
#pragma GCC diagnostic pop
#include <debug-log.h>
#include <driver-base.h>

// what the lighting may light and how bright, for one adapter rating
struct Profile {
    uint8_t perimeterStep;   // 2 lights every second perimeter LED, 1 lights every one
    uint8_t ambientMax;      // the brightness the perimeters and the ceiling may reach
    uint8_t playfieldMax;    // the brightness the playfield lights may reach
    uint32_t capMa;          // the lighting's cap, in mA at 5 V
};

template <uint16_t LEDS_A, uint16_t LEDS_B, uint16_t LEDS_C, uint16_t LEDS_D, uint16_t LEDS_E,
          uint16_t LEDS_F>
class LightingChains : public Driver {
public:
    static constexpr uint8_t CHAINS = 6;
    static constexpr uint16_t CHAIN_LEDS[CHAINS] = {LEDS_A, LEDS_B, LEDS_C, LEDS_D, LEDS_E, LEDS_F};

    // The two profiles of design.md, Current ceiling: the playfield at full brightness, the
    // ambient lights at what the cap leaves in FastLED's sum with the whole layout at full white.
    static constexpr Profile PROFILE_3A = {2, 96, 255, 2000};
    static constexpr Profile PROFILE_6A = {1, 126, 255, 2930};

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

    // the config's lighting.adapter_a, 3 A by default
    struct Settings {
        uint8_t adapterA = ADAPTER_MIN_A;
    };

    // Takes the adapter's rating. A rating outside the range counts as the nearer end. The main
    // task calls it before begin(), and without it begin() takes the default.
    void configure(const Settings& settings) {
        if (settings.adapterA < ADAPTER_MIN_A || settings.adapterA > ADAPTER_MAX_A) {
            DEBUG_LOG("Lighting::configure: %u A lies outside %u to %u A\n", settings.adapterA,
                      ADAPTER_MIN_A, ADAPTER_MAX_A);
        }
        adapterA = settings.adapterA;
    }

    // Selects the profile for the adapter, adds the chains to FastLED on their pins and sets the
    // power model and the cap.
    bool begin() override {
        current = adapterA >= ADAPTER_MAX_A ? PROFILE_6A : PROFILE_3A;
        // ObjectFLED on QuadTimer4. Slot 1 is FlexIO2, which the display owns.
        FastLED.setExclusiveDriver<fl::Bus::FLEX_IO, 0>();
        if constexpr (LEDS_A > 0) {
            FastLED.addLeds<WS2812B, PIN_A, GRB>(chainA, LEDS_A);
        }
        if constexpr (LEDS_B > 0) {
            FastLED.addLeds<WS2812B, PIN_B, GRB>(chainB, LEDS_B);
        }
        if constexpr (LEDS_C > 0) {
            FastLED.addLeds<WS2812B, PIN_C, GRB>(chainC, LEDS_C);
        }
        if constexpr (LEDS_D > 0) {
            FastLED.addLeds<WS2812B, PIN_D, GRB>(chainD, LEDS_D);
        }
        if constexpr (LEDS_E > 0) {
            FastLED.addLeds<WS2812B, PIN_E, GRB>(chainE, LEDS_E);
        }
        if constexpr (LEDS_F > 0) {
            FastLED.addLeds<WS2812B, PIN_F, GRB>(chainF, LEDS_F);
        }
        FastLED.setPowerModel(PowerModelRGB(RED_MW, GREEN_MW, BLUE_MW, DARK_MW));
        FastLED.setMaxPowerInVoltsAndMilliamps(VOLTS, current.capMa);
        return true;
    }

    // Gets the profile the lighting runs with.
    const Profile& profile() const { return current; }

    // Gets the LEDs of chain c, 0 to 5, whose colors the next show() sends. Gets nullptr for any
    // other c.
    CRGB* chain(uint8_t c) {
        if (c >= CHAINS) {
            DEBUG_LOG("Lighting::chain: no chain %u\n", c);
            return nullptr;
        }
        return chains[c];
    }

    // Sends the colors of every LED by DMA. Returns at once.
    void show() { FastLED.show(); }

private:
    static constexpr uint8_t PIN_A = 5;    // pin-assignment.md: LED chain A
    static constexpr uint8_t PIN_B = 6;    // pin-assignment.md: LED chain B
    static constexpr uint8_t PIN_C = 7;    // pin-assignment.md: LED chain C
    static constexpr uint8_t PIN_D = 8;    // pin-assignment.md: LED chain D
    static constexpr uint8_t PIN_E = 9;    // pin-assignment.md: LED chain E
    static constexpr uint8_t PIN_F = 10;   // pin-assignment.md: LED chain F

    // A chain of no LEDs keeps one LED's room, which no show() sends.
    static constexpr uint16_t room(uint16_t leds) { return leds > 0 ? leds : 1; }

    uint8_t adapterA = ADAPTER_MIN_A;   // the rating configure() took
    Profile current = PROFILE_3A;
    CRGB chainA[room(LEDS_A)];
    CRGB chainB[room(LEDS_B)];
    CRGB chainC[room(LEDS_C)];
    CRGB chainD[room(LEDS_D)];
    CRGB chainE[room(LEDS_E)];
    CRGB chainF[room(LEDS_F)];
    CRGB* const chains[CHAINS] = {chainA, chainB, chainC, chainD, chainE, chainF};
};

// The chains of the light layout table in docs/parts/lighting/design.md. D to F grow by the
// playfield action lights once their places are fixed.
using Lighting = LightingChains<79, 79, 26, 21, 12, 7>;
