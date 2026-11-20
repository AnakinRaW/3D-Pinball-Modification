// Appends records to a file on the SD card without ever making the writer wait. The records
// collect in a buffer the caller owns. The main loop writes them to the card once a second, or
// sooner once half the buffer is taken. A write that fails keeps its records and is tried again, so
// a record is lost only when the buffer is full.
#pragma once
#include "core/byte-ring.h"
#include <atomic>
#include <stddef.h>
#include <stdint.h>

// How a write to an AppendFile stands.
enum class AppendStatus : uint8_t {
    IDLE,        // no write has started yet
    PENDING,     // the write runs
    SUCCEEDED,   // the last write has ended and the bytes are on the card
    FAILED,      // the last write has ended without the bytes reaching the card
};

// One file on the card as the appender writes it. The storage driver's files provide it. Only a
// CardAppender calls it, so no two writers meet in one file.
class AppendFile {
protected:
    ~AppendFile() = default;

private:
    friend class CardAppender;

    // Starts writing n bytes at position at of the file and returns at once.
    // data stays in place and unchanged until status() has left PENDING.
    virtual void write(uint32_t at, const uint8_t* data, size_t n) = 0;

    // Starts replacing the file's content with n bytes and returns at once. The file then holds
    // those bytes alone. data stays in place as for write().
    virtual void rewrite(const uint8_t* data, size_t n) = 0;

    // Gets how the last write() or rewrite() stands.
    virtual AppendStatus status() const = 0;
};

// The writer appends from one task at a time. The main loop calls update(). The appender never
// waits.
class CardAppender {
public:
    static constexpr uint32_t WRITE_PERIOD_MS = 1000;   // logging.md: one write a second

    // Creates an appender with no file yet.
    // buffer holds the records until they are on the card. It stays with the appender for its
    // whole life. bytes is the buffer's size, of which the largest power of two is used.
    CardAppender(uint8_t* buffer, size_t bytes) : ring(buffer, bytes) {}

    // Appends to the end of a file whose size the caller has learned. Never waits. Before the
    // first append().
    // sizeBytes is the file's size, where the first write goes.
    void begin(AppendFile& file, uint32_t sizeBytes);

    // Turns the records appended from now on into a new file. The records appended before still go
    // to the old file. The new file is started anew with the first write, so whatever it held
    // before goes. Never waits.
    // Returns false while an earlier start() still waits for the records of the file before it to
    // reach the card. The records then keep going to the file they went to.
    bool start(AppendFile& file);

    // Copies a record in, whole or not at all. Never waits. One task at a time.
    // Returns false when no file was given yet, or when the record does not fit into the free
    // part of the buffer. The record is then lost and counted.
    bool append(const void* data, size_t n);

    // Writes the waiting records once a second has passed since the last write or half the buffer
    // is taken. flush() makes it write at once. A failed write is tried again a second later. Never
    // waits. The main loop calls it in every pass.
    void update();

    // Makes the next update() write the waiting records without waiting for a second or for half
    // the buffer. Any task, never an interrupt.
    void flush() { flushWanted.store(true, std::memory_order_relaxed); }

    // Gets how many records were lost since the last call. Starts counting anew. A writer
    // calls it once append() works again, to write a record about the loss.
    uint32_t takeLost();

    // Gets how many records were lost since the start.
    uint32_t lost() const { return lostTotal; }

    // Gets the size the current file has once every waiting record is on the card. The writer
    // compares it with a limit before it appends.
    uint32_t fileBytes() const { return nextFileBytes; }

private:
    // Starts the next write if one is due.
    void startWrite();

    ByteRing ring;

    // The reader's side, moved by update() alone once begin() or start() has handed it a file.
    AppendFile* file = nullptr;   // the file the oldest waiting records go to
    uint32_t position = 0;        // where the next write goes in that file
    bool fresh = false;           // the next write starts that file anew
    size_t inFlight = 0;          // bytes the running write takes, 0 for none
    bool lastFailed = false;      // the last write failed, so the next waits a second
    // The last write took only the records up to the end of the buffer. The rest is due at once.
    bool restOwed = false;
    uint32_t lastTryUs = 0;   // nowUs() when the last write started
    std::atomic<bool> flushWanted{false};

    // A start() that waits for the old file's records to reach the card. The writer sets nextAt
    // and then nextFile, the reader clears nextFile once it has turned to the new file.
    std::atomic<AppendFile*> nextFile{nullptr};
    std::atomic<size_t> nextAt{0};   // the ring's appended() count where the new file begins

    // The writer's side.
    bool hasFile = false;         // begin() or start() has handed over a file
    uint32_t nextFileBytes = 0;   // the current file's size once every record is on the card
    uint32_t lostSince = 0;       // records lost since the last takeLost()
    uint32_t lostTotal = 0;
};
