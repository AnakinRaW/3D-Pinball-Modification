// Tests on the computer the file channel, which reads files through the storage driver's requests
// from a fake card in memory. The test plays the storage task and serves a read only when it lets
// the card answer.
//
//   whole-file   take() gives every byte once and in order, and ended() turns true after the last
//   from-byte    a file started at a byte gives that byte first
//   loop         a file that loops runs on from its first byte after its last
//   slow-card    reads held back give fewer bytes, and later the rest
//   restart      a new file drops the read of the old one still on its way
//   stop         stop() drops the file and its read on its way
//   failed-read  the bytes ahead of a failed read come out, then ended() turns true, and
//                failed() tells the failure from a file that ran out
//   skip         take() with nullptr skips bytes
//   nothing      a missing file and a channel never started count as ended at once
//
// Run: pio test -e computer, which runs in CI as well.

#include "io/file-channel.h"
#include <map>
#include <string>
#include <unity.h>
#include <vector>

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

    // Serves every waiting request.
    void serveAll() {
        while (serveNext()) {
        }
    }
};

// A card with the service mounted on it.
struct Rig {
    FakeCard card;
    TestService service{card};
    Rig() { service.mount(); }
};

// Gets a file of n bytes whose byte i is i modulo 251, a prime, so no two chunks look alike.
static std::string pattern(size_t n) {
    std::string s(n, '\0');
    for (size_t i = 0; i < n; ++i) {
        s[i] = char(i % 251);
    }
    return s;
}

// Takes bytes until the channel has ended, serving the card between takes, and gets them.
template <uint8_t CHUNKS>
static std::string drain(Rig& rig, FileChannel<CHUNKS>& channel, size_t step, size_t most) {
    std::string out;
    std::vector<char> buffer(step);
    while (!channel.ended() && out.size() < most) {
        rig.service.serveAll();
        const size_t n = channel.take(buffer.data(), step);
        out.append(buffer.data(), n);
    }
    return out;
}

void setUp() {}
void tearDown() {}

static void testWholeFile() {
    const size_t sizes[] = {1, 63, 64, 65, 200, 1000};
    const uint32_t chunks[] = {1, 16, 64};
    for (size_t size : sizes) {
        for (uint32_t chunk : chunks) {
            Rig rig;
            rig.card.files["/f"] = pattern(size);
            std::vector<uint8_t> ring(3 * chunk);
            FileChannel<3> channel(ring.data(), chunk);
            channel.start(rig.service.open("/f"), false);
            TEST_ASSERT_FALSE(channel.ended());
            const std::string got = drain(rig, channel, 7, 2 * size);
            TEST_ASSERT_TRUE(got == rig.card.files["/f"]);
            TEST_ASSERT_TRUE(channel.ended());
            TEST_ASSERT_EQUAL_UINT32(0, channel.available());
        }
    }
}

static void testFromByte() {
    Rig rig;
    rig.card.files["/f"] = pattern(300);
    uint8_t ring[4 * 32];
    FileChannel<4> channel(ring, 32);
    channel.start(rig.service.open("/f"), false, 123);
    const std::string got = drain(rig, channel, 10, 1000);
    TEST_ASSERT_TRUE(got == rig.card.files["/f"].substr(123));
}

static void testLoop() {
    Rig rig;
    const std::string file = pattern(100);
    rig.card.files["/f"] = file;
    uint8_t ring[3 * 32];
    FileChannel<3> channel(ring, 32);
    channel.start(rig.service.open("/f"), true);
    const std::string got = drain(rig, channel, 9, 450);
    TEST_ASSERT_TRUE(got.size() >= 450);
    for (size_t i = 0; i < got.size(); ++i) {
        TEST_ASSERT_EQUAL_INT8(file[i % file.size()], got[i]);
    }
    TEST_ASSERT_FALSE(channel.ended());
}

static void testSlowCard() {
    Rig rig;
    rig.card.files["/f"] = pattern(256);
    uint8_t ring[2 * 64];
    FileChannel<2> channel(ring, 64);
    channel.start(rig.service.open("/f"), false);
    char buffer[256];
    // No read has ended yet.
    TEST_ASSERT_EQUAL_UINT32(0, channel.take(buffer, sizeof buffer));
    TEST_ASSERT_FALSE(channel.ended());
    std::string got;
    while (!channel.ended()) {
        // One read ends between two takes, which ask for more than the ring holds.
        rig.service.serveNext();
        const size_t n = channel.take(buffer, sizeof buffer);
        TEST_ASSERT_TRUE(n <= 64);
        got.append(buffer, n);
    }
    TEST_ASSERT_TRUE(got == rig.card.files["/f"]);
}

static void testRestart() {
    Rig rig;
    rig.card.files["/old"] = std::string(64, 'o');
    rig.card.files["/new"] = pattern(100);
    uint8_t ring[2 * 32];
    FileChannel<2> channel(ring, 32);
    channel.start(rig.service.open("/old"), false);
    channel.start(rig.service.open("/new"), false);
    // The old file's read ends, its bytes land in the ring and are dropped.
    const std::string got = drain(rig, channel, 16, 1000);
    TEST_ASSERT_TRUE(got == rig.card.files["/new"]);
}

static void testStop() {
    Rig rig;
    rig.card.files["/f"] = pattern(100);
    uint8_t ring[2 * 32];
    FileChannel<2> channel(ring, 32);
    channel.start(rig.service.open("/f"), true);
    channel.stop();
    rig.service.serveAll();
    char buffer[32];
    TEST_ASSERT_EQUAL_UINT32(0, channel.take(buffer, sizeof buffer));
    TEST_ASSERT_TRUE(channel.ended());
    rig.service.serveAll();
    TEST_ASSERT_EQUAL_UINT32(0, channel.available());
}

static void testFailedRead() {
    Rig rig;
    rig.card.files["/f"] = pattern(200);
    uint8_t ring[4 * 32];
    FileChannel<4> channel(ring, 32);
    channel.start(rig.service.open("/f"), true);
    // Two reads work, the third fails.
    rig.service.serveNext();
    channel.available();
    rig.service.serveNext();
    channel.available();
    TEST_ASSERT_FALSE(channel.failed());
    rig.card.failNext = 1;
    rig.service.serveNext();
    TEST_ASSERT_EQUAL_UINT32(64, channel.available());
    TEST_ASSERT_FALSE(channel.ended());
    TEST_ASSERT_TRUE(channel.failed());
    char buffer[100];
    TEST_ASSERT_EQUAL_UINT32(64, channel.take(buffer, sizeof buffer));
    TEST_ASSERT_TRUE(std::string(buffer, 64) == rig.card.files["/f"].substr(0, 64));
    TEST_ASSERT_TRUE(channel.ended());
    // A file that runs out ends without a failure, and a new start forgets the failure.
    rig.card.files["/g"] = pattern(10);
    channel.start(rig.service.open("/g"), false);
    TEST_ASSERT_FALSE(channel.failed());
    rig.service.serveAll();
    TEST_ASSERT_EQUAL_UINT32(10, channel.take(buffer, sizeof buffer));
    TEST_ASSERT_TRUE(channel.ended());
    TEST_ASSERT_FALSE(channel.failed());
}

static void testSkip() {
    Rig rig;
    rig.card.files["/f"] = pattern(100);
    uint8_t ring[4 * 32];
    FileChannel<4> channel(ring, 32);
    channel.start(rig.service.open("/f"), false);
    // The skip runs across two chunks, so it takes two reads.
    size_t skipped = 0;
    while (skipped < 40) {
        rig.service.serveAll();
        skipped = skipped + channel.take(nullptr, 40 - skipped);
    }
    const std::string got = drain(rig, channel, 13, 1000);
    TEST_ASSERT_TRUE(got == rig.card.files["/f"].substr(40));
}

static void testNothing() {
    Rig rig;
    uint8_t ring[32];
    FileChannel<1> missing(ring, 32);
    missing.start(rig.service.open("/missing"), true);
    rig.service.serveAll();
    char buffer[8];
    TEST_ASSERT_EQUAL_UINT32(0, missing.take(buffer, sizeof buffer));
    TEST_ASSERT_TRUE(missing.ended());
    FileChannel<1> unstarted(ring, 32);
    TEST_ASSERT_TRUE(unstarted.ended());
    TEST_ASSERT_EQUAL_UINT32(0, unstarted.take(buffer, sizeof buffer));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(testWholeFile);
    RUN_TEST(testFromByte);
    RUN_TEST(testLoop);
    RUN_TEST(testSlowCard);
    RUN_TEST(testRestart);
    RUN_TEST(testStop);
    RUN_TEST(testFailedRead);
    RUN_TEST(testSkip);
    RUN_TEST(testNothing);
    return UNITY_END();
}
