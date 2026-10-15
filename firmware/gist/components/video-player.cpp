// the videos of the running game on the background, with their sound in place of the music;
// every call returns at once
class VideoPlayer : public Component {
public:
    VideoPlayer(Display& display, FileSystem& files, Sounds& sounds)
        : display_(display), files_(files), sounds_(sounds) {}

    // turns the end of the video last started into MovieEnded and ends its sound with it
    bool translate(const DriverEvent& in, PinballEvents& out) override;

    // plays video/<name>.rgb in the viewport at x, y on the background, over and over with loop, with
    // sound audio/<name>.wav in place of the music; false and nothing played for a viewport whose x or
    // width is no multiple of 16 or that reaches past the screen; ends the sound of a video it
    // replaces when the new one brings none
    bool play(const char* name, uint16_t x, uint16_t y, uint16_t width, uint16_t height, bool loop, bool sound);

    void stop();            // stops the video and its sound; the music goes on where it stopped
    bool playing() const;   // gets whether the video last started has neither ended nor been stopped

private:
    friend class GameHost;

    void reset();           // stops the video of the stopped game; the host calls it when it switches games

    void ended();           // forgets the video

    Display&     display_;
    FileSystem&  files_;               // puts the running game's directory in front of a name
    Sounds&      sounds_;
    uint32_t     video_ = 0;           // the number the driver gave the video last started, 0 while none plays
    bool         sound_ = false;       // the video playing brings its sound
    PinballEvent ended_;               // the MovieEnded that translate() points out at
};

bool VideoPlayer::play(const char* name, uint16_t x, uint16_t y, uint16_t width, uint16_t height, bool loop, bool sound) {
    char path[Storage::kPath];
    if (snprintf(path, sizeof path, "video/%s.rgb", name) >= (int)sizeof path) path[0] = 0;   // a name too long finds no file
    const uint32_t video = display_.play(files_.open(path), x, y, width, height, loop);
    if (video == 0) return false;              // the viewport is off the steps; what plays goes on
    video_ = video;
    if (sound) {
        char wav[Storage::kPath];
        if (snprintf(wav, sizeof wav, "%s.wav", name) >= (int)sizeof wav) wav[0] = 0;
        sounds_.interlude(wav, loop);          // in the same pass as the picture
    } else if (sound_) {
        sounds_.stopInterlude();               // the video it replaces brought sound, the new one none
    }
    sound_ = sound;
    return true;
}

void VideoPlayer::stop() {
    display_.stopMovie();
    if (sound_) sounds_.stopInterlude();
    ended();
}

void VideoPlayer::ended() {
    video_ = 0;
    sound_ = false;
}

bool VideoPlayer::playing() const { return video_ != 0; }

bool VideoPlayer::translate(const DriverEvent& in, PinballEvents& out) {
    if (in.type != DriverEventType::MovieEnded) return false;
    out = {};
    if (video_ == 0 || in.payload != video_) return true;   // an older video's end
    if (sound_) sounds_.stopInterlude();                     // a sound longer than the video ends with it
    ended();
    ended_ = {in.time, EventType::MovieEnded, 0, in.payload};
    out    = {&ended_, 1};
    return true;
}

void VideoPlayer::reset() {
    display_.stopMovie();
    ended();                                 // the sounds' own reset silences the music channel
}
