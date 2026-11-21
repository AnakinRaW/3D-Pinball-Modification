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

/// A full path takes at most this many bytes with its end. A longer path gives a file whose every
/// request fails.
constexpr size_t FILE_PATH_BYTES = 64;   // storage.md: the longest path

/// How a request stands.
enum class FileStatus : uint8_t {
    IDLE,        // not started yet
    PENDING,     // started and not ended yet
    SUCCEEDED,   // ended and worked
    FAILED,      // ended and failed, or could not start
};

/// The order in which the storage task serves the requests. A request of a higher priority goes
/// before every waiting request of a lower one. Requests of one priority go in the order they came.
enum class FilePriority : uint8_t {
    HIGHEST,   // goes before every other request
    RAISED,    // goes before every request of NORMAL
    NORMAL,    // the priority of a request that names none
};

class StorageService;
class FileReader;
class FileWriter;

/// One read or one write on the card. The storage task writes the results into it, so it can be
/// neither copied nor moved. The buffer or the data it names stays in place until it has ended. It
/// carries one operation at a time and can be started again once that has ended.
class FileRequest {
public:
    /// Creates a request that the storage task serves in the order of its priority.
    ///
    /// @param priority  Places the request among the waiting ones. NORMAL serves it after every
    ///                  other.
    explicit FileRequest(FilePriority priority = FilePriority::NORMAL) : priority(priority) {}

    /// Waits for the end of a request that is still pending, so the storage task never writes
    /// into memory that has gone.
    ///
    /// @note  A request started from an interrupt has to live as long as the firmware, since an
    ///        interrupt cannot wait.
    ~FileRequest();

    FileRequest(const FileRequest&) = delete;
    FileRequest& operator=(const FileRequest&) = delete;

    /// Gets how the request stands.
    FileStatus status() const { return state.load(std::memory_order_acquire); }

    /// Gets whether the request has ended, as SUCCEEDED or as FAILED.
    bool ended() const;

    /// Gets how many bytes a read brought.
    ///
    /// It is 0 until the read has succeeded.
    size_t bytes() const;

    /// Gets the file's size as the request left it.
    ///
    /// It is 0 until the request has ended, for a missing file, after a card error and while the
    /// card counts as failed.
    uint32_t fileSize() const;

    /// Waits until the request has ended.
    ///
    /// @return  Whether the request succeeded. False at once for a request that was not started,
    ///          from an interrupt and from the storage task. The request then goes on.
    /// @note    Any task but the storage task may call it, never an interrupt.
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

/// A file on the card by its full path, which reads. It holds the path alone, so it can be copied
/// and let go at any time. Getting one touches no card.
class FileReader {
public:
    /// Creates a file whose every request fails.
    FileReader() = default;

    /// Starts reading the file into a buffer and returns at once.
    ///
    /// A read stops at the end of the file. A missing file and a position at or past the end
    /// bring 0 bytes and succeed.
    ///
    /// @param at      The byte of the file the read starts at.
    /// @param buffer  The buffer that takes the bytes. It stays in place until @p r has ended and
    ///                may lie in any memory. In RAM2 or the PSRAM a read is copied once more
    ///                unless @p buffer starts on 32 bytes, @p n is a multiple of 32 and @p at
    ///                starts on a sector of 512 bytes.
    /// @param n       The most bytes the read brings.
    /// @param r       The request that carries the read. A request still pending makes the
    ///                read fail.
    /// @note          Callable from any task, and from an interrupt at priority 208 or a
    ///                lower priority, that is, a higher number.
    void read(uint32_t at, void* buffer, size_t n, FileRequest& r) const;

    /// Gets the full path, empty for a file whose every request fails.
    const char* name() const { return path; }

protected:
    friend class StorageService;

    StorageService* service = nullptr;
    char path[FILE_PATH_BYTES] = {};
};

/// A file on the card that reads and writes.
class FileWriter : public FileReader {
public:
    /// Creates a file whose every request fails.
    FileWriter() = default;

    /// Starts writing bytes into the file and returns at once.
    ///
    /// A missing file is created with the directories above it. The bytes are on the card once
    /// the request has succeeded.
    ///
    /// @param at    The byte of the file the write starts at. A write that starts past the end of
    ///              the file fails.
    /// @param data  The bytes. They stay in place and unchanged until @p r has ended.
    /// @param n     How many bytes the write takes.
    /// @param r     The request that carries the write. A request still pending makes both
    ///              operations fail.
    /// @note        Callable from any task, and from an interrupt at priority 208 or a
    ///              lower priority, that is, a higher number.
    void write(uint32_t at, const void* data, size_t n, FileRequest& r) const;

    /// Starts replacing the file's content and returns at once.
    ///
    /// The file holds the new bytes alone afterwards.
    ///
    /// @param data  The bytes. They stay in place and unchanged until @p r has ended.
    /// @param n     How many bytes the file holds afterwards.
    /// @param r     The request that carries the write. A request still pending makes both
    ///              operations fail.
    /// @note        Callable from any task, and from an interrupt at priority 208 or a
    ///              lower priority, that is, a higher number.
    void rewrite(const void* data, size_t n, FileRequest& r) const;
};

/// A file on the card as the card appender writes it.
class CardFile : public AppendFile {
public:
    /// Creates the appender's file.
    ///
    /// @param file  The file on the card the appender writes into.
    explicit CardFile(const FileWriter& file) : file(file) {}

private:
    void write(uint32_t at, const uint8_t* data, size_t n) override;
    void rewrite(const uint8_t* data, size_t n) override;
    AppendStatus status() const override;

    FileWriter file;
    FileRequest request;
};

/// The file system on the card, as the storage task works it. Each open file takes one of SLOTS
/// places. Only the storage task calls it.
class CardFiles {
public:
    static constexpr uint8_t SLOTS = 16;   // storage.md: the files open at once

    /// Starts the card and reads its file system.
    ///
    /// @return  False when either fails.
    virtual bool mount() = 0;

    /// Starts the card again and reads its file system anew, after a card error or while the card
    /// counts as failed.
    ///
    /// @return  False when no card answers, when its file system does not read, or when another
    ///          card sits in the socket than the one mount() found.
    /// @note    The caller closes every slot before.
    virtual bool remount() = 0;

    /// Gets how many transfers with the card have failed since the start.
    virtual uint32_t failedTransfers() const = 0;

    /// Gets the error code of the transfer that failed last.
    virtual uint32_t errorCode() const = 0;

    /// Opens a file into a slot.
    ///
    /// @param slot    A free slot below SLOTS.
    /// @param path    The file's full path.
    /// @param writes  True opens the file for writing as well and creates a missing file with the
    ///                directories above it.
    /// @return        False when the file does not open.
    virtual bool open(uint8_t slot, const char* path, bool writes) = 0;

    /// Closes a file.
    ///
    /// Its slot is free afterwards.
    ///
    /// @param slot  The slot that holds the open file.
    virtual void close(uint8_t slot) = 0;

    /// Gets the size of a file in bytes.
    ///
    /// @param slot  The slot that holds the open file.
    virtual uint32_t size(uint8_t slot) = 0;

    /// Reads bytes of a file.
    ///
    /// @param slot  The slot that holds the open file.
    /// @param at    The byte the read starts at. It lies inside the file.
    /// @param to    The buffer that takes the bytes.
    /// @param n     How many bytes the read brings at most.
    /// @param got   Set to the bytes that arrived.
    /// @return      False when the read failed.
    virtual bool read(uint8_t slot, uint32_t at, uint8_t* to, size_t n, size_t& got) = 0;

    /// Writes bytes into a file.
    ///
    /// @param slot  The slot that holds the file, open for writing.
    /// @param at    The byte the write starts at. It lies at most at the end of the file.
    /// @param n     How many bytes the write takes.
    /// @return      False when the write failed.
    virtual bool write(uint8_t slot, uint32_t at, const uint8_t* data, size_t n) = 0;

    /// Cuts a file to a size.
    ///
    /// @param slot  The slot that holds the file, open for writing.
    /// @param size  The size in bytes the file keeps.
    /// @return      False when the cut failed.
    virtual bool cut(uint8_t slot, uint32_t size) = 0;

    /// Puts everything written into a file onto the card, its size included.
    ///
    /// @param slot  The slot that holds the file, open for writing.
    /// @return      False when putting it onto the card failed.
    virtual bool commit(uint8_t slot) = 0;

protected:
    ~CardFiles() = default;
};

/// Gets whether a read can move its bytes into a buffer straight, so it needs no copy.
///
/// That holds for memory without a cache, and for cached memory where the buffer starts on a cache
/// line, its size is a whole number of cache lines and the read starts on a sector of the file.
///
/// @param buffer  The buffer that takes the bytes.
/// @param at      The byte of the file the read starts at.
/// @param n       How many bytes the read brings at most.
bool readsInPlace(const void* buffer, uint32_t at, size_t n);

/// Keeps the requests in their order and handles the card's failures. The storage task calls
/// mount(), serveNext() and recover(). Every other member answers any caller at once.
class StorageService {
public:
    static constexpr uint8_t FAILURES = 3;   // storage.md: card errors in a row to fail

    /// Creates the service on a card it has not mounted yet.
    ///
    /// Every request fails until mount().
    ///
    /// @param card  The file system on the card the service works.
    explicit StorageService(CardFiles& card) : card(card) {}

    StorageService(const StorageService&) = delete;
    StorageService& operator=(const StorageService&) = delete;

    /// Gets a file that reads.
    ///
    /// Touches no card.
    ///
    /// @param path  The file's full path, such as "/config.yml". A longer path than
    ///              FILE_PATH_BYTES allows gives a file whose every request fails.
    FileReader open(const char* path);

    /// Gets a file that reads and writes.
    ///
    /// Touches no card.
    ///
    /// @param path  The file's full path, as for open().
    FileWriter openWriter(const char* path);

    /// Mounts the card.
    ///
    /// @return  False when the card is missing or its file system does not read. The card then
    ///          counts as failed until a restart.
    /// @note    The storage task calls it once at start-up.
    bool mount();

    /// Carries out the next request whole and ends it.
    ///
    /// @return  False when no request waits.
    /// @note    The storage task calls it.
    bool serveNext();

    /// Tries to take a failed card back into use.
    ///
    /// Does nothing unless the card counts as failed after mount() had found it.
    ///
    /// @return  Whether the card works again.
    /// @note    The storage task calls it.
    bool recover();

    /// Gets whether the card counts as failed. Every request then fails at once.
    bool failedCard() const { return down.load(std::memory_order_relaxed); }

    /// Gets whether mount() found the card.
    bool mounted() const { return wasMounted; }

    /// Gets the error code of the card's last failure.
    uint32_t lastError() const { return lastErrorCode.load(std::memory_order_relaxed); }

protected:
    ~StorageService() = default;

    /// Gets whether the caller may start a request: a task, or an interrupt at priority 208 or a
    /// lower priority, that is, a higher number.
    ///
    /// The default allows every caller.
    virtual bool callerMayStart() const { return true; }

    /// Gets whether the caller may wait for a request: a task other than the storage task.
    ///
    /// The default allows every caller.
    virtual bool callerMayWait() const { return true; }

    /// Wakes the storage task after a request was added.
    ///
    /// The default does nothing.
    virtual void wake() {}

    /// Lets the storage task run for a moment while a caller waits.
    ///
    /// The default does nothing.
    virtual void pause() {}

    /// Keeps every other task and the interrupts that start requests out of the request lists,
    /// until unlock().
    ///
    /// The default keeps no one out.
    ///
    /// @return  What unlock() needs.
    virtual uint32_t lock() { return 0; }

    /// Lets the others in again.
    ///
    /// @param held  What lock() returned.
    virtual void unlock(uint32_t held) { (void)held; }

private:
    friend class FileRequest;
    friend class FileReader;
    friend class FileWriter;

    static constexpr uint8_t PRIORITIES = 3;   // the values of FilePriority
    static constexpr uint8_t NO_SLOT = 0xFF;

    /// Holds the requests that wait, oldest first.
    struct RequestList {
        FileRequest* head = nullptr;
        FileRequest* tail = nullptr;
    };

    /// Holds what the table knows about a slot of the card.
    struct Slot {
        char path[FILE_PATH_BYTES] = {};   // empty for a free slot
        bool writes = false;               // opened for writing
        uint32_t lastUse = 0;              // the use count at the slot's last request
    };

    /// Starts one operation on @p r through the service that @p file came from.
    ///
    /// Every read and every write starts here. A file from no service fails the request at once.
    ///
    /// @param kind  The kind of operation.
    /// @param at    Where the operation starts in the file.
    /// @param into  Where a read lands.
    /// @param from  The data a write takes.
    /// @param size  The size of the operation in bytes.
    static void start(FileRequest& r, const FileReader& file, FileRequest::Kind kind, uint32_t at,
                      uint8_t* into, const uint8_t* from, size_t size);

    /// Fills @p r in and adds it to its list.
    ///
    /// A request still pending gets nothing new and ends as failed. An empty path, a failed card
    /// and a caller that may not start a request end @p r at once as failed.
    ///
    /// @param path  The file's full path.
    /// @param kind  The kind of operation.
    /// @param at    Where the operation starts in the file.
    /// @param into  Where a read lands.
    /// @param from  The data a write takes.
    /// @param size  The size of the operation in bytes.
    void add(FileRequest& r, const char* path, FileRequest::Kind kind, uint32_t at, uint8_t* into,
             const uint8_t* from, size_t size);

    /// Carries out @p r.
    ///
    /// @param slot  The slot that holds the file of @p r, set by the call.
    /// @return      Whether it worked.
    bool serve(FileRequest& r, uint8_t& slot);

    /// Gets the slot that holds the file of @p r, opening it where it is not open.
    ///
    /// @return   NO_SLOT when the file does not open.
    uint8_t find(const FileRequest& r);

    /// Ends @p r: takes it out of its list and reports to it.
    ///
    /// @param worked  Whether the request worked.
    /// @param slot    The slot that held the file of @p r.
    void end(FileRequest& r, bool worked, uint8_t slot);

    /// Counts a card error, closes every slot and starts the card anew.
    ///
    /// The card counts as failed after the third in a row, or when it does not come back.
    void failure();

    /// Closes every slot.
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
