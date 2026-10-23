class AudioDriver : public Driver {
public:
    static constexpr uint8_t kChannels = 4;

    // a sound and where it stands, which play() goes on from
    class Position {
    public:
        bool     empty() const { return !file.name()[0]; }   // gets whether no sound stands at it
        uint32_t played() const { return (data - left) / 2; }   // gets how many samples of the sound have played, two bytes each

    private:
        friend class AudioDriver;
        friend class ChannelPlayer;

        FileReader file;            // the sound's file, one with no name for none
        bool       loop   = false;
        uint32_t   offset = 0;      // the byte of the file the next sample stands at, 0 to start with the WAV headers
        uint32_t   left   = 0;      // bytes of the data chunk not played yet
        uint32_t   size   = 0;      // the file's bytes, from its RIFF header
        uint32_t   data   = 0;      // the bytes of the data chunk
    };

    // reserves PJRC's audio blocks with AudioMemory(kBlocks), attaches the queue, sets up
    // the channels and the level, and sets the update interrupt back to priority 208 after the
    // kernel's start
    bool begin() override;

    uint32_t play(uint8_t channel, const FileReader& file, bool loop = false);   // starts file on channel from its beginning, once or in a loop; returns its number at once
    uint32_t play(uint8_t channel, const Position& from);                        // plays on from a position that position() gave; returns its number at once, 0 for an empty position
    void     stop(uint8_t channel);                                              // ends the sound on channel for good and frees the channel; returns at once
    void     volume(uint8_t channel, float level);                               // sets the volume of channel from 0 to 1, a value outside counting as the nearer end; returns at once
    Position position(uint8_t channel) const;                                    // gets the sound on channel and where it stands, an empty position for a free channel
    bool     playing(uint8_t channel) const;                                     // gets whether channel plays a sound or has one requested

private:
    friend class ChannelPlayer;

    uint32_t start(uint8_t channel, const Position& from);   // leaves the request for from under a new number

    static constexpr float   kCap    = 0.786;   // the master gain's ceiling, docs/parts/audio
    static constexpr uint8_t kBlocks = 6;       // PJRC's audio blocks in RAM2: one per channel, two the output still sends

    static uint32_t              count_;        // the numbers play() has given
    static EventQueue::Producer* out_;          // the queue SoundEnded goes into
};

// one channel: plays a sound out of its ring buffer, which its file channel keeps filled from
// the card; the update never waits for the card
class ChannelPlayer : public AudioStream {
public:
    // a channel that reads into ring and publishes its SoundEnded with index as the source
    ChannelPlayer(uint8_t index, uint8_t* ring)
        : AudioStream(0, nullptr), file_(ring, kChunk, Storage::Priority::High), index_(index) {}

    // leaves a request for a sound from a position under number, or for silence with number 0
    void request(const AudioDriver::Position& from, uint32_t number);

    bool requested() const;                    // gets whether the channel plays a sound or has one requested
    AudioDriver::Position position() const;    // gets the sound playing and where it stands, read consistently while the update writes it
    void volume(float level) { volume_ = level < 0 ? 0 : level > 1 ? 1 : level; }   // sets the volume the caller wants, 0 to 1

    // applies the last request, once a sound it cuts off has fallen to 0 across one block; finds
    // the samples behind the WAV headers, or starts right at them from a position; takes one block
    // out of the ring at the channel's volume, skips to the samples again at the end of a sound that
    // loops, keeps its position, and publishes SoundEnded once the sound has run out
    void update() override;

private:
    static constexpr uint32_t kChunk  = 4096;    // bytes in one read of the card
    static constexpr uint8_t  kChunks = 8;       // chunks in the ring

    // a request: the main loop fills the slot the request word does not name, then publishes it,
    // and the update reads the slot the word names
    struct Request {
        AudioDriver::Position from;     // the sound and where it starts, empty for silence
        uint32_t              number;   // what play() returned, 0 for silence
    };

    FileChannel<kChunks>  file_;          // the sound playing, read ahead into the ring
    const uint8_t         index_;         // the source of its SoundEnded, 0 to 3
    Request               requests_[2];
    volatile uint32_t     word_    = 0;   // the requests made so far; its lowest bit names the slot of the last
    uint32_t              taken_   = 0;   // the requests the update has applied
    volatile uint32_t     number_  = 0;   // the number of the sound playing, 0 for none
    volatile float        volume_  = 1;   // the volume the caller set, 0 to 1; the channel plays at exactly this
    float                 level_   = 0;   // the volume the last block ended at, which the next block glides from
    AudioDriver::Position now_;           // the sound playing and where it stands, which the update moves on every block
    volatile uint32_t     seq_     = 0;   // odd while the update writes now_, so position() reads it again
    uint32_t              skip_    = 0;   // bytes ahead of the samples the update still drops
    uint8_t               head_[8];       // the header of the RIFF chunk the update reads, its id and its size
    uint8_t               headAt_  = 0;   // bytes of head_ the update has taken
    bool                  samples_ = false;   // the update has reached the data chunk and plays its samples
};

void ChannelPlayer::request(const AudioDriver::Position& from, uint32_t number) {
    requests_[(word_ + 1) & 1] = {from, number};   // the slot the word does not name
    asm volatile("dmb" ::: "memory");              // the slot is filled before the word names it
    word_ = word_ + 1;
}

bool ChannelPlayer::requested() const {
    const uint32_t word = word_;
    return word != taken_ ? requests_[word & 1].number != 0 : number_ != 0;
}

AudioDriver::Position ChannelPlayer::position() const {
    AudioDriver::Position at;
    uint32_t seq;
    do {
        seq = seq_;
        asm volatile("dmb" ::: "memory");
        at = now_;
        asm volatile("dmb" ::: "memory");
    } while ((seq & 1) || seq != seq_);   // the update wrote now_ meanwhile
    return at;
}

// the driver's mixer: adds the four channels in 32 bits, so the sum loses nothing, and turns the
// sum down ahead of a block that would overdrive the output; afterwards it turns the sum slowly
// back up. It writes the sum into the first channel block it receives, so it needs no block of
// its own
class Mixer : public AudioStream {
public:
    Mixer() : AudioStream(4, inputs_) {}

    // adds the channels' blocks in 32 bits and keeps the sum one block back; sends the block before
    // with a gain that glides toward what the new block needs, so a loud block meets a gain that is
    // already down
    void update() override;

private:
    static constexpr float kReleaseMs = 100;   // how long the gain takes to come back up after an overdrive; a decision, slow enough not to be heard as pumping

    audio_block_t* inputs_[4];
    int32_t        sums_[2][AUDIO_BLOCK_SAMPLES];   // the sum of the new block and of the block before, which goes out one block late
    uint8_t        held_ = 0;   // which of sums_ holds the block before
    float          gain_ = 1;   // the gain the last block ended at, 1 while nothing overdrives
};

// the four rings in RAM2, 32 KB each and aligned to 32 bytes
DMAMEM static uint8_t rings_[4][32768] __attribute__((aligned(32)));

// the audio graph, which PJRC's Audio library updates every block in the order it is built,
// the channels first
ChannelPlayer    channels[4] = {ChannelPlayer(0, rings_[0]), ChannelPlayer(1, rings_[1]),
                                ChannelPlayer(2, rings_[2]), ChannelPlayer(3, rings_[3])};
Mixer            mix;          // adds the four channels and keeps their sum from overdriving
AudioAmplifier   level;        // the master gain
AudioOutputI2S2  out;          // the I²S2 port, docs/pin-assignment.md
AudioConnection  wire0(channels[0], 0, mix, 0);
AudioConnection  wire1(channels[1], 0, mix, 1);
AudioConnection  wire2(channels[2], 0, mix, 2);
AudioConnection  wire3(channels[3], 0, mix, 3);
AudioConnection  wireLevel(mix, 0, level, 0);
AudioConnection  wireLeft(level, 0, out, 0);
AudioConnection  wireRight(level, 0, out, 1);

uint32_t AudioDriver::play(uint8_t channel, const FileReader& file, bool loop) {
    Position from;
    from.file = file;
    from.loop = loop;                              // offset 0: the update reads the WAV headers first
    return start(channel, from);                   // a file with no name gives a sound that ends at once
}

uint32_t AudioDriver::play(uint8_t channel, const Position& from) { return from.empty() ? 0 : start(channel, from); }

uint32_t AudioDriver::start(uint8_t channel, const Position& from) {
    count_ = count_ + 1;
    channels[channel].request(from, count_);
    return count_;
}

void AudioDriver::stop(uint8_t channel) { channels[channel].request(Position{}, 0); }

void AudioDriver::volume(uint8_t channel, float level) { channels[channel].volume(level); }

AudioDriver::Position AudioDriver::position(uint8_t channel) const { return channels[channel].position(); }

bool AudioDriver::playing(uint8_t channel) const { return channels[channel].requested(); }
