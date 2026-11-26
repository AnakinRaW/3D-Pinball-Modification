// Draws the game choice, the host's screen while no game runs: a ring of cards, one per game with
// its cover, its name and its high score, and Play. The neighbors peek in at both sides. The ring
// follows a finger that drags it and snaps to the next card once let go, a tap on a neighbor turns
// it as well, and the last game leads back to the first.
//
// Design: docs/firmware/host-screens.md, Games and A game that cannot start.
#pragma once
#include "controls.h"
#include "view.h"

/// The game choice under the PINBALL header, with the gear for Settings and the status bulb while
/// a device is failed.
class GamesView : public View {
public:
    static constexpr uint32_t SLIDE_MS = 150;     // host-screens.md: t_page_slide
    static constexpr int32_t DRAG_SLOP_PX = 10;   // a finger moved this far drags the ring
    static constexpr int32_t SNAP_PX = 56;   // a drag this far turns the ring, else it goes back

    /// What the player can press.
    enum Action : uint8_t { PLAY, SETTINGS, STATUS, SHOWN };

    /// A game's card.
    struct Game {
        const char* name = "";   // in capitals
        uint32_t highScore = 0;
        const void* cover = nullptr;   // the cover picture for lv_image_set_src(), nullptr for none
        bool broken = false;           // the loader rejected the game's game.yml
        const char* errorAt = "";      // where, such as "game.yml line 42"
        const char* error = "";        // the first error
    };

    /// Builds the choice into @p parent with the card of @p shown in the middle.
    ///
    /// The view keeps @p games, which stay valid while the view is open.
    ///
    /// @param count    The games, at least 1. The built-in game is the first.
    /// @param fault    A device is failed, so the status bulb shows.
    /// @param pressed  Runs with PLAY and the game's index, SETTINGS for the gear, STATUS for the
    ///                 status bulb, and SHOWN with the index once another card stands in the
    ///                 middle.
    void build(lv_obj_t* parent, const Game* games, uint8_t count, uint8_t shown, bool fault,
               Pressed pressed, void* context);

    /// Turns the ring by one card in SLIDE_MS, to the next game or the one before.
    ///
    /// Does nothing while the ring turns or with a single game.
    void step(bool forward);

    /// Moves the ring by @p dx pixels from where it stands, as a finger dragging it does. The ring
    /// stays within one card either way.
    ///
    /// Does nothing while the ring turns or with a single game.
    void drag(int32_t dx);

    /// Lets the dragged ring go: it turns by one card once dragged SNAP_PX or more, and slides back
    /// otherwise.
    void letGo();

    /// Gets how far the ring stands from its rest, negative toward the next game.
    int32_t offset() const;

    /// Gets the index of the card in the middle.
    uint8_t shown() const { return index; }

private:
    /// Places the card of the game in the middle and two on each side of it.
    void place();

    /// Creates a game's card at @p x in the ring.
    lv_obj_t* card(const Game& g, int32_t x, bool middle);

    /// Slides the ring from where it stands to @p to in SLIDE_MS, turning it by @p turn cards.
    void slide(int32_t to, int8_t turn);

    static void touched(lv_event_t* e);
    static void tapped(lv_event_t* e);
    static void turned(lv_anim_t* a);

    const Game* games = nullptr;
    uint8_t count = 0;
    uint8_t index = 0;
    int8_t turning = 0;     // the step the ring is turning, 0 while it stands
    bool sliding = false;   // the ring slides to a card or back
    int32_t pressX = 0;     // where the finger came down on a card
    bool dragged = false;   // the finger on a card has dragged the ring, so its lift is no tap
    lv_obj_t* ring = nullptr;
    lv_obj_t* pages = nullptr;
    lv_obj_t* play = nullptr;
};
