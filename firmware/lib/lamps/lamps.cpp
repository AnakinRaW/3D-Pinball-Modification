#include "lamps.h"
#include <new>

// the lighting model's pitch_strip: 6.25 mm from LED to LED at 160 LEDs/m, in quarter millimeters
constexpr uint32_t PITCH_Q = 25;
constexpr uint32_t US_PER_MS = 1000;
constexpr uint8_t FULL = 255;   // the value of an effect's second color
// a palette by time moves one of its 256 steps per frame
constexpr uint32_t PALETTE_STEP_MS = Lamps::FRAME_US / US_PER_MS;
constexpr float HALF = 0.5f;   // rounds a scale to the nearest step

static CRGB toCrgb(Color c) {
    return CRGB(c.r, c.g, c.b);
}

static Color toColor(const CRGB& c) {
    return {c.r, c.g, c.b, false};
}

LampRange LampRange::sub(uint16_t from, uint16_t n) const {
    if (from >= count) {
        return {lamps, uint16_t(first + count), 0, layer};
    }
    const uint16_t left = count - from;
    return {lamps, uint16_t(first + from), n < left ? n : left, layer};
}

LampRange& LampRange::fade(Paint c, uint16_t ms) {
    for (uint16_t i = 0; i < count; ++i) {
        const Color now = toColor(lamps->playfieldFrame[first + i]);
        Lamps::Entry* e = lamps->start(lamps->playfieldEffects[first + i], layer.key,
                                       layer.priority, now, c, Fade(ms));
        if (e == nullptr) {
            DEBUG_LOG("Lamps: lamp %u holds five effects and refuses a fade of key %u\n", first + i,
                      layer.key);
            continue;
        }
        e->index = i;
        e->count = count;
    }
    return *this;
}

LampRange& LampRange::run(Pattern p, Direction d, Paint c, uint16_t stepMs, Paint back) {
    if (stepMs == 0) {
        DEBUG_LOG("Lamps: a run of key %u takes 1 ms a lamp in place of 0\n", layer.key);
        stepMs = 1;
    }
    return play(Run(p, d, stepMs), back, c);
}

void LampRange::lasting(uint16_t ms) {
    const uint32_t endMs = lamps->clockAt(nowUs()) + ms;
    for (uint16_t i = 0; i < count; ++i) {
        Lamps::Entry* e = lamps->playfieldEffects[first + i].find(
            [this](const Lamps::Entry& x) { return x.key == layer.key; });
        if (e != nullptr) {
            e->ends = true;
            e->endMs = endMs;
        }
    }
}

void LampRange::clear() {
    for (uint16_t i = 0; i < count; ++i) {
        lamps->playfieldEffects[first + i].removeIf(
            [this](const Lamps::Entry& x) { return x.key == layer.key; });
    }
}

uint16_t Strip::size() const {
    return lamps->stripSize(strip);
}

Strip& Strip::fade(Paint c, uint16_t ms) {
    if (size() == 0) {
        return *this;
    }
    const Color now = toColor(lamps->ambientFrame[lamps->locate(strip, 0).led]);
    return play(Fade(ms), now, c);
}

Strip& Strip::run(Pattern p, Direction d, Paint c, uint16_t mmPerS, Paint back) {
    return play(Run(p, d, mmPerS), back, c);
}

void Strip::lasting(uint16_t ms) {
    Lamps::Entry* e = lamps->stripEffects[strip].find(
        [this](const Lamps::Entry& x) { return x.key == layer.key; });
    if (e != nullptr) {
        e->ends = true;
        e->endMs = lamps->clockAt(nowUs()) + ms;
    }
}

void Strip::clear() {
    lamps->stripEffects[strip].removeIf(
        [this](const Lamps::Entry& x) { return x.key == layer.key; });
}

bool Lamps::Entry::above(const Entry& other) const {
    if (priority != other.priority) {
        return priority > other.priority;
    }
    return int32_t(seq - other.seq) > 0;
}

bool Lamps::begin() {
    if (playfieldEffects != nullptr || ambientFrame != nullptr) {
        DEBUG_LOG("Lamps::begin: keeps the room of the first begin()\n");
        return true;
    }
    bool taken[Lighting::CHAINS] = {};
    for (uint8_t g = 0; g < GROUPS; ++g) {
        const uint8_t chain = settings.groups[g].chain;
        if (chain == NO_CHAIN) {
            continue;
        }
        if (chain >= Lighting::CHAINS || taken[chain]) {
            DEBUG_LOG("Lamps::begin: group %u finds chain %u past F or taken, and holds no lamps\n",
                      g, chain);
            continue;
        }
        taken[chain] = true;
        groupChain[g] = chain;
        groupLeds[g] = lighting.leds(chain);
    }
    const uint16_t ambientLeds =
        groupLeds[GROUP_LEFT] + groupLeds[GROUP_RIGHT] + groupLeds[GROUP_CEILING];
    for (uint8_t g = GROUP_LEFT; g < FIRST_SECTION; ++g) {
        visible[g] = new (std::nothrow) uint16_t[groupLeds[g] > 0 ? groupLeds[g] : 1];
        if (visible[g] == nullptr) {
            DEBUG_LOG("Lamps::begin: no room for the LEDs of group %u\n", g);
            return false;
        }
        visibleCount[g] = 0;
        for (uint16_t led = 0; led < groupLeds[g]; ++led) {
            if (!hidden(g, led)) {
                visible[g][visibleCount[g]++] = led;
            }
        }
    }
    const uint16_t lamps = walk(nullptr);
    playfieldEffects = new (std::nothrow) Effects[lamps];
    playfieldFrame = new (std::nothrow) CRGB[lamps]();
    spans = new (std::nothrow) Span[lamps]();
    ambientFrame = new (std::nothrow) CRGB[ambientLeds]();
    if (playfieldEffects == nullptr || playfieldFrame == nullptr || spans == nullptr ||
        ambientFrame == nullptr) {
        DEBUG_LOG("Lamps::begin: no room for %u playfield lamps and %u ambient LEDs\n", lamps,
                  ambientLeds);
        return false;
    }
    walk(spans);
    playfieldLamps = lamps;
    return true;
}

// A count of a section's lamps takes that many LEDs of its chain for one lamp. A hidden LED
// belongs to no lamp: a lamp starts at the next LED that is not hidden, and a gap or the chain's
// end cuts a lamp short. The LEDs past the last count are one lamp each, and the counts past the
// last LED go unused.
uint16_t Lamps::walk(Span* out) const {
    uint16_t lamps = 0;
    for (uint8_t g = FIRST_SECTION; g < GROUPS; ++g) {
        const GroupSettings& section = settings.groups[g];
        uint16_t next = 0;   // the next count of the section
        uint16_t led = 0;
        while (led < groupLeds[g]) {
            if (hidden(g, led)) {
                ++led;
                continue;
            }
            uint16_t leds = 1;
            if (next < section.lampCount) {
                leds = section.lamps[next++];
                if (leds == 0) {
                    DEBUG_LOG("Lamps: playfield lamp %u counts as 1 LED in place of 0\n", lamps);
                    leds = 1;
                }
            }
            uint16_t run = 0;
            while (run < leds && led + run < groupLeds[g] && !hidden(g, led + run)) {
                ++run;
            }
            if (run < leds) {
                DEBUG_LOG("Lamps: playfield lamp %u ends at a gap or its chain's end, at %u LEDs\n",
                          lamps, run);
                leds = run;
            }
            if (out != nullptr) {
                out[lamps] = {groupChain[g], led, leds};
            }
            led += leds;
            ++lamps;
        }
        if (next < section.lampCount) {
            DEBUG_LOG("Lamps: %u lamp counts of group %u find no LEDs\n", section.lampCount - next,
                      g);
        }
    }
    return lamps;
}

bool Lamps::chainLeds(const Settings& settings, uint16_t (&leds)[Lighting::CHAINS]) {
    bool taken[Lighting::CHAINS] = {};
    bool placed = true;
    for (uint16_t& n : leds) {
        n = 0;
    }
    for (uint8_t g = 0; g < GROUPS; ++g) {
        const GroupSettings& group = settings.groups[g];
        if (group.chain == NO_CHAIN) {
            continue;
        }
        if (group.chain >= Lighting::CHAINS || taken[group.chain]) {
            DEBUG_LOG("Lamps::chainLeds: group %u finds chain %u past F or taken\n", g,
                      group.chain);
            placed = false;
            continue;
        }
        taken[group.chain] = true;
        leds[group.chain] = group.leds;
    }
    return placed;
}

LampGroups Lamps::groups(Layer layer) {
    LampGroups g;
    g.perimeterLeft = Strip(this, LEFT, layer);
    g.perimeterRight = Strip(this, RIGHT, layer);
    g.ceiling = Strip(this, CEILING, layer);
    g.perimeter = Strip(this, PERIMETER, layer);
    g.ambient = Strip(this, AMBIENT, layer);
    g.playfield = LampRange(this, 0, playfieldLamps, layer);
    return g;
}

void Lamps::remove(Layer layer) {
    const uint8_t key = layer.key;
    const auto ofKey = [key](const Entry& e) { return e.key == key; };
    for (uint16_t lamp = 0; lamp < playfieldLamps; ++lamp) {
        playfieldEffects[lamp].removeIf(ofKey);
    }
    for (Effects& strip : stripEffects) {
        strip.removeIf(ofKey);
    }
}

void Lamps::clear() {
    for (uint16_t lamp = 0; lamp < playfieldLamps; ++lamp) {
        playfieldEffects[lamp].clear();
    }
    for (Effects& strip : stripEffects) {
        strip.clear();
    }
    playfieldShare = 1;
    ambientShare = 1;
}

static float share(float value, const char* what) {
    if (value >= 0 && value <= 1) {
        return value;
    }
    const float nearer = value > 1 ? 1 : 0;
    DEBUG_LOG("Lamps::brightness: the %s brightness counts as %d\n", what, int(nearer));
    return nearer;
}

void Lamps::brightness(float playfield, float ambient) {
    playfieldShare = share(playfield, "playfield");
    ambientShare = share(ambient, "ambient");
}

bool Lamps::update(uint32_t now) {
    if (ambientFrame == nullptr) {
        return false;
    }
    if (framed && now - shownUs < FRAME_US) {
        return false;
    }
    framed = true;
    shownUs = now;
    const uint32_t ms = clockAt(now);
    expire(ms);
    paintPlayfield(ms);
    paintStrips(ms);
    write();
    lighting.show();
    return true;
}

uint8_t Lamps::step() const {
    const uint8_t s = lighting.profile().perimeterStep;
    return s == 0 ? 1 : s;
}

uint32_t Lamps::pitchQ() const {
    return PITCH_Q * step();
}

// Gets how many lamps a perimeter group holds, every step-th of its visible LEDs from its first.
uint16_t Lamps::sideLamps(uint8_t group) const {
    return (visibleCount[group] + step() - 1) / step();
}

// Gets whether one of a group's gaps holds led.
bool Lamps::hidden(uint8_t group, uint16_t led) const {
    const GroupSettings& g = settings.groups[group];
    for (uint16_t k = 0; k < g.gapCount; ++k) {
        const uint16_t first = g.gaps[2 * k];
        if (led >= first && led - first < g.gaps[2 * k + 1]) {
            return true;
        }
    }
    return false;
}

uint16_t Lamps::stripSize(uint8_t strip) const {
    switch (strip) {
    case LEFT:
        return sideLamps(GROUP_LEFT);
    case RIGHT:
        return sideLamps(GROUP_RIGHT);
    case CEILING:
        return visibleCount[GROUP_CEILING];
    case PERIMETER:
        return sideLamps(GROUP_LEFT) + sideLamps(GROUP_RIGHT);
    case AMBIENT:
        return sideLamps(GROUP_LEFT) + visibleCount[GROUP_CEILING] + sideLamps(GROUP_RIGHT);
    }
    return 0;
}

// The perimeters' chains start at the playfield's lower end and the ceiling's at its left end. A
// lamp's position counts the lamps before it, so a run crosses a gap without a pause. A path joins
// its strips one lamp spacing apart and runs down the right perimeter from its top.
Lamps::Place Lamps::locate(uint8_t strip, uint16_t i) const {
    const uint8_t s = step();
    const uint32_t pitch = pitchQ();
    const uint16_t left = sideLamps(GROUP_LEFT);
    const uint16_t right = sideLamps(GROUP_RIGHT);
    const uint16_t ceiling = visibleCount[GROUP_CEILING];
    const uint16_t* leftLeds = visible[GROUP_LEFT];
    const uint16_t* rightLeds = visible[GROUP_RIGHT];
    const uint16_t* ceilingLeds = visible[GROUP_CEILING];
    // where the right perimeter and the ceiling begin in the ambient frame
    const uint16_t rightLed = groupLeds[GROUP_LEFT];
    const uint16_t ceilingLed = rightLed + groupLeds[GROUP_RIGHT];
    switch (strip) {
    case LEFT:
        return {leftLeds[i * s], i * pitch};
    case RIGHT:
        return {uint16_t(rightLed + rightLeds[i * s]), i * pitch};
    case CEILING:
        return {uint16_t(ceilingLed + ceilingLeds[i]), i * PITCH_Q};
    }
    if (i < left) {
        return {leftLeds[i * s], i * pitch};
    }
    i -= left;
    uint32_t start = left * pitch;   // where the path's next part begins
    if (strip == AMBIENT) {
        if (i < ceiling) {
            return {uint16_t(ceilingLed + ceilingLeds[i]), start + i * PITCH_Q};
        }
        i -= ceiling;
        start += ceiling == 0 ? 0 : (ceiling - 1) * PITCH_Q + pitch;
    }
    return {uint16_t(rightLed + rightLeds[(right - 1 - i) * s]), start + i * pitch};
}

uint32_t Lamps::stripLength(uint8_t strip) const {
    const uint16_t size = stripSize(strip);
    return size == 0 ? 0 : locate(strip, uint16_t(size - 1)).q;
}

uint32_t Lamps::clockAt(uint32_t now) {
    if (!clockStarted) {
        clockStarted = true;
        clockUs = now;
    }
    if (!before(now, clockUs)) {
        const uint32_t passedMs = (now - clockUs) / US_PER_MS;
        clockMs += passedMs;
        clockUs += passedMs * US_PER_MS;
    }
    return clockMs;
}

Lamps::Entry* Lamps::claim(Effects& effects, uint8_t key, uint8_t priority) {
    // A mode's new effect takes the place of its own one.
    effects.removeIf([key](const Entry& e) { return e.key == key; });
    Entry e;
    e.key = key;
    e.priority = priority;
    e.seq = ++seq;
    e.startMs = clockAt(nowUs());
    Entry* placed = effects.insert(e);
    if (placed == nullptr) {
        ++refusedCount;
    }
    return placed;
}

void Lamps::expire(uint32_t ms) {
    const auto over = [ms](const Entry& e) { return e.ends && !before(ms, e.endMs); };
    for (uint16_t lamp = 0; lamp < playfieldLamps; ++lamp) {
        playfieldEffects[lamp].removeIf(over);
    }
    for (Effects& strip : stripEffects) {
        strip.removeIf(over);
    }
}

// Each lamp mixes its effects from the lowest up, so the top one mixes last.
void Lamps::paintPlayfield(uint32_t ms) {
    for (uint16_t lamp = 0; lamp < playfieldLamps; ++lamp) {
        const Effects& effects = playfieldEffects[lamp];
        CRGB c(0, 0, 0);
        for (size_t k = effects.size(); k-- > 0;) {
            const Entry& e = effects[k];
            c = mix(e, ms, Spot{e.index, e.count, 0, 0, 0}, c);
        }
        playfieldFrame[lamp] = c;
    }
}

// The strips share their LEDs, so the effects of every strip paint in one order, the lowest
// first, each over what the ones below it painted.
void Lamps::paintStrips(uint32_t ms) {
    const uint16_t ambientLeds =
        groupLeds[GROUP_LEFT] + groupLeds[GROUP_RIGHT] + groupLeds[GROUP_CEILING];
    for (uint16_t led = 0; led < ambientLeds; ++led) {
        ambientFrame[led] = CRGB(0, 0, 0);
    }
    const Entry* layers[STRIPS * MAX_EFFECTS];
    uint8_t layerStrips[STRIPS * MAX_EFFECTS];
    uint8_t n = 0;
    for (uint8_t strip = 0; strip < STRIPS; ++strip) {
        const Effects& effects = stripEffects[strip];
        for (size_t k = 0; k < effects.size(); ++k) {
            const Entry& e = effects[k];
            uint8_t at = n++;
            while (at > 0 && layers[at - 1]->above(e)) {
                layers[at] = layers[at - 1];
                layerStrips[at] = layerStrips[at - 1];
                --at;
            }
            layers[at] = &e;
            layerStrips[at] = strip;
        }
    }
    const uint32_t pitch = pitchQ();
    for (uint8_t k = 0; k < n; ++k) {
        const uint8_t strip = layerStrips[k];
        const uint16_t size = stripSize(strip);
        const uint32_t length = stripLength(strip);
        for (uint16_t i = 0; i < size; ++i) {
            const Place p = locate(strip, i);
            CRGB& led = ambientFrame[p.led];
            led = mix(*layers[k], ms, Spot{i, size, p.q, length, pitch}, led);
        }
    }
}

// Writes the frame into the chains at the brightness the game set, as a share of the profile's
// most.
void Lamps::write() {
    const Profile& profile = lighting.profile();
    const uint8_t playfieldScale = uint8_t(profile.playfieldMax * playfieldShare + HALF);
    const uint8_t ambientScale = uint8_t(profile.ambientMax * ambientShare + HALF);
    uint16_t led = 0;
    for (uint8_t g = GROUP_LEFT; g < FIRST_SECTION; ++g) {
        CRGB* chain = groupLeds[g] > 0 ? lighting.chain(groupChain[g]) : nullptr;
        for (uint16_t i = 0; i < groupLeds[g]; ++i) {
            chain[i] = ambientFrame[led++];
            chain[i].nscale8(ambientScale);
        }
    }
    for (uint16_t lamp = 0; lamp < playfieldLamps; ++lamp) {
        const Span& s = spans[lamp];
        CRGB color = playfieldFrame[lamp];
        color.nscale8(playfieldScale);
        CRGB* chain = lighting.chain(s.chain);
        for (uint16_t i = 0; i < s.leds; ++i) {
            chain[s.led + i] = color;
        }
    }
}

CRGB Lamps::paint(const Paint& p, CRGB below, uint8_t value, uint8_t place, uint32_t elapsedMs) {
    if (p.palette != nullptr) {
        uint8_t index = value;
        if (p.by == By::TIME) {
            index = uint8_t(elapsedMs / PALETTE_STEP_MS);
        } else if (p.by == By::POSITION) {
            index = place;
        }
        return ColorFromPalette(*p.palette, index);
    }
    return p.color.below ? below : toCrgb(p.color);
}

CRGB Lamps::mix(const Entry& e, uint32_t ms, const Spot& at, CRGB below) {
    const uint32_t elapsedMs = ms - e.startMs;
    const uint8_t value = e.effect->value(elapsedMs, at);
    uint8_t place = 0;
    if (at.lengthQ > 0) {
        place = uint8_t(uint64_t(at.mmQ) * FULL / at.lengthQ);
    } else if (at.count > 1) {
        place = uint8_t(uint32_t(at.index) * FULL / (at.count - 1));
    }
    return blend(paint(e.from, below, value, place, elapsedMs),
                 paint(e.to, below, value, place, elapsedMs), value);
}
