// Tests on the computer which files the file system component gives a game, the host and the
// other components. The tests look at each file's path. No file reaches a card.
//
//   paths   a path inside the game's directory gives the file in /games/<id>/. An empty path, one
//           from the root, one with .., one too long and one before a game is named give a file
//           whose requests fail. A full path reaches the card's root
//   switch  a file keeps its path after the host names another game. The next file lies in the
//           other game's directory
//   root    once the host names the card's root, a path counts from there, and a path that leaves
//           it is refused. The next game named gets its directory again
//
// Run: pio test -e computer, which runs in CI as well.

#include "components/file-system.h"
#include <string>
#include <unity.h>

// A card that is never mounted. Every member answers that nothing works.
struct NoCard : CardFiles {
    bool mount() override { return false; }
    bool remount() override { return false; }
    uint32_t failedTransfers() const override { return 0; }
    uint32_t errorCode() const override { return 0; }
    bool open(uint8_t, const char*, bool) override { return false; }
    void close(uint8_t) override {}
    uint32_t size(uint8_t) override { return 0; }
    bool read(uint8_t, uint32_t, uint8_t*, size_t, size_t&) override { return false; }
    bool write(uint8_t, uint32_t, const uint8_t*, size_t) override { return false; }
    bool cut(uint8_t, uint32_t) override { return false; }
    bool commit(uint8_t) override { return false; }
};

struct Service : StorageService {
    explicit Service(CardFiles& card) : StorageService(card) {}
    ~Service() = default;
};

// The host's side of the file system, as the game host reaches it.
class GameHost {
public:
    static void use(FileSystem& files, const char* game) { files.use(game); }
    static void useRoot(FileSystem& files) { files.useRoot(); }
    static FileWriter openFull(const FileSystem& files, const char* path) {
        return files.openFull(path);
    }
};

void setUp() {}
void tearDown() {}

static void testPaths() {
    NoCard card;
    Service storage(card);
    FileSystem files(storage);
    TEST_ASSERT_EQUAL_STRING("", files.open("best").name());
    GameHost::use(files, "a");
    TEST_ASSERT_EQUAL_STRING("/games/a/best", files.open("best").name());
    TEST_ASSERT_EQUAL_STRING("/games/a/audio/1.wav", files.open("audio/1.wav").name());
    TEST_ASSERT_EQUAL_STRING("", files.open("").name());
    TEST_ASSERT_EQUAL_STRING("", files.open("/log-a.txt").name());
    TEST_ASSERT_EQUAL_STRING("", files.open("../b/best").name());
    TEST_ASSERT_EQUAL_STRING("", files.open("audio/../../b/best").name());
    // "/games/a/" takes 9 bytes, so 54 more fill the path to its end.
    const std::string longest(FILE_PATH_BYTES - 1 - 9, 'x');
    TEST_ASSERT_EQUAL_STRING(("/games/a/" + longest).c_str(), files.open(longest.c_str()).name());
    const std::string tooLong = longest + "x";
    TEST_ASSERT_EQUAL_STRING("", files.open(tooLong.c_str()).name());
    // A file whose path was refused fails every request at once.
    FileRequest r;
    uint8_t into[1];
    files.open("").read(0, into, sizeof into, r);
    TEST_ASSERT_TRUE(r.status() == FileStatus::FAILED);
    TEST_ASSERT_EQUAL_STRING("/config.yml", GameHost::openFull(files, "/config.yml").name());
}

static void testSwitch() {
    NoCard card;
    Service storage(card);
    FileSystem files(storage);
    GameHost::use(files, "a");
    const FileWriter first = files.open("best");
    GameHost::use(files, "b");
    const FileWriter second = files.open("best");
    TEST_ASSERT_EQUAL_STRING("/games/a/best", first.name());
    TEST_ASSERT_EQUAL_STRING("/games/b/best", second.name());
}

static void testRoot() {
    NoCard card;
    Service storage(card);
    FileSystem files(storage);
    GameHost::useRoot(files);
    const FileWriter film = files.open("video/intro.rgb");
    TEST_ASSERT_EQUAL_STRING("/video/intro.rgb", film.name());
    TEST_ASSERT_EQUAL_STRING("", files.open("").name());
    TEST_ASSERT_EQUAL_STRING("", files.open("/video/intro.rgb").name());
    TEST_ASSERT_EQUAL_STRING("", files.open("../intro.rgb").name());
    // "/" takes 1 byte, so 62 more fill the path to its end.
    const std::string longest(FILE_PATH_BYTES - 1 - 1, 'x');
    TEST_ASSERT_EQUAL_STRING(("/" + longest).c_str(), files.open(longest.c_str()).name());
    const std::string tooLong = longest + "x";
    TEST_ASSERT_EQUAL_STRING("", files.open(tooLong.c_str()).name());
    GameHost::use(files, "a");
    TEST_ASSERT_EQUAL_STRING("/games/a/video/intro.rgb", files.open("video/intro.rgb").name());
    TEST_ASSERT_EQUAL_STRING("/video/intro.rgb", film.name());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(testPaths);
    RUN_TEST(testSwitch);
    RUN_TEST(testRoot);
    return UNITY_END();
}
