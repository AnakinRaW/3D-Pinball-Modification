#include "lighting.h"
#include "logging/logger.h"
#include <inttypes.h>
#include <new>

constexpr uint8_t PINS[] = {32, 6, 7, 8, 9, 10};   // pin-assignment.md: LED chain A to F
static_assert(sizeof PINS == Lighting::CHAINS, "a pin for every chain");
constexpr uint8_t FULL_BRIGHTNESS = 255;   // FastLED's brightness at full value
// show() hands the output each chain's colors as bytes, three to an LED, red, green, blue
static_assert(sizeof(CRGB) == 3, "FastLED's CRGB holds an LED in three bytes");

void Lighting::configure(const Settings& given) {
    settings = given;
    if (given.adapterA < ADAPTER_MIN_A || given.adapterA > ADAPTER_MAX_A) {
        logWarning("lighting", "%u A lies outside %u to %u A", given.adapterA, ADAPTER_MIN_A,
                   ADAPTER_MAX_A);
    }
    for (uint8_t c = 0; c < CHAINS; ++c) {
        if (given.leds[c] > MAX_CHAIN_LEDS) {
            logWarning("lighting", "chain %u counts as %u LEDs in place of %u", c, MAX_CHAIN_LEDS,
                       given.leds[c]);
            settings.leds[c] = MAX_CHAIN_LEDS;
        }
    }
}

bool Lighting::begin() {
    current = settings.adapterA >= ADAPTER_MAX_A ? PROFILE_6A : PROFILE_3A;
    logInfo("lighting", "%u A adapter, the LEDs capped at %" PRIu32 " mA", settings.adapterA,
            current.capMa);
    FastLED.setPowerModel(PowerModelRGB(RED_MW, GREEN_MW, BLUE_MW, DARK_MW));
    // The colors come from the heap, once, during the start, so a later begin() keeps the chains
    // of the first.
    if (!allocated) {
        for (uint8_t c = 0; c < CHAINS; ++c) {
            total += settings.leds[c];
        }
        if (total > 0) {
            colors = new (std::nothrow) CRGB[total]();
            if (colors == nullptr) {
                logWarning("lighting", "no room for the colors of %u LEDs", total);
                total = 0;
                return false;
            }
        }
        uint16_t first = 0;
        for (uint8_t c = 0; c < CHAINS; ++c) {
            counts[c] = settings.leds[c];
            chains[c] = counts[c] > 0 ? colors + first : nullptr;
            first += counts[c];
        }
        allocated = true;
    }
    for (uint8_t c = 0; c < CHAINS; ++c) {
        if (counts[c] != settings.leds[c]) {
            DEBUG_LOG("Lighting::begin: chain %u keeps the %u LEDs of the first begin()\n", c,
                      counts[c]);
        }
    }
    if (total == 0) {
        return true;
    }
    Ws2812Flexio2::Lane lanes[CHAINS];
    for (uint8_t c = 0; c < CHAINS; ++c) {
        lanes[c] = {PINS[c], counts[c]};
    }
    return output.begin(lanes, CHAINS);
}

uint16_t Lighting::leds(uint8_t c) const {
    return c < CHAINS ? counts[c] : 0;
}

CRGB* Lighting::chain(uint8_t c) {
    if (c >= CHAINS) {
        DEBUG_LOG("Lighting::chain: no chain %u\n", c);
        return nullptr;
    }
    return chains[c];
}

uint8_t Lighting::brightnessUnder(const CRGB* leds, uint16_t count, uint32_t capMa) {
    return calculate_max_brightness_for_power_mW(leds, count, FULL_BRIGHTNESS, capMa * VOLTS);
}

bool Lighting::show() {
    if (total == 0) {
        DEBUG_LOG("Lighting::show: no LEDs to send\n");
        return false;
    }
    const uint8_t brightness = brightnessUnder(colors, total, current.capMa);
    const uint8_t* rgb[CHAINS];
    for (uint8_t c = 0; c < CHAINS; ++c) {
        rgb[c] = reinterpret_cast<const uint8_t*>(chains[c]);
    }
    if (!output.send(rgb, brightness)) {
        return false;
    }
    shown = brightness;
    return true;
}
