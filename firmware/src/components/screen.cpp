#include "screen.h"
#include "drivers/touch.h"
#include "io/psram.h"
#include "logging/debug-log.h"
#include <Arduino.h>
#include <string.h>

constexpr uint32_t PIXEL_BYTES = 2;   // RGB565
constexpr size_t ROW_BYTES = size_t(Display::WIDTH) * PIXEL_BYTES;
constexpr size_t SCREEN_BYTES = ROW_BYTES * Display::HEIGHT;
constexpr uint32_t OPACITY_BYTES = 1;   // RGB565A8: one byte of opacity behind the colors
constexpr uint8_t BITS_PER_BYTE = 8;
constexpr uint32_t US_PER_MS = 1000;

// LVGL's two buffers of the whole screen, RGB565 high byte first as the driver sends it. LVGL draws
// into one while the driver's interrupt reads the other.
DMAMEM static uint8_t buffers[2][SCREEN_BYTES] __attribute__((aligned(PictureMemory::ALIGN_BYTES)));

Screen* Screen::animating = nullptr;

/// Gets the milliseconds since the start for LVGL's timers and animations.
static uint32_t tickMs() {
    return millis();
}

static uint16_t get16(const uint8_t* at) {
    return uint16_t(at[0] | (at[1] << BITS_PER_BYTE));
}

/// Gets the distance of two points along the axis on which they lie further apart.
static uint16_t distance(TouchPoint a, TouchPoint b) {
    const uint16_t dx = a.x > b.x ? a.x - b.x : b.x - a.x;
    const uint16_t dy = a.y > b.y ? a.y - b.y : b.y - a.y;
    return dx > dy ? dx : dy;
}

void Screen::begin() {
    if (begun) {
        return;
    }
    begun = true;
    animating = this;
    lv_init();
    lv_tick_set_cb(tickMs);
    lv = lv_display_create(Display::WIDTH, Display::HEIGHT);
    // In direct mode each buffer holds the whole screen, and LVGL copies what changed into the
    // other buffer itself, so any rows of a buffer show the screen as it is.
    lv_display_set_buffers(lv, buffers[0], buffers[1], sizeof buffers[0],
                           LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(lv, flush);
    lv_display_set_user_data(lv, this);

    // The background and the slides lie on LVGL's screen, which is transparent where they hold
    // nothing and shows the black bottom layer there. LVGL looks for the object that covers a
    // changed area on its screen alone and starts drawing at that object. A video that fills the
    // screen therefore costs no drawing of what lies behind it.
    lv_obj_t* bottom = lv_layer_bottom();
    lv_obj_set_style_bg_color(bottom, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(bottom, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_TRANSP, 0);
    back = lv_obj_create(lv_screen_active());
    deck = lv_obj_create(lv_screen_active());
    over = lv_obj_create(lv_screen_active());
    // LVGL draws its system layer last, in front of its popups on the top layer.
    host = lv_obj_create(lv_layer_sys());
    for (lv_obj_t* o : {back, deck, over, host}) {
        lv_obj_remove_style_all(o);
        lv_obj_set_size(o, Display::WIDTH, Display::HEIGHT);
        lv_obj_set_scrollable(o, false);
    }
    for (lv_obj_t* o : {lv_screen_active(), bottom, back, deck, over, host}) {
        // A touch on no object passes through to the game.
        lv_obj_set_clickable(o, false);
    }

    // LVGL reads the touch only when translate() hands it an event, and between the events while
    // the finger is down, for its long press.
    pointer = lv_indev_create();
    lv_indev_set_type(pointer, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(pointer, readTouch);
    lv_indev_set_user_data(pointer, this);
    lv_indev_set_display(pointer, lv);
    lv_indev_set_mode(pointer, LV_INDEV_MODE_EVENT);

    size_t bytes = 0;
    uint8_t* memory = io::takeFreePsram(bytes);
    pictures.begin(memory, bytes);
    if (memory == nullptr) {
        DEBUG_LOG("screen: no PSRAM, so every picture fails to load\n");
    }
}

// LVGL hands every changed area of a refresh apart, and the last one with
// lv_display_flush_is_last(). The areas before it are only noted, so the driver gets one rectangle
// per refresh and LVGL never waits for a rectangle of the same refresh. The rectangle spans whole
// rows from the first changed row to the last, which lie in one piece in the buffer and on the
// driver's 16-pixel steps.
void Screen::flush(lv_display_t* d, const lv_area_t* area, uint8_t* pixels) {
    Screen* self = static_cast<Screen*>(lv_display_get_user_data(d));
    if (self->lastRow < self->firstRow) {
        self->firstRow = area->y1;
        self->lastRow = area->y2;
    } else {
        self->firstRow = area->y1 < self->firstRow ? area->y1 : self->firstRow;
        self->lastRow = area->y2 > self->lastRow ? area->y2 : self->lastRow;
    }
    if (!lv_display_flush_is_last(d)) {
        lv_display_flush_ready(d);
        return;
    }
    const int32_t top = self->firstRow;
    const int32_t rows = self->lastRow - self->firstRow + 1;
    self->lastRow = -1;
    const uint8_t* from = pixels + size_t(top) * ROW_BYTES;
    // A refused rectangle still lets LVGL go on, so it costs a stale piece of the screen rather
    // than a main loop that waits for ever.
    if (!self->display.send(0, uint16_t(top), Display::WIDTH, uint16_t(rows), from,
                            [d] { lv_display_flush_ready(d); })) {
        DEBUG_LOG("screen: the display refused rows %ld to %ld\n", long(top), long(top + rows - 1));
        lv_display_flush_ready(d);
    }
}

void Screen::render() {
    for (Picture* p = first; p != nullptr; p = p->later) {
        p->settle();
    }
    if (!begun || display.restarting()) {
        return;
    }
    if (redraw) {
        // The panel lost its picture with its setup. The bottom layer spans the screen and is never
        // hidden, so LVGL draws every layer over its area again.
        lv_obj_invalidate(lv_layer_bottom());
        redraw = false;
    }
    lv_timer_handler();
}

void Screen::readTouch(lv_indev_t* indev, lv_indev_data_t* data) {
    const Screen* self = static_cast<const Screen*>(lv_indev_get_user_data(indev));
    data->point.x = self->finger.x;
    data->point.y = self->finger.y;
    data->state = self->pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

// LVGL searches the layers in this order and finds only an object that takes touches. The screen's
// own objects take none.
lv_obj_t* Screen::controlAt(TouchPoint at) const {
    if (!begun) {
        return nullptr;
    }
    lv_point_t p = {at.x, at.y};
    for (lv_obj_t* layer : {lv_layer_sys(), lv_layer_top(), lv_screen_active()}) {
        if (lv_obj_t* found = lv_indev_search_obj(layer, &p)) {
            return found;
        }
    }
    return nullptr;
}

bool Screen::translate(const DriverEvent& in, PinballEvents& out) {
    const bool touch =
        in.type == DriverEventType::TOUCH_DOWN || in.type == DriverEventType::TOUCH_MOVED ||
        in.type == DriverEventType::TOUCH_HELD || in.type == DriverEventType::TOUCH_UP;
    if (!touch) {
        return false;
    }
    out = {};
    // The four touch events carry the same payload.
    const TouchPoint at = payload<DriverEventType::TOUCH_DOWN>(in);
    if (in.type == DriverEventType::TOUCH_DOWN) {
        // Only the point where the finger comes down decides, so a finger that slides onto a
        // control afterwards does not press it.
        handed = controlAt(at) != nullptr;
        downAt = at;
        downUs = in.time;
        strayed = false;
        held = false;
    }
    if (handed) {
        // LVGL holds a long press by itself and needs no TOUCH_HELD.
        if (in.type != DriverEventType::TOUCH_HELD) {
            finger = at;
            pressed = in.type != DriverEventType::TOUCH_UP;
            lv_indev_read(pointer);
        }
        handed = in.type != DriverEventType::TOUCH_UP;
        return true;
    }
    if (distance(at, downAt) >= Touch::SLOP_PX) {
        strayed = true;
    }
    if (in.type == DriverEventType::TOUCH_HELD && !strayed && !held) {
        held = true;
        makeGesture(Gesture::HOLD, in.time, out);
    } else if (in.type == DriverEventType::TOUCH_UP && !strayed && !held &&
               in.time - downUs <= TAP_MS * US_PER_MS) {
        makeGesture(Gesture::TAP, in.time, out);
    }
    return true;
}

void Screen::makeGesture(Gesture g, uint32_t time, PinballEvents& out) {
    uint32_t packed = 0;
    memcpy(&packed, &downAt, sizeof downAt);
    const EventType type = g == Gesture::TAP ? EventType::SCREEN_TAPPED : EventType::SCREEN_HELD;
    gesture = {time, type, 0, packed};
    out = {&gesture, 1};
    lastGesture = g;
    lastAt = downAt;
}

Screen::Gesture Screen::takeGesture(TouchPoint& at) {
    const Gesture g = lastGesture;
    at = lastAt;
    lastGesture = Gesture::NONE;
    return g;
}

void Screen::showOverlay(bool shown) {
    if (over == nullptr) {
        DEBUG_LOG("Screen::showOverlay: no begin() yet\n");
        return;
    }
    lv_obj_set_hidden(over, !shown);
}

void Screen::showHost(bool shown) {
    if (host == nullptr) {
        DEBUG_LOG("Screen::showHost: no begin() yet\n");
        return;
    }
    lv_obj_set_hidden(host, !shown);
}

void Screen::restart() {
    if (!begun) {
        DEBUG_LOG("Screen::restart: no begin() yet\n");
        return;
    }
    display.restart();
    redraw = true;
}

lv_obj_t* Screen::slide(Layer layer) {
    if (Slide* s = slides.find([&](const Slide& s) { return s.key == layer.key; })) {
        return s->box;
    }
    if (slides.full()) {
        DEBUG_LOG("screen: a slide for key %u refused, %u modes hold one\n", layer.key,
                  unsigned(SLIDES));
        return nullptr;
    }
    lv_obj_t* box = lv_obj_create(deck);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, Display::WIDTH, Display::HEIGHT);
    lv_obj_set_scrollable(box, false);
    lv_obj_set_clickable(box, false);
    slides.insert(Slide{layer.key, layer.priority, box});
    restack();
    return box;
}

void Screen::remove(Layer layer) {
    Slide* s = slides.find([&](const Slide& s) { return s.key == layer.key; });
    if (s == nullptr) {
        return;
    }
    // Deleting the slide deletes its objects. A picture's image leaves the picture's list, and the
    // video player's image ends its video.
    lv_obj_t* box = s->box;
    slides.removeIf([&](const Slide& s) { return s.key == layer.key; });
    lv_obj_delete(box);
    restack();
}

// LVGL draws no hidden object and finds none under a finger, so a covered slide neither shows nor
// takes a touch. Its objects and pictures stay.
void Screen::restack() {
    for (size_t i = 0; i < slides.size(); ++i) {
        if (i == 0) {
            lv_obj_set_hidden(slides[i].box, false);
        } else {
            lv_obj_set_hidden(slides[i].box, true);
        }
    }
    pauseCovered();
}

lv_anim_t* Screen::animate(const lv_anim_t& anim) {
    Animation* free = nullptr;
    for (Animation& a : animations) {
        if (a.running == nullptr) {
            free = &a;
            break;
        }
    }
    if (free == nullptr) {
        DEBUG_LOG("screen: an animation refused, %u run already\n", unsigned(ANIMATIONS));
        return nullptr;
    }
    // The screen takes the function for the animation's deletion, so it hears when LVGL deletes
    // the animation, and calls the starter's function from there.
    lv_anim_t copy = anim;
    lv_anim_set_deleted_cb(&copy, forgetAnimation);
    lv_anim_t* running = lv_anim_start(&copy);
    if (running == nullptr) {
        DEBUG_LOG("screen: LVGL refused an animation\n");
        return nullptr;
    }
    free->running = running;
    free->deleted = anim.deleted_cb;
    free->paused = false;
    pauseCovered();
    return running;
}

void Screen::forgetAnimation(lv_anim_t* a) {
    if (animating == nullptr) {
        return;
    }
    for (Animation& entry : animating->animations) {
        if (entry.running == a) {
            const lv_anim_deleted_cb_t deleted = entry.deleted;
            entry = {};
            if (deleted != nullptr) {
                deleted(a);
            }
            return;
        }
    }
}

void Screen::pauseCovered() {
    const lv_obj_t* shown = top();
    for (Animation& a : animations) {
        if (a.running == nullptr) {
            continue;
        }
        lv_obj_t* obj = static_cast<lv_obj_t*>(a.running->var);
        lv_obj_t* on = lv_obj_is_valid(obj) ? slideOf(obj) : nullptr;
        if (on != nullptr && on != shown && !a.paused) {
            lv_anim_pause(a.running);
            a.paused = true;
        } else if ((on == nullptr || on == shown) && a.paused) {
            lv_anim_resume(a.running);
            a.paused = false;
        }
    }
}

lv_obj_t* Screen::slideOf(lv_obj_t* obj) const {
    while (obj != nullptr && lv_obj_get_parent(obj) != deck) {
        obj = lv_obj_get_parent(obj);
    }
    return obj;
}

uint8_t Screen::keyOf(const lv_obj_t* slide) const {
    for (size_t i = 0; i < slides.size(); ++i) {
        if (slides[i].box == slide) {
            return slides[i].key;
        }
    }
    return 0;
}

// A game's Picture lives as long as the firmware, so it outlives its game and is released here.
void Screen::reset() {
    slides.clear();
    lv_obj_clean(deck);
    lv_obj_clean(back);
    lv_obj_clean(over);
    lv_obj_set_hidden(over, false);
    for (Picture* p = first; p != nullptr; p = p->later) {
        p->release();
    }
}

/// Sets a loaded picture into an image.
static void fillImage(lv_obj_t* shown, const lv_image_dsc_t& picture) {
    lv_image_set_src(shown, &picture);
    // LVGL takes an image as covering what lies behind it only where the image has a background
    // that covers. A picture that holds no opacity gets a black one, so LVGL draws nothing behind
    // it. A picture with opacity gets none and shows what lies behind it.
    if (!lv_color_format_has_alpha(lv_color_format_t(picture.header.cf))) {
        lv_obj_set_style_bg_color(shown, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(shown, LV_OPA_COVER, 0);
    }
}

void Picture::load(const FileReader& picture) {
    if (!known) {
        // Not in the constructor: a game's Picture may be built before the screen.
        later = screen.first;
        screen.first = this;
        known = true;
    }
    release();
    failedLoad = false;
    if (read.status() == FileStatus::PENDING) {
        next = picture;
        queued = true;
        return;
    }
    file = picture;
    start();
}

void Picture::start() {
    step = Step::HEADER;
    released = false;
    file.read(0, header, sizeof header, read);
}

lv_obj_t* Picture::show(lv_obj_t* parent) {
    lv_obj_t* shown = lv_image_create(parent);
    lv_obj_set_user_data(shown, images);
    images = shown;
    lv_obj_add_event_cb(shown, forget, LV_EVENT_DELETE, this);
    if (step == Step::LOADED) {
        fillImage(shown, image);
    }
    return shown;
}

void Picture::forget(lv_event_t* e) {
    Picture* self = static_cast<Picture*>(lv_event_get_user_data(e));
    lv_obj_t* gone = static_cast<lv_obj_t*>(lv_event_get_target(e));
    lv_obj_t* after = static_cast<lv_obj_t*>(lv_obj_get_user_data(gone));
    if (self->images == gone) {
        self->images = after;
        return;
    }
    for (lv_obj_t* o = self->images; o != nullptr;) {
        lv_obj_t* n = static_cast<lv_obj_t*>(lv_obj_get_user_data(o));
        if (n == gone) {
            lv_obj_set_user_data(o, after);
            return;
        }
        o = n;
    }
}

void Picture::release() {
    // Each deletion takes the image off the list through forget().
    while (images != nullptr) {
        lv_obj_delete(images);
    }
    queued = false;
    if (read.status() == FileStatus::PENDING) {
        released = true;
        return;
    }
    if (room != nullptr) {
        screen.pictures.give(room);
    }
    room = nullptr;
    image = {};
    step = Step::IDLE;
}

bool Picture::loading() const {
    return queued || step == Step::HEADER || step == Step::PIXELS;
}

void Picture::fill() {
    for (lv_obj_t* o = images; o != nullptr; o = static_cast<lv_obj_t*>(lv_obj_get_user_data(o))) {
        fillImage(o, image);
    }
}

void Picture::settle() {
    if (read.status() == FileStatus::PENDING || step == Step::IDLE || step == Step::LOADED) {
        return;
    }
    const bool atHeader = step == Step::HEADER;
    if (released) {
        if (room != nullptr) {
            screen.pictures.give(room);
        }
        room = nullptr;
        image = {};
        step = Step::IDLE;
        released = false;
    } else if (atHeader) {
        const uint8_t format = header[1];
        const uint16_t width = get16(header + 4);
        const uint16_t height = get16(header + 6);
        const uint16_t stride = get16(header + 8);
        const bool opaque = format == LV_COLOR_FORMAT_RGB565_SWAPPED;
        const bool shown = opaque || format == LV_COLOR_FORMAT_RGB565A8;
        if (read.status() != FileStatus::SUCCEEDED || read.bytes() < HEADER_BYTES ||
            header[0] != LV_IMAGE_HEADER_MAGIC || !shown || width == 0 || width > Display::WIDTH ||
            height == 0 || height > Display::HEIGHT || stride != width * PIXEL_BYTES) {
            DEBUG_LOG("screen: %s holds no picture the screen shows\n", file.name());
            failedLoad = true;
            step = Step::IDLE;
        } else {
            const size_t pixels =
                size_t(stride) * height + (opaque ? 0 : size_t(width) * height * OPACITY_BYTES);
            fileBytes = HEADER_BYTES + pixels;
            const bool full = width == Display::WIDTH && height == Display::HEIGHT;
            room = screen.pictures.take(fileBytes, full);
            if (room == nullptr) {
                DEBUG_LOG("screen: the picture memory has no room for %s\n", file.name());
                failedLoad = true;
                step = Step::IDLE;
            } else {
                image.header.magic = LV_IMAGE_HEADER_MAGIC;
                image.header.cf = format;
                image.header.w = width;
                image.header.h = height;
                image.header.stride = stride;
                image.data_size = uint32_t(pixels);
                image.data = room + HEADER_BYTES;
                // The whole file from its first byte, rounded up to the room's 32 bytes, so the
                // storage driver reads it straight into the room.
                step = Step::PIXELS;
                const size_t n = (fileBytes + PictureMemory::ALIGN_BYTES - 1) /
                                 PictureMemory::ALIGN_BYTES * PictureMemory::ALIGN_BYTES;
                file.read(0, room, n, read);
            }
        }
    } else if (read.status() != FileStatus::SUCCEEDED || read.bytes() < fileBytes) {
        DEBUG_LOG("screen: %s is short\n", file.name());
        screen.pictures.give(room);
        room = nullptr;
        image = {};
        failedLoad = true;
        step = Step::IDLE;
    } else {
        step = Step::LOADED;
        fill();
    }
    if (queued && read.status() != FileStatus::PENDING) {
        queued = false;
        file = next;
        start();
    }
}
