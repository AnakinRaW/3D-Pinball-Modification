// The audio driver of firmware/src/drivers/audio.h for the tests that run on the computer: the same
// interface on the driver's real channels, which play a block whenever the test calls block()
// instead of in the Audio library's update. It records the file and the volume each channel got
// last, and the last sample of each channel's block.
#pragma once
#include "core/event-queue.h"
#include "drivers/sound-channel.h"
#include <memory>
#include <string.h>
#include <string>
#include <vector>

class AudioDriver {
public:
    static constexpr uint8_t CHANNELS = 4;

    using Position = SoundPosition;

    AudioDriver() {
        for (uint8_t c = 0; c < CHANNELS; ++c) {
            rings[c].resize(SoundChannel::RING_BYTES);
            channels[c] = std::make_unique<SoundChannel>(rings[c].data());
        }
    }

    bool begin() {
        begun = true;
        return true;
    }

    uint32_t play(uint8_t channel, const FileReader& file, bool loop = false) {
        if (channel < CHANNELS) {
            files[channel] = file.name();
        }
        return start(channel, Position(file, loop));
    }

    uint32_t play(uint8_t channel, const Position& from) {
        return from.empty() ? 0 : start(channel, from);
    }

    void stop(uint8_t channel) {
        if (!begun || channel >= CHANNELS) {
            return;
        }
        stops[channel] = stops[channel] + 1;
        channels[channel]->request(Position{}, 0);
    }

    void volume(uint8_t channel, float level) {
        if (channel >= CHANNELS) {
            return;
        }
        levels[channel] = level;
        channels[channel]->volume(level);
    }

    Position position(uint8_t channel) const {
        return begun && channel < CHANNELS ? channels[channel]->position() : Position{};
    }

    bool playing(uint8_t channel) const {
        return begun && channel < CHANNELS && channels[channel]->requested();
    }

    /// Plays one block on every channel.
    ///
    /// @return  The SOUND_ENDED events of the block, with the channel as their source.
    std::vector<DriverEvent> block() {
        std::vector<DriverEvent> ended;
        for (uint8_t c = 0; c < CHANNELS; ++c) {
            int16_t samples[SOUND_BLOCK_SAMPLES];
            SoundEnd end{};
            const bool sounding = channels[c]->play(samples, end);
            last[c] = sounding ? samples[SOUND_BLOCK_SAMPLES - 1] : 0;
            if (end.number != 0) {
                DriverEvent e{0, DriverEventType::SOUND_ENDED, c, 0};
                memcpy(&e.payload, &end, sizeof end);
                ended.push_back(e);
            }
        }
        return ended;
    }

    std::string files[CHANNELS];             // the file play() got last on each channel
    float levels[CHANNELS] = {1, 1, 1, 1};   // the volume volume() set last on each channel
    int16_t last[CHANNELS] = {};             // the last sample of each channel's block, 0 silent
    uint32_t stops[CHANNELS] = {};           // the calls of stop() on each channel

private:
    uint32_t start(uint8_t channel, const Position& from) {
        if (!begun || channel >= CHANNELS) {
            return 0;
        }
        count = count + 1;
        channels[channel]->request(from, count);
        return count;
    }

    std::vector<uint8_t> rings[CHANNELS];
    std::unique_ptr<SoundChannel> channels[CHANNELS];
    uint32_t count = 0;
    bool begun = false;
};
