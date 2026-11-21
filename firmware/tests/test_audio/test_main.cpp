// Tests on the computer the audio driver's channels and its mixer, which touch no hardware. A
// channel reads WAV files through the storage driver's requests from a fake card in memory, and
// the test plays both the storage task and the Audio library's update.
//
//   wav-chunks  the samples of the data chunk come out once and in order, behind any chunks
//   wav-format  a file in another format, a missing file and a failed read end the sound
//   wav-ffmpeg  a sound that the command of assets/media.md made plays every sample of its data
//               chunk. ffmpeg.wav beside this file is such a sound, made from a stereo MP3 at
//               another sample rate
//   wav-loop    a sound that loops runs on from its first sample after its last
//   card-waits  a channel waiting for the card plays silence and loses no sample
//   position    a sound goes on from where position() saw it, also across a loop
//   glides      starting, stopping, replacing and a new volume glide across one block
//   ends        a sound that plays out reports its number once, a cut-off sound none
//   mix-sum     the mixer adds the channels exactly, one block late
//   mix-limit   a sum past full scale is turned down ahead of time and comes back up slowly
//
// Run: pio test -e computer, which runs in CI as well.

#include "drivers/sound-channel.h"
#include "drivers/sound-mixer.h"
#include <cmath>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <unity.h>
#include <vector>

constexpr uint32_t N = SOUND_BLOCK_SAMPLES;
constexpr uint32_t RATE_HZ = 44100;
constexpr int16_t FULL = 32767;
constexpr uint32_t BLOCKS_PER_100_MS = 35;   // 100 ms of blocks of 128 samples at 44.1 kHz, up

// A card in memory. Each file is a string, each slot names the file it holds.
struct FakeCard : CardFiles {
    std::map<std::string, std::string> files;
    std::string slotPath[SLOTS];
    uint32_t failNext = 0;   // reads that fail from now on

    bool mount() override { return true; }
    bool remount() override { return true; }
    uint32_t failedTransfers() const override { return 0; }
    uint32_t errorCode() const override { return 0; }
    bool open(uint8_t slot, const char* path, bool) override {
        if (files.count(path) == 0) {
            return false;
        }
        slotPath[slot] = path;
        return true;
    }
    void close(uint8_t) override {}
    uint32_t size(uint8_t slot) override { return uint32_t(files[slotPath[slot]].size()); }
    bool read(uint8_t slot, uint32_t at, uint8_t* to, size_t n, size_t& got) override {
        if (failNext > 0) {
            failNext = failNext - 1;
            return false;
        }
        const std::string& f = files[slotPath[slot]];
        got = at >= f.size() ? 0 : std::min(n, f.size() - at);
        f.copy(reinterpret_cast<char*>(to), got, at);
        return true;
    }
    bool write(uint8_t, uint32_t, const uint8_t*, size_t) override { return false; }
    bool cut(uint8_t, uint32_t) override { return false; }
    bool commit(uint8_t) override { return false; }
};

// The service with the storage task played by the test.
struct TestService : StorageService {
    explicit TestService(FakeCard& card) : StorageService(card) {}
    ~TestService() = default;

    // A request that waits, such as one whose channel goes out of scope while it reads, is served
    // meanwhile, as the storage task would serve it.
    void pause() override { serveNext(); }

    void serveAll() {
        while (serveNext()) {
        }
    }
};

static std::vector<uint8_t> ring(SoundChannel::RING_BYTES);

// A card, the service on it and one channel.
struct Rig {
    FakeCard card;
    TestService service{card};
    SoundChannel channel{ring.data()};
    uint32_t ended = 0;   // the number the last block reported, 0 for none

    Rig() { service.mount(); }

    // Serves the card, then plays one block and gets its samples, empty for no sound.
    std::vector<int16_t> block() {
        service.serveAll();
        int16_t samples[N];
        const bool sound = channel.play(samples, ended);
        return sound ? std::vector<int16_t>(samples, samples + N) : std::vector<int16_t>();
    }
};

static void put16(std::string& s, uint16_t v) {
    s.push_back(char(v & 0xFF));
    s.push_back(char(v >> 8));
}

static void put32(std::string& s, uint32_t v) {
    put16(s, uint16_t(v & 0xFFFF));
    put16(s, uint16_t(v >> 16));
}

// Gets a WAV file with its samples, the chunks before the data chunk and the bytes behind it.
static std::string wav(const std::vector<int16_t>& samples, const std::string& before = "",
                       const std::string& behind = "", uint16_t channels = 1,
                       uint32_t rateHz = RATE_HZ) {
    std::string body = "WAVEfmt ";
    put32(body, 16);
    put16(body, 1);
    put16(body, channels);
    put32(body, rateHz);
    put32(body, rateHz * 2 * channels);
    put16(body, uint16_t(2 * channels));
    put16(body, 16);
    body += before;
    body += "data";
    put32(body, uint32_t(samples.size() * 2));
    for (int16_t s : samples) {
        put16(body, uint16_t(s));
    }
    body += behind;
    std::string file = "RIFF";
    put32(file, uint32_t(body.size()));
    return file + body;
}

// Gets a chunk of an id with bytes of a size, padded to an even size.
static std::string chunk(const char* id, uint32_t bytes) {
    std::string c = id;
    put32(c, bytes);
    c += std::string(bytes + (bytes & 1), 'x');
    return c;
}

// Gets samples that count up by one from a first one.
static std::vector<int16_t> ramp(size_t n, int16_t first = 1) {
    std::vector<int16_t> s(n);
    for (size_t i = 0; i < n; ++i) {
        s[i] = int16_t(first + int(i));
    }
    return s;
}

// Plays blocks until the sound has ended or most blocks have played, and gets the samples of the
// blocks with sound.
static std::vector<int16_t> playOut(Rig& rig, uint32_t most, uint32_t* endedNumber = nullptr) {
    std::vector<int16_t> out;
    for (uint32_t i = 0; i < most; ++i) {
        const std::vector<int16_t> b = rig.block();
        out.insert(out.end(), b.begin(), b.end());
        if (rig.ended != 0) {
            if (endedNumber != nullptr) {
                *endedNumber = rig.ended;
            }
            break;
        }
    }
    return out;
}

void setUp() {}
void tearDown() {}

static void testWavChunks() {
    const size_t counts[] = {1, N - 1, N, N + 1, 5 * N + 17, 9000};
    const std::string befores[] = {"", chunk("LIST", 26), chunk("odd ", 3) + chunk("JUNK", 1000)};
    for (size_t count : counts) {
        for (const std::string& before : befores) {
            Rig rig;
            const std::vector<int16_t> samples = ramp(count);
            rig.card.files["/s.wav"] = wav(samples, before, chunk("LIST", 9));
            // A volume of 1 from the start, so the samples come out unchanged after the glide in.
            rig.channel.request(SoundPosition(rig.service.open("/s.wav"), false), 7);
            uint32_t number = 0;
            std::vector<int16_t> out = playOut(rig, 200, &number);
            TEST_ASSERT_EQUAL_UINT32(7, number);
            // The first block glides in from 0, so its samples are checked against the glide.
            TEST_ASSERT_TRUE(out.size() >= count);
            for (size_t i = 0; i < count; ++i) {
                const float gain = i < N ? float(i + 1) / float(N) : 1.0f;
                const int expected = int(std::lround(float(samples[i]) * gain));
                TEST_ASSERT_INT_WITHIN(1, expected, out[i]);
                if (i >= N) {
                    TEST_ASSERT_EQUAL_INT16(samples[i], out[i]);
                }
            }
            // The rest of the last block is silence.
            for (size_t i = count; i < out.size(); ++i) {
                TEST_ASSERT_EQUAL_INT16(0, out[i]);
            }
            TEST_ASSERT_FALSE(rig.channel.requested());
            TEST_ASSERT_TRUE(rig.channel.position().empty());
        }
    }
}

static void testWavFormat() {
    struct Case {
        const char* path;
        std::string file;
    };
    std::string notWave = wav(ramp(100));
    notWave.replace(8, 4, "AVI ");
    std::string noFormat = "RIFF";
    put32(noFormat, 4 + 8 + 200);
    noFormat += "WAVEdata";
    put32(noFormat, 200);
    noFormat += std::string(200, 'x');
    const Case cases[] = {
        {"/stereo.wav", wav(ramp(1000), "", "", 2)},
        {"/48k.wav", wav(ramp(1000), "", "", 1, 48000)},
        {"/avi.wav", notWave},
        {"/noformat.wav", noFormat},
    };
    for (const Case& c : cases) {
        Rig rig;
        rig.card.files[c.path] = c.file;
        rig.channel.request(SoundPosition(rig.service.open(c.path), true), 3);
        uint32_t number = 0;
        const std::vector<int16_t> out = playOut(rig, 5, &number);
        TEST_ASSERT_EQUAL_UINT32(3, number);
        TEST_ASSERT_TRUE(out.empty());
    }
    // A missing file ends at once, as a file with no name does.
    Rig missing;
    missing.channel.request(SoundPosition(missing.service.open("/none.wav"), true), 4);
    uint32_t number = 0;
    playOut(missing, 3, &number);
    TEST_ASSERT_EQUAL_UINT32(4, number);
    missing.channel.request(SoundPosition(FileReader{}, false), 5);
    playOut(missing, 3, &number);
    TEST_ASSERT_EQUAL_UINT32(5, number);

    // A read that fails partway through ends the sound behind the samples before it.
    Rig failing;
    const std::vector<int16_t> samples = ramp(3 * SoundChannel::CHUNK_BYTES);
    failing.card.files["/long.wav"] = wav(samples);
    failing.channel.request(SoundPosition(failing.service.open("/long.wav"), false), 6);
    failing.block();
    failing.card.failNext = 1;
    number = 0;
    const std::vector<int16_t> out = playOut(failing, 1000, &number);
    TEST_ASSERT_EQUAL_UINT32(6, number);
    TEST_ASSERT_TRUE(out.size() < samples.size());
}

static void testWavFfmpeg() {
    const std::string here = __FILE__;
    const std::string path = here.substr(0, here.find_last_of("/\\") + 1) + "ffmpeg.wav";
    std::ifstream in(path, std::ios::binary);
    TEST_ASSERT_TRUE_MESSAGE(in.good(), path.c_str());
    const std::string file((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    // The data chunk, found here without the channel.
    size_t at = 12;
    size_t dataAt = 0;
    uint32_t dataBytes = 0;
    while (at + 8 <= file.size()) {
        const auto* p = reinterpret_cast<const uint8_t*>(file.data() + at + 4);
        const uint32_t size = p[0] | p[1] << 8 | p[2] << 16 | uint32_t(p[3]) << 24;
        if (file.compare(at, 4, "data") == 0) {
            dataAt = at + 8;
            dataBytes = size;
            break;
        }
        at = at + 8 + size + (size & 1);
    }
    TEST_ASSERT_TRUE(dataAt > 0 && dataAt + dataBytes <= file.size());
    std::vector<int16_t> samples(dataBytes / 2);
    for (size_t i = 0; i < samples.size(); ++i) {
        const auto* p = reinterpret_cast<const uint8_t*>(file.data() + dataAt + 2 * i);
        samples[i] = int16_t(p[0] | p[1] << 8);
    }
    Rig rig;
    rig.card.files["/ffmpeg.wav"] = file;
    rig.channel.request(SoundPosition(rig.service.open("/ffmpeg.wav"), false), 21);
    uint32_t number = 0;
    const std::vector<int16_t> out = playOut(rig, 200, &number);
    TEST_ASSERT_EQUAL_UINT32(21, number);
    TEST_ASSERT_TRUE(out.size() >= samples.size());
    // The first block glides in from 0, then every sample comes out unchanged.
    for (size_t i = 0; i < samples.size(); ++i) {
        if (i < N) {
            TEST_ASSERT_INT_WITHIN(1, std::lround(samples[i] * double(i + 1) / N), out[i]);
        } else {
            TEST_ASSERT_EQUAL_INT16(samples[i], out[i]);
        }
    }
}

static void testWavLoop() {
    Rig rig;
    const std::vector<int16_t> samples = ramp(300);
    rig.card.files["/l.wav"] = wav(samples, chunk("LIST", 30), chunk("cue ", 11));
    rig.channel.request(SoundPosition(rig.service.open("/l.wav"), true), 9);
    // The first block starts the card's read and stays silent.
    TEST_ASSERT_TRUE(rig.block().empty());
    const std::vector<int16_t> out = playOut(rig, 40);
    TEST_ASSERT_EQUAL_UINT32(0, rig.ended);
    TEST_ASSERT_EQUAL_UINT32(40 * N, out.size());
    // Past the first block, the samples run through the file again and again with no gap.
    for (size_t i = N; i < out.size(); ++i) {
        TEST_ASSERT_EQUAL_INT16(samples[i % samples.size()], out[i]);
    }
    TEST_ASSERT_TRUE(rig.channel.requested());
}

static void testCardWaits() {
    Rig rig;
    // Samples from 1000 on stay above 0 through the glide in.
    const std::vector<int16_t> samples = ramp(5 * SoundChannel::CHUNK_BYTES / 2, 1000);
    rig.card.files["/w.wav"] = wav(samples);
    rig.channel.request(SoundPosition(rig.service.open("/w.wav"), false), 2);
    int16_t block[N];
    uint32_t ended = 0;
    // The card has brought nothing yet, so the block is silent and the sound waits.
    TEST_ASSERT_FALSE(rig.channel.play(block, ended));
    TEST_ASSERT_FALSE(rig.channel.play(block, ended));
    TEST_ASSERT_EQUAL_UINT32(0, ended);
    TEST_ASSERT_TRUE(rig.channel.requested());
    std::vector<int16_t> out;
    // Every block lets the card bring at most one chunk, and blocks without one stay silent.
    for (uint32_t i = 0; i < 4000 && ended == 0; ++i) {
        if (i % 50 == 0) {
            rig.service.serveNext();
        }
        if (rig.channel.play(block, ended)) {
            out.insert(out.end(), block, block + N);
        }
    }
    TEST_ASSERT_EQUAL_UINT32(2, ended);
    // Leaving out the silence of the gaps, every sample came once and in order.
    std::vector<int16_t> sounding;
    for (int16_t s : out) {
        if (s != 0) {
            sounding.push_back(s);
        }
    }
    TEST_ASSERT_EQUAL_UINT32(samples.size(), sounding.size());
    for (size_t i = N; i < samples.size(); ++i) {
        TEST_ASSERT_EQUAL_INT16(samples[i], sounding[i]);
    }
}

static void testPosition() {
    Rig rig;
    const std::vector<int16_t> first = ramp(20 * N);
    const std::vector<int16_t> other = ramp(50 * N, -20000);
    rig.card.files["/a.wav"] = wav(first, chunk("LIST", 30));
    rig.card.files["/b.wav"] = wav(other);
    rig.channel.request(SoundPosition(rig.service.open("/a.wav"), true), 1);
    // The first block starts the card's read, and seven blocks of samples follow.
    for (int i = 0; i < 8; ++i) {
        rig.block();
    }
    const SoundPosition at = rig.channel.position();
    TEST_ASSERT_FALSE(at.empty());
    TEST_ASSERT_EQUAL_UINT32(7 * N, at.played());
    rig.channel.request(SoundPosition(rig.service.open("/b.wav"), false), 2);
    for (int i = 0; i < 10; ++i) {
        rig.block();
    }
    rig.channel.request(at, 3);
    // The first block glides the other sound out, the next starts the card's read again.
    TEST_ASSERT_EQUAL_UINT32(N, rig.block().size());
    TEST_ASSERT_TRUE(rig.block().empty());
    std::vector<int16_t> out = playOut(rig, 3);
    TEST_ASSERT_EQUAL_UINT32(3 * N, out.size());
    for (size_t i = N; i < out.size(); ++i) {
        TEST_ASSERT_EQUAL_INT16(first[7 * N + i], out[i]);
    }

    // A position in the bytes behind the samples of a sound that loops goes on with its loop. The
    // chunk behind the samples is longer than two reads. The first block glides the sound
    // before out, the second starts the read, and the fourth stops inside that chunk.
    rig.card.files["/c.wav"] = wav(ramp(N + 1), "", chunk("LIST", 20000));
    rig.channel.request(SoundPosition(rig.service.open("/c.wav"), true), 4);
    for (int i = 0; i < 4; ++i) {
        rig.block();
    }
    const SoundPosition behind = rig.channel.position();
    TEST_ASSERT_FALSE(behind.empty());
    TEST_ASSERT_EQUAL_UINT32(N + 1, behind.played());
    rig.channel.request(SoundPosition(), 0);
    rig.block();
    rig.block();
    TEST_ASSERT_TRUE(rig.channel.position().empty());
    rig.channel.request(behind, 5);
    out = playOut(rig, 6);
    TEST_ASSERT_EQUAL_UINT32(0, rig.ended);
    // The loop starts over at the first sample, which glides in across its first block.
    TEST_ASSERT_TRUE(out.size() > N);
    TEST_ASSERT_INT_WITHIN(1, 0, out[0]);
    TEST_ASSERT_EQUAL_INT16(int16_t(N + 1), out[N]);
}

static void testGlides() {
    Rig rig;
    rig.card.files["/g.wav"] = wav(std::vector<int16_t>(100 * N, 16000));
    rig.card.files["/h.wav"] = wav(std::vector<int16_t>(100 * N, -8000));
    rig.channel.request(SoundPosition(rig.service.open("/g.wav"), false), 1);
    // Starting glides in across the first block of samples, behind the block that starts the read.
    TEST_ASSERT_TRUE(rig.block().empty());
    std::vector<int16_t> b = rig.block();
    TEST_ASSERT_EQUAL_UINT32(N, b.size());
    for (uint32_t i = 0; i < N; ++i) {
        TEST_ASSERT_INT_WITHIN(1, std::lround(16000.0 * (i + 1) / N), b[i]);
    }
    b = rig.block();
    TEST_ASSERT_EQUAL_INT16(16000, b[0]);
    // A new volume glides across the next block, then stays.
    rig.channel.volume(0.25f);
    b = rig.block();
    for (uint32_t i = 0; i < N; ++i) {
        TEST_ASSERT_INT_WITHIN(1, std::lround(16000.0 * (1 - 0.75 * (i + 1) / N)), b[i]);
    }
    b = rig.block();
    TEST_ASSERT_EQUAL_INT16(4000, b[N - 1]);
    // A volume outside 0 to 1 counts as the nearer end.
    rig.channel.volume(7);
    rig.block();
    b = rig.block();
    TEST_ASSERT_EQUAL_INT16(16000, b[0]);
    rig.channel.volume(-1);
    rig.block();
    b = rig.block();
    TEST_ASSERT_EQUAL_INT16(0, b[0]);
    rig.channel.volume(1);
    rig.block();
    // Replacing glides the old sound out across one block, and the new one glides in.
    rig.channel.request(SoundPosition(rig.service.open("/h.wav"), false), 2);
    b = rig.block();
    for (uint32_t i = 0; i < N; ++i) {
        TEST_ASSERT_INT_WITHIN(1, std::lround(16000.0 * (1 - double(i + 1) / N)), b[i]);
    }
    TEST_ASSERT_EQUAL_UINT32(0, rig.ended);
    TEST_ASSERT_TRUE(rig.block().empty());
    b = rig.block();
    for (uint32_t i = 0; i < N; ++i) {
        TEST_ASSERT_INT_WITHIN(1, std::lround(-8000.0 * (i + 1) / N), b[i]);
    }
    // Stopping glides out across one block, then the channel is silent and free.
    rig.channel.request(SoundPosition(), 0);
    TEST_ASSERT_FALSE(rig.channel.requested());
    b = rig.block();
    TEST_ASSERT_EQUAL_UINT32(N, b.size());
    TEST_ASSERT_EQUAL_INT16(0, b[N - 1]);
    TEST_ASSERT_TRUE(rig.block().empty());
    TEST_ASSERT_EQUAL_UINT32(0, rig.ended);
    TEST_ASSERT_TRUE(rig.channel.position().empty());
}

static void testEnds() {
    Rig rig;
    rig.card.files["/e.wav"] = wav(ramp(3 * N));
    rig.channel.request(SoundPosition(rig.service.open("/e.wav"), false), 11);
    TEST_ASSERT_TRUE(rig.channel.requested());
    uint32_t reports = 0;
    for (int i = 0; i < 10; ++i) {
        rig.block();
        if (rig.ended != 0) {
            TEST_ASSERT_EQUAL_UINT32(11, rig.ended);
            reports = reports + 1;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(1, reports);
    // A sound replaced, a sound stopped and a request replaced before its block report nothing.
    rig.channel.request(SoundPosition(rig.service.open("/e.wav"), false), 12);
    rig.block();
    rig.channel.request(SoundPosition(rig.service.open("/e.wav"), false), 13);
    rig.block();
    rig.block();
    rig.channel.request(SoundPosition(), 0);
    rig.channel.request(SoundPosition(rig.service.open("/e.wav"), false), 14);
    rig.channel.request(SoundPosition(), 0);
    for (int i = 0; i < 10; ++i) {
        rig.block();
        TEST_ASSERT_EQUAL_UINT32(0, rig.ended);
    }
    TEST_ASSERT_FALSE(rig.channel.requested());
}

static void testMixSum() {
    SoundMixer mixer;
    int16_t a[N];
    int16_t b[N];
    int16_t out[N];
    for (uint32_t i = 0; i < N; ++i) {
        a[i] = int16_t(1000 + i);
        b[i] = int16_t(-3000 + 7 * int(i));
    }
    const int16_t* const both[SoundMixer::INPUTS] = {a, nullptr, b, nullptr};
    const int16_t* const none[SoundMixer::INPUTS] = {};
    // The first call gives the silence before any block.
    TEST_ASSERT_FALSE(mixer.mix(both, out));
    TEST_ASSERT_TRUE(mixer.mix(none, out));
    for (uint32_t i = 0; i < N; ++i) {
        TEST_ASSERT_EQUAL_INT16(a[i] + b[i], out[i]);
    }
    TEST_ASSERT_FALSE(mixer.mix(none, out));
    // The sum may land in one of the inputs.
    const int16_t* const one[SoundMixer::INPUTS] = {a, nullptr, nullptr, nullptr};
    TEST_ASSERT_FALSE(mixer.mix(one, a));
    const int16_t first = a[0];
    TEST_ASSERT_TRUE(mixer.mix(one, a));
    TEST_ASSERT_EQUAL_INT16(first, a[0]);
    TEST_ASSERT_EQUAL_FLOAT(1, mixer.gain());
}

static void testMixLimit() {
    SoundMixer mixer;
    int16_t loud[N];
    int16_t quiet[N];
    int16_t out[N];
    for (uint32_t i = 0; i < N; ++i) {
        loud[i] = int16_t(i % 2 == 0 ? FULL : -FULL - 1);
        quiet[i] = 1000;
    }
    const int16_t* const four[SoundMixer::INPUTS] = {loud, loud, loud, loud};
    const int16_t* const q[SoundMixer::INPUTS] = {quiet, nullptr, nullptr, nullptr};
    std::vector<int16_t> all;
    mixer.mix(q, out);
    // A quiet block, then loud blocks on all four channels.
    TEST_ASSERT_TRUE(mixer.mix(four, out));
    all.insert(all.end(), out, out + N);
    // The quiet block glided down ahead of the loud one.
    TEST_ASSERT_TRUE(mixer.gain() <= 0.25f + 1e-4f);
    for (int k = 0; k < 10; ++k) {
        TEST_ASSERT_TRUE(mixer.mix(four, out));
        all.insert(all.end(), out, out + N);
    }
    for (int16_t s : all) {
        TEST_ASSERT_TRUE(s <= FULL && s >= -FULL - 1);
    }
    // The quiet block's gain falls steadily, with no jump between neighbors.
    for (size_t i = 1; i < N; ++i) {
        TEST_ASSERT_TRUE(all[i] <= all[i - 1]);
    }
    // Quiet again: the gain comes back up to 1 within 100 ms, never by a jump.
    float last = mixer.gain();
    uint32_t blocks = 0;
    while (mixer.gain() < 1 && blocks < 2 * BLOCKS_PER_100_MS) {
        mixer.mix(q, out);
        TEST_ASSERT_TRUE(mixer.gain() - last <= 0.03f);
        last = mixer.gain();
        blocks = blocks + 1;
    }
    TEST_ASSERT_EQUAL_FLOAT(1, mixer.gain());
    TEST_ASSERT_TRUE(blocks <= BLOCKS_PER_100_MS + 1);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(testWavChunks);
    RUN_TEST(testWavFormat);
    RUN_TEST(testWavFfmpeg);
    RUN_TEST(testWavLoop);
    RUN_TEST(testCardWaits);
    RUN_TEST(testPosition);
    RUN_TEST(testGlides);
    RUN_TEST(testEnds);
    RUN_TEST(testMixSum);
    RUN_TEST(testMixLimit);
    return UNITY_END();
}
