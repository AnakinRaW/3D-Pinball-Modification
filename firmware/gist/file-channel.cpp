// reads files one after another from the card into a ring of chunks, for the one place that takes
// the bytes; every call comes from that place and returns at once
template <uint8_t Chunks>                  // the chunks the ring holds, such as FileChannel<4>
class FileChannel {
    static_assert(Chunks > 0, "a ring holds at least one chunk");

public:
    // a ring of Chunks chunks of chunk bytes each at ring, which starts on a 32-byte boundary, read
    // with priority
    FileChannel(uint8_t* ring, uint32_t chunk, Storage::Priority priority = Storage::Priority::Normal)
        : read_(priority, false, IRQ_NUMBER_t(0)), ring_(ring), chunk_(chunk) {}

    // the same, and every read raises the interrupt wake when it ends
    FileChannel(uint8_t* ring, uint32_t chunk, IRQ_NUMBER_t wake,
              Storage::Priority priority = Storage::Priority::Normal)
        : read_(priority, true, wake), ring_(ring), chunk_(chunk) {}

    // reads file from its beginning, and from its beginning again at its end when loop is set; the
    // bytes of the file read before are gone at once
    void start(const FileReader& file, bool loop);

    void stop();                           // ends the file it reads; its bytes are gone at once

    // copies up to n bytes from the front of the ring into to, across chunks, or skips them when to
    // is nullptr; starts the next read where a chunk is free and gets how many it took
    size_t take(void* to, size_t n);

    // gets how many bytes take() could take now; also starts the next read where a chunk is free
    size_t available();

    bool ended() const;                    // gets whether the file has run out or a read has failed and every byte before is taken, or no file is read

private:
    void pump();                           // takes a read that has ended into its chunk and starts the next while a chunk is free

    FileRequest read_;                     // the one read on its way
    uint8_t*    ring_;
    uint32_t    chunk_;
    FileReader  file_;                     // the file it reads, empty for none
    bool        loop_    = false;
    bool        reading_ = false;          // read_ runs into the chunk at back_
    bool        stale_   = false;          // read_ belongs to a file read before, so its bytes are dropped
    bool        failed_  = false;          // a read of the file has failed
    bool        done_    = true;           // no read is left, since the file has run out or none is read
    uint32_t    at_      = 0;              // where in the file the next read starts
    uint8_t     front_   = 0;              // the chunk take() takes from
    uint8_t     back_    = 0;              // the chunk the next read fills
    uint8_t     count_   = 0;              // the chunks that hold bytes
    size_t      taken_   = 0;              // the bytes of the front chunk take() has taken
    uint32_t    bytes_[Chunks] = {};       // the bytes each chunk holds
};

template <uint8_t Chunks>
void FileChannel<Chunks>::start(const FileReader& file, bool loop) {
    file_   = file;
    loop_   = loop;
    stale_  = reading_;                    // a read of the old file still on its way lands later and is dropped
    failed_ = false;
    done_   = !file_.name()[0];
    at_     = 0;
    front_  = 0;
    back_   = 0;
    count_  = 0;
    taken_  = 0;
    pump();
}

template <uint8_t Chunks>
void FileChannel<Chunks>::stop() {
    start(FileReader{}, false);            // a file with no name reads nothing
}

// the size the last read found tells where the file ends, so no read starts past the end
template <uint8_t Chunks>
void FileChannel<Chunks>::pump() {
    if (reading_) {
        const FileStatus s = read_.status();
        if (s == FileStatus::Pending) return;
        reading_ = false;
        if (stale_) {
            stale_ = false;                // the old file's bytes landed in a chunk that counts as free
        } else if (s == FileStatus::Failed) {
            failed_ = true;
            return;
        } else {
            const uint32_t got = read_.bytes();
            if (got > 0) {
                bytes_[back_] = got;
                back_  = (back_ + 1) % Chunks;
                count_ = count_ + 1;
            }
            at_ = at_ + got;
            if (at_ >= read_.fileSize()) {     // the end of the file
                if (loop_ && read_.fileSize() > 0) at_ = 0;
                else                           done_ = true;
            }
        }
    }
    if (done_ || failed_ || count_ == Chunks) return;
    file_.read(at_, ring_ + back_ * chunk_, chunk_, read_);   // a read at the end of the file brings fewer bytes
    reading_ = true;
}

template <uint8_t Chunks>
size_t FileChannel<Chunks>::take(void* to, size_t n) {
    pump();
    size_t got = 0;
    while (got < n && count_ > 0) {
        const size_t m = min((size_t)bytes_[front_] - taken_, n - got);
        if (to) memcpy(static_cast<uint8_t*>(to) + got, ring_ + front_ * chunk_ + taken_, m);
        taken_ = taken_ + m;
        got    = got + m;
        if (taken_ == bytes_[front_]) {    // the chunk is used up, so the next read can fill it
            taken_ = 0;
            front_ = (front_ + 1) % Chunks;
            count_ = count_ - 1;
            pump();
        }
    }
    return got;
}

template <uint8_t Chunks>
size_t FileChannel<Chunks>::available() {
    pump();
    size_t n = 0;
    for (uint8_t i = 0, c = front_; i < count_; ++i, c = (c + 1) % Chunks) n = n + bytes_[c];
    return n - taken_;
}

template <uint8_t Chunks>
bool FileChannel<Chunks>::ended() const {
    return (failed_ || done_) && count_ == 0;
}
