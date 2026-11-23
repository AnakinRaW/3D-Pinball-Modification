#include "config.h"
#include "logging/debug-log.h"
#include "logging/logger.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

constexpr char WHO[] = "config";   // the writer's name in the log
constexpr uint8_t BASE = 10;
constexpr uint8_t DECIMALS = 6;               // config.md: set() keeps six decimal places
constexpr uint64_t DECIMAL_SCALE = 1000000;   // 10 to the power of DECIMALS
constexpr size_t NUMBER_BYTES = 24;           // the longest number set() writes, with its end

/// Writes a number with up to DECIMALS decimal places and without trailing zeros.
///
/// printf() takes memory from the heap for the first floats it formats, which the firmware avoids
/// after the start.
///
/// @param v    Lies below VALUE_LIMIT in magnitude.
/// @param out  Takes the text, such as "-0.25".
/// @return     False where @p v lies outside.
static bool formatNumber(double v, char (&out)[NUMBER_BYTES]) {
    if (!(fabs(v) < Config::VALUE_LIMIT)) {
        return false;
    }
    const uint64_t scaled = uint64_t(llround(fabs(v) * double(DECIMAL_SCALE)));
    char reversed[NUMBER_BYTES];
    size_t n = 0;
    uint64_t whole = scaled / DECIMAL_SCALE;
    do {
        reversed[n++] = char('0' + whole % BASE);
        whole /= BASE;
    } while (whole != 0);
    size_t at = 0;
    if (v < 0 && scaled != 0) {
        out[at++] = '-';
    }
    while (n > 0) {
        out[at++] = reversed[--n];
    }
    uint64_t fraction = scaled % DECIMAL_SCALE;
    if (fraction != 0) {
        char digits[DECIMALS];
        for (uint8_t i = DECIMALS; i > 0; --i) {
            digits[i - 1] = char('0' + fraction % BASE);
            fraction /= BASE;
        }
        uint8_t last = DECIMALS;
        while (digits[last - 1] == '0') {
            --last;
        }
        out[at++] = '.';
        for (uint8_t i = 0; i < last; ++i) {
            out[at++] = digits[i];
        }
    }
    out[at] = '\0';
    return true;
}

/// Gets a number as text for a log line, which formats no float.
static const char* shown(double v, char (&out)[NUMBER_BYTES]) {
    if (!formatNumber(v, out)) {
        snprintf(out, sizeof out, "%s", v < 0 ? "-huge" : "huge");
    }
    return out;
}

/// Writes what the tree finds in the file into the log, with the file's path and the place.
class LogReport final : public YamlTree::Report {
public:
    explicit LogReport(const char* path) : path(path) {}

    void found(const YamlTree::Problem& p) override {
        logWarning(WHO, "%s:%lu:%lu: %s%s", path, static_cast<unsigned long>(p.line),
                   static_cast<unsigned long>(p.column), p.text,
                   p.broken ? ", every setting at its default" : "");
    }

private:
    const char* const path;
};

/// Holds the names of a key, split at its dots.
struct KeyNames {
    const char* start[YamlTree::LEVELS] = {};
    size_t length[YamlTree::LEVELS] = {};
    uint8_t count = 0;

    /// Splits a key.
    ///
    /// @return  False for an empty key, an empty name, and a key of more names than a tree has
    ///          levels.
    bool split(const char* key) {
        count = 0;
        const char* at = key;
        for (;;) {
            if (count == YamlTree::LEVELS) {
                return false;
            }
            const char* dot = strchr(at, '.');
            const size_t n = dot != nullptr ? size_t(dot - at) : strlen(at);
            if (n == 0) {
                return false;
            }
            start[count] = at;
            length[count] = n;
            ++count;
            if (dot == nullptr) {
                return true;
            }
            at = dot + 1;
        }
    }
};

void Config::begin() {
    const std::lock_guard<std::mutex> hold(mutex);
    tree.clear();
    FileRequest r;
    files.openFull(path).read(0, buffer, sizeof buffer, r);
    if (!r.wait()) {
        logWarning(WHO, "%s unreadable, every setting at its default", path);
        return;
    }
    if (r.fileSize() > sizeof buffer) {
        logWarning(WHO, "%s larger than %u bytes, every setting at its default", path,
                   unsigned(FILE_BYTES));
        return;
    }
    if (r.bytes() == 0) {
        logInfo(WHO, "%s missing or empty, every setting at its default", path);
        return;
    }
    LogReport report(path);
    tree.parse(buffer, r.bytes(), report);
}

double Config::get(const char* key, double fallback, double min, double max) const {
    const std::lock_guard<std::mutex> hold(mutex);
    const YamlTree::Id at = tree.find(key);
    if (at == YamlTree::NONE) {
        char text[NUMBER_BYTES];
        logInfo(WHO, "%s missing, %s taken", key, shown(fallback, text));
        return fallback;
    }
    return number(key, at, &fallback, min, max).value_or(fallback);
}

std::optional<double> Config::get(const char* key, double min, double max) const {
    const std::lock_guard<std::mutex> hold(mutex);
    return number(key, tree.find(key), nullptr, min, max);
}

uint16_t Config::count(const char* key) const {
    const std::lock_guard<std::mutex> hold(mutex);
    const YamlTree::Id at = tree.find(key);
    if (at == YamlTree::NONE || tree.kind(at) != YamlTree::Kind::LIST) {
        return 0;
    }
    return tree.count(at);
}

bool Config::set(const char* key, double value) {
    char text[NUMBER_BYTES];
    if (!formatNumber(value, text)) {
        DEBUG_LOG("Config::set: %s refused, the value lies past the limit\n", key);
        return false;
    }
    const std::lock_guard<std::mutex> hold(mutex);
    const YamlTree::Id at = make(key, strlen(text) + 1);
    // make() has made room for the text, so setText() takes it.
    return at != YamlTree::NONE && tree.setText(at, text);
}

void Config::remove(const char* key) {
    const std::lock_guard<std::mutex> hold(mutex);
    const YamlTree::Id at = tree.find(key);
    if (at != YamlTree::NONE) {
        tree.remove(at);
    }
}

bool Config::save() {
    const std::lock_guard<std::mutex> hold(mutex);
    if (saving.status() == FileStatus::PENDING) {
        DEBUG_LOG("Config::save: refused, the previous save is still being written\n");
        return false;
    }
    size_t bytes = 0;
    if (!tree.write(buffer, sizeof buffer, bytes)) {
        logWarning(WHO, "the settings take more than %u bytes, %s left as it was",
                   unsigned(FILE_BYTES), path);
        return false;
    }
    files.openFull(path).rewrite(buffer, bytes, saving);
    return true;
}

std::optional<double> Config::number(const char* key, YamlTree::Id at, const double* fallback,
                                     double min, double max) const {
    if (at == YamlTree::NONE) {
        return std::nullopt;
    }
    const bool value = tree.kind(at) == YamlTree::Kind::VALUE;
    if (value && YamlTree::isUnset(tree.text(at))) {
        return std::nullopt;
    }
    double v = 0;
    if (!value || !YamlTree::readNumber(tree.text(at), v)) {
        char text[NUMBER_BYTES];
        if (fallback != nullptr) {
            logWarning(WHO, "%s holds no number, %s taken", key, shown(*fallback, text));
        } else {
            logWarning(WHO, "%s holds no number, left unset", key);
        }
        return std::nullopt;
    }
    const double held = v < min ? min : (v > max ? max : v);
    if (held < v || held > v) {
        char low[NUMBER_BYTES];
        char high[NUMBER_BYTES];
        char taken[NUMBER_BYTES];
        logWarning(WHO, "%s holds %s, outside %s to %s, %s taken", key, tree.text(at),
                   shown(min, low), shown(max, high), shown(held, taken));
    }
    return held;
}

YamlTree::Id Config::make(const char* key, size_t textBytes) {
    KeyNames names;
    if (!names.split(key)) {
        DEBUG_LOG("Config::set: %s refused, an empty name or more than %u names\n", key,
                  unsigned(YamlTree::LEVELS));
        return YamlTree::NONE;
    }
    // The first pass finds the names that stand already and checks the ones to add, so a refusal
    // leaves the tree as it was.
    YamlTree::Id at = tree.root();
    uint8_t standing = 0;
    for (; standing < names.count; ++standing) {
        const YamlTree::Id c = tree.child(at, names.start[standing], names.length[standing]);
        if (c == YamlTree::NONE) {
            break;
        }
        at = c;
    }
    if (standing == names.count) {
        if (tree.kind(at) != YamlTree::Kind::VALUE) {
            DEBUG_LOG("Config::set: %s refused, it names a section or a list\n", key);
            return YamlTree::NONE;
        }
        return tree.fits(0, textBytes) ? at : YamlTree::NONE;
    }
    if (tree.kind(at) == YamlTree::Kind::VALUE) {
        DEBUG_LOG("Config::set: %s refused, a name of it holds a value\n", key);
        return YamlTree::NONE;
    }
    size_t bytes = textBytes;
    for (uint8_t i = standing; i < names.count; ++i) {
        uint32_t number = 0;
        const bool inList = i == standing
                                ? tree.kind(at) == YamlTree::Kind::LIST
                                : YamlTree::readIndex(names.start[i], names.length[i], number);
        // A list grows only at its end, so a new entry's number is the list's count. A new list
        // starts at 0.
        const uint16_t end = i == standing ? tree.count(at) : 0;
        if (inList &&
            (!YamlTree::readIndex(names.start[i], names.length[i], number) || number != end)) {
            DEBUG_LOG("Config::set: %s refused, a list's entry past its end\n", key);
            return YamlTree::NONE;
        }
        bytes += inList ? 0 : names.length[i] + 1;
    }
    if (!tree.fits(uint16_t(names.count - standing), bytes)) {
        DEBUG_LOG("Config::set: %s refused, the tree has no room\n", key);
        return YamlTree::NONE;
    }
    for (uint8_t i = standing; i < names.count; ++i) {
        uint32_t number = 0;
        const YamlTree::Kind kind =
            i + 1 == names.count ? YamlTree::Kind::VALUE
            : YamlTree::readIndex(names.start[i + 1], names.length[i + 1], number)
                ? YamlTree::Kind::LIST
                : YamlTree::Kind::SECTION;
        at = tree.add(at, names.start[i], names.length[i], kind);
    }
    return at;
}
