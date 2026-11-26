// Draws the stage the host shows at a game's start and end: the launch card that the curtain opens
// on, the closed curtain between the host and a game, and Game over with the players' scores.
//
// Design: docs/firmware/host-screens.md, Launch card, Curtain and Game over.
#pragma once
#include "controls.h"
#include "view.h"

/// The launch card: the game's name in gold lettering and its high score between the open
/// curtains, with the swipe for the menu named below.
class LaunchCardView : public View {
public:
    /// Builds the card into @p parent. It has nothing to press.
    ///
    /// @param name       The game's name, in capitals.
    /// @param highScore  The game's high score.
    void build(lv_obj_t* parent, const char* name, uint32_t highScore);
};

/// The curtain between the host and a game, with a gold plaque naming where the machine goes.
class CurtainView : public View {
public:
    static constexpr uint32_t CLOSE_MS = 250;   // host-screens.md: t_curtain_close
    static constexpr uint32_t OPEN_MS = 300;    // host-screens.md: t_curtain_open

    /// Builds the curtain into @p parent, open and out of sight. It has nothing to press.
    void build(lv_obj_t* parent);

    /// Closes the curtain in CLOSE_MS. The plaque shows once it is closed.
    ///
    /// @param where  What the plaque names, such as the game's name, in capitals.
    /// @param at     True closes it at once, as a picture of the closed curtain.
    void close(const char* where, bool at = false);

    /// Opens the curtain in OPEN_MS. The plaque goes at once.
    void open();

    /// Gets whether the curtain is closed or closing.
    bool isClosed() const { return closed; }

private:
    void move(bool closing, uint32_t ms);

    lv_obj_t* left = nullptr;
    lv_obj_t* right = nullptr;
    lv_obj_t* plaque = nullptr;
    bool closed = false;
};

/// Game over: every player's score between curtains drawn to the screen's edges, each marked TOP 5
/// where it made the game's list. A tap anywhere goes on.
class GameOverView : public View {
public:
    static constexpr uint8_t PLAYERS_MAX = 4;   // host-screens.md: a game's players

    /// What the player can press.
    enum Action : uint8_t { GO_ON };

    /// A player's result.
    struct Player {
        uint32_t score = 0;
        uint8_t place = 0;   // the place in the game's list, 1 to 5, 0 where the score missed it
    };

    /// Builds the scores into @p parent.
    ///
    /// @param players  The players in their order.
    /// @param count    The players, 1 to PLAYERS_MAX.
    /// @param pressed  Runs with GO_ON on a tap anywhere.
    void build(lv_obj_t* parent, const Player* players, uint8_t count, Pressed pressed,
               void* context);
};

/// Gets a place's short name, "1ST" to "5TH", or "" for a place outside the list.
const char* placeName(uint8_t place);
