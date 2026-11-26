// Plays sound files from the SD card on four channels at once through the DFR0954 amplifier. The
// Audio library computes the sound in an interrupt and hands it to the I²S2 port by DMA, so the
// main loop only starts and stops sounds and never waits for the card or the amplifier. The
// driver reports the end of every sound that plays out.
//
// Design: docs/firmware/drivers/audio.md.
#pragma once
#include "core/event-queue.h"
#include "drivers/driver-base.h"
#include "drivers/sound-channel.h"
#include "drivers/sound-mixer.h"
#include <AudioStream.h>
#include <mixer.h>

/// One channel of the audio driver as a stage of the Audio library: plays its sound block by
/// block and publishes SOUND_ENDED at the sound's end.
class ChannelPlayer : public AudioStream {
public:
    /// Creates a channel.
    ///
    /// @param index  The channel, 0 to 3, the source of its events.
    /// @param ring   SoundChannel::RING_BYTES on 32 bytes for the sound's bytes.
    ChannelPlayer(uint8_t index, uint8_t* ring);

    /// Plays one block and hands it to the mixer.
    ///
    /// @note  Runs in the update of the Audio library.
    void update() override;

    SoundChannel sound;
    EventQueue::Producer* out = nullptr;   // where SOUND_ENDED goes, nullptr before begin()

private:
    const uint8_t index;
    int16_t block[SOUND_BLOCK_SAMPLES] = {};
};

/// The mixer of the audio driver as a stage of the Audio library: adds the four channels.
class ChannelMixer : public AudioStream {
public:
    ChannelMixer();

    /// Adds the channels' blocks and hands the sum of the block before to the master gain.
    ///
    /// @note  Runs in the update of the Audio library.
    void update() override;

private:
    audio_block_t* inputs[SoundMixer::INPUTS] = {};
    SoundMixer mixer;
    int16_t silence[SOUND_BLOCK_SAMPLES] = {};   // takes the sum while no channel sent a block
};

class AudioDriver : public Driver {
public:
    static constexpr uint8_t CHANNELS = SoundMixer::INPUTS;
    static constexpr float MASTER_GAIN = 0.786f;   // audio model: g_cap

    /// A sound and where it stands, which play() can go on from.
    using Position = SoundPosition;

    /// Creates a driver that publishes into @p queue.
    ///
    /// The four channels share one set of rings in the PSRAM, which only the driver that begins
    /// uses.
    ///
    /// @param queue  The event queue that begin() attaches SOUND_ENDED to.
    /// @note         The driver lives as long as the firmware, as a static object, since the
    ///               Audio library keeps every stage it has seen.
    explicit AudioDriver(EventQueue& queue);

    /// Starts the I²S2 port and the Audio library's update, at priority 208.
    ///
    /// A second call returns what the first returned.
    ///
    /// @return  False when another audio driver has begun, which holds the I²S2 port, or when
    ///          the event queue takes no further queue.
    /// @note    The main task calls it after the start of FreeRTOS.
    bool begin() override;

    /// Starts a file on a channel from its beginning, in place of the channel's sound.
    ///
    /// The sound starts within one block once the card has brought the first piece of the file.
    /// A file with no name, a missing file and a file in another format end at once.
    ///
    /// @param channel  The channel, 0 to 3. Another one starts nothing.
    /// @param file     The sound's file.
    /// @param loop     Starts the sound over at its end, for as long as it plays.
    /// @return         The sound's number, which its SOUND_ENDED carries, below SoundEnd::NUMBERS.
    ///                 0 for a channel past 3 or before begin().
    /// @note           The main task calls it. It returns at once.
    uint32_t play(uint8_t channel, const FileReader& file, bool loop = false);

    /// Plays on from a position that position() gave, in place of the channel's sound.
    ///
    /// @param channel  The channel, 0 to 3. Another one starts nothing.
    /// @param from     The position. An empty one starts nothing.
    /// @return         The sound's number. 0 for an empty position, a channel past 3 and before
    ///                 begin().
    /// @note           The main task calls it. It returns at once.
    uint32_t play(uint8_t channel, const Position& from);

    /// Ends the sound on a channel for good, and frees the channel.
    ///
    /// The sound glides out across one block and publishes no SOUND_ENDED.
    ///
    /// @param channel  The channel, 0 to 3. Another one is left alone.
    /// @note           The main task calls it. It returns at once.
    void stop(uint8_t channel);

    /// Sets the volume of a channel, which every channel starts at 1.
    ///
    /// The channel glides to it across one block. The volume stays until the next call for that
    /// channel.
    ///
    /// @param channel  The channel, 0 to 3. Another one is left alone.
    /// @param level    Runs from 0 to 1. A value outside counts as the nearer end.
    /// @note           The main task calls it. It returns at once.
    void volume(uint8_t channel, float level);

    /// Gets the sound on a channel and where it stands.
    ///
    /// Right after play() it gives the position the sound starts from, and right after stop() an
    /// empty position, also before the sound has started or stopped.
    ///
    /// @param channel  The channel, 0 to 3.
    /// @return         An empty position for a free channel, a channel past 3 and before begin().
    /// @note           The main task calls it.
    Position position(uint8_t channel) const;

    /// Gets whether a channel plays a sound or has one requested.
    ///
    /// It turns true with the call of play().
    ///
    /// @param channel  The channel, 0 to 3. False for another one and before begin().
    /// @note           The main task calls it.
    bool playing(uint8_t channel) const;

    /// Gets the stage of the Audio library whose blocks go to the amplifier, behind the master
    /// gain.
    ///
    /// A further stage connected to it receives every block the amplifier gets.
    AudioStream& output() { return level; }

private:
    static constexpr uint32_t QUEUE_DEPTH = 8;   // the SOUND_ENDED events the queue holds

    /// Leaves a request for a sound under a new number.
    uint32_t start(uint8_t channel, const Position& from);

    ChannelPlayer players[CHANNELS];
    ChannelMixer mixer;
    AudioAmplifier level;                  // the master gain
    AudioConnection wires[CHANNELS + 1];   // the channels into the mixer, the mixer into level
    EventQueue& queue;
    DriverEvent storage[QUEUE_DEPTH] = {};
    uint32_t count = 0;   // the numbers play() has given
    bool begun = false;
    bool begunOk = false;
};
