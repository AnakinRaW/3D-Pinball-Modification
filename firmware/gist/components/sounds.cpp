// the sounds of the running game, the music on one channel and the effects on the other three;
// every call returns at once
class Sounds : public Component {
public:
    Sounds(AudioDriver& audio, FileSystem& files) : audio_(audio), files_(files) {}

    // sets both volumes to their starting volumes from the config, the effects lowered where the two
    // squared add up to more than 1; the host calls it before the first game
    void begin();

    // turns the end of a track into MusicEnded and lets the track below go on; ignores the end of an
    // effect, and sends nothing for the end of a video's sound
    bool translate(const DriverEvent& in, PinballEvents& out) override;

    // plays an effect of the running game by its file name in its audio directory, such as "bumper.wav"
    void play(const char* name);

    // plays a track of the running game by its file name for mode at the mode's priority, from its
    // beginning, over and over with loop; it replaces the mode's track, and on top it pauses the track
    // below
    void music(const Mode& mode, const char* name, bool loop = false);

    // ends the mode's track, and the mode stack calls it when the mode ends; on top, the paused track
    // below resumes
    void stopMusic(const Mode& mode);
    bool musicEnded(const Mode& mode) const;   // gets whether the mode holds no track that plays or is paused

    void musicVolume(float percent);       // sets the volume of the music and of a video's sound, 0 to 1 as a percentage
    void effectsVolume(float percent);     // sets the volume of the effects, 0 to 1 as a percentage

private:
    friend class GameHost;
    friend class VideoPlayer;

    static constexpr uint8_t kMusic = 0;   // the music's channel, as SoundEnded names it
    static constexpr float   kMusicStart   = 0.5f;   // audio.music_volume's default
    static constexpr float   kEffectsStart = 0.7f;   // audio.effects_volume's default
    static constexpr size_t  kTracks = 9;            // one for each mode the mode stack runs and one for a video's sound

    // whose a track is: a mode's under the mode's key, or a video's over every mode's
    enum class Owner : uint8_t { Mode, Video };

    // one track: the file it starts from, or where it was paused
    struct Track {
        Owner      owner    = Owner::Mode;
        uint8_t    key      = 0;           // the mode's key; none for a video's track
        uint8_t    priority = 0;           // the mode's priority; none for a video's track
        FileReader file;                   // the track from its beginning
        bool       loop     = false;
        AudioDriver::Position at;          // where it was paused, empty before it has played and while it plays
        uint32_t   number   = 0;           // the driver's number while it plays, 0 while it is paused

        bool same(const Track& o) const { return owner == o.owner && (owner != Owner::Mode || key == o.key); }   // gets whether o takes this track's place

        // a video's track over every priority a mode can have
        friend uint16_t priorityOf(const Track& t) { return t.owner == Owner::Video ? 256 : t.priority; }
    };

    // gets the driver's volume for a percentage along a square curve, so the middle of the range
    // sounds about half as loud as the top
    static constexpr float gain(float percent) { return percent < 0 ? 0 : percent > 1 ? 1 : percent * percent; }

    FileWriter file(const char* name);     // gets a sound of the running game by its file name in its audio directory
    uint8_t    pick() const;               // gets a free effect channel, or the one whose effect started first
    void       hold(Track& t);             // keeps where t stands while it plays, so it is paused there
    void       resume();                   // plays the track on top from where it was paused, silence without one
    void       start(const Track& track);  // puts track in the place of the one it replaces, at its priority, and lets the track on top play
    void       start(Owner owner, const char* name, bool loop);   // starts a video's track
    template <typename Pred> void drop(Pred pred);   // removes the track pred holds for; on top, the track below goes on
    bool       holds(Owner owner) const;   // gets whether a track of owner plays or is paused
    void     videoSound(const char* name, bool loop);   // plays a video's sound over every track of the game, which is paused meanwhile
    void     stopVideoSound();                         // ends the video's sound; the paused track on top resumes
    bool     videoSounding() const;                    // gets whether the video's sound plays, neither ended nor stopped
    uint32_t videoPlayed() const;                      // gets how many samples of the video's sound have played, 0 before it has
    void       reset();                    // silences every channel and sets the volumes again; the host calls it when it switches games

    AudioDriver& audio_;
    FileSystem&  files_;                   // puts the running game's directory in front of a name
    PriorityList<Track, kTracks> tracks_;  // the game's tracks, the one that plays first
    uint32_t     started_[AudioDriver::kChannels] = {};   // when the effect on each channel started
    float        musicGain_   = 0;   // the driver's volume for the music and a video's sound; begin() sets the config's
    float        effectsGain_ = 0;   // the driver's volume for the effects
    PinballEvent ended_;                   // the MusicEnded that translate() points out at
};

void Sounds::begin() {
    const float music   = config.get(ConfigKey::kMusicVolume, kMusicStart, 0, 1);
    float       effects = config.get(ConfigKey::kEffectsVolume, kEffectsStart, 0, 1);
    const float left    = sqrtf(1 - music * music);   // the most the effects may take, so music and an effect never overdrive the output
    if (effects > left) {
        logger.write("sounds", "audio.effects_volume: %g -> %g, so music and an effect never overdrive the output", effects, left);
        effects = left;
    }
    musicVolume(music);
    effectsVolume(effects);
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

void Sounds::hold(Track& t) {
    if (t.number == 0) return;                            // it is paused already
    t.at     = audio_.position(kMusic);
    t.number = 0;
}

// play() replaces what the channel plays, so the end of a covered track sends nothing
void Sounds::resume() {
    if (!tracks_.size()) { audio_.stop(kMusic); return; }
    Track& t = tracks_[0];
    if (t.number != 0) return;                            // it plays already
    t.number = t.at.empty() ? audio_.play(kMusic, t.file, t.loop) : audio_.play(kMusic, t.at);
    t.at     = {};
}

// the track it replaces leaves first, so the new one lies above the tracks of equal priority; the
// channel changes only in resume(), so the track the new one covers is held where it stands; a
// track for each mode the stack runs and one for the video's sound, so the insert always finds room
void Sounds::start(const Track& track) {
    tracks_.removeIf([&](const Track& t) { return t.same(track); });
    tracks_.insert(track);
    if (tracks_[0].same(track) && tracks_.size() > 1) hold(tracks_[1]);   // the track below is paused where it stands
    resume();                                             // the track on top, whichever it is now
}

template <typename Pred>
void Sounds::drop(Pred pred) {
    const bool top = tracks_.size() && pred(tracks_[0]);
    tracks_.removeIf(pred);
    if (top) resume();                                    // the paused track below resumes
}

void Sounds::music(const Mode& mode, const char* name, bool loop) {
    Track track;
    track.key      = mode.key();
    track.priority = mode.priority();
    track.file     = file(name);
    track.loop     = loop;
    start(track);
}

void Sounds::start(Owner owner, const char* name, bool loop) {
    Track track;
    track.owner = owner;
    track.file  = file(name);
    track.loop  = loop;
    start(track);
}

bool Sounds::holds(Owner owner) const {
    for (size_t i = 0; i < tracks_.size(); ++i) if (tracks_[i].owner == owner) return true;
    return false;
}

void Sounds::stopMusic(const Mode& mode) {
    const uint8_t key = mode.key();
    drop([&](const Track& t) { return t.owner == Owner::Mode && t.key == key; });
}

bool Sounds::musicEnded(const Mode& mode) const {
    const uint8_t key = mode.key();
    for (size_t i = 0; i < tracks_.size(); ++i) if (tracks_[i].owner == Owner::Mode && tracks_[i].key == key) return false;
    return true;
}

void Sounds::videoSound(const char* name, bool loop) { start(Owner::Video, name, loop); }
void Sounds::stopVideoSound()                         { drop([](const Track& t) { return t.owner == Owner::Video; }); }
bool Sounds::videoSounding() const                    { return holds(Owner::Video); }

// the video's sound lies on top while it plays, so the channel counts it; covered by nothing, it
// is paused only before it has started
uint32_t Sounds::videoPlayed() const {
    if (!tracks_.size() || tracks_[0].owner != Owner::Video) return 0;
    return tracks_[0].number != 0 ? audio_.position(kMusic).played() : 0;
}

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
    if (tracks_.size() && in.payload == tracks_[0].number) {   // the track that plays, and no older sound
        const Track ended = tracks_[0];
        if (ended.owner != Owner::Video) {                 // the video player follows its own sound
            ended_ = {in.time, EventType::MusicEnded, ended.key, in.payload};
            out    = {&ended_, 1};
        }
        tracks_.removeIf([&](const Track& t) { return t.same(ended); });
        resume();                                          // the paused track below resumes
    }
    return true;
}

void Sounds::reset() {
    tracks_.clear();
    for (uint8_t c = 0; c < AudioDriver::kChannels; ++c) {
        audio_.stop(c);                                          // no sound of the stopped game plays on
        audio_.volume(c, c == kMusic ? musicGain_ : effectsGain_);   // every game starts at the set volumes
    }
}
