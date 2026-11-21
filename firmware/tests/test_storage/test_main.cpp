// Tests on the computer the storage driver's requests, their order, its files and its handling of a
// failing card, on a fake card in memory. The test plays the storage task and serves the requests
// one at a time.
//
//   service-order  a higher priority goes before a lower one, each in the order it came
//   busy           a second operation on a pending request makes both fail
//   long-path      a path too long gives a file whose requests fail. Other files still work
//   read-ends      a read stops at the end of the file. A missing file and a position at or past
//                  the end bring 0 bytes and succeed
//   past-end       a write past the end of a file fails without counting as a card error
//   open-files     more files than slots close the one used longest ago. A write reopens a file
//                  read before
//   card-errors    a card error starts the card anew. The card counts as failed after the third
//                  in a row, or when it does not come back. Every request then fails at once
//   recover        a failed card that answers again works again. Another card does not
//   refused        a caller that may not start a request gets a failed request at once
//   wait-ends      wait() and the destructor return once the request has ended. wait() refuses
//                  where waiting is not allowed
//   cache-path     a read lands in its buffer straight only where the cache cannot spoil it
//   card-file      the card appender's file starts writes and reports their end
//
// Run: pio test -e computer, which runs in CI as well.

#include "drivers/storage-service.h"
#include <algorithm>
#include <map>
#include <string>
#include <unity.h>

constexpr uint32_t CODE = 0x2A;   // the error code the fake card reports

// A card in memory. Each file is a string. Each slot names the file it holds.
struct FakeCard : CardFiles {
    std::map<std::string, std::string> files;
    std::string slotPath[SLOTS];
    bool slotOpen[SLOTS] = {};
    bool slotWrites[SLOTS] = {};
    bool present = true;     // a card answers
    bool sameCard = true;    // the card that answers is the one mount() found
    uint32_t failNext = 0;   // transfers that fail from now on
    uint32_t failedCount = 0;
    uint32_t mounts = 0;
    uint32_t remounts = 0;
    uint32_t opens = 0;
    uint32_t closes = 0;

    // Counts a transfer and gets whether it worked.
    bool transfer() {
        if (!present || failNext > 0) {
            failNext = failNext > 0 ? failNext - 1 : 0;
            failedCount = failedCount + 1;
            return false;
        }
        return true;
    }

    bool mount() override {
        mounts = mounts + 1;
        return present;
    }
    bool remount() override {
        remounts = remounts + 1;
        for (uint8_t i = 0; i < SLOTS; ++i) {
            TEST_ASSERT_FALSE(slotOpen[i]);
        }
        return present && sameCard;
    }
    uint32_t failedTransfers() const override { return failedCount; }
    uint32_t errorCode() const override { return CODE; }
    bool open(uint8_t slot, const char* path, bool writes) override {
        TEST_ASSERT_FALSE(slotOpen[slot]);
        if (!transfer()) {
            return false;
        }
        opens = opens + 1;
        if (files.count(path) == 0) {
            if (!writes) {
                return false;
            }
            files[path] = "";
        }
        slotPath[slot] = path;
        slotOpen[slot] = true;
        slotWrites[slot] = writes;
        return true;
    }
    void close(uint8_t slot) override {
        TEST_ASSERT_TRUE(slotOpen[slot]);
        closes = closes + 1;
        slotOpen[slot] = false;
    }
    uint32_t size(uint8_t slot) override { return uint32_t(files[slotPath[slot]].size()); }
    bool read(uint8_t slot, uint32_t at, uint8_t* to, size_t n, size_t& got) override {
        if (!transfer()) {
            return false;
        }
        const std::string& f = files[slotPath[slot]];
        got = at >= f.size() ? 0 : std::min(n, f.size() - at);
        f.copy(reinterpret_cast<char*>(to), got, at);
        return true;
    }
    bool write(uint8_t slot, uint32_t at, const uint8_t* data, size_t n) override {
        TEST_ASSERT_TRUE(slotWrites[slot]);
        if (!transfer()) {
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
        return transfer();
    }
    bool commit(uint8_t) override { return transfer(); }
};

// The service with the storage task played by the test: a caller that waits serves the next
// request, as the storage task would meanwhile.
struct TestService : StorageService {
    explicit TestService(FakeCard& card) : StorageService(card) {}
    ~TestService() = default;

    bool mayStart = true;
    bool mayWait = true;
    uint32_t wakes = 0;

    // Serves every waiting request.
    void serveAll() {
        while (serveNext()) {
        }
    }

private:
    bool callerMayStart() const override { return mayStart; }
    bool callerMayWait() const override { return mayWait; }
    void wake() override { wakes = wakes + 1; }
    void pause() override { serveNext(); }
};

void setUp() {}
void tearDown() {}

// Writes text into a file through the service and serves the write.
static void put(TestService& s, const char* path, const std::string& text) {
    FileRequest r;
    s.openWriter(path).rewrite(text.data(), text.size(), r);
    s.serveAll();
    TEST_ASSERT_TRUE(r.status() == FileStatus::SUCCEEDED);
}

static void testServiceOrder() {
    FakeCard card;
    card.files["/a"] = "abcdefgh";
    TestService s(card);
    TEST_ASSERT_TRUE(s.mount());
    const FileReader f = s.open("/a");
    char into[6][1];
    FileRequest normal1(FilePriority::NORMAL);
    FileRequest video1(FilePriority::RAISED);
    FileRequest audio1(FilePriority::HIGHEST);
    FileRequest normal2(FilePriority::NORMAL);
    FileRequest audio2(FilePriority::HIGHEST);
    FileRequest video2(FilePriority::RAISED);
    FileRequest* order[] = {&normal1, &video1, &audio1, &normal2, &audio2, &video2};
    for (uint8_t i = 0; i < 6; ++i) {
        f.read(i, into[i], 1, *order[i]);
    }
    TEST_ASSERT_EQUAL_UINT32(6, s.wakes);
    // The order the storage task serves them in.
    FileRequest* const wanted[] = {&audio1, &audio2, &video1, &video2, &normal1, &normal2};
    for (FileRequest* w : wanted) {
        TEST_ASSERT_TRUE(w->status() == FileStatus::PENDING);
        TEST_ASSERT_TRUE(s.serveNext());
        TEST_ASSERT_TRUE(w->status() == FileStatus::SUCCEEDED);
    }
    TEST_ASSERT_FALSE(s.serveNext());
    TEST_ASSERT_EQUAL_INT('c', into[2][0]);
}

static void testBusy() {
    FakeCard card;
    card.files["/a"] = "abc";
    TestService s(card);
    s.mount();
    char into[3];
    FileRequest r;
    s.open("/a").read(0, into, 3, r);
    s.open("/a").read(1, into, 2, r);
    TEST_ASSERT_TRUE(r.status() == FileStatus::PENDING);
    s.serveAll();
    TEST_ASSERT_TRUE(r.status() == FileStatus::FAILED);
    TEST_ASSERT_EQUAL_UINT32(0, r.bytes());
    // The request starts afresh once it has ended.
    s.open("/a").read(1, into, 2, r);
    s.serveAll();
    TEST_ASSERT_TRUE(r.status() == FileStatus::SUCCEEDED);
    TEST_ASSERT_EQUAL_UINT32(2, r.bytes());
}

static void testLongPath() {
    FakeCard card;
    TestService s(card);
    s.mount();
    const std::string longPath = "/" + std::string(FILE_PATH_BYTES, 'x');
    const FileWriter tooLong = s.openWriter(longPath.c_str());
    TEST_ASSERT_EQUAL_STRING("", tooLong.name());
    FileRequest r;
    tooLong.write(0, "a", 1, r);
    TEST_ASSERT_TRUE(r.status() == FileStatus::FAILED);
    char into[1];
    tooLong.read(0, into, 1, r);
    TEST_ASSERT_TRUE(r.status() == FileStatus::FAILED);
    // A path of exactly the longest length still works.
    const std::string longest = "/" + std::string(FILE_PATH_BYTES - 2, 'y');
    put(s, longest.c_str(), "b");
    TEST_ASSERT_EQUAL_STRING("b", card.files[longest].c_str());
    // A file from no service fails as well.
    FileRequest none;
    FileReader().read(0, into, 1, none);
    TEST_ASSERT_TRUE(none.status() == FileStatus::FAILED);
}

static void testReadEnds() {
    FakeCard card;
    card.files["/a"] = "abcde";
    TestService s(card);
    s.mount();
    char into[8] = {};
    FileRequest r;
    s.open("/a").read(3, into, 8, r);
    s.serveAll();
    TEST_ASSERT_TRUE(r.status() == FileStatus::SUCCEEDED);
    TEST_ASSERT_EQUAL_UINT32(2, r.bytes());
    TEST_ASSERT_EQUAL_UINT32(5, r.fileSize());
    TEST_ASSERT_EQUAL_MEMORY("de", into, 2);
    s.open("/a").read(5, into, 8, r);
    s.serveAll();
    TEST_ASSERT_TRUE(r.status() == FileStatus::SUCCEEDED);
    TEST_ASSERT_EQUAL_UINT32(0, r.bytes());
    TEST_ASSERT_EQUAL_UINT32(5, r.fileSize());
    s.open("/missing").read(0, into, 8, r);
    s.serveAll();
    TEST_ASSERT_TRUE(r.status() == FileStatus::SUCCEEDED);
    TEST_ASSERT_EQUAL_UINT32(0, r.bytes());
    TEST_ASSERT_EQUAL_UINT32(0, r.fileSize());
    TEST_ASSERT_EQUAL_UINT32(0, card.files.count("/missing"));
}

static void testPastEnd() {
    FakeCard card;
    card.files["/a"] = "abc";
    TestService s(card);
    s.mount();
    FileRequest r;
    const FileWriter f = s.openWriter("/a");
    f.write(4, "x", 1, r);
    s.serveAll();
    TEST_ASSERT_TRUE(r.status() == FileStatus::FAILED);
    TEST_ASSERT_EQUAL_STRING("abc", card.files["/a"].c_str());
    TEST_ASSERT_EQUAL_UINT32(0, card.remounts);
    TEST_ASSERT_FALSE(s.failedCard());
    // A write right at the end appends.
    f.write(3, "d", 1, r);
    s.serveAll();
    TEST_ASSERT_TRUE(r.status() == FileStatus::SUCCEEDED);
    TEST_ASSERT_EQUAL_STRING("abcd", card.files["/a"].c_str());
    TEST_ASSERT_EQUAL_UINT32(4, r.fileSize());
}

static void testOpenFiles() {
    FakeCard card;
    TestService s(card);
    s.mount();
    std::string paths[CardFiles::SLOTS + 1];
    for (uint8_t i = 0; i <= CardFiles::SLOTS; ++i) {
        paths[i] = "/f" + std::to_string(i);
        card.files[paths[i]] = "x";
    }
    char into[1];
    FileRequest r;
    for (uint8_t i = 0; i < CardFiles::SLOTS; ++i) {
        s.open(paths[i].c_str()).read(0, into, 1, r);
        s.serveAll();
    }
    TEST_ASSERT_EQUAL_UINT32(CardFiles::SLOTS, card.opens);
    TEST_ASSERT_EQUAL_UINT32(0, card.closes);
    // The first file again takes no open.
    s.open("/F0").read(0, into, 1, r);
    s.serveAll();
    TEST_ASSERT_EQUAL_UINT32(CardFiles::SLOTS, card.opens);
    // One file more closes the one used longest ago, now the second.
    s.open(paths[CardFiles::SLOTS].c_str()).read(0, into, 1, r);
    s.serveAll();
    TEST_ASSERT_EQUAL_UINT32(1, card.closes);
    bool secondOpen = false;
    for (uint8_t i = 0; i < CardFiles::SLOTS; ++i) {
        secondOpen = secondOpen || (card.slotOpen[i] && card.slotPath[i] == paths[1]);
    }
    TEST_ASSERT_FALSE(secondOpen);
    // A write to a file open for reading opens it anew for writing.
    s.openWriter(paths[0].c_str()).write(1, "y", 1, r);
    s.serveAll();
    TEST_ASSERT_TRUE(r.status() == FileStatus::SUCCEEDED);
    TEST_ASSERT_EQUAL_UINT32(2, card.closes);
    TEST_ASSERT_EQUAL_STRING("xy", card.files[paths[0]].c_str());
}

static void testCardErrors() {
    FakeCard card;
    card.files["/a"] = "abc";
    TestService s(card);
    s.mount();
    char into[3];
    FileRequest r;
    const FileReader f = s.open("/a");
    // One error starts the card anew. The next read works.
    card.failNext = 1;
    f.read(0, into, 3, r);
    s.serveAll();
    TEST_ASSERT_TRUE(r.status() == FileStatus::FAILED);
    TEST_ASSERT_EQUAL_UINT32(0, r.fileSize());
    TEST_ASSERT_EQUAL_UINT32(1, card.remounts);
    TEST_ASSERT_FALSE(s.failedCard());
    f.read(0, into, 3, r);
    s.serveAll();
    TEST_ASSERT_TRUE(r.status() == FileStatus::SUCCEEDED);
    // A success between errors starts the count anew: two errors, a success, two errors.
    for (uint8_t i = 0; i < 2; ++i) {
        card.failNext = 1;
        f.read(0, into, 3, r);
        s.serveAll();
    }
    f.read(0, into, 3, r);
    s.serveAll();
    TEST_ASSERT_TRUE(r.status() == FileStatus::SUCCEEDED);
    for (uint8_t i = 0; i < 2; ++i) {
        card.failNext = 1;
        f.read(0, into, 3, r);
        s.serveAll();
    }
    TEST_ASSERT_FALSE(s.failedCard());
    // The third in a row makes the card count as failed.
    card.failNext = 1;
    f.read(0, into, 3, r);
    s.serveAll();
    TEST_ASSERT_TRUE(s.failedCard());
    TEST_ASSERT_EQUAL_UINT32(CODE, s.lastError());
    // Every request then fails at once, without a wake.
    const uint32_t wakes = s.wakes;
    f.read(0, into, 3, r);
    TEST_ASSERT_TRUE(r.status() == FileStatus::FAILED);
    TEST_ASSERT_EQUAL_UINT32(wakes, s.wakes);
    // A card that does not come back counts as failed after its first error.
    FakeCard pulled;
    pulled.files["/a"] = "abc";
    TestService t(pulled);
    t.mount();
    pulled.present = false;
    t.open("/a").read(0, into, 3, r);
    t.serveAll();
    TEST_ASSERT_TRUE(t.failedCard());
}

static void testRecover() {
    FakeCard card;
    card.files["/a"] = "abc";
    TestService s(card);
    TEST_ASSERT_TRUE(s.recover() == false);   // never mounted, nothing to take back
    s.mount();
    TEST_ASSERT_TRUE(s.recover());   // the card works, nothing to do
    TEST_ASSERT_EQUAL_UINT32(0, card.remounts);
    card.present = false;
    char into[3];
    FileRequest r;
    s.open("/a").read(0, into, 3, r);
    s.serveAll();
    TEST_ASSERT_TRUE(s.failedCard());
    TEST_ASSERT_FALSE(s.recover());
    // Another card stays unused.
    card.present = true;
    card.sameCard = false;
    TEST_ASSERT_FALSE(s.recover());
    TEST_ASSERT_TRUE(s.failedCard());
    // The same card works again.
    card.sameCard = true;
    TEST_ASSERT_TRUE(s.recover());
    TEST_ASSERT_FALSE(s.failedCard());
    s.open("/a").read(0, into, 3, r);
    s.serveAll();
    TEST_ASSERT_TRUE(r.status() == FileStatus::SUCCEEDED);
    // A card that mount() did not find is never taken up later.
    FakeCard missing;
    missing.present = false;
    TestService t(missing);
    TEST_ASSERT_FALSE(t.mount());
    missing.present = true;
    TEST_ASSERT_FALSE(t.recover());
    TEST_ASSERT_EQUAL_UINT32(0, missing.remounts);
}

static void testRefused() {
    FakeCard card;
    card.files["/a"] = "abc";
    TestService s(card);
    s.mount();
    char into[3];
    FileRequest r;
    s.mayStart = false;
    s.open("/a").read(0, into, 3, r);
    TEST_ASSERT_TRUE(r.status() == FileStatus::FAILED);
    // A pending request stays as it is for such a caller.
    s.mayStart = true;
    s.open("/a").read(0, into, 3, r);
    s.mayStart = false;
    s.open("/a").read(0, into, 3, r);
    TEST_ASSERT_TRUE(r.status() == FileStatus::PENDING);
    s.serveAll();
    TEST_ASSERT_TRUE(r.status() == FileStatus::SUCCEEDED);
    // Before mount() every request fails.
    FakeCard other;
    TestService t(other);
    t.open("/a").read(0, into, 3, r);
    TEST_ASSERT_TRUE(r.status() == FileStatus::FAILED);
}

static void testWaitEnds() {
    FakeCard card;
    card.files["/a"] = "abc";
    TestService s(card);
    s.mount();
    char into[3];
    FileRequest idle;
    TEST_ASSERT_FALSE(idle.wait());
    FileRequest r;
    s.open("/a").read(0, into, 3, r);
    TEST_ASSERT_TRUE(r.wait());
    TEST_ASSERT_EQUAL_UINT32(3, r.bytes());
    s.open("/missing-dir/a").read(0, into, 3, r);
    s.mayWait = false;
    TEST_ASSERT_FALSE(r.wait());
    TEST_ASSERT_TRUE(r.status() == FileStatus::PENDING);
    s.mayWait = true;
    s.serveAll();
    {
        FileRequest scoped;
        s.openWriter("/b").rewrite("xyz", 3, scoped);
        TEST_ASSERT_TRUE(scoped.status() == FileStatus::PENDING);
    }
    // The destructor waited, so the write is on the card and the list holds nothing.
    TEST_ASSERT_EQUAL_STRING("xyz", card.files["/b"].c_str());
    TEST_ASSERT_FALSE(s.serveNext());
}

static void testCachePath() {
    alignas(32) static uint8_t lines[64];
    // RAM1 has no cache, so any read lands there straight.
    TEST_ASSERT_TRUE(readsInPlace(reinterpret_cast<void*>(uintptr_t(0x20000001)), 7, 3));
    TEST_ASSERT_TRUE(readsInPlace(reinterpret_cast<void*>(uintptr_t(0x2007FFFF)), 7, 3));
    // Cached memory needs whole lines and a read from a sector of the file.
    TEST_ASSERT_TRUE(readsInPlace(lines, 512, 64));
    TEST_ASSERT_FALSE(readsInPlace(lines + 1, 512, 32));
    TEST_ASSERT_FALSE(readsInPlace(lines, 512, 33));
    TEST_ASSERT_FALSE(readsInPlace(lines, 100, 64));
    TEST_ASSERT_FALSE(readsInPlace(reinterpret_cast<void*>(uintptr_t(0x20080000)), 7, 3));
}

static void testCardFile() {
    FakeCard card;
    TestService s(card);
    s.mount();
    CardFile file(s.openWriter("/log"));
    AppendFile& f = file;
    uint8_t buffer[64];
    CardAppender appender(buffer, sizeof buffer);
    appender.start(f);
    appender.append("ab", 2);
    appender.flush();
    appender.update();
    s.serveAll();
    appender.append("cd", 2);
    appender.update();   // takes the end of the first write
    appender.flush();
    appender.update();
    s.serveAll();
    appender.update();
    TEST_ASSERT_EQUAL_STRING("abcd", card.files["/log"].c_str());
    // A failing write reports as failed. The appender keeps its records.
    card.failNext = 1;
    appender.append("e", 1);
    appender.flush();
    appender.update();
    s.serveAll();
    appender.update();
    TEST_ASSERT_EQUAL_STRING("abcd", card.files["/log"].c_str());
    TEST_ASSERT_EQUAL_UINT32(0, appender.lost());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(testServiceOrder);
    RUN_TEST(testBusy);
    RUN_TEST(testLongPath);
    RUN_TEST(testReadEnds);
    RUN_TEST(testPastEnd);
    RUN_TEST(testOpenFiles);
    RUN_TEST(testCardErrors);
    RUN_TEST(testRecover);
    RUN_TEST(testRefused);
    RUN_TEST(testWaitEnds);
    RUN_TEST(testCachePath);
    RUN_TEST(testCardFile);
    return UNITY_END();
}
