// Tests on the Teensy the rules every driver follows, on every driver of firmware/src/drivers. Each
// driver stands in the register below with a function for each row, and a row runs every driver's
// function in turn. A driver that has no case for a row is named in the run's output.
//
//   before-begin  every call before begin() answers at once with false or empty data
//   out-of-range  a setting past its range behaves as the nearer end of the range
//   at-once       a call that starts a long transfer or a move returns while it still runs
//   begin-fails   a driver whose begin() failed answers every later call at once, with no data
//
// begin-fails takes every slot of the driver tick and keeps them, so it runs last and relies on the
// restart before the program.
//
// Wiring: none. The lighting runs on chain F, which has no LEDs at the bench, and sends dark
// frames. Chain A and its LEDs stay untouched. A servo on the servo pin stays at its start
// position. The display, where one is connected, shows black and then a dark gray column at its
// left edge. The audio driver plays a file that holds no sound, so a speaker stays silent.

#include <Arduino.h>
#include <stdarg.h>
#include <string.h>
// FastLED's fl/gfx/colorutils.h turns warnings off twice and back on once, so without
// the push and pop -Wunused-parameter and -Wreturn-type stay off for all code after it.
#pragma GCC diagnostic push
#include <FastLED.h>
#pragma GCC diagnostic pop
#include "core/event-queue.h"
#include "drivers/audio.h"
#include "drivers/break-beam.h"
#include "drivers/display.h"
#include "drivers/lighting.h"
#include "drivers/servo-driver.h"
#include "drivers/storage.h"
#include "drivers/touch.h"
#include "drivers/ws2812-color-encoder.h"
#include "test-drivers.h"
#include "test-harness.h"

// the frame: a WS2812B takes 24 bits, and the lighting driver sends a bit as one symbol
constexpr uint32_t BITS_PER_LED = ws2812::BITS_PER_LED;
constexpr uint32_t BIT_NS = ws2812::SYMBOL_BITS * ws2812::SYMBOL_NS;
constexpr uint32_t NS_PER_US = 1000;
constexpr uint32_t SETTLE_MS = 10;    // longer than a frame, so no transfer is pending
constexpr uint32_t AT_ONCE_US = 50;   // what a call that only sets a value may take
constexpr uint32_t LIMIT_S = 10;      // the longest each test takes, with room to spare
constexpr size_t TEXT = 96;           // the room for what a test saw or wanted

// the chain the lighting runs on, F, which has no LEDs at the bench
constexpr uint8_t BENCH_CHAIN = Lighting::CHAINS - 1;
constexpr uint16_t TOO_MANY_LEDS = 1000;   // more LEDs than a chain takes
// adapter ratings below and above the two ratings
constexpr uint8_t ADAPTER_BELOW_A = Lighting::ADAPTER_MIN_A - 1;
constexpr uint8_t ADAPTER_ABOVE_A = Lighting::ADAPTER_MAX_A + 3;
// an angle past ServoDriver::ANGLE_LIMIT_DEG, and speeds below and above its range
constexpr uint8_t TOO_FAR_DEG = 255;
constexpr uint32_t TOO_SLOW_DEG_S = 0;
constexpr uint32_t TOO_FAST_DEG_S = 5000;

// What a driver's case saw and what it wanted, where it failed.
struct Verdict {
    char seen[TEXT] = "";
    char wanted[TEXT] = "";

    // Notes what the case wanted.
    __attribute__((format(printf, 2, 3))) Verdict& want(const char* format, ...) {
        va_list args;
        va_start(args, format);
        vsnprintf(wanted, sizeof wanted, format, args);
        va_end(args);
        return *this;
    }

    // Notes what the case saw. Returns false, which the case returns as its failure.
    __attribute__((format(printf, 2, 3))) bool saw(const char* format, ...) {
        va_list args;
        va_start(args, format);
        vsnprintf(seen, sizeof seen, format, args);
        va_end(args);
        return false;
    }
};

// One driver of the register: its name and a function for each row, nullptr where the row has no
// case for it. A function returns true where the driver keeps the rule.
struct DriverCase {
    const char* name;
    bool (*beforeBegin)(Verdict&);
    bool (*outOfRange)(Verdict&);
    bool (*atOnce)(Verdict&);
    bool (*beginFails)(Verdict&);
};

// Gets how long fn takes, in microseconds.
template <typename F> static uint32_t timedUs(F fn) {
    const uint32_t t0Us = micros();
    fn();
    return micros() - t0Us;
}

// Gets whether a call that took us microseconds answered at once.
static bool atOnce(uint32_t us, const char* call, Verdict& v) {
    return us <= AT_ONCE_US || v.want("%lu us at most", AT_ONCE_US).saw("%s took %lu us", call, us);
}

// ---- the lighting driver

static Lighting lightingFresh;   // before-begin: never begun
// out-of-range and at-once: the shared lighting driver, the bench chain past its bound
using test::lighting;
static bool lightingBegun = false;

// an adapter rating past the range and the cap of the profile it counts as
struct AdapterCase {
    uint8_t adapterA;
    uint32_t capMa;
};
static const AdapterCase ADAPTERS[] = {
    {ADAPTER_BELOW_A, Lighting::PROFILE_3A.capMa},
    {ADAPTER_ABOVE_A, Lighting::PROFILE_6A.capMa},
};

// Configures the lighting with an adapter rating and too many LEDs on the bench chain, and begins
// it. A later begin() keeps the chains of the first.
static bool beginLighting(uint8_t adapterA) {
    Lighting::Settings settings;
    settings.adapterA = adapterA;
    settings.leds[BENCH_CHAIN] = TOO_MANY_LEDS;
    lighting.configure(settings);
    lightingBegun = lighting.begin();
    return lightingBegun;
}

static bool lightingBeforeBegin(Verdict& v) {
    bool shown = true;
    const uint32_t us = timedUs([&] { shown = lightingFresh.show(); });
    if (shown || lightingFresh.chain(0) != nullptr || lightingFresh.leds(0) != 0) {
        return v.want("show() false and no chain").saw("show() true or a chain with LEDs");
    }
    return atOnce(us, "show()", v);
}

static bool lightingOutOfRange(Verdict& v) {
    for (const AdapterCase& a : ADAPTERS) {
        if (!beginLighting(a.adapterA)) {
            return v.want("begin() true").saw("%u A: begin() false", a.adapterA);
        }
        if (lighting.profile().capMa != a.capMa ||
            lighting.leds(BENCH_CHAIN) != Lighting::MAX_CHAIN_LEDS) {
            return v.want("cap %lu mA, %u LEDs", a.capMa, Lighting::MAX_CHAIN_LEDS)
                .saw("%u A: cap %lu mA, %u LEDs", a.adapterA, lighting.profile().capMa,
                     lighting.leds(BENCH_CHAIN));
        }
    }
    return true;
}

// show() returns while the frame of the longest chain still goes out. A second show() right after
// it returns false at once, and one after the frame and its latch returns true.
static bool lightingAtOnce(Verdict& v) {
    if (!lightingBegun && !beginLighting(Lighting::ADAPTER_MIN_A)) {
        return v.want("begin() true").saw("begin() false");
    }
    const uint16_t longest = lighting.leds(BENCH_CHAIN);
    fill_solid(lighting.chain(BENCH_CHAIN), longest, CRGB::Black);
    const uint32_t frameUs = longest * BITS_PER_LED * BIT_NS / NS_PER_US;
    delay(SETTLE_MS);
    bool first = false;
    bool second = true;
    const uint32_t firstUs = timedUs([&] { first = lighting.show(); });
    const uint32_t secondUs = timedUs([&] { second = lighting.show(); });
    Serial.printf("show(): %lu us, the next one %lu us, a frame of %u LEDs %lu us\n", firstUs,
                  secondUs, longest, frameUs);
    if (!first || second) {
        return v.want("true, then false").saw("show() %d, the next one %d", first, second);
    }
    if (firstUs >= frameUs) {
        return v.want("less than the frame's %lu us", frameUs).saw("show() took %lu us", firstUs);
    }
    if (!atOnce(secondUs, "the second show()", v)) {
        return false;
    }
    delay(SETTLE_MS);
    return lighting.show() || v.want("true").saw("show() false after the frame");
}

// ---- the servo driver

static ServoDriver servoFresh;     // before-begin: never begun
static ServoDriver servoStarved;   // begin-fails: begins with the driver tick full
static ServoDriver servoMoving;    // at-once

// out-of-range: settings past a range, each beside a twin at the nearer end
struct ServoPair {
    ServoDriver::Settings past;
    ServoDriver::Settings nearer;
};
static const ServoPair SERVO_PAIRS[] = {
    {{ServoDriver::START_DEG, TOO_FAR_DEG, TOO_FAST_DEG_S},
     {ServoDriver::START_DEG, ServoDriver::ANGLE_LIMIT_DEG, ServoDriver::SPEED_MAX_DEG_S}},
    {{ServoDriver::START_DEG, ServoDriver::ANGLE_LIMIT_DEG, TOO_SLOW_DEG_S},
     {ServoDriver::START_DEG, ServoDriver::ANGLE_LIMIT_DEG, ServoDriver::SPEED_MIN_DEG_S}},
};
constexpr size_t SERVO_PAIR_COUNT = sizeof SERVO_PAIRS / sizeof SERVO_PAIRS[0];
static ServoDriver servoPast[SERVO_PAIR_COUNT];
static ServoDriver servoNearer[SERVO_PAIR_COUNT];

// Gets whether a servo driver answers every call at once with empty data: no move, the start
// position, no travel and no failed part.
static bool servoEmpty(ServoDriver& servo, Verdict& v) {
    const uint32_t us = timedUs([&] { servo.moveTo(ServoDriver::Position::END); });
    if (servo.position() != ServoDriver::Position::START || servo.travelMs() != 0 ||
        servo.failed().parts != 0) {
        return v.want("empty data").saw("a position, a travel or a failed part");
    }
    return atOnce(us, "moveTo()", v);
}

static bool servoBeforeBegin(Verdict& v) {
    return servoEmpty(servoFresh, v);
}

static bool servoOutOfRange(Verdict& v) {
    for (size_t i = 0; i < SERVO_PAIR_COUNT; ++i) {
        const ServoPair& p = SERVO_PAIRS[i];
        servoPast[i].configure(p.past);
        servoNearer[i].configure(p.nearer);
        if (!servoPast[i].begin() || !servoNearer[i].begin()) {
            return v.want("begin() true").saw("begin() false");
        }
        if (servoPast[i].travelMs() != servoNearer[i].travelMs()) {
            return v.want("%lu ms, as at the nearer end", servoNearer[i].travelMs())
                .saw("%u to %u deg at %lu deg/s: %lu ms", p.past.startDeg, p.past.endDeg,
                     p.past.speedDegS, servoPast[i].travelMs());
        }
    }
    return true;
}

// moveTo() returns at once and leaves the move to the driver tick. Both ends lie at the start
// position, so a servo on the pin does not move.
static bool servoAtOnce(Verdict& v) {
    servoMoving.configure(
        {ServoDriver::START_DEG, ServoDriver::START_DEG, ServoDriver::SPEED_DEG_S});
    if (!servoMoving.begin()) {
        return v.want("begin() true").saw("begin() false");
    }
    return atOnce(timedUs([] { servoMoving.moveTo(ServoDriver::Position::END); }), "moveTo()", v);
}

// With every slot of the driver tick taken, begin() returns false and every later call answers at
// once with nothing.
static bool servoBeginFails(Verdict& v) {
    if (servoStarved.begin()) {
        return v.want("false").saw("begin() true with the driver tick full");
    }
    return servoEmpty(servoStarved, v);
}

// ---- the storage driver

constexpr size_t STORAGE_BYTES = 65536;   // a read long enough to run past its start
constexpr char STORAGE_FILE[] = "/tests/every-driver.bin";
EXTMEM static uint8_t storageBuffer[STORAGE_BYTES] __attribute__((aligned(32)));

// Before begin() a request fails at once. No part is reported failed. No stack is in use.
static bool storageBeforeBegin(Verdict& v) {
    FileRequest r;
    uint8_t into[16];
    const uint32_t us = timedUs([&] { storage.open(STORAGE_FILE).read(0, into, sizeof into, r); });
    if (r.status() != FileStatus::FAILED || storage.failed().parts != 0 ||
        storage.stackLeftBytes() != 0) {
        return v.want("FAILED at once, no failed part, no stack")
            .saw("status %u, parts %u, stack %lu", unsigned(r.status()),
                 unsigned(storage.failed().parts), storage.stackLeftBytes());
    }
    return atOnce(us, "read()", v);
}

// A long read returns while the card still moves its bytes.
static bool storageAtOnce(Verdict& v) {
    if (!storage.begin()) {
        return v.want("a card in the socket").saw("begin() false");
    }
    FileRequest r;
    storage.openWriter(STORAGE_FILE).rewrite(storageBuffer, STORAGE_BYTES, r);
    if (!r.wait()) {
        return v.want("the file written").saw("the write failed");
    }
    const uint32_t us =
        timedUs([&] { storage.open(STORAGE_FILE).read(0, storageBuffer, STORAGE_BYTES, r); });
    const bool pending = r.status() == FileStatus::PENDING;
    r.wait();
    return (pending || v.want("PENDING right after read()").saw("a read that had ended")) &&
           atOnce(us, "read()", v);
}

// ---- the display driver

// a column one step wide down the whole screen, two bytes of RGB565 a pixel
constexpr uint32_t DISPLAY_BAND_BYTES = uint32_t(Display::STEP_PX) * Display::HEIGHT * 2;
constexpr uint8_t DARK_GRAY = 0x10;         // both bytes of a dark RGB565 gray
constexpr uint32_t DISPLAY_DONE_MS = 100;   // far longer than a whole screen's bus time
static Display displayFresh;                // before-begin: never begun
static Display displayStarved;              // begin-fails: begins with the driver tick full
using test::display;                        // at-once: the shared display driver
DMAMEM static uint8_t displayBand[DISPLAY_BAND_BYTES];   // display.md: pixels in RAM1 or RAM2
static volatile bool displayOut = false;

// Gets whether a display driver answers every call at once with false or nothing.
static bool displayEmpty(Display& driver, Verdict& v) {
    bool sent = true;
    const uint32_t us =
        timedUs([&] { sent = driver.send(0, 0, Display::STEP_PX, 1, displayBand, [] {}); });
    driver.restart();
    if (sent || driver.restarting()) {
        return v.want("send() false, not restarting")
            .saw("send() %d, restarting %d", sent, driver.restarting());
    }
    return atOnce(us, "send()", v);
}

static bool displayBeforeBegin(Verdict& v) {
    return displayEmpty(displayFresh, v);
}

// A column down the whole screen returns while its bytes still go out, and a second rectangle
// right after it returns false at once.
static bool displayAtOnce(Verdict& v) {
    if (!display.begin()) {
        return v.want("begin() true").saw("begin() false");
    }
    memset(displayBand, DARK_GRAY, sizeof displayBand);
    displayOut = false;
    bool first = false;
    bool second = true;
    const uint32_t firstUs = timedUs([&] {
        first = display.send(0, 0, Display::STEP_PX, Display::HEIGHT, displayBand,
                             [] { displayOut = true; });
    });
    const bool outAtReturn = displayOut;
    const uint32_t secondUs =
        timedUs([&] { second = display.send(0, 0, Display::STEP_PX, 1, displayBand, [] {}); });
    const uint32_t t0 = millis();
    while (!displayOut && millis() - t0 < DISPLAY_DONE_MS) {
    }
    if (!first || second || outAtReturn || !displayOut) {
        return v.want("true while it goes out, then false, then its end")
            .saw("send() %d, the next %d, out at return %d, out later %d", first, second,
                 outAtReturn, displayOut);
    }
    return atOnce(firstUs, "send()", v) && atOnce(secondUs, "the second send()", v);
}

static bool displayBeginFails(Verdict& v) {
    if (displayStarved.begin()) {
        return v.want("false").saw("begin() true with the driver tick full");
    }
    return displayEmpty(displayStarved, v);
}

// ---- the touch driver

static EventQueue touchQueue;
static Touch touchFresh(io::wire2Bus, touchQueue, driverTick);     // before-begin: never begun
static Touch touchStarved(io::wire2Bus, touchQueue, driverTick);   // begin-fails

// Gets whether a touch driver answers every call at once with empty data.
static bool touchEmpty(Touch& touch, Verdict& v) {
    Finger f{};
    const uint32_t us = timedUs([&] { f = touch.finger(); });
    if (touch.present() || f.down || f.x != 0 || f.y != 0 || touch.failed().parts != 0) {
        return v.want("no module, no finger, no failed part")
            .saw("present %d, finger %d at %u, %u, failed parts %u", touch.present(), f.down, f.x,
                 f.y, unsigned(touch.failed().parts));
    }
    return atOnce(us, "finger()", v);
}

static bool touchBeforeBegin(Verdict& v) {
    return touchEmpty(touchFresh, v);
}

// out-of-range: readings above the range, beside a twin configured at the top of the range
static Touch touchPast(io::wire2Bus, touchQueue, driverTick);
static Touch touchNearer(io::wire2Bus, touchQueue, driverTick);

static bool touchOutOfRange(Verdict& v) {
    constexpr uint16_t ABOVE = Touch::RAW_MAX + 1;
    touchPast.configure({ABOVE, Touch::RAW_RIGHT, Touch::RAW_TOP, ABOVE});
    touchNearer.configure({Touch::RAW_MAX, Touch::RAW_RIGHT, Touch::RAW_TOP, Touch::RAW_MAX});
    const Touch::Settings past = touchPast.settings();
    const Touch::Settings nearer = touchNearer.settings();
    if (past.rawLeft != nearer.rawLeft || past.rawRight != nearer.rawRight ||
        past.rawTop != nearer.rawTop || past.rawBottom != nearer.rawBottom) {
        return v
            .want("%u, %u, %u, %u, as at the top of the range", nearer.rawLeft, nearer.rawRight,
                  nearer.rawTop, nearer.rawBottom)
            .saw("%u, %u, %u, %u", past.rawLeft, past.rawRight, past.rawTop, past.rawBottom);
    }
    return true;
}

static bool touchBeginFails(Verdict& v) {
    if (touchStarved.begin()) {
        return v.want("false").saw("begin() true with the driver tick full");
    }
    return touchEmpty(touchStarved, v);
}

// ---- the audio driver

static EventQueue audioQueue;
static AudioDriver audioFresh(audioQueue);   // before-begin: never begun
// at-once: the shared audio driver, which holds the I²S2 port
using test::audio;
static AudioDriver audioSecond(audioQueue);   // begin-fails: begins after the shared driver

// Gets whether an audio driver answers every call at once with empty data: no number, no sound
// and an empty position.
static bool audioEmpty(AudioDriver& driver, Verdict& v) {
    uint32_t number = 1;
    const uint32_t us = timedUs([&] { number = driver.play(0, storage.open(STORAGE_FILE)); });
    driver.stop(0);
    if (number != 0 || driver.playing(0) || !driver.position(0).empty()) {
        return v.want("number 0, not playing, an empty position")
            .saw("number %lu, playing %d, empty %d", number, driver.playing(0),
                 driver.position(0).empty());
    }
    return atOnce(us, "play()", v);
}

static bool audioBeforeBegin(Verdict& v) {
    return audioEmpty(audioFresh, v);
}

// play() returns while the card still reads the file, with the sound requested. The file holds
// no sound, so the speaker stays silent.
static bool audioAtOnce(Verdict& v) {
    if (!storage.begin()) {
        return v.want("a card in the socket").saw("storage begin() false");
    }
    if (!audio.begin()) {
        return v.want("begin() true").saw("begin() false");
    }
    uint32_t number = 0;
    const uint32_t us = timedUs([&] { number = audio.play(0, storage.open(STORAGE_FILE)); });
    const bool requested = audio.playing(0);
    audio.stop(0);
    if (number == 0 || !requested) {
        return v.want("a number and the sound requested")
            .saw("number %lu, playing %d", number, requested);
    }
    return atOnce(us, "play()", v);
}

// A second audio driver finds the I²S2 port held, and answers every call at once with nothing.
static bool audioBeginFails(Verdict& v) {
    if (!audio.begin()) {
        return v.want("the first driver's begin() true").saw("begin() false");
    }
    if (audioSecond.begin()) {
        return v.want("false").saw("begin() true with the I2S2 port held");
    }
    return audioEmpty(audioSecond, v);
}

// ---- the break beam driver

static BreakBeamDriver beamFresh(events);    // before-begin: never begun
using test::breakBeam;                       // begin-fails: begins first and holds the pin
static BreakBeamDriver beamSecond(events);   // begin-fails: begins after the shared driver

// Gets whether a break beam driver answers every call at once with empty data.
static bool beamEmpty(BreakBeamDriver& beam, Verdict& v) {
    bool blocked = true;
    const uint32_t us = timedUs([&] { blocked = beam.blocked(); });
    if (blocked || beam.failed().parts != 0) {
        return v.want("not blocked, no failed part")
            .saw("blocked %d, failed parts %u", blocked, unsigned(beam.failed().parts));
    }
    return atOnce(us, "blocked()", v);
}

static bool beamBeforeBegin(Verdict& v) {
    return beamEmpty(beamFresh, v);
}

static bool beamBeginFails(Verdict& v) {
    if (!breakBeam.begin()) {
        return v.want("true").saw("the first begin() false");
    }
    if (beamSecond.begin()) {
        return v.want("false").saw("begin() true with the pin held");
    }
    return beamEmpty(beamSecond, v);
}

// ---- the register and the rows

static const DriverCase DRIVERS[] = {
    // begin() fails only when the heap has no room for the colors, which no setting reaches
    {"lighting", lightingBeforeBegin, lightingOutOfRange, lightingAtOnce, nullptr},
    {"servo", servoBeforeBegin, servoOutOfRange, servoAtOnce, servoBeginFails},
    // the storage driver takes no settings and no slot of the driver tick
    {"storage", storageBeforeBegin, nullptr, storageAtOnce, nullptr},
    // the display takes no settings
    {"display", displayBeforeBegin, nullptr, displayAtOnce, displayBeginFails},
    // the touch driver starts no long transfer
    {"touch", touchBeforeBegin, touchOutOfRange, nullptr, touchBeginFails},
    // the audio program checks the volume's range through its tap. begin() needs no slot of the
    // driver tick and fails when another audio driver holds the I²S2 port
    {"audio", audioBeforeBegin, nullptr, audioAtOnce, audioBeginFails},
    // the break beam driver takes no settings and starts no transfer. begin() needs no slot of the
    // driver tick and fails when another break beam driver holds the pin
    {"break-beam", beamBeforeBegin, nullptr, nullptr, beamBeginFails},
};

// Runs a row's case of every driver and reports the first failure, or PASS.
template <bool (*DriverCase::*CASE)(Verdict&)> static void row() {
    for (const DriverCase& d : DRIVERS) {
        if (d.*CASE == nullptr) {
            Serial.printf("%s: no case for this row\n", d.name);
            continue;
        }
        Verdict v;
        if (!(d.*CASE)(v)) {
            char seen[TEXT + 16];
            snprintf(seen, sizeof seen, "%s: %s", d.name, v.seen);
            test::fail(seen, v.wanted);
            return;
        }
    }
    test::pass();
}

static void testBeginFails() {
    uint8_t taken = 0;
    while (driverTick.attach([] {}, DriverTick::PERIOD_US)) {
        ++taken;
    }
    Serial.printf("the driver tick had %u free slots\n", taken);
    row<&DriverCase::beginFails>();
}

static const Test TESTS[] = {
    {"every-driver.before-begin", RunsOn::TEENSY, LIMIT_S, LIMIT_S, row<&DriverCase::beforeBegin>,
     nullptr},
    {"every-driver.out-of-range", RunsOn::TEENSY, LIMIT_S, LIMIT_S, row<&DriverCase::outOfRange>,
     nullptr},
    {"every-driver.at-once", RunsOn::TEENSY, LIMIT_S, LIMIT_S, row<&DriverCase::atOnce>, nullptr},
    {"every-driver.begin-fails", RunsOn::TEENSY, LIMIT_S, LIMIT_S, testBeginFails, nullptr},
};

extern const Program EVERY_DRIVER = {TESTS, sizeof TESTS / sizeof TESTS[0]};
