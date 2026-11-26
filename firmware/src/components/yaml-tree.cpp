#include "yaml-tree.h"
#include "logging/debug-log.h"
#include <initializer_list>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <yaml.h>

constexpr uint8_t BASE = 10;
/// A mantissa past it takes no further digit, since a double holds 15 to 17 digits.
constexpr uint64_t MANTISSA_LIMIT = 100000000000000000ULL;
/// An exponent past it lies outside a double's range either way.
constexpr int32_t EXPONENT_LIMIT = 400;
constexpr uint32_t INDEX_LIMIT = 65535;      // the largest number of a list's entry
constexpr uint16_t POSITION_LIMIT = 65535;   // the largest line and column an entry keeps
constexpr uint8_t INDENT = 2;          // the spaces by which each level of the file is indented
constexpr size_t MESSAGE_BYTES = 96;   // the longest text of a problem, with its end
constexpr uint8_t HEX_MASK = 0x0F;
constexpr uint8_t NIBBLE_BITS = 4;
constexpr char HEX_DIGITS[] = "0123456789ABCDEF";
constexpr char CONTROL_LIMIT = ' ';   // characters below it are control characters
/// Characters that end a plain text or change what it means, inside brackets or outside.
constexpr char SPECIAL[] = ":#,[]{}\"'\\";
/// Characters a plain text cannot start with.
constexpr char LEADING[] = "&*!|>%@`";

/// Gets whether a character is a decimal digit.
static bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

bool YamlTree::readIndex(const char* name, size_t length, uint32_t& number) {
    if (length == 0 || (length > 1 && name[0] == '0')) {
        return false;
    }
    number = 0;
    for (size_t i = 0; i < length; ++i) {
        if (!isDigit(name[i]) || number > INDEX_LIMIT) {
            return false;
        }
        number = number * BASE + uint32_t(name[i] - '0');
    }
    return number <= INDEX_LIMIT;
}

/// Gets a line or a column as the tree keeps it, counted from 1. libyaml counts from 0.
static uint16_t position(size_t fromZero) {
    return fromZero + 1 > POSITION_LIMIT ? POSITION_LIMIT : uint16_t(fromZero + 1);
}

/// Gets whether a text has to stand in quotes in the file, so YAML reads it back as the same text.
static bool needsQuotes(const char* text) {
    const size_t n = strlen(text);
    if (n == 0) {
        return false;
    }
    if (strchr(LEADING, text[0]) != nullptr || text[0] == ' ' || text[n - 1] == ' ') {
        return true;
    }
    // A dash or a question mark followed by a space or by nothing marks a list's entry or a name.
    if ((text[0] == '-' || text[0] == '?') && (text[1] == ' ' || text[1] == '\0')) {
        return true;
    }
    for (size_t i = 0; i < n; ++i) {
        if (strchr(SPECIAL, text[i]) != nullptr || uint8_t(text[i]) < uint8_t(CONTROL_LIMIT)) {
            return true;
        }
    }
    return false;
}

/// Collects the text of a file in a buffer and notes when it runs past the buffer's end.
class TextWriter {
public:
    TextWriter(char* to, size_t room) : to(to), room(room) {}

    void put(char c) {
        if (used < room) {
            to[used] = c;
        }
        ++used;
    }

    void put(const char* text) {
        for (; *text != '\0'; ++text) {
            put(*text);
        }
    }

    void indent(uint8_t spaces) {
        for (uint8_t i = 0; i < spaces; ++i) {
            put(' ');
        }
    }

    /// Writes a value or a name: ~ for an empty one, in quotes where YAML would read it otherwise.
    void scalar(const char* text) {
        if (text[0] == '\0') {
            put('~');
        } else if (!needsQuotes(text)) {
            put(text);
        } else {
            put('"');
            for (; *text != '\0'; ++text) {
                escaped(*text);
            }
            put('"');
        }
    }

    bool fits() const { return used <= room; }
    size_t length() const { return used; }

private:
    /// Writes a character inside double quotes, with a backslash where YAML needs one.
    void escaped(char c) {
        if (c == '"' || c == '\\') {
            put('\\');
            put(c);
        } else if (c == '\n') {
            put("\\n");
        } else if (c == '\t') {
            put("\\t");
        } else if (uint8_t(c) < uint8_t(CONTROL_LIMIT)) {
            put("\\x");
            put(HEX_DIGITS[(uint8_t(c) >> NIBBLE_BITS) & HEX_MASK]);
            put(HEX_DIGITS[uint8_t(c) & HEX_MASK]);
        } else {
            put(c);
        }
    }

    char* const to;
    const size_t room;
    size_t used = 0;
};

YamlTree::YamlTree(Entry* entries, uint16_t count, char* text, size_t textBytes)
    : entries(entries), capacity(count), pool(text), poolBytes(textBytes) {
    clear();
}

bool YamlTree::parse(const char* yaml, size_t bytes, Report& report) {
    // Each frame stands for a section or a list the parser is inside, the file's top first.
    struct Frame {
        Id at;           // the entry its entries go under, NONE where they are left out
        Id last;         // its last entry so far, NONE for none
        Kind kind;       // SECTION or LIST
        bool named;      // a section's name came, so its value comes next
        bool nameFits;   // the name went into the text and holds no dot
        uint32_t name;   // where the name stands in the text
        uint16_t line;   // where the name stands in the file
        uint16_t column;
        uint32_t entries;   // a list's entries so far, which number the next one
    };
    Frame frames[LEVELS];
    uint8_t depth = 0;
    uint32_t skipped = 0;   // the sections and lists open inside a part that is left out
    bool topSeen = false;
    bool ok = true;
    bool fullNoted = false;
    bool textNoted = false;
    char message[MESSAGE_BYTES];

    clear();
    // A compaction would move the names the frames point to.
    compacting = false;
    yaml_parser_t parser;
    if (yaml_parser_initialize(&parser) == 0) {
        report.found({0, 0, "no room on the heap to parse the file", true});
        compacting = true;
        return false;
    }
    yaml_parser_set_input_string(&parser, reinterpret_cast<const unsigned char*>(yaml), bytes);
    for (bool done = false; !done;) {
        yaml_event_t event;
        if (yaml_parser_parse(&parser, &event) == 0) {
            snprintf(message, sizeof message, "%s",
                     parser.problem != nullptr ? parser.problem : "unreadable");
            report.found({position(parser.problem_mark.line), position(parser.problem_mark.column),
                          message, true});
            ok = false;
            break;
        }
        const yaml_event_type_t type = event.type;
        const uint16_t line = position(event.start_mark.line);
        const uint16_t column = position(event.start_mark.column);
        const bool opens = type == YAML_MAPPING_START_EVENT || type == YAML_SEQUENCE_START_EVENT;
        const bool node = opens || type == YAML_SCALAR_EVENT || type == YAML_ALIAS_EVENT;
        const bool closes = type == YAML_MAPPING_END_EVENT || type == YAML_SEQUENCE_END_EVENT;
        const char* scalar = reinterpret_cast<const char*>(event.data.scalar.value);
        const size_t length = type == YAML_SCALAR_EVENT ? event.data.scalar.length : 0;
        if (type == YAML_STREAM_END_EVENT) {
            done = true;
        } else if (closes) {
            if (skipped > 0) {
                --skipped;
            } else if (depth > 0) {
                --depth;
            }
        } else if (!node) {
            // The stream's and the documents' starts and ends carry nothing.
        } else if (skipped > 0) {
            skipped += opens ? 1 : 0;
        } else if (depth == 0) {
            if (topSeen) {
                report.found({line, column, "a second document, left out", false});
                skipped += opens ? 1 : 0;
            } else if (type == YAML_MAPPING_START_EVENT) {
                topSeen = true;
                frames[depth++] = {root(), NONE, Kind::SECTION, false, true, NO_TEXT, 0, 0, 0};
            } else if (type == YAML_SCALAR_EVENT && length == 0) {
                // A document with nothing in it, such as a lone ---.
                topSeen = true;
            } else {
                report.found({line, column, "the top holds no section", true});
                ok = false;
                done = true;
            }
        } else if (frames[depth - 1].kind == Kind::SECTION && !frames[depth - 1].named) {
            Frame& f = frames[depth - 1];
            f.named = true;
            f.line = line;
            f.column = column;
            f.name = NO_TEXT;
            f.nameFits = false;
            if (type != YAML_SCALAR_EVENT) {
                report.found({line, column, "a name that is no text", true});
                ok = false;
                done = true;
            } else if (f.at == NONE) {
                // The section was left out. Its problem is reported already.
            } else if (strlen(scalar) != length || memchr(scalar, '.', length) != nullptr) {
                snprintf(message, sizeof message, "the name %s holds a dot or a 0 byte, left out",
                         scalar);
                report.found({line, column, message, false});
            } else {
                f.name = store(scalar, length);
                f.nameFits = f.name != NOT_STORED;
                if (!f.nameFits && !textNoted) {
                    snprintf(message, sizeof message,
                             "more than %lu bytes of names and values, the rest left out",
                             static_cast<unsigned long>(poolBytes));
                    report.found({line, column, message, false});
                    textNoted = true;
                }
            }
        } else {
            Frame& f = frames[depth - 1];
            const bool inSection = f.kind == Kind::SECTION;
            const uint16_t at = inSection ? f.line : line;
            const uint16_t in = inSection ? f.column : column;
            char shown[MESSAGE_BYTES / 2];
            if (inSection) {
                snprintf(shown, sizeof shown, "%s", string(f.name));
            } else {
                snprintf(shown, sizeof shown, "entry %lu", static_cast<unsigned long>(f.entries));
            }
            const Kind kind = type == YAML_MAPPING_START_EVENT    ? Kind::SECTION
                              : type == YAML_SEQUENCE_START_EVENT ? Kind::LIST
                                                                  : Kind::VALUE;
            Id made = NONE;
            if (f.at == NONE || (inSection && !f.nameFits)) {
                // The section or the list above was left out. Its problem is reported already.
            } else if (type == YAML_ALIAS_EVENT) {
                snprintf(message, sizeof message, "%s refers to an anchor, left out", shown);
                report.found({at, in, message, false});
            } else if (inSection && child(f.at, string(f.name), strlen(string(f.name))) != NONE) {
                snprintf(message, sizeof message, "%s stands twice, the second left out", shown);
                report.found({at, in, message, false});
            } else if (opens && depth == LEVELS) {
                snprintf(message, sizeof message, "%s lies deeper than %u levels, left out", shown,
                         unsigned(LEVELS));
                report.found({at, in, message, false});
            } else if (type == YAML_SCALAR_EVENT && strlen(scalar) != length) {
                snprintf(message, sizeof message, "%s holds a 0 byte, left out", shown);
                report.found({at, in, message, false});
            } else if (length > 0 && used + length + 1 > poolBytes) {
                if (!textNoted) {
                    snprintf(message, sizeof message,
                             "more than %lu bytes of names and values, the rest left out",
                             static_cast<unsigned long>(poolBytes));
                    report.found({at, in, message, false});
                    textNoted = true;
                }
            } else {
                made = take(f.at, f.last, kind);
                if (made == NONE && !fullNoted) {
                    snprintf(message, sizeof message, "more than %u entries, the rest left out",
                             unsigned(capacity - 1));
                    report.found({at, in, message, false});
                    fullNoted = true;
                }
            }
            if (made != NONE) {
                Entry& e = entries[made];
                e.name = inSection ? f.name : NO_TEXT;
                e.line = at;
                e.column = in;
                if (type == YAML_SCALAR_EVENT) {
                    e.text = store(scalar, length);
                }
                f.last = made;
            } else if (inSection && f.nameFits && f.name != NO_TEXT &&
                       f.name + strlen(pool + f.name) + 1 == used) {
                // The name was the last string stored, so its room goes back.
                drop(f.name);
                used = f.name;
            }
            f.entries += inSection ? 0 : 1;
            f.named = false;
            if (opens && depth == LEVELS) {
                ++skipped;
            } else if (opens) {
                frames[depth++] = {made, NONE, kind, false, true, NO_TEXT, 0, 0, 0};
            }
        }
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);
    compacting = true;
    if (!ok) {
        clear();
    }
    return ok;
}

void YamlTree::clear() {
    entries[0] = Entry();
    entries[0].name = NO_TEXT;
    entries[0].text = NO_TEXT;
    entries[0].parent = NONE;
    entries[0].first = NONE;
    entries[0].next = NONE;
    entries[0].kind = Kind::SECTION;
    for (uint16_t i = 1; i < capacity; ++i) {
        entries[i].parent = FREE;
        entries[i].next = i + 1 < capacity ? Id(i + 1) : NONE;
    }
    freeList = capacity > 1 ? 1 : NONE;
    freeCount = uint16_t(capacity - 1);
    used = 0;
    live = 0;
}

const char* YamlTree::name(Id id) const {
    return string(entries[id].name);
}

const char* YamlTree::text(Id id) const {
    return string(entries[id].text);
}

const char* YamlTree::string(uint32_t at) const {
    return at < NOT_STORED ? pool + at : "";
}

uint16_t YamlTree::count(Id id) const {
    uint16_t n = 0;
    for (Id c = entries[id].first; c != NONE; c = entries[c].next) {
        ++n;
    }
    return n;
}

YamlTree::Id YamlTree::child(Id id, const char* name, size_t length) const {
    if (entries[id].kind == Kind::LIST) {
        uint32_t number = 0;
        if (!readIndex(name, length, number)) {
            return NONE;
        }
        Id c = entries[id].first;
        for (uint32_t k = 0; k < number && c != NONE; ++k) {
            c = entries[c].next;
        }
        return c;
    }
    if (entries[id].kind != Kind::SECTION) {
        return NONE;
    }
    for (Id c = entries[id].first; c != NONE; c = entries[c].next) {
        const char* n = this->name(c);
        if (strncmp(n, name, length) == 0 && n[length] == '\0') {
            return c;
        }
    }
    return NONE;
}

YamlTree::Id YamlTree::find(const char* key) const {
    Id at = root();
    const char* start = key;
    for (;;) {
        const char* dot = strchr(start, '.');
        const size_t n = dot != nullptr ? size_t(dot - start) : strlen(start);
        if (n == 0) {
            return NONE;
        }
        at = child(at, start, n);
        if (at == NONE || dot == nullptr) {
            return at;
        }
        start = dot + 1;
    }
}

YamlTree::Id YamlTree::add(Id id, const char* name, size_t length, Kind kind) {
    const bool inSection = entries[id].kind == Kind::SECTION;
    if (entries[id].kind == Kind::VALUE) {
        DEBUG_LOG("YamlTree::add: %.*s refused, entry %u holds a value\n", int(length), name,
                  unsigned(id));
        return NONE;
    }
    if (inSection && (memchr(name, '.', length) != nullptr ||
                      memchr(name, '\0', length) != nullptr || child(id, name, length) != NONE)) {
        DEBUG_LOG("YamlTree::add: %.*s refused, a name with a dot or one the section holds\n",
                  int(length), name);
        return NONE;
    }
    const size_t bytes = inSection && length > 0 ? length + 1 : 0;
    if (!fits(1, bytes)) {
        DEBUG_LOG("YamlTree::add: %.*s refused, the tree has no room\n", int(length), name);
        return NONE;
    }
    const uint32_t at = inSection ? store(name, length) : NO_TEXT;
    const Id made = take(id, last(id), kind);
    entries[made].name = at;
    return made;
}

bool YamlTree::setText(Id id, const char* text) {
    if (entries[id].kind != Kind::VALUE) {
        DEBUG_LOG("YamlTree::setText: refused, entry %u holds no value\n", unsigned(id));
        return false;
    }
    const size_t n = strlen(text);
    const uint32_t old = entries[id].text;
    if (old != NO_TEXT && strlen(pool + old) >= n) {
        // A text no longer than the one it replaces takes its place.
        drop(old);
        if (n == 0) {
            entries[id].text = NO_TEXT;
        } else {
            memcpy(pool + old, text, n + 1);
            live += n + 1;
        }
        return true;
    }
    const uint32_t at = store(text, n);
    if (at == NOT_STORED) {
        DEBUG_LOG("YamlTree::setText: refused, the tree has no room for %u bytes\n",
                  unsigned(n + 1));
        return false;
    }
    if (entries[id].text != NO_TEXT) {
        drop(entries[id].text);
    }
    entries[id].text = at;
    return true;
}

void YamlTree::remove(Id id) {
    if (id != root()) {
        cut(id);
        return;
    }
    while (entries[id].first != NONE) {
        cut(entries[id].first);
    }
}

void YamlTree::cut(Id id) {
    const Id up = entries[id].parent;
    if (entries[up].first == id) {
        entries[up].first = entries[id].next;
    } else {
        Id before = entries[up].first;
        while (entries[before].next != id) {
            before = entries[before].next;
        }
        entries[before].next = entries[id].next;
    }
    // Frees the entries below from the first down. Every entry freed is then the first of its
    // parent, so the walk needs no stack.
    Id n = id;
    for (;;) {
        if (entries[n].first != NONE) {
            n = entries[n].first;
            continue;
        }
        const Id above = entries[n].parent;
        const Id after = entries[n].next;
        drop(entries[n].name);
        drop(entries[n].text);
        entries[n].parent = FREE;
        entries[n].next = freeList;
        freeList = n;
        ++freeCount;
        if (n == id) {
            return;
        }
        entries[above].first = after;
        n = after != NONE ? after : above;
    }
}

bool YamlTree::fits(uint16_t entries, size_t bytes) const {
    return freeCount >= entries && live + bytes <= poolBytes;
}

bool YamlTree::write(char* out, size_t room, size_t& bytes) const {
    // Each step stands for a section or a list being written, the file's top first.
    struct Step {
        Id at;            // the section or list being written
        Id next;          // its next entry to write
        uint8_t indent;   // the spaces in front of its entries
    };
    Step steps[LEVELS];
    uint8_t depth = 0;
    steps[depth++] = {root(), entries[root()].first, 0};
    TextWriter w(out, room);
    bool deep = false;
    while (depth > 0) {
        Step& s = steps[depth - 1];
        const Id c = s.next;
        if (c == NONE) {
            --depth;
            continue;
        }
        s.next = entries[c].next;
        const bool inList = entries[s.at].kind == Kind::LIST;
        const Entry& e = entries[c];
        w.indent(s.indent);
        if (inList) {
            w.put('-');
        } else {
            w.scalar(name(c));
            w.put(':');
        }
        bool block = false;
        if (e.kind == Kind::VALUE) {
            w.put(' ');
            w.scalar(text(c));
        } else if (e.first == NONE) {
            w.put(e.kind == Kind::LIST ? " []" : " {}");
        } else if (valuesOnly(c) && (e.kind == Kind::LIST || inList)) {
            // A list of values stands on one line. So does a section of values inside a list.
            w.put(e.kind == Kind::LIST ? " [" : " {");
            for (Id v = e.first; v != NONE; v = entries[v].next) {
                w.put(v == e.first ? "" : ", ");
                if (e.kind == Kind::SECTION) {
                    w.scalar(name(v));
                    w.put(": ");
                }
                w.scalar(text(v));
            }
            w.put(e.kind == Kind::LIST ? "]" : "}");
        } else {
            block = true;
        }
        w.put('\n');
        if (block && depth == LEVELS) {
            deep = true;
        } else if (block) {
            steps[depth] = {c, e.first, uint8_t(s.indent + INDENT)};
            ++depth;
        }
    }
    bytes = w.length();
    return w.fits() && !deep;
}

bool YamlTree::isUnset(const char* text) {
    return text[0] == '\0' || strcmp(text, "~") == 0 || strcmp(text, "null") == 0 ||
           strcmp(text, "Null") == 0 || strcmp(text, "NULL") == 0;
}

bool YamlTree::readNumber(const char* text, double& v) {
    // newlib's strtod() takes memory from the heap for its digits, which the firmware avoids
    // after the start.
    const char* p = text;
    const bool negative = *p == '-';
    if (*p == '-' || *p == '+') {
        ++p;
    }
    uint64_t mantissa = 0;
    int32_t exponent = 0;
    bool digits = false;
    for (; isDigit(*p); ++p) {
        digits = true;
        if (mantissa < MANTISSA_LIMIT) {
            mantissa = mantissa * BASE + uint64_t(*p - '0');
        } else {
            ++exponent;
        }
    }
    if (*p == '.') {
        for (++p; isDigit(*p); ++p) {
            digits = true;
            if (mantissa < MANTISSA_LIMIT) {
                mantissa = mantissa * BASE + uint64_t(*p - '0');
                --exponent;
            }
        }
    }
    if (!digits) {
        return false;
    }
    if (*p == 'e' || *p == 'E') {
        ++p;
        const bool down = *p == '-';
        if (*p == '-' || *p == '+') {
            ++p;
        }
        if (!isDigit(*p)) {
            return false;
        }
        int32_t e = 0;
        for (; isDigit(*p); ++p) {
            if (e < EXPONENT_LIMIT) {
                e = e * BASE + int32_t(*p - '0');
            }
        }
        exponent += down ? -e : e;
    }
    if (*p != '\0') {
        return false;
    }
    if (mantissa == 0) {
        v = negative ? -0.0 : 0.0;
        return true;
    }
    // Each power of ten up to 10^22 is exact in a double, so one division or multiplication
    // rounds once for every number a file holds.
    double scale = 1;
    const int32_t steps = exponent < 0 ? -exponent : exponent;
    for (int32_t i = 0; i < steps; ++i) {
        scale *= BASE;
    }
    const double magnitude = exponent < 0 ? double(mantissa) / scale : double(mantissa) * scale;
    if (!isfinite(magnitude)) {
        return false;
    }
    v = negative ? -magnitude : magnitude;
    return true;
}

YamlTree::Id YamlTree::take(Id parent, Id after, Kind kind) {
    if (freeList == NONE) {
        return NONE;
    }
    const Id id = freeList;
    freeList = entries[id].next;
    --freeCount;
    Entry& e = entries[id];
    e = Entry();
    e.name = NO_TEXT;
    e.text = NO_TEXT;
    e.parent = parent;
    e.first = NONE;
    e.kind = kind;
    if (after == NONE) {
        e.next = entries[parent].first;
        entries[parent].first = id;
    } else {
        e.next = entries[after].next;
        entries[after].next = id;
    }
    return id;
}

YamlTree::Id YamlTree::last(Id id) const {
    Id c = entries[id].first;
    while (c != NONE && entries[c].next != NONE) {
        c = entries[c].next;
    }
    return c;
}

uint32_t YamlTree::store(const char* s, size_t length) {
    if (length == 0) {
        return NO_TEXT;
    }
    if (used + length + 1 > poolBytes && compacting) {
        compact();
    }
    if (used + length + 1 > poolBytes) {
        return NOT_STORED;
    }
    const uint32_t at = uint32_t(used);
    memcpy(pool + at, s, length);
    pool[at + length] = '\0';
    used += length + 1;
    live += length + 1;
    return at;
}

void YamlTree::drop(uint32_t at) {
    if (at < NOT_STORED) {
        live -= strlen(pool + at) + 1;
    }
}

void YamlTree::compact() {
    // Each pass moves the string that starts first among those not moved yet down to the end of
    // the ones moved, so no string is overwritten before it has moved.
    size_t to = 0;
    for (;;) {
        uint32_t* nearest = nullptr;
        for (uint16_t i = 0; i < capacity; ++i) {
            Entry& e = entries[i];
            if (e.parent == FREE) {
                continue;
            }
            for (uint32_t* at : {&e.name, &e.text}) {
                if (*at < NOT_STORED && *at >= to && (nearest == nullptr || *at < *nearest)) {
                    nearest = at;
                }
            }
        }
        if (nearest == nullptr) {
            break;
        }
        const size_t n = strlen(pool + *nearest) + 1;
        memmove(pool + to, pool + *nearest, n);
        *nearest = uint32_t(to);
        to += n;
    }
    used = to;
}

bool YamlTree::valuesOnly(Id id) const {
    for (Id c = entries[id].first; c != NONE; c = entries[c].next) {
        if (entries[c].kind != Kind::VALUE) {
            return false;
        }
    }
    return true;
}
