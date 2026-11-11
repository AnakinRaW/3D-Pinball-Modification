// the videos of the running game on its slides, with their sound over the music;
// every call returns at once
class VideoPlayer : public Component {
public:
    VideoPlayer(Screen& screen, FileSystem& files, Sounds& sounds)
        : screen_(screen), files_(files), sounds_(sounds) {}

    void begin();           // attaches its timer to LVGL, after the screen's begin()

    // hands the game the MovieEnded that step() found, once
    bool update(uint32_t now, PinballEvents& out) override;

    // plays video/<name>.rgb in viewport, an LVGL object on the slide that shows, at the viewport's
    // size and place, over and over with loop, with sound audio/<name>.wav over the music;
    // false and nothing played for a viewport on a hidden slide, on no slide or beyond the screen,
    // or without room for its frame in the picture memory
    bool play(lv_obj_t* viewport, const char* name, bool loop, bool sound);

    void stop(const lv_obj_t* viewport);            // stops the video in viewport and its sound; the paused music resumes
    bool playing(const lv_obj_t* viewport) const { return room_ && viewport_ == viewport; }   // gets whether a video plays in viewport, neither ended nor stopped

private:
    friend class GameHost;

    static constexpr uint32_t kFrameUs      = 40000;   // 25 frames a second, docs/parts/display
    static constexpr uint32_t kFrameSamples = 1764;    // the samples of one frame, 44.1 kHz over 25 frames a second

    void reset() { if (room_) end(); endedDue_ = false; }   // stops the video of the stopped game, whose end reaches no game; the host calls it when it switches games

    // draws a frame once it is due and reads the next, and keeps MovieEnded for update() once the
    // video last started has run out; LVGL's timer calls it in every pass of the main loop
    void step(uint32_t now);
    uint32_t due(uint32_t now);   // gets the frame due now, from how far the sound has played or, without one or once it has run out, from the clock
    static void tick(lv_timer_t* t) { static_cast<VideoPlayer*>(lv_timer_get_user_data(t))->step(nowUs()); }
    static void gone(lv_event_t* e);   // ends the video once its image is deleted with its viewport or its slide
    void end();                // ends the sound and deletes the image; gives the room back at once, or once a read on its way has landed in it

    Screen&        screen_;
    FileSystem&    files_;
    Sounds&        sounds_;
    FileReader     file_;
    FileRequest    reads_[2];            // the next frame, read whole in one request; the other may still land in a stopped video's room
    uint8_t        which_   = 0;         // the request of the video playing
    uint8_t*       leaving_ = nullptr;   // a stopped video's room, given back once its read has landed
    uint8_t*       room_    = nullptr;   // one frame in the screen's picture memory
    lv_image_dsc_t frame_   = {};        // RGB565 high byte first, LVGL's RGB565_SWAPPED, so LVGL copies it as it is
    lv_obj_t*      image_   = nullptr;   // inside the viewport, filling it
    lv_obj_t*      viewport_ = nullptr;  // the game's object the video plays in
    lv_obj_t*      slide_   = nullptr;   // the slide the viewport lies on, which owns the video
    uint32_t       startAt_ = 0;         // nowUs() as the first frame was due
    uint32_t       frames_  = 0;         // the frames in the file
    uint32_t       next_    = 0;         // the frame in the room, or on its way into it
    bool           landed_  = false;     // the frame in the room has landed and waits to be drawn
    bool           loop_    = false;
    bool           sound_   = false;
    bool           endedDue_ = false;    // step() found the end, and update() has not handed it out yet
    PinballEvent   ended_;               // the MovieEnded that update() points out at
};

void VideoPlayer::begin() {
    lv_timer_create(tick, 0, this);          // a period of 0: every lv_timer_handler() runs it, inside the screen's render()
}

// only the slide that shows plays a video, so a video playing belongs to the same slide and is
// replaced; a hidden slide gets false and leaves it playing
bool VideoPlayer::play(lv_obj_t* viewport, const char* name, bool loop, bool sound) {
    lv_obj_t* slide = screen_.slideOf(viewport);
    if (!slide || slide != screen_.top()) return false;
    lv_obj_update_layout(viewport);                       // its place and size as the game has set them
    lv_area_t a;
    lv_obj_get_content_coords(viewport, &a);
    if (a.x2 < a.x1 || a.y2 < a.y1 || a.x1 < 0 || a.y1 < 0 || a.x2 >= 320 || a.y2 >= 240) return false;   // empty, or beyond the screen
    const uint16_t width = lv_area_get_width(&a), height = lv_area_get_height(&a);
    if (leaving_ && room_ && reads_[which_].status() == FileStatus::Pending) return false;   // both requests still land in stopped videos' rooms, for a few ms at most
    if (room_) end();                                     // the room of the video it replaces is free again, unless a read still lands in it
    uint8_t* room = screen_.pictures_.take((size_t)width * height * 2, width == 320 && height == 240);
    if (!room) return false;                              // the picture memory is full
    room_  = room;
    frame_ = {};
    frame_.header = {LV_IMAGE_HEADER_MAGIC, LV_COLOR_FORMAT_RGB565_SWAPPED, 0, width, height, (uint16_t)(width * 2), 0};
    frame_.data_size = (uint32_t)width * height * 2;
    frame_.data = room_;
    lv_timer_pause(lv_display_get_refr_timer(lv_display_get_default()));   // the frames refresh the screen meanwhile
    image_ = lv_image_create(viewport);                   // at the viewport's top left, inside its padding
    lv_image_set_src(image_, &frame_);
    lv_obj_add_event_cb(image_, gone, LV_EVENT_DELETE, this);
    viewport_ = viewport;
    slide_    = slide;
    // ... file_ = files_.open("video/<name>.rgb"), frames_ from its size; the read of frame 0
    next_    = 0;
    landed_  = false;
    loop_    = loop;
    startAt_ = nowUs();
    char wav[Storage::kPath];
    snprintf(wav, sizeof wav, "%s.wav", name);
    sound_ = sound;
    if (sound) sounds_.videoSound(wav, loop);            // over every track of the game, which is paused meanwhile
    return true;
}

// a video with sound follows its sound, so a sound that stalls holds the video with it; one whose
// sound has run out, or one without, follows the clock from the frame it stands at
uint32_t VideoPlayer::due(uint32_t now) {
    if (sound_ && sounds_.videoSounding()) {
        const uint32_t frame = sounds_.videoPlayed() / kFrameSamples;
        startAt_ = now - frame * kFrameUs;               // where the clock takes over once the sound has run out
        return frame;
    }
    return (now - startAt_) / kFrameUs;
}

// the frame due follows the clock due() takes, so a late frame is skipped rather than slowing the
// video
void VideoPlayer::step(uint32_t now) {
    if (leaving_ && reads_[which_ ^ 1].status() != FileStatus::Pending) {   // the stopped video's read has landed
        screen_.pictures_.give(leaving_);
        leaving_ = nullptr;
    }
    if (room_ && screen_.top() != slide_) { end(); return; }   // another slide covers the video's own
    const FileRequest& read_ = reads_[which_];
    if (!room_ || read_.status() == FileStatus::Pending) return;
    if (!landed_) {
        if (read_.status() == FileStatus::Failed || read_.bytes() < frame_.data_size) {   // the file has run out or is missing
            if (loop_ && frames_ > 0) { /* ... the read of frame 0 again, next_ = 0 and startAt_ = now; a sound that loops starts over with it */ return; }
            ended_    = {now, EventType::MovieEnded, screen_.keyOf(slide_), 0};   // the key of the video's slide
            endedDue_ = true;
            end();
            return;
        }
        landed_ = true;
    }
    if (due(now) < next_) return;                         // not due yet
    lv_obj_invalidate(image_);
    lv_refr_now(nullptr);                                 // draws the screen now, with every other change; the room is free again once this returns
    landed_ = false;
    next_   = due(now) + 1;                               // a late frame skips the ones whose time has passed
    // ... the read of frame next_ into the room, whole, at next_ * frame_.data_size
}

bool VideoPlayer::update(uint32_t now, PinballEvents& out) {
    out = {};
    if (!endedDue_) return false;
    endedDue_ = false;
    out       = {&ended_, 1};
    return true;
}

void VideoPlayer::stop(const lv_obj_t* viewport) {
    if (playing(viewport)) end();
}

// LVGL deletes the image itself when the game deletes the viewport or removes the slide
void VideoPlayer::gone(lv_event_t* e) {
    VideoPlayer* self = static_cast<VideoPlayer*>(lv_event_get_user_data(e));
    if (self->image_ != lv_event_get_target(e)) return;  // end() deletes it itself and has let go of it
    self->image_ = nullptr;
    self->end();
}

void VideoPlayer::end() {
    if (sound_) sounds_.stopVideoSound();                // the track it covered goes on
    sound_ = false;
    lv_obj_t* image = image_;
    image_ = nullptr;                                     // so gone() leaves the image end() deletes alone
    if (image) lv_obj_delete(image);
    viewport_ = nullptr;
    slide_    = nullptr;
    lv_timer_resume(lv_display_get_refr_timer(lv_display_get_default()));   // LVGL refreshes on its own again
    if (reads_[which_].status() == FileStatus::Pending) {   // step() gives the room back once the read has landed
        leaving_ = room_;
        which_  ^= 1;
    } else {
        screen_.pictures_.give(room_);
    }
    room_ = nullptr;
}
