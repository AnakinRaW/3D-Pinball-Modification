#include "sound-mixer.h"

constexpr float RATE_HZ = 44100;   // audio model: f_s
constexpr float MS_PER_S = 1000;
constexpr float HALF = 0.5f;                              // rounds a sample to the nearest step
constexpr int32_t LOWEST = -SoundMixer::FULL_SCALE - 1;   // the lowest 16-bit sample
// The gain comes back up linearly, from 0 to 1 within RELEASE_MS.
constexpr float RELEASE_PER_BLOCK =
    float(SOUND_BLOCK_SAMPLES) / (RATE_HZ * SoundMixer::RELEASE_MS / MS_PER_S);

bool SoundMixer::mix(const int16_t* const (&in)[INPUTS], int16_t (&out)[SOUND_BLOCK_SAMPLES]) {
    // The new block is summed first, since out may be one of the inputs.
    const uint8_t fresh = uint8_t(held ^ 1);
    int32_t* sum = sums[fresh];
    bool any = false;
    int32_t highest = 0;
    int32_t lowest = 0;
    for (uint32_t i = 0; i < SOUND_BLOCK_SAMPLES; ++i) {
        int32_t s = 0;
        for (const int16_t* block : in) {
            if (block != nullptr) {
                s = s + block[i];
            }
        }
        sum[i] = s;
        highest = s > highest ? s : highest;
        lowest = s < lowest ? s : lowest;
    }
    for (const int16_t* block : in) {
        any = any || block != nullptr;
    }
    sounding[fresh] = any;
    float need = highest > FULL_SCALE ? float(FULL_SCALE) / float(highest) : 1;
    if (lowest < LOWEST) {
        const float down = float(LOWEST) / float(lowest);
        need = down < need ? down : need;
    }
    needs[fresh] = need;

    // The gain ends this block no higher than the block before and the new block need, and comes
    // back up by at most a step. Both ends of the glide stay within what the block before needs,
    // so every sample between them does as well.
    const float from = ending;
    float to = from + RELEASE_PER_BLOCK;
    to = needs[held] < to ? needs[held] : to;
    to = needs[fresh] < to ? needs[fresh] : to;
    const bool sound = sounding[held];
    if (sound) {
        const int32_t* before = sums[held];
        for (uint32_t i = 0; i < SOUND_BLOCK_SAMPLES; ++i) {
            const float g = from + (to - from) * float(i + 1) / float(SOUND_BLOCK_SAMPLES);
            const float scaled = float(before[i]) * g;
            const int32_t rounded = int32_t(scaled < 0 ? scaled - HALF : scaled + HALF);
            // Rounding alone can reach one step past full scale.
            out[i] = int16_t(rounded > FULL_SCALE ? FULL_SCALE
                             : rounded < LOWEST   ? LOWEST
                                                  : rounded);
        }
    }
    ending = to;
    held = fresh;
    return sound;
}
