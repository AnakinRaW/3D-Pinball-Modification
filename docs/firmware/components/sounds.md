# Sounds component

This component allows the game to play its own audio files from the SD card in the `/games/<id>/audio` directory.

The host hands every `SoundEnded` event of the driver to this component.

## Music and effects

| Kind | Plays on | Started with |
|---|---|---|
| Music, such as the background music or the win and the loss music | channel `0` | `music()` |
| Effects, such as a bumper hit or the drain | channels `1` to `3` | `play()` |
| A video's sound | channel `0`, over every track of the music | the [video player](video-player.md) |

Effects play on top of music, up to three at once. When all three channels are busy, a new effect replaces the oldest.

A sound is named by its file name in the game's audio directory, such as `bumper.wav`. 

`reset()` is available to the host and stops all channels.

## Music

The music plays one track at a time. Several modes can each start a track. Only the track of the mode with the highest [priority](../game-abstraction.md#modes) plays. Between equal priorities, the later track plays. The other tracks are paused. When the track on top ends or is stopped, the next one resumes where it was paused. A track started with `loop` repeats.

A mode starts a track with `music(*this, name)`. A new track of the same mode replaces the one before, so a mode plays its tracks one after another. `stopMusic(*this)` ends the track while the mode runs on. When the mode ends, [`ModeStack::stop()`](../game-abstraction.md#modes) calls `stopMusic(mode)`, and the paused track below resumes.

A video's sound is a track as well. It lies over every track of the game, whatever their priorities. A track started while the video's sound plays stays paused under it. Once the video's sound has ended, the track on top resumes, or starts from its beginning when it has not played yet.

## Volume

`musicVolume()` and `effectsVolume()` take a percentage from 0 to 1. The component squares it, so half the range sounds about half as loud.

`begin()` sets both to their starting volumes from the [config](#config). The host calls it before the first game.

## Config

The starting volumes stand in the [machine config](config.md):

| Key | Default | Range |
|---|---|---|
| `audio.music_volume` | 0.5 | 0 to 1 |
| `audio.effects_volume` | 0.7 | 0 to 1 |

The two starting volumes squared add up to at most 1, so music and an effect together never overdrive the output. Where they add up to more, `begin()` lowers the effects volume until they add up to 1 and writes a line to the log.

## End of the music

`musicEnded(mode)` tells whether the track of a mode has played to its end or was stopped. A paused track has not ended. When a track has played to its end, the component sends `MusicEnded`. A track that loops does not send the event. A track that is stopped or replaced sends none either. The end of a video's sound and the end of an effect send nothing.

| Event | When | Payload |
|---|---|---|
| `MusicEnded` | A track has played to its end. Its source is the key of the track's mode | The number the driver gave the track |

## Interface

```cpp
// the sounds of the running game, the music on one channel and the effects on the other three;
// every call returns at once
class Sounds : public Component {
public:
    Sounds(AudioDriver& audio, FileSystem& files);

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

    void     videoSound(const char* name, bool loop);   // plays a video's sound over every track of the game, which is paused meanwhile
    void     stopVideoSound();                         // ends the video's sound; the paused track on top resumes
    bool     videoSounding() const;                    // gets whether the video's sound plays, neither ended nor stopped
    uint32_t videoPlayed() const;                      // gets how many samples of the video's sound have played, 0 before it has
    void reset();                          // silences every channel, forgets every track and sets the volumes again; the host calls it when it switches games
};
```
