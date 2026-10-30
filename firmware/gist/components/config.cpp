#include <ctype.h>
#include <math.h>
#include <optional>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yaml.h>   // libyaml 0.2.5, https://github.com/yaml/libyaml; C, reports a broken file by return value

// the machine's settings from /config.yml, held as a tree of sections, lists and values. A key names
// a value by its path, such as "servo.start_deg", "solenoid.pull_ms.2" or "ir.channels.0.order". The
// part that uses a setting reads it with its own default and range. Reading never changes the tree; a
// mutex guards it, so any task may read and write, and no interrupt may
class Config {
public:
    explicit Config(FileSystem& files) : files_(files), lock_(xSemaphoreCreateMutexStatic(&lockBuffer_)) {}

    // reads /config.yml into the tree; a file that is missing, broken or too large leaves the tree
    // empty, so every setting takes its default. The main task, right after the logger's begin()
    void begin() {
        static char text[kFileMax + 1];   // one byte more, so a file that fills the buffer shows as too long
        FileRequest r;
        storage.open(kPath).read(0, text, sizeof(text), r);
        if (!r.wait()) return note("%s unreadable, every setting at its default", kPath);
        if (r.bytes() == 0) return note("%s missing, every setting at its default", kPath);
        if (r.bytes() > kFileMax) {
            return note("%s larger than %u bytes, every setting at its default", kPath, unsigned(kFileMax));
        }

        yaml_parser_t   parser;
        yaml_document_t doc;
        yaml_parser_initialize(&parser);
        yaml_parser_set_input_string(&parser, reinterpret_cast<const unsigned char*>(text), r.bytes());
        if (yaml_parser_load(&parser, &doc)) {
            load(doc, yaml_document_get_root_node(&doc), kRoot, nullptr);
            yaml_document_delete(&doc);
        } else {
            note("%s line %u: %s, every setting at its default", kPath, unsigned(parser.problem_mark.line + 1),
                 parser.problem);
        }
        yaml_parser_delete(&parser);
    }

    // gets the number under key, fitted to min to max, or fallback where the key is missing, holds ~ or
    // holds no number; a missing key, a value that is no number and a fitted value each write a line
    // to the log. Any task
    double get(const char* key, double fallback, double min, double max) const {
        Lock l(lock_);
        const int16_t i = find(key);
        if (i < 0) return note("%s missing, %g taken", key, fallback), fallback;
        if (unset(nodes_[i].text)) return fallback;
        double v;
        if (!parse(nodes_[i].text, v)) return note("%s holds no number, %g taken", key, fallback), fallback;
        return fit(key, v, min, max);
    }

    // gets the number under key, fitted to min to max, or nothing where the key is missing, holds ~ or
    // holds no number; a value that is no number and a fitted value each write a line to the log. Any task
    std::optional<double> get(const char* key, double min, double max) const {
        Lock l(lock_);
        const int16_t i = find(key);
        if (i < 0 || unset(nodes_[i].text)) return {};
        double v;
        if (!parse(nodes_[i].text, v)) return note("%s holds no number, left unset", key), std::nullopt;
        return fit(key, v, min, max);
    }

    // gets how many entries the list under key holds, 0 where there is none. Any task
    uint8_t count(const char* key) const {
        Lock l(lock_);
        const int16_t i = find(key);
        if (i < 0 || nodes_[i].kind != Kind::List) return 0;
        uint8_t n = 0;
        for (const Node& c : nodes_) n += c.parent == i;
        return n;
    }

    // sets key to value, creating the sections and list entries above it; false once the tree is full.
    // The next save() writes it. Any task
    bool set(const char* key, double value) {
        Lock l(lock_);
        const int16_t i = make(key);
        if (i < 0) return false;
        snprintf(nodes_[i].text, sizeof(Node::text), "%g", value);
        return true;
    }

    // removes key with everything under it; the next save() writes the file without it. Any task
    void remove(const char* key) {
        Lock l(lock_);
        const int16_t i = find(key);
        if (i < 0) return;
        nodes_[i].parent = kFree;
        for (bool freed = true; freed;) {   // frees every node whose parent is free, level by level
            freed = false;
            for (Node& c : nodes_) {
                if (c.parent >= 0 && nodes_[c.parent].parent == kFree) c.parent = kFree, freed = true;
            }
        }
    }

    // starts writing the tree as the whole file, so it survives a restart; false while the previous
    // save is still being written. Any task
    bool save() {
        Lock l(lock_);
        if (save_.status() == FileStatus::Pending) return false;
        files_.openFull(kPath).rewrite(out_, render(), save_);   // out_ stays unchanged until save_ has ended
        return true;
    }

private:
    static constexpr const char* kPath    = "/config.yml";
    static constexpr size_t      kFileMax = 4096;   // the largest file read, 4 KB
    static constexpr int16_t     kNodes   = 128;    // sections, list entries and values together
    static constexpr int16_t     kRoot    = -1;     // the parent of a section
    static constexpr int16_t     kFree    = -2;     // the parent of an unused node

    enum class Kind : uint8_t { Value, Map, List };

    struct Node {
        int16_t parent   = kFree;
        Kind    kind     = Kind::Value;
        char    name[24] = {};      // its key, or its index inside a list
        char    text[16] = {};      // a value's text as the file holds it
    };

    // holds the mutex for as long as it lives
    struct Lock {
        explicit Lock(SemaphoreHandle_t m) : m_(m) { xSemaphoreTake(m_, portMAX_DELAY); }
        ~Lock() { xSemaphoreGive(m_); }
        SemaphoreHandle_t m_;
    };

    template <typename... A>
    static void note(const char* format, A... args) { logger.write("config", format, args...); }

    // gets v at the nearest end of min to max; a corrected value goes into the log under key
    static double fit(const char* key, double v, double min, double max) {
        const double fitted = v < min ? min : v > max ? max : v;
        if (fitted != v) note("%s: %g -> %g", key, v, fitted);
        return fitted;
    }

    // gets whether a value's text reads as unset
    static bool unset(const char* s) { return !*s || !strcmp(s, "~") || !strcmp(s, "null"); }

    // gets whether s holds a number and nothing else
    static bool parse(const char* s, double& v) {
        char* end;
        v = strtod(s, &end);
        return end != s && *end == 0;
    }

    // gets the child of parent named name, or -1
    int16_t child(int16_t parent, const char* name) const {
        for (int16_t i = 0; i < kNodes; ++i) {
            if (nodes_[i].parent == parent && !strcmp(nodes_[i].name, name)) return i;
        }
        return -1;
    }

    // gets a free node under parent, or -1 once the tree is full
    int16_t add(int16_t parent, const char* name, Kind kind) {
        for (int16_t i = 0; i < kNodes; ++i) {
            if (nodes_[i].parent != kFree) continue;
            nodes_[i] = {parent, kind, {}, {}};
            strncpy(nodes_[i].name, name, sizeof(Node::name) - 1);
            return i;
        }
        note("more than %d settings, the rest is ignored", int(kNodes));
        return -1;
    }

    // gets the node under key, or -1
    int16_t find(const char* key) const {
        char path[64];
        strncpy(path, key, sizeof(path) - 1);
        path[sizeof(path) - 1] = 0;
        int16_t at = kRoot;
        char*   rest;
        for (char* part = strtok_r(path, ".", &rest); part; part = strtok_r(nullptr, ".", &rest)) {
            if ((at = child(at, part)) < 0) return -1;
        }
        return at;
    }

    // gets the node under key, adding what is missing: a list where the next part of the key is an
    // index and a section otherwise; -1 once the tree is full
    int16_t make(const char* key) {
        char path[64];
        strncpy(path, key, sizeof(path) - 1);
        path[sizeof(path) - 1] = 0;
        int16_t at = kRoot;
        char*   rest;
        for (char *part = strtok_r(path, ".", &rest), *next; part; part = next) {
            next         = strtok_r(nullptr, ".", &rest);
            const Kind k = !next ? Kind::Value : isdigit(*next) ? Kind::List : Kind::Map;
            int16_t    i = child(at, part);
            if (i < 0 && (i = add(at, part, k)) < 0) return -1;
            at = i;
        }
        return at;
    }

    // takes a parsed node into the tree under parent, as name
    void load(yaml_document_t& doc, yaml_node_t* n, int16_t parent, const char* name) {
        if (!n || (!name && n->type != YAML_MAPPING_NODE)) return;   // the file's top holds sections only
        int16_t i = parent;
        if (name) {
            const Kind k = n->type == YAML_MAPPING_NODE ? Kind::Map : n->type == YAML_SEQUENCE_NODE ? Kind::List : Kind::Value;
            if ((i = add(parent, name, k)) < 0) return;
        }
        if (n->type == YAML_SCALAR_NODE) {
            strncpy(nodes_[i].text, reinterpret_cast<const char*>(n->data.scalar.value), sizeof(Node::text) - 1);
        } else if (n->type == YAML_MAPPING_NODE) {
            for (yaml_node_pair_t* p = n->data.mapping.pairs.start; p < n->data.mapping.pairs.top; ++p) {
                const yaml_node_t* key = yaml_document_get_node(&doc, p->key);
                if (key && key->type == YAML_SCALAR_NODE) {
                    load(doc, yaml_document_get_node(&doc, p->value), i, reinterpret_cast<const char*>(key->data.scalar.value));
                }
            }
        } else if (n->type == YAML_SEQUENCE_NODE) {
            for (yaml_node_item_t* e = n->data.sequence.items.start; e < n->data.sequence.items.top; ++e) {
                char index[4];
                snprintf(index, sizeof(index), "%d", int(e - n->data.sequence.items.start));
                load(doc, yaml_document_get_node(&doc, *e), i, index);
            }
        }
    }

    // -- writing ------------------------------------------------------------------

    // writes the tree as the file's text into out_ and gets its length
    size_t render() {
        n_ = 0;
        for (int16_t i = 0; i < kNodes; ++i) {
            if (nodes_[i].parent == kRoot) write(i, 0);
        }
        return n_;
    }

    template <typename... A>
    void put(const char* format, A... args) {
        const int k = snprintf(out_ + n_, sizeof(out_) - n_, format, args...);
        if (k > 0) n_ = min(n_ + size_t(k), sizeof(out_) - 1);
    }

    // writes node i at its indent: a value as key: text, a list of values in brackets, a list of
    // sections one entry to a line in braces
    void write(int16_t i, int indent) {
        const Node& n = nodes_[i];
        if (n.kind == Kind::Value) return put("%*s%s: %s\n", indent, "", n.name, n.text);
        if (n.kind == Kind::Map) {
            put("%*s%s:\n", indent, "", n.name);
            for (int16_t c = 0; c < kNodes; ++c) {
                if (nodes_[c].parent == i) write(c, indent + 2);
            }
            return;
        }
        const int16_t first = child(i, "0");
        if (first < 0 || nodes_[first].kind == Kind::Value) {   // a list of values, or an empty one
            put("%*s%s: [", indent, "", n.name);
            for (int16_t k = 0, e; (e = child(i, index(k))) >= 0; ++k) put(k ? ", %s" : "%s", nodes_[e].text);
            return put("]\n");
        }
        put("%*s%s:\n", indent, "", n.name);
        for (int16_t k = 0, e; (e = child(i, index(k))) >= 0; ++k) {
            put("%*s- {", indent + 2, "");
            bool more = false;
            for (int16_t c = 0; c < kNodes; ++c) {
                if (nodes_[c].parent != e) continue;
                put(more ? ", %s: %s" : "%s: %s", nodes_[c].name, nodes_[c].text);
                more = true;
            }
            put("}\n");
        }
    }

    // gets k as the name of a list entry
    static const char* index(int16_t k) {
        static char s[4];
        snprintf(s, sizeof(s), "%d", int(k));
        return s;
    }

    FileSystem&       files_;
    StaticSemaphore_t lockBuffer_;
    SemaphoreHandle_t lock_;       // guards nodes_, out_ and save_
    Node              nodes_[kNodes];
    char              out_[kFileMax];   // the text save() writes, unchanged until save_ has ended
    size_t            n_ = 0;           // how much of out_ render() has filled
    FileRequest       save_;            // the write save() started last
};
