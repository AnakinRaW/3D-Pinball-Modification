# Audio driver

The driver plays sound files through the amplifier that [`docs/parts/audio/design.md`](../../parts/audio/design.md) describes. It builds on PJRC's Audio library, which computes the sound in a separate interrupt and hands it to the I²S2 port by DMA. The main loop therefore only starts and stops sounds. The sound files come from the SD card through the [storage driver](storage.md).

## Channels

The driver plays up to four sounds at once, one on each of its four channels. Each channel is a `ChannelPlayer`, a stage of PJRC's audio chain that plays one file. The caller picks the channel for every sound. A channel plays one sound from its beginning, either once or in a loop.

| Call | The sound | The channel afterwards |
|---|---|---|
| `play()` | starts from its beginning or from a position and replaces the sound on the channel | taken |
| `stop()` | ends for good | free |

Both calls only leave a request for the channel, so they return at once.

`position()` gets the sound on a channel and where it stands. A later `play()` from that position goes on there, also after the channel has played something else meanwhile. A free channel gives an empty position, which plays nothing.

`playing()` tells whether a channel plays a sound. It turns true with the call of `play()`.

`volume()` sets the volume of a channel from 0 to 1. Every channel starts at 1. The volume stays until the next `volume()` for that channel.

## Audio files

A sound file is a WAV with 16-bit PCM at 44.1 kHz, mono. PJRC's Audio library only plays at 44.1 kHz and the machine only has one speaker. [`media.md`](../../../assets/media.md) shows how FFmpeg makes such a file.

## Audio chain

### Stages

PJRC's Audio library builds the sound in a chain of stages. Each stage is a class derived from PJRC's `AudioStream`. This driver wires its four channels into its own mixer, then into PJRC's master gain and the I²S2 output. Every 2.90 ms, PJRC's library asks every stage for a block of the next 128 samples.

`begin()` reserves 6 blocks, one for each of the four channels and two for the output. The output sends one block to the amplifier while the next one waits.

[`general-design.md`](../general-design.md#interrupts) lists the priority of the interrupt in which PJRC's library computes the blocks. 

Because starting FreeRTOS resets that priority, `begin()` sets it back to 208.

### Channel player

`ChannelPlayer` is our own class derived from `AudioStream`. It reads one channel's sound file from the card and hands its samples on to the mixer. When PJRC's library asks for the next block, the channel first takes the caller's last request, e.g., `play()` or `stop()`. Requests therefore take effect within one block (2.9 ms). A new sound starts once the file channel has read the first piece of its file from the card.

The channel fills its block through a [file channel](../file-channel.md), a buffer that refills itself from the card. The file channel reads with high priority. A block the file channel could not fill in time plays silence.

### Volume

`volume()` sets the volume level at which each channel plays. The mixer adds the four channels. If their sum would overdrive the output, the mixer turns it down for that moment.

When a sound starts, stops or changes its volume, the driver moves to the new volume gradually over 2.9 ms. A sudden jump would be heard as a click.

The master gain after the mixer keeps the speaker within its rating. [`design.md`](../../parts/audio/design.md#level) derives that limit.

## Event

| Field | Content |
|---|---|
| Type | `DriverEventType::SoundEnded` |
| Source | The channel, `0` to `3` |
| Timestamp | `nowUs()` when the driver found the sound finished |
| Payload | The number `play()` returned |

The driver publishes `SoundEnded` once a sound has played to its end or a read has failed. A sound that `play()` or `stop()` cuts off publishes nothing. A sound that loops ends only when its file is missing or a read fails. Without a card every sound ends at once.

## Limitations

Playback depends on the card's read speed. Using the file channel's ring buffer only bridges short delays.

## Existing libraries

The driver uses the output and the master gain of PJRC's Audio library unchanged. It cannot use any of the existing SD players, because each of them reads the card directly. Only the storage task may access the card, as the [storage driver](storage.md#requirements-and-use-cases) requires.

`AudioPlaySdWav` and `AudioPlaySdRaw` read the card inside the audio interrupt. h4yn0nnym0u5e's [buffered players](https://github.com/PaulStoffregen/Audio/pull/483) and newdigate's [TeensyVariablePlayback](https://github.com/newdigate/teensy-variable-playback) open and read the file in the main loop when a sound starts. The main loop would then wait for the card at every new sound.

`AudioMixer4` clips when the channels together exceed full scale, as [PJRC's documentation](https://www.pjrc.com/teensy/gui/?info=AudioMixer4) states. It also switches a channel to a new gain at once, without a ramp. The driver's own mixer turns the sum down in advance and ramps every change of volume.

## Device faults

The driver notes no fault. The amplifier gives the Teensy no feedback, so the driver cannot tell whether it plays. A failing card shows up as the storage driver's fault.

## Driver

```cpp
class AudioDriver : public Driver {
public:
    static constexpr uint8_t kChannels = 4;

    // reserves PJRC's audio blocks with AudioMemory(), attaches the queue, sets up
    // the channels and the level, and sets the update interrupt back to priority 208 after the
    // start of FreeRTOS
    bool begin() override;

    // a sound and where it stands, which play() goes on from
    class Position {
    public:
        bool empty() const;   // gets whether no sound stands at it
    };

    uint32_t play(uint8_t channel, const FileReader& file, bool loop = false);   // starts file on channel from its beginning, once or in a loop; returns its number at once
    uint32_t play(uint8_t channel, const Position& from);                        // plays on from a position that position() gave; returns its number at once, 0 for an empty position
    void     stop(uint8_t channel);                                              // ends the sound on channel for good and frees the channel; returns at once
    void     volume(uint8_t channel, float level);                               // sets the volume of channel from 0 to 1, a value outside counting as the nearer end; returns at once
    Position position(uint8_t channel) const;                                    // gets the sound on channel and where it stands, an empty position for a free channel
    bool     playing(uint8_t channel) const;                                     // gets whether channel plays a sound or has one requested
};
```
