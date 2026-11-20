// The lighting driver of firmware/src/drivers/lighting.h for the tests that run on the computer:
// the same interface and profiles, with show() counting the frames instead of sending them.
#pragma once
#include <FastLED.h>
#include <cstdint>
#include <vector>

struct Profile {
    uint8_t perimeterStep;
    uint8_t ambientMax;
    uint8_t playfieldMax;
    uint32_t capMa;
};

class Lighting {
public:
    static constexpr uint8_t CHAINS = 6;

    // the driver's two profiles
    static constexpr Profile PROFILE_3A = {2, 102, 255, 2000};
    static constexpr Profile PROFILE_6A = {1, 128, 255, 2920};

    struct Settings {
        uint8_t adapterA = 3;
        uint16_t leds[CHAINS] = {};
    };

    void configure(const Settings& given) { settings = given; }

    bool begin() {
        for (uint8_t c = 0; c < CHAINS; ++c) {
            chains[c].assign(settings.leds[c], CRGB());
        }
        return true;
    }

    const Profile& profile() const { return current; }

    uint16_t leds(uint8_t c) const { return c < CHAINS ? uint16_t(chains[c].size()) : 0; }

    CRGB* chain(uint8_t c) { return c < CHAINS && !chains[c].empty() ? chains[c].data() : nullptr; }

    bool show() {
        ++shows;
        return true;
    }

    // what a test sets and reads
    Profile current = PROFILE_3A;
    uint32_t shows = 0;

private:
    Settings settings;
    std::vector<CRGB> chains[CHAINS];
};
