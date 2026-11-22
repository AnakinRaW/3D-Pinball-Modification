// The music and the effects of the running game, played from its audio directory on the card. The
// music takes one channel of the audio driver and the effects the other three. Several modes can
// each hold a track of music, and only the track of the highest priority plays while the others
// wait paused. A video's sound lies over every track. When the track on top ends or stops, the
// track below goes on where it was paused.
//
// Design: docs/firmware/components/sounds.md.
#pragma once
#include "components/file-system.h"
#include "core/priority-list.h"
#include "drivers/audio.h"
#include "game/component.h"
#include "game/layer.h"

class Sounds : public Component {
public:
    static constexpr uint8_t MUSIC = 0;           // sounds.md: the music's channel
    static constexpr uint8_t MODES = 8;           // sounds.md: the modes that hold a track at once
    static constexpr float MUSIC_VOLUME = 0.5f;   // audio.music_volume's default
    static constexpr float EFFECTS_VOLUME = 0.7f;   // audio.effects_volume's default
    static constexpr float VOLUME_MIN = 0;
    static constexpr float VOLUME_MAX = 1;

    /// The starting volumes from the config, each a percentage from 0 to 1.
    struct Settings {
        float musicVolume = MUSIC_VOLUME;       // audio.music_volume
        float effectsVolume = EFFECTS_VOLUME;   // audio.effects_volume
    };

    /// Creates the sounds on an audio driver and the game's files.
    ///
    /// @param audio  The driver that plays the sounds.
    /// @param files  Gives the files of the running game.
    Sounds(AudioDriver& audio, FileSystem& files) : audio(audio), files(files) {}

    /// Takes the starting volumes.
    ///
    /// @note  The main task calls it before begin().
    void configure(const Settings& given) { settings = given; }

    /// Sets both volumes to their starting volumes.
    ///
    /// Where the two starting volumes squared add up to more than 1, it lowers the effects' volume
    /// until they add up to 1 and writes a warning to the log, so music and an effect together
    /// never overdrive the output. A volume outside 0 to 1 counts as the nearer end.
    ///
    /// @note  The host calls it before the first game, after the audio driver's begin().
    void begin();

    /// Takes the end of a sound.
    ///
    /// The end of a mode's track becomes MUSIC_ENDED, and the track below goes on. The end of an
    /// effect, of a video's sound and of a track already stopped or replaced makes no event.
    ///
    /// @return  True for every SOUND_ENDED, false for any other event.
    bool translate(const DriverEvent& in, PinballEvents& out) override;

    /// Plays an effect of the running game.
    ///
    /// The effect takes a free effect channel. With all three busy, it replaces the effect that
    /// started first.
    ///
    /// @param name  The file's name in the game's audio directory, such as "bumper.wav". A name
    ///              too long for a path finds no file, and the effect ends at once.
    /// @note        The main loop calls it. It returns at once.
    void play(const char* name);

    /// Plays a track of the running game for a mode, from its beginning.
    ///
    /// The track replaces the mode's track. It plays when its priority is the highest of the
    /// tracks, and the latest of equal priorities. Otherwise it waits paused until the tracks
    /// above have ended. A track that comes on top pauses the track below where it stands.
    ///
    /// @param layer  The mode's key and priority.
    /// @param name   The file's name in the game's audio directory, such as "theme.wav".
    /// @param loop   Plays the track over and over, so it never ends.
    /// @note         The main loop calls it. It returns at once. The track of a ninth mode is
    ///               refused.
    void music(Layer layer, const char* name, bool loop = false);

    /// Ends the track of a mode.
    ///
    /// On top, the paused track below goes on. The track sends no MUSIC_ENDED.
    ///
    /// @param layer  The mode's key and priority. A mode without a track is left alone.
    /// @note         The mode stack calls it when a mode ends.
    void stopMusic(Layer layer);

    /// Gets whether the track of a mode has played to its end or was stopped.
    ///
    /// @param layer  The mode's key and priority.
    /// @return       False while the mode's track plays or waits paused.
    bool musicEnded(Layer layer) const;

    /// Sets the volume of the music and of a video's sound.
    ///
    /// @param percent  Runs from 0 to 1. A value outside counts as the nearer end. The driver
    ///                 gets its square, so half the range sounds about half as loud.
    void musicVolume(float percent);

    /// Sets the volume of the effects.
    ///
    /// @param percent  Runs from 0 to 1, as for musicVolume().
    void effectsVolume(float percent);

private:
    friend class GameHost;
    friend class VideoPlayer;

    /// Above every priority a mode can have, which takes a byte.
    static constexpr uint16_t VIDEO_PRIORITY = 256;

    /// Whose a track is: a mode's under its key, or a video's over every mode's.
    enum class Owner : uint8_t { MODE, VIDEO };

    /// A track of music: its file, and where it was paused.
    struct Track {
        Owner owner = Owner::MODE;
        uint8_t key = 0;        // the mode's key, none for a video's sound
        uint8_t priority = 0;   // the mode's priority, none for a video's sound
        FileReader file;        // the track from its beginning
        bool loop = false;
        AudioDriver::Position at;   // where it was paused, empty before it has played
        /// The driver's number while it plays or while its end is on the way, 0 while paused
        uint32_t number = 0;

        /// Gets whether @p other takes this track's place.
        bool same(const Track& other) const {
            return owner == other.owner && (owner == Owner::VIDEO || key == other.key);
        }
        friend uint16_t priorityOf(const Track& t) {
            return t.owner == Owner::VIDEO ? VIDEO_PRIORITY : t.priority;
        }
    };

    /// Plays a video's sound over every track of the game, which waits paused meanwhile.
    ///
    /// A track started meanwhile waits paused as well. Once the video's sound has ended, the
    /// track on top goes on, or starts from its beginning when it has not played yet.
    ///
    /// @param name  The file's name in the game's audio directory.
    /// @param loop  Plays the sound over and over.
    void videoSound(const char* name, bool loop);

    /// Ends the video's sound. The track on top goes on.
    void stopVideoSound();

    /// Gets whether the video's sound plays, neither ended nor stopped.
    bool videoSounding() const;

    /// Gets how many samples of the video's sound have played.
    ///
    /// @return  0 before it has started and without a video's sound.
    uint32_t videoPlayed() const;

    /// Silences every channel, forgets every track and sets the volumes again.
    ///
    /// @note  The host calls it when it switches games.
    void reset();

    /// Gets the driver's volume for a percentage, the square of it inside 0 to 1.
    static float gain(float percent);

    /// Gets a sound of the running game by its name in the game's audio directory.
    FileReader file(const char* name) const;

    /// Gets a free effect channel, or the one whose effect started first.
    uint8_t pick() const;

    /// Pauses a track where it stands on the music channel.
    void hold(Track& t);

    /// Lets the track on top play, from where it was paused. Without a track the music is silent.
    void resume();

    /// Puts a track in the place of the one it replaces, at its priority, and lets the track on
    /// top play.
    void start(const Track& track);

    /// Removes the tracks @p pred holds for. On top, the track below goes on.
    template <typename Pred> void drop(Pred pred);

    AudioDriver& audio;
    FileSystem& files;
    Settings settings;
    PriorityList<Track, MODES + 1> tracks;   // the game's tracks and a video's sound, on top first
    uint32_t onChannel = 0;   // the number of the track the music channel was handed last
    uint32_t effects = 0;     // counts the effects play() has started
    uint32_t startedAs[AudioDriver::CHANNELS] =
        {};                    // which count each channel's effect started at
    float musicGain = 0;       // the driver's volume for the music and a video's sound
    float effectsGain = 0;     // the driver's volume for the effects
    PinballEvent ended = {};   // the MUSIC_ENDED that translate() points at
};
