// Draws what follows a game: the list of known names for a player, the keyboard for a new name,
// and the game's high scores with the countdown to the next game.
//
// Design: docs/firmware/host-screens.md, After a game.
#pragma once
#include "controls.h"
#include "view.h"

/// Whose name a page asks for: the player, the place the score made and the score.
struct NameFor {
    uint8_t player = 1;   // 1 to 4
    uint8_t place = 0;    // 1 to 5, 0 where the score missed the list
    uint32_t score = 0;
};

/// The known names for a player, the last one entered first and lit, with Other name and Skip.
class ChooseNameView : public View {
public:
    static constexpr uint8_t NAMES_MAX = 6;   // host-screens.md: the six names entered most often

    /// What the player can press. NAME comes with the name's index.
    enum Action : uint8_t { SKIP, NAME, OTHER };

    /// @param names  The names, the one this player entered last first.
    /// @param count  The names, up to NAMES_MAX.
    /// @param lit    True lights the first name as the one this player entered last.
    void build(lv_obj_t* parent, const NameFor& who, const char* const* names, uint8_t count,
               bool lit, Pressed pressed, void* context);
};

/// The keyboard for a new name of up to LETTERS_MAX letters from A to Z.
class NameView : public View {
public:
    static constexpr uint8_t LETTERS_MAX = 10;   // host-screens.md: a name's letters

    /// What the player can press. KEY comes with the letter, or 0 for delete, and OK with the
    /// name's length.
    enum Action : uint8_t { SKIP, KEY, OK };

    /// Builds the keyboard with an empty name.
    void build(lv_obj_t* parent, const NameFor& who, Pressed pressed, void* context);

    /// Gets the name typed so far.
    const char* name() const { return letters; }

private:
    static void typed(lv_event_t* e);
    void showName();

    char letters[LETTERS_MAX + 1] = {};
    lv_obj_t* field = nullptr;
    lv_obj_t* cursor = nullptr;
    lv_obj_t* length = nullptr;
};

/// The game's list of high scores, each new entry lit, with Games and Play again.
class HighScoresView : public View {
public:
    static constexpr uint8_t PLACES = 5;   // host-screens.md: a game's list

    /// What the player can press.
    enum Action : uint8_t { GAMES, PLAY_AGAIN };

    /// A place of the list.
    struct Entry {
        const char* name = "";   // "???" for a skipped name
        uint32_t score = 0;
        bool fresh = false;   // entered by this game, so it is lit
    };

    /// @param game     The game's name, in capitals.
    /// @param entries  The list's places, best first.
    /// @param count    The places in use, up to PLACES.
    /// @param others   A line of the players' scores for a game whose scores all missed the list,
    ///                 nullptr otherwise.
    /// @param seconds  The countdown's seconds to Play again.
    void build(lv_obj_t* parent, const char* game, const Entry* entries, uint8_t count,
               const char* others, uint8_t seconds, Pressed pressed, void* context);

    /// Shows the countdown's seconds on Play again.
    void setCountdown(uint8_t seconds);

private:
    lv_obj_t* again = nullptr;
};
