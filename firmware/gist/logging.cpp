// the machine's log on the card in /log.txt; a game, a component and the host write into it from
// the main loop
class Logger {
public:
    explicit Logger(FileSystem& files) : files_(files) {}

    // opens /log.txt and goes on at its end; the main task only, right after the storage
    // driver's begin()
    void begin();

    // formats a line like printf, puts the time since the machine started and who in front of it
    // and collects it; returns at once. False when the line is lost
    bool write(const char* who, const char* format, ...);

    // writes the collected lines once kFlushUs has passed and new lines have come in; the main
    // loop calls it in every pass
    void update(uint32_t now);

    uint32_t lost() const { return lost_; }   // gets how many lines were lost

    static constexpr size_t kBuffer = 2048;   // the lines collected between two writes, and so the longest line

private:
    static constexpr uint32_t kSize    = 1048576;   // 1 MB, after which the logger empties the file and starts it over
    static constexpr uint32_t kFlushUs = 1000000;   // 1 s from one write to the next

    uint32_t sinceStartMs();                        // gets the milliseconds since setup() started the clock, past the wraps of nowUs()

    FileSystem& files_;
    FileWriter  file_;                              // /log.txt
    FileRequest write_;                             // the write of the collected lines, or the rewrite that empties the file
    bool        emptying_ = false;                  // write_ empties the file
    char        text_[kBuffer];                     // the lines collected since the last write
    char        out_[kBuffer];                      // the lines the last write took, which stay in place until it has ended
    size_t      used_     = 0;                      // bytes of lines in text_
    size_t      sent_     = 0;                      // bytes the last write took
    uint32_t    size_     = 0;                      // where the next write starts in the file
    bool        known_    = false;                  // begin() found the end of the file
    uint32_t    written_  = 0;                      // nowUs() of the last write
    uint32_t    lost_     = 0;
    uint32_t    lastUs_   = 0;                      // nowUs() when sinceStartMs() last ran
    uint32_t    wraps_    = 0;                      // the wraps of nowUs() since the start
};

void Logger::begin() {
    file_ = files_.openFull("/log.txt");
    FileRequest r;
    file_.read(0, nullptr, 0, r);                   // a read of 0 bytes brings only the file's size
    known_ = r.wait();                              // a log whose end is unknown takes no line until a restart
    size_  = r.fileSize();
}

bool Logger::write(const char* who, const char* format, ...) {
    if (!known_) {                                  // before begin(), or a log whose end is unknown
        lost_ = lost_ + 1;
        return false;
    }
    char* const  at   = text_ + used_;               // the line goes straight into the buffer
    const size_t room = kBuffer - used_;
    int n = snprintf(at, room, "%lu %s ", (unsigned long)sinceStartMs(), who);   // the time since the start in ms and the writer
    if (n >= 0 && (size_t)n < room) {
        va_list args;
        va_start(args, format);
        n += vsnprintf(at + n, room - n, format, args);
        va_end(args);
    }
    if (n < 0 || (size_t)n + 1 >= room) {           // the line and its line break do not fit until the next write
        lost_ = lost_ + 1;
        return false;
    }
    at[n] = '\n';
    used_ = used_ + n + 1;
    return true;
}

// counts every wrap of nowUs(), which comes every 71.6 minutes; the main loop calls update(), and
// with it this, in every pass, so no wrap goes unseen
uint32_t Logger::sinceStartMs() {
    const uint32_t us = nowUs();
    if (us < lastUs_) wraps_ = wraps_ + 1;
    lastUs_ = us;
    return (uint32_t)((((uint64_t)wraps_ << 32) | us) / 1000);
}

void Logger::update(uint32_t now) {
    sinceStartMs();                                 // keeps the count of wraps going
    const FileStatus s = write_.status();
    if (s == FileStatus::Pending) return;           // the last write, or the emptying, still runs
    if (s == FileStatus::Succeeded) size_ = emptying_ ? 0 : size_ + sent_;   // a failed write leaves size_, so the next one writes over its place
    emptying_ = false;
    sent_     = 0;
    if (!known_ || used_ == 0 || now - written_ < kFlushUs) return;
    if (size_ >= kSize) {                           // the file is full: it is emptied before the next lines go in
        file_.rewrite(out_, 0, write_);
        emptying_ = true;
        return;
    }
    memcpy(out_, text_, used_);                     // the write takes out_, so text_ goes on collecting
    file_.write(size_, out_, used_, write_);
    sent_    = used_;
    used_    = 0;
    written_ = now;
}
