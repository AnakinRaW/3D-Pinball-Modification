// Tests on the computer the sounds component on a fake audio driver, whose channels play WAV files
// from a fake card in memory whenever the test plays a block. Every track is a ramp of samples that
// count up by one from a first sample no other track shares, so the last sample of a block names
// the track and the place in it.
//
//   channels         the music keeps its channel, the effects share the other three, the oldest
//                    effect gives way
//   video-sound      a video's sound covers the music, which goes on where it stood
//   video-sound-top  a video's sound lies over every track, and a track that waited starts from
//                    its beginning
//   tracks           the track of the highest priority plays, the later of equal priorities, and
//                    a covered track goes on where it was paused
//   same-pass        a track that comes on top before the driver took the track below pauses
//                    that track where it starts
//   music-ended      only a mode's track that played to its end gives MUSIC_ENDED, once
//   volume           the driver gets the square of each volume inside 0 to 1
//   balance          music and effects whose squares add up to more than 1 are scaled down
//                    together to a sum of 1, and each keeps the volume set
//   start-volumes    begin() sets the starting volumes as the two setters do
//   names            a name plays from the game's audio directory, a long one finds no file
//   reset            reset() silences every channel, forgets every track and sets the volumes
//
// Run: pio test -e computer, which runs in CI as well.

#include "components/file-system.h"
#include "components/sounds.h"
#include <map>
#include <string>
#include <unity.h>
#include <vector>

constexpr uint32_t N = SOUND_BLOCK_SAMPLES;
constexpr uint32_t RATE_HZ = 44100;
constexpr uint32_t TRACK_BLOCKS = 40;   // a track's length, short enough to play out in a test
constexpr uint32_t TRACK = TRACK_BLOCKS * N;
constexpr uint32_t SETTLE = 4;   // glide out, start the card's read, glide in, one block at full
constexpr uint32_t PLAY_OUT = TRACK_BLOCKS + 10;

// the first sample of each track, so the ramps do not overlap
constexpr int16_t A = 0;
constexpr int16_t B = 6000;
constexpr int16_t C = 12000;
constexpr int16_t D = 18000;
constexpr int16_t V = -20000;
constexpr int16_t T = -12000;
constexpr int16_t EFFECT = -30000;

constexpr Layer LOW = {1, 10};
constexpr Layer MIDDLE = {2, 50};
constexpr Layer MIDDLE_LATER = {3, 50};
constexpr Layer HIGH = {4, 255};

constexpr char GAME[] = "a";

// A card in memory. Each file is a string, each slot names the file it holds.
struct FakeCard : CardFiles {
    std::map<std::string, std::string> files;
    std::string slotPath[SLOTS];

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

    void pause() override { serveNext(); }

    void serveAll() {
        while (serveNext()) {
        }
    }
};

// The host's calls to the file system and to the sounds.
class GameHost {
public:
    static void use(FileSystem& files, const char* game) { files.use(game); }
    static void reset(Sounds& sounds) { sounds.reset(); }
};

// The video player's calls to the sounds.
class VideoPlayer {
public:
    static void sound(Sounds& sounds, const char* name, bool loop = false) {
        sounds.videoSound(name, loop);
    }
    static void stop(Sounds& sounds) { sounds.stopVideoSound(); }
    static bool sounding(const Sounds& sounds) { return sounds.videoSounding(); }
    static uint32_t played(const Sounds& sounds) { return sounds.videoPlayed(); }
};

static void put16(std::string& s, uint16_t v) {
    s.push_back(char(v & 0xFF));
    s.push_back(char(v >> 8));
}

static void put32(std::string& s, uint32_t v) {
    put16(s, uint16_t(v & 0xFFFF));
    put16(s, uint16_t(v >> 16));
}

// Gets a WAV file whose samples count up by one from a first sample.
static std::string ramp(int16_t first, uint32_t samples = TRACK) {
    std::string body = "WAVEfmt ";
    put32(body, 16);
    put16(body, 1);
    put16(body, 1);
    put32(body, RATE_HZ);
    put32(body, RATE_HZ * 2);
    put16(body, 2);
    put16(body, 16);
    body += "data";
    put32(body, samples * 2);
    for (uint32_t i = 0; i < samples; ++i) {
        put16(body, uint16_t(int16_t(first + int(i))));
    }
    std::string file = "RIFF";
    put32(file, uint32_t(body.size()));
    return file + body;
}

// A card, the storage service on it, the game's files, the fake driver and the sounds, with the
// tracks and the effects on the card, the music at 1 and the effects at 0, so the music's samples
// come out unchanged.
struct Rig {
    FakeCard card;
    TestService service{card};
    FileSystem files{service};
    AudioDriver audio;
    Sounds sounds{audio, files};
    std::vector<PinballEvent> events;   // what translate() made, in order
    std::vector<DriverEvent> held;      // the driver's events a test holds back

    Rig() {
        service.mount();
        GameHost::use(files, GAME);
        audio.begin();
        sounds.begin();
        sounds.musicVolume(1);
        sounds.effectsVolume(0);
        const std::pair<const char*, int16_t> tracks[] = {{"a.wav", A}, {"b.wav", B}, {"c.wav", C},
                                                          {"d.wav", D}, {"v.wav", V}, {"t.wav", T}};
        for (const auto& t : tracks) {
            card.files[std::string("/games/a/audio/") + t.first] = ramp(t.second);
        }
        for (const char* e : {"e1.wav", "e2.wav", "e3.wav", "e4.wav", "e5.wav"}) {
            card.files[std::string("/games/a/audio/") + e] = ramp(EFFECT, 4 * TRACK);
        }
    }

    // Hands a driver event to the sounds, which have to take it, and keeps the events they made.
    void hand(const DriverEvent& e) {
        PinballEvents out;
        TEST_ASSERT_TRUE(sounds.translate(e, out));
        events.insert(events.end(), out.events, out.events + out.count);
    }

    // Serves the card and plays blocks, handing the driver's events to the sounds, or holding
    // them back.
    void blocks(uint32_t n, bool hold = false) {
        for (uint32_t i = 0; i < n; ++i) {
            service.serveAll();
            for (const DriverEvent& e : audio.block()) {
                if (hold) {
                    held.push_back(e);
                } else {
                    hand(e);
                }
            }
        }
    }

    // Gets how many samples the music channel's sound has played.
    uint32_t played() const { return audio.position(Sounds::MUSIC).played(); }
};

// Checks that the music channel plays the track that starts at a first sample, at full volume.
static void expectTrack(const Rig& rig, int16_t first) {
    const uint32_t played = rig.played();
    TEST_ASSERT_TRUE(played > 0);
    TEST_ASSERT_EQUAL_INT16(int16_t(first + int(played) - 1), rig.audio.last[Sounds::MUSIC]);
}

void setUp() {}
void tearDown() {}

static void testChannels() {
    Rig rig;
    rig.sounds.music(LOW, "a.wav");
    rig.blocks(SETTLE);
    rig.sounds.play("e1.wav");
    rig.sounds.play("e2.wav");
    rig.sounds.play("e3.wav");
    rig.blocks(SETTLE);
    TEST_ASSERT_EQUAL_STRING("/games/a/audio/a.wav", rig.audio.files[0].c_str());
    TEST_ASSERT_EQUAL_STRING("/games/a/audio/e1.wav", rig.audio.files[1].c_str());
    TEST_ASSERT_EQUAL_STRING("/games/a/audio/e2.wav", rig.audio.files[2].c_str());
    TEST_ASSERT_EQUAL_STRING("/games/a/audio/e3.wav", rig.audio.files[3].c_str());
    // The fourth effect replaces the first, which started first, and the music plays on.
    rig.sounds.play("e4.wav");
    rig.blocks(SETTLE);
    TEST_ASSERT_EQUAL_STRING("/games/a/audio/e4.wav", rig.audio.files[1].c_str());
    expectTrack(rig, A);
    // The next one replaces the second, which is the oldest now.
    rig.sounds.play("e5.wav");
    TEST_ASSERT_EQUAL_STRING("/games/a/audio/e5.wav", rig.audio.files[2].c_str());
    // An effect that has ended leaves its channel free, which the next effect takes.
    rig.audio.stop(3);
    rig.blocks(2);
    rig.sounds.play("e1.wav");
    TEST_ASSERT_EQUAL_STRING("/games/a/audio/e1.wav", rig.audio.files[3].c_str());
    TEST_ASSERT_TRUE(rig.events.empty());
}

static void testVideoSound() {
    Rig rig;
    rig.sounds.music(LOW, "a.wav");
    rig.blocks(SETTLE + 5);
    expectTrack(rig, A);
    TEST_ASSERT_FALSE(VideoPlayer::sounding(rig.sounds));
    VideoPlayer::sound(rig.sounds, "v.wav");
    TEST_ASSERT_TRUE(VideoPlayer::sounding(rig.sounds));
    TEST_ASSERT_EQUAL_UINT32(0, VideoPlayer::played(rig.sounds));
    rig.blocks(SETTLE);
    expectTrack(rig, V);
    TEST_ASSERT_EQUAL_UINT32(rig.played(), VideoPlayer::played(rig.sounds));
    TEST_ASSERT_TRUE(VideoPlayer::played(rig.sounds) > 0);
    // The paused track goes on where it stood once the video's sound has ended.
    rig.blocks(PLAY_OUT);
    TEST_ASSERT_FALSE(VideoPlayer::sounding(rig.sounds));
    TEST_ASSERT_TRUE(rig.events.empty());
    expectTrack(rig, A);
    TEST_ASSERT_FALSE(rig.sounds.musicEnded(LOW));
    // The track noted its place in the block that glided it out, so it plays on from about
    // SETTLE + 5 blocks, and far from its end.
    TEST_ASSERT_TRUE(rig.played() < (2 * SETTLE + 6) * N + (PLAY_OUT - TRACK_BLOCKS) * N);
}

static void testVideoSoundTop() {
    Rig rig;
    rig.sounds.music(LOW, "a.wav");
    rig.blocks(SETTLE);
    VideoPlayer::sound(rig.sounds, "v.wav");
    rig.blocks(SETTLE);
    // A track of the highest priority and the stop of the track below leave the video's sound
    // playing.
    rig.sounds.music(HIGH, "t.wav");
    rig.sounds.stopMusic(LOW);
    rig.blocks(SETTLE);
    expectTrack(rig, V);
    TEST_ASSERT_TRUE(rig.sounds.musicEnded(LOW));
    // The track that waited starts from its beginning once the video's sound has ended.
    for (uint32_t i = 0; i < PLAY_OUT && VideoPlayer::sounding(rig.sounds); ++i) {
        rig.blocks(1);
    }
    TEST_ASSERT_FALSE(VideoPlayer::sounding(rig.sounds));
    rig.blocks(SETTLE);
    expectTrack(rig, T);
    TEST_ASSERT_TRUE(rig.played() <= (SETTLE + 1) * N);
    TEST_ASSERT_TRUE(rig.events.empty());
    // A video's sound with no track leaves the music off after it.
    rig.sounds.stopMusic(HIGH);
    VideoPlayer::sound(rig.sounds, "v.wav");
    rig.blocks(SETTLE);
    expectTrack(rig, V);
    rig.blocks(PLAY_OUT);
    TEST_ASSERT_FALSE(rig.audio.playing(Sounds::MUSIC));
    TEST_ASSERT_EQUAL_INT16(0, rig.audio.last[Sounds::MUSIC]);
    TEST_ASSERT_TRUE(rig.events.empty());
}

static void testTracks() {
    Rig rig;
    rig.sounds.music(LOW, "a.wav");
    rig.blocks(SETTLE);
    expectTrack(rig, A);
    rig.sounds.music(MIDDLE, "b.wav");
    rig.blocks(SETTLE + 3);
    expectTrack(rig, B);
    // The later of equal priorities plays and pauses the one before.
    const uint32_t pausedB = rig.played();
    rig.sounds.music(MIDDLE_LATER, "c.wav");
    rig.blocks(SETTLE);
    expectTrack(rig, C);
    // A second track of a mode replaces the first and starts from its beginning.
    rig.sounds.music(MIDDLE_LATER, "d.wav");
    rig.blocks(SETTLE);
    expectTrack(rig, D);
    TEST_ASSERT_TRUE(rig.played() <= SETTLE * N);
    // Stopping the track on top lets the paused one go on where it was paused.
    rig.sounds.stopMusic(MIDDLE_LATER);
    TEST_ASSERT_EQUAL_UINT32(pausedB, rig.played());
    rig.blocks(SETTLE);
    expectTrack(rig, B);
    TEST_ASSERT_TRUE(rig.sounds.musicEnded(MIDDLE_LATER));
    // Stopping the track below leaves the track on top playing on.
    const uint32_t before = rig.played();
    rig.sounds.stopMusic(LOW);
    rig.blocks(1);
    TEST_ASSERT_EQUAL_UINT32(before + N, rig.played());
    expectTrack(rig, B);
    TEST_ASSERT_TRUE(rig.sounds.musicEnded(LOW));
    // The last track ends, and the music falls silent.
    rig.blocks(PLAY_OUT);
    TEST_ASSERT_EQUAL_UINT32(1, rig.events.size());
    TEST_ASSERT_EQUAL_UINT8(MIDDLE.key, rig.events[0].source);
    TEST_ASSERT_TRUE(rig.sounds.musicEnded(MIDDLE));
    TEST_ASSERT_FALSE(rig.audio.playing(Sounds::MUSIC));
}

static void testSamePass() {
    Rig rig;
    rig.sounds.music(LOW, "a.wav");
    rig.blocks(SETTLE + 5);
    const uint32_t pausedA = rig.played();
    rig.sounds.music(MIDDLE, "b.wav");
    rig.blocks(SETTLE);
    expectTrack(rig, B);
    // The track on top stops, and before the driver plays a block another one comes on top. The
    // track below is paused where it was to go on, never where the stopped track stood.
    rig.sounds.stopMusic(MIDDLE);
    rig.sounds.music(HIGH, "c.wav");
    rig.blocks(SETTLE);
    expectTrack(rig, C);
    rig.sounds.stopMusic(HIGH);
    TEST_ASSERT_EQUAL_UINT32(pausedA, rig.played());
    rig.blocks(SETTLE);
    expectTrack(rig, A);
}

static void testMusicEnded() {
    Rig rig;
    // A track plays to its end once.
    rig.sounds.music(LOW, "a.wav");
    TEST_ASSERT_FALSE(rig.sounds.musicEnded(LOW));
    rig.blocks(PLAY_OUT);
    TEST_ASSERT_EQUAL_UINT32(1, rig.events.size());
    TEST_ASSERT_EQUAL(EventType::MUSIC_ENDED, rig.events[0].type);
    TEST_ASSERT_EQUAL_UINT8(LOW.key, rig.events[0].source);
    TEST_ASSERT_TRUE(payload<EventType::MUSIC_ENDED>(rig.events[0]) != 0);
    TEST_ASSERT_TRUE(rig.sounds.musicEnded(LOW));
    rig.events.clear();

    // A looping track never ends.
    rig.sounds.music(LOW, "a.wav", true);
    rig.blocks(2 * PLAY_OUT);
    TEST_ASSERT_TRUE(rig.events.empty());
    TEST_ASSERT_FALSE(rig.sounds.musicEnded(LOW));
    rig.sounds.stopMusic(LOW);
    rig.blocks(SETTLE);

    // A track that played to its end just before a track came over it ends once and does not
    // start again.
    rig.sounds.music(LOW, "a.wav");
    rig.blocks(PLAY_OUT, true);
    TEST_ASSERT_EQUAL_UINT32(1, rig.held.size());
    rig.sounds.music(MIDDLE, "b.wav");
    rig.blocks(SETTLE);
    rig.hand(rig.held[0]);
    rig.held.clear();
    TEST_ASSERT_EQUAL_UINT32(1, rig.events.size());
    TEST_ASSERT_EQUAL_UINT8(LOW.key, rig.events[0].source);
    TEST_ASSERT_TRUE(rig.sounds.musicEnded(LOW));
    expectTrack(rig, B);
    rig.sounds.stopMusic(MIDDLE);
    rig.blocks(SETTLE);
    TEST_ASSERT_FALSE(rig.audio.playing(Sounds::MUSIC));
    rig.events.clear();

    // The end of a track that a mode's next track has replaced comes late and makes no event.
    rig.sounds.music(LOW, "a.wav");
    rig.blocks(PLAY_OUT, true);
    rig.sounds.music(LOW, "c.wav");
    rig.hand(rig.held[0]);
    rig.held.clear();
    TEST_ASSERT_TRUE(rig.events.empty());
    TEST_ASSERT_FALSE(rig.sounds.musicEnded(LOW));
    rig.blocks(SETTLE);
    expectTrack(rig, C);

    // The end of an effect makes no event.
    rig.card.files["/games/a/audio/short.wav"] = ramp(EFFECT, N);
    rig.sounds.play("short.wav");
    rig.blocks(SETTLE);
    TEST_ASSERT_TRUE(rig.events.empty());
    // Any other driver event goes on to the next component.
    PinballEvents out;
    TEST_ASSERT_FALSE(rig.sounds.translate({0, DriverEventType::TOUCH_DOWN, 0, 0}, out));
}

// Checks the driver's volume of the music and of every effect channel.
static void expectLevels(const Rig& rig, float music, float effects) {
    TEST_ASSERT_EQUAL_FLOAT(music, rig.audio.levels[Sounds::MUSIC]);
    for (uint8_t c = Sounds::MUSIC + 1; c < AudioDriver::CHANNELS; ++c) {
        TEST_ASSERT_EQUAL_FLOAT(effects, rig.audio.levels[c]);
    }
}

static void testVolume() {
    Rig rig;
    rig.sounds.musicVolume(0.5f);
    expectLevels(rig, 0.25f, 0);
    rig.sounds.effectsVolume(0.3f);
    expectLevels(rig, 0.25f, 0.09f);
    rig.sounds.musicVolume(-1);
    expectLevels(rig, 0, 0.09f);
    rig.sounds.effectsVolume(2);
    expectLevels(rig, 0, 1);
    rig.sounds.effectsVolume(-1);
    expectLevels(rig, 0, 0);
    rig.sounds.musicVolume(2);
    expectLevels(rig, 1, 0);
}

static void testBalance() {
    Rig rig;
    // Both at 1 share the output equally.
    rig.sounds.effectsVolume(1);
    expectLevels(rig, 0.5f, 0.5f);
    // 1 and 0.25 squared add up to 1.25, so both are divided by it.
    rig.sounds.effectsVolume(0.5f);
    expectLevels(rig, 0.8f, 0.2f);
    // The music kept its volume of 1 and gets it back once the effects leave room.
    rig.sounds.effectsVolume(0);
    expectLevels(rig, 1, 0);
    // Squares that add up to 1 exactly stay as they are.
    rig.sounds.musicVolume(0.6f);
    rig.sounds.effectsVolume(0.8f);
    expectLevels(rig, 0.36f, 0.64f);
    // reset() hands the driver the scaled volumes again.
    rig.sounds.musicVolume(1);
    rig.sounds.effectsVolume(1);
    GameHost::reset(rig.sounds);
    expectLevels(rig, 0.5f, 0.5f);
}

static void testStartVolumes() {
    // The defaults add up to less than 1 squared and stay.
    {
        Rig rig;
        rig.sounds.begin();
        expectLevels(rig, Sounds::MUSIC_VOLUME * Sounds::MUSIC_VOLUME,
                     Sounds::EFFECTS_VOLUME * Sounds::EFFECTS_VOLUME);
    }
    // Both at 1 share the output, as when the two setters set them, and a value outside 0 to 1
    // counts as the nearer end.
    Rig rig;
    Sounds::Settings settings;
    settings.musicVolume = 1;
    settings.effectsVolume = 2;
    rig.sounds.configure(settings);
    rig.sounds.begin();
    expectLevels(rig, 0.5f, 0.5f);
    settings.musicVolume = -1;
    rig.sounds.configure(settings);
    rig.sounds.begin();
    expectLevels(rig, 0, 1);
}

static void testNames() {
    Rig rig;
    rig.sounds.play("e1.wav");
    TEST_ASSERT_EQUAL_STRING("/games/a/audio/e1.wav", rig.audio.files[1].c_str());
    rig.sounds.music(LOW, "a.wav");
    TEST_ASSERT_EQUAL_STRING("/games/a/audio/a.wav", rig.audio.files[0].c_str());
    // A name too long for a path finds no file, and the effect ends at once.
    const std::string longName(FILE_PATH_BYTES, 'x');
    rig.sounds.play(longName.c_str());
    TEST_ASSERT_EQUAL_STRING("", rig.audio.files[2].c_str());
    rig.blocks(1, true);
    bool ended = false;
    for (const DriverEvent& e : rig.held) {
        ended = ended || e.source == 2;
    }
    TEST_ASSERT_TRUE(ended);
}

static void testReset() {
    Rig rig;
    rig.sounds.music(LOW, "a.wav");
    rig.sounds.music(MIDDLE, "b.wav");
    VideoPlayer::sound(rig.sounds, "v.wav");
    rig.sounds.play("e1.wav");
    rig.sounds.play("e2.wav");
    rig.sounds.play("e3.wav");
    rig.blocks(SETTLE);
    rig.sounds.musicVolume(0.5f);
    rig.sounds.effectsVolume(0.3f);
    // Something else has changed the channels' volumes meanwhile.
    for (uint8_t c = 0; c < AudioDriver::CHANNELS; ++c) {
        rig.audio.volume(c, 1);
    }
    GameHost::reset(rig.sounds);
    TEST_ASSERT_EQUAL_FLOAT(0.25f, rig.audio.levels[0]);
    for (uint8_t c = 0; c < AudioDriver::CHANNELS; ++c) {
        TEST_ASSERT_EQUAL_UINT32(1, rig.audio.stops[c]);
        if (c != Sounds::MUSIC) {
            TEST_ASSERT_EQUAL_FLOAT(0.09f, rig.audio.levels[c]);
        }
    }
    TEST_ASSERT_TRUE(rig.sounds.musicEnded(LOW));
    TEST_ASSERT_TRUE(rig.sounds.musicEnded(MIDDLE));
    TEST_ASSERT_FALSE(VideoPlayer::sounding(rig.sounds));
    rig.blocks(PLAY_OUT);
    for (uint8_t c = 0; c < AudioDriver::CHANNELS; ++c) {
        TEST_ASSERT_FALSE(rig.audio.playing(c));
    }
    TEST_ASSERT_TRUE(rig.events.empty());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(testChannels);
    RUN_TEST(testVideoSound);
    RUN_TEST(testVideoSoundTop);
    RUN_TEST(testTracks);
    RUN_TEST(testSamePass);
    RUN_TEST(testMusicEnded);
    RUN_TEST(testVolume);
    RUN_TEST(testBalance);
    RUN_TEST(testStartVolumes);
    RUN_TEST(testNames);
    RUN_TEST(testReset);
    return UNITY_END();
}
