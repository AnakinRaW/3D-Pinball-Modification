// The machine's settings, read from the YAML file /config.yml on the SD card, so a setting changes
// on a computer without a new firmware. A part reads its setting by its key with its default and
// its range. A missing or broken file leaves every setting at its default. A value out of its
// range is held at the nearer end. Changed settings are written back into the file.
//
// The keys stand in components/config-keys.h. The file's tree comes from components/yaml-tree.h.
//
// Design: docs/firmware/components/config.md.
#pragma once
#include "components/file-system.h"
#include "components/yaml-tree.h"
#include <mutex>
#include <optional>
#include <stddef.h>
#include <stdint.h>

class Config {
public:
    static constexpr const char* PATH = "/config.yml";   // config.md: the file
    static constexpr size_t FILE_BYTES = 8192;           // config.md: the largest file
    static constexpr uint16_t ENTRIES = 512;             // config.md: the entries the tree holds
    static constexpr double VALUE_LIMIT = 1e12;          // config.md: set() takes less in magnitude

    /// Creates an empty tree, in which every setting takes its default.
    ///
    /// @param files  Gives the file on the card.
    /// @param path   The file's full path on the card.
    explicit Config(FileSystem& files, const char* path = PATH)
        : files(files), path(path), tree(entries, ENTRIES + 1, text, sizeof text) {}

    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    /// Reads the file into the tree.
    ///
    /// A file that is missing, empty, unreadable, larger than FILE_BYTES or broken leaves the tree
    /// empty, so every setting takes its default. An entry the tree cannot hold is left out with
    /// everything under it, as YamlTree::parse() gives. Each of these writes a line to the log,
    /// with the line and the column of the file where it found the problem.
    ///
    /// @note  It waits for the card. libyaml takes its input buffers from the heap and gives them
    ///        back before it returns. The main task calls it once at start-up, right after the
    ///        logger's begin().
    void begin();

    /// Gets the number under a key, held inside a range, or a default.
    ///
    /// The default comes where the key is missing, where it holds ~, null or nothing, and where
    /// it holds no number. A missing key, a value that is no number and a value outside the range
    /// each write a line to the log.
    ///
    /// @param key       The names from the file's top down, joined by dots, such as
    ///                  "servo.start_deg". A list's entries are named 0, 1 and on, as in
    ///                  "ir.channels.0.order".
    /// @param fallback  The setting's default.
    /// @param min       The least value it gets. A smaller value counts as @p min.
    /// @param max       The largest value it gets. A larger value counts as @p max.
    /// @note            Any task, never an interrupt. It waits while another task reads or writes
    ///                  the tree.
    double get(const char* key, double fallback, double min, double max) const;

    /// Gets the number under a key that has no default, held inside a range.
    ///
    /// A value that is no number and a value outside the range each write a line to the log.
    ///
    /// @param key  The key, as for the get() with a default.
    /// @param min  The least value it gets. A smaller value counts as @p min.
    /// @param max  The largest value it gets. A larger value counts as @p max.
    /// @return     No value where the key is missing, holds ~, null or nothing, or holds no number.
    /// @note       Any task, never an interrupt. It waits while another task reads or writes the
    ///             tree.
    std::optional<double> get(const char* key, double min, double max) const;

    /// Gets whether the switch under a key is on, or a default.
    ///
    /// The file writes a switch as true or false, yes or no, on or off, or 1 or 0, in any case.
    /// The default comes where the key is missing, where it holds ~, null or nothing, and where it
    /// holds anything else. A missing key and a value that is no switch each write a line to the
    /// log.
    ///
    /// @param key       The key, as for the get() of a number.
    /// @param fallback  The switch's default.
    /// @note            Any task, never an interrupt. It waits while another task reads or writes
    ///                  the tree.
    bool get(const char* key, bool fallback) const;

    /// Gets how many entries the list under a key holds.
    ///
    /// @param key  The key, as for get().
    /// @return     0 where the key is missing or holds no list.
    /// @note       Any task, never an interrupt. It waits as get() does.
    uint16_t count(const char* key) const;

    /// Sets the number under a key, creating the sections and the lists above it.
    ///
    /// A missing name becomes a list where the name after it is a number. Otherwise it becomes a
    /// section.
    /// A list grows by one entry at its end, so a new entry's number equals the list's count. The
    /// value keeps six decimal places. The next save() writes it into the file.
    ///
    /// @param key    The key, as for get().
    /// @param value  Lies below VALUE_LIMIT in magnitude.
    /// @return       False where @p value lies outside, where the key names a section or a list,
    ///               where a name of the key holds a value, where a list's entry lies past its
    ///               end, where the key passes more names than a tree has levels, and where the
    ///               tree has no room for what is missing. The tree then stays as it was.
    /// @note         Any task, never an interrupt. It waits as get() does.
    bool set(const char* key, double value);

    /// Removes the entry under a key with everything under it.
    ///
    /// An entry removed from a list moves the entries after it down by one number. A missing key
    /// changes nothing. The next save() writes the file without the entry.
    ///
    /// @param key  The key, as for get().
    /// @note       Any task, never an interrupt. It waits as get() does.
    void remove(const char* key);

    /// Starts writing the whole tree as the file and returns.
    ///
    /// The file then holds every entry of the tree, those no part reads included. Comments in the
    /// file are lost.
    ///
    /// @return  False while the previous save is still being written, and where the tree's text
    ///          takes more than FILE_BYTES. The file then stays as it was.
    /// @note    Any task, never an interrupt. It waits as get() does, never for the card.
    bool save();

    /// Gets how the last save stands: IDLE before the first, PENDING while it is being written,
    /// then SUCCEEDED or FAILED.
    FileStatus saveStatus() const { return saving.status(); }

private:
    static constexpr size_t TEXT_BYTES = 8192;       // config.md: the names and the values together
    static constexpr size_t CACHE_LINE_BYTES = 32;   // the processor's cache line

    /// Gets the entry under a key, adding what set() adds.
    ///
    /// @param textBytes  The characters of the value the entry is to take, with its 0 byte.
    /// @return           NONE where set() refuses the key.
    YamlTree::Id make(const char* key, size_t textBytes);

    /// Gets the value under a key's entry as a number held inside a range, as get() does.
    ///
    /// @param key       The key, for the log.
    /// @param at        The key's entry, NONE where there is none.
    /// @param fallback  Where it points to a value, that value is written into the log where the
    ///                  entry holds no number.
    /// @return          No value where the entry is missing, unset or holds no number.
    std::optional<double> number(const char* key, YamlTree::Id at, const double* fallback,
                                 double min, double max) const;

    FileSystem& files;
    const char* const path;
    mutable std::mutex mutex;               // guards the tree, buffer and saving
    YamlTree::Entry entries[ENTRIES + 1];   // the tree's entries, its top included
    char text[TEXT_BYTES];                  // the tree's names and values
    YamlTree tree;
    /// The file's text, which begin() reads and save() writes.
    alignas(CACHE_LINE_BYTES) char buffer[FILE_BYTES];
    FileRequest saving;   // the write save() started last
};
