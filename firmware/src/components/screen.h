// Runs LVGL, the library for user interfaces, on the display. A game places LVGL's objects, such as
// labels, images and buttons, on the slides of its modes and on its background, and LVGL draws the
// parts of the screen that changed into one of two buffers while the display driver sends the
// other. The slide of the mode with the highest priority shows. The game's pictures load from the
// card into the picture memory in the PSRAM without the game waiting for the card.
//
// Design: docs/firmware/components/screen.md.
#pragma once
#include "components/picture-memory.h"
#include "core/priority-list.h"
#include "drivers/display.h"
#include "drivers/storage-service.h"
#include "game/layer.h"
#include <lvgl.h>

class Screen;

/// A picture file in the screen's picture memory, shown in as many LVGL images as the game likes.
///
/// A picture file is in LVGL's image format: a header of 12 bytes, then the pixels. The pixels are
/// RGB565 high byte first, or RGB565 low byte first followed by an opacity byte for each pixel.
class Picture {
public:
    /// Creates a picture of a screen that holds no file yet.
    explicit Picture(Screen& screen) : screen(screen) {}

    Picture(const Picture&) = delete;
    Picture& operator=(const Picture&) = delete;

    /// Reserves memory for a picture file, starts reading the file into it and returns at once.
    ///
    /// It releases the picture it held first. A load while the picture still loads starts once
    /// that read has landed.
    ///
    /// @param file  The picture file, such as one the file system opened in the game's directory.
    /// @note        The main loop calls it, after the screen's begin().
    void load(const FileReader& file);

    /// Creates an image of the picture inside @p parent, to place like any LVGL object.
    ///
    /// The image is empty until the picture has loaded and shows it from then on. The image of a
    /// picture that holds no opacity covers what lies behind it. Where that picture leaves the
    /// image empty, the image is black. The picture keeps its images in a list through their LVGL
    /// user data, which the game leaves alone.
    ///
    /// @param parent  The LVGL object the image lies in.
    /// @return        The image.
    lv_obj_t* show(lv_obj_t* parent);

    /// Deletes every image of the picture and frees its memory.
    ///
    /// A picture that still loads frees its memory once the read has landed.
    void release();

    /// Gets whether the picture still loads.
    bool loading() const;

    /// Gets whether the last load found no file, a short file, a file in no format the screen
    /// shows, or a full picture memory.
    bool failed() const { return failedLoad; }

private:
    friend class Screen;

    /// Where a load stands.
    enum class Step : uint8_t { IDLE, HEADER, PIXELS, LOADED };

    static constexpr size_t HEADER_BYTES = 12;   // LVGL's image header

    /// Takes an image the game deleted off the picture's list.
    static void forget(lv_event_t* e);

    /// Goes on with a load once its read has landed.
    ///
    /// @note  The screen's render() calls it in every pass.
    void settle();

    /// Starts reading the header of the file the picture loads next.
    void start();

    /// Sets the pixels into every image of the picture.
    void fill();

    Screen& screen;
    FileReader file;   // the file being loaded
    FileReader next;   // the file a load() during a read asked for
    FileRequest read;
    uint8_t header[HEADER_BYTES] = {};
    lv_image_dsc_t image = {};    // what every image of the picture shows once it has loaded
    uint8_t* room = nullptr;      // the header and the pixels in the picture memory
    size_t fileBytes = 0;         // the header and the pixels the file holds
    lv_obj_t* images = nullptr;   // the first image; each image's user data points at the next
    Step step = Step::IDLE;
    bool failedLoad = false;
    bool released = false;      // release() came while a read was on its way
    bool queued = false;        // load() came while a read was on its way, with next
    bool known = false;         // the picture stands in the screen's list
    Picture* later = nullptr;   // the next picture in the screen's list
};

/// LVGL on the display: the slides of the game's modes in front of the game's background.
class Screen {
public:
    static constexpr size_t SLIDES = 8;        // one for each mode the mode stack runs at once
    static constexpr size_t ANIMATIONS = 32;   // the animations animate() follows at once

    /// Creates the screen on a display driver.
    explicit Screen(Display& display) : display(display) {}

    Screen(const Screen&) = delete;
    Screen& operator=(const Screen&) = delete;

    /// Starts LVGL on the display driver and takes the free PSRAM as the picture memory.
    ///
    /// A second call does nothing.
    ///
    /// @note  The main task calls it at the start, after the display's begin().
    void begin();

    /// Runs LVGL: its timers, the handlers of its controls and the redraw.
    ///
    /// It goes on with every picture that loads first. LVGL rests while the display restarts.
    ///
    /// @note  The main loop calls it in every pass. It never waits for the display.
    void render();

    /// Gets the subject that tells LVGL objects of a change in the firmware's warnings.
    ///
    /// Its value is warningList's version() as render() saw it last. render() sets it when the
    /// version has changed, and LVGL then calls every observer of the subject. An observer reads
    /// the entries from warningList.
    ///
    /// @note  Valid from begin() on. The main task only.
    lv_subject_t* warnings() { return warningSubject; }

    /// Gets the game's background, the parent of what lies behind every slide.
    lv_obj_t* background() { return back; }

    /// Gets the slide of a mode, the parent of its objects.
    ///
    /// A second call for the same mode gets the same slide. A new slide lies at the mode's
    /// priority, above the slides of equal priority. Only the slide on top shows.
    ///
    /// @param layer  The mode's key and priority.
    /// @return       The slide, nullptr when SLIDES modes hold one already.
    lv_obj_t* slide(Layer layer);

    /// Deletes the slide of a mode with its objects, so the slide below shows.
    ///
    /// @param layer  The mode's key and priority. A mode without a slide is left alone.
    void remove(Layer layer);

    /// Starts an LVGL animation that stands still while its object's slide is covered.
    ///
    /// The animation pauses when another slide shows in place of its slide and goes on where it
    /// stood once its slide shows again. One started on a covered slide waits until its slide
    /// shows. An animation of an object on no slide runs as LVGL runs it.
    ///
    /// @param anim  The animation, set up as for LVGL's lv_anim_start(). Its variable is an LVGL
    ///              object. A function it names for its deletion still runs.
    /// @return      The running animation, nullptr when LVGL refused it or ANIMATIONS are followed
    ///              already.
    lv_anim_t* animate(const lv_anim_t& anim);

private:
    friend class GameHost;
    friend class Picture;
    friend class VideoPlayer;

    /// One mode's slide: a full-screen object in the deck, under the mode's key.
    struct Slide {
        uint8_t key = 0;
        uint8_t priority = 0;
        lv_obj_t* box = nullptr;
        friend uint8_t priorityOf(const Slide& s) { return s.priority; }
    };

    /// Gets whether the picture memory is empty, with every read of a picture or a frame landed.
    bool settled() const { return pictures.empty(); }

    /// Deletes the game's slides and the objects on its background, and releases its pictures.
    void reset();

    /// Sets the display up again and sends the whole screen once the display is back.
    ///
    /// LVGL rests until then. Before begin() it does nothing.
    ///
    /// @note  Returns at once. The main loop calls it once the display module has its power back.
    void restart();

    /// Gets the slide @p obj lies on, nullptr for an object on no slide.
    lv_obj_t* slideOf(lv_obj_t* obj) const;

    /// Gets the slide that shows, nullptr when there is none.
    lv_obj_t* top() const { return slides.size() != 0 ? slides[0].box : nullptr; }

    /// Gets the key of the mode that holds a slide, 0 for no slide.
    uint8_t keyOf(const lv_obj_t* slide) const;

    /// Shows the slide on top and hides every other.
    void restack();

    /// Hands a part of the rendered screen to the display driver.
    static void flush(lv_display_t* d, const lv_area_t* area, uint8_t* pixels);

    /// Takes an animation off the list once LVGL deletes it, and runs the game's function for it.
    static void forgetAnimation(lv_anim_t* a);

    /// Pauses every animation on a covered slide and lets those on the slide that shows go on.
    void pauseCovered();

    /// An animation animate() started, with the function its starter named for its deletion.
    struct Animation {
        lv_anim_t* running = nullptr;
        lv_anim_deleted_cb_t deleted = nullptr;
        bool paused = false;   // the screen paused it, so the screen resumes it
    };

    Display& display;
    PictureMemory pictures;
    Picture* first = nullptr;   // every picture that has loaded once
    lv_display_t* lv = nullptr;
    lv_obj_t* back = nullptr;             // a full-screen object on LVGL's screen
    lv_obj_t* deck = nullptr;             // the same, created after back, so it lies in front of it
    PriorityList<Slide, SLIDES> slides;   // the game's slides, the one that shows first
    int32_t firstRow = 0;   // the rows that changed in this refresh, from firstRow to lastRow
    int32_t lastRow = -1;
    Animation animations[ANIMATIONS];
    bool begun = false;
    bool redraw = false;                      // the whole screen goes out once the display is back
    lv_subject_t* warningSubject = nullptr;   // LVGL's subject of the warnings
    uint32_t warningsSeen = 0;                // warningList's version() as render() saw it last
    static Screen* animating;                 // the screen whose animations LVGL reports deleted
};
