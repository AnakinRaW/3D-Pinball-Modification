// The firmware's start: setup() starts the clock, creates the main task and starts FreeRTOS. The
// main task runs the start order of docs/firmware/general-design.md and then the main loop.

// Keeps Arduino's macros, which FastLED's Teensy 4 code and WDT_T4 use.
#define USE_ARDUINO_DEFINES
#include "arduino_freertos.h"
#include "core/clock.h"
#include "components/file-system.h"
#include "components/sounds.h"
#include "components/screen.h"
#include "components/video-player.h"
#include "core/device-monitor.h"
#include "core/event-queue.h"
#include "game/component.h"
#include "drivers/audio.h"
#include "drivers/display.h"
#include "drivers/storage.h"
#include "drivers/touch.h"
#include "io/i2c-bus.h"
#include "logging/debug-log.h"
#include "logging/logger.h"
// FastLED's fl/gfx/colorutils.h turns warnings off twice and back on once, so without
// the push and pop -Wunused-parameter and -Wreturn-type stay off for all code after it.
#pragma GCC diagnostic push
#include <FastLED.h>
#pragma GCC diagnostic pop

constexpr uint32_t BLINK_MS = 500;   // half the LED's blink period
constexpr uint32_t US_PER_MS = 1000;
/// The most driver events one pass of the main loop hands on, an estimate. A debug build reports a
/// pass that finds more waiting.
constexpr uint32_t EVENTS_PER_PASS = 64;
/// The boot film's layer. No game runs while the film plays, so no mode holds its key.
constexpr Layer BOOT_LAYER = {1, 0};
constexpr char BOOT_FILM[] = "intro";   // media.md: video/intro.rgb with audio/intro.wav

/// PJRC's guard behind the static variables is 32 bytes, and an MPU region of 32 bytes has to start
/// on a multiple of its size
constexpr uintptr_t GUARD_BYTES = 32;
/// ARMv7-M Architecture Reference Manual, B3.5: region 15 is the last of the 16 and wins over every
/// region PJRC's start-up sets, and SIZE 4 gives 2^(4+1) = 32 bytes
constexpr uint32_t GUARD_REGION = 15;
constexpr uint32_t GUARD_SIZE = 4;

/// general-design.md, Interrupts: the pin interrupts' row. tempmon.c of the Teensy core sets the
/// temperature panic to 0, which the start of FreeRTOS resets to 128 with every other interrupt.
constexpr uint8_t PIN_PRIORITY = 96;
constexpr uint8_t TEMPERATURE_PANIC_PRIORITY = 0;

/// PJRC's linker script: the end of the static variables in RAM1, and the top of the stack
extern "C" uint8_t _ebss[];
extern "C" uint8_t _estack[];

static StaticTask_t mainTask;

constexpr char LOG_FIRST[] = "/log-a.txt";    // logging.md: the log's first file
constexpr char LOG_SECOND[] = "/log-b.txt";   // logging.md: the log's second file
EXTMEM static uint8_t logBuffer[Logger::BUFFER_BYTES];

static AudioDriver audio(events);
static FileSystem files(storage);
static Sounds sounds(audio, files);
static Display display;
static Touch touch(io::wire2Bus, events, driverTick);
static Screen screen(display);
static VideoPlayer videoPlayer(screen, files, sounds);

/// The components a driver event is offered to, in this order. The first that takes it ends the
/// offer, so a new component is one more entry here.
static Component* const components[] = {&screen, &sounds};

/// True until the main loop runs; the watchdog's feed from FreeRTOS's idle task will read it
static volatile bool startingUp = true;

/// The port's default starts a 1 ms timer and a task for PJRC's MillisTimer and EventResponder,
/// which the firmware does not use.
namespace freertos {
void setup_event_responder() {}
}   // namespace freertos

/// The game host's handling of a device fault, where the host's dispatch() stands in the main loop.
class GameHost {
public:
    /// Takes a fault event of the device monitor, so no component sees it.
    ///
    /// The touch controller shares the display module's power. Once it answers again, the display
    /// has lost its setup, and the screen sets it up again.
    ///
    /// @param e  A driver event of any type.
    /// @return   Whether @p e was DEVICE_FAILED or DEVICE_RECOVERED.
    /// @note     The main loop calls it.
    static bool takeFault(const DriverEvent& e) {
        if (e.type == DriverEventType::DEVICE_FAILED) {
            return true;
        }
        if (e.type != DriverEventType::DEVICE_RECOVERED) {
            return false;
        }
        if (faultDevice(e.source) == Device::TOUCH) {
            logInfo("main", "the display module answers again, so the display restarts");
            screen.restart();
        }
        return true;
    }

    /// Plays the machine's boot film once, on the whole screen and with its sound.
    ///
    /// The film and its sound lie at the card's root. A card without the film ends it at once.
    ///
    /// @note  The main task calls it once at the start, after the begin() of the screen, the
    ///        video player and the sounds.
    static void playBootFilm() {
        files.useRoot();
        lv_obj_t* slide = screen.slide(BOOT_LAYER);
        if (slide == nullptr) {
            logWarning("main", "the screen has no slide for the boot film");
            return;
        }
        lv_obj_t* viewport = lv_obj_create(slide);
        lv_obj_remove_style_all(viewport);
        lv_obj_set_size(viewport, Display::WIDTH, Display::HEIGHT);
        if (!videoPlayer.play(viewport, BOOT_FILM, false, true)) {
            logWarning("main", "the video player refuses the boot film");
            screen.remove(BOOT_LAYER);
        }
    }

    /// Takes the end of the boot film and removes its slide, so the screen is black again.
    ///
    /// @param made  The events of the video player's update().
    /// @note        The main loop calls it.
    static void takeEnded(const PinballEvents& made) {
        for (uint8_t i = 0; i < made.count; ++i) {
            const PinballEvent& e = made.events[i];
            if (e.type == EventType::MOVIE_ENDED && e.source == BOOT_LAYER.key) {
                logInfo("main", "the boot film has ended");
                screen.remove(BOOT_LAYER);
            }
        }
    }
};

/// Blocks the 32 bytes at @p at, as PJRC guards the end of the static variables.
///
/// An overflowing stack then crashes at once instead of overwriting what lies below it.
///
/// @param at  The first of the 32 bytes.
static void guardStack(uintptr_t at) {
    SCB_MPU_RBAR = at | SCB_MPU_RBAR_REGION(GUARD_REGION) | SCB_MPU_RBAR_VALID;
    SCB_MPU_RASR = SCB_MPU_RASR_TEX(0) | SCB_MPU_RASR_AP(0) | SCB_MPU_RASR_XN |
                   SCB_MPU_RASR_SIZE(GUARD_SIZE) | SCB_MPU_RASR_ENABLE;
    asm volatile("dsb");
    asm volatile("isb");
}

/// Reads the start of a log file for the logger's begin().
///
/// @param path  The log file's full path.
/// @param head  Takes the file's first bytes.
/// @note        It waits for the card, so the main task calls it at start-up only.
static LogFileStart readLogStart(const char* path, uint8_t (&head)[Logger::HEAD_BYTES]) {
    FileRequest r;
    storage.open(path).read(0, head, sizeof head, r);
    r.wait();
    return {head, r.bytes(), r.fileSize()};
}

/// The main task: sets the machine up, then runs the main loop, which never waits.
static void runMainTask(void*) {
    NVIC_SET_PRIORITY(IRQ_TEMPERATURE_PANIC, TEMPERATURE_PANIC_PRIORITY);
    NVIC_SET_PRIORITY(IRQ_GPIO6789, PIN_PRIORITY);

    // Without a card the log does not begin. Every line is then lost.
    const bool card = storage.begin();
    if (!deviceMonitor.watch(Device::SD_CARD, storage)) {
        logWarning("main", "the device monitor does not watch the card");
    }
    static CardFile first(storage.openWriter(LOG_FIRST));
    static CardFile second(storage.openWriter(LOG_SECOND));
    static Logger logger(first, second, logBuffer, sizeof logBuffer);
    if (card) {
        static uint8_t firstHead[Logger::HEAD_BYTES];
        static uint8_t secondHead[Logger::HEAD_BYTES];
        logger.begin(readLogStart(LOG_FIRST, firstHead), readLogStart(LOG_SECOND, secondHead));
    }

    if (!audio.begin()) {
        logWarning("main", "the audio driver does not begin, so no sound plays");
    }

    if (!display.begin()) {
        logWarning("main", "the display does not begin, so the screen stays dark");
    }
    // After the display, so the black screen does not wait for the touch controller.
    if (!touch.begin()) {
        logWarning("main", "the touch driver does not begin, so the screen takes no touch");
    }
    if (!deviceMonitor.watch(Device::TOUCH, touch)) {
        logWarning("main", "the device monitor does not watch the touch controller");
    }

    screen.begin();
    videoPlayer.begin();
    pinMode(LED_BUILTIN, OUTPUT);

    // The sounds start at their starting volumes where the game host's begin() stands in the
    // start order.
    sounds.begin();
    // The boot film plays where the host launches the first game.
    GameHost::playBootFilm();

    startingUp = false;
    uint32_t toggledUs = nowUs();
    for (;;) {
        if (elapsedMs(toggledUs, BLINK_MS)) {
            toggledUs += BLINK_MS * US_PER_MS;
            digitalToggleFast(LED_BUILTIN);
        }
        // One event past the cap shows that more waited. It stays first for the next pass, so the
        // order holds.
        static DriverEvent pending[EVENTS_PER_PASS + 1];
        static uint32_t carried = 0;
        const uint32_t n = carried + events.read(pending + carried, EVENTS_PER_PASS + 1 - carried);
        const uint32_t taken = n > EVENTS_PER_PASS ? EVENTS_PER_PASS : n;
        if (n > EVENTS_PER_PASS) {
            DEBUG_LOG("main: more than %lu driver events waited for one pass\n", EVENTS_PER_PASS);
        }
        for (uint32_t i = 0; i < taken; ++i) {
            if (GameHost::takeFault(pending[i])) {
                continue;
            }
            PinballEvents made;
            for (Component* component : components) {
                if (component->translate(pending[i], made)) {
                    break;
                }
            }
        }
        carried = n - taken;
        if (carried != 0) {
            pending[0] = pending[EVENTS_PER_PASS];
        }
        PinballEvents ended;
        if (videoPlayer.update(nowUs(), ended)) {
            GameHost::takeEnded(ended);
        }
        logger.update();
        screen.render();
        sendDebugLog();
        yield();
    }
}

/// The main task's stack is the rest of RAM1, between PJRC's guard behind the static variables and
/// a guard below the interrupts' stack of configMAIN_STACK_DEPTH bytes at the top of RAM1.
void setup() {
    startClock();   // before anything takes a time
    const uintptr_t bottom = reinterpret_cast<uintptr_t>(_ebss) + GUARD_BYTES;
    const uintptr_t guard =
        reinterpret_cast<uintptr_t>(_estack) - configMAIN_STACK_DEPTH - GUARD_BYTES;
    guardStack(guard);
    xTaskCreateStatic(runMainTask, "main", (guard - bottom) / sizeof(StackType_t), nullptr, 1,
                      reinterpret_cast<StackType_t*>(bottom), &mainTask);
    vTaskStartScheduler();   // sets every interrupt to priority 128 and never returns
}

/// Never runs, since the main task carries the main loop.
void loop() {}
