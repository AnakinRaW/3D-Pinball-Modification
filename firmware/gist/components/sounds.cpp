// the sounds of the running game, the music on one channel and the effects on the other three;
// every call returns at once
class Sounds : public Component {
public:
    Sounds(AudioDriver& audio, FileSystem& files) : audio_(audio), files_(files) {}

    // turns the end of a track into MusicEnded, lets the music go on after an interlude and ignores the end of an effect
    bool translate(const DriverEvent& in, PinballEvents& out) override;

    // plays an effect of the running game by its file name in its audio directory, such as "bumper.wav"
    void play(const char* name);

    // plays a track of the running game by its file name from its beginning, over and over with loop;
    // it ends an interlude
    void music(const char* name, bool loop = false);

    // plays a sound of the running game in place of the music, over and over with loop; the music
    // goes on where it stopped once the sound has ended
    void interlude(const char* name, bool loop = false);

    void stopInterlude();                  // ends the interlude early; the music goes on where it stopped
    uint32_t interludePlayed() const;      // gets how many samples of the interlude have played, 0 while none plays
    void stopMusic();                      // silences the music, and an interlude with it
    bool musicPlaying() const;             // gets whether the track last started has neither ended nor been stopped

    void musicVolume(float percent);       // sets the volume of the music and of an interlude, 0 to 1 as a percentage
    void effectsVolume(float percent);     // sets the volume of the effects, 0 to 1 as a percentage

private:
    friend class GameHost;

    static constexpr uint8_t kMusic = 0;   // the music's channel, as SoundEnded names it
    static constexpr float   kMusicStart   = 0.5f;   // the music's starting percentage, a decision: under the effects
    static constexpr float   kEffectsStart = 0.7f;   // the effects' starting percentage, a decision

    // gets the driver's volume for a percentage along a square curve, so the middle of the range
    // sounds about half as loud as the top
    static constexpr float gain(float percent) { return percent < 0 ? 0 : percent > 1 ? 1 : percent * percent; }

    FileWriter file(const char* name);     // gets a sound of the running game by its file name in its audio directory
    uint8_t    pick() const;               // gets a free effect channel, or the one whose effect started first
    void       reset();                    // silences every channel and sets the volumes again; the host calls it when it switches games

    AudioDriver& audio_;
    FileSystem&  files_;                   // puts the running game's directory in front of a name
    uint32_t     track_     = 0;           // the number of the track last started, 0 once it has ended or been stopped
    uint32_t     interlude_ = 0;           // the number of the interlude playing, 0 while none plays
    AudioDriver::Position musicAt_;        // where the music stood when the interlude began, empty when none played
    uint32_t     started_[AudioDriver::kChannels] = {};   // when the effect on each channel started
    float        musicGain_   = gain(kMusicStart);     // the driver's volume for the music and an interlude
    float        effectsGain_ = gain(kEffectsStart);   // the driver's volume for the effects
    PinballEvent ended_;                   // the MusicEnded that translate() points out at
};

uint32_t Sounds::interludePlayed() const {
    return interlude_ ? audio_.position(kMusic).played() : 0;   // the interlude plays on the music's channel
}

FileWriter Sounds::file(const char* name) {
    char path[Storage::kPath];
    if (snprintf(path, Storage::kPath, "audio/%s", name) >= Storage::kPath) path[0] = 0;   // a name too long finds no file
    return files_.open(path);
}

uint8_t Sounds::pick() const {
    uint8_t pick = 0;
    for (uint8_t c = kMusic + 1; c < AudioDriver::kChannels; ++c) {
        if (!audio_.playing(c)) return c;
        if (pick == 0 || before(started_[c], started_[pick])) pick = c;
    }
    return pick;
}

void Sounds::play(const char* name) {
    const uint8_t c = pick();
    started_[c] = nowUs();
    audio_.play(c, file(name));
}

void Sounds::music(const char* name, bool loop) {
    interlude_ = 0;                        // a new track ends an interlude, and nothing goes on afterwards
    musicAt_   = {};
    track_     = audio_.play(kMusic, file(name), loop);
}

void Sounds::interlude(const char* name, bool loop) {
    if (interlude_ == 0) musicAt_ = audio_.position(kMusic);   // the first interlude keeps where the music stands
    interlude_ = audio_.play(kMusic, file(name), loop);
}

void Sounds::stopInterlude() {
    if (interlude_ == 0) return;
    interlude_ = 0;
    track_     = audio_.play(kMusic, musicAt_);   // replaces the interlude, so its end sends nothing
    musicAt_   = {};
}

void Sounds::stopMusic() {
    track_     = 0;
    interlude_ = 0;
    musicAt_   = {};
    audio_.stop(kMusic);
}

bool Sounds::musicPlaying() const { return interlude_ != 0 ? !musicAt_.empty() : audio_.playing(kMusic); }

void Sounds::musicVolume(float percent) {
    musicGain_ = gain(percent);
    audio_.volume(kMusic, musicGain_);
}

void Sounds::effectsVolume(float percent) {
    effectsGain_ = gain(percent);
    for (uint8_t c = kMusic + 1; c < AudioDriver::kChannels; ++c) audio_.volume(c, effectsGain_);
}

bool Sounds::translate(const DriverEvent& in, PinballEvents& out) {
    if (in.type != DriverEventType::SoundEnded) return false;
    out = {};
    if (in.source != kMusic) return true;                  // an effect's end
    if (interlude_ != 0 && in.payload == interlude_) {     // the interlude has ended
        interlude_ = 0;
        track_     = audio_.play(kMusic, musicAt_);        // the music goes on where it stopped; music that was off stays off
        musicAt_   = {};
        return true;
    }
    if (in.payload == track_) {                            // the track last started, and no older one
        track_ = 0;
        ended_ = {in.time, EventType::MusicEnded, kMusic, in.payload};
        out    = {&ended_, 1};
    }
    return true;
}

void Sounds::reset() {
    track_     = 0;
    interlude_ = 0;
    musicAt_   = {};
    for (uint8_t c = 0; c < AudioDriver::kChannels; ++c) {
        audio_.stop(c);                                          // no sound of the stopped game plays on
        audio_.volume(c, c == kMusic ? musicGain_ : effectsGain_);   // every game starts at the set volumes
    }
}
