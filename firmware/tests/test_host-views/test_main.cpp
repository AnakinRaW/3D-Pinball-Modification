// Tests on the computer the host's views: LVGL on a display that draws nowhere, with a pointer the
// test presses, and each view filled with data as the storyboard shows it.
//
//   score            a score groups its digits in threes
//   notice           a card shows its lines and reports its buttons, and a modal one dims the page
//   launch card      the launch card names the game and its high score
//   curtain          the curtain closes on its plaque and opens again
//   game over        Game over lists the players by score with the winner first, shows a new high
//                    score, gives a single player the score large, and goes on at a tap
//   games            the game choice reports Play, the gear and the status bulb, turns its ring of
//                    cards from the last game to the first, and keeps Play off for a broken game
//   settings         the four tiles report their pages, Status glows red for a fault, and the
//                    switch of the animated background reports
//   levels           the levels show and report their values, each from 0 to 100 %
//   service          the steps report their pages, the firmware and the config show, and a change
//                    of the lights' adapter asks first and warns before the larger one
//   sensors          the ball sensors stand in a list that scrolls, each with its recording
//   record sensor    the recording asks for every sensor clear and then for the ball, and ends
//                    with Done and a note
//   calibrate touch  the mark moves from corner to corner and Cancel reports
//   warning lights   a tile per device reports its index
//   warnings         the warnings show with their counts in a list that scrolls, with fades at
//                    the edges more lies beyond
//   status           the status bulb shows for a fault and reports a tap, and the card indicator
//                    shows while the card is written
//   toast            a toast drops in, reports a tap, and shrinks away after its time
//   choose name      the names report their index, Other name and Skip report
//   name             the keys type a name of up to ten letters, delete takes one off, OK reports
//                    the length
//   high scores      the list lights its new entries, the countdown shows on Play again, and the
//                    buttons report
//
// Run: pio test -e computer, which runs in CI as well.

#include <Arduino.h>
#include <string.h>
#include <unity.h>
#include "host/views/game-views.h"
#include "host/views/games-view.h"
#include "host/views/machine-views.h"
#include "host/views/name-views.h"
#include "host/views/notice-view.h"
#include "host/views/settings-views.h"
#include "host/views/stage-views.h"

constexpr int32_t W = 320;
constexpr int32_t H = 240;
constexpr uint32_t STEP_MS = 5;   // one pass of LVGL's timers
constexpr uint32_t BUFFER_ROWS = 10;
constexpr uint32_t REFRESH_MS =
    40;   // longer than LVGL's refresh period, which its animations step at

struct Pointer {
    bool down = false;
    int32_t x = 0;
    int32_t y = 0;
};
static Pointer pointer;
static lv_indev_t* indev = nullptr;
static uint8_t buffer[W * BUFFER_ROWS * 2];

// the presses a view reported, the last one first
struct Press {
    int32_t action = -1;
    int32_t value = 0;
};
static Press last;
static uint32_t presses = 0;

static void pressed(uint8_t action, int32_t value, void*) {
    last = {action, value};
    ++presses;
}

static uint32_t tick() {
    return fakeMillis;
}

static void flush(lv_display_t* d, const lv_area_t*, uint8_t*) {
    lv_display_flush_ready(d);
}

static void read(lv_indev_t*, lv_indev_data_t* data) {
    data->point.x = pointer.x;
    data->point.y = pointer.y;
    data->state = pointer.down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

// Runs LVGL's timers for ms and lays the objects out, so their coordinates hold.
static void run(uint32_t ms = STEP_MS) {
    for (uint32_t t = 0; t < ms; t += STEP_MS) {
        fakeMillis += STEP_MS;
        lv_timer_handler();
    }
    lv_obj_update_layout(lv_screen_active());
}

// Presses the pointer at a point and lets it go.
static void tapAt(int32_t x, int32_t y) {
    run();
    pointer = {true, x, y};
    lv_indev_read(indev);
    pointer.down = false;
    lv_indev_read(indev);
    run();
}

// Presses the pointer at the middle of an object and lets it go.
static void tap(lv_obj_t* o) {
    run();
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    tapAt((a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2);
}

// Finds the first label under o whose text holds text, nullptr for none.
static lv_obj_t* find(lv_obj_t* o, const char* text) {
    if (lv_obj_check_type(o, &lv_label_class) && strstr(lv_label_get_text(o), text) != nullptr) {
        return o;
    }
    const uint32_t n = lv_obj_get_child_count(o);
    for (uint32_t i = 0; i < n; ++i) {
        if (lv_obj_t* f = find(lv_obj_get_child(o, int32_t(i)), text)) {
            return f;
        }
    }
    return nullptr;
}

// Taps the label that holds text, which hands the touch to the control it stands on.
static void tapText(lv_obj_t* root, const char* text) {
    lv_obj_t* l = find(root, text);
    TEST_ASSERT_NOT_NULL_MESSAGE(l, text);
    tap(l);
}

static uint32_t hex(lv_color_t c) {
    return lv_color_to_u32(c) & 0xFFFFFF;
}

// A clean full-screen parent.
struct Rig {
    lv_obj_t* parent = nullptr;
    Rig() {
        parent = lv_obj_create(lv_screen_active());
        lv_obj_remove_style_all(parent);
        lv_obj_set_size(parent, W, H);
        lv_obj_set_clickable(parent, false);
        last = {};
        presses = 0;
    }
    ~Rig() { lv_obj_delete(parent); }
};

void setUp() {
    if (indev == nullptr) {
        lv_init();
        lv_tick_set_cb(tick);
        lv_display_t* d = lv_display_create(W, H);
        lv_display_set_buffers(d, buffer, nullptr, sizeof buffer, LV_DISPLAY_RENDER_MODE_PARTIAL);
        lv_display_set_flush_cb(d, flush);
        indev = lv_indev_create();
        lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(indev, read);
        lv_timer_set_period(lv_indev_get_read_timer(indev), UINT32_MAX);
    }
    pointer = {};
}

void tearDown() {}

// score: a score groups its digits in threes.
void testScore() {
    char out[View::SCORE_CHARS];
    const struct {
        uint32_t value;
        const char* text;
    } cases[] = {{0, "0"},
                 {999, "999"},
                 {1000, "1,000"},
                 {1352330, "1,352,330"},
                 {4294967295u, "4,294,967,295"}};
    for (const auto& c : cases) {
        View::score(c.value, out);
        TEST_ASSERT_EQUAL_STRING(c.text, out);
    }
}

// notice: a card shows its lines and reports its buttons, and a modal one dims the page.
void testNotice() {
    Rig rig;
    NoticeView v;
    NoticeView::Notice n;
    n.tone = controls::Tone::FAULT;
    n.title = "2 FAULTS AT START";
    n.lines[0] = {controls::Light::FAILED, true, "Drain beam", "Beam blocked"};
    n.lines[1] = {controls::Light::WARNING, true, "3 warnings", ""};
    n.count = 2;
    n.secondary = "DETAILS";
    v.build(rig.parent, n, pressed, nullptr);
    run();
    TEST_ASSERT_TRUE(v.isOpen());
    TEST_ASSERT_NOT_NULL(find(v.root(), "2 FAULTS AT START"));
    TEST_ASSERT_NOT_NULL(find(v.root(), "Beam blocked"));
    tapText(v.root(), "OK");
    TEST_ASSERT_EQUAL_INT32(NoticeView::PRIMARY, last.action);
    tapText(v.root(), "DETAILS");
    TEST_ASSERT_EQUAL_INT32(NoticeView::SECONDARY, last.action);

    // A modal card stands on a backdrop over the page, which takes the touch beside the card.
    n.lines[0] = {controls::Light::FAILED, true, "Top lane 2 failed",
                  "Its sensor reads as covered. The game goes on without this lane."};
    n.lines[1] = {controls::Light::WORKS, true, "15 sensors work", ""};
    n.modal = true;
    n.width = NoticeView::DIALOG_PX;
    n.secondary = nullptr;
    v.build(rig.parent, n, pressed, nullptr);
    run();
    lv_obj_t* backdrop = lv_obj_get_child(v.root(), 0);
    TEST_ASSERT_EQUAL_HEX32(controls::color::NIGHT,
                            hex(lv_obj_get_style_bg_color(backdrop, LV_PART_MAIN)));
    presses = 0;
    tapAt(4, 4);
    TEST_ASSERT_EQUAL_UINT32(0, presses);
    TEST_ASSERT_NULL(find(v.root(), "DETAILS"));
    // the strip sits right under the card's rim, a fault's card as much as a note's
    lv_obj_t* card = lv_obj_get_child(backdrop, 0);
    lv_area_t c;
    lv_area_t s;
    lv_obj_get_coords(card, &c);
    lv_obj_get_coords(lv_obj_get_child(card, 0), &s);
    TEST_ASSERT_EQUAL_INT32(c.y1 + 2, s.y1);
    TEST_ASSERT_EQUAL_INT32(c.x1 + 2, s.x1);
    v.close();
    TEST_ASSERT_FALSE(v.isOpen());
}

// launch card: the launch card names the game and its high score.
void testLaunchCard() {
    Rig rig;
    LaunchCardView v;
    v.build(rig.parent, "CLASSIC", 1352330);
    run();
    TEST_ASSERT_NOT_NULL(find(v.root(), "NOW PLAYING"));
    TEST_ASSERT_NOT_NULL(find(v.root(), "1,352,330"));
    // the swipe line stays between the curtains at both sides
    lv_obj_t* swipe = find(v.root(), "SWIPE IN FROM THE RIGHT");
    TEST_ASSERT_NOT_NULL(swipe);
    lv_area_t a;
    lv_obj_get_coords(swipe, &a);
    TEST_ASSERT_TRUE(a.x1 >= 34);
    TEST_ASSERT_TRUE(a.x2 < W - 34);
}

// curtain: the curtain closes on its plaque and opens again.
void testCurtain() {
    Rig rig;
    CurtainView v;
    v.build(rig.parent);
    run();
    TEST_ASSERT_FALSE(v.isClosed());
    lv_obj_t* left = lv_obj_get_child(v.root(), 0);
    TEST_ASSERT_TRUE(lv_obj_get_x(left) < 0);
    v.close("CLASSIC");
    run(CurtainView::CLOSE_MS + STEP_MS);
    TEST_ASSERT_TRUE(v.isClosed());
    TEST_ASSERT_EQUAL_INT32(0, lv_obj_get_x(left));
    lv_obj_t* name = find(v.root(), "CLASSIC");
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_FALSE(lv_obj_is_visible(name) == false);
    v.open();
    run(CurtainView::OPEN_MS + STEP_MS);
    TEST_ASSERT_FALSE(v.isClosed());
    TEST_ASSERT_TRUE(lv_obj_get_x(left) < 0);
}

// Counts the labels below @p obj that read @p text exactly.
constexpr int32_t TITLE_GAP_PX = 4;   // between the valance's scallops and a title

static uint32_t countText(lv_obj_t* obj, const char* text) {
    uint32_t n = 0;
    if (lv_obj_check_type(obj, &lv_label_class) && strcmp(lv_label_get_text(obj), text) == 0) {
        n = 1;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); ++i) {
        n += countText(lv_obj_get_child(obj, int32_t(i)), text);
    }
    return n;
}

// game over: Game over lists several players by their scores with the winner first and marks
// each score that made the game's list TOP 5, shows a new high score, gives a single player the
// score large, marked or not, and goes on at a tap.
void testGameOver() {
    Rig rig;
    GameOverView v;
    const GameOverView::Player players[] = {{980120, 3}, {1288450, 2}, {212300, 0}};
    v.build(rig.parent, players, 3, pressed, nullptr);
    run();
    TEST_ASSERT_NOT_NULL(find(v.root(), "WINNER"));
    // the two scores in the list are marked, with no place that would stand beside WINNER
    TEST_ASSERT_EQUAL_UINT32(2, countText(v.root(), "TOP 5"));
    TEST_ASSERT_NULL(find(v.root(), "2ND"));
    TEST_ASSERT_NULL(find(v.root(), "3RD"));
    TEST_ASSERT_NULL(find(v.root(), "NEW HIGH SCORE"));
    TEST_ASSERT_NOT_NULL(find(v.root(), "your names"));
    // the lines stand by score: P2 first and lit, then P1, then P3
    lv_area_t first;
    lv_area_t second;
    lv_area_t third;
    lv_obj_get_coords(find(v.root(), "1,288,450"), &first);
    lv_obj_get_coords(find(v.root(), "980,120"), &second);
    lv_obj_get_coords(find(v.root(), "212,300"), &third);
    TEST_ASSERT_TRUE(first.y1 < second.y1);
    TEST_ASSERT_TRUE(second.y1 < third.y1);
    lv_obj_t* winner = lv_obj_get_parent(find(v.root(), "WINNER"));
    TEST_ASSERT_EQUAL_PTR(winner, lv_obj_get_parent(find(v.root(), "P2")));
    TEST_ASSERT_EQUAL_HEX32(controls::color::BULB,
                            hex(lv_obj_get_style_border_color(winner, LV_PART_MAIN)));
    // the winner's line is an opaque gold, so the stage does not show through it
    TEST_ASSERT_EQUAL_UINT8(LV_OPA_COVER, lv_obj_get_style_bg_opa(winner, LV_PART_MAIN));
    TEST_ASSERT_EQUAL_UINT8(LV_OPA_COVER, lv_obj_get_style_bg_main_opa(winner, LV_PART_MAIN));
    TEST_ASSERT_EQUAL_UINT8(LV_OPA_COVER, lv_obj_get_style_bg_grad_opa(winner, LV_PART_MAIN));
    TEST_ASSERT_EQUAL(LV_GRAD_DIR_VER, lv_obj_get_style_bg_grad_dir(winner, LV_PART_MAIN));
    TEST_ASSERT_TRUE(lv_obj_get_style_bg_color(winner, LV_PART_MAIN).red > 100);
    tapAt(W / 2, H - 10);
    TEST_ASSERT_EQUAL_INT32(GameOverView::GO_ON, last.action);

    // the title stands clear of the valance's scallops, and four lines leave the footer room
    // below them; the stage places the rays, two curtains, the valance and then the title
    lv_area_t valance;
    lv_area_t title;
    lv_obj_get_coords(lv_obj_get_child(v.root(), 3), &valance);
    lv_obj_get_coords(lv_obj_get_child(v.root(), 4), &title);
    TEST_ASSERT_TRUE(title.y1 >= valance.y2 + TITLE_GAP_PX);
    const GameOverView::Player four[] = {{400, 0}, {300, 0}, {200, 0}, {100, 0}};
    v.build(rig.parent, four, 4, pressed, nullptr);
    run();
    lv_area_t lowest;
    lv_area_t footer;
    lv_obj_get_coords(lv_obj_get_parent(find(v.root(), "100")), &lowest);
    lv_obj_get_coords(find(v.root(), "your names"), &footer);
    TEST_ASSERT_TRUE(lowest.y2 < footer.y1);
    TEST_ASSERT_TRUE(footer.y2 < H);

    const GameOverView::Player best[] = {{1402760, 1}, {655300, 0}};
    v.build(rig.parent, best, 2, pressed, nullptr);
    run();
    TEST_ASSERT_NOT_NULL(find(v.root(), "NEW HIGH SCORE"));
    TEST_ASSERT_EQUAL_UINT32(1, countText(v.root(), "TOP 5"));

    // a score of ten characters ends before the winner's TOP 5
    const GameOverView::Player wide[] = {{12345670, 2}, {980120, 0}};
    v.build(rig.parent, wide, 2, pressed, nullptr);
    run();
    lv_area_t digits;
    lv_area_t top;
    lv_obj_get_coords(find(v.root(), "12,345,670"), &digits);
    lv_obj_get_coords(find(v.root(), "TOP 5"), &top);
    TEST_ASSERT_TRUE(digits.x2 < top.x1);

    const GameOverView::Player single[] = {{744600, 4}};
    v.build(rig.parent, single, 1, pressed, nullptr);
    run();
    TEST_ASSERT_NOT_NULL(find(v.root(), "YOUR SCORE"));
    TEST_ASSERT_NOT_NULL(find(v.root(), "744,600"));
    TEST_ASSERT_NOT_NULL(find(v.root(), "TOP 5"));
    TEST_ASSERT_NULL(find(v.root(), "4TH"));
    TEST_ASSERT_NULL(find(v.root(), "NEW HIGH SCORE"));
    TEST_ASSERT_NULL(find(v.root(), "WINNER"));
    TEST_ASSERT_NOT_NULL(find(v.root(), "your name"));

    // a single best score shows the banner alone, and a score off the list no mark
    const GameOverView::Player singleBest[] = {{1402760, 1}};
    v.build(rig.parent, singleBest, 1, pressed, nullptr);
    run();
    TEST_ASSERT_NOT_NULL(find(v.root(), "NEW HIGH SCORE"));
    TEST_ASSERT_NULL(find(v.root(), "TOP 5"));
    const GameOverView::Player singleMissed[] = {{212300, 0}};
    v.build(rig.parent, singleMissed, 1, pressed, nullptr);
    run();
    TEST_ASSERT_NOT_NULL(find(v.root(), "212,300"));
    TEST_ASSERT_NULL(find(v.root(), "NEW HIGH SCORE"));
    TEST_ASSERT_NULL(find(v.root(), "TOP 5"));
    TEST_ASSERT_NOT_NULL(find(v.root(), "your name"));
}

// games: the game choice reports Play, the gear and the status bulb, turns its ring of cards with
// a tap on a neighbor or a drag past the snap, lets a short drag slide back, leads from the last
// game back to the first, and keeps Play off for a game that cannot start.
void testGames() {
    Rig rig;
    GamesView v;
    const GamesView::Game games[] = {{"CLASSIC", 1352330, nullptr, false, "", ""},
                                     {"CROC RIDE", 0, nullptr, true, "game.yml line 42", "Unknown"},
                                     {"SPACE RUN", 845200, nullptr, false, "", ""}};
    v.build(rig.parent, games, 3, 0, true, pressed, nullptr);
    run();
    TEST_ASSERT_NOT_NULL(find(v.root(), "1,352,330"));
    // both neighbors peek in, the game before the first being the last
    TEST_ASSERT_NOT_NULL(find(v.root(), "845,200"));
    TEST_ASSERT_NOT_NULL(find(v.root(), "game.yml line 42"));
    tapText(v.root(), "PLAY");
    TEST_ASSERT_EQUAL_INT32(GamesView::PLAY, last.action);
    TEST_ASSERT_EQUAL_INT32(0, last.value);
    tapText(v.root(), LV_SYMBOL_SETTINGS);
    TEST_ASSERT_EQUAL_INT32(GamesView::SETTINGS, last.action);
    tapAt(22, 17);
    TEST_ASSERT_EQUAL_INT32(GamesView::STATUS, last.action);
    // the neighbor at the right turns the ring to the game that cannot start
    tapAt(W - 10, 110);
    run(GamesView::SLIDE_MS + REFRESH_MS);
    TEST_ASSERT_EQUAL_INT32(GamesView::SHOWN, last.action);
    TEST_ASSERT_EQUAL_INT32(1, last.value);
    TEST_ASSERT_EQUAL_UINT8(1, v.shown());
    presses = 0;
    tapText(v.root(), "PLAY");
    TEST_ASSERT_EQUAL_UINT32(0, presses);
    // on past the last game to the first, and back by the neighbor at the left
    v.step(true);
    run(GamesView::SLIDE_MS + REFRESH_MS);
    v.step(true);
    run(GamesView::SLIDE_MS + REFRESH_MS);
    TEST_ASSERT_EQUAL_UINT8(0, v.shown());
    tapAt(10, 110);
    run(GamesView::SLIDE_MS + REFRESH_MS);
    TEST_ASSERT_EQUAL_UINT8(2, v.shown());

    // A finger dragging the ring moves it along, a short drag lets it slide back, and a drag past
    // the snap turns it.
    const uint8_t before = v.shown();
    v.drag(-20);
    TEST_ASSERT_EQUAL_INT32(-20, v.offset());
    v.letGo();
    run(GamesView::SLIDE_MS + REFRESH_MS);
    TEST_ASSERT_EQUAL_UINT8(before, v.shown());
    v.drag(-GamesView::SNAP_PX - 4);
    v.letGo();
    run(GamesView::SLIDE_MS + REFRESH_MS);
    TEST_ASSERT_EQUAL_UINT8((before + 1) % 3, v.shown());
    v.drag(GamesView::SNAP_PX + 4);
    v.letGo();
    run(GamesView::SLIDE_MS + REFRESH_MS);
    TEST_ASSERT_EQUAL_UINT8(before, v.shown());
    // a finger dragged across a neighbor and lifted there turns the ring once, by the drag
    presses = 0;
    run();
    pointer = {true, W - 10, 110};
    lv_indev_read(indev);
    for (int32_t x = W - 10; x > W - 10 - GamesView::SNAP_PX - 20; x -= 8) {
        pointer.x = x;
        lv_indev_read(indev);
    }
    pointer.down = false;
    lv_indev_read(indev);
    run(GamesView::SLIDE_MS + REFRESH_MS);
    TEST_ASSERT_EQUAL_UINT32(1, presses);
    TEST_ASSERT_EQUAL_UINT8((before + 1) % 3, v.shown());

    // Without a fault the status bulb stays away, and a single game has no neighbors.
    v.build(rig.parent, games, 1, 0, false, pressed, nullptr);
    run();
    presses = 0;
    tapAt(22, 17);
    v.step(true);
    run(GamesView::SLIDE_MS + REFRESH_MS);
    TEST_ASSERT_EQUAL_UINT32(0, presses);
    TEST_ASSERT_NULL(find(v.root(), "845,200"));
}

// games preview: a card that can start asks for its preview once it has stood PREVIEW_MS in the
// middle, once each time it comes there, its cover has the preview's size, a dragged or turning
// ring has no cover to play in, and a finger that takes the ring before the wait ends keeps the
// preview away.
void testGamesPreview() {
    Rig rig;
    GamesView v;
    const GamesView::Game games[] = {{"CLASSIC", 1352330, nullptr, false, "", ""},
                                     {"CROC RIDE", 0, nullptr, true, "game.yml line 42", "Unknown"},
                                     {"SPACE RUN", 845200, nullptr, false, "", ""}};
    v.build(rig.parent, games, 3, 0, false, pressed, nullptr);
    presses = 0;
    run(GamesView::PREVIEW_MS - REFRESH_MS);
    TEST_ASSERT_EQUAL_UINT32(0, presses);
    run(2 * REFRESH_MS);
    TEST_ASSERT_EQUAL_UINT32(1, presses);
    TEST_ASSERT_EQUAL_INT32(GamesView::PREVIEW, last.action);
    TEST_ASSERT_EQUAL_INT32(0, last.value);
    lv_obj_t* cover = v.preview();
    TEST_ASSERT_NOT_NULL(cover);
    lv_area_t a;
    lv_obj_get_content_coords(cover, &a);
    TEST_ASSERT_EQUAL_INT32(GamesView::PREVIEW_WIDTH, lv_area_get_width(&a));
    TEST_ASSERT_EQUAL_INT32(GamesView::PREVIEW_HEIGHT, lv_area_get_height(&a));
    // a short drag takes the cover away, and the card that slides back asks no second time
    v.drag(-20);
    TEST_ASSERT_NULL(v.preview());
    v.letGo();
    run(GamesView::SLIDE_MS + GamesView::PREVIEW_MS + 2 * REFRESH_MS);
    TEST_ASSERT_EQUAL_PTR(cover, v.preview());
    TEST_ASSERT_EQUAL_UINT32(1, presses);
    // the game that cannot start asks for none
    v.step(true);
    TEST_ASSERT_NULL(v.preview());
    run(GamesView::SLIDE_MS + GamesView::PREVIEW_MS + 2 * REFRESH_MS);
    TEST_ASSERT_EQUAL_INT32(GamesView::SHOWN, last.action);
    TEST_ASSERT_EQUAL_INT32(1, last.value);
    // a finger on the ring before the wait ends holds the preview until the ring stands again
    v.step(true);
    run(GamesView::SLIDE_MS + REFRESH_MS);
    presses = 0;
    v.drag(-20);
    run(GamesView::PREVIEW_MS + REFRESH_MS);
    TEST_ASSERT_EQUAL_UINT32(0, presses);
    v.letGo();
    run(GamesView::SLIDE_MS + GamesView::PREVIEW_MS + 2 * REFRESH_MS);
    TEST_ASSERT_EQUAL_INT32(GamesView::PREVIEW, last.action);
    TEST_ASSERT_EQUAL_INT32(2, last.value);
}

// settings: the four tiles and the Service row report their pages, Status glows red for a fault,
// and a tile's detail with its icons fits the tile.
void testSettings() {
    Rig rig;
    SettingsView v;
    SettingsView::State s;
    s.sound = LV_SYMBOL_AUDIO " 50 %   " LV_SYMBOL_BELL " 70 %";
    s.lights = "80 %";
    s.status = "1 fault";
    s.fault = true;
    v.build(rig.parent, s, pressed, nullptr);
    run();
    const struct {
        const char* name;
        SettingsView::Action action;
    } tiles[] = {{"SOUND", SettingsView::SOUND},
                 {"LIGHTS", SettingsView::LIGHTS},
                 {"STATUS", SettingsView::STATUS},
                 {"OPTIONS", SettingsView::OPTIONS},
                 {"Service", SettingsView::SERVICE}};
    for (const auto& t : tiles) {
        tapText(v.root(), t.name);
        TEST_ASSERT_EQUAL_INT32(t.action, last.action);
    }
    tapText(v.root(), LV_SYMBOL_LEFT);
    TEST_ASSERT_EQUAL_INT32(SettingsView::BACK, last.action);
    lv_obj_t* status = lv_obj_get_parent(find(v.root(), "STATUS"));
    TEST_ASSERT_EQUAL_HEX32(controls::color::NEON,
                            hex(lv_obj_get_style_border_color(status, LV_PART_MAIN)));
    TEST_ASSERT_NOT_NULL(find(v.root(), "1 fault"));
    TEST_ASSERT_NOT_NULL(find(v.root(), "Set up this machine"));
    // a tile's detail with its icons stands in one line inside the tile
    lv_obj_t* detail = find(v.root(), " 70 %");
    lv_area_t d;
    lv_area_t t;
    lv_obj_get_coords(detail, &d);
    lv_obj_get_coords(lv_obj_get_parent(detail), &t);
    TEST_ASSERT_TRUE(d.x2 < t.x2 && d.y2 < t.y2);
}

// options: Options lists a switch for the animated background and one for the boot film, each
// reports off and then on, and a switch built off shows off.
void testOptions() {
    Rig rig;
    OptionsView v;
    OptionsView::State s;
    v.build(rig.parent, s, pressed, nullptr);
    run();
    const struct {
        const char* title;
        OptionsView::Action action;
    } switches[] = {{"Animated background", OptionsView::ANIMATED},
                    {"Boot film", OptionsView::BOOT_FILM}};
    for (const auto& sw : switches) {
        presses = 0;
        tapText(v.root(), sw.title);
        TEST_ASSERT_EQUAL_INT32(sw.action, last.action);
        TEST_ASSERT_EQUAL_INT32(0, last.value);
        tapText(v.root(), sw.title);
        TEST_ASSERT_EQUAL_INT32(1, last.value);
        TEST_ASSERT_EQUAL_UINT32(2, presses);
    }
    tapText(v.root(), LV_SYMBOL_LEFT);
    TEST_ASSERT_EQUAL_INT32(OptionsView::BACK, last.action);
    s.bootFilm = false;
    v.build(rig.parent, s, pressed, nullptr);
    run();
    TEST_ASSERT_FALSE(controls::isOn(lv_obj_get_parent(find(v.root(), "Boot film"))));
    TEST_ASSERT_TRUE(controls::isOn(lv_obj_get_parent(find(v.root(), "Animated background"))));
}

// levels: the levels show and report their values, each from 0 to 100 %.
void testLevels() {
    Rig rig;
    LevelsView v;
    LevelsView::Levels l;
    l.title = "SOUND";
    l.names[0] = "MUSIC";
    l.names[1] = "EFFECTS";
    l.values[0] = 50;
    l.values[1] = 95;
    v.build(rig.parent, l, pressed, nullptr);
    run();
    TEST_ASSERT_NOT_NULL(find(v.root(), "50 %"));
    TEST_ASSERT_NOT_NULL(find(v.root(), "95 %"));
    // the first level's plus, at the right end of its slider
    tapAt(W - 30, 60 + 20 + 22);
    TEST_ASSERT_EQUAL_INT32(LevelsView::FIRST, last.action);
    TEST_ASSERT_EQUAL_INT32(55, last.value);
    TEST_ASSERT_EQUAL_INT32(55, v.value(0));
    // the second level reaches the end of its track whatever the first one holds
    tapAt(W - 30, 146 + 20 + 22);
    TEST_ASSERT_EQUAL_INT32(LevelsView::SECOND, last.action);
    TEST_ASSERT_EQUAL_INT32(controls::SLIDER_MAX, v.value(1));
    tapText(v.root(), LV_SYMBOL_LEFT);
    TEST_ASSERT_EQUAL_INT32(LevelsView::BACK, last.action);
}

// service: the steps report their pages, the firmware and the config show, and a change of the
// lights' adapter asks first, warns before the larger one, and reports only once confirmed.
void testService() {
    Rig rig;
    ServiceView v;
    ServiceView::About a;
    a.firmware = "5835ccf";
    a.config = "/config.yml";
    a.adapterA = ServiceView::SMALL_ADAPTER_A;
    v.build(rig.parent, a, pressed, nullptr);
    run();
    TEST_ASSERT_NOT_NULL(find(v.root(), "5835ccf"));
    TEST_ASSERT_NOT_NULL(find(v.root(), "/config.yml"));
    TEST_ASSERT_NOT_NULL(find(v.root(), "3 A adapter"));
    tapText(v.root(), "Ball sensors");
    TEST_ASSERT_EQUAL_INT32(ServiceView::RECORD, last.action);
    tapText(v.root(), "Calibrate touch");
    TEST_ASSERT_EQUAL_INT32(ServiceView::CALIBRATE, last.action);
    // the larger adapter asks with a warning, and keeping the smaller one changes nothing
    presses = 0;
    tapText(v.root(), "Lights adapter");
    TEST_ASSERT_NOT_NULL(find(v.root(), "SWITCH TO 6 A?"));
    TEST_ASSERT_NOT_NULL(find(v.root(), "runs hot"));
    tapText(v.root(), "KEEP 3 A");
    run();
    TEST_ASSERT_EQUAL_UINT32(0, presses);
    TEST_ASSERT_NULL(find(v.root(), "SWITCH TO 6 A?"));
    TEST_ASSERT_EQUAL_UINT8(ServiceView::SMALL_ADAPTER_A, v.adapter());
    // confirmed, it reports the larger adapter and shows it
    tapText(v.root(), "Lights adapter");
    tapText(v.root(), "USE 6 A");
    run();
    TEST_ASSERT_EQUAL_INT32(ServiceView::ADAPTER, last.action);
    TEST_ASSERT_EQUAL_INT32(ServiceView::LARGE_ADAPTER_A, last.value);
    TEST_ASSERT_NOT_NULL(find(v.root(), "6 A adapter"));
    // back to the smaller one asks without the warning, with 3 A still at the left
    tapText(v.root(), "Lights adapter");
    TEST_ASSERT_NOT_NULL(find(v.root(), "SWITCH TO 3 A?"));
    TEST_ASSERT_NULL(find(v.root(), "runs hot"));
    run();
    lv_area_t small;
    lv_area_t large;
    lv_obj_get_coords(find(v.root(), "USE 3 A"), &small);
    lv_obj_get_coords(find(v.root(), "KEEP 6 A"), &large);
    TEST_ASSERT_TRUE(small.x1 < large.x1);
    tapText(v.root(), "USE 3 A");
    run();
    TEST_ASSERT_EQUAL_INT32(ServiceView::SMALL_ADAPTER_A, last.value);
    tapText(v.root(), LV_SYMBOL_LEFT);
    TEST_ASSERT_EQUAL_INT32(ServiceView::BACK, last.action);
}

// sensors: the ball sensors stand in a list that scrolls, each says whether it is recorded, and a
// tap reports the sensor.
void testSensors() {
    Rig rig;
    SensorsView v;
    SensorsView::Sensor list[10];
    for (auto& sensor : list) {
        sensor = {"Inlane 1", SensorsView::Recorded::YES};
    }
    list[1] = {"Top lane 2", SensorsView::Recorded::AGAIN};
    list[2] = {"Scoop", SensorsView::Recorded::NO};
    v.build(rig.parent, list, 10, pressed, nullptr);
    run();
    TEST_ASSERT_NOT_NULL(find(v.root(), "Record it again"));
    TEST_ASSERT_NOT_NULL(find(v.root(), "Not recorded"));
    TEST_ASSERT_EQUAL_HEX32(
        controls::color::AMBER,
        hex(lv_obj_get_style_text_color(find(v.root(), "Not recorded"), LV_PART_MAIN)));
    tapText(v.root(), "Top lane 2");
    TEST_ASSERT_EQUAL_INT32(SensorsView::SENSOR, last.action);
    TEST_ASSERT_EQUAL_INT32(1, last.value);
    lv_obj_t* rows = lv_obj_get_parent(lv_obj_get_parent(find(v.root(), "Top lane 2")));
    TEST_ASSERT_TRUE(lv_obj_get_scroll_bottom(rows) > 0);
    tapText(v.root(), LV_SYMBOL_LEFT);
    TEST_ASSERT_EQUAL_INT32(SensorsView::BACK, last.action);
}

// record sensor: the recording asks for every sensor clear, then for the ball, each started with
// Start, shows its progress while it measures, and ends with Done and a note.
void testRecordSensor() {
    using Stage = RecordSensorView::Stage;
    Rig rig;
    RecordSensorView v;
    v.build(rig.parent, "Top lane 2", pressed, nullptr);
    run();
    TEST_ASSERT_NOT_NULL(find(v.root(), "Keep every ball off the sensors"));
    tapText(v.root(), "START");
    TEST_ASSERT_EQUAL_INT32(RecordSensorView::START, last.action);
    v.setStage(Stage::MEASURING_CLEAR);
    v.setProgress(40);
    run();
    // while it measures, the bar shows and Start waits
    TEST_ASSERT_FALSE(lv_obj_is_visible(lv_obj_get_parent(find(v.root(), "START"))));
    v.setStage(Stage::BALL);
    run();
    TEST_ASSERT_NOT_NULL(find(v.root(), "Lay a ball on Top lane 2"));
    v.setStage(Stage::DONE, "Top lane 3 moved in the read order. Record it again.");
    run();
    TEST_ASSERT_NOT_NULL(find(v.root(), "Top lane 2 is recorded"));
    TEST_ASSERT_NOT_NULL(find(v.root(), "Record it again"));
    TEST_ASSERT_FALSE(lv_obj_is_visible(lv_obj_get_parent(find(v.root(), "CANCEL"))));
    tapText(v.root(), "DONE");
    TEST_ASSERT_EQUAL_INT32(RecordSensorView::DONE, last.action);
}

// calibrate touch: the mark moves from corner to corner and Cancel reports.
void testCalibrateTouch() {
    Rig rig;
    CalibrateTouchView v;
    v.build(rig.parent, pressed, nullptr);
    run();
    lv_obj_t* mark = lv_obj_get_child(v.root(), 0);
    lv_area_t a;
    lv_obj_get_coords(mark, &a);
    TEST_ASSERT_EQUAL_INT32(0, a.x1);
    TEST_ASSERT_EQUAL_INT32(0, a.y1);
    v.setMark(2);
    run();
    lv_obj_get_coords(mark, &a);
    TEST_ASSERT_EQUAL_INT32(W - 1, a.x2);
    TEST_ASSERT_EQUAL_INT32(H - 1, a.y2);
    v.setMark(3);
    run();
    lv_obj_get_coords(mark, &a);
    TEST_ASSERT_EQUAL_INT32(0, a.x1);
    TEST_ASSERT_EQUAL_INT32(H - 1, a.y2);
    tapText(v.root(), "CANCEL");
    TEST_ASSERT_EQUAL_INT32(CalibrateTouchView::CANCEL, last.action);
}

// warning lights: a tile per device reports its index.
void testWarningLights() {
    Rig rig;
    WarningLightsView v;
    const WarningLightsView::Tile tiles[] = {{"Seal sensor", controls::Light::WORKS},
                                             {"Drain beam", controls::Light::WORKS},
                                             {"Bumpers", controls::Light::WORKS},
                                             {"Ball sensors", controls::Light::FAILED},
                                             {"3 warnings", controls::Light::WARNING}};
    v.build(rig.parent, tiles, 5, pressed, nullptr);
    run();
    tapText(v.root(), "Ball sensors");
    TEST_ASSERT_EQUAL_INT32(WarningLightsView::TILE, last.action);
    TEST_ASSERT_EQUAL_INT32(3, last.value);
    tapText(v.root(), "3 warnings");
    TEST_ASSERT_EQUAL_INT32(4, last.value);
    lv_obj_t* failed = lv_obj_get_parent(find(v.root(), "Ball sensors"));
    TEST_ASSERT_EQUAL_HEX32(controls::color::NEON,
                            hex(lv_obj_get_style_border_color(failed, LV_PART_MAIN)));
    tapText(v.root(), LV_SYMBOL_LEFT);
    TEST_ASSERT_EQUAL_INT32(WarningLightsView::BACK, last.action);
}

// warnings: the warnings show with their counts in a list that scrolls, with a fade at the foot
// while more lies below and at the top once scrolled.
void testWarnings() {
    Rig rig;
    WarningsView v;
    WarningsView::Warning list[8];
    for (auto& w : list) {
        w = {"Config", "audio.music_volume is 1.4, held at 1", 1};
    }
    list[0].count = 2;
    v.build(rig.parent, list, 8, pressed, nullptr);
    run();
    TEST_ASSERT_NOT_NULL(find(v.root(), "\xC3\x97"
                                        "2"));
    TEST_ASSERT_NOT_NULL(find(v.root(), "8"));
    lv_obj_t* entries = lv_obj_get_parent(lv_obj_get_parent(find(v.root(), "audio.music")));
    TEST_ASSERT_TRUE(lv_obj_get_scroll_bottom(entries) > 0);
    lv_obj_t* top = lv_obj_get_sibling(entries, 1);
    lv_obj_t* foot = lv_obj_get_sibling(entries, 2);
    TEST_ASSERT_TRUE(lv_obj_is_hidden(top));
    TEST_ASSERT_FALSE(lv_obj_is_hidden(foot));
    lv_obj_scroll_to_y(entries, LV_COORD_MAX, LV_ANIM_OFF);
    run();
    TEST_ASSERT_FALSE(lv_obj_is_hidden(top));
    TEST_ASSERT_TRUE(lv_obj_is_hidden(foot));
    tapText(v.root(), LV_SYMBOL_LEFT);
    TEST_ASSERT_EQUAL_INT32(WarningsView::BACK, last.action);
}

// status: the status bulb shows for a fault and reports a tap, and the card indicator shows while
// the card is written.
void testStatus() {
    Rig rig;
    StatusView v;
    v.build(rig.parent, pressed, nullptr);
    run();
    tapAt(W - 10, 10);
    TEST_ASSERT_EQUAL_UINT32(0, presses);
    v.setFault(true);
    tapAt(W - 10, 10);
    TEST_ASSERT_EQUAL_INT32(StatusView::STATUS, last.action);
    lv_obj_t* card = lv_obj_get_child(v.root(), 1);
    TEST_ASSERT_FALSE(lv_obj_is_visible(card));
    v.setWriting(true);
    run();
    TEST_ASSERT_TRUE(lv_obj_is_visible(card));
    // a tap beside the bulb goes through to what lies beneath
    presses = 0;
    tapAt(W / 2, H / 2);
    TEST_ASSERT_EQUAL_UINT32(0, presses);
}

// toast: a toast drops in, reports a tap, and shrinks away after its time.
void testToast() {
    Rig rig;
    ToastView v;
    v.build(rig.parent, pressed, nullptr);
    run();
    TEST_ASSERT_FALSE(v.isShowing());
    v.show(true, "BALL SENSOR FAILED", "Top lane 2");
    run(ToastView::DROP_MS + STEP_MS);
    TEST_ASSERT_TRUE(v.isShowing());
    tapText(v.root(), "BALL SENSOR FAILED");
    TEST_ASSERT_EQUAL_INT32(ToastView::OPEN, last.action);
    run(ToastView::FAULT_MS + 2 * ToastView::DROP_MS);
    TEST_ASSERT_FALSE(v.isShowing());
    // a toast of a device that works again takes no tap and goes sooner
    presses = 0;
    v.show(false, "BALL SENSOR WORKS AGAIN", "Top lane 2");
    run(ToastView::DROP_MS + STEP_MS);
    tapText(v.root(), "WORKS AGAIN");
    TEST_ASSERT_EQUAL_UINT32(0, presses);
    run(ToastView::WORKS_MS + 2 * ToastView::DROP_MS);
    TEST_ASSERT_FALSE(v.isShowing());
}

// choose name: the names report their index, Other name and Skip report.
void testChooseName() {
    Rig rig;
    ChooseNameView v;
    const char* const names[] = {"LEA", "ANNA", "MAXI"};
    v.build(rig.parent, NameFor{1, 3, 980120}, names, 3, true, pressed, nullptr);
    run();
    TEST_ASSERT_NOT_NULL(find(v.root(), "PLAYER 1"));
    TEST_ASSERT_NOT_NULL(find(v.root(), "980,120"));
    tapText(v.root(), "ANNA");
    TEST_ASSERT_EQUAL_INT32(ChooseNameView::NAME, last.action);
    TEST_ASSERT_EQUAL_INT32(1, last.value);
    tapText(v.root(), "OTHER NAME");
    TEST_ASSERT_EQUAL_INT32(ChooseNameView::OTHER, last.action);
    tapText(v.root(), "SKIP");
    TEST_ASSERT_EQUAL_INT32(ChooseNameView::SKIP, last.action);
    lv_obj_t* lit = lv_obj_get_parent(find(v.root(), "LEA"));
    TEST_ASSERT_EQUAL_HEX32(controls::color::BULB,
                            hex(lv_obj_get_style_border_color(lit, LV_PART_MAIN)));
}

// name: the keys type a name of up to ten letters, delete takes one off, OK reports the length.
void testName() {
    Rig rig;
    NameView v;
    v.build(rig.parent, NameFor{2, 2, 1288450}, pressed, nullptr);
    run();
    TEST_ASSERT_NOT_NULL(find(v.root(), "0 / 10"));
    // the keys of the first row stand 46 px apart from the left edge, 51 px from the top
    const auto key = [](char c) { tapAt(22 + 46 * ((c - 'A') % 7), 73 + 47 * ((c - 'A') / 7)); };
    key('M');
    key('A');
    key('X');
    TEST_ASSERT_EQUAL_INT32(NameView::KEY, last.action);
    TEST_ASSERT_EQUAL_INT32('X', last.value);
    tapText(v.root(), LV_SYMBOL_BACKSPACE);
    TEST_ASSERT_EQUAL_INT32(0, last.value);
    TEST_ASSERT_EQUAL_STRING("MA", v.name());
    TEST_ASSERT_NOT_NULL(find(v.root(), "2 / 10"));
    for (int i = 0; i < 12; ++i) {
        key('Z');
    }
    TEST_ASSERT_EQUAL_size_t(NameView::LETTERS_MAX, strlen(v.name()));
    tapText(v.root(), "OK");
    TEST_ASSERT_EQUAL_INT32(NameView::OK, last.action);
    TEST_ASSERT_EQUAL_INT32(NameView::LETTERS_MAX, last.value);
    tapText(v.root(), "SKIP");
    TEST_ASSERT_EQUAL_INT32(NameView::SKIP, last.action);
}

// high scores: the list lights its new entries, the countdown shows on Play again, and the
// buttons report.
void testHighScores() {
    Rig rig;
    HighScoresView v;
    const HighScoresView::Entry entries[] = {
        {"ANNA", 1352330, false}, {"MAXI", 1288450, true}, {"LEA", 980120, true}};
    v.build(rig.parent, "CLASSIC", entries, 3, nullptr, 15, pressed, nullptr);
    run();
    TEST_ASSERT_NOT_NULL(find(v.root(), "1,288,450"));
    TEST_ASSERT_NOT_NULL(find(v.root(), "3RD"));
    lv_obj_t* fresh = lv_obj_get_parent(find(v.root(), "MAXI"));
    lv_obj_t* old = lv_obj_get_parent(find(v.root(), "ANNA"));
    TEST_ASSERT_EQUAL_HEX32(controls::color::BULB,
                            hex(lv_obj_get_style_border_color(fresh, LV_PART_MAIN)));
    TEST_ASSERT_TRUE(hex(lv_obj_get_style_border_color(old, LV_PART_MAIN)) !=
                     controls::color::BULB);
    TEST_ASSERT_NOT_NULL(find(v.root(), "15#"));
    v.setCountdown(3);
    TEST_ASSERT_NOT_NULL(find(v.root(), " 3#"));
    tapText(v.root(), "PLAY AGAIN");
    TEST_ASSERT_EQUAL_INT32(HighScoresView::PLAY_AGAIN, last.action);
    tapText(v.root(), "GAMES");
    TEST_ASSERT_EQUAL_INT32(HighScoresView::GAMES, last.action);
    // the players' scores stand below a list none of them made
    v.build(rig.parent, "CLASSIC", entries, 3, "P1 TOM 212,300", 15, pressed, nullptr);
    run();
    TEST_ASSERT_NOT_NULL(find(v.root(), "P1 TOM 212,300"));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(testScore);
    RUN_TEST(testNotice);
    RUN_TEST(testLaunchCard);
    RUN_TEST(testCurtain);
    RUN_TEST(testGameOver);
    RUN_TEST(testGames);
    RUN_TEST(testGamesPreview);
    RUN_TEST(testSettings);
    RUN_TEST(testOptions);
    RUN_TEST(testLevels);
    RUN_TEST(testService);
    RUN_TEST(testSensors);
    RUN_TEST(testRecordSensor);
    RUN_TEST(testCalibrateTouch);
    RUN_TEST(testWarningLights);
    RUN_TEST(testWarnings);
    RUN_TEST(testStatus);
    RUN_TEST(testToast);
    RUN_TEST(testChooseName);
    RUN_TEST(testName);
    RUN_TEST(testHighScores);
    return UNITY_END();
}
