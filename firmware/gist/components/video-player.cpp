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
    // or without room for its two frames in the picture memory
    bool play(lv_obj_t* viewport, const char* name, bool loop, bool sound);

    void stop(const lv_obj_t* viewport);            // stops the video in viewport and its sound; the paused music resumes
    bool playing(const lv_obj_t* viewport) const { return rooms_[0] && viewport_ == viewport; }   // gets whether a video plays in viewport, neither ended nor stopped

private:
    friend class GameHost;

    static constexpr uint32_t kFrameUs      = 40000;   // 25 frames a second, docs/parts/display
    static constexpr uint32_t kFrameSamples = 1764;    // the samples of one frame, 44.1 kHz over 25 frames a second

    void reset() { if (rooms_[0]) end(); endedDue_ = false; }   // stops the video of the stopped game, whose end reaches no game; the host calls it when it switches games

    // draws a frame once it is due and reads the next, and keeps MovieEnded for update() once the
    // video last started has run out; LVGL's timer calls it in every pass of the main loop
    void step(uint32_t now);
    uint32_t due(uint32_t now);   // gets the frame due now, from how far the sound has played or, without one or once it has run out, from the clock
    static void tick(lv_timer_t* t) { static_cast<VideoPlayer*>(lv_timer_get_user_data(t))->step(nowUs()); }
    static void gone(lv_event_t* e);   // ends the video once its image is deleted with its viewport or its slide
    void end();                // ends the sound and deletes the image; gives the rooms back at once, the one a read still lands in once it has landed
    void readNext(uint32_t now);   // reads the frame after the last one asked for into the room that holds no frame

    Screen&        screen_;
    FileSystem&    files_;
    Sounds&        sounds_;
    FileReader     file_;
    FileRequest    reads_[2];            // the next frame, read whole in one request; the other may still land in a stopped video's room
    uint8_t        which_   = 0;         // the request of the video playing
    uint8_t*       leaving_ = nullptr;   // a stopped video's room, given back once its read has landed
    // two frames in the screen's picture memory: the card reads the next frame into one while LVGL
    // draws the other, so a read and a drawing each get a whole movie frame, docs/parts/display
    uint8_t*       rooms_[2]  = {};
    lv_image_dsc_t frames_[2] = {};      // RGB565 high byte first, LVGL's RGB565_SWAPPED, so LVGL copies it as it is
    uint32_t       in_[2]     = {};      // the frame each room holds, or that is on its way into it
    bool           ready_[2]  = {};      // the room holds a frame that has landed and waits to be drawn
    uint8_t        into_      = 0;       // the room the read on its way lands in
    lv_obj_t*      image_   = nullptr;   // inside the viewport, filling it
    lv_obj_t*      viewport_ = nullptr;  // the game's object the video plays in
    lv_obj_t*      slide_   = nullptr;   // the slide the viewport lies on, which owns the video
    uint32_t       startAt_ = 0;         // nowUs() as the first frame was due
    uint32_t       count_   = 0;         // the frames in the file
    uint32_t       asked_   = 0;         // the last frame a read was started for
    bool           reading_ = false;     // a read of this video is on its way
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
    if (leaving_ && rooms_[0] && reads_[which_].status() == FileStatus::Pending) return false;   // both requests still land in stopped videos' rooms, for a few ms at most
    if (rooms_[0]) end();                                 // the rooms of the video it replaces are free again, unless a read still lands in one
    const size_t bytes = (size_t)width * height * 2;
    const bool   full  = width == 320 && height == 240;
    uint8_t* a = screen_.pictures_.take(bytes, full);
    uint8_t* b = a ? screen_.pictures_.take(bytes, full) : nullptr;
    if (!b) { if (a) screen_.pictures_.give(a); return false; }   // the picture memory is full
    rooms_[0] = a;
    rooms_[1] = b;
    for (uint8_t r = 0; r < 2; ++r) {
        frames_[r] = {};
        frames_[r].header = {LV_IMAGE_HEADER_MAGIC, LV_COLOR_FORMAT_RGB565_SWAPPED, 0, width, height, (uint16_t)(width * 2), 0};
        frames_[r].data_size = (uint32_t)bytes;
        frames_[r].data = rooms_[r];
        ready_[r] = false;
    }
    lv_timer_pause(lv_display_get_refr_timer(lv_display_get_default()));   // the frames refresh the screen meanwhile
    image_ = lv_image_create(viewport);                   // at the viewport's top left, inside its padding
    lv_obj_add_event_cb(image_, gone, LV_EVENT_DELETE, this);
    viewport_ = viewport;
    slide_    = slide;
    // ... file_ = files_.open("video/<name>.rgb"), count_ from its size; the read of frame 0 into
    // rooms_[0], with into_ = 0, in_[0] = 0, asked_ = 0 and reading_ = true
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
// video. A read lands in one room while LVGL draws from the other, and the room LVGL has drawn from
// takes the next read at once
void VideoPlayer::step(uint32_t now) {
    if (leaving_ && reads_[which_ ^ 1].status() != FileStatus::Pending) {   // the stopped video's read has landed
        screen_.pictures_.give(leaving_);
        leaving_ = nullptr;
    }
    if (rooms_[0] && screen_.top() != slide_) { end(); return; }   // another slide covers the video's own
    if (!rooms_[0]) return;
    const FileRequest& read_ = reads_[which_];
    if (reading_ && read_.status() != FileStatus::Pending) {        // the read has landed
        reading_ = false;
        if (read_.status() == FileStatus::Failed || read_.bytes() < frames_[into_].data_size) {   // the file has run out or is missing
            if (loop_ && count_ > 0) { /* ... the read of frame 0 again into the same room, asked_ = 0 and startAt_ = now; a sound that loops starts over with it */ return; }
            if (!ready_[into_ ^ 1]) {                     // nothing left to draw
                ended_    = {now, EventType::MovieEnded, screen_.keyOf(slide_), 0};   // the key of the video's slide
                endedDue_ = true;
                end();
                return;
            }
        } else {
            ready_[into_] = true;
        }
    }
    const uint8_t r = ready_[0] && ready_[1] ? (in_[0] < in_[1] ? 0 : 1) : (ready_[0] ? 0 : 1);   // the older frame first
    if (ready_[r] && due(now) >= in_[r]) {
        lv_image_set_src(image_, &frames_[r]);
        lv_refr_now(nullptr);                             // draws the screen now, with every other change; the room is free again once this returns
        ready_[r] = false;
    }
    if (!reading_) readNext(now);
}

// the frame after the last one asked for, or the one after the frame due once the video is late,
// into the room that holds no frame
void VideoPlayer::readNext(uint32_t now) {
    const uint8_t r = !ready_[0] ? 0 : (!ready_[1] ? 1 : 2);
    if (r == 2) return;                                   // both rooms wait to be drawn
    const uint32_t frame = max(asked_ + 1, due(now) + 1);
    if (frame >= count_ && !loop_) return;                // the read of the last frame has been asked for
    into_    = r;
    in_[r]   = frame;
    asked_   = frame;
    reading_ = true;
    // ... the read of frame into rooms_[r], whole, at frame * frames_[r].data_size, into reads_[which_]
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
    if (reading_ && reads_[which_].status() == FileStatus::Pending) {   // step() gives that room back once the read has landed
        leaving_ = rooms_[into_];
        screen_.pictures_.give(rooms_[into_ ^ 1]);
        which_  ^= 1;
    } else {
        screen_.pictures_.give(rooms_[0]);
        screen_.pictures_.give(rooms_[1]);
    }
    rooms_[0] = rooms_[1] = nullptr;
    reading_  = false;
}
