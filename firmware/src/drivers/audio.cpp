#include "audio.h"
#include "core/clock.h"
#include "logging/debug-log.h"
#include "logging/debug-trace.h"
#include "logging/logger.h"
#include <output_i2s2.h>
#include <string.h>

constexpr size_t CACHE_LINE_BYTES = 32;   // the Cortex-M7's data cache line

/// The four rings in the PSRAM, which every audio driver's channels read into and only the one
/// that begins uses. A ring on 32 bytes takes the card's bytes without a copy.
EXTMEM static uint8_t rings[AudioDriver::CHANNELS][SoundChannel::RING_BYTES]
    __attribute__((aligned(CACHE_LINE_BYTES)));

/// The driver that has begun and holds the I²S2 port, nullptr before.
static AudioDriver* owner = nullptr;

ChannelPlayer::ChannelPlayer(uint8_t index, uint8_t* ring)
    : AudioStream(0, nullptr), sound(ring), index(index) {}

void ChannelPlayer::update() {
    SoundEnd ended{};
    const bool sounding = sound.play(block, ended);
    if (ended.number != 0 && out != nullptr) {
        out->publish<DriverEventType::SOUND_ENDED>(nowUs(), index, ended);
    }
    if (!sounding) {
        return;
    }
    audio_block_t* b = allocate();
    if (b == nullptr) {
        // Every block of the pool is taken, so this block of the sound is lost.
        DEBUG_TRACE("audio no block", index, 0);
        return;
    }
    memcpy(b->data, block, sizeof block);
    transmit(b);
    release(b);
}

ChannelMixer::ChannelMixer() : AudioStream(SoundMixer::INPUTS, inputs) {}

void ChannelMixer::update() {
    audio_block_t* in[SoundMixer::INPUTS] = {};
    const int16_t* samples[SoundMixer::INPUTS] = {};
    audio_block_t* sum = nullptr;
    for (uint8_t i = 0; i < SoundMixer::INPUTS; ++i) {
        // A channel's block goes to the mixer alone, so it comes writable without a copy, and the
        // first one takes the sum.
        in[i] = receiveWritable(i);
        if (in[i] != nullptr) {
            samples[i] = in[i]->data;
            sum = sum == nullptr ? in[i] : sum;
        }
    }
    int16_t(&into)[SOUND_BLOCK_SAMPLES] = sum != nullptr ? sum->data : silence;
    const bool sounding = mixer.mix(samples, into);
    bool allocated = false;
    if (sounding && sum == nullptr) {
        // The sum of the block before goes out while no channel sends one.
        sum = allocate();
        allocated = sum != nullptr;
        if (allocated) {
            memcpy(sum->data, silence, sizeof silence);
        }
    }
    if (sounding && sum != nullptr) {
        transmit(sum);
    }
    for (audio_block_t* b : in) {
        if (b != nullptr) {
            release(b);
        }
    }
    if (allocated) {
        release(sum);
    }
}

AudioDriver::AudioDriver(EventQueue& queue)
    : players{{0, rings[0]}, {1, rings[1]}, {2, rings[2]}, {3, rings[3]}}, queue(queue) {}

bool AudioDriver::begin() {
    if (begun) {
        return begunOk;
    }
    begun = true;
    if (owner != nullptr) {
        logWarning("audio", "another audio driver holds the I2S2 port");
        return false;
    }
    EventQueue::Producer* out = nullptr;
    if (!queue.attach(storage, QUEUE_DEPTH, out)) {
        logWarning("audio", "the event queue takes no further queue");
        return false;
    }
    owner = this;
    for (uint8_t i = 0; i < CHANNELS; ++i) {
        players[i].out = out;
        if (wires[i].connect(players[i], 0, mixer, i) != 0) {
            logWarning("audio", "channel %u does not connect to the mixer", i);
            return false;
        }
    }
    if (wires[CHANNELS].connect(mixer, 0, level, 0) != 0) {
        logWarning("audio", "the mixer does not connect to the master gain");
        return false;
    }
    level.gain(MASTER_GAIN);
    // The output starts the I²S2 port and the update when it is made. Made here, after the start
    // of FreeRTOS, its update keeps the priority 208 the library gives it, and it joins the
    // library's stages behind the channels, the mixer and the master gain, so a block passes
    // them all in one update. The amplifier plays the left slot, and both slots carry the sound.
    static AudioOutputI2S2 i2s;
    static AudioConnection left(level, 0, i2s, 0);
    static AudioConnection right(level, 0, i2s, 1);
    // The output holds the update, so the pool starts no timer of its own.
    AudioMemory(BLOCKS);
    begunOk = true;
    return true;
}

uint32_t AudioDriver::play(uint8_t channel, const FileReader& file, bool loop) {
    return start(channel, Position(file, loop));
}

uint32_t AudioDriver::play(uint8_t channel, const Position& from) {
    if (from.empty()) {
        DEBUG_LOG("audio: play() from an empty position");
        return 0;
    }
    return start(channel, from);
}

uint32_t AudioDriver::start(uint8_t channel, const Position& from) {
    if (!begunOk || channel >= CHANNELS) {
        DEBUG_LOG("audio: play() on channel %u refused, begun %d", channel, begunOk);
        return 0;
    }
    // The number fits the 24 bits of SOUND_ENDED's payload, and 0 names no sound.
    count = count + 1 == SoundEnd::NUMBERS ? 1 : count + 1;
    players[channel].sound.request(from, count);
    return count;
}

void AudioDriver::stop(uint8_t channel) {
    if (!begunOk || channel >= CHANNELS) {
        DEBUG_LOG("audio: stop() on channel %u refused, begun %d", channel, begunOk);
        return;
    }
    players[channel].sound.request(Position{}, 0);
}

void AudioDriver::volume(uint8_t channel, float levelWanted) {
    if (channel >= CHANNELS) {
        DEBUG_LOG("audio: volume() on channel %u refused", channel);
        return;
    }
    players[channel].sound.volume(levelWanted);
}

AudioDriver::Position AudioDriver::position(uint8_t channel) const {
    if (!begunOk || channel >= CHANNELS) {
        return {};
    }
    return players[channel].sound.position();
}

bool AudioDriver::playing(uint8_t channel) const {
    return begunOk && channel < CHANNELS && players[channel].sound.requested();
}
