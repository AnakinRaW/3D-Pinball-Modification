// Tests on the computer the YAML tree, on texts of files the test writes itself and in rooms it
// sizes itself.
//
//   walk        a file of sections, lists of values and lists of sections reads entry by entry,
//               by name, by number and by key
//   positions   every entry keeps the line and the column where it stands in the file
//   problems    every problem reaches the report with its place. A broken file empties the
//               tree
//   long-text   names, values and texts over several lines keep their whole text
//   text-room   a file with more text than the room is cut there with one problem
//   change      add(), setText() and remove() change the tree. They refuse what it cannot take
//   compact     changing texts again and again in a small room keeps every text right
//   write       the tree's text reads back as the same tree. Too small a room is refused
//   numbers     readNumber(), readIndex() and isUnset() read what a file holds
//
// Run: pio test -e computer, which runs in CI as well.

#include "components/yaml-tree.h"
#include <string>
#include <unity.h>
#include <vector>

// Collects every problem as "line:column: text", with a ! for a broken file.
struct Collect : YamlTree::Report {
    std::vector<std::string> lines;

    void found(const YamlTree::Problem& p) override {
        lines.push_back(std::to_string(p.line) + ":" + std::to_string(p.column) + ": " + p.text +
                        (p.broken ? " !" : ""));
    }
};

// Holds a tree in rooms of a given size.
struct Rig {
    std::vector<YamlTree::Entry> entries;
    std::vector<char> text;
    YamlTree tree;
    Collect report;

    explicit Rig(uint16_t count = 64, size_t bytes = 4096)
        : entries(count), text(bytes), tree(entries.data(), count, text.data(), bytes) {}

    bool parse(const std::string& yaml) { return tree.parse(yaml.data(), yaml.size(), report); }

    // Gets the tree written as text.
    std::string written() {
        std::vector<char> out(8192);
        size_t n = 0;
        TEST_ASSERT_TRUE(tree.write(out.data(), out.size(), n));
        return std::string(out.data(), n);
    }

    YamlTree::Id at(const char* key) { return tree.find(key); }
};

void setUp() {}
void tearDown() {}

static void testWalk() {
    Rig rig;
    TEST_ASSERT_TRUE(rig.parse("modes:\n"
                               "  base: {priority: 10}\n"
                               "  bonus:\n"
                               "    priority: 50\n"
                               "    lamps: [drain, scoop]\n"
                               "shows:\n"
                               "  - {lamp: drain, color: red}\n"
                               "  - lamp: scoop\n"
                               "    color: blue\n"
                               "empty:\n"));
    TEST_ASSERT_EQUAL_UINT32(0, rig.report.lines.size());
    const YamlTree& t = rig.tree;
    TEST_ASSERT_EQUAL_UINT16(3, t.count(t.root()));
    const YamlTree::Id modes = t.first(t.root());
    TEST_ASSERT_EQUAL_STRING("modes", t.name(modes));
    TEST_ASSERT_TRUE(t.kind(modes) == YamlTree::Kind::SECTION);
    TEST_ASSERT_EQUAL_UINT16(t.root(), t.parent(modes));
    const YamlTree::Id shows = t.next(modes);
    TEST_ASSERT_EQUAL_STRING("shows", t.name(shows));
    TEST_ASSERT_TRUE(t.kind(shows) == YamlTree::Kind::LIST);
    const YamlTree::Id empty = t.next(shows);
    TEST_ASSERT_TRUE(t.kind(empty) == YamlTree::Kind::VALUE);
    TEST_ASSERT_EQUAL_STRING("", t.text(empty));
    TEST_ASSERT_EQUAL_UINT16(YamlTree::NONE, t.next(empty));
    TEST_ASSERT_EQUAL_UINT16(YamlTree::NONE, t.first(empty));
    TEST_ASSERT_EQUAL_UINT16(0, t.count(empty));
    // A list's entries have no name and are reached by their numbers.
    const YamlTree::Id second = t.child(shows, "1", 1);
    TEST_ASSERT_EQUAL_STRING("", t.name(second));
    TEST_ASSERT_EQUAL_STRING("blue", t.text(t.child(second, "color", 5)));
    TEST_ASSERT_EQUAL_UINT16(YamlTree::NONE, t.child(shows, "2", 1));
    TEST_ASSERT_EQUAL_UINT16(YamlTree::NONE, t.child(shows, "01", 2));
    TEST_ASSERT_EQUAL_UINT16(YamlTree::NONE, t.child(empty, "x", 1));
    TEST_ASSERT_EQUAL_STRING("scoop", t.text(rig.at("modes.bonus.lamps.1")));
    TEST_ASSERT_EQUAL_STRING("10", t.text(rig.at("modes.base.priority")));
    TEST_ASSERT_EQUAL_STRING("red", t.text(rig.at("shows.0.color")));
    TEST_ASSERT_EQUAL_UINT16(YamlTree::NONE, rig.at("modes.missing"));
    TEST_ASSERT_EQUAL_UINT16(YamlTree::NONE, rig.at("modes..base"));
    TEST_ASSERT_EQUAL_UINT16(YamlTree::NONE, rig.at("modes."));
    TEST_ASSERT_EQUAL_UINT16(YamlTree::NONE, rig.at(""));
    // The entries of a section come in the file's order.
    const YamlTree::Id bonus = rig.at("modes.bonus");
    TEST_ASSERT_EQUAL_STRING("priority", t.name(t.first(bonus)));
    TEST_ASSERT_EQUAL_STRING("lamps", t.name(t.next(t.first(bonus))));
}

static void testPositions() {
    Rig rig;
    TEST_ASSERT_TRUE(rig.parse("# a comment\n"
                               "modes:\n"
                               "  bonus:\n"
                               "    lamps: [drain, scoop]\n"
                               "    rules:\n"
                               "      - add: 5\n"
                               "      -   take: 2\n"));
    const YamlTree& t = rig.tree;
    TEST_ASSERT_EQUAL_UINT16(2, t.line(rig.at("modes")));
    TEST_ASSERT_EQUAL_UINT16(1, t.column(rig.at("modes")));
    TEST_ASSERT_EQUAL_UINT16(3, t.line(rig.at("modes.bonus")));
    TEST_ASSERT_EQUAL_UINT16(3, t.column(rig.at("modes.bonus")));
    TEST_ASSERT_EQUAL_UINT16(4, t.line(rig.at("modes.bonus.lamps")));
    TEST_ASSERT_EQUAL_UINT16(5, t.column(rig.at("modes.bonus.lamps")));
    TEST_ASSERT_EQUAL_UINT16(4, t.line(rig.at("modes.bonus.lamps.1")));
    TEST_ASSERT_EQUAL_UINT16(20, t.column(rig.at("modes.bonus.lamps.1")));
    TEST_ASSERT_EQUAL_UINT16(6, t.line(rig.at("modes.bonus.rules.0")));
    TEST_ASSERT_EQUAL_UINT16(9, t.column(rig.at("modes.bonus.rules.0")));
    TEST_ASSERT_EQUAL_UINT16(7, t.line(rig.at("modes.bonus.rules.1.take")));
    TEST_ASSERT_EQUAL_UINT16(11, t.column(rig.at("modes.bonus.rules.1.take")));
    TEST_ASSERT_EQUAL_UINT16(0, t.line(t.root()));
    const YamlTree::Id made = rig.tree.add(t.root(), "new", 3, YamlTree::Kind::VALUE);
    TEST_ASSERT_EQUAL_UINT16(0, t.line(made));
    TEST_ASSERT_EQUAL_UINT16(0, t.column(made));
}

static void testProblems() {
    {
        Rig rig(6);
        TEST_ASSERT_TRUE(rig.parse("\"a.b\": 1\n"
                                   "twice: 1\n"
                                   "twice: 2\n"
                                   "anchor: &x 3\n"
                                   "list: [1, *x]\n"
                                   "kept: 4\n"
                                   "more: 5\n"
                                   "most: 6\n"
                                   "---\n"
                                   "second: 1\n"));
        const std::vector<std::string> wanted = {
            "1:1: the name a.b holds a dot or a 0 byte, left out",
            "3:1: twice stands twice, the second left out",
            "5:11: entry 1 refers to an anchor, left out",
            "7:1: more than 5 entries, the rest left out",
            "10:1: a second document, left out",
        };
        TEST_ASSERT_EQUAL_UINT32(wanted.size(), rig.report.lines.size());
        for (size_t i = 0; i < wanted.size(); ++i) {
            TEST_ASSERT_EQUAL_STRING(wanted[i].c_str(), rig.report.lines[i].c_str());
        }
        TEST_ASSERT_EQUAL_STRING("1", rig.tree.text(rig.at("twice")));
        TEST_ASSERT_EQUAL_UINT16(1, rig.tree.count(rig.at("list")));
        TEST_ASSERT_EQUAL_STRING("4", rig.tree.text(rig.at("kept")));
        TEST_ASSERT_EQUAL_UINT16(YamlTree::NONE, rig.at("more"));
    }
    {
        Rig rig;
        TEST_ASSERT_TRUE(rig.parse("a: 1\n"));
        TEST_ASSERT_FALSE(rig.parse("a: 1\nb: [1, 2\nc: 3\n"));
        TEST_ASSERT_EQUAL_UINT32(1, rig.report.lines.size());
        TEST_ASSERT_TRUE(rig.report.lines[0].back() == '!');
        TEST_ASSERT_EQUAL_UINT16(0, rig.tree.count(rig.tree.root()));
        TEST_ASSERT_EQUAL_UINT16(YamlTree::NONE, rig.at("a"));
    }
    for (const char* top : {"- 1\n- 2\n", "just text\n", "[1, 2]: x\n"}) {
        Rig rig;
        TEST_ASSERT_FALSE(rig.parse(top));
        TEST_ASSERT_EQUAL_UINT32(1, rig.report.lines.size());
        TEST_ASSERT_TRUE(rig.report.lines[0].back() == '!');
    }
    {
        // A file of comments alone and a lone --- give an empty tree without a problem.
        Rig rig;
        TEST_ASSERT_TRUE(rig.parse("# nothing\n"));
        TEST_ASSERT_TRUE(rig.parse("---\n"));
        TEST_ASSERT_EQUAL_UINT32(0, rig.report.lines.size());
        TEST_ASSERT_EQUAL_UINT16(0, rig.tree.count(rig.tree.root()));
    }
}

static void testLongText() {
    Rig rig;
    TEST_ASSERT_TRUE(rig.parse("a_name_far_longer_than_twenty_three_characters: x\n"
                               "score: \"abs(event.payload) * 2\"\n"
                               "slide: |\n"
                               "  Welcome to the machine.\n"
                               "  Press start to play.\n"
                               "folded: >\n"
                               "  one\n"
                               "  two\n"));
    TEST_ASSERT_EQUAL_UINT32(0, rig.report.lines.size());
    TEST_ASSERT_EQUAL_STRING("x", rig.tree.text(rig.at("a_name_far_longer_than_twenty_three_"
                                                       "characters")));
    TEST_ASSERT_EQUAL_STRING("abs(event.payload) * 2", rig.tree.text(rig.at("score")));
    TEST_ASSERT_EQUAL_STRING("Welcome to the machine.\nPress start to play.\n",
                             rig.tree.text(rig.at("slide")));
    TEST_ASSERT_EQUAL_STRING("one two\n", rig.tree.text(rig.at("folded")));
    Rig back;
    TEST_ASSERT_TRUE(back.parse(rig.written()));
    TEST_ASSERT_EQUAL_STRING("Welcome to the machine.\nPress start to play.\n",
                             back.tree.text(back.at("slide")));
}

static void testTextRoom() {
    // 12 bytes hold "first" and "one" with their 0 bytes. "second" does not fit after them.
    Rig rig(16, 12);
    TEST_ASSERT_TRUE(rig.parse("first: one\nsecond: two\nthird: x\n"));
    TEST_ASSERT_EQUAL_UINT32(1, rig.report.lines.size());
    TEST_ASSERT_EQUAL_STRING("2:1: more than 12 bytes of names and values, the rest left out",
                             rig.report.lines[0].c_str());
    TEST_ASSERT_EQUAL_STRING("one", rig.tree.text(rig.at("first")));
    TEST_ASSERT_EQUAL_UINT16(YamlTree::NONE, rig.at("second"));
    TEST_ASSERT_EQUAL_UINT16(1, rig.tree.count(rig.tree.root()));
}

static void testChange() {
    Rig rig(9, 64);
    YamlTree& t = rig.tree;
    TEST_ASSERT_TRUE(rig.parse("list: [a, b, c]\nvalue: 1\n"));
    const YamlTree::Id section = t.add(t.root(), "section", 7, YamlTree::Kind::SECTION);
    TEST_ASSERT_TRUE(section != YamlTree::NONE);
    const YamlTree::Id x = t.add(section, "x", 1, YamlTree::Kind::VALUE);
    TEST_ASSERT_TRUE(t.setText(x, "long text"));
    TEST_ASSERT_TRUE(t.setText(x, "short"));
    TEST_ASSERT_EQUAL_STRING("short", t.text(x));
    TEST_ASSERT_TRUE(t.setText(x, ""));
    TEST_ASSERT_EQUAL_STRING("", t.text(x));
    TEST_ASSERT_FALSE(t.setText(section, "no"));
    // Refusals: under a value, a name twice, a name with a dot.
    TEST_ASSERT_EQUAL_UINT16(YamlTree::NONE, t.add(rig.at("value"), "y", 1, YamlTree::Kind::VALUE));
    TEST_ASSERT_EQUAL_UINT16(YamlTree::NONE, t.add(t.root(), "value", 5, YamlTree::Kind::VALUE));
    TEST_ASSERT_EQUAL_UINT16(YamlTree::NONE, t.add(t.root(), "a.b", 3, YamlTree::Kind::VALUE));
    // A list takes a new entry at its end. An entry removed moves the later ones down.
    const YamlTree::Id d = t.add(rig.at("list"), "ignored", 7, YamlTree::Kind::VALUE);
    TEST_ASSERT_TRUE(t.setText(d, "d"));
    TEST_ASSERT_EQUAL_STRING("d", t.text(rig.at("list.3")));
    t.remove(rig.at("list.1"));
    TEST_ASSERT_EQUAL_STRING("c", t.text(rig.at("list.1")));
    TEST_ASSERT_EQUAL_STRING("d", t.text(rig.at("list.2")));
    TEST_ASSERT_EQUAL_UINT16(3, t.count(rig.at("list")));
    // The tree holds 7 of its 8 entries besides its top: list with 3, value, section with x.
    TEST_ASSERT_TRUE(t.fits(1, 1));
    TEST_ASSERT_FALSE(t.fits(2, 1));
    TEST_ASSERT_TRUE(t.add(t.root(), "z", 1, YamlTree::Kind::VALUE) != YamlTree::NONE);
    TEST_ASSERT_EQUAL_UINT16(YamlTree::NONE, t.add(t.root(), "w", 1, YamlTree::Kind::VALUE));
    // Removing a section frees it with its entries. Removing the top empties the tree.
    t.remove(section);
    TEST_ASSERT_EQUAL_UINT16(YamlTree::NONE, rig.at("section.x"));
    TEST_ASSERT_TRUE(t.fits(2, 1));
    t.remove(t.root());
    TEST_ASSERT_EQUAL_UINT16(0, t.count(t.root()));
    TEST_ASSERT_TRUE(t.fits(8, 64));
    TEST_ASSERT_FALSE(t.fits(9, 1));
    TEST_ASSERT_FALSE(t.fits(0, 65));
}

static void testCompact() {
    // The text has 48 bytes of room for a few names and values, which change many times.
    Rig rig(8, 48);
    YamlTree& t = rig.tree;
    TEST_ASSERT_TRUE(rig.parse("a: 1\nb: 2\nc: 3\n"));
    for (uint32_t round = 0; round < 200; ++round) {
        const std::string value = std::to_string(round * 7919);
        const char* key = round % 3 == 0 ? "a" : (round % 3 == 1 ? "b" : "c");
        TEST_ASSERT_TRUE(t.setText(rig.at(key), value.c_str()));
        TEST_ASSERT_EQUAL_STRING(value.c_str(), t.text(rig.at(key)));
        if (round % 10 == 0) {
            const YamlTree::Id extra = t.add(t.root(), "extra", 5, YamlTree::Kind::VALUE);
            TEST_ASSERT_TRUE(extra != YamlTree::NONE);
            TEST_ASSERT_TRUE(t.setText(extra, "temporary"));
            t.remove(extra);
        }
    }
    // The last rounds of a, b and c are 198, 199 and 197.
    const std::string a = std::to_string(198 * 7919);
    TEST_ASSERT_EQUAL_STRING(a.c_str(), t.text(rig.at("a")));
    TEST_ASSERT_EQUAL_STRING(std::to_string(199 * 7919).c_str(), t.text(rig.at("b")));
    TEST_ASSERT_EQUAL_STRING(std::to_string(197 * 7919).c_str(), t.text(rig.at("c")));
    // The room left takes a text that fills it, and nothing more.
    const YamlTree::Id fill = t.add(t.root(), "f", 1, YamlTree::Kind::VALUE);
    size_t room = 0;
    while (t.fits(0, room + 2)) {
        ++room;
    }
    TEST_ASSERT_TRUE(t.setText(fill, std::string(room, 'x').c_str()));
    TEST_ASSERT_FALSE(t.setText(rig.at("a"), "longer than anything left"));
    TEST_ASSERT_EQUAL_STRING(a.c_str(), t.text(rig.at("a")));
}

static void testWrite() {
    Rig rig;
    const std::string file = "machine:\n"
                             "  name: \"pin: ball\"\n"
                             "  values: [1, 2, 3]\n"
                             "  none: []\n"
                             "  rows:\n"
                             "    - {a: 1, b: x}\n"
                             "    -\n"
                             "      deep:\n"
                             "        c: 2\n"
                             "    - {}\n"
                             "  blank: ~\n"
                             "empty: {}\n";
    TEST_ASSERT_TRUE(rig.parse(file));
    TEST_ASSERT_EQUAL_STRING(file.c_str(), rig.written().c_str());
    std::vector<char> small(file.size() - 1);
    size_t n = 0;
    TEST_ASSERT_FALSE(rig.tree.write(small.data(), small.size(), n));
    TEST_ASSERT_EQUAL_UINT32(file.size(), n);
    Rig back;
    TEST_ASSERT_TRUE(back.parse(rig.written()));
    TEST_ASSERT_EQUAL_STRING("2", back.tree.text(back.at("machine.rows.1.deep.c")));
    TEST_ASSERT_EQUAL_STRING("pin: ball", back.tree.text(back.at("machine.name")));
}

static void testNumbers() {
    double v = 0;
    TEST_ASSERT_TRUE(YamlTree::readNumber("-2.5e1", v));
    TEST_ASSERT_EQUAL_DOUBLE(-25, v);
    TEST_ASSERT_TRUE(YamlTree::readNumber(".5", v));
    TEST_ASSERT_EQUAL_DOUBLE(0.5, v);
    TEST_ASSERT_FALSE(YamlTree::readNumber("0x10", v));
    TEST_ASSERT_FALSE(YamlTree::readNumber("1e999", v));
    TEST_ASSERT_FALSE(YamlTree::readNumber("", v));
    uint32_t n = 0;
    TEST_ASSERT_TRUE(YamlTree::readIndex("12", 2, n));
    TEST_ASSERT_EQUAL_UINT32(12, n);
    TEST_ASSERT_FALSE(YamlTree::readIndex("012", 3, n));
    TEST_ASSERT_FALSE(YamlTree::readIndex("x", 1, n));
    TEST_ASSERT_FALSE(YamlTree::readIndex("65536", 5, n));
    TEST_ASSERT_TRUE(YamlTree::isUnset(""));
    TEST_ASSERT_TRUE(YamlTree::isUnset("~"));
    TEST_ASSERT_TRUE(YamlTree::isUnset("null"));
    TEST_ASSERT_FALSE(YamlTree::isUnset("0"));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(testWalk);
    RUN_TEST(testPositions);
    RUN_TEST(testProblems);
    RUN_TEST(testLongText);
    RUN_TEST(testTextRoom);
    RUN_TEST(testChange);
    RUN_TEST(testCompact);
    RUN_TEST(testWrite);
    RUN_TEST(testNumbers);
    return UNITY_END();
}
