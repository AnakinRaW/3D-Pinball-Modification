// The WebAssembly module of the browser simulator: the firmware's lamps component and lighting
// driver on a simulated clock. The browser places the lamp groups on their chains, starts effects
// on them, advances the time and reads each chain's colors as the LEDs would show them.
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <string>
#include <vector>
#include "components/lamps.h"
#include "drivers/lighting.h"
#include "logging/debug-log.h"

using emscripten::val;

namespace {

constexpr uint32_t US_PER_MS = 1000;   // one pass of the main loop advances the clock by 1 ms

// a palette the browser names, and FastLED's colors behind it
struct NamedPalette {
    const char* name;
    const TProgmemRGBPalette16* colors;
};
const NamedPalette PALETTES[] = {
    {"rainbow", &RainbowColors_p}, {"rainbowStripe", &RainbowStripeColors_p},
    {"party", &PartyColors_p},     {"ocean", &OceanColors_p},
    {"lava", &LavaColors_p},       {"forest", &ForestColors_p},
    {"cloud", &CloudColors_p},     {"heat", &HeatColors_p},
};

Lighting lighting;
Lamps lamps(lighting);
Lamps::Settings lampSettings;
Lighting::Settings lightingSettings;
// the arrays the lamp settings point into, which Lamps::begin() reads
std::vector<uint16_t> lampCounts[Lamps::GROUPS];
std::vector<uint16_t> gapPairs[Lamps::GROUPS];
std::vector<uint16_t> heightPairs[Lamps::GROUPS];
bool started = false;

bool has(const val& object, const char* key) {
    return object.hasOwnProperty(key) && !object[key].isNull() && !object[key].isUndefined();
}

// Gets a paint from the browser's description: {r, g, b}, {none: true} for the effect below, or
// {palette: "rainbow", by: "time" | "position" | "value"}. A missing paint shows the effect below.
Paint paintOf(const val& p) {
    if (p.isNull() || p.isUndefined() || (has(p, "none") && p["none"].as<bool>())) {
        return Color::NONE;
    }
    if (has(p, "palette")) {
        const std::string name = p["palette"].as<std::string>();
        const std::string by = has(p, "by") ? p["by"].as<std::string>() : "value";
        const By pick = by == "time" ? By::TIME : by == "position" ? By::POSITION : By::VALUE;
        for (const NamedPalette& np : PALETTES) {
            if (name == np.name) {
                return Paint(*np.colors, pick);
            }
        }
        DEBUG_LOG("simulator: no palette %s, the paint shows the effect below\n", name.c_str());
        return Color::NONE;
    }
    return Color{p["r"].as<uint8_t>(), p["g"].as<uint8_t>(), p["b"].as<uint8_t>(), false};
}

uint16_t number(const val& spec, const char* key, uint16_t fallback) {
    return has(spec, key) ? spec[key].as<uint16_t>() : fallback;
}

// Starts the effect a spec describes on a group of lamps, a LampRange or a Strip.
template <typename Group> bool start(Group group, const val& spec) {
    const std::string kind = spec["kind"].as<std::string>();
    const Paint a = paintOf(spec["a"]);
    const Paint b = paintOf(spec["b"]);
    if (kind == "set") {
        group.set(a);
    } else if (kind == "off") {
        group.off();
    } else if (kind == "blink") {
        const bool dark = has(spec, "dark") && spec["dark"].as<bool>();
        group.blink(a, number(spec, "onMs", 0), number(spec, "offMs", 0),
                    dark ? Off::DARK : Off::THROUGH);
    } else if (kind == "alternate") {
        group.alternate(a, b, number(spec, "onMs", 0), number(spec, "offMs", 0));
    } else if (kind == "pulse") {
        group.pulse(a, b, number(spec, "periodMs", 0));
    } else if (kind == "fade") {
        group.fade(a, number(spec, "ms", 0));
    } else if (kind == "run") {
        const std::string pattern = has(spec, "pattern") ? spec["pattern"].as<std::string>() : "";
        const Pattern p = pattern == "asteroid" ? Pattern::ASTEROID
                          : pattern == "wipe"   ? Pattern::WIPE
                                                : Pattern::CHASE;
        const bool down = has(spec, "down") && spec["down"].as<bool>();
        group.run(p, down ? Direction::DOWN : Direction::UP, a, number(spec, "step", 0), b);
    } else {
        DEBUG_LOG("simulator: no effect %s\n", kind.c_str());
        return false;
    }
    if (has(spec, "lastingMs")) {
        group.lasting(spec["lastingMs"].as<uint16_t>());
    }
    return true;
}

// Places the lamp groups on their chains and starts the driver and the lamps, as the main task
// does at the start. groups holds one entry for each group in the order of Lamps::Group, null for
// a group the config leaves out, else {chain: 0 to 5, leds, lamps: [counts], gaps: [first, count,
// ...], heights: [led, mm, ...]}. Returns false on a second call, on two groups on one chain and
// when a start fails.
bool setUp(uint8_t adapterA, val groups) {
    if (started) {
        DEBUG_LOG("simulator: the lamps are set up already\n");
        return false;
    }
    for (uint8_t g = 0; g < Lamps::GROUPS; ++g) {
        const val entry = groups[g];
        if (entry.isNull() || entry.isUndefined()) {
            continue;
        }
        Lamps::GroupSettings& s = lampSettings.groups[g];
        s.chain = entry["chain"].as<uint8_t>();
        s.leds = entry["leds"].as<uint16_t>();
        if (has(entry, "lamps")) {
            lampCounts[g] = emscripten::vecFromJSArray<uint16_t>(entry["lamps"]);
            s.lamps = lampCounts[g].data();
            s.lampCount = uint16_t(lampCounts[g].size());
        }
        if (has(entry, "gaps")) {
            gapPairs[g] = emscripten::vecFromJSArray<uint16_t>(entry["gaps"]);
            s.gaps = gapPairs[g].data();
            s.gapCount = uint16_t(gapPairs[g].size() / 2);
        }
        if (has(entry, "heights")) {
            heightPairs[g] = emscripten::vecFromJSArray<uint16_t>(entry["heights"]);
            s.heights = heightPairs[g].data();
            s.heightCount = uint16_t(heightPairs[g].size() / 2);
        }
    }
    if (!Lamps::chainLeds(lampSettings, lightingSettings.leds)) {
        return false;
    }
    lightingSettings.adapterA = adapterA;
    lighting.configure(lightingSettings);
    if (!lighting.begin()) {
        return false;
    }
    lamps.configure(lampSettings);
    if (!lamps.begin()) {
        return false;
    }
    started = true;
    return true;
}

// Selects the profile of another adapter, as a start with that rating in the config would.
bool setAdapter(uint8_t adapterA) {
    lightingSettings.adapterA = adapterA;
    lighting.configure(lightingSettings);
    return lighting.begin();
}

// Runs one pass of the main loop for every millisecond of ms. Gets how many frames the lamps
// handed the driver.
uint32_t step(uint32_t ms) {
    uint32_t frames = 0;
    for (uint32_t i = 0; i < ms; ++i) {
        GPT2_CNT = GPT2_CNT + US_PER_MS;
        fakeMillis = GPT2_CNT / US_PER_MS;
        if (lamps.update(nowUs())) {
            ++frames;
        }
        // the debug output goes to the browser's console, as the main loop sends it to USB serial
        sendDebugLog();
    }
    return frames;
}

uint32_t nowMs() {
    return nowUs() / US_PER_MS;
}

// Gets the colors chain c, 0 to 5, shows: three bytes for each LED, red, green, blue, after the
// cap's brightness. The view lies in the module's memory, so the browser copies it at once.
val chain(uint8_t c) {
    const Ws2812Flexio2* stage = Ws2812Flexio2::shown;
    if (stage == nullptr || c >= stage->lanes()) {
        return val::global("Uint8Array").new_(0);
    }
    const std::vector<uint8_t>& frame = stage->frame(c);
    return val(emscripten::typed_memory_view(frame.size(), frame.data()));
}

// Gets the brightness the cap scaled the last frame by, 255 where it dimmed nothing.
uint8_t capBrightness() {
    return lighting.brightness();
}

val profile() {
    const Profile& p = lighting.profile();
    val out = val::object();
    out.set("perimeterStep", p.perimeterStep);
    out.set("ambientMax", p.ambientMax);
    out.set("playfieldMax", p.playfieldMax);
    out.set("capMa", p.capMa);
    return out;
}

// Starts an effect. spec names the target, "playfield" with first and count or a strip:
// "perimeterLeft", "perimeterRight", "ceiling", "perimeter" or "ambient". Its key and priority
// give the layer, kind the effect, a and b the paints. skipGaps: true lets a strip's run jump its
// hidden LEDs. Gets false for an unknown target or kind.
bool effect(val spec) {
    const Layer layer{spec["key"].as<uint8_t>(), spec["priority"].as<uint8_t>()};
    LampGroups g = lamps.groups(layer);
    const std::string target = spec["target"].as<std::string>();
    if (target == "playfield") {
        return start(
            g.playfield.sub(number(spec, "first", 0), number(spec, "count", g.playfield.size())),
            spec);
    }
    Strip* strip = target == "perimeterLeft"    ? &g.perimeterLeft
                   : target == "perimeterRight" ? &g.perimeterRight
                   : target == "ceiling"        ? &g.ceiling
                   : target == "perimeter"      ? &g.perimeter
                   : target == "ambient"        ? &g.ambient
                                                : nullptr;
    if (strip == nullptr) {
        DEBUG_LOG("simulator: no target %s\n", target.c_str());
        return false;
    }
    // a run that jumps the hidden LEDs, where the spec asks for it
    if (has(spec, "skipGaps") && spec["skipGaps"].as<bool>()) {
        return start(strip->skipGaps(), spec);
    }
    return start(*strip, spec);
}

void removeLayer(uint8_t key) {
    lamps.remove(Layer{key, 0});
}

void clearLamps() {
    lamps.clear();
}

void setBrightness(float playfield, float ambient) {
    lamps.brightness(playfield, ambient);
}

uint32_t refused() {
    return lamps.refused();
}

uint16_t playfieldLamps() {
    return lamps.groups(Layer{0, 0}).playfield.size();
}

}   // namespace

EMSCRIPTEN_BINDINGS(simulator) {
    emscripten::function("setUp", &setUp);
    emscripten::function("setAdapter", &setAdapter);
    emscripten::function("step", &step);
    emscripten::function("nowMs", &nowMs);
    emscripten::function("chain", &chain);
    emscripten::function("capBrightness", &capBrightness);
    emscripten::function("profile", &profile);
    emscripten::function("effect", &effect);
    emscripten::function("removeLayer", &removeLayer);
    emscripten::function("clearLamps", &clearLamps);
    emscripten::function("setBrightness", &setBrightness);
    emscripten::function("refused", &refused);
    emscripten::function("playfieldLamps", &playfieldLamps);
}
