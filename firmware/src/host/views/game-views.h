// Draws what the host shows over a running game: the status bulb while a device is failed, the
// card indicator while the memory card is written, and the toast when a device fails or works
// again.
//
// Design: docs/firmware/host-screens.md, In a game.
#pragma once
#include "controls.h"
#include "view.h"

/// The status bulb top right and the card indicator bottom left, over the game.
class StatusView : public View {
public:
    /// What the player can press.
    enum Action : uint8_t { STATUS };

    /// Builds the overlay into @p parent, with nothing showing.
    ///
    /// @param pressed  Runs with STATUS on a tap on the status bulb's 44 px area.
    void build(lv_obj_t* parent, Pressed pressed, void* context);

    /// Shows the status bulb while a device is failed.
    void setFault(bool fault);

    /// Shows the card indicator, its dots running, while the card is written.
    void setWriting(bool writing);

private:
    lv_obj_t* status = nullptr;
    lv_obj_t* card = nullptr;
};

/// A toast that drops in from the top, stays, and shrinks into the status bulb.
class ToastView : public View {
public:
    static constexpr uint32_t DROP_MS = 200;     // host-screens.md: t_toast_drop
    static constexpr uint32_t FAULT_MS = 4000;   // host-screens.md: t_toast_fault
    static constexpr uint32_t WORKS_MS = 3000;   // host-screens.md: t_toast_recovered

    /// What the player can press.
    enum Action : uint8_t { OPEN };

    /// Builds the overlay into @p parent, with no toast showing.
    ///
    /// @param pressed  Runs with OPEN on a tap on the toast of a failed device.
    void build(lv_obj_t* parent, Pressed pressed, void* context);

    /// Drops a toast in. It stays FAULT_MS for a failed device and WORKS_MS for one that works
    /// again, and then shrinks away in DROP_MS. A toast still showing gives way to it.
    ///
    /// The view copies the texts.
    ///
    /// @param failed  True for a failed device, red. False for one that works again, green.
    /// @param title   Such as "BALL SENSOR FAILED".
    /// @param detail  Such as "Top lane 2".
    /// @param at      True places it at once and keeps it, as a picture of the toast.
    void show(bool failed, const char* title, const char* detail, bool at = false);

    /// Gets whether a toast shows.
    bool isShowing() const;

private:
    lv_obj_t* toast = nullptr;
};
