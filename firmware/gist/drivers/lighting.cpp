#include <FastLED.h>   // FastLED 3.10.6 or newer from the Library Manager, firmware/README.md

// what the lighting may light and how bright, for one adapter rating
struct Profile {
    uint8_t  perimeterStep;   // 2 lights every second perimeter LED, 1 lights every one
    uint8_t  ambientMax;      // the brightness the perimeters and the ceiling may reach
    uint8_t  playfieldMax;    // the brightness the playfield lights may reach
    uint32_t capMa;           // the lighting's cap, in mA at 5 V
};

class Lighting : public Driver {
public:
    // reads the adapter's rating from the card, adds the six chains to FastLED on their pins and sets the
    // power model and the cap
    bool begin() override {
        profile_ = readAdapter() == 6 ? k6A : k3A;
        FastLED.setExclusiveDriver<fl::Bus::FLEX_IO, 0>();   // ObjectFLED on QuadTimer4; slot 1 is FlexIO2, which the display owns
        FastLED.addLeds<WS2812B, kPinA, GRB>(a_, kLedsA);
        FastLED.addLeds<WS2812B, kPinB, GRB>(b_, kLedsB);
        FastLED.addLeds<WS2812B, kPinC, GRB>(c_, kLedsC);
        FastLED.addLeds<WS2812B, kPinD, GRB>(d_, kLedsD);
        FastLED.addLeds<WS2812B, kPinE, GRB>(e_, kLedsE);
        FastLED.addLeds<WS2812B, kPinF, GRB>(f_, kLedsF);
        FastLED.setPowerModel(PowerModelRGB(kRedMw, kGreenMw, kBlueMw, kDarkMw));
        FastLED.setMaxPowerInVoltsAndMilliamps(kVolts, profile_.capMa);
        return true;
    }

    // gets the profile the lighting runs with
    const Profile& profile() const { return profile_; }

    // gets the LEDs of chain c, 0 to 5, whose colors the next show() sends
    CRGB* chain(uint8_t c) { return chains_[c]; }

    // sends the colors of every LED by DMA; returns at once
    void show() { FastLED.show(); }

private:
    // the adapter's rated current in amperes from /adapter.txt, capped at 6; 3 for a lower rating,
    // a missing file, an unreadable card or content that is no whole number
    static uint8_t readAdapter() {
        char text[8] = {};
        FileRequest r;
        storage.open("/adapter.txt").read(0, text, sizeof(text) - 1, r);
        if (!r.wait() || r.bytes() == 0) return 3;
        char*      end;
        const long amps = strtol(text, &end, 10);
        if (end == text || (*end && !isspace(*end))) return 3;
        return amps >= 6 ? 6 : 3;
    }

    // the two profiles of the lighting design's current ceiling: the playfield at full brightness,
    // the ambient lights at what the cap leaves with the whole layout at full white
    static constexpr Profile k3A = {2, 78, 255, 2000};
    static constexpr Profile k6A = {1, 117, 255, 2930};

    static constexpr uint8_t kPinA = 5;    // pin-assignment.md: LED chain A
    static constexpr uint8_t kPinB = 6;    // pin-assignment.md: LED chain B
    static constexpr uint8_t kPinC = 7;    // pin-assignment.md: LED chain C
    static constexpr uint8_t kPinD = 8;    // pin-assignment.md: LED chain D
    static constexpr uint8_t kPinE = 9;    // pin-assignment.md: LED chain E
    static constexpr uint8_t kPinF = 10;   // pin-assignment.md: LED chain F

    // the light layout table in docs/parts/lighting/design.md; D to F grow by the playfield
    // action lights once their places are fixed
    static constexpr uint16_t kLedsA = 79, kLedsB = 79, kLedsC = 26;
    static constexpr uint16_t kLedsD = 17, kLedsE = 12, kLedsF = 7;

    // what one LED of the strip draws at full value, in mW at 5 V and rounded up, from the currents
    // measured in docs/research/Components/sezo-ws2812b-strip.md
    static constexpr uint8_t kRedMw = 28, kGreenMw = 31, kBlueMw = 31, kDarkMw = 9;
    static constexpr uint8_t kVolts = 5;

    Profile profile_ = k3A;
    CRGB a_[kLedsA], b_[kLedsB], c_[kLedsC], d_[kLedsD], e_[kLedsE], f_[kLedsF];
    CRGB* const chains_[6] = {a_, b_, c_, d_, e_, f_};
};
