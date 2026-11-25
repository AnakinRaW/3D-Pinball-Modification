// Draws the host's menu that a swipe from the right edge pulls in over a running game, and the hint
// at the edge that shows where the menu comes from. The view only draws: the host tells it where
// the finger is and what the rows say, and hears through a function which item the player chose.
//
// Design: docs/firmware/host-screens.md, In a game.
#pragma once
#include <lvgl.h>
#include <stdint.h>

/// The menu over a running game: a drawer of Sound, Lights, Machine and End game, with the gold
/// tab and the drawer's edge as the hint at the right edge.
class EdgeMenuView {
public:
    static constexpr int32_t SCREEN_WIDTH_PX = 320;   // the display's pixels across
    static constexpr int32_t SCREEN_HEIGHT_PX = 240;
    static constexpr int32_t WIDTH_PX = 208;         // host-screens.md: the menu's drawer
    static constexpr int32_t PEEK_PX = 2;            // the drawer's gold rim the hint shows
    static constexpr uint32_t SNAP_MS = 150;         // host-screens.md: t_menu_snap
    static constexpr uint32_t HINT_IN_MS = 150;      // host-screens.md: t_hint_in
    static constexpr uint32_t HINT_STAY_MS = 1000;   // host-screens.md: t_hint_stay
    static constexpr uint32_t HINT_FADE_MS = 300;    // host-screens.md: t_hint_fade

    /// What the player can choose in the menu.
    enum class Item : uint8_t { SOUND, LIGHTS, MACHINE, END_GAME, CLOSE };

    /// What the rows show under their names, which the host sets from the machine's state.
    struct Status {
        const char* sound = "";     // such as the volumes
        const char* lights = "";    // such as the brightnesses
        const char* machine = "";   // such as the count of failed devices
        bool fault = false;         // a device has failed, so the Machine row's bulb lights red
    };

    /// Runs when the player chooses an item.
    ///
    /// @param item     The item.
    /// @param context  What build() took.
    using Chosen = void (*)(Item item, void* context);

    EdgeMenuView() = default;
    EdgeMenuView(const EdgeMenuView&) = delete;
    EdgeMenuView& operator=(const EdgeMenuView&) = delete;

    /// Creates the hint and the closed drawer inside @p parent, both out of sight.
    ///
    /// A second call does nothing.
    ///
    /// @param parent   A full-screen object, such as the host's layer.
    /// @param chosen   Runs when the player chooses an item. nullptr chooses nothing.
    /// @param context  Handed to @p chosen.
    /// @note           The main loop calls it, after the screen's begin().
    void build(lv_obj_t* parent, Chosen chosen, void* context);

    /// Sets what the rows show under their names.
    ///
    /// The view copies the texts.
    void show(const Status& status);

    /// Fades the gold tab and the drawer's edge in, for a finger that came down at the right edge.
    ///
    /// Does nothing while the menu is open.
    void hint();

    /// Lets the hint stand for HINT_STAY_MS and then fade in HINT_FADE_MS, for a finger at the
    /// right edge that lifted or rested.
    void releaseHint();

    /// Moves the drawer's left edge to the finger, for a swipe on its way. The hint goes at once.
    ///
    /// @param x  The finger's x. The drawer stays between closed and open.
    void follow(uint16_t x);

    /// Snaps the drawer open or back in SNAP_MS once the swipe's finger lets go.
    ///
    /// @param x  Where the finger left the screen. Past half the drawer's width opens it.
    /// @return   Whether the menu is open.
    bool letGo(uint16_t x);

    /// Snaps an open drawer back in SNAP_MS, for a tap on the game or 20 s without a touch.
    void close();

    /// Gets whether the menu is open or snapping open.
    bool isOpen() const { return open; }

    /// Gets the drawer's left edge, SCREEN_WIDTH_PX while it is out of sight.
    int32_t menuLeft() const;

    /// Gets how opaque the hint is, LV_OPA_TRANSP while it is out of sight.
    lv_opa_t hintOpacity() const;

private:
    /// Creates a row that opens a page: an icon, a name, its status and a chevron.
    lv_obj_t* row(const char* symbol, uint32_t color, const char* name, Item item);

    /// Moves the drawer's left edge to @p x, at once or in SNAP_MS.
    void moveTo(int32_t x, bool snap);

    /// Fades the tab and the drawer to @p to in @p ms, after @p delayMs.
    ///
    /// @param step  The function that sets each step's opacity, one for fading in and one for
    ///              fading out, so each replaces only a fade of its kind.
    void fade(lv_anim_exec_xcb_t step, lv_opa_t to, uint32_t delayMs, uint32_t ms);

    /// Stops every fade of the tab and the drawer where it stands.
    void stopFades();

    static void clicked(lv_event_t* e);
    static void faded(lv_anim_t* a);

    lv_obj_t* drawer = nullptr;
    lv_obj_t* tab = nullptr;
    lv_obj_t* soundText = nullptr;
    lv_obj_t* lightsText = nullptr;
    lv_obj_t* machineText = nullptr;
    lv_obj_t* machineBulb = nullptr;
    Chosen chosen = nullptr;
    void* context = nullptr;
    bool open = false;
};
