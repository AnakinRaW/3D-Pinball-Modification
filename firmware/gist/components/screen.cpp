// a picture file starts with 256 palette entries of four bytes, RGB565 high byte first like a
// movie's pixels, the opacity and an unused byte. Entry 0 holds the width and the height, each
// high byte first, since index 0 stays transparent. Then come the pixels row by row from the top
// left, one byte each with the pixel's palette index.
// The pictures of one layer are converted together, so their files hold the same palette
constexpr uint32_t kTable = 256 * 4;          // where the pixels start

// a picture smaller than the screen, read once into a buffer the game gives and then drawn as
// often as needed into a layer with the colors of its set
class Picture {
public:
    template <size_t N>
    explicit Picture(uint8_t (&pixels)[N]) : pixels_(pixels), room_(N) {}   // the buffer, with room for N pixels

    void    load(const FileReader& picture); // reads a picture file into the buffer and returns at once; replaces one still on its way
    bool    loading() const;                 // gets whether the picture still loads, and starts a load that waits for the one before
    int16_t width() const { return width_; }     // gets the picture's width, 0 while it loads and when the file is missing, short or too large
    int16_t height() const { return height_; }   // gets the picture's height, 0 as for width()

private:
    friend class Canvas;

    // loading() starts and finishes a load, so a game's const done() can wait for it
    uint8_t*            pixels_;
    size_t              room_;
    mutable FileRequest sizeRead_;           // the read of entry 0, the width and the height
    mutable FileRequest pixelsRead_;         // and of the pixels into the buffer
    mutable uint8_t     size_[4];
    FileReader          next_;               // the picture that waits for the reads on their way
    mutable bool        waits_   = false;    // next_ waits
    mutable bool        landing_ = false;    // the reads have started, and loading() has not taken the size yet
    mutable int16_t     width_   = 0;
    mutable int16_t     height_  = 0;
};

// a layer of the picture as an Adafruit_GFX canvas of palette indices, index 0 transparent
class Canvas : public Adafruit_GFX {
public:
    Canvas(Display& display, Display::Layer layer)
        : Adafruit_GFX(kWidth, kHeight), display_(display), layer_(layer), pixels_(display.pixels(layer)) {}

    void drawPixel(int16_t x, int16_t y, uint16_t index) override;                  // sets one pixel's palette index
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t index) override;   // fills a rectangle row by row
    void fillScreen(uint16_t index) override;                                        // fills the whole layer
    void draw(const Picture& picture, int16_t x, int16_t y);                            // draws a picture with its top left corner at x, y; its transparent pixels leave the layer as it is
    void palette(uint8_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t opacity = 255);   // sets a color of the layer's palette, 255 opaque, and marks every row
    void show();                                                                     // hands the rows drawn since the last show() to the driver

    // reads a full-screen picture file into the layer with its colors and returns at once. The
    // screen's update() shows the picture once it has landed; a newer picture replaces one still
    // on its way
    void load(const FileReader& picture);

    // takes the palette of a picture file for the layer, without its pixels, and returns at once.
    // It loads like a picture, and replaces one still on its way
    void colors(const FileReader& picture);

    bool    loading() const;                                                         // gets whether a picture or a palette still loads
    uint8_t nearest(uint8_t r, uint8_t g, uint8_t b) const;                          // gets the index of the layer's opaque color closest to r, g, b, 0 when it has none

private:
    friend class Screen;

    static constexpr int16_t kWidth  = 320;
    static constexpr int16_t kHeight = 240;

    void drawn(int16_t top, int16_t bottom);   // widens the rows drawn since the last show()
    void clear();                              // sets the layer back to 0 and shows it, once a picture still on its way has landed
    void settle();                             // once the reads on their way have ended, starts what waits or shows the picture they brought

    Display&       display_;
    Display::Layer layer_;
    uint8_t*       pixels_;
    int16_t        top_    = kHeight;          // no row drawn yet
    int16_t        bottom_ = 0;
    FileRequest    pixelsRead_;                // the read of a picture's pixels into the layer
    FileRequest    colorsRead_;               // and of its palette into colors_
    uint8_t        colors_[kTable];           // the palette of the picture that loads, four bytes an entry
    FileReader     next_;                      // what waits for the reads on their way, a picture, a palette or a file with no name for a clear
    bool           waits_         = false;     // next_ waits
    bool           nextPixels_    = true;      // next_ brings its pixels, false for colors()
    bool           landing_       = false;     // a picture's reads have started, and update() has not shown it yet
    bool           landingPixels_ = true;      // the picture on its way brings its pixels
};

// the controls of one layer, from the UI control library; a stub until the library exists
class View {
public:
    bool modal = false;                          // takes every touch on its layer, as the host's menu does

    bool press(int16_t x, int16_t y);            // takes the touch when a control lies under the finger, or when modal
    void drag(int16_t x, int16_t y);             // the finger moved while this view keeps the touch
    void release(int16_t x, int16_t y);          // the finger left; a control under it calls its handler
    void update(Canvas& canvas);                 // redraws the controls that changed, then canvas.show() once
    void clear();                                // drops every control
};

// the picture: the game's layer and background, and the host's layer over them
class Screen : public Component {
public:
    Screen(Display& display, Touch& touch)
        : display_(display), touch_(touch), game_(display, Display::Layer::Middle),
          back_(display, Display::Layer::Back), host_(display, Display::Layer::Front) {}

    // takes every touch event; a touch sinks from the front layer to the back, and one that no
    // layer takes becomes ScreenTapped or ScreenHeld
    bool translate(const DriverEvent& in, PinballEvents& out) override;

    // shows a picture once it has landed, clears a layer once a picture still on its way has, and
    // redraws the controls that changed; the main loop calls it in every pass
    void update();

    Canvas& layer() { return game_; }        // gets the game's layer
    Canvas& background() { return back_; }   // gets the game's background, which a playing video covers inside its viewport
    void    showLayer(bool shown);           // shows or hides the game's layer; hidden, it keeps what is drawn
    View&   view() { return gameView_; }     // gets the controls on the game's layer

    Finger finger() const;                   // gets where a finger touches the screen

private:
    friend class GameHost;

    static constexpr uint32_t kTapMs = 500;  // the longest touch that counts as a tap
    static constexpr uint8_t  kSlop  = 10;   // pixels a tap may move

    enum class Gesture : uint8_t { None, Tap, Hold };

    Canvas& hostLayer() { return host_; }    // gets the host's layer
    View&   hostView() { return hostView_; } // gets the controls on the host's layer
    bool    settled() const { return !game_.loading() && !back_.loading(); }   // gets whether the game's layer and background neither load nor wait for a clear
    void    showHost(bool shown);            // shows or hides the host's layer
    Gesture takeGesture(uint16_t& x, uint16_t& y);   // gets the host the last gesture once, None when there is none
    void    reset();                         // clears the game's layer and background, and shows the game's layer
    View*   take(int16_t x, int16_t y);      // gets the view of the front-most shown layer that takes a touch, nullptr when every layer lets it pass
    void    gesture(Gesture g, const DriverEvent& in);   // keeps a gesture for the host and makes it the game's event
    static bool near(uint32_t a, uint32_t b, uint8_t slop);   // gets whether two positions lie within slop pixels on both axes

    Display&     display_;
    Touch&       touch_;
    Canvas       game_;
    Canvas       back_;
    Canvas       host_;
    View         hostView_;
    View         gameView_;
    View*        held_      = nullptr;       // the view that took the touch on the screen, until the finger leaves
    bool         hostShown_ = false;
    bool         gameShown_ = true;
    uint32_t     downAt_    = 0;             // nowUs() of the last TouchDown
    uint32_t     downXy_    = 0;             // and its position
    Gesture      gesture_   = Gesture::None; // the last gesture, until the host takes it
    uint32_t     gestureXy_ = 0;
    PinballEvent made_;                      // the event translate() points out at
};

void Canvas::drawPixel(int16_t x, int16_t y, uint16_t index) {
    if (x < 0 || y < 0 || x >= kWidth || y >= kHeight || loading()) return;   // a picture that loads keeps the layer
    pixels_[y * kWidth + x] = index;
    drawn(y, y + 1);
}

void Canvas::drawn(int16_t top, int16_t bottom) {
    top_    = min(top_, top);
    bottom_ = max(bottom_, bottom);
}

void Canvas::show() {
    if (top_ >= bottom_) return;             // nothing drawn
    display_.show(top_, bottom_);
    top_    = kHeight;
    bottom_ = 0;
}

void Canvas::palette(uint8_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t opacity) {
    if (index == 0) return;                  // 0 stays transparent
    display_.palette(layer_, index, (r & 0xF8) << 8 | (g & 0xFC) << 3 | b >> 3, opacity);
    drawn(0, kHeight);                       // any row may use the color
}

// weighs the channels as tools/picture.py does when it picks a picture's colors
uint8_t Canvas::nearest(uint8_t r, uint8_t g, uint8_t b) const {
    uint8_t  best    = 0;
    uint32_t closest = UINT32_MAX;
    for (int i = 1; i < 256; ++i) {
        if (!display_.opaque(layer_, i)) continue;
        const uint16_t c  = display_.color(layer_, i);
        const int      dr = (c >> 8 & 0xF8) - r;
        const int      dg = (c >> 3 & 0xFC) - g;
        const int      db = (c << 3 & 0xF8) - b;
        const uint32_t d  = 2 * dr * dr + 4 * dg * dg + 3 * db * db;
        if (d < closest) {
            closest = d;
            best    = i;
        }
    }
    return best;
}

void Canvas::draw(const Picture& picture, int16_t x, int16_t y) {
    if (picture.loading() || loading()) return;              // a picture that loads into the layer keeps it
    const int16_t w      = picture.width();
    const int16_t left   = max(0, -x), right  = min<int16_t>(w, kWidth - x);   // the part inside the layer
    const int16_t top    = max(0, -y), bottom = min<int16_t>(picture.height(), kHeight - y);
    if (left >= right || top >= bottom) return;
    for (int16_t r = top; r < bottom; ++r) {
        const uint8_t* from = picture.pixels_ + r * w;
        uint8_t*       to   = pixels_ + (y + r) * kWidth + x;
        for (int16_t c = left; c < right; ++c)
            if (from[c]) to[c] = from[c];                    // a transparent pixel leaves the layer as it is
    }
    drawn(y + top, y + bottom);
}

void Picture::load(const FileReader& picture) {
    next_  = picture;
    waits_ = true;
    loading();                               // starts the reads at once where none is on its way
}

bool Picture::loading() const {
    if (sizeRead_.status() == FileStatus::Pending || pixelsRead_.status() == FileStatus::Pending) return true;
    if (waits_) {                            // the reads before have ended, so the next start
        waits_   = false;
        landing_ = true;
        width_   = 0;
        height_  = 0;
        next_.read(0, size_, sizeof size_, sizeRead_);
        next_.read(kTable, pixels_, room_, pixelsRead_);
        return loading();
    }
    if (landing_) {
        landing_ = false;
        const uint16_t w = size_[0] << 8 | size_[1];
        const uint16_t h = size_[2] << 8 | size_[3];
        if (sizeRead_.bytes() == sizeof size_ && w > 0 && h > 0 && (size_t)w * h <= pixelsRead_.bytes()) {
            width_  = w;                     // a missing or short file and one too large for the buffer keep 0
            height_ = h;
        }
    }
    return false;
}
// fillRect() and fillScreen() write whole rows of the buffer with memset() and call drawn() once;
// while a picture loads, they do nothing

void Canvas::load(const FileReader& picture) {
    next_       = picture;
    nextPixels_ = true;
    waits_      = true;
    settle();                                // starts the reads at once where none is on its way
}

void Canvas::colors(const FileReader& picture) {
    next_       = picture;
    nextPixels_ = false;
    waits_      = true;
    settle();
}

void Canvas::clear() {
    load(FileReader{});                      // a file with no name stands for the clear
}

bool Canvas::loading() const {
    return waits_ || landing_;
}

// what waits replaces a picture that has landed, without showing it
void Canvas::settle() {
    if (pixelsRead_.status() == FileStatus::Pending || colorsRead_.status() == FileStatus::Pending) return;
    if (waits_) {
        waits_         = false;
        landing_       = next_.name()[0] != 0;
        landingPixels_ = nextPixels_;
        if (!landing_) {
            fillScreen(0);
            show();
            return;
        }
        next_.read(0, colors_, sizeof colors_, colorsRead_);
        if (landingPixels_) next_.read(sizeof colors_, pixels_, kWidth * kHeight, pixelsRead_);   // straight into the layer, which is aligned in RAM2
        return;
    }
    if (!landing_) return;
    landing_ = false;
    const bool read = colorsRead_.bytes() == sizeof colors_;
    if (!landingPixels_) {                   // colors() takes the palette of a picture of any size
        if (read) display_.palette(layer_, colors_);
        drawn(0, kHeight);                   // any row may use the colors
        show();
        return;
    }
    const bool whole = read && (colors_[0] << 8 | colors_[1]) == kWidth && (colors_[2] << 8 | colors_[3]) == kHeight;
    if (whole) display_.palette(layer_, colors_);
    const size_t kept = whole ? pixelsRead_.bytes() : 0;
    memset(pixels_ + kept, 0, kWidth * kHeight - kept);      // a missing, short or smaller file reads as empty
    drawn(0, kHeight);                       // the picture's rows
    show();
}

bool Screen::translate(const DriverEvent& in, PinballEvents& out) {
    out = {};
    const int16_t x = in.payload & 0xFFFF;
    const int16_t y = in.payload >> 16;
    switch (in.type) {
        case DriverEventType::TouchDown:
            held_   = take(x, y);                    // the touch sinks from the front layer to the back
            downAt_ = in.time;
            downXy_ = in.payload;
            return true;
        case DriverEventType::TouchMoved:
            if (held_) held_->drag(x, y);
            return true;
        case DriverEventType::TouchUp:
            if (held_) {                             // a layer took the touch and keeps it to the end
                held_->release(x, y);
                held_ = nullptr;
                return true;
            }
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

View* Screen::take(int16_t x, int16_t y) {
    if (hostShown_ && hostView_.press(x, y)) return &hostView_;   // a hidden layer lets the touch pass
    if (gameShown_ && gameView_.press(x, y)) return &gameView_;
    return nullptr;                                                  // the background has no controls
}

void Screen::gesture(Gesture g, const DriverEvent& in) {
    gesture_   = g;
    gestureXy_ = in.payload;
    made_ = {in.time, g == Gesture::Tap ? EventType::ScreenTapped : EventType::ScreenHeld, 0, in.payload};
}

Screen::Gesture Screen::takeGesture(uint16_t& x, uint16_t& y) {
    const Gesture g = gesture_;
    gesture_ = Gesture::None;
    x = gestureXy_ & 0xFFFF;
    y = gestureXy_ >> 16;
    return g;
}

Finger Screen::finger() const { return touch_.finger(); }

void Screen::showHost(bool shown) {
    hostShown_ = shown;
    display_.shown(Display::Layer::Front, shown);
}

void Screen::showLayer(bool shown) {
    gameShown_ = shown;
    display_.shown(Display::Layer::Middle, shown);
}

void Screen::update() {
    game_.settle();
    back_.settle();
    host_.settle();
    hostView_.update(host_);
    gameView_.update(game_);
}

void Screen::reset() {
    game_.clear();                           // transparent, so the next game starts on black
    back_.clear();
    gameView_.clear();                       // the next game starts without the stopped game's controls
    if (held_ == &gameView_) held_ = nullptr;
    showLayer(true);                         // a stopped game may have left its layer hidden
}
