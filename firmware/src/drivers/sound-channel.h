// Plays one sound file from the SD card block by block, for one channel of the audio driver. The
// file's samples come out of a ring that a file channel keeps filled from the card, so a block
// never waits for the card. A sound starts, stops and changes its volume with a glide across one
// block, so no change clicks. A sound remembers where it stands and can go on from there later.
//
// Design: docs/firmware/drivers/audio.md.
#pragma once
#include "drivers/storage-service.h"
#include "io/file-channel.h"
#include <atomic>
#include <stdint.h>

/// The samples of one block, which the Audio library asks every channel for at a time.
constexpr uint32_t SOUND_BLOCK_SAMPLES = 128;   // audio model: n_block

/// A sound and where it stands, which a channel can go on from.
class SoundPosition {
public:
    /// Creates an empty position, which plays nothing.
    SoundPosition() = default;

    /// Creates the position at the beginning of a file.
    ///
    /// @param file  The sound's file.
    /// @param loop  Starts the sound over at its end.
    SoundPosition(const FileReader& file, bool loop) : file(file), loop(loop) {}

    /// Gets whether no sound stands at the position.
    bool empty() const { return file.name()[0] == '\0'; }

    /// Gets how many samples of the sound have played.
    ///
    /// It is 0 until the channel has found the sound's samples. A sound that loops counts from
    /// the start of the loop that plays.
    uint32_t played() const { return (data - left) / SAMPLE_BYTES; }

private:
    friend class SoundChannel;

    static constexpr uint32_t SAMPLE_BYTES = 2;   // audio.md: 16-bit PCM, mono

    FileReader file;       // the sound's file, one with no name for none
    bool loop = false;     // the sound starts over at its end
    uint32_t offset = 0;   // the byte of the file the next sample stands at, 0 before the samples
    uint32_t left = 0;     // the bytes of the data chunk not played yet
    uint32_t size = 0;     // the bytes of the file, from its RIFF header
    uint32_t data = 0;     // the bytes of the data chunk
};

/// One channel of the audio driver: plays a sound file out of its ring, block by block.
///
/// A file is a WAV with 16-bit PCM at 44.1 kHz, mono. A file in any other format, a missing file
/// and a failed read end the sound.
///
/// @note  The main task calls request(), volume(), requested() and position(). The Audio library's
///        update calls play().
class SoundChannel {
public:
    static constexpr uint32_t CHUNK_BYTES = 8192;   // audio model: chunk
    static constexpr uint8_t CHUNKS = 24;           // audio model: n_chunks
    static constexpr uint32_t RING_BYTES = CHUNK_BYTES * CHUNKS;

    /// Creates a channel that reads into a ring.
    ///
    /// @param ring  RING_BYTES for the sound's bytes, on 32 bytes, so the card's bytes land in it
    ///              without a copy.
    explicit SoundChannel(uint8_t* ring);

    /// Leaves a request for a sound, which the next block takes.
    ///
    /// A later request before that block replaces it. A sound playing glides out across one block
    /// before the request takes effect. The cut-off sound reports no end.
    ///
    /// @param from    The sound and where it starts. An empty position asks for silence.
    /// @param number  The sound's number, which play() reports at its end. 0 asks for silence.
    void request(const SoundPosition& from, uint32_t number);

    /// Gets whether the channel plays a sound or has one requested.
    bool requested() const;

    /// Gets the sound playing and where it stands, an empty position for none.
    ///
    /// A request not taken yet gives the position it starts from, and a request for silence an
    /// empty position, so the position always belongs to the last request.
    SoundPosition position() const;

    /// Sets the volume the channel plays at.
    ///
    /// The channel glides to it across the next block that holds samples. The volume stays until
    /// the next call.
    ///
    /// @param level  Runs from 0 to 1. A value outside counts as the nearer end.
    void volume(float level);

    /// Plays the next block.
    ///
    /// Takes the last request first. Samples the ring does not hold yet stay 0, and the sound goes
    /// on from the next of its samples in a later block.
    ///
    /// @param block  Takes the block's samples.
    /// @param ended  Set to the number of a sound that has played to its end or whose file could
    ///               not be read in this block, 0 for none.
    /// @return       Whether @p block holds sound. False leaves @p block undefined.
    /// @note         Runs in the update of the Audio library and never waits.
    bool play(int16_t (&block)[SOUND_BLOCK_SAMPLES], uint32_t& ended);

private:
    static constexpr uint32_t SECTOR_BYTES =
        512;                                    // storage.md: a read from a sector needs no copy
    static constexpr uint8_t HEAD_BYTES = 16;   // the longest header piece, the fmt chunk's body

    /// What the channel reads from the stream next.
    enum class Phase : uint8_t {
        HEADER,    // the RIFF header and the chunks ahead of the samples
        SAMPLES,   // the data chunk
        TRAILER,   // the bytes behind the data chunk of a sound that loops, up to the file's end
    };

    /// The piece of the headers the channel reads next.
    enum class Piece : uint8_t {
        RIFF,     // the file's header: RIFF, its size and WAVE
        CHUNK,    // a chunk's header: its id and its size
        FORMAT,   // the fmt chunk's body
    };

    /// A request: the main task fills the slot the request count does not name, then counts it.
    struct Request {
        SoundPosition from;
        uint32_t number;
    };

    /// Starts a request's sound, or silence.
    void start(const Request& r);

    /// Takes samples into a block, from where the sound stands.
    ///
    /// @param block  Takes the samples. Those the ring does not hold yet stay as they are.
    /// @return       The samples it took.
    uint32_t fill(int16_t (&block)[SOUND_BLOCK_SAMPLES]);

    /// Reads the header piece that has arrived.
    ///
    /// @return  False for a file this channel cannot play.
    bool readPiece();

    /// Sets the next header piece to read.
    void expect(Piece piece);

    /// Ends the sound and frees the channel.
    void end();

    /// Writes where the sound stands into the position that position() reads.
    void publish();

    FileChannel<CHUNKS> stream;
    Request requests[2];
    std::atomic<uint32_t> made{0};   // the requests made; its lowest bit names the slot of the last
    std::atomic<uint32_t> taken{0};     // the requests play() has taken
    std::atomic<uint32_t> number{0};    // the number of the sound playing, 0 for none
    std::atomic<float> target{1};       // the volume asked for, 0 to 1
    float level = 0;                    // the volume the last block ended at
    SoundPosition now;                  // what position() reads
    std::atomic<uint32_t> version{0};   // odd while play() writes now

    // what play() alone keeps about the sound playing
    SoundPosition sound;   // the sound with where it stands
    Phase phase = Phase::HEADER;
    uint32_t at = 0;     // the byte of the file the stream gives next
    uint32_t skip = 0;   // bytes the stream still drops ahead of what comes next
    Piece piece = Piece::RIFF;
    uint8_t head[HEAD_BYTES] = {};
    uint8_t headWanted = 0;   // the bytes of the piece being read
    uint8_t headAt = 0;       // the bytes of it read so far
    bool format = false;      // the fmt chunk has named a format this channel plays
};
