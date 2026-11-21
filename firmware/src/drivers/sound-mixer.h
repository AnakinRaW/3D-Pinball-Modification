// Adds the blocks of the audio driver's four channels into one. Where the sum would pass full
// scale, the mixer turns it down ahead of time and brings it back up slowly afterwards, so loud
// sounds together neither clip nor pump.
//
// Design: docs/firmware/drivers/audio.md.
#pragma once
#include "drivers/sound-channel.h"
#include <stdint.h>

/// Adds blocks of samples and keeps the sum within full scale.
///
/// The sum leaves one block late. That block lets the gain glide down before a loud block
/// arrives, so the gain never jumps.
///
/// @note  Runs in the update of the Audio library.
class SoundMixer {
public:
    static constexpr uint8_t INPUTS = 4;           // audio model: n_streams
    static constexpr float RELEASE_MS = 100;       // audio.md: the gain's way back up to 1
    static constexpr int32_t FULL_SCALE = 32767;   // the largest 16-bit sample

    /// Adds one block of every input and gives the sum of the block before.
    ///
    /// The block given glides from the gain the last block ended at toward the gain the block
    /// after it needs, so no sample passes full scale.
    ///
    /// @param in   The inputs' blocks. nullptr counts as silence. @p out may be one of them.
    /// @param out  Takes the sum of the blocks of the call before.
    /// @return     Whether @p out holds sound. False leaves @p out undefined.
    bool mix(const int16_t* const (&in)[INPUTS], int16_t (&out)[SOUND_BLOCK_SAMPLES]);

    /// Gets the gain the last block given ended at, 1 while nothing passes full scale.
    float gain() const { return ending; }

private:
    int32_t sums[2][SOUND_BLOCK_SAMPLES] = {};   // the new block's sum and the one before
    float needs[2] = {1, 1};   // the largest gain each sum takes without passing full scale
    bool sounding[2] = {};     // each sum holds an input block
    uint8_t held = 0;          // which of sums holds the block before
    float ending = 1;          // the gain the last block given ended at
};
