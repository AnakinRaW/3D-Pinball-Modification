// The host's views with made-up data, one scene per screen of host-screens.md. Each scene's presses
// lead where the host's flow leads, so the Arcade clicks through the host: Back returns to the page
// a page was opened from, the curtain closes and opens between the host and a game, and every
// player is asked for a name in turn.
#include "view-gallery.h"
#include <initializer_list>
#include <stdio.h>
#include <string.h>
#include "host/views/game-views.h"
#include "host/views/games-view.h"
#include "host/views/machine-views.h"
#include "host/views/name-views.h"
#include "host/views/notice-view.h"
#include "host/views/settings-views.h"
#include "host/views/stage-views.h"

namespace {

using controls::Light;
using controls::Plaque;
using controls::Tone;

constexpr uint32_t COUNTDOWN_MS = 1000;        // a second of the high scores' countdown
constexpr uint32_t PROGRESS_STEP_MS = 50;      // a step of a made-up measurement
constexpr int32_t PROGRESS_STEP = 4;           // percent a step, so a measurement takes 1.25 s
constexpr uint32_t MARK_STEP_MS = 1500;        // a mark of the made-up touch calibration
constexpr uint32_t LAUNCH_MS = 1500;           // host-screens.md: t_launch_card
constexpr uint32_t CURTAIN_HOLD_MS = 200;      // host-screens.md: t_curtain_hold
constexpr uint8_t COUNTDOWN_S = 15;            // host-screens.md: t_play_again
constexpr size_t HISTORY_MAX = 16;             // the pages Back can return through
constexpr char BACK[] = "<back>";              // a step that returns to the page before
constexpr char NEXT_PLAYER[] = "<next>";       // a step to the next player's name
constexpr char TO_LAUNCH[] = "<launch>";       // a step through the curtain to the launch card
constexpr char TO_GAMES[] = "<games>";         // a step through the curtain to the games
constexpr char PLAY_PREVIEW[] = "<preview>";   // a step that plays the game's preview

GalleryNote noteFn = nullptr;
GalleryGo goFn = nullptr;
GalleryPreview previewFn = nullptr;
lv_timer_t* timer = nullptr;          // what the scene shown runs, such as a countdown
lv_timer_t* curtainTimer = nullptr;   // the curtain's steps, apart from what the scenes run
uint32_t ticks = 0;
const char* shown = nullptr;   // the scene that shows
const char* history[HISTORY_MAX] = {};
size_t depth = 0;

NoticeView notice;
NoticeView overNotice;   // a notice over a page built beneath it
LaunchCardView launch;
CurtainView curtain;
GameOverView gameOver;
GamesView games;
SettingsView settings;
LevelsView levels;
ServiceView service;
SensorsView sensors;
RecordSensorView recordSensor;
CalibrateTouchView calibrate;
WarningLightsView warningLights;
WarningsView warnings;
StatusView status;
ToastView toast;
ChooseNameView chooseName;
NameView nameEntry;
HighScoresView highScores;

/// What a press on a scene's control names and where it leads: one entry per action, in the
/// order of the view's Action.
struct Step {
    const char* name;
    const char* to;   // the scene it leads to, one of the steps above, or nullptr to stay
};

// -- the way through the scenes -------------------------------------------------------------------

/// Leads to a scene. A page opened this way returns to the scene shown with Back.
void open(const char* key) {
    if (shown != nullptr && depth < HISTORY_MAX) {
        history[depth++] = shown;
    }
    goFn(key);
}

/// Leads to a scene in place of the one shown, which Back then skips.
void replace(const char* key) {
    goFn(key);
}

void back() {
    goFn(depth > 0 ? history[--depth] : "games");
}

const char* curtainTo = "launch-card";   // where the curtain opens
const char* curtainName = "CLASSIC";     // what its plaque names

/// Closes the curtain over the scene shown and opens it on @p key. A game's start or end leaves
/// no way back.
void throughCurtain(const char* key, const char* name) {
    curtainTo = key;
    curtainName = name;
    depth = 0;
    replace("curtain");
}

void follow(const char* to);

/// Notes a press and follows its step. The context is the scene's table of steps.
/// Plays the preview of the game of @p index in the middle card of the game choice.
void playPreview(int32_t index);

void pressed(uint8_t action, int32_t value, void* context) {
    const Step* steps = static_cast<const Step*>(context);
    noteFn(steps[action].name, value);
    if (steps[action].to == PLAY_PREVIEW) {
        playPreview(value);
        return;
    }
    if (steps[action].to != nullptr) {
        follow(steps[action].to);
    }
}

void every(uint32_t ms, lv_timer_cb_t cb) {
    ticks = 0;
    timer = lv_timer_create(cb, ms, nullptr);
}

/// A scene of the table: its key, its name in the Arcade, what builds it and whether it stands over
/// a running game.
struct Entry {
    GalleryScene scene;
    void (*build)(lv_obj_t* parent);
    bool inGame;
};

const Entry* find(const char* key);

// -- start-up -------------------------------------------------------------------------------------

const Step REPORT_STEPS[] = {{"OK", "games"}, {"DETAILS", "warning-lights"}};
const Step OK_GAMES[] = {{"OK", "games"}};
const Step OK_LAUNCH[] = {{"OK", TO_LAUNCH}};

void startReport(lv_obj_t* parent) {
    NoticeView::Notice n;
    n.tone = Tone::FAULT;
    n.title = "2 FAULTS AT START";
    n.lines[0] = {Light::FAILED, true, "Drain beam", "Beam blocked: a ball may sit in it"};
    n.lines[1] = {Light::FAILED, true, "Seal sensor", "Magnet too far from the sensor"};
    n.lines[2] = {Light::WARNING, true, "3 warnings", "Config values held at their limits"};
    n.count = 3;
    n.secondary = "DETAILS";
    notice.build(parent, n, pressed, const_cast<Step*>(REPORT_STEPS));
}

void restarted(lv_obj_t* parent) {
    NoticeView::Notice n;
    n.title = "RESTARTED";
    n.lines[0] = {Light::WARNING, true, "The machine restarted itself after a fault.",
                  "Cause: a coil stayed on too long."};
    n.count = 1;
    notice.build(parent, n, pressed, const_cast<Step*>(OK_GAMES));
}

void noCard(lv_obj_t* parent) {
    NoticeView::Notice n;
    n.tone = Tone::WARNING;
    n.title = "NO MEMORY CARD";
    n.lines[0] = {Light::WARNING, true, "Only the built-in game can run.",
                  "Insert the card and restart."};
    n.count = 1;
    notice.build(parent, n, pressed, const_cast<Step*>(OK_LAUNCH));
}

void launchCard(lv_obj_t* parent) {
    launch.build(parent, "CLASSIC", 1352330);
    every(LAUNCH_MS, [](lv_timer_t*) {
        depth = 0;
        replace("game-runs");
    });
}

// -- the host
// --------------------------------------------------------------------------------------

const GamesView::Game GAMES[] = {
    {"CLASSIC", 1352330, nullptr, false, "", ""},
    {"CROC RIDE", 0, nullptr, true, "game.yml line 42", "Unknown lamp \"scop\""},
    {"SPACE RUN", 845200, nullptr, false, "", ""},
};
/// The games' directories under /games on the card, in the order of GAMES
const char* const GAME_IDS[] = {"classic", "croc-ride", "space-run"};
static_assert(sizeof(GAME_IDS) / sizeof(GAME_IDS[0]) == sizeof(GAMES) / sizeof(GAMES[0]),
              "every game has its directory");
const Step GAMES_STEPS[] = {{"PLAY", TO_LAUNCH},
                            {"SETTINGS", "settings"},
                            {"STATUS", "warning-lights"},
                            {"SHOWN", nullptr},
                            {"PREVIEW", PLAY_PREVIEW}};

void playPreview(int32_t index) {
    lv_obj_t* viewport = games.preview();
    if (previewFn != nullptr && viewport != nullptr && index >= 0 &&
        size_t(index) < sizeof(GAME_IDS) / sizeof(GAME_IDS[0])) {
        previewFn(viewport, GAME_IDS[index]);
    }
}

void gameChoice(lv_obj_t* parent, uint8_t first) {
    games.build(parent, GAMES, sizeof(GAMES) / sizeof(GAMES[0]), first, true, pressed,
                const_cast<Step*>(GAMES_STEPS));
}

// The curtain closes over the scene it was called from, holds with its plaque while the scene to
// come takes the place beneath it, and opens on that scene.
lv_obj_t* beneath = nullptr;

void openCurtain(lv_timer_t* t) {
    if (!curtain.isClosed()) {
        lv_timer_pause(t);
        replace(curtainTo);
        return;
    }
    lv_obj_clean(beneath);
    find(curtainTo)->build(beneath);
    curtain.open();
    lv_timer_set_period(t, CurtainView::OPEN_MS);
}

void curtainScene(lv_obj_t* parent) {
    beneath = lv_obj_create(parent);
    lv_obj_remove_style_all(beneath);
    lv_obj_set_size(beneath, lv_pct(100), lv_pct(100));
    lv_obj_set_clickable(beneath, false);
    const Entry* from = shown != nullptr ? find(shown) : nullptr;
    if (from != nullptr && from->build != curtainScene) {
        from->build(beneath);
        // What the scene left beneath runs stands still under the curtain.
        galleryLeave();
    }
    curtain.build(parent);
    curtain.close(curtainName);
    curtainTimer = lv_timer_create(openCurtain, CurtainView::CLOSE_MS + CURTAIN_HOLD_MS, nullptr);
}

const Step SETTINGS_STEPS[] = {{"BACK", BACK},         {"SOUND", "sound"},
                               {"LIGHTS", "lights"},   {"MACHINE", "warning-lights"},
                               {"SERVICE", "service"}, {"ANIMATED", nullptr}};

void settingsPressed(uint8_t action, int32_t value, void* context) {
    if (action == SettingsView::ANIMATED) {
        // The host keeps the choice in the saved settings, and every sunburst follows it.
        controls::setAnimated(value != 0);
    }
    pressed(action, value, context);
}

void settingsPage(lv_obj_t* parent) {
    SettingsView::State s;
    s.sound = "50 % \xC2\xB7 70 %";
    s.lights = "80 % \xC2\xB7 60 %";
    s.machine = "1 fault \xC2\xB7 3 warnings";
    s.fault = true;
    s.animated = controls::isAnimated();
    settings.build(parent, s, settingsPressed, const_cast<Step*>(SETTINGS_STEPS));
}

// -- in a game
// --------------------------------------------------------------------------------------

const Step STATUS_STEPS[] = {{"STATUS", "warning-lights"}};
const Step END_STEPS[] = {{"END GAME", TO_GAMES}, {"KEEP PLAYING", BACK}};
const Step TOAST_STEPS[] = {{"OPEN", "failed-device"}};

void gameRuns(lv_obj_t* parent) {
    status.build(parent, pressed, const_cast<Step*>(STATUS_STEPS));
    status.setFault(true);
    status.setWriting(true);
}

void endGame(lv_obj_t* parent) {
    NoticeView::Notice n;
    n.title = "END CLASSIC?";
    n.lines[0] = {Light::WORKS, false, "Its score of #78ffe6 12,340# is not saved.", ""};
    n.count = 1;
    n.primary = "END GAME";
    n.primaryPlaque = Plaque::RED;
    n.secondary = "KEEP PLAYING";
    n.modal = true;
    n.width = NoticeView::DIALOG_PX;
    notice.build(parent, n, pressed, const_cast<Step*>(END_STEPS));
}

void deviceFails(lv_obj_t* parent) {
    gameRuns(parent);
    toast.build(parent, pressed, const_cast<Step*>(TOAST_STEPS));
    toast.show(true, "BALL SENSOR FAILED", "Top lane 2");
}

void deviceWorks(lv_obj_t* parent) {
    gameRuns(parent);
    toast.build(parent, pressed, const_cast<Step*>(TOAST_STEPS));
    toast.show(false, "BALL SENSOR WORKS AGAIN", "Top lane 2");
}

// -- after a game
// -----------------------------------------------------------------------------------

// The results of the games that ended, each player with the place made in the game's list
const NameFor THREE_PLAYERS[] = {{1, 3, 980120}, {2, 2, 1288450}, {3, 0, 212300}};
const NameFor BEST_OF_TWO[] = {{1, 1, 1402760}, {2, 0, 655300}};
const NameFor ONE_IN_LIST[] = {{1, 4, 744600}};
const NameFor ONE_BEST[] = {{1, 1, 1402760}};
const NameFor ONE_MISSED[] = {{1, 0, 212300}};

const NameFor* ended = THREE_PLAYERS;   // the players of the game that ended
uint8_t endedCount = 3;
uint8_t asking = 0;   // the player the name pages ask

const Step OVER_STEPS[] = {{"GO ON", "choose-name"}};
const Step CHOOSE_STEPS[] = {{"SKIP", NEXT_PLAYER}, {"NAME", NEXT_PLAYER}, {"OTHER", "name"}};
const Step NAME_STEPS[] = {{"SKIP", NEXT_PLAYER}, {"KEY", nullptr}, {"OK", NEXT_PLAYER}};
const Step SCORES_STEPS[] = {{"GAMES", TO_GAMES}, {"PLAY AGAIN", TO_LAUNCH}};

/// Shows Game over for @p players, whom the name pages then ask in turn.
template <uint8_t N> void showGameOver(lv_obj_t* parent, const NameFor (&players)[N]) {
    static_assert(N <= GameOverView::PLAYERS_MAX, "a game has four players at most");
    ended = players;
    endedCount = N;
    asking = 0;
    GameOverView::Player results[N];
    for (uint8_t i = 0; i < N; ++i) {
        results[i] = {players[i].score, players[i].place};
    }
    gameOver.build(parent, results, N, pressed, const_cast<Step*>(OVER_STEPS));
}

void gameOverPage(lv_obj_t* parent) {
    showGameOver(parent, THREE_PLAYERS);
}

void gameOverBest(lv_obj_t* parent) {
    showGameOver(parent, BEST_OF_TWO);
}

void gameOverSingle(lv_obj_t* parent) {
    showGameOver(parent, ONE_IN_LIST);
}

void gameOverSingleBest(lv_obj_t* parent) {
    showGameOver(parent, ONE_BEST);
}

void gameOverSingleMissed(lv_obj_t* parent) {
    showGameOver(parent, ONE_MISSED);
}

void chooseNamePage(lv_obj_t* parent) {
    static const char* const NAMES[] = {"LEA", "ANNA", "MAXI", "JULE", "TOM", "PAPA"};
    chooseName.build(parent, ended[asking % endedCount], NAMES, 6, true, pressed,
                     const_cast<Step*>(CHOOSE_STEPS));
}

void namePage(lv_obj_t* parent) {
    nameEntry.build(parent, ended[asking % endedCount], pressed, const_cast<Step*>(NAME_STEPS));
}

void countDown(lv_timer_t*) {
    ++ticks;
    if (ticks >= COUNTDOWN_S) {
        throughCurtain("launch-card", "CLASSIC");
        return;
    }
    highScores.setCountdown(uint8_t(COUNTDOWN_S - ticks));
}

void highScoresPage(lv_obj_t* parent) {
    const HighScoresView::Entry entries[] = {{"ANNA", 1352330, false},
                                             {"MAXI", 1288450, true},
                                             {"LEA", 980120, true},
                                             {"JULE", 744600, false},
                                             {"TOM", 512090, false}};
    highScores.build(parent, "CLASSIC", entries, 5, nullptr, COUNTDOWN_S, pressed,
                     const_cast<Step*>(SCORES_STEPS));
    every(COUNTDOWN_MS, countDown);
}

// -- sound and lights
// ------------------------------------------------------------------------------

const Step SOUND_STEPS[] = {{"BACK", BACK}, {"MUSIC", nullptr}, {"EFFECTS", nullptr}};
const Step LIGHTS_STEPS[] = {{"BACK", BACK}, {"PLAYFIELD", nullptr}, {"AMBIENT", nullptr}};

void soundPage(lv_obj_t* parent) {
    LevelsView::Levels l;
    l.title = "SOUND";
    l.fill = controls::Fill::SOUND;
    l.names[0] = "MUSIC";
    l.names[1] = "EFFECTS";
    l.values[0] = 50;
    l.values[1] = 70;
    levels.build(parent, l, pressed, const_cast<Step*>(SOUND_STEPS));
}

void lightsPage(lv_obj_t* parent) {
    LevelsView::Levels l;
    l.title = "LIGHTS";
    l.fill = controls::Fill::LIGHT;
    l.names[0] = "PLAYFIELD";
    l.names[1] = "AMBIENT";
    l.values[0] = 80;
    l.values[1] = 60;
    levels.build(parent, l, pressed, const_cast<Step*>(LIGHTS_STEPS));
}

// -- machine and service
// ---------------------------------------------------------------------------

const WarningLightsView::Tile TILES[] = {
    {"Seal sensor", Light::WORKS},     {"Drain beam", Light::WORKS},
    {"Bumpers", Light::WORKS},         {"Ball sensors", Light::FAILED},
    {"Targets & bells", Light::WORKS}, {"Separator", Light::WORKS},
    {"Lights", Light::WORKS},          {"Speaker", Light::WORKS},
    {"Memory card", Light::WORKS},     {"Toggle", Light::WORKS},
    {"Screen", Light::WORKS},          {"3 warnings", Light::WARNING},
};
constexpr int32_t FAILED_TILE = 3;
constexpr int32_t WARNINGS_TILE = 11;
const Step TILES_STEPS[] = {{"BACK", BACK}, {"TILE", nullptr}};
const Step FAILED_STEPS[] = {{"OK", BACK}};
const Step WARNINGS_STEPS[] = {{"BACK", BACK}};
const Step SERVICE_STEPS[] = {{"BACK", BACK},
                              {"RECORD", "ball-sensors"},
                              {"CALIBRATE", "calibrate-touch"},
                              {"ADAPTER", nullptr}};
uint8_t adapterA = ServiceView::SMALL_ADAPTER_A;   // what the host would save in the config

void servicePressed(uint8_t action, int32_t value, void* context) {
    if (action == ServiceView::ADAPTER) {
        adapterA = uint8_t(value);
    }
    pressed(action, value, context);
}
const Step SENSORS_STEPS[] = {{"BACK", BACK}, {"SENSOR", nullptr}};
const Step RECORD_STEPS[] = {{"CANCEL", BACK}, {"START", nullptr}, {"DONE", BACK}};
const Step CALIBRATE_STEPS[] = {{"CANCEL", BACK}};

void tilePressed(uint8_t action, int32_t value, void* context) {
    pressed(action, value, context);
    if (action == WarningLightsView::TILE && value == FAILED_TILE) {
        open("failed-device");
    } else if (action == WarningLightsView::TILE && value == WARNINGS_TILE) {
        open("warnings");
    }
}

void warningLightsPage(lv_obj_t* parent) {
    warningLights.build(parent, TILES, sizeof(TILES) / sizeof(TILES[0]), tilePressed,
                        const_cast<Step*>(TILES_STEPS));
}

void failedDevice(lv_obj_t* parent) {
    warningLightsPage(parent);
    NoticeView::Notice n;
    n.tone = Tone::FAULT;
    n.title = "BALL SENSORS";
    n.lines[0] = {Light::FAILED, true, "Top lane 2 failed",
                  "Its sensor reads as covered. The game goes on without this lane."};
    n.lines[1] = {Light::WORKS, true, "15 sensors work", ""};
    n.count = 2;
    n.modal = true;
    n.width = NoticeView::DIALOG_PX;
    overNotice.build(parent, n, pressed, const_cast<Step*>(FAILED_STEPS));
}

void warningsPage(lv_obj_t* parent) {
    const WarningsView::Warning list[] = {
        {"Config", "audio.music_volume is 1.4, held at 1", 2},
        {"Sounds", "Starting volumes too loud together, effects lowered to 0.87", 1},
        {"Lamps", "Group \"stars\" names lamp \"star-800\", which is missing", 1},
        {"Config", "ir.release_sigma is missing, the default holds", 1},
        {"Game", "Mode \"multiball\" plays the missing sound \"jackpot2\"", 3},
        {"Card", "Writing the log took 412 ms", 1},
        {"Lamps", "Group \"ramps\" is empty", 1},
        {"Config", "servo.speed_deg_s is 900, held at 600", 1},
    };
    warnings.build(parent, list, sizeof(list) / sizeof(list[0]), pressed,
                   const_cast<Step*>(WARNINGS_STEPS));
}

void servicePage(lv_obj_t* parent) {
    ServiceView::About a;
    a.firmware = "5835ccf";
    a.config = "/config.yml \xC2\xB7 saved";
    a.adapterA = adapterA;
    service.build(parent, a, servicePressed, const_cast<Step*>(SERVICE_STEPS));
}

SensorsView::Sensor SENSORS[] = {
    {"Top lane 1", SensorsView::Recorded::YES}, {"Top lane 2", SensorsView::Recorded::YES},
    {"Top lane 3", SensorsView::Recorded::YES}, {"Inlane 1", SensorsView::Recorded::YES},
    {"Inlane 2", SensorsView::Recorded::YES},   {"Inlane 3", SensorsView::Recorded::NO},
    {"Inlane 4", SensorsView::Recorded::YES},   {"Drain 1", SensorsView::Recorded::YES},
    {"Drain 2", SensorsView::Recorded::YES},    {"Scoop", SensorsView::Recorded::YES},
};
constexpr uint8_t SENSOR_COUNT = sizeof(SENSORS) / sizeof(SENSORS[0]);
uint8_t chosenSensor = 0;   // the sensor the recording page records

void sensorPressed(uint8_t action, int32_t value, void* context) {
    pressed(action, value, context);
    if (action == SensorsView::SENSOR) {
        chosenSensor = uint8_t(value);
        open("record-sensor");
    }
}

void sensorsPage(lv_obj_t* parent) {
    sensors.build(parent, SENSORS, SENSOR_COUNT, sensorPressed, const_cast<Step*>(SENSORS_STEPS));
}

RecordSensorView::Stage stage = RecordSensorView::Stage::CLEAR;
int32_t measured = 0;   // the made-up measurement's progress in percent
char doneNote[64] = {};

// A measurement runs to its end and leads to the next stage. The ball's measurement moves the next
// sensor in the read order, which then needs recording again.
void measure(lv_timer_t*) {
    using Stage = RecordSensorView::Stage;
    measured += PROGRESS_STEP;
    recordSensor.setProgress(measured);
    if (measured < controls::SLIDER_MAX) {
        return;
    }
    lv_timer_delete(timer);
    timer = nullptr;
    if (stage == Stage::MEASURING_CLEAR) {
        stage = Stage::BALL;
        recordSensor.setStage(stage);
        return;
    }
    stage = Stage::DONE;
    SENSORS[chosenSensor].recorded = SensorsView::Recorded::YES;
    const uint8_t moved = uint8_t((chosenSensor + 1) % SENSOR_COUNT);
    SENSORS[moved].recorded = SensorsView::Recorded::AGAIN;
    snprintf(doneNote, sizeof(doneNote), "%s moved in the read order. Record it again.",
             SENSORS[moved].name);
    recordSensor.setStage(stage, doneNote);
}

void recordPressed(uint8_t action, int32_t value, void* context) {
    using Stage = RecordSensorView::Stage;
    pressed(action, value, context);
    if (action == RecordSensorView::START && timer == nullptr) {
        stage = stage == Stage::CLEAR ? Stage::MEASURING_CLEAR : Stage::MEASURING_BALL;
        recordSensor.setStage(stage);
        measured = 0;
        every(PROGRESS_STEP_MS, measure);
    }
}

void recordPage(lv_obj_t* parent) {
    stage = RecordSensorView::Stage::CLEAR;
    recordSensor.build(parent, SENSORS[chosenSensor].name, recordPressed,
                       const_cast<Step*>(RECORD_STEPS));
}

void markStep(lv_timer_t*) {
    ++ticks;
    // Each mark is held green for a step, then the next mark shows.
    if (ticks % 2 == 1) {
        calibrate.setHeld(true);
    } else {
        calibrate.setMark(uint8_t((ticks / 2) % CalibrateTouchView::MARKS));
    }
}

void calibratePage(lv_obj_t* parent) {
    calibrate.build(parent, pressed, const_cast<Step*>(CALIBRATE_STEPS));
    every(MARK_STEP_MS, markStep);
}

const Entry SCENES[] = {
    {{"start-report", "Start report"}, startReport, false},
    {{"restarted", "Restarted"}, restarted, false},
    {{"no-card", "No memory card"}, noCard, false},
    {{"launch-card", "Launch card"}, launchCard, false},
    {{"games", "Games"}, [](lv_obj_t* p) { gameChoice(p, 0); }, false},
    {{"broken-game", "A game that cannot start"}, [](lv_obj_t* p) { gameChoice(p, 1); }, false},
    {{"curtain", "Curtain"}, curtainScene, false},
    {{"settings", "Settings"}, settingsPage, false},
    {{"game-runs", "Game runs"}, gameRuns, true},
    {{"end-game", "End the game"}, endGame, true},
    {{"device-fails", "A device fails"}, deviceFails, true},
    {{"device-works", "A device works again"}, deviceWorks, true},
    {{"game-over", "Game over"}, gameOverPage, false},
    {{"game-over-best", "Game over, new high score"}, gameOverBest, false},
    {{"game-over-single", "Game over, one player in the list"}, gameOverSingle, false},
    {{"game-over-single-best", "Game over, one player, new high score"}, gameOverSingleBest, false},
    {{"game-over-single-missed", "Game over, one player off the list"},
     gameOverSingleMissed,
     false},
    {{"choose-name", "Choose a name"}, chooseNamePage, false},
    {{"name", "Name"}, namePage, false},
    {{"high-scores", "High scores"}, highScoresPage, false},
    {{"sound", "Sound"}, soundPage, false},
    {{"lights", "Lights"}, lightsPage, false},
    {{"warning-lights", "Warning lights"}, warningLightsPage, false},
    {{"failed-device", "Failed device"}, failedDevice, false},
    {{"warnings", "Warnings"}, warningsPage, false},
    {{"service", "Service"}, servicePage, false},
    {{"ball-sensors", "Ball sensors"}, sensorsPage, false},
    {{"record-sensor", "Record a sensor"}, recordPage, false},
    {{"calibrate-touch", "Calibrate touch"}, calibratePage, false},
};
constexpr size_t SCENE_COUNT = sizeof(SCENES) / sizeof(SCENES[0]);
GalleryScene table[SCENE_COUNT];

const Entry* find(const char* key) {
    for (const Entry& e : SCENES) {
        if (strcmp(e.scene.key, key) == 0) {
            return &e;
        }
    }
    return nullptr;
}

void follow(const char* to) {
    if (strcmp(to, BACK) == 0) {
        back();
    } else if (strcmp(to, TO_LAUNCH) == 0) {
        throughCurtain("launch-card", "CLASSIC");
    } else if (strcmp(to, TO_GAMES) == 0) {
        throughCurtain("games", "GAMES");
    } else if (strcmp(to, NEXT_PLAYER) == 0) {
        // Every player is asked in turn, whether the one before entered a name or skipped.
        ++asking;
        if (asking < endedCount) {
            replace("choose-name");
        } else {
            replace("high-scores");
        }
    } else {
        open(to);
    }
}

}   // namespace

void galleryBegin(GalleryNote note, GalleryGo go, GalleryPreview preview) {
    previewFn = preview;
    noteFn = note;
    goFn = go;
}

const GalleryScene* galleryScenes(size_t& count) {
    for (size_t i = 0; i < SCENE_COUNT; ++i) {
        table[i] = SCENES[i].scene;
    }
    count = SCENE_COUNT;
    return table;
}

const char* galleryBuild(const char* key, lv_obj_t* parent, bool picked) {
    const Entry* e = find(key);
    if (e == nullptr) {
        return nullptr;
    }
    if (picked) {
        depth = 0;
        shown = nullptr;
    }
    e->build(parent);
    shown = e->scene.key;
    return shown;
}

bool galleryInGame(const char* key) {
    const Entry* e = find(key);
    return e != nullptr && e->inGame;
}

void galleryOpen(const char* key) {
    open(key);
}

void galleryLeave() {
    for (lv_timer_t** t : {&timer, &curtainTimer}) {
        if (*t != nullptr) {
            lv_timer_delete(*t);
            *t = nullptr;
        }
    }
}
