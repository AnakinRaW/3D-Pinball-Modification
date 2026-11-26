// Draws the pages behind the gear and the menu: Settings with its four tiles and the switch of the
// animated background, Sound and Lights with their levels, and Service with its steps for the
// operator, the ball sensors and their recording, and the touch calibration.
//
// Design: docs/firmware/host-screens.md, Settings, Sound and lights, and Machine and service.
#pragma once
#include "controls.h"
#include "view.h"

/// Settings: tiles for Sound, Lights, Machine and Service, each with its current state, and the
/// switch of the animated background below them.
class SettingsView : public View {
public:
    /// What the player can press. ANIMATED comes with 1 for on and 0 for off.
    enum Action : uint8_t { BACK, SOUND, LIGHTS, MACHINE, SERVICE, ANIMATED };

    /// What the tiles and the switch show.
    struct State {
        const char* sound = "";     // such as the volumes
        const char* lights = "";    // such as the brightnesses
        const char* machine = "";   // such as the count of faults and warnings
        bool fault = false;         // a device is failed, so the Machine tile glows red
        bool animated = true;       // the background turns
    };

    void build(lv_obj_t* parent, const State& state, Pressed pressed, void* context);
};

/// Sound or Lights: two levels under a header, each from 0 to 100 %.
class LevelsView : public View {
public:
    /// What the player can press or move. FIRST and SECOND come with their level's value.
    enum Action : uint8_t { BACK, FIRST, SECOND };

    /// What the page shows.
    struct Levels {
        const char* title = "";   // "SOUND" or "LIGHTS"
        controls::Fill fill = controls::Fill::SOUND;
        const char* names[2] = {"", ""};   // such as "MUSIC" and "EFFECTS"
        int32_t values[2] = {0, 0};        // 0 to controls::SLIDER_MAX
    };

    void build(lv_obj_t* parent, const Levels& levels, Pressed pressed, void* context);

    /// Gets a level's value.
    ///
    /// @param which  0 for the first level, 1 for the second.
    int32_t value(uint8_t which) const;

private:
    lv_obj_t* levels[2] = {};
};

/// Service: the operator's steps, the adapter the lights run from, and the firmware and the config
/// below them. A change of the adapter asks first in a dialog that warns of an adapter too weak.
class ServiceView : public View {
public:
    static constexpr uint8_t SMALL_ADAPTER_A = 3;   // lighting.md: the two adapter profiles
    static constexpr uint8_t LARGE_ADAPTER_A = 6;

    /// What the player can press. ADAPTER comes with the adapter's new rating in amperes, once
    /// the dialog confirmed it.
    enum Action : uint8_t { BACK, RECORD, CALIBRATE, ADAPTER };

    /// What the page names.
    struct About {
        const char* firmware = "";            // the firmware's build, such as its commit
        const char* config = "";              // the config file and its state
        uint8_t adapterA = SMALL_ADAPTER_A;   // the adapter the config names, in amperes
    };

    void build(lv_obj_t* parent, const About& about, Pressed pressed, void* context);

    /// Gets the adapter's rating the page shows, in amperes.
    uint8_t adapter() const { return adapterA; }

private:
    /// Opens the dialog that asks to change the adapter to the other rating.
    void ask();

    /// Shows the adapter's rating in its row.
    void showAdapter();

    static void answered(lv_event_t* e);

    uint8_t adapterA = SMALL_ADAPTER_A;
    lv_obj_t* adapterRow = nullptr;
    lv_obj_t* dialog = nullptr;
};

/// The ball sensors in a list that scrolls, each with whether it is recorded. A tap on a sensor
/// records it again.
class SensorsView : public View {
public:
    /// What the player can press. SENSOR comes with the sensor's index.
    enum Action : uint8_t { BACK, SENSOR };

    /// What a sensor's line says about its recording.
    enum class Recorded : uint8_t { YES, NO, AGAIN };

    /// A sensor's line.
    struct Sensor {
        const char* name = "";   // such as "Top lane 2"
        Recorded recorded = Recorded::YES;
    };

    void build(lv_obj_t* parent, const Sensor* sensors, uint8_t count, Pressed pressed,
               void* context);
};

/// The recording of one ball sensor in two stages: every sensor clear, then a ball on this one.
/// Each stage starts with Start, and the measurement then shows its progress.
class RecordSensorView : public View {
public:
    /// What the player can press.
    enum Action : uint8_t { CANCEL, START, DONE };

    /// Where the recording stands.
    enum class Stage : uint8_t { CLEAR, MEASURING_CLEAR, BALL, MEASURING_BALL, DONE };

    /// Builds the page at the first stage, every sensor clear.
    ///
    /// @param name  The sensor's name, such as "Top lane 2". The view copies it.
    void build(lv_obj_t* parent, const char* name, Pressed pressed, void* context);

    /// Shows a stage of the recording.
    ///
    /// @param note  At DONE, a line below the result, such as the sensors that need recording
    ///              again, "" for none.
    void setStage(Stage stage, const char* note = "");

    /// Shows how far a measurement has come, 0 to controls::SLIDER_MAX.
    void setProgress(int32_t percent);

private:
    char name[24] = {};
    lv_obj_t* text = nullptr;
    lv_obj_t* note = nullptr;
    lv_obj_t* bar = nullptr;
    lv_obj_t* start = nullptr;
    lv_obj_t* cancel = nullptr;
    lv_obj_t* finish = nullptr;
};

/// The touch calibration: a gold mark at one corner after the other, and Cancel.
class CalibrateTouchView : public View {
public:
    static constexpr uint8_t MARKS = 4;   // host-screens.md: one mark per corner

    /// What the player can press.
    enum Action : uint8_t { CANCEL };

    void build(lv_obj_t* parent, Pressed pressed, void* context);

    /// Moves the mark to the corner of @p mark: top left, top right, bottom right, bottom left.
    void setMark(uint8_t mark);

    /// Turns the mark green while the finger holds it long enough, gold otherwise.
    void setHeld(bool held);

private:
    lv_obj_t* cross = nullptr;
    lv_obj_t* pages = nullptr;
};
