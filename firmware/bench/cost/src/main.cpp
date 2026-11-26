// The cost bench: what the lamps, the screen and the sounds cost the Teensy 4.1's processor, which
// the Arcade takes as its budget. It runs the firmware's lamps component and lighting driver, its
// screen component with the display driver and the host's views, and its audio driver, each in the
// main loop the way the firmware runs them, and times each call with the cycle counter.
//
//   bus      ten full black frames through Display::send(), from the call to the sent callback,
//            and the share of the processor the bus's refill interrupt takes meanwhile
//   lamps    one effect after another on 40 LEDs of chain A: the time of each update() that hands
//            the lighting driver a frame, which includes the driver's work on it
//   screen   the screen alone: idle, a full-screen game picture with a score changing four times a
//            second, the Settings and Sound pages with the sunburst turning and standing, and the
//            Sound page over the running game. LVGL's display events give each refresh's time, its
//            drawing and its wait for the bus
//   sound    the four channels of the audio driver each looping a silent file from the card: the
//            Audio library's share of the processor
//   all      the lamps' chase, the Sound page with the sunburst turning over the running game and
//            the four channels together
//
// Every phase lasts PHASE_MS. The loop counts a fixed spin in every pass, so the spins a phase
// loses against the idle phase are the share of the main loop the phase takes.
//
// Wiring: up to 40 WS2812B on chain A, which may also stay unplugged. The program lights them at
// a tenth of the ambient brightness in blue, red, green, white and the rainbow, as the effects
// below give, and leaves them dark at the end. The display may stay unplugged, since its driver
// only writes to the panel. The card sits in the socket: the program writes one second of
// silence to /games/bench-cost/audio/silence.wav and leaves it there. The amplifier may stay
// unplugged, since the files hold silence. It prints over USB serial and ends with "done".

#include <Arduino.h>
#include <AudioStream.h>
#include <FreeRTOS.h>
#include <task.h>
#include "components/lamps.h"
#include "components/screen.h"
#include "core/clock.h"
#include "core/event-queue.h"
#include "drivers/audio.h"
#include "drivers/display.h"
#include "drivers/lighting.h"
#include "drivers/storage.h"
#include "host/views/controls.h"
#include "host/views/settings-views.h"
#include "logging/debug-log.h"

constexpr uint32_t CPU_MHZ = 600;           // teensy-4.1 model: f_cpu
constexpr uint32_t PHASE_MS = 5000;         // each phase's measuring time
constexpr uint32_t EFFECT_MS = 3000;        // each lamp effect's measuring time
constexpr uint32_t SETTLE_MS = 500;         // the first refresh of a new scene, left out
constexpr uint32_t SCORE_MS = 250;          // four score changes a second, as a game's score
constexpr uint32_t SCORE_STEP = 1250;       // what a score change adds
constexpr uint32_t FIRST_SCORE = 1352330;   // the score the game picture starts with
constexpr uint32_t SPIN_LOOPS = 100;        // one spin, a fixed piece of the main loop's work
constexpr uint32_t BUS_FRAMES = 10;
constexpr uint32_t BUS_GAP_MS = 20;      // between two frames of the bus test
constexpr uint32_t REFERENCE_MS = 100;   // the idle spin rate the bus test compares with
constexpr uint32_t MAIN_STACK_BYTES = 32768;
constexpr uint32_t US_PER_MS = 1000;
constexpr uint32_t MS_PER_S = 1000;

// the screen: lcd-display model, n_cols × n_rows × b_pixel
constexpr uint16_t SCREEN_W = 320;
constexpr uint16_t SCREEN_H = 240;
constexpr uint32_t PIXEL_BYTES = 2;
constexpr uint32_t FRAME_BYTES = uint32_t(SCREEN_W) * SCREEN_H * PIXEL_BYTES;
constexpr uint32_t GAME_SCORE_RGB = 0x78ffe6;   // host-screens.md: cyan, the score's digits
constexpr int32_t GAME_SCORE_Y = 60;

// the lamps: 40 LEDs on chain A, as the left perimeter, at a tenth of the ambient brightness
constexpr uint16_t BENCH_LEDS = 40;
constexpr uint8_t CHAIN_A = 0;
constexpr float BRIGHTNESS = 0.1f;
constexpr Layer BENCH_LAYER = {1, 1};
constexpr uint16_t SPEED_MM_PER_S = 200;   // the chase's speed along the strip
constexpr uint16_t PERIOD_MS = 2000;       // a pulse's and an alternation's period
constexpr uint16_t ON_MS = 300;            // a blink's on and off time

// the sounds: one second of silence in 16-bit mono at the audio model's f_s, looped on every
// channel
constexpr uint32_t RATE_HZ = 44100;
constexpr uint32_t SILENCE_SAMPLES = RATE_HZ;
constexpr uint32_t WAV_HEADER_BYTES = 44;
constexpr uint32_t WAV_FMT_BYTES = 16;
constexpr uint16_t WAV_PCM = 1;
constexpr uint16_t WAV_BITS = 16;
constexpr uint16_t WAV_BLOCK_ALIGN = 2;
constexpr uint32_t SILENCE_BYTES = WAV_HEADER_BYTES + SILENCE_SAMPLES * WAV_BLOCK_ALIGN;
constexpr char SILENCE[] = "/games/bench-cost/audio/silence.wav";
constexpr uint32_t EVENTS_DRAINED = 16;

static Display display;
static Screen screen(display);
static Lighting lighting;
static Lamps lamps(lighting);
static AudioDriver audio(events);

DMAMEM static uint8_t black[FRAME_BYTES] __attribute__((aligned(32)));
EXTMEM static uint8_t gamePixels[FRAME_BYTES] __attribute__((aligned(32)));
EXTMEM static uint8_t silence[SILENCE_BYTES];
static lv_image_dsc_t gameImage;

/// The host's calls to the screen.
class GameHost {
public:
    static lv_obj_t* hostLayer(Screen& s) { return s.hostLayer(); }
};

static inline uint32_t cycles() {
    return ARM_DWT_CYCCNT;
}

static inline float microseconds(uint64_t c) {
    return float(c) / CPU_MHZ;
}

static inline void spin() {
    for (volatile uint32_t i = 0; i < SPIN_LOOPS; i = i + 1) {
    }
}

// Spins for @p ms and gets how many spins fit, the main loop's capacity without load.
static uint32_t spinsFor(uint32_t ms) {
    const uint32_t start = cycles();
    const uint32_t span = ms * US_PER_MS * CPU_MHZ;
    uint32_t n = 0;
    while (cycles() - start < span) {
        spin();
        ++n;
    }
    return n;
}

// -- LVGL's display events --------------------------------------------------------------------
struct ScreenStats {
    uint32_t refreshes = 0;
    uint32_t emptyRefreshes = 0;
    uint32_t rows = 0;
    uint32_t waits = 0;
    uint64_t refreshCycles = 0;
    uint64_t renderCycles = 0;
    uint32_t refreshMax = 0;
    uint32_t renderMax = 0;
    uint32_t waitMax = 0;
};
static ScreenStats screenStats;
static uint32_t refreshStart = 0;
static uint32_t renderStart = 0;
static uint32_t waitStart = 0;
static int32_t rowLow = 0;
static int32_t rowHigh = -1;

static void onDisplay(lv_event_t* e) {
    const uint32_t now = cycles();
    ScreenStats& s = screenStats;
    switch (lv_event_get_code(e)) {
    case LV_EVENT_REFR_START:
        refreshStart = now;
        rowLow = INT32_MAX;
        rowHigh = -1;
        break;
    case LV_EVENT_REFR_READY: {
        const uint32_t d = now - refreshStart;
        if (rowHigh < 0) {
            ++s.emptyRefreshes;
            break;
        }
        ++s.refreshes;
        s.refreshCycles += d;
        s.refreshMax = d > s.refreshMax ? d : s.refreshMax;
        s.rows += uint32_t(rowHigh - rowLow + 1);
        break;
    }
    case LV_EVENT_RENDER_START:
        renderStart = now;
        break;
    case LV_EVENT_RENDER_READY: {
        const uint32_t d = now - renderStart;
        s.renderCycles += d;
        s.renderMax = d > s.renderMax ? d : s.renderMax;
        break;
    }
    case LV_EVENT_FLUSH_START: {
        const auto* a = static_cast<const lv_area_t*>(lv_event_get_param(e));
        rowLow = a->y1 < rowLow ? a->y1 : rowLow;
        rowHigh = a->y2 > rowHigh ? a->y2 : rowHigh;
        break;
    }
    case LV_EVENT_FLUSH_WAIT_START:
        waitStart = now;
        break;
    case LV_EVENT_FLUSH_WAIT_FINISH: {
        const uint32_t d = now - waitStart;
        ++s.waits;
        s.waitMax = d > s.waitMax ? d : s.waitMax;
        break;
    }
    default:
        break;
    }
}

// -- the bus ----------------------------------------------------------------------------------
static volatile uint32_t sentAt = 0;
static volatile bool sent = false;

static void measureBus() {
    const float idleRate =
        float(spinsFor(REFERENCE_MS)) / float(REFERENCE_MS * US_PER_MS * CPU_MHZ);
    uint64_t total = 0;
    uint32_t shortest = UINT32_MAX;
    uint32_t longest = 0;
    uint64_t busySpins = 0;
    for (uint32_t i = 0; i < BUS_FRAMES; ++i) {
        sent = false;
        const uint32_t start = cycles();
        if (!display.send(0, 0, SCREEN_W, SCREEN_H, black, [] {
                sentAt = ARM_DWT_CYCCNT;
                sent = true;
            })) {
            Serial.printf("bus: send refused\n");
            return;
        }
        uint32_t n = 0;
        while (!sent) {
            spin();
            ++n;
        }
        const uint32_t d = sentAt - start;
        total += d;
        shortest = d < shortest ? d : shortest;
        longest = d > longest ? d : longest;
        busySpins += n;
        delay(BUS_GAP_MS);
    }
    const float busyRate = float(busySpins) / float(total);
    Serial.printf("bus  full frame %.0f us mean, %.0f to %.0f us over %lu frames, refill interrupt "
                  "%.1f %% of the processor while sending\n",
                  microseconds(total) / BUS_FRAMES, microseconds(shortest), microseconds(longest),
                  (unsigned long)BUS_FRAMES, 100.0f * (1.0f - busyRate / idleRate));
}

// -- the game picture and the host's pages ----------------------------------------------------
static lv_obj_t* gamePicture = nullptr;
static lv_obj_t* score = nullptr;
static uint32_t scoreValue = FIRST_SCORE;
static SettingsView settings;
static LevelsView levels;

static void ignore(uint8_t, int32_t, void*) {}

// A picture of smooth color ramps, which LVGL draws as it draws a game's full-screen picture.
static void makeGamePixels() {
    constexpr uint32_t RED_MAX = 31;
    constexpr uint32_t GREEN_MAX = 63;
    constexpr uint32_t BLUE_MAX = 31;
    constexpr uint32_t RED_SHIFT = 11;
    constexpr uint32_t GREEN_SHIFT = 5;
    constexpr uint32_t BYTE_BITS = 8;
    for (uint32_t y = 0; y < SCREEN_H; ++y) {
        for (uint32_t x = 0; x < SCREEN_W; ++x) {
            const uint32_t r = x * RED_MAX / (SCREEN_W - 1);
            const uint32_t g = y * GREEN_MAX / (SCREEN_H - 1);
            const uint32_t b = BLUE_MAX - (x + y) * BLUE_MAX / (SCREEN_W + SCREEN_H - 2);
            const uint16_t px = uint16_t(r << RED_SHIFT | g << GREEN_SHIFT | b);
            const uint32_t at = (y * SCREEN_W + x) * PIXEL_BYTES;
            gamePixels[at] = uint8_t(px >> BYTE_BITS);   // RGB565 high byte first
            gamePixels[at + 1] = uint8_t(px);
        }
    }
    arm_dcache_flush(gamePixels, sizeof gamePixels);
    gameImage = {};
    gameImage.header.magic = LV_IMAGE_HEADER_MAGIC;
    gameImage.header.cf = LV_COLOR_FORMAT_RGB565_SWAPPED;
    gameImage.header.w = SCREEN_W;
    gameImage.header.h = SCREEN_H;
    gameImage.header.stride = SCREEN_W * PIXEL_BYTES;
    gameImage.data_size = FRAME_BYTES;
    gameImage.data = gamePixels;
}

static void showGame(bool on) {
    if (on && gamePicture == nullptr) {
        gamePicture = lv_image_create(screen.background());
        lv_image_set_src(gamePicture, &gameImage);
        score = lv_label_create(screen.overlay());
        lv_obj_set_style_text_color(score, lv_color_hex(GAME_SCORE_RGB), 0);
        lv_obj_set_style_text_font(score, &jetbrains_mono_bold_20, 0);
        lv_obj_align(score, LV_ALIGN_TOP_MID, 0, GAME_SCORE_Y);
        lv_label_set_text_fmt(score, "%lu", (unsigned long)scoreValue);
    } else if (!on && gamePicture != nullptr) {
        lv_obj_delete(gamePicture);
        lv_obj_delete(score);
        gamePicture = nullptr;
        score = nullptr;
    }
}

enum class Page : uint8_t { NONE, SETTINGS, LEVELS };

static void showPage(Page p) {
    constexpr int32_t MUSIC = 80;
    constexpr int32_t EFFECTS = 70;
    settings.close();
    levels.close();
    lv_obj_t* host = GameHost::hostLayer(screen);
    if (p == Page::SETTINGS) {
        SettingsView::State st;
        st.sound = "MUSIC 80 %  EFFECTS 70 %";
        st.lights = "LAMPS 60 %  DISPLAY 90 %";
        st.status = "ALL DEVICES WORK";
        settings.build(host, st, ignore, nullptr);
    } else if (p == Page::LEVELS) {
        LevelsView::Levels l;
        l.title = "SOUND";
        l.names[0] = "MUSIC";
        l.names[1] = "EFFECTS";
        l.values[0] = MUSIC;
        l.values[1] = EFFECTS;
        levels.build(host, l, ignore, nullptr);
    }
}

// -- the lamps --------------------------------------------------------------------------------
static bool lampsReady = false;

static bool beginLamps() {
    Lamps::Settings groups;
    groups.of(Lamps::Group::PERIMETER_LEFT).chain = CHAIN_A;
    groups.of(Lamps::Group::PERIMETER_LEFT).leds = BENCH_LEDS;
    Lighting::Settings bench;
    if (!Lamps::chainLeds(groups, bench.leds)) {
        Serial.printf("lamps: chainLeds() false\n");
        return false;
    }
    lighting.configure(bench);
    lamps.configure(groups);
    if (!lighting.begin() || !lamps.begin()) {
        Serial.printf("lamps: begin() false\n");
        return false;
    }
    lamps.clear();
    lamps.brightness(BRIGHTNESS, BRIGHTNESS);
    return true;
}

enum class BenchEffect : uint8_t { STEADY, BLINK, PULSE, CHASE, FADE, RAINBOW, DARK };

static void startEffect(BenchEffect e) {
    lamps.remove(BENCH_LAYER);
    LampGroups g = lamps.groups(BENCH_LAYER);
    switch (e) {
    case BenchEffect::STEADY:
        g.perimeterLeft.set(Color::BLUE);
        break;
    case BenchEffect::BLINK:
        g.perimeterLeft.blink(Color::RED, ON_MS, ON_MS, Off::DARK);
        break;
    case BenchEffect::PULSE:
        g.perimeterLeft.pulse(Color::GREEN, Color::BLUE, PERIOD_MS);
        break;
    case BenchEffect::CHASE:
        g.perimeterLeft.run(Pattern::CHASE, Direction::UP, Color::BLUE, SPEED_MM_PER_S, Color::RED);
        break;
    case BenchEffect::FADE:
        g.perimeterLeft.fade(Color::WHITE, PERIOD_MS);
        break;
    case BenchEffect::RAINBOW:
        g.perimeterLeft.alternate(Paint(RainbowColors_p, By::POSITION), Color::NONE, ON_MS,
                                  PERIOD_MS);
        break;
    case BenchEffect::DARK:
        g.perimeterLeft.off();
        break;
    }
}

// -- the sounds -------------------------------------------------------------------------------
static bool soundReady = false;

static void put16(uint8_t* p, uint16_t v) {
    p[0] = uint8_t(v);
    p[1] = uint8_t(v >> 8);
}

static void put32(uint8_t* p, uint32_t v) {
    put16(p, uint16_t(v));
    put16(p + 2, uint16_t(v >> 16));
}

static bool beginSound() {
    if (!storage.begin()) {
        Serial.printf("sound: no card in the socket\n");
        return false;
    }
    if (!audio.begin()) {
        Serial.printf("sound: the audio driver's begin() false\n");
        return false;
    }
    memset(silence, 0, sizeof silence);
    memcpy(silence, "RIFF", 4);
    put32(silence + 4, SILENCE_BYTES - 8);
    memcpy(silence + 8, "WAVEfmt ", 8);
    put32(silence + 16, WAV_FMT_BYTES);
    put16(silence + 20, WAV_PCM);
    put16(silence + 22, 1);
    put32(silence + 24, RATE_HZ);
    put32(silence + 28, RATE_HZ * WAV_BLOCK_ALIGN);
    put16(silence + 32, WAV_BLOCK_ALIGN);
    put16(silence + 34, WAV_BITS);
    memcpy(silence + 36, "data", 4);
    put32(silence + 40, SILENCE_SAMPLES * WAV_BLOCK_ALIGN);
    FileRequest r;
    storage.openWriter(SILENCE).rewrite(silence, SILENCE_BYTES, r);
    if (!r.wait()) {
        Serial.printf("sound: writing %s failed\n", SILENCE);
        return false;
    }
    return true;
}

static void playSound(bool on) {
    for (uint8_t c = 0; c < AudioDriver::CHANNELS; ++c) {
        if (on) {
            audio.play(c, storage.open(SILENCE), true);
        } else {
            audio.stop(c);
        }
    }
}

// -- a phase ----------------------------------------------------------------------------------
struct Load {
    bool lamps = false;
    bool game = false;
    Page page = Page::NONE;
    bool turning = true;
    bool sound = false;
};

struct LoopStats {
    uint32_t spins = 0;
    uint32_t frames = 0;   // the lamps' update() calls that handed a frame
    uint64_t frameCycles = 0;
    uint32_t frameMax = 0;
    uint64_t renderCallCycles = 0;
    uint32_t renderCallMax = 0;
};

static uint32_t idleSpins = 0;   // the spins of the idle phase

// Runs the main loop with @p load for @p ms and gets what each call took.
static LoopStats runLoop(const Load& load, uint32_t ms) {
    LoopStats s;
    const uint32_t span = ms * US_PER_MS * CPU_MHZ;
    const uint32_t start = cycles();
    uint32_t nextScore = millis() + SCORE_MS;
    DriverEvent drop[EVENTS_DRAINED];
    while (cycles() - start < span) {
        if (load.lamps) {
            const uint32_t a = cycles();
            const bool handed = lamps.update(nowUs());
            const uint32_t d = cycles() - a;
            if (handed) {
                ++s.frames;
                s.frameCycles += d;
                s.frameMax = d > s.frameMax ? d : s.frameMax;
            }
        }
        const uint32_t a = cycles();
        screen.render();
        const uint32_t d = cycles() - a;
        s.renderCallCycles += d;
        s.renderCallMax = d > s.renderCallMax ? d : s.renderCallMax;
        if (load.game && int32_t(millis() - nextScore) >= 0) {
            nextScore += SCORE_MS;
            scoreValue += SCORE_STEP;
            lv_label_set_text_fmt(score, "%lu", (unsigned long)scoreValue);
        }
        while (events.read(drop, EVENTS_DRAINED) > 0) {
        }
        yield();
        spin();
        ++s.spins;
    }
    return s;
}

static void phase(const char* name, const Load& load) {
    controls::setAnimated(load.turning);
    showGame(load.game);
    showPage(load.page);
    if (load.lamps) {
        startEffect(BenchEffect::CHASE);
    }
    playSound(load.sound);
    runLoop(load, SETTLE_MS);
    screenStats = {};
    AudioProcessorUsageMaxReset();
    const LoopStats s = runLoop(load, PHASE_MS);
    if (idleSpins == 0) {
        idleSpins = s.spins;
    }
    const ScreenStats& v = screenStats;
    const float seconds = float(PHASE_MS) / MS_PER_S;
    Serial.printf(
        "%-16s refreshes %5.1f/s rows %5.1f refresh %6.0f us mean %6.0f max render "
        "%6.0f us mean %6.0f max bus-wait %lu x %5.0f us max render() %5.1f %% longest "
        "%6.0f us",
        name, v.refreshes / seconds, v.refreshes ? float(v.rows) / v.refreshes : 0.0f,
        v.refreshes ? microseconds(v.refreshCycles) / v.refreshes : 0.0f,
        microseconds(v.refreshMax), v.refreshes ? microseconds(v.renderCycles) / v.refreshes : 0.0f,
        microseconds(v.renderMax), (unsigned long)v.waits, microseconds(v.waitMax),
        100.0f * float(s.renderCallCycles) / float(uint64_t(PHASE_MS) * US_PER_MS * CPU_MHZ),
        microseconds(s.renderCallMax));
    if (load.lamps) {
        Serial.printf(" lamps %lu frames %5.0f us mean %5.0f max", (unsigned long)s.frames,
                      s.frames ? microseconds(s.frameCycles) / s.frames : 0.0f,
                      microseconds(s.frameMax));
    }
    if (load.sound) {
        Serial.printf(" audio %4.1f %% max", float(AudioProcessorUsageMax()));
    }
    Serial.printf(" loop lost %5.1f %%\n", 100.0f * (1.0f - float(s.spins) / float(idleSpins)));
    while (sendDebugLog() != 0) {
        delay(1);
    }
}

// Runs each lamp effect for EFFECT_MS and prints what a frame costs, also for each LED.
static void lampEffects() {
    static const char* const NAMES[] = {"steady", "blink", "pulse", "chase", "fade", "rainbow"};
    const BenchEffect effects[] = {BenchEffect::STEADY, BenchEffect::BLINK, BenchEffect::PULSE,
                                   BenchEffect::CHASE,  BenchEffect::FADE,  BenchEffect::RAINBOW};
    Load load;
    load.lamps = true;
    showGame(false);
    showPage(Page::NONE);
    for (size_t i = 0; i < sizeof effects / sizeof effects[0]; ++i) {
        startEffect(effects[i]);
        const LoopStats s = runLoop(load, EFFECT_MS);
        const float mean = s.frames ? microseconds(s.frameCycles) / s.frames : 0.0f;
        Serial.printf("lamps-%-10s %lu frames on %u LEDs, update %5.1f us mean %5.1f max, %5.0f ns "
                      "an LED\n",
                      NAMES[i], (unsigned long)s.frames, unsigned(BENCH_LEDS), mean,
                      microseconds(s.frameMax), mean * US_PER_MS / BENCH_LEDS);
    }
}

static void run() {
    Load idle;
    phase("idle", idle);
    if (lampsReady) {
        lampEffects();
    }
    Load game;
    game.game = true;
    phase("game", game);
    Load settingsOn;
    settingsOn.page = Page::SETTINGS;
    phase("settings-on", settingsOn);
    Load settingsOff = settingsOn;
    settingsOff.turning = false;
    phase("settings-off", settingsOff);
    Load levelsOn;
    levelsOn.page = Page::LEVELS;
    phase("levels-on", levelsOn);
    Load levelsOff = levelsOn;
    levelsOff.turning = false;
    phase("levels-off", levelsOff);
    Load levelsGame = levelsOn;
    levelsGame.game = true;
    phase("levels-game-on", levelsGame);
    Load levelsGameOff = levelsGame;
    levelsGameOff.turning = false;
    phase("levels-game-off", levelsGameOff);
    if (soundReady) {
        Load sound;
        sound.sound = true;
        phase("sound", sound);
    }
    Load all = levelsGame;
    all.lamps = lampsReady;
    all.sound = soundReady;
    phase("all", all);
    phase("idle-again", idle);
}

static void mainTask(void*) {
    if (!display.begin()) {
        Serial.printf("display: begin() false\n");
    } else {
        measureBus();
    }
    screen.begin();
    lv_display_add_event_cb(lv_display_get_default(), onDisplay, LV_EVENT_ALL, nullptr);
    makeGamePixels();
    lampsReady = beginLamps();
    soundReady = beginSound();
    run();
    playSound(false);
    if (lampsReady) {
        startEffect(BenchEffect::DARK);
        runLoop(Load{true}, SETTLE_MS);
    }
    Serial.printf("done\n");
    for (;;) {
        screen.render();
        yield();
    }
}

void setup() {
    startClock();
    Serial.begin(115200);
    while (!Serial) {
    }
    ARM_DEMCR |= ARM_DEMCR_TRCENA;
    ARM_DWT_CTRL |= ARM_DWT_CTRL_CYCCNTENA;
    Serial.printf("cost bench, F_CPU_ACTUAL %lu\n", (unsigned long)F_CPU_ACTUAL);
    if (xTaskCreate(mainTask, "main", MAIN_STACK_BYTES / sizeof(StackType_t), nullptr, 1,
                    nullptr) != pdPASS) {
        Serial.printf("no memory for the main task\n");
        return;
    }
    vTaskStartScheduler();
}

// never runs once FreeRTOS has started
void loop() {}
