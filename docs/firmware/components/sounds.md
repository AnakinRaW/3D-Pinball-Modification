# Sounds component

This component allows the game to play its own audio files from the SD card in the `/games/<id>/audio` directory.

The host hands every `SoundEnded` event of the driver to this component.

## Music and effects

| Kind | Plays on | Started with |
|---|---|---|
| Music, such as the background music or the win and the loss music | channel `0` | `music()` |
| Effects, such as a bumper hit or the drain | channels `1` to `3` | `play()` |
| Interlude, such as the sound of a movie | channel `0`, in place of the music | `interlude()` |

The music plays one track at a time. A new track replaces the one playing. A track started with `loop` repeats.

Effects play on top of music, up to three at once. When all three channels are busy, a new effect replaces the oldest.

An interlude plays in place of the music. Once it has ended, the music goes on where it stopped. Music that was off stays off. A new track or `stopMusic()` ends the interlude. `stopInterlude()` ends it early, after which the music goes on where it stopped.

A sound is named by its file name in the game's audio directory, such as `bumper.wav`. 

`reset()` is available to the host and stops all channels.

## Volume

`musicVolume()` and `effectsVolume()` take a percentage from 0 to 1. The component squares it, so half the range sounds about half as loud.

## End of the music

`musicPlaying()` tells whether the music on channel 0 still plays. Interlude also counts as playing. When a track has played to its end, the component sends `MusicEnded`. A track that loops does not send the event. The end of an effect sends nothing.

| Event | When | Payload |
|---|---|---|
| `MusicEnded` | A track has played to its end | The number the driver gave the track |

## Interface

```cpp
// the sounds of the running game, the music on one channel and the effects on the other three;
// every call returns at once
class Sounds : public Component {
public:
    Sounds(AudioDriver& audio, FileSystem& files);

    // turns the end of a track into MusicEnded, lets the music go on after an interlude and ignores the end of an effect
    bool translate(const DriverEvent& in, PinballEvents& out) override;

    // plays an effect of the running game by its file name in its audio directory, such as "bumper.wav"
    void play(const char* name);

    // plays a track of the running game by its file name from its beginning, over and over with loop;
    // it ends an interlude
    void music(const char* name, bool loop = false);

    // plays a sound of the running game in place of the music; the music goes on where it stopped
    // once the sound has ended
    void interlude(const char* name);

    void stopInterlude();                  // ends the interlude early; the music goes on where it stopped
    void stopMusic();                      // silences the music, and an interlude with it
    bool musicPlaying() const;             // gets whether the track last started has neither ended nor been stopped

    void musicVolume(float percent);       // sets the volume of the music and of an interlude, 0 to 1 as a percentage
    void effectsVolume(float percent);     // sets the volume of the effects, 0 to 1 as a percentage

private:
    friend class GameHost;

    void reset();                          // silences every channel and sets the volumes again; the host calls it when it switches games
};
```
