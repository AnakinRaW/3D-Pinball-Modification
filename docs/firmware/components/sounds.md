# Sounds component

The sounds component plays the game's audio files from the SD card in the `/games/<id>/audio` directory.

The host hands every `SOUND_ENDED` event of the driver to this component.

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

A mode starts a track with `music(layer(), name)`. A new track of the same mode replaces the one before, so a mode plays its tracks one after another. `stopMusic(layer())` ends the track while the mode runs on. When the mode ends, [`ModeStack::stop()`](../game-abstraction.md#modes) calls `stopMusic()` with the mode's layer, and the paused track below resumes.

The component holds the tracks of up to eight modes and a video's sound. The track of a ninth mode is refused.

A video's sound is a track as well. It lies over every track of the game, whatever their priorities. A track started while the video's sound plays stays paused under it. Once the video's sound has ended, the track on top resumes, or starts from its beginning when it has not played yet.

## Volume

`musicVolume()` and `effectsVolume()` take a percentage from 0 to 1. The component squares it, so half the range sounds about half as loud. Each volume runs over its full range whatever the other holds.

The driver's [mixer](../drivers/audio.md#volume) adds the music and the effects. Where the two squares add up to more than 1, the component hands the driver both squares divided by their sum. Music and an effect together then stay within the output's full scale. The ratio between them stays the one the two volumes set. With both at 1, each plays at half the full scale.

The component keeps each volume as it was set. Lowering one volume therefore brings the other back up toward its square.

`configure()` takes the starting volumes from the [config](#config), and `begin()` sets both to them. The host calls `begin()` before the first game.

## Config

The starting volumes stand in the [machine config](config.md):

| Key | Default | Range |
|---|---|---|
| `audio.music_volume` | 0.5 | 0 to 1 |
| `audio.effects_volume` | 0.7 | 0 to 1 |

The two defaults squared add up to less than 1, so both play as set.

## End of the music

`musicEnded(layer)` tells whether the track of a mode has played to its end or was stopped. A paused track has not ended. When a track has played to its end, the component sends `MUSIC_ENDED`. A track that ends right before another track covers it sends the event as well, once its end arrives, and never starts again. A track that loops does not send the event. A track that is stopped or replaced sends none either. The end of a video's sound and the end of an effect send nothing.

| Event | When | Payload |
|---|---|---|
| `MUSIC_ENDED` | A track has played to its end. Its source is the key of the track's mode | The number the driver gave the track |

## Interface

[`components/sounds.h`](../../../firmware/src/components/sounds.h) holds `Sounds`.
