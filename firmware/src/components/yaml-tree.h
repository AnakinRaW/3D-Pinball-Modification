// YamlTree holds a YAML file as a tree of sections, lists and values, in the memory its user hands
// over. It reads the text of a file with libyaml. Every entry keeps where it stands in the file,
// for the messages about it. The tree is written back as the text of a file. Every value stays the
// text the file holds, so a number, a name and an expression read alike.
//
// Design: docs/firmware/components/config.md.
#pragma once
#include <stddef.h>
#include <stdint.h>

class YamlTree {
public:
    /// Names an entry of the tree. The file's top is root().
    using Id = uint16_t;

    static constexpr Id NONE = 0xFFFF;      // no entry
    static constexpr uint8_t LEVELS = 16;   // config.md: the levels of a tree, its top included

    /// What an entry holds.
    enum class Kind : uint8_t {
        VALUE,     // a text
        SECTION,   // entries by name
        LIST,      // entries by number, counted from 0
    };

    /// Holds one entry of the tree. Its user provides the room for the entries. Only the tree
    /// fills it.
    struct Entry {
        uint32_t name = 0;   // where its name starts in the text, NO_TEXT for none
        uint32_t text = 0;   // where a value's text starts in the text, NO_TEXT for none
        Id parent = 0;
        Id first = 0;   // its first entry
        Id next = 0;    // the entry after it under its parent
        uint16_t line = 0;
        uint16_t column = 0;
        Kind kind = Kind::VALUE;
    };

    /// Describes something parse() found in the file and where it stands.
    struct Problem {
        uint32_t line;      // counted from 1
        uint32_t column;    // counted from 1
        const char* text;   // what is wrong, such as "drain stands twice, the second left out"
        bool broken;        // the file is broken and the tree stays empty
    };

    /// Takes what parse() finds in a file.
    class Report {
    public:
        /// Takes one problem.
        ///
        /// @param problem  The problem. Its text lasts until the call returns.
        virtual void found(const Problem& problem) = 0;

    protected:
        ~Report() = default;
    };

    /// Creates an empty tree in the room its user gives.
    ///
    /// @param entries    Holds the entries, the file's top included. It stays with the tree for
    ///                   the tree's whole life.
    /// @param count      How many entries @p entries holds, at least 1 and below NONE.
    /// @param text       Holds the names and the values, each followed by a 0 byte. It stays with
    ///                   the tree for the tree's whole life.
    /// @param textBytes  The size of @p text.
    YamlTree(Entry* entries, uint16_t count, char* text, size_t textBytes);

    YamlTree(const YamlTree&) = delete;
    YamlTree& operator=(const YamlTree&) = delete;

    /// Reads the text of a YAML file into the tree, in place of what it held.
    ///
    /// The file's top has to be a section. An entry the tree cannot hold is left out with
    /// everything under it. @p report takes a problem for each: a name with a dot, a name that
    /// stands twice in a section, a reference to an anchor, a level past LEVELS, every entry past
    /// the tree's room and every text past the room for text. A broken file leaves the tree empty
    /// with a broken problem. A file counts as broken where it is no valid YAML or its top is no
    /// section.
    ///
    /// @param yaml    The file's text. The tree keeps nothing of it.
    /// @param bytes   The length of @p yaml.
    /// @param report  Takes every problem found.
    /// @return        False where the file is broken, or where libyaml finds no room on the heap.
    /// @note          libyaml takes its input buffers from the heap and gives them back before
    ///                parse() returns. Nothing else of the tree touches the heap.
    bool parse(const char* yaml, size_t bytes, Report& report);

    /// Empties the tree, so it holds the empty top alone.
    void clear();

    /// Gets the file's top, a section.
    Id root() const { return 0; }

    /// Gets what an entry holds.
    Kind kind(Id id) const { return entries[id].kind; }

    /// Gets an entry's name inside its section. An entry of a list and the top have the empty
    /// name.
    const char* name(Id id) const;

    /// Gets a value's text as the file holds it, without its quotes. A section, a list and a value
    /// of nothing give the empty text.
    const char* text(Id id) const;

    /// Gets the line an entry stands on, counted from 1. An entry the file did not hold gives 0.
    uint16_t line(Id id) const { return entries[id].line; }

    /// Gets the column an entry starts in, counted from 1. An entry of a section starts at its
    /// name. An entry the file did not hold gives 0.
    uint16_t column(Id id) const { return entries[id].column; }

    /// Gets the section or the list an entry stands in, NONE for the top.
    Id parent(Id id) const { return entries[id].parent; }

    /// Gets the first entry of a section or a list, NONE where it is empty or holds a value.
    Id first(Id id) const { return entries[id].first; }

    /// Gets the entry after an entry in its section or list, NONE after the last.
    Id next(Id id) const { return entries[id].next; }

    /// Gets how many entries a section or a list holds, 0 for a value.
    uint16_t count(Id id) const;

    /// Gets the entry of a section by its name, or of a list by its number.
    ///
    /// @param id      The section or the list.
    /// @param name    The name, or the number in decimal without a leading zero.
    /// @param length  How many characters of @p name count.
    /// @return        NONE where there is no such entry.
    Id child(Id id, const char* name, size_t length) const;

    /// Gets the entry under a key from the file's top.
    ///
    /// @param key  The names from the top down, joined by dots, such as "ir.channels.0.order". A
    ///             list's entries are named by their numbers.
    /// @return     NONE where the key leads nowhere or holds an empty name.
    Id find(const char* key) const;

    /// Adds an entry at the end of a section or a list.
    ///
    /// @param id      The section or the list.
    /// @param name    The new entry's name inside a section, ignored inside a list.
    /// @param length  How many characters of @p name count.
    /// @param kind    What the new entry holds. A new value holds nothing.
    /// @return        NONE where @p id holds a value, where the section holds @p name already,
    ///                where @p name holds a dot or a 0 byte, and where the tree has no room for
    ///                the entry or its name.
    Id add(Id id, const char* name, size_t length, Kind kind);

    /// Sets a value's text.
    ///
    /// @param id    The value.
    /// @param text  The text. The tree keeps a copy.
    /// @return      False where @p id is no value, or where the tree has no room for the text.
    bool setText(Id id, const char* text);

    /// Removes an entry with everything under it.
    ///
    /// An entry removed from a list moves the entries after it down by one number. The top stays.
    void remove(Id id);

    /// Gets whether the tree has room for more entries and more text.
    ///
    /// @param entries  The entries to add.
    /// @param bytes    The characters of their names and values together, each with a 0 byte.
    bool fits(uint16_t entries, size_t bytes) const;

    /// Writes the tree as the text of a YAML file.
    ///
    /// A list of values stands on one line in brackets. A section of values inside a list stands
    /// on one line in braces. Every other section and list spreads over lines, indented by two
    /// spaces for each level. A text YAML would read differently stands in double quotes. A value
    /// of nothing is written as ~.
    ///
    /// @param out    Takes the text.
    /// @param room   The size of @p out.
    /// @param bytes  Set to the length of the text.
    /// @return       False where the text takes more than @p room.
    bool write(char* out, size_t room, size_t& bytes) const;

    /// Gets whether a value's text reads as unset: nothing, ~ or null.
    static bool isUnset(const char* text);

    /// Reads a decimal number with an optional sign, fraction and exponent, and nothing after it.
    ///
    /// @param text  The text, such as "-0.25" or "1.5e3".
    /// @param v     Set to the number.
    /// @return      False where the text holds anything else or the number lies outside a double.
    static bool readNumber(const char* text, double& v);

    /// Reads the number a list's entry is named by.
    ///
    /// @param name    The name, @p length characters long.
    /// @param length  How many characters of @p name count.
    /// @param number  Set to the number.
    /// @return        False where the name is no number of digits without a leading zero.
    static bool readIndex(const char* name, size_t length, uint32_t& number);

private:
    static constexpr uint32_t NO_TEXT = 0xFFFFFFFF;   // a string the tree does not hold
    static constexpr Id FREE = 0xFFFE;                // the parent of an unused entry

    /// Takes a free entry and links it after @p after under @p parent, or first where @p after is
    /// NONE.
    ///
    /// @return  NONE once the tree is full.
    Id take(Id parent, Id after, Kind kind);

    /// Gets the last entry under @p id, NONE where it holds none.
    Id last(Id id) const;

    /// Gets a string of the text by where it starts, the empty string for NO_TEXT.
    const char* string(uint32_t at) const;

    /// Removes an entry other than the top with everything under it.
    void cut(Id id);

    /// Copies a string into the text, compacting the text first where it lacks room.
    ///
    /// @param s       The string.
    /// @param length  How many characters of @p s count.
    /// @return        Where the copy starts, NO_TEXT for the empty string, or NOT_STORED where the
    ///                text has no room.
    uint32_t store(const char* s, size_t length);

    /// Takes a string out of what the text holds, so a compaction reclaims it.
    void drop(uint32_t at);

    /// Moves the strings of the entries in use to the start of the text, closing the gaps the
    /// removed ones left.
    void compact();

    /// Gets whether every entry directly under @p id holds a value.
    bool valuesOnly(Id id) const;

    static constexpr uint32_t NOT_STORED = 0xFFFFFFFE;   // a string the text had no room for

    Entry* const entries;
    const uint16_t capacity;
    char* const pool;
    const size_t poolBytes;
    size_t used = 0;          // the bytes of the text up to its last string
    size_t live = 0;          // the bytes of the strings in use, each with its 0 byte
    Id freeList = NONE;       // the first unused entry, the others linked by next
    uint16_t freeCount = 0;   // how many entries are unused
    bool compacting = true;   // false while parse() keeps where its names stand
};
