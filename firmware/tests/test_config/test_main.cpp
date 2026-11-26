// Tests on the computer the config component on a fake card in memory. The test plays the storage
// task, hands the component the text of a file on that card and reads keys it makes up itself.
//
//   defaults    without a file, with an empty one, one too large and one that does not parse,
//               every read gets its default or no value. No file is written
//   values      values in sections, lists of values and lists of sections read as the file holds
//               them. count() gets each list's length
//   clamp       a value outside its range reads as the nearer end, with a log line
//   wrong       text and a section read as their default or as no value, with a log line
//   switches    true, yes, on and 1 read as on, false, no, off and 0 as off, in any case. Other
//               text, a section and a missing key take the default with a log line
//   numbers     signs, fractions and exponents read as written, text that only starts like a
//               number reads as no number
//   limits      what the tree cannot hold is left out with a log line that names its line and
//               column. The rest reads
//   set         set() adds sections, lists and entries at a list's end. It refuses the rest
//               without a change to the tree
//   remove      remove() takes an entry with everything under it. A list closes its gap
//   save        the file save() writes holds every entry of the tree, the unknown ones included
//   round-trip  a saved file reads back as the same tree, text with YAML's marks included
//   save-busy   a save during the previous one is refused. The file holds the first
//   save-full   a tree whose text exceeds the file's limit is not saved
//
// Run: pio test -e computer, which runs in CI as well.

#include "components/config.h"
#include "logging/logger.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <unity.h>

constexpr char PATH[] = "/config.yml";

// The fake card keeps its files in memory. Each file is a string. Each slot names the file it
// holds.
struct FakeCard : CardFiles {
    std::map<std::string, std::string> files;
    std::string slotPath[SLOTS];

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

// The service with the storage task played by the test: a caller that waits serves the next
// request, as the storage task would meanwhile.
struct TestService : StorageService {
    explicit TestService(FakeCard& card) : StorageService(card) {}
    ~TestService() = default;

    void pause() override { serveNext(); }

    void serveAll() {
        while (serveNext()) {
        }
    }
};

// Keeps a log file in memory. Its writes end at once.
struct LogFile : AppendFile {
    std::string content;
    AppendStatus state = AppendStatus::IDLE;

    void write(uint32_t at, const uint8_t* data, size_t n) override {
        content.resize(at);
        content.append(reinterpret_cast<const char*>(data), n);
        state = AppendStatus::SUCCEEDED;
    }

    void rewrite(const uint8_t* data, size_t n) override {
        content.clear();
        write(0, data, n);
    }

    AppendStatus status() const override { return state; }
};

// The firmware's log for the tests that read it. It lives as long as the program, since
// logWarning() keeps writing to the logger that began last.
static LogFile logA;
static LogFile logB;
static uint8_t logBuffer[16384];
static Logger logger(logA, logB, logBuffer, sizeof(logBuffer));
static size_t logRead = 0;   // how much of the log the tests have looked at

// Gets the lines the config wrote since the last call.
static std::string newLines() {
    logger.flush();
    logger.update();
    logger.update();
    const std::string fresh = logA.content.substr(logRead);
    logRead = logA.content.size();
    std::string lines;
    size_t start = 0;
    for (size_t end = fresh.find('\n'); end != std::string::npos; end = fresh.find('\n', start)) {
        const std::string line = fresh.substr(start, end - start + 1);
        if (line.find(" config ") != std::string::npos) {
            lines += line;
        }
        start = end + 1;
    }
    return lines;
}

// Gets how many lines a text holds.
static size_t lineCount(const std::string& text) {
    return size_t(std::count(text.begin(), text.end(), '\n'));
}

// Holds a card, its storage service and file system, and a config on them.
struct Rig {
    FakeCard card;
    TestService storage{card};
    FileSystem files{storage};
    Config config{files};

    explicit Rig(const char* text = nullptr) {
        if (text != nullptr) {
            card.files[PATH] = text;
        }
        TEST_ASSERT_TRUE(storage.mount());
    }

    // Saves and serves the write, and gets the file.
    std::string saved() {
        TEST_ASSERT_TRUE(config.save());
        storage.serveAll();
        TEST_ASSERT_TRUE(config.saveStatus() == FileStatus::SUCCEEDED);
        return card.files[PATH];
    }
};

void setUp() {
    newLines();
}
void tearDown() {}

static void testDefaults() {
    {
        Rig rig;
        rig.config.begin();
        TEST_ASSERT_EQUAL_DOUBLE(4, rig.config.get("servo.start_deg", 4, 0, 10));
        TEST_ASSERT_FALSE(rig.config.get("servo.end_deg", 0, 10).has_value());
        TEST_ASSERT_EQUAL_UINT16(0, rig.config.count("ir.channels"));
        rig.storage.serveAll();
        TEST_ASSERT_EQUAL_UINT32(0, rig.card.files.count(PATH));
        const std::string lines = newLines();
        TEST_ASSERT_TRUE(lines.find("I config /config.yml missing or empty") != std::string::npos);
        TEST_ASSERT_TRUE(lines.find("I config servo.start_deg missing, 4 taken") !=
                         std::string::npos);
        TEST_ASSERT_EQUAL_UINT32(2, lineCount(lines));
    }
    {
        Rig rig("");
        rig.config.begin();
        TEST_ASSERT_EQUAL_DOUBLE(4, rig.config.get("a", 4, 0, 10));
        TEST_ASSERT_TRUE(newLines().find("missing or empty") != std::string::npos);
    }
    {
        const std::string large = "a: 1\n" + std::string(Config::FILE_BYTES, '#');
        Rig rig(large.c_str());
        rig.config.begin();
        TEST_ASSERT_EQUAL_DOUBLE(4, rig.config.get("a", 4, 0, 10));
        TEST_ASSERT_TRUE(newLines().find("W config /config.yml larger than 8192 bytes") !=
                         std::string::npos);
    }
    {
        const char broken[] = "a: 1\nservo:\n  start_deg: [1, 2\n  end_deg: 3\n";
        Rig rig(broken);
        rig.config.begin();
        TEST_ASSERT_EQUAL_DOUBLE(4, rig.config.get("a", 4, 0, 10));
        TEST_ASSERT_FALSE(rig.config.get("servo.end_deg", 0, 10).has_value());
        const std::string lines = newLines();
        // libyaml names where the list that never closes ended, past its start.
        TEST_ASSERT_TRUE(lines.find("W config /config.yml:") != std::string::npos);
        TEST_ASSERT_TRUE(lines.find("every setting at its default") != std::string::npos);
        rig.storage.serveAll();
        TEST_ASSERT_EQUAL_STRING(broken, rig.card.files[PATH].c_str());
    }
    {
        Rig rig("- 1\n- 2\n");
        rig.config.begin();
        TEST_ASSERT_TRUE(newLines().find("the top holds no section") != std::string::npos);
    }
}

static void testValues() {
    Rig rig("# the machine's settings\n"
            "servo:\n"
            "  start_deg: 5\n"
            "  speed_deg_s: 80   # degrees per second\n"
            "solenoid:\n"
            "  pull_ms: [30, 25, 20]\n"
            "ir:\n"
            "  channels:\n"
            "    - {channel: 0, order: 3}\n"
            "    - channel: 1\n"
            "      order: 2\n"
            "top: 0.5\n");
    rig.config.begin();
    TEST_ASSERT_EQUAL_DOUBLE(5, rig.config.get("servo.start_deg", 0, 0, 60));
    TEST_ASSERT_EQUAL_DOUBLE(80, *rig.config.get("servo.speed_deg_s", 10, 200));
    TEST_ASSERT_EQUAL_DOUBLE(30, rig.config.get("solenoid.pull_ms.0", 1, 0, 50));
    TEST_ASSERT_EQUAL_DOUBLE(20, rig.config.get("solenoid.pull_ms.2", 1, 0, 50));
    TEST_ASSERT_EQUAL_DOUBLE(3, *rig.config.get("ir.channels.0.order", 0, 15));
    TEST_ASSERT_EQUAL_DOUBLE(1, *rig.config.get("ir.channels.1.channel", 0, 15));
    TEST_ASSERT_EQUAL_DOUBLE(2, *rig.config.get("ir.channels.1.order", 0, 15));
    TEST_ASSERT_EQUAL_DOUBLE(0.5, rig.config.get("top", 0, 0, 1));
    TEST_ASSERT_EQUAL_UINT16(3, rig.config.count("solenoid.pull_ms"));
    TEST_ASSERT_EQUAL_UINT16(2, rig.config.count("ir.channels"));
    TEST_ASSERT_EQUAL_UINT16(0, rig.config.count("servo"));
    TEST_ASSERT_EQUAL_UINT16(0, rig.config.count("nothing"));
    TEST_ASSERT_FALSE(rig.config.get("solenoid.pull_ms.3", 0, 50).has_value());
    TEST_ASSERT_FALSE(rig.config.get("servo..start_deg", 0, 60).has_value());
    TEST_ASSERT_FALSE(rig.config.get("servo.start_deg.", 0, 60).has_value());
    TEST_ASSERT_FALSE(rig.config.get("", 0, 60).has_value());
    TEST_ASSERT_EQUAL_STRING("", newLines().c_str());
}

static void testClamp() {
    Rig rig("high: 12\nlow: -3\n");
    rig.config.begin();
    TEST_ASSERT_EQUAL_DOUBLE(10, rig.config.get("high", 5, 0, 10));
    TEST_ASSERT_EQUAL_DOUBLE(0.5, rig.config.get("low", 5, 0.5, 10));
    TEST_ASSERT_EQUAL_DOUBLE(10, *rig.config.get("high", 0, 10));
    const std::string lines = newLines();
    TEST_ASSERT_TRUE(lines.find("W config high holds 12, outside 0 to 10, 10 taken") !=
                     std::string::npos);
    TEST_ASSERT_TRUE(lines.find("W config low holds -3, outside 0.5 to 10, 0.5 taken") !=
                     std::string::npos);
    TEST_ASSERT_EQUAL_UINT32(3, lineCount(lines));
}

static void testWrong() {
    Rig rig("text: fast\nsection:\n  a: 1\nunset: ~\nnull: null\nempty:\n");
    rig.config.begin();
    TEST_ASSERT_EQUAL_DOUBLE(5, rig.config.get("text", 5, 0, 10));
    TEST_ASSERT_FALSE(rig.config.get("text", 0, 10).has_value());
    TEST_ASSERT_EQUAL_DOUBLE(5, rig.config.get("section", 5, 0, 10));
    TEST_ASSERT_FALSE(rig.config.get("section", 0, 10).has_value());
    std::string lines = newLines();
    TEST_ASSERT_TRUE(lines.find("W config text holds no number, 5 taken") != std::string::npos);
    TEST_ASSERT_TRUE(lines.find("W config text holds no number, left unset") != std::string::npos);
    TEST_ASSERT_TRUE(lines.find("W config section holds no number, 5 taken") != std::string::npos);
    TEST_ASSERT_EQUAL_UINT32(4, lineCount(lines));
    // A key that holds ~, null or nothing is unset and takes its default without a line.
    for (const char* key : {"unset", "null", "empty"}) {
        TEST_ASSERT_EQUAL_DOUBLE(5, rig.config.get(key, 5, 0, 10));
        TEST_ASSERT_FALSE(rig.config.get(key, 0, 10).has_value());
    }
    TEST_ASSERT_EQUAL_STRING("", newLines().c_str());
}

static void testSwitches() {
    Rig rig("on: [true, Yes, ON, 1]\n"
            "off: [false, No, off, 0]\n"
            "unset: ~\n"
            "text: maybe\n"
            "section:\n"
            "  a: 1\n");
    rig.config.begin();
    for (int i = 0; i < 4; ++i) {
        const std::string on = "on." + std::to_string(i);
        const std::string off = "off." + std::to_string(i);
        TEST_ASSERT_TRUE(rig.config.get(on.c_str(), false));
        TEST_ASSERT_FALSE(rig.config.get(off.c_str(), true));
    }
    TEST_ASSERT_TRUE(rig.config.get("unset", true));
    TEST_ASSERT_EQUAL_STRING("", newLines().c_str());
    TEST_ASSERT_TRUE(rig.config.get("text", true));
    TEST_ASSERT_FALSE(rig.config.get("section", false));
    TEST_ASSERT_TRUE(rig.config.get("missing", true));
    const std::string lines = newLines();
    TEST_ASSERT_TRUE(lines.find("W config text holds no switch, true taken") != std::string::npos);
    TEST_ASSERT_TRUE(lines.find("W config section holds no switch, false taken") !=
                     std::string::npos);
    TEST_ASSERT_TRUE(lines.find("I config missing missing, true taken") != std::string::npos);
    TEST_ASSERT_EQUAL_UINT32(3, lineCount(lines));
}

static void testNumbers() {
    Rig rig("plus: +7\nminus: -2.5\nexp: 1e3\nsmall: 2.5E-2\npoint: .5\ntrail: 5.\nzero: -0\n"
            "long: 3.14159265358979\nhex: 0x10\nunit: 12mm\nhuge: 1e999\nlone: e5\ndot: .\n"
            "expless: 1e\n");
    rig.config.begin();
    constexpr double MIN = -1e6;
    constexpr double MAX = 1e6;
    TEST_ASSERT_EQUAL_DOUBLE(7, *rig.config.get("plus", MIN, MAX));
    TEST_ASSERT_EQUAL_DOUBLE(-2.5, *rig.config.get("minus", MIN, MAX));
    TEST_ASSERT_EQUAL_DOUBLE(1000, *rig.config.get("exp", MIN, MAX));
    TEST_ASSERT_EQUAL_DOUBLE(0.025, *rig.config.get("small", MIN, MAX));
    TEST_ASSERT_EQUAL_DOUBLE(0.5, *rig.config.get("point", MIN, MAX));
    TEST_ASSERT_EQUAL_DOUBLE(5, *rig.config.get("trail", MIN, MAX));
    TEST_ASSERT_EQUAL_DOUBLE(0, *rig.config.get("zero", MIN, MAX));
    TEST_ASSERT_EQUAL_DOUBLE(3.14159265358979, *rig.config.get("long", MIN, MAX));
    for (const char* key : {"hex", "unit", "huge", "lone", "dot", "expless"}) {
        TEST_ASSERT_EQUAL_DOUBLE(9, rig.config.get(key, 9, MIN, MAX));
    }
    TEST_ASSERT_EQUAL_UINT32(6, lineCount(newLines()));
}

static void testLimits() {
    {
        Rig rig("name_longer_than_twenty_three_characters: 1\n"
                "short: a value longer than twenty-three characters\n"
                "\"a.b\": 2\n"
                "twice: 1\n"
                "twice: 2\n"
                "anchor: &x 3\n"
                "alias: *x\n"
                "l1: {l2: {l3: {l4: {l5: {l6: {l7: {l8: {l9: {l10: {l11: {l12: {l13: {l14: {l15: "
                "{l16: {l17: 1}, fine: 2}}}}}}}}}}}}}}}\n"
                "kept: 4\n");
        rig.config.begin();
        const std::string lines = newLines();
        TEST_ASSERT_TRUE(lines.find("W config /config.yml:3:1: the name a.b holds a dot or a 0 "
                                    "byte, left out") != std::string::npos);
        TEST_ASSERT_TRUE(lines.find("W config /config.yml:5:1: twice stands twice, the second "
                                    "left out") != std::string::npos);
        TEST_ASSERT_TRUE(lines.find("W config /config.yml:7:1: alias refers to an anchor, left "
                                    "out") != std::string::npos);
        TEST_ASSERT_TRUE(lines.find(":8:") != std::string::npos);
        TEST_ASSERT_TRUE(lines.find("l16 lies deeper than 16 levels, left out") !=
                         std::string::npos);
        TEST_ASSERT_EQUAL_UINT32(4, lineCount(lines));
        TEST_ASSERT_EQUAL_DOUBLE(
            1, *rig.config.get("name_longer_than_twenty_three_characters", 0, 10));
        TEST_ASSERT_FALSE(rig.config.get("short", 0, 10).has_value());
        TEST_ASSERT_EQUAL_DOUBLE(1, *rig.config.get("twice", 0, 10));
        TEST_ASSERT_EQUAL_DOUBLE(3, *rig.config.get("anchor", 0, 10));
        TEST_ASSERT_FALSE(rig.config.get("alias", 0, 10).has_value());
        TEST_ASSERT_EQUAL_DOUBLE(
            2, *rig.config.get("l1.l2.l3.l4.l5.l6.l7.l8.l9.l10.l11.l12.l13.l14.l15.fine", 0, 10));
        TEST_ASSERT_EQUAL_DOUBLE(4, *rig.config.get("kept", 0, 10));
        // The read of short above logs that it holds no number.
        TEST_ASSERT_EQUAL_UINT32(1, lineCount(newLines()));
    }
    {
        // The file holds a section with more values than the tree has entries.
        std::string many = "s:\n";
        for (uint32_t i = 0; i < Config::ENTRIES + 10; ++i) {
            many += "  k" + std::to_string(i) + ": " + std::to_string(i) + "\n";
        }
        Rig rig(many.c_str());
        rig.config.begin();
        const std::string lines = newLines();
        // The section takes the first entry, so the values k0 to k510 fill the rest.
        const std::string full = "/config.yml:" + std::to_string(Config::ENTRIES + 1) +
                                 ":3: more than " + std::to_string(Config::ENTRIES) +
                                 " entries, the rest left out";
        TEST_ASSERT_TRUE(lines.find(full) != std::string::npos);
        TEST_ASSERT_EQUAL_UINT32(1, lineCount(lines));
        const std::string last = "s.k" + std::to_string(Config::ENTRIES - 2);
        const std::string past = "s.k" + std::to_string(Config::ENTRIES - 1);
        TEST_ASSERT_EQUAL_DOUBLE(Config::ENTRIES - 2, *rig.config.get(last.c_str(), 0, 1000));
        TEST_ASSERT_FALSE(rig.config.get(past.c_str(), 0, 1000).has_value());
    }
}

static void testSet() {
    Rig rig("kept: 1\n");
    rig.config.begin();
    TEST_ASSERT_TRUE(rig.config.set("a.b", 1));
    TEST_ASSERT_TRUE(rig.config.set("a.b", 2));
    TEST_ASSERT_TRUE(rig.config.set("list.0", 5));
    TEST_ASSERT_TRUE(rig.config.set("list.1", 6));
    TEST_ASSERT_TRUE(rig.config.set("rows.0.x", 1));
    TEST_ASSERT_TRUE(rig.config.set("rows.0.y", 2));
    TEST_ASSERT_TRUE(rig.config.set("rows.1.x", 3));
    TEST_ASSERT_TRUE(rig.config.set("fine", 0.1234567));
    TEST_ASSERT_TRUE(rig.config.set("tiny", -0.0000001));
    TEST_ASSERT_TRUE(rig.config.set("big", -999999999999.5));
    TEST_ASSERT_EQUAL_DOUBLE(2, *rig.config.get("a.b", 0, 10));
    TEST_ASSERT_EQUAL_DOUBLE(6, *rig.config.get("list.1", 0, 10));
    TEST_ASSERT_EQUAL_DOUBLE(3, *rig.config.get("rows.1.x", 0, 10));
    TEST_ASSERT_EQUAL_DOUBLE(0.123457, *rig.config.get("fine", 0, 10));
    TEST_ASSERT_EQUAL_DOUBLE(0, *rig.config.get("tiny", -1, 1));
    TEST_ASSERT_EQUAL_DOUBLE(-999999999999.5, *rig.config.get("big", -1e12, 0));
    TEST_ASSERT_EQUAL_UINT16(2, rig.config.count("list"));
    TEST_ASSERT_EQUAL_UINT16(2, rig.config.count("rows"));
    const std::string before = rig.saved();
    TEST_ASSERT_FALSE(rig.config.set("list.3", 7));    // past the list's end
    TEST_ASSERT_FALSE(rig.config.set("list.x", 7));    // a name in a list
    TEST_ASSERT_FALSE(rig.config.set("new.1", 7));     // a new list starts at 0
    TEST_ASSERT_FALSE(rig.config.set("a", 7));         // a section
    TEST_ASSERT_FALSE(rig.config.set("list", 7));      // a list
    TEST_ASSERT_FALSE(rig.config.set("a.b.c", 7));     // under a value
    TEST_ASSERT_FALSE(rig.config.set("new.0.1", 7));   // a new list's entry past 0
    TEST_ASSERT_FALSE(rig.config.set("b.b.b.b.b.b.b.b.b.b.b.b.b.b.b.b.b", 7));   // past the levels
    TEST_ASSERT_FALSE(rig.config.set("a..c", 7));
    TEST_ASSERT_FALSE(rig.config.set("v", Config::VALUE_LIMIT));
    TEST_ASSERT_FALSE(rig.config.set("v", -Config::VALUE_LIMIT));
    TEST_ASSERT_FALSE(rig.config.set("v", NAN));
    TEST_ASSERT_FALSE(rig.config.set("v", INFINITY));
    TEST_ASSERT_EQUAL_STRING(before.c_str(), rig.saved().c_str());
    TEST_ASSERT_EQUAL_STRING("", newLines().c_str());

    // A tree with one free entry refuses a key that needs two and takes one that needs one.
    Rig full;
    full.config.begin();
    for (uint32_t i = 0; i + 1 < Config::ENTRIES; ++i) {
        TEST_ASSERT_TRUE(full.config.set(("k" + std::to_string(i)).c_str(), i));
    }
    const std::string filled = full.saved();
    TEST_ASSERT_FALSE(full.config.set("new.key", 1));
    TEST_ASSERT_EQUAL_STRING(filled.c_str(), full.saved().c_str());
    TEST_ASSERT_TRUE(full.config.set("last", 1));
    TEST_ASSERT_FALSE(full.config.set("more", 1));
}

static void testRemove() {
    Rig rig("list: [10, 11, 12, 13]\nsection:\n  a: 1\n  b: {c: 2}\nkept: 5\n");
    rig.config.begin();
    rig.config.remove("list.1");
    TEST_ASSERT_EQUAL_UINT16(3, rig.config.count("list"));
    TEST_ASSERT_EQUAL_DOUBLE(10, *rig.config.get("list.0", 0, 100));
    TEST_ASSERT_EQUAL_DOUBLE(12, *rig.config.get("list.1", 0, 100));
    TEST_ASSERT_EQUAL_DOUBLE(13, *rig.config.get("list.2", 0, 100));
    TEST_ASSERT_FALSE(rig.config.get("list.3", 0, 100).has_value());
    rig.config.remove("section");
    TEST_ASSERT_FALSE(rig.config.get("section.a", 0, 100).has_value());
    TEST_ASSERT_FALSE(rig.config.get("section.b.c", 0, 100).has_value());
    rig.config.remove("nothing");
    rig.config.remove("list.9");
    TEST_ASSERT_EQUAL_DOUBLE(5, *rig.config.get("kept", 0, 100));
    TEST_ASSERT_TRUE(rig.config.set("list.3", 14));
    TEST_ASSERT_EQUAL_STRING("list: [10, 12, 13, 14]\nkept: 5\n", rig.saved().c_str());
}

static void testSave() {
    Rig rig("unknown: 7\n"
            "ir:\n"
            "  channels:\n"
            "    - {channel: 0, order: 1}\n"
            "    - {channel: 1, order: 0}\n");
    rig.config.begin();
    TEST_ASSERT_TRUE(rig.config.saveStatus() == FileStatus::IDLE);
    rig.config.remove("ir.channels");
    TEST_ASSERT_TRUE(rig.config.set("ir.channels.0.channel", 2));
    TEST_ASSERT_TRUE(rig.config.set("ir.channels.0.order", 0));
    TEST_ASSERT_TRUE(rig.config.set("ir.channels.1.channel", 3));
    TEST_ASSERT_TRUE(rig.config.set("ir.channels.1.order", 1));
    TEST_ASSERT_TRUE(rig.config.save());
    TEST_ASSERT_TRUE(rig.config.saveStatus() == FileStatus::PENDING);
    rig.storage.serveAll();
    TEST_ASSERT_TRUE(rig.config.saveStatus() == FileStatus::SUCCEEDED);
    TEST_ASSERT_EQUAL_STRING("unknown: 7\n"
                             "ir:\n"
                             "  channels:\n"
                             "    - {channel: 2, order: 0}\n"
                             "    - {channel: 3, order: 1}\n",
                             rig.card.files[PATH].c_str());
    TEST_ASSERT_EQUAL_STRING("", newLines().c_str());
}

static void testRoundTrip() {
    Rig first("text:\n"
              "  colon: \"a: b\"\n"
              "  hash: \"x # y\"\n"
              "  quote: \"say \\\"hi\\\"\"\n"
              "  slash: \"a\\\\b\"\n"
              "  tab: \"a\\tb\"\n"
              "  dash: \"- x\"\n"
              "  amp: \"&a\"\n"
              "  space: \" a \"\n"
              "  empty:\n"
              "  tilde: ~\n"
              "  range: 5-6\n"
              "  negative: -3\n"
              "  word: Grün\n"
              "lists:\n"
              "  values: [1, 2, 3]\n"
              "  none: []\n"
              "  sections:\n"
              "    - {a: 1, b: x}\n"
              "    - nested:\n"
              "        c: 2\n"
              "    - {}\n"
              "  lists:\n"
              "    - [1, \"a, b\"]\n"
              "    - []\n"
              "    - - [5]\n"
              "empty_section: {}\n");
    first.config.begin();
    TEST_ASSERT_EQUAL_STRING("", newLines().c_str());
    const std::string text = first.saved();
    TEST_ASSERT_TRUE(text.find("  colon: \"a: b\"\n") != std::string::npos);
    TEST_ASSERT_TRUE(text.find("  quote: \"say \\\"hi\\\"\"\n") != std::string::npos);
    TEST_ASSERT_TRUE(text.find("  tab: \"a\\tb\"\n") != std::string::npos);
    TEST_ASSERT_TRUE(text.find("  range: 5-6\n") != std::string::npos);
    TEST_ASSERT_TRUE(text.find("  empty: ~\n") != std::string::npos);
    TEST_ASSERT_TRUE(text.find("  values: [1, 2, 3]\n") != std::string::npos);
    TEST_ASSERT_TRUE(text.find("    - {a: 1, b: x}\n") != std::string::npos);
    Rig second(text.c_str());
    second.config.begin();
    TEST_ASSERT_EQUAL_STRING("", newLines().c_str());
    TEST_ASSERT_EQUAL_STRING(text.c_str(), second.saved().c_str());
    TEST_ASSERT_EQUAL_DOUBLE(2, *second.config.get("lists.sections.1.nested.c", 0, 10));
    TEST_ASSERT_EQUAL_DOUBLE(5, *second.config.get("lists.lists.2.0.0", 0, 10));
    TEST_ASSERT_EQUAL_DOUBLE(-3, *second.config.get("text.negative", -10, 10));
    TEST_ASSERT_EQUAL_UINT16(3, second.config.count("lists.sections"));
}

static void testSaveBusy() {
    Rig rig("a: 1\n");
    rig.config.begin();
    TEST_ASSERT_TRUE(rig.config.set("a", 2));
    TEST_ASSERT_TRUE(rig.config.save());
    TEST_ASSERT_TRUE(rig.config.set("a", 3));
    TEST_ASSERT_FALSE(rig.config.save());
    rig.storage.serveAll();
    TEST_ASSERT_EQUAL_STRING("a: 2\n", rig.card.files[PATH].c_str());
    TEST_ASSERT_EQUAL_STRING("a: 3\n", rig.saved().c_str());
}

static void testSaveFull() {
    Rig rig("a: 1\n");
    rig.config.begin();
    for (uint32_t i = 0; i < Config::ENTRIES - 1; ++i) {
        const std::string key = "setting_long_name_" + std::to_string(i % 10);
        const std::string section = "section_" + std::to_string(i / 10);
        rig.config.set((section + "." + key).c_str(), 123456789.123456);
    }
    TEST_ASSERT_FALSE(rig.config.save());
    rig.storage.serveAll();
    TEST_ASSERT_TRUE(rig.config.saveStatus() == FileStatus::IDLE);
    TEST_ASSERT_EQUAL_STRING("a: 1\n", rig.card.files[PATH].c_str());
    TEST_ASSERT_TRUE(newLines().find("W config the settings take more than 8192 bytes") !=
                     std::string::npos);
}

int main() {
    logger.begin(LogFileStart{}, LogFileStart{});
    UNITY_BEGIN();
    RUN_TEST(testDefaults);
    RUN_TEST(testValues);
    RUN_TEST(testClamp);
    RUN_TEST(testWrong);
    RUN_TEST(testSwitches);
    RUN_TEST(testNumbers);
    RUN_TEST(testLimits);
    RUN_TEST(testSet);
    RUN_TEST(testRemove);
    RUN_TEST(testSave);
    RUN_TEST(testRoundTrip);
    RUN_TEST(testSaveBusy);
    RUN_TEST(testSaveFull);
    return UNITY_END();
}
