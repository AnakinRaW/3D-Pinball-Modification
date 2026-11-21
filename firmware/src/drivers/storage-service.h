// Reads and writes files on the SD card without making the caller wait. A caller starts a read or
// a write and returns at once. The storage task carries the requests out one after another, so
// nothing but that task ever waits for the card. A caller learns the end from
// its request. A request of a higher priority goes before the waiting requests of a lower one, so
// a stream that must not run dry is served while a game saves.
//
// This part touches no hardware. The storage driver in drivers/storage.h runs it in the storage
// task on the card that SdFat reads.
//
// Design: docs/firmware/drivers/storage.md.
#pragma once
#include "io/card-appender.h"
#include <atomic>
#include <stddef.h>
#include <stdint.h>

// A full path takes at most this many bytes with its end. A longer path gives a file whose every
// request fails.
constexpr size_t FILE_PATH_BYTES = 64;   // storage.md: the longest path

// How a request stands.
enum class FileStatus : uint8_t {
    IDLE,        // not started yet
    PENDING,     // started and not ended yet
    SUCCEEDED,   // ended and worked
    FAILED,      // ended and failed, or could not start
};

// The order in which the storage task serves the requests. A request of a higher priority goes
// before every waiting request of a lower one. Requests of one priority go in the order they came.
enum class FilePriority : uint8_t {
    HIGHEST,   // goes before every other request
    RAISED,    // goes before every request of NORMAL
    NORMAL,    // the priority of a request that names none
};

class StorageService;
class FileReader;
class FileWriter;

// One read or one write on the card. The storage task writes the results into it, so it can be
// neither copied nor moved. The buffer or the data it names stays in place until it has ended. It
// carries one operation at a time and can be started again once that has ended.
class FileRequest {
public:
    // Creates a request that the storage task serves in the order of its priority.
    // priority places the request among the waiting ones. NORMAL serves it after every other.
    explicit FileRequest(FilePriority priority = FilePriority::NORMAL) : priority(priority) {}

    // Waits for the end of a request that is still pending, so the storage task never writes into
    // memory that has gone. A request started from an interrupt has to live as long as the
    // firmware, since an interrupt cannot wait.
    ~FileRequest();

    FileRequest(const FileRequest&) = delete;
    FileRequest& operator=(const FileRequest&) = delete;

    // Gets how the request stands.
    FileStatus status() const { return state.load(std::memory_order_acquire); }

    // Gets whether the request has ended, as SUCCEEDED or as FAILED.
    bool ended() const;

    // Gets how many bytes a read brought. It is 0 until the read has succeeded.
    size_t bytes() const;

    // Gets the file's size as the request left it. It is 0 until the request has ended, for a
    // missing file, after a card error and while the card counts as failed.
    uint32_t fileSize() const;

    // Waits until the request has ended. Any task but the storage task, never an interrupt.
    // Returns whether the request succeeded. Returns false at once for a request that was not
    // started, from an interrupt and from the storage task. The request then goes on.
    bool wait() const;

private:
    friend class StorageService;
    friend class FileReader;
    friend class FileWriter;

    enum class Kind : uint8_t { READ, WRITE, REWRITE };

    const FilePriority priority;
    StorageService* service = nullptr;   // the service that took the request last
    FileRequest* next = nullptr;         // the request behind it in its list
    char path[FILE_PATH_BYTES] = {};     // a copy of its file's path
    Kind kind = Kind::READ;
    uint32_t at = 0;                 // where it starts in the file
    uint8_t* into = nullptr;         // where a read lands
    const uint8_t* from = nullptr;   // the data a write takes
    size_t size = 0;
    bool dropped = false;     // a second operation came before the end
    size_t got = 0;           // the bytes a read brought
    uint32_t sizeBytes = 0;   // the file's size as the request left it
    std::atomic<FileStatus> state{FileStatus::IDLE};
};

// A file on the card by its full path, which reads. It holds the path alone, so it can be copied
// and let go at any time. Getting one touches no card.
class FileReader {
public:
    // Creates a file whose every request fails.
    FileReader() = default;

    // Starts reading the file into a buffer and returns at once. A read stops at the end of the
    // file. A missing file and a position at or past the end bring 0 bytes and succeed. Any task,
    // and an interrupt at FreeRTOS's limit of 208 or below.
    // at is the byte of the file the read starts at.
    // buffer takes the bytes and stays in place until r has ended. It may lie in any memory. In
    // RAM2 or the PSRAM a read is copied once more unless buffer starts on 32 bytes, n is a
    // multiple of 32 and at starts on a sector of 512 bytes.
    // n is the most bytes the read brings.
    // r is the request that carries the read. A request still pending makes both operations fail.
    void read(uint32_t at, void* buffer, size_t n, FileRequest& r) const;

    // Gets the full path, empty for a file whose every request fails.
    const char* name() const { return path; }

protected:
    friend class StorageService;

    StorageService* service = nullptr;
    char path[FILE_PATH_BYTES] = {};
};

// A file on the card that reads and writes.
class FileWriter : public FileReader {
public:
    // Creates a file whose every request fails.
    FileWriter() = default;

    // Starts writing bytes into the file and returns at once. A missing file is created with the
    // directories above it. The bytes are on the card once the request has succeeded. Any task,
    // and an interrupt at FreeRTOS's limit of 208 or below.
    // at is the byte of the file the write starts at. A write that starts past the end of the file
    // fails.
    // data holds the bytes and stays in place and unchanged until r has ended.
    // n is how many bytes the write takes.
    // r is the request that carries the write. A request still pending makes both operations fail.
    void write(uint32_t at, const void* data, size_t n, FileRequest& r) const;

    // Starts replacing the file's content and returns at once. The file holds the new bytes alone
    // afterwards. Any task, and an interrupt at FreeRTOS's limit of 208 or below.
    // data holds the bytes and stays in place and unchanged until r has ended.
    // n is how many bytes the file holds afterwards.
    // r is the request that carries the write. A request still pending makes both operations fail.
    void rewrite(const void* data, size_t n, FileRequest& r) const;
};

// A file on the card as the card appender writes it.
class CardFile : public AppendFile {
public:
    // Creates the appender's file.
    // file is the file on the card the appender writes into.
    explicit CardFile(const FileWriter& file) : file(file) {}

private:
    void write(uint32_t at, const uint8_t* data, size_t n) override;
    void rewrite(const uint8_t* data, size_t n) override;
    AppendStatus status() const override;

    FileWriter file;
    FileRequest request;
};

// The file system on the card, as the storage task works it. Each open file takes one of SLOTS
// places. Only the storage task calls it.
class CardFiles {
public:
    static constexpr uint8_t SLOTS = 16;   // storage.md: the files open at once

    // Starts the card and reads its file system. Returns false when either fails.
    virtual bool mount() = 0;

    // Starts the card again and reads its file system anew, after a card error or while the card
    // counts as failed. The caller closes every slot before. Returns false when no card answers,
    // when its file system does not read, or when another card sits in the socket than the one
    // mount() found.
    virtual bool remount() = 0;

    // Gets how many transfers with the card have failed since the start.
    virtual uint32_t failedTransfers() const = 0;

    // Gets the error code of the transfer that failed last.
    virtual uint32_t errorCode() const = 0;

    // Opens a file into a slot. Returns false when the file does not open.
    // slot is a free slot below SLOTS.
    // path is the file's full path.
    // writes opens the file for writing as well and creates a missing file with the directories
    // above it.
    virtual bool open(uint8_t slot, const char* path, bool writes) = 0;

    // Closes a file. Its slot is free afterwards.
    // slot holds the open file.
    virtual void close(uint8_t slot) = 0;

    // Gets the size of a file in bytes.
    // slot holds the open file.
    virtual uint32_t size(uint8_t slot) = 0;

    // Reads bytes of a file. Returns false when the read failed.
    // slot holds the open file.
    // at is the byte the read starts at. It lies inside the file.
    // to takes the bytes.
    // n is how many bytes the read brings at most.
    // got is set to the bytes that arrived.
    virtual bool read(uint8_t slot, uint32_t at, uint8_t* to, size_t n, size_t& got) = 0;

    // Writes bytes into a file. Returns false when the write failed.
    // slot holds the file, open for writing.
    // at is the byte the write starts at. It lies at most at the end of the file.
    // data holds the bytes.
    // n is how many bytes the write takes.
    virtual bool write(uint8_t slot, uint32_t at, const uint8_t* data, size_t n) = 0;

    // Cuts a file to a size. Returns false when that failed.
    // slot holds the file, open for writing.
    // size is the size in bytes the file keeps.
    virtual bool cut(uint8_t slot, uint32_t size) = 0;

    // Puts everything written into a file onto the card, its size included. Returns false when
    // that failed.
    // slot holds the file, open for writing.
    virtual bool commit(uint8_t slot) = 0;

protected:
    ~CardFiles() = default;
};

// Gets whether a read can move its bytes into a buffer straight, so it needs no copy. That holds
// for memory without a cache, and for cached memory where the buffer starts on a cache line, its
// size is a whole number of cache lines and the read starts on a sector of the file.
// buffer takes the bytes.
// at is the byte of the file the read starts at.
// n is how many bytes the read brings at most.
bool readsInPlace(const void* buffer, uint32_t at, size_t n);

// Keeps the requests in their order and handles the card's failures. The storage task calls
// mount(), serveNext() and recover(). Every other member answers any caller at once.
class StorageService {
public:
    static constexpr uint8_t FAILURES = 3;   // storage.md: card errors in a row to fail

    // Creates the service on a card it has not mounted yet. Every request fails until mount().
    // card is the file system on the card the service works.
    explicit StorageService(CardFiles& card) : card(card) {}

    StorageService(const StorageService&) = delete;
    StorageService& operator=(const StorageService&) = delete;

    // Gets a file that reads. Touches no card.
    // path is the file's full path, such as "/config.yml". A longer path than FILE_PATH_BYTES
    // allows gives a file whose every request fails.
    FileReader open(const char* path);

    // Gets a file that reads and writes. Touches no card.
    // path is the file's full path, as for open().
    FileWriter openWriter(const char* path);

    // Mounts the card. The storage task calls it once at start-up. Returns false when the card is
    // missing or its file system does not read. The card then counts as failed until a restart.
    bool mount();

    // Carries out the next request whole and ends it. The storage task calls it. Returns false
    // when no request waits.
    bool serveNext();

    // Tries to take a failed card back into use. The storage task calls it. Does nothing unless
    // the card counts as failed after mount() had found it. Returns whether the card works again.
    bool recover();

    // Gets whether the card counts as failed. Every request then fails at once.
    bool failedCard() const { return down.load(std::memory_order_relaxed); }

    // Gets whether mount() found the card.
    bool mounted() const { return wasMounted; }

    // Gets the error code of the card's last failure.
    uint32_t lastError() const { return lastErrorCode.load(std::memory_order_relaxed); }

protected:
    ~StorageService() = default;

    // Gets whether the caller may start a request: a task, or an interrupt at FreeRTOS's limit or
    // below. The default allows every caller.
    virtual bool callerMayStart() const { return true; }

    // Gets whether the caller may wait for a request: a task other than the storage task. The
    // default allows every caller.
    virtual bool callerMayWait() const { return true; }

    // Wakes the storage task after a request was added. The default does nothing.
    virtual void wake() {}

    // Lets the storage task run for a moment while a caller waits. The default does nothing.
    virtual void pause() {}

    // Keeps every other task and the interrupts that start requests out of the request lists,
    // until unlock(). Returns what unlock() needs. The default keeps no one out.
    virtual uint32_t lock() { return 0; }

    // Lets the others in again. held is what lock() returned.
    virtual void unlock(uint32_t held) { (void)held; }

private:
    friend class FileRequest;
    friend class FileReader;
    friend class FileWriter;

    static constexpr uint8_t PRIORITIES = 3;   // the values of FilePriority
    static constexpr uint8_t NO_SLOT = 0xFF;

    // Holds the requests that wait, oldest first.
    struct RequestList {
        FileRequest* head = nullptr;
        FileRequest* tail = nullptr;
    };

    // Holds what the table knows about a slot of the card.
    struct Slot {
        char path[FILE_PATH_BYTES] = {};   // empty for a free slot
        bool writes = false;               // opened for writing
        uint32_t lastUse = 0;              // the use count at the slot's last request
    };

    // Starts one operation on r through the service that file came from. Every read and every
    // write starts here. A file from no service fails the request at once.
    static void start(FileRequest& r, const FileReader& file, FileRequest::Kind kind, uint32_t at,
                      uint8_t* into, const uint8_t* from, size_t size);

    // Fills r in and adds it to its list. A request still pending gets nothing new and ends as
    // failed. An empty path, a failed card and a caller that may not start a request end r at once
    // as failed.
    void add(FileRequest& r, const char* path, FileRequest::Kind kind, uint32_t at, uint8_t* into,
             const uint8_t* from, size_t size);

    // Carries r out and gets whether it worked.
    bool serve(FileRequest& r, uint8_t& slot);

    // Gets the slot that holds r's file, opening it where it is not open. Returns NO_SLOT when the
    // file does not open.
    uint8_t find(const FileRequest& r);

    // Ends r: takes it out of its list and reports to it.
    void end(FileRequest& r, bool worked, uint8_t slot);

    // Counts a card error, closes every slot and starts the card anew. The card counts as failed
    // after the third in a row, or when it does not come back.
    void failure();

    // Closes every slot.
    void closeAll();

    CardFiles& card;
    RequestList lists[PRIORITIES];
    Slot slots[CardFiles::SLOTS];
    uint32_t uses = 0;      // requests served, which orders the slots by their last use
    uint8_t failures = 0;   // card errors in a row
    bool wasMounted = false;
    std::atomic<bool> down{true};   // the card counts as failed
    std::atomic<uint32_t> lastErrorCode{0};
};
