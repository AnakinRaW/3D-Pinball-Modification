// Draws a card that tells the player something and asks for one or two answers: the report of the
// faults at start, the restart after a fault, the missing memory card, a failed device, and the
// question before a game ends.
//
// Design: docs/firmware/host-screens.md, Start-up, End the game and Failed device.
#pragma once
#include "controls.h"
#include "view.h"

/// A card of up to LINES_MAX lines, each with a warning light, a line in bold and a line of detail,
/// and one or two buttons below.
class NoticeView : public View {
public:
    static constexpr uint8_t LINES_MAX = 3;     // the lines a card holds
    static constexpr int32_t WIDTH_PX = 292;    // host-screens.md: a card on the rays
    static constexpr int32_t DIALOG_PX = 280;   // host-screens.md: a card over a page

    /// What the player can press.
    enum Action : uint8_t { PRIMARY, SECONDARY };

    /// A line of the card.
    struct Line {
        controls::Light light = controls::Light::WORKS;
        bool bulb = true;          // false sets the texts at the card's left edge
        const char* title = "";    // in bold. A number in cyan is written "#78ffe6 12,340#"
        const char* detail = "";   // the line of detail below, "" for none
    };

    /// What the card shows.
    struct Notice {
        controls::Tone tone = controls::Tone::NOTE;
        const char* title = "";
        Line lines[LINES_MAX] = {};
        uint8_t count = 0;   // the lines in use
        const char* primary = "OK";
        controls::Plaque primaryPlaque = controls::Plaque::GOLD;
        // the violet button left of the primary, nullptr for none
        const char* secondary = nullptr;
        bool modal = false;   // true dims the page beneath, false stands the card on the rays
        int32_t width = WIDTH_PX;
    };

    /// Builds the card into @p parent.
    ///
    /// The view copies the texts.
    ///
    /// @param parent   A full-screen object, such as the host's layer.
    /// @param pressed  Runs with PRIMARY or SECONDARY when the player presses a button.
    /// @param context  Handed to @p pressed.
    void build(lv_obj_t* parent, const Notice& notice, Pressed pressed, void* context);
};
