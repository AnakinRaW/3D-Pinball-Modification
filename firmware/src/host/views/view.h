// The base of the host's views. A view builds its page or its overlay into a parent the host
// gives it, shows the data the host hands it, and tells the host through one function which of
// its controls the player pressed. The view keeps no state of the machine: the host fills it, and
// the Arcade fills it with made-up data.
//
// Design: docs/firmware/host-screens.md.
#pragma once
#include <lvgl.h>
#include <stddef.h>
#include <stdint.h>

/// A page or an overlay of the host's screens.
class View {
public:
    /// Runs when the player presses a control of a view or moves one of its sliders.
    ///
    /// @param action   What the control asks for, one of the view's Action values.
    /// @param value    What goes with it, such as a slider's value, a key's letter or a card's
    ///                 index, 0 where nothing goes with it.
    /// @param context  What the view's build() took.
    using Pressed = void (*)(uint8_t action, int32_t value, void* context);

    View() = default;
    View(const View&) = delete;
    View& operator=(const View&) = delete;

    /// Deletes the view's objects. The view builds again with its build().
    void close();

    /// Gets whether the view's objects stand.
    bool isOpen() const { return root_ != nullptr; }

    /// Gets the view's outermost object, nullptr while the view is closed.
    lv_obj_t* root() const { return root_; }

    static constexpr size_t SCORE_CHARS = 14;   // "4,294,967,295" and its end

    /// Writes a score with a comma between each group of three digits, as "1,352,330".
    static void score(uint32_t value, char (&out)[SCORE_CHARS]);

protected:
    ~View() = default;

    /// Starts building the view: closes what stood and creates its root over the whole parent.
    ///
    /// @param page  True for a page, whose root takes every touch beside its controls. False for
    ///              an overlay, which lets them through to what lies beneath.
    /// @return      The root, which the view builds into.
    lv_obj_t* create(lv_obj_t* parent, bool page, Pressed pressed, void* context);

    /// Makes a press on @p control report @p action with @p value.
    ///
    /// @param control  A control the view built. Its user data holds the action and the value.
    void bind(lv_obj_t* control, uint8_t action, int32_t value = 0);

    /// Makes every change of @p track report @p action with its value.
    ///
    /// @param track  A slider's track, as controls::levelTrack() gets it.
    void bindValue(lv_obj_t* track, uint8_t action);

    /// Reports @p action with @p value to the host.
    void report(uint8_t action, int32_t value) const;

private:
    static constexpr uint8_t TRACKS_MAX = 2;   // the sliders a view reports at most

    static void clicked(lv_event_t* e);
    static void changed(lv_event_t* e);
    static void deleted(lv_event_t* e);

    lv_obj_t* root_ = nullptr;
    Pressed pressed_ = nullptr;
    void* context_ = nullptr;
    lv_obj_t* tracks_[TRACKS_MAX] = {};
    uint8_t trackActions_[TRACKS_MAX] = {};
};
