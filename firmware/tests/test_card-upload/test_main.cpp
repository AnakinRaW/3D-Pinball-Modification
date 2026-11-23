// Tests on the computer the card upload on a fake link and a fake card in memory. The test plays
// the computer and the storage task, and moves the clock itself.
//
//   put         a file in several chunks lands on the card whole. A shorter file replaces it
//   empty       a file of no bytes empties the file on the card
//   checksum    a chunk whose checksum does not match is refused. Sent again, it lands. The
//               checksum is the one zlib.crc32() computes
//   refusals    a wrong path, a wrong command and a chunk past the file are refused. A line
//               without CARD gets no answer
//   card-fails  a write the card refuses ends the file
//   quiet       a file with no byte for QUIET_MS ends with a failure
//   restart     RESTART restarts the Teensy RESTART_MS after its answer
//   slow        commands and chunks that arrive a byte at a time land whole
//
// Run: pio test -e computer, which runs in CI as well.

#include "components/card-upload.h"
#include "core/clock.h"
#include <Arduino.h>
#include <algorithm>
#include <map>
#include <string>
#include <unity.h>
#include <vector>

constexpr uint32_t US_PER_MS = 1000;
constexpr uint32_t PASSES = 64;   // more passes than any step of a test needs

// The fake card keeps its files in memory. Each file is a string. Each slot names the file it
// holds.
struct FakeCard : CardFiles {
    std::map<std::string, std::string> files;
    std::string slotPath[SLOTS];
    bool failWrites = false;

    bool mount() override { return true; }
    bool remount() override { return true; }
    uint32_t failedTransfers() const override { return 0; }
    uint32_t errorCode() const override { return 0; }
    bool open(uint8_t slot, const char* path, bool writes) override {
        if (files.count(path) == 0) {
            if (!writes) {
                return false;
            }
            files[path] = "";
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
    bool write(uint8_t slot, uint32_t at, const uint8_t* data, size_t n) override {
        if (failWrites) {
            return false;
        }
        std::string& f = files[slotPath[slot]];
        if (f.size() < at + n) {
            f.resize(at + n);
        }
        f.replace(at, n, reinterpret_cast<const char*>(data), n);
        return true;
    }
    bool cut(uint8_t slot, uint32_t bytes) override {
        files[slotPath[slot]].resize(bytes);
        return true;
    }
    bool commit(uint8_t) override { return true; }
};

struct TestService : StorageService {
    explicit TestService(FakeCard& card) : StorageService(card) {}
    ~TestService() = default;

    void serveAll() {
        while (serveNext()) {
        }
    }
};

// The computer's side: the bytes it sent that the upload has not taken yet, and the answers.
struct FakeLink : UploadLink {
    std::string sent;
    size_t taken = 0;
    size_t perRead = 1u << 20;   // the most bytes one read() hands over
    std::vector<std::string> answers;

    size_t read(uint8_t* to, size_t n) override {
        const size_t k = std::min({n, sent.size() - taken, perRead});
        sent.copy(reinterpret_cast<char*>(to), k, taken);
        taken += k;
        return k;
    }

    void answer(const char* line, size_t n) override { answers.emplace_back(line, n); }
};

static uint32_t restarts = 0;

static void countRestart() {
    restarts = restarts + 1;
}

// Gets the CRC-32 of a text as zlib.crc32() computes it, bit by bit.
static uint32_t crc(const std::string& s) {
    uint32_t c = 0xFFFFFFFF;
    for (const char ch : s) {
        c ^= uint8_t(ch);
        for (int b = 0; b < 8; ++b) {
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
        }
    }
    return ~c;
}

// Gets the command and the bytes of one chunk.
static std::string chunk(const std::string& bytes, uint32_t sum) {
    char header[64];
    snprintf(header, sizeof header, "CARD DATA %u %08x\n", unsigned(bytes.size()), unsigned(sum));
    return header + bytes;
}

static std::string chunk(const std::string& bytes) {
    return chunk(bytes, crc(bytes));
}

// A card with its files, the computer, and the upload between them.
struct Rig {
    FakeCard card;
    TestService storage{card};
    FileSystem files{storage};
    FakeLink link;
    CardUpload upload{link, files, countRestart};

    Rig() {
        TEST_ASSERT_TRUE(storage.mount());
        restarts = 0;
        GPT2_CNT = 0;
    }

    // Sends bytes, then runs the upload and the card until both have nothing left to do.
    void send(const std::string& bytes) {
        link.sent += bytes;
        for (uint32_t i = 0; i < PASSES; ++i) {
            upload.update();
            storage.serveAll();
        }
    }

    // Gets the answers since the last call, joined by a |.
    std::string answers() {
        std::string joined;
        for (const std::string& a : link.answers) {
            joined += (joined.empty() ? "" : "|") + a.substr(0, a.size() - 1);
        }
        link.answers.clear();
        return joined;
    }
};

void setUp() {}
void tearDown() {}

static std::string pattern(size_t n, uint32_t seed) {
    std::string s(n, '\0');
    for (size_t i = 0; i < n; ++i) {
        s[i] = char((i * 7 + seed * 13 + (i >> 8)) & 0xFF);
    }
    return s;
}

static void testPut() {
    Rig rig;
    const std::string file = pattern(10000, 1);
    rig.send("CARD PUT /games/a/game.yml 10000\n");
    TEST_ASSERT_EQUAL_STRING("CARD READY", rig.answers().c_str());
    TEST_ASSERT_TRUE(rig.upload.receiving());
    rig.send(chunk(file.substr(0, 4096)));
    rig.send(chunk(file.substr(4096, 4096)));
    rig.send(chunk(file.substr(8192)));
    TEST_ASSERT_EQUAL_STRING("CARD OK 4096|CARD OK 8192|CARD DONE 10000", rig.answers().c_str());
    TEST_ASSERT_FALSE(rig.upload.receiving());
    TEST_ASSERT_TRUE(rig.card.files["/games/a/game.yml"] == file);
    // A shorter file in its place leaves nothing of the longer one.
    rig.send("CARD PUT /games/a/game.yml 3\n" + chunk("abc"));
    TEST_ASSERT_EQUAL_STRING("CARD READY|CARD DONE 3", rig.answers().c_str());
    TEST_ASSERT_EQUAL_STRING("abc", rig.card.files["/games/a/game.yml"].c_str());
}

static void testEmpty() {
    Rig rig;
    rig.card.files["/config.yml"] = "old";
    rig.send("CARD PUT /config.yml 0\n");
    TEST_ASSERT_EQUAL_STRING("CARD DONE 0", rig.answers().c_str());
    TEST_ASSERT_EQUAL_STRING("", rig.card.files["/config.yml"].c_str());
    TEST_ASSERT_FALSE(rig.upload.receiving());
}

static void testChecksum() {
    Rig rig;
    // 0xCBF43926 is the CRC-32 of "123456789" that zlib.crc32() gives.
    rig.send("CARD PUT /config.yml 9\n" + chunk("123456789", 0xCBF43927));
    TEST_ASSERT_EQUAL_STRING("CARD READY|CARD FAIL checksum", rig.answers().c_str());
    TEST_ASSERT_EQUAL_STRING("", rig.card.files["/config.yml"].c_str());
    TEST_ASSERT_TRUE(rig.upload.receiving());
    rig.send(chunk("123456789", 0xCBF43926));
    TEST_ASSERT_EQUAL_STRING("CARD DONE 9", rig.answers().c_str());
    TEST_ASSERT_EQUAL_STRING("123456789", rig.card.files["/config.yml"].c_str());
}

static void testRefusals() {
    Rig rig;
    const std::string tooLong = "/" + std::string(FILE_PATH_BYTES, 'x');
    const std::string lines[] = {
        "CARD PUT config.yml 4\n",      "CARD PUT /games/../config.yml 4\n",
        "CARD PUT " + tooLong + " 4\n", "CARD PUT /config.yml\n",
        "CARD PUT /config.yml -4\n",    "CARD PUT /config.yml 4x\n",
    };
    for (const std::string& line : lines) {
        rig.send(line);
        const std::string answer = rig.answers();
        TEST_ASSERT_EQUAL_STRING_LEN("CARD FAIL ", answer.c_str(), 10);
        TEST_ASSERT_FALSE(rig.upload.receiving());
    }
    rig.send("CARD DATA 3 0\n");
    TEST_ASSERT_EQUAL_STRING("CARD FAIL no file is being received", rig.answers().c_str());
    rig.send("CARD FORMAT\n");
    TEST_ASSERT_EQUAL_STRING("CARD FAIL unknown command", rig.answers().c_str());
    rig.send("hello\n" + std::string(CardUpload::LINE_BYTES * 2, 'C') + "\n");
    TEST_ASSERT_EQUAL_STRING("", rig.answers().c_str());
    // A chunk past the file, a chunk larger than CHUNK_BYTES and a malformed chunk end the file.
    rig.send("CARD PUT /config.yml 4\nCARD DATA 5 0\n");
    TEST_ASSERT_EQUAL_STRING("CARD READY|CARD FAIL the chunk is empty, too large or runs past "
                             "the file",
                             rig.answers().c_str());
    rig.send("CARD PUT /big 10000\nCARD DATA 4097 0\n");
    TEST_ASSERT_EQUAL_STRING("CARD READY|CARD FAIL the chunk is empty, too large or runs past "
                             "the file",
                             rig.answers().c_str());
    rig.send("CARD PUT /big 10000\nCARD DATA 10 xyz\n");
    TEST_ASSERT_EQUAL_STRING("CARD READY|CARD FAIL DATA takes a size and a checksum",
                             rig.answers().c_str());
    TEST_ASSERT_FALSE(rig.upload.receiving());
}

static void testCardFails() {
    Rig rig;
    rig.card.failWrites = true;
    rig.send("CARD PUT /config.yml 3\n" + chunk("abc"));
    TEST_ASSERT_EQUAL_STRING("CARD READY|CARD FAIL the card did not take the chunk",
                             rig.answers().c_str());
    TEST_ASSERT_FALSE(rig.upload.receiving());
    rig.card.failWrites = false;
    rig.send("CARD DATA 3 0\n");
    TEST_ASSERT_EQUAL_STRING("CARD FAIL no file is being received", rig.answers().c_str());
}

static void testQuiet() {
    Rig rig;
    rig.send("CARD PUT /config.yml 8\n");
    TEST_ASSERT_EQUAL_STRING("CARD READY", rig.answers().c_str());
    GPT2_CNT = (CardUpload::QUIET_MS - 1) * US_PER_MS;
    rig.send("");
    TEST_ASSERT_EQUAL_STRING("", rig.answers().c_str());
    GPT2_CNT = CardUpload::QUIET_MS * US_PER_MS;
    rig.send("");
    TEST_ASSERT_EQUAL_STRING("CARD FAIL no byte for too long", rig.answers().c_str());
    TEST_ASSERT_FALSE(rig.upload.receiving());
    // Inside a chunk as well.
    rig.send("CARD PUT /config.yml 8\n" + std::string("CARD DATA 8 0\nabcd"));
    TEST_ASSERT_EQUAL_STRING("CARD READY", rig.answers().c_str());
    GPT2_CNT = GPT2_CNT + CardUpload::QUIET_MS * US_PER_MS;
    rig.send("");
    TEST_ASSERT_EQUAL_STRING("CARD FAIL no byte for too long", rig.answers().c_str());
}

static void testRestart() {
    Rig rig;
    rig.send("CARD RESTART\n");
    TEST_ASSERT_EQUAL_STRING("CARD RESTARTING", rig.answers().c_str());
    TEST_ASSERT_EQUAL_UINT32(0, restarts);
    GPT2_CNT = (CardUpload::RESTART_MS - 1) * US_PER_MS;
    rig.send("");
    TEST_ASSERT_EQUAL_UINT32(0, restarts);
    GPT2_CNT = CardUpload::RESTART_MS * US_PER_MS;
    rig.upload.update();
    TEST_ASSERT_EQUAL_UINT32(1, restarts);
    Rig inFile;
    inFile.send("CARD PUT /config.yml 3\nCARD RESTART\n");
    TEST_ASSERT_EQUAL_STRING("CARD READY|CARD FAIL the restart ends the file|CARD RESTARTING",
                             inFile.answers().c_str());
}

static void testSlow() {
    Rig rig;
    rig.link.perRead = 1;
    const std::string file = pattern(5000, 2);
    rig.link.sent =
        "CARD PUT /slow.bin 5000\n" + chunk(file.substr(0, 4096)) + chunk(file.substr(4096));
    // A byte at a time takes a pass for every byte.
    for (size_t i = 0; i < rig.link.sent.size() * 2; ++i) {
        rig.upload.update();
        rig.storage.serveAll();
    }
    TEST_ASSERT_EQUAL_STRING("CARD READY|CARD OK 4096|CARD DONE 5000", rig.answers().c_str());
    TEST_ASSERT_TRUE(rig.card.files["/slow.bin"] == file);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(testPut);
    RUN_TEST(testEmpty);
    RUN_TEST(testChecksum);
    RUN_TEST(testRefusals);
    RUN_TEST(testCardFails);
    RUN_TEST(testQuiet);
    RUN_TEST(testRestart);
    RUN_TEST(testSlow);
    return UNITY_END();
}
