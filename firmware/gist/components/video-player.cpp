// the videos of the running game on the background, with their sound in place of the music;
// every call returns at once
class VideoPlayer : public Component {
public:
    VideoPlayer(Screen& screen, FileSystem& files, Sounds& sounds)
        : screen_(screen), files_(files), sounds_(sounds) {}

    void begin();           // attaches its queue to the event queue and its timer to LVGL, after the screen's begin()

    // hands the MovieEnded it published to the game
    bool translate(const DriverEvent& in, PinballEvents& out) override;

    // plays video/<name>.rgb in the viewport at x, y on the background, over and over with loop,
    // with sound audio/<name>.wav in place of the music; false and nothing played for a viewport
    // beyond the screen or without room for its frame in the picture memory
    bool play(const char* name, uint16_t x, uint16_t y, uint16_t width, uint16_t height, bool loop, bool sound);

    void stop();            // stops the video and its sound; the music goes on where it stopped
    bool playing() const { return room_ != nullptr; }   // gets whether the video last started has neither ended nor been stopped

private:
    friend class GameHost;

    static constexpr uint32_t kFrameUs = 40000;   // 25 frames a second, docs/parts/display
    static constexpr size_t   kDepth   = 1;       // step() publishes at most one end in a pass, and the main loop empties the queue in every pass

    void reset() { stop(); }   // stops the video of the stopped game; the host calls it when it switches games

    // draws a frame once it is due and reads the next, and publishes MovieEnded once the video
    // last started has run out; LVGL's timer calls it in every pass of the main loop
    void step(uint32_t now);
    static void tick(lv_timer_t* t) { static_cast<VideoPlayer*>(lv_timer_get_user_data(t))->step(nowUs()); }
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
    lv_obj_t*      image_   = nullptr;   // in the screen's container in front of the game's background
    uint32_t       startAt_ = 0;         // nowUs() as the first frame was due
    uint32_t       frames_  = 0;         // the frames in the file
    uint32_t       next_    = 0;         // the frame in the room, or on its way into it
    bool           landed_  = false;     // the frame in the room has landed and waits to be drawn
    bool           loop_    = false;
    bool           sound_   = false;
    DriverEvent    queue_[kDepth];
    EventQueue::Producer* out_ = nullptr;
    PinballEvent   ended_;               // the MovieEnded that translate() points out at
};

void VideoPlayer::begin() {
    out_ = &events.attach(queue_, kDepth);
    lv_timer_create(tick, 0, this);          // a period of 0: every lv_timer_handler() runs it, inside the screen's update()
}

bool VideoPlayer::play(const char* name, uint16_t x, uint16_t y, uint16_t width, uint16_t height, bool loop, bool sound) {
    if (width == 0 || height == 0 || x + width > 320 || y + height > 240) return false;
    if (leaving_ && room_ && reads_[which_].status() == FileStatus::Pending) return false;   // both requests still land in stopped videos' rooms, for a few ms at most
    stop();                                               // the room of the video it replaces is free again, unless a read still lands in it
    uint8_t* room = screen_.pictures_.take((size_t)width * height * 2, width == 320 && height == 240);
    if (!room) return false;                              // the picture memory is full
    room_  = room;
    frame_ = {};
    frame_.header = {LV_IMAGE_HEADER_MAGIC, LV_COLOR_FORMAT_RGB565_SWAPPED, 0, width, height, (uint16_t)(width * 2), 0};
    frame_.data_size = (uint32_t)width * height * 2;
    frame_.data = room_;
    lv_timer_pause(lv_display_get_refr_timer(lv_display_get_default()));   // the frames refresh the screen meanwhile
    image_ = lv_image_create(screen_.videos());
    lv_obj_set_pos(image_, x, y);
    lv_image_set_src(image_, &frame_);
    // ... file_ = files_.open("video/<name>.rgb"), frames_ from its size; the read of frame 0
    next_    = 0;
    landed_  = false;
    loop_    = loop;
    startAt_ = nowUs();
    // ... the sound as an interlude, as before
    return true;
}

// the frame due follows the time since the start, so a late frame is skipped rather than slowing
// the video against its sound
void VideoPlayer::step(uint32_t now) {
    if (leaving_ && reads_[which_ ^ 1].status() != FileStatus::Pending) {   // the stopped video's read has landed
        screen_.pictures_.give(leaving_);
        leaving_ = nullptr;
    }
    const FileRequest& read_ = reads_[which_];
    if (!room_ || read_.status() == FileStatus::Pending) return;
    if (!landed_) {
        if (read_.status() == FileStatus::Failed || read_.bytes() < frame_.data_size) {   // the file has run out or is missing
            if (loop_ && frames_ > 0) { /* ... the read of frame 0 again, startAt_ = now */ return; }
            out_->publish(DriverEvent{now, DriverEventType::MovieEnded, 0, 0});
            end();
            return;
        }
        landed_ = true;
    }
    if (!elapsedUs(startAt_, next_ * kFrameUs)) return;   // not due yet
    lv_obj_invalidate(image_);
    lv_refr_now(nullptr);                                 // draws the screen now, with every other change; the room is free again once this returns
    landed_ = false;
    next_   = (now - startAt_) / kFrameUs + 1;            // a late frame skips the ones whose time has passed
    // ... the read of frame next_ into the room, whole, at next_ * frame_.data_size
}

bool VideoPlayer::translate(const DriverEvent& in, PinballEvents& out) {
    if (in.type != DriverEventType::MovieEnded) return false;
    ended_ = {in.time, EventType::MovieEnded, 0, 0};
    out    = {&ended_, 1};
    return true;
}

void VideoPlayer::stop() {
    if (room_) end();
}

void VideoPlayer::end() {
    if (sound_) sounds_.stopInterlude();
    sound_ = false;
    if (image_) lv_obj_delete(image_);
    image_ = nullptr;
    lv_timer_resume(lv_display_get_refr_timer(lv_display_get_default()));   // LVGL refreshes on its own again
    if (reads_[which_].status() == FileStatus::Pending) {   // step() gives the room back once the read has landed
        leaving_ = room_;
        which_  ^= 1;
    } else {
        screen_.pictures_.give(room_);
    }
    room_ = nullptr;
}
