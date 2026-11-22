#include "sound-channel.h"
#include "logging/debug-trace.h"
#include <string.h>

// the WAV format the channel plays
constexpr uint16_t FORMAT_PCM = 1;        // the fmt chunk's code for integer PCM
constexpr uint16_t CHANNELS = 1;          // audio.md: mono
constexpr uint32_t RATE_HZ = 44100;       // audio model: f_s
constexpr uint16_t BITS = 16;             // audio.md: 16-bit PCM
constexpr uint8_t RIFF_BYTES = 12;        // "RIFF", the size of the rest, "WAVE"
constexpr uint8_t CHUNK_HEAD_BYTES = 8;   // a chunk's id and its size
constexpr uint8_t FORMAT_BYTES = 16;      // the fmt chunk's body for PCM
constexpr uint32_t RIFF_HEAD_BYTES = 8;   // "RIFF" and the size, which the size leaves out
constexpr uint32_t SAMPLE_BYTES = 2;      // audio.md: 16-bit PCM, mono
constexpr float HALF = 0.5f;              // rounds a sample to the nearest step
constexpr uint32_t BYTE_BITS = 8;

// where the fields stand in a header piece, which the WAV format numbers from the piece's start
constexpr size_t ID_BYTES = 4;             // a chunk's id, such as "RIFF"
constexpr size_t SIZE_AT = 4;              // the size behind a chunk's id
constexpr size_t FORM_AT = 8;              // "WAVE" in the RIFF header
constexpr size_t FORMAT_CHANNELS_AT = 2;   // in the fmt chunk's body
constexpr size_t FORMAT_RATE_AT = 4;
constexpr size_t FORMAT_BITS_AT = 14;

/// Gets a 16-bit little-endian number.
static uint16_t le16(const uint8_t* p) {
    return uint16_t(p[0] | p[1] << BYTE_BITS);
}

/// Gets a 32-bit little-endian number.
static uint32_t le32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << BYTE_BITS | uint32_t(p[2]) << 2 * BYTE_BITS |
           uint32_t(p[3]) << 3 * BYTE_BITS;
}

SoundChannel::SoundChannel(uint8_t* ring) : stream(ring, CHUNK_BYTES, FilePriority::HIGHEST) {}

void SoundChannel::request(const SoundPosition& from, uint32_t numberOfSound) {
    const uint32_t count = made.load(std::memory_order_relaxed);
    // The update reads only the slot the count names, so the other slot is free to fill.
    requests[(count + 1) & 1] = {from, numberOfSound};
    made.store(count + 1, std::memory_order_release);
}

bool SoundChannel::requested() const {
    const uint32_t count = made.load(std::memory_order_acquire);
    if (count != taken.load(std::memory_order_acquire)) {
        return requests[count & 1].number != 0;
    }
    return number.load(std::memory_order_relaxed) != 0;
}

SoundPosition SoundChannel::position() const {
    // The main task alone fills the slots, so a slot it filled stays as it is while it reads it.
    const uint32_t count = made.load(std::memory_order_acquire);
    if (count != taken.load(std::memory_order_acquire)) {
        return requests[count & 1].from;
    }
    SoundPosition copy;
    uint32_t before = 0;
    uint32_t after = 0;
    // The update may write the position meanwhile. It runs to its end before this task goes on,
    // so a second pass reads a whole position.
    do {
        before = version.load(std::memory_order_acquire);
        copy = now;
        std::atomic_thread_fence(std::memory_order_acquire);
        after = version.load(std::memory_order_relaxed);
    } while ((before & 1) != 0 || before != after);
    return copy;
}

void SoundChannel::volume(float levelWanted) {
    target.store(levelWanted < 0   ? 0
                 : levelWanted > 1 ? 1
                                   : levelWanted,
                 std::memory_order_relaxed);
}

bool SoundChannel::play(int16_t (&block)[SOUND_BLOCK_SAMPLES], uint32_t& ended) {
    ended = 0;
    const uint32_t count = made.load(std::memory_order_acquire);
    bool cutting = false;
    if (count != taken.load(std::memory_order_relaxed)) {
        if (number.load(std::memory_order_relaxed) != 0 && level > 0) {
            // The sound playing glides out across this block, and the request takes effect in
            // the next one, so the cut does not click.
            cutting = true;
        } else {
            taken.store(count, std::memory_order_release);
            start(requests[count & 1]);
        }
    }
    const uint32_t playing = number.load(std::memory_order_relaxed);
    if (playing == 0) {
        return false;
    }
    memset(block, 0, sizeof block);
    const uint32_t got = fill(block);
    if (number.load(std::memory_order_relaxed) == 0 && !cutting) {
        ended = playing;
    }
    if (got > 0) {
        // The volume glides from where the last block ended to where it is asked to be. It moves
        // only in a block with samples, so a sound waiting for the card starts with the glide.
        const float from = level;
        const float to = cutting ? 0 : target.load(std::memory_order_relaxed);
        for (uint32_t i = 0; i < SOUND_BLOCK_SAMPLES; ++i) {
            const float gain = from + (to - from) * float(i + 1) / float(SOUND_BLOCK_SAMPLES);
            const float scaled = float(block[i]) * gain;
            block[i] = int16_t(scaled < 0 ? scaled - HALF : scaled + HALF);
        }
        level = to;
    }
    if (cutting) {
        // A sound waiting for the card has no samples to glide across. The request takes effect
        // in the next block all the same.
        level = 0;
    }
    publish();
    return got > 0;
}

void SoundChannel::start(const Request& r) {
    level = 0;
    skip = 0;
    number.store(r.number, std::memory_order_relaxed);
    if (r.number == 0) {
        end();
        publish();
        return;
    }
    // A file with no name reads nothing, so its sound ends in this block.
    sound = r.from;
    if (sound.offset == 0) {
        phase = Phase::HEADER;
        sound.left = 0;
        sound.data = 0;
        at = 0;
        expect(Piece::RIFF);
        stream.start(sound.file, sound.loop, 0);
    } else {
        // A read from a sector takes the card's bytes without a copy, so the stream starts at the
        // sector and drops the bytes ahead of the sample.
        phase = sound.left > 0 ? Phase::SAMPLES : Phase::TRAILER;
        skip = sound.offset % SECTOR_BYTES;
        at = sound.offset - skip;
        stream.start(sound.file, sound.loop, at);
    }
    publish();
}

uint32_t SoundChannel::fill(int16_t (&block)[SOUND_BLOCK_SAMPLES]) {
    uint32_t got = 0;
    while (got < SOUND_BLOCK_SAMPLES) {
        size_t n = 0;
        if (skip > 0) {
            n = stream.take(nullptr, skip);
            skip = skip - uint32_t(n);
        } else if (phase == Phase::HEADER) {
            n = stream.take(head + headAt, size_t(headWanted - headAt));
            headAt = uint8_t(headAt + n);
            if (headAt == headWanted) {
                at = at + uint32_t(n);
                if (!readPiece()) {
                    DEBUG_TRACE("sound format", sound.size, at);
                    end();
                    return got;
                }
                continue;
            }
        } else if (phase == Phase::SAMPLES) {
            const uint32_t room = (SOUND_BLOCK_SAMPLES - got) * SAMPLE_BYTES;
            const size_t want = room < sound.left ? room : sound.left;
            n = stream.take(reinterpret_cast<uint8_t*>(block) + got * SAMPLE_BYTES, want);
            got = got + uint32_t(n) / SAMPLE_BYTES;
            sound.left = sound.left - uint32_t(n);
            if (sound.left == 0) {
                at = at + uint32_t(n);
                if (!sound.loop) {
                    end();
                    return got;
                }
                phase = Phase::TRAILER;
                continue;
            }
        } else {
            n = sound.size > at ? stream.take(nullptr, sound.size - at) : 0;
            if (at + n >= sound.size) {
                // The stream starts the file over behind its last byte, and the headers come again.
                at = 0;
                phase = Phase::HEADER;
                expect(Piece::RIFF);
                continue;
            }
        }
        at = at + uint32_t(n);
        if (n == 0) {
            if (stream.ended()) {
                // The file has run out ahead of its end, or a read failed.
                DEBUG_TRACE("sound cut short", sound.left, at);
                end();
            }
            return got;
        }
    }
    return got;
}

bool SoundChannel::readPiece() {
    switch (piece) {
    case Piece::RIFF:
        if (memcmp(head, "RIFF", ID_BYTES) != 0 || memcmp(head + FORM_AT, "WAVE", ID_BYTES) != 0) {
            return false;
        }
        sound.size = le32(head + SIZE_AT) + RIFF_HEAD_BYTES;
        format = false;
        expect(Piece::CHUNK);
        return true;
    case Piece::CHUNK: {
        const uint32_t bytes = le32(head + SIZE_AT);
        if (memcmp(head, "fmt ", ID_BYTES) == 0) {
            if (bytes < FORMAT_BYTES) {
                return false;
            }
            // The fmt chunk's bytes past the PCM fields and a pad byte behind an odd size.
            skip = bytes - FORMAT_BYTES + (bytes & 1);
            expect(Piece::FORMAT);
            return true;
        }
        if (memcmp(head, "data", ID_BYTES) == 0) {
            if (!format) {
                return false;
            }
            sound.data = bytes;
            sound.left = bytes;
            phase = Phase::SAMPLES;
            return true;
        }
        skip = bytes + (bytes & 1);
        expect(Piece::CHUNK);
        return true;
    }
    case Piece::FORMAT:
        format = le16(head) == FORMAT_PCM && le16(head + FORMAT_CHANNELS_AT) == CHANNELS &&
                 le32(head + FORMAT_RATE_AT) == RATE_HZ && le16(head + FORMAT_BITS_AT) == BITS;
        expect(Piece::CHUNK);
        return format;
    }
    return false;
}

void SoundChannel::expect(Piece next) {
    piece = next;
    headAt = 0;
    headWanted = next == Piece::RIFF    ? RIFF_BYTES
                 : next == Piece::CHUNK ? CHUNK_HEAD_BYTES
                                        : FORMAT_BYTES;
}

void SoundChannel::end() {
    number.store(0, std::memory_order_relaxed);
    sound = SoundPosition{};
    phase = Phase::HEADER;
    skip = 0;
    stream.stop();
}

void SoundChannel::publish() {
    const uint32_t v = version.load(std::memory_order_relaxed);
    version.store(v + 1, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
    now = sound;
    // Ahead of the samples a position starts the sound from its beginning.
    now.offset = phase == Phase::HEADER ? 0 : at + skip;
    std::atomic_thread_fence(std::memory_order_release);
    version.store(v + 2, std::memory_order_release);
}
