# Audio driver

The driver plays sound files through the amplifier that [`docs/parts/audio/design.md`](../../parts/audio/design.md) describes. It builds on PJRC's Audio library, which computes the sound in a separate interrupt and hands it to the I²S2 port by DMA. The main loop therefore only starts and stops sounds. The sound files come from the SD card through the [storage driver](storage.md).

[`firmware/src/drivers/audio.h`](../../../firmware/src/drivers/audio.h) holds the driver's interface.

## Channels

The driver plays up to four sounds at once, one on each of its four channels. The caller picks the channel for every sound. A channel plays one sound from its beginning, either once or in a loop.

| Call | The sound | The channel afterwards |
|---|---|---|
| `play()` | starts from its beginning or from a position and replaces the sound on the channel | taken |
| `stop()` | ends for good | free |

Both calls only leave a request for the channel, so they return at once. The main task makes every call.

`position()` gets the sound on a channel and where it stands. A later `play()` from that position goes on there, also after the channel has played something else meanwhile. A free channel gives an empty position, which plays nothing. `played()` returns how many samples of a position's sound have played so far.

`playing()` tells whether a channel plays a sound. It turns true with the call of `play()`.

`volume()` sets the volume of a channel from 0 to 1. Every channel starts at 1. The volume stays until the next `volume()` for that channel.

## Audio files

A sound file is a WAV with 16-bit PCM at 44.1 kHz, mono. PJRC's Audio library only plays at 44.1 kHz and the machine only has one speaker. [`media.md`](../../../assets/media.md) shows how FFmpeg makes such a file. A file in another format ends at once, as a missing file does.

## Audio chain

### Stages

PJRC's Audio library builds the sound in a chain of stages. Each stage is a class derived from PJRC's `AudioStream`. The driver wires its four channels into its own mixer, then into PJRC's master gain and the I²S2 output. Every 2.90 ms, PJRC's library asks every stage for a block of the next 128 samples, in the order the stages were made.

`begin()` reserves 6 blocks, one for each of the four channels and two for the output. The output sends one block to the amplifier while the next one waits.

[`general-design.md`](../general-design.md#interrupts) lists the priority of the interrupt in which PJRC's library computes the blocks. The library sets it to 208 when its output starts, and starting FreeRTOS resets every priority to 128. `begin()` therefore makes the output, after the start of FreeRTOS. The output then joins the chain behind the channels, the mixer and the master gain, so a block passes every stage in one round.

The Audio library keeps every stage it has seen for good. The driver is therefore a static object. Only one driver holds the I²S2 port, and the `begin()` of a second one returns false.

### Channel player

`ChannelPlayer` is the stage of one channel. It reads the channel's sound file from the card and hands its samples on to the mixer. When PJRC's library asks for the next block, the channel first takes the caller's last request, such as `play()` or `stop()`. Requests therefore take effect within one block (2.9 ms). A new sound starts once the file channel has read the first piece of its file from the card.

The channel fills its block through a [file channel](../file-channel.md), a buffer that refills itself from the card. Its ring in the PSRAM holds 24 chunks of 8192 bytes. The file channel reads with the priority `HIGHEST`. Samples the file channel could not bring in time play as silence, and the sound goes on from its next sample once they arrive.

A sound that loops reads its file over and over without a pause. At the end of the samples the channel skips the rest of the file and the headers ahead of the samples, while the ring already holds the beginning of the next pass.

### Volume

`volume()` sets the volume level at which each channel plays. The mixer adds the four channels. If their sum would overdrive the output, the mixer turns it down for that moment and brings it back up to full within 100 ms.

The mixer sends each sum one block late. The extra block lets the mixer's gain glide down across the block before a loud one, so the gain never jumps.

When a sound starts, stops or changes its volume, the driver moves to the new volume gradually over one block of 2.9 ms. A sudden jump would be heard as a click. A sound that `play()` or `stop()` cuts off glides out across one block, and the request takes effect in the next.

The master gain after the mixer keeps the speaker within its rating. [`design.md`](../../parts/audio/design.md#level) derives that limit.

## Event

| Field | Content |
|---|---|
| Type | `DriverEventType::SOUND_ENDED` |
| Source | The channel, `0` to `3` |
| Timestamp | `nowUs()` when the driver found the sound finished |
| Payload | The number `play()` returned |

The driver publishes `SOUND_ENDED` once a sound has played to its end or its file could not be read. A sound that `play()` or `stop()` cuts off publishes nothing. A sound that loops ends only when its file is missing or a read fails. Without a card every sound ends at once.

## Limitations

Playback depends on the card's read speed. A channel's ring holds more than two seconds of sound, which carries it through the longest stall of the card measured, 953 ms.

A sound played on from a position repeats up to one block, since the sound plays the block that glides it out after `position()` has noted where it stood.

A loop starts its next pass where the RIFF header puts the end of the file. A file whose RIFF header names a size other than its own ends after its first pass.

## Existing libraries

The driver uses the output and the master gain of PJRC's Audio library unchanged. It cannot use any of the existing SD players, because each of them reads the card directly. Only the storage task may access the card, as the [storage driver](storage.md#requirements-and-use-cases) requires.

`AudioPlaySdWav` and `AudioPlaySdRaw` read the card inside the audio interrupt. h4yn0nnym0u5e's [buffered players](https://github.com/PaulStoffregen/Audio/pull/483) and newdigate's [TeensyVariablePlayback](https://github.com/newdigate/teensy-variable-playback) open and read the file in the main loop when a sound starts. The main loop would then wait for the card at every new sound.

`AudioMixer4` clips when the channels together exceed full scale, as [PJRC's documentation](https://www.pjrc.com/teensy/gui/?info=AudioMixer4) states. It also switches a channel to a new gain at once, without a ramp. The driver's own mixer turns the sum down in advance and ramps every change of volume.

## Device faults

The driver notes no fault. The amplifier gives the Teensy no feedback, so the driver cannot tell whether it plays. A failing card shows up as the storage driver's fault.
