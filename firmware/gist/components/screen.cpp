// the picture memory of the running game in the PSRAM: full-screen pictures and frames from the
// top end, smaller pictures from the bottom end, so room a full-screen picture gives back is free
// in one piece for a frame. Every room starts on a 32-byte boundary and is a whole number of 32 bytes,
// so the storage driver reads straight into it rather than through its bounce buffer
class PictureMemory {
public:
    uint8_t* take(size_t bytes, bool large);   // gets room from its end, first fit in the holes there, nullptr when the ends would meet or no PSRAM was found
    void     give(uint8_t* room);              // gives room back; holes next to each other merge, and the end closes over its last block
    bool     empty() const;                    // gets whether every room has come back

private:
    static constexpr size_t kBytes = 8 * 1024 * 1024;   // the whole PSRAM, docs/parts/psram

    // ... the blocks of each end, in the order they lie
};

EXTMEM static uint8_t memory_[8 * 1024 * 1024] __attribute__((aligned(32)));   // the picture memory; take() hands out none of it while external_psram_size is below 8

// a picture file in the screen's picture memory, shown in as many LVGL images as the game likes
class Picture {
public:
    explicit Picture(Screen& screen) : screen_(screen) {}

    void      load(const FileReader& picture);   // reserves memory for the picture, starts reading the file into it and returns at once
    lv_obj_t* show(lv_obj_t* parent);        // creates an image of the picture inside parent, to place like any LVGL object
    void      release();                     // deletes every image of the picture and frees its memory, once the picture has loaded
    bool      loading() const;               // gets whether the picture still loads
    bool      failed() const;                // gets whether the last load found no file, a short one or a full picture memory

private:
    friend class Screen;

    static void forget(lv_event_t* e);       // takes an image the game deleted itself off the picture's list

    void settle();                           // once the header has landed, takes the room and reads the pixels; once they have, fills every image; gives back room a release() left waiting

    Screen&        screen_;
    FileReader     file_;
    FileRequest    read_;                    // the header first, then the pixels
    uint8_t        header_[12];              // LVGL's image header: format, width, height, stride
    lv_image_dsc_t image_ = {};              // what every image of the picture points at, 0 x 0 until the pixels land
    uint8_t*       room_  = nullptr;
    lv_obj_t*      images_   = nullptr;      // the first image of the picture; each image's user data points at the next
    bool           failed_   = false;
    bool           released_ = false;        // release() came while a read was on its way
    Picture*       next_     = nullptr;      // the next picture the screen knows, set at the first load()
    bool           known_    = false;        // load() registers the picture once, after every static object exists
};

// LVGL on the display: the game's layer and background, and the host's layer over them
class Screen : public Component {
public:
    Screen(Display& display, Touch& touch) : display_(display), touch_(touch) {}

    void begin();                            // starts LVGL on the display driver

    // takes every touch event; hands a touch to LVGL when an object lies under the finger where
    // it comes down, and makes one that none took into ScreenTapped or ScreenHeld
    bool translate(const DriverEvent& in, PinballEvents& out) override;

    // runs LVGL: its timers, the handlers of its controls and the redraw, except while the display
    // restarts; the main loop calls it in every pass
    void update();

    lv_obj_t* layer() { return lv_screen_active(); }   // gets the game's layer, the parent of the game's objects
    lv_obj_t* background() { return back_; }           // gets the game's background, the parent of what lies behind everything else
    void      showLayer(bool shown);         // shows or hides the game's layer; hidden, it keeps its objects

private:
    friend class GameHost;
    friend class Picture;
    friend class VideoPlayer;

    static constexpr uint16_t kWidth = 320;           // a row, which is whole steps of the driver
    static constexpr uint32_t kTapMs = 500;           // the longest touch that counts as a tap
    static constexpr uint8_t  kSlop  = 10;            // pixels a tap may move

    enum class Gesture : uint8_t { None, Tap, Hold };

    lv_obj_t* hostLayer() { return host_; }  // gets the parent of the host's objects
    lv_obj_t* videos() { return videos_; }   // gets the parent of the video player's image, in front of the game's background
    bool      settled() const { return pictures_.empty(); }   // gets whether the picture memory is empty
    void      showHost(bool shown);          // shows or hides the host's layer
    Gesture   takeGesture(uint16_t& x, uint16_t& y);   // gets the host the last gesture once, None when there is none
    void      reset();                       // deletes the game's objects, releases its pictures and shows the game's layer
    void      restart();                     // sets the display up again after a supervisor reset and sends the whole picture again once it is back; returns at once
    bool      taken(int16_t x, int16_t y);   // gets whether an object under x, y takes a touch, searched as LVGL does
    void      gesture(Gesture g, const DriverEvent& in);   // keeps a gesture for the host and makes it the game's event
    static bool near(uint32_t a, uint32_t b, uint8_t slop);   // gets whether two positions lie within slop pixels on both axes

    static void flush(lv_display_t* d, const lv_area_t* area, uint8_t* pixels);   // hands a rendered piece to the driver
    static void widen(lv_event_t* e);        // widens a changed area to whole rows
    static void readTouch(lv_indev_t* indev, lv_indev_data_t* data);   // LVGL's own look at the finger, pressed only for a touch an object took

    Display&      display_;
    Touch&        touch_;
    PictureMemory pictures_;
    Picture*      first_     = nullptr;      // every Picture that has loaded once
    lv_display_t* lv_        = nullptr;
    lv_obj_t*     host_      = nullptr;      // a full-screen container on LVGL's system layer, above everything
    lv_obj_t*     back_      = nullptr;      // a full-screen container on LVGL's bottom layer
    lv_obj_t*     videos_    = nullptr;      // the same, created after back_, so it lies in front of it
    bool          held_      = false;        // an object took the touch on the screen, until the finger leaves
    bool          redraw_    = false;        // the whole screen goes out again once the display is back
    uint32_t      downAt_    = 0;            // nowUs() of the last TouchDown
    uint32_t      downXy_    = 0;            // and its position
    Gesture       gesture_   = Gesture::None;
    uint32_t      gestureXy_ = 0;
    PinballEvent  made_;                     // the event translate() points out at
};

// LVGL's two buffers of the whole screen, RGB565 high byte first as the driver sends it, in RAM2:
// LVGL draws into one while the driver's interrupt reads the other, both through the cache
DMAMEM static uint8_t buffers_[2][320 * 240 * 2] __attribute__((aligned(32)));

void Screen::begin() {
    lv_init();
    lv_tick_set_cb(xTaskGetTickCount);       // FreeRTOS's milliseconds, which wrap where LVGL expects; nowUs() / 1000 would not
    lv_ = lv_display_create(320, 240);
    lv_display_set_buffers(lv_, buffers_[0], buffers_[1], sizeof buffers_[0], LV_DISPLAY_RENDER_MODE_DIRECT);   // LVGL copies what changed into the other buffer
    lv_display_set_flush_cb(lv_, flush);
    lv_display_set_user_data(lv_, this);
    lv_display_add_event_cb(lv_, widen, LV_EVENT_INVALIDATE_AREA, nullptr);

    // the game's layer is LVGL's screen itself, so LVGL's lists that open on the screen belong to
    // it; LVGL's top layer takes the game's popups; the host sits on the system layer above both
    lv_obj_t* screen = lv_screen_active();
    lv_obj_set_style_bg_opa(screen, LV_OPA_TRANSP, 0);   // so the background on LVGL's bottom layer shows
    host_   = lv_obj_create(lv_layer_sys());
    back_   = lv_obj_create(lv_layer_bottom());
    videos_ = lv_obj_create(lv_layer_bottom());
    for (lv_obj_t* o : {host_, back_, videos_}) {
        lv_obj_remove_style_all(o);                        // transparent, no border
        lv_obj_set_size(o, 320, 240);
    }
    for (lv_obj_t* o : {screen, lv_layer_top(), lv_layer_sys(), lv_layer_bottom(), host_, back_, videos_}) {
        lv_obj_set_clickable(o, false);                    // a touch on no object passes through
    }
    showHost(false);

    lv_indev_t* touch = lv_indev_create();
    lv_indev_set_type(touch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch, readTouch);                // LVGL looks at the finger itself, at its own pace
    lv_indev_set_user_data(touch, this);
}

// LVGL hands the whole buffer and the changed area apart; widen() has made the area whole rows, so
// its pixels lie in one piece from its first row on. A refused area cannot come from LVGL; LVGL
// still gets its flush_ready, so a wrong area costs a stale piece of screen rather than a main loop
// that waits for ever
void Screen::flush(lv_display_t* d, const lv_area_t* area, uint8_t* pixels) {
    Screen* self = static_cast<Screen*>(lv_display_get_user_data(d));
    const uint8_t* rows = pixels + (size_t)area->y1 * kWidth * 2;
    if (!self->display_.send(0, area->y1, kWidth, lv_area_get_height(area), rows,
                             [d] { lv_display_flush_ready(d); })) {
        lv_display_flush_ready(d);
    }
}

void Screen::widen(lv_event_t* e) {
    lv_area_t* a = static_cast<lv_area_t*>(lv_event_get_param(e));
    a->x1 = 0;
    a->x2 = kWidth - 1;                                  // whole rows lie in one piece in the buffer and on the driver's steps
}

// LVGL sees a touch only when an object took it at TouchDown, so a finger that comes down beside a
// button and slides onto it presses nothing, and its tap or hold stays the game's
void Screen::readTouch(lv_indev_t* indev, lv_indev_data_t* data) {
    Screen* self = static_cast<Screen*>(lv_indev_get_user_data(indev));
    const Finger f = self->touch_.finger();
    data->point.x = f.x;
    data->point.y = f.y;
    data->state   = f.down && self->held_ ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

// the order LVGL's own search takes: the system layer, the top layer, the screen, the bottom layer
bool Screen::taken(int16_t x, int16_t y) {
    lv_point_t p = {x, y};
    for (lv_obj_t* layer : {lv_layer_sys(), lv_layer_top(), lv_screen_active(), lv_layer_bottom()}) {
        if (lv_indev_search_obj(layer, &p)) return true;  // a hidden object finds nothing
    }
    return false;
}

bool Screen::translate(const DriverEvent& in, PinballEvents& out) {
    out = {};
    switch (in.type) {
        case DriverEventType::TouchDown:
            held_   = taken(in.payload & 0xFFFF, in.payload >> 16);   // LVGL itself hands the touch to the object
            downAt_ = in.time;
            downXy_ = in.payload;
            return true;
        case DriverEventType::TouchMoved:
            return true;
        case DriverEventType::TouchUp:
            if (held_) { held_ = false; return true; }
            if (elapsedUs(downAt_, kTapMs * 1000) || !near(in.payload, downXy_, kSlop)) return true;
            gesture(Gesture::Tap, in);
            break;
        case DriverEventType::TouchHeld:
            if (held_) return true;
            gesture(Gesture::Hold, in);
            break;
        default:
            return false;
    }
    out = {&made_, 1};
    return true;
}

void Screen::showLayer(bool shown) {
    lv_obj_set_hidden(lv_screen_active(), !shown);
}

void Screen::update() {
    for (Picture* p = first_; p; p = p->next_) p->settle();   // header and pixels landed, room left waiting
    if (display_.restarting()) return;       // LVGL rests while the panel restarts, so it never waits for it
    if (redraw_) {
        lv_obj_invalidate(lv_screen_active());   // LVGL redraws every layer over the invalid area
        redraw_ = false;
    }
    lv_timer_handler();                      // a refresh finds the last picture off the bus
}

// the panel's setup; update() redraws the whole screen once the panel is back
void Screen::restart() {
    display_.restart();
    redraw_ = true;
}

// the game's objects on its layer, its popups on LVGL's top layer and its background; the video
// player's image stays, which the video player's own reset() deletes
void Screen::reset() {
    lv_obj_clean(lv_screen_active());
    lv_obj_clean(lv_layer_top());
    lv_obj_clean(back_);
    for (Picture* p = first_; p; p = p->next_) p->release();  // a game's Picture outlives its game, since games live as long as the firmware
    showLayer(true);
}

void Picture::load(const FileReader& picture) {
    if (!known_) {                           // not in the constructor: a game's Picture may be built before the screen
        next_ = screen_.first_;
        screen_.first_ = this;
        known_ = true;
    }
    release();
    // ... file_ = picture, failed_ = false, the read of header_; settle() goes on from there
}

lv_obj_t* Picture::show(lv_obj_t* parent) {
    lv_obj_t* image = lv_image_create(parent);
    lv_image_set_src(image, &image_);        // 0 x 0 until the pixels land, then settle() sets it again
    lv_obj_add_event_cb(image, forget, LV_EVENT_DELETE, this);
    // ... onto the picture's list of images
    return image;
}

void Picture::release() {
    // ... every image on the list: lv_obj_delete(), whose LV_EVENT_DELETE takes it off the list
    if (read_.status() == FileStatus::Pending) { released_ = true; return; }   // settle() gives the room back once the read has landed
    if (room_) screen_.pictures_.give(room_);
    room_  = nullptr;
    image_ = {};
}
