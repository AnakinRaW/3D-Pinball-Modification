#include "storage-service.h"
#include "logging/debug-log.h"
#include <string.h>

/// RAM1 is the i.MX RT1062's tightly coupled data memory, which no cache serves. PJRC's linker
/// script for the Teensy 4.1 places its 512 KB at 0x20000000.
constexpr uintptr_t RAM1_START = 0x20000000;
constexpr uintptr_t RAM1_BYTES = 0x80000;
constexpr uintptr_t CACHE_LINE_BYTES = 32;   // the Cortex-M7's data cache line
constexpr uint32_t SECTOR_BYTES = 512;       // the card's sector, SD Physical Layer 5.3.3

bool readsInPlace(const void* buffer, uint32_t at, size_t n) {
    const uintptr_t address = reinterpret_cast<uintptr_t>(buffer);
    if (address - RAM1_START < RAM1_BYTES) {
        return true;
    }
    // The cache has to be dropped over the buffer after the read, a whole line at a time. A line
    // that holds bytes outside the buffer would lose what the processor wrote there meanwhile.
    // SdFat moves only whole sectors straight, so a read that starts inside a sector copies its
    // first bytes through the processor, which the dropped lines would lose as well.
    return address % CACHE_LINE_BYTES == 0 && n % CACHE_LINE_BYTES == 0 && at % SECTOR_BYTES == 0;
}

/// Compares two paths without regard to case, as the card's file system names its files.
///
/// @param a  The first path.
/// @param b  The second path.
static bool samePath(const char* a, const char* b) {
    for (;; ++a, ++b) {
        const char x = *a >= 'A' && *a <= 'Z' ? char(*a - 'A' + 'a') : *a;
        const char y = *b >= 'A' && *b <= 'Z' ? char(*b - 'A' + 'a') : *b;
        if (x != y) {
            return false;
        }
        if (x == '\0') {
            return true;
        }
    }
}

/// Copies a path that fits with its end.
///
/// A path that does not fit leaves an empty path.
///
/// @param to    Takes the copy.
/// @param path  The path to copy.
static void copyPath(char (&to)[FILE_PATH_BYTES], const char* path) {
    const size_t n = path != nullptr ? strlen(path) : FILE_PATH_BYTES;
    if (n >= FILE_PATH_BYTES) {
        DEBUG_LOG("storage: the path %.20s... is longer than %u bytes, its file fails\n",
                  path != nullptr ? path : "", unsigned(FILE_PATH_BYTES - 1));
        to[0] = '\0';
        return;
    }
    memcpy(to, path, n + 1);
}

FileReader StorageService::open(const char* path) {
    FileReader f;
    f.service = this;
    copyPath(f.path, path);
    return f;
}

FileWriter StorageService::openWriter(const char* path) {
    FileWriter f;
    f.service = this;
    copyPath(f.path, path);
    return f;
}

void FileReader::read(uint32_t at, void* buffer, size_t n, FileRequest& r) const {
    StorageService::start(r, *this, FileRequest::Kind::READ, at, static_cast<uint8_t*>(buffer),
                          nullptr, n);
}

void FileWriter::write(uint32_t at, const void* data, size_t n, FileRequest& r) const {
    StorageService::start(r, *this, FileRequest::Kind::WRITE, at, nullptr,
                          static_cast<const uint8_t*>(data), n);
}

void FileWriter::rewrite(const void* data, size_t n, FileRequest& r) const {
    StorageService::start(r, *this, FileRequest::Kind::REWRITE, 0, nullptr,
                          static_cast<const uint8_t*>(data), n);
}

void StorageService::start(FileRequest& r, const FileReader& file, FileRequest::Kind kind,
                           uint32_t at, uint8_t* into, const uint8_t* from, size_t size) {
    if (file.service != nullptr) {
        file.service->add(r, file.path, kind, at, into, from, size);
    } else if (r.service != nullptr) {
        // The service that has the request decides whether it is still pending.
        r.service->add(r, "", kind, at, into, from, size);
    } else {
        DEBUG_LOG("storage: a file from no storage driver, the request fails\n");
        r.got = 0;
        r.sizeBytes = 0;
        r.state.store(FileStatus::FAILED, std::memory_order_release);
    }
}

void StorageService::add(FileRequest& r, const char* path, FileRequest::Kind kind, uint32_t at,
                         uint8_t* into, const uint8_t* from, size_t size) {
    if (!callerMayStart()) {
        // Only a caller that may take the lock may touch a request on its way.
        DEBUG_LOG("storage: a request from an interrupt above FreeRTOS's limit fails\n");
        if (r.status() != FileStatus::PENDING) {
            r.got = 0;
            r.sizeBytes = 0;
            r.state.store(FileStatus::FAILED, std::memory_order_release);
        }
        return;
    }
    const uint32_t held = lock();
    if (r.status() == FileStatus::PENDING) {
        // The second operation starts nothing. The first ends as failed.
        r.dropped = true;
        unlock(held);
        DEBUG_LOG("storage: a second operation on a pending request, both fail\n");
        return;
    }
    unlock(held);
    // A request that has ended belongs to its caller, so no lock guards the filling.
    r.service = this;
    memcpy(r.path, path, strnlen(path, FILE_PATH_BYTES - 1));
    r.path[strnlen(path, FILE_PATH_BYTES - 1)] = '\0';
    r.kind = kind;
    r.at = at;
    r.into = into;
    r.from = from;
    r.size = size;
    r.next = nullptr;
    r.dropped = false;
    r.got = 0;
    r.sizeBytes = 0;
    if (r.path[0] == '\0' || failedCard()) {
        if (r.path[0] == '\0') {
            DEBUG_LOG("storage: a request on a file without a path fails\n");
        }
        r.state.store(FileStatus::FAILED, std::memory_order_release);
        return;
    }
    r.state.store(FileStatus::PENDING, std::memory_order_release);
    RequestList& list = lists[uint8_t(r.priority)];
    const uint32_t again = lock();
    if (list.tail != nullptr) {
        list.tail->next = &r;
    } else {
        list.head = &r;
    }
    list.tail = &r;
    unlock(again);
    wake();
}

bool StorageService::mount() {
    if (!card.mount()) {
        lastErrorCode.store(card.errorCode(), std::memory_order_relaxed);
        return false;
    }
    wasMounted = true;
    failures = 0;
    down.store(false, std::memory_order_relaxed);
    return true;
}

bool StorageService::serveNext() {
    FileRequest* r = nullptr;
    const uint32_t held = lock();
    for (const RequestList& list : lists) {
        if (list.head != nullptr) {
            r = list.head;
            break;
        }
    }
    unlock(held);
    if (r == nullptr) {
        return false;
    }
    if (failedCard()) {
        end(*r, false, NO_SLOT);
        return true;
    }
    const uint32_t before = card.failedTransfers();
    uint8_t slot = NO_SLOT;
    const bool worked = serve(*r, slot);
    if (card.failedTransfers() == before) {
        failures = 0;
        end(*r, worked, slot);
        return true;
    }
    // The caller learns of the failure before the card is started anew, which can take seconds.
    lastErrorCode.store(card.errorCode(), std::memory_order_relaxed);
    end(*r, false, NO_SLOT);
    failure();
    return true;
}

/// Carries out @p r.
///
/// A file that does not open reads as empty. A write fails there. A card error that made the file
/// fail to open is told apart by serveNext().
bool StorageService::serve(FileRequest& r, uint8_t& slot) {
    slot = find(r);
    if (slot == NO_SLOT) {
        DEBUG_LOG("storage: %s does not open, %s\n", r.path,
                  r.kind == FileRequest::Kind::READ ? "so it reads as empty"
                                                    : "so the write fails");
        return r.kind == FileRequest::Kind::READ;
    }
    const uint32_t fileBytes = card.size(slot);
    switch (r.kind) {
    case FileRequest::Kind::READ: {
        if (r.at >= fileBytes) {
            return true;
        }
        const size_t left = fileBytes - r.at;
        return card.read(slot, r.at, r.into, r.size < left ? r.size : left, r.got);
    }
    case FileRequest::Kind::WRITE:
        if (r.at > fileBytes) {
            DEBUG_LOG("storage: a write at %lu past the end %lu of %s fails\n",
                      static_cast<unsigned long>(r.at), static_cast<unsigned long>(fileBytes),
                      r.path);
            return false;
        }
        return card.write(slot, r.at, r.from, r.size) && card.commit(slot);
    case FileRequest::Kind::REWRITE:
        return card.write(slot, 0, r.from, r.size) && card.cut(slot, uint32_t(r.size)) &&
               card.commit(slot);
    }
    return false;
}

/// Gets the slot that holds the file of @p r, opening it where it is not open.
///
/// A file open for reading alone is opened anew for a write. A full table closes the slot used
/// longest ago.
uint8_t StorageService::find(const FileRequest& r) {
    const bool writes = r.kind != FileRequest::Kind::READ;
    uint8_t chosen = NO_SLOT;
    uint8_t oldest = 0;
    for (uint8_t i = 0; i < CardFiles::SLOTS; ++i) {
        Slot& s = slots[i];
        if (s.path[0] == '\0') {
            chosen = chosen == NO_SLOT ? i : chosen;
            continue;
        }
        if (samePath(s.path, r.path)) {
            if (s.writes || !writes) {
                return i;
            }
            card.close(i);
            s.path[0] = '\0';
            chosen = i;
            break;
        }
        if (s.lastUse < slots[oldest].lastUse || slots[oldest].path[0] == '\0') {
            oldest = i;
        }
    }
    if (chosen == NO_SLOT) {
        chosen = oldest;
        card.close(chosen);
        slots[chosen].path[0] = '\0';
    }
    if (!card.open(chosen, r.path, writes)) {
        return NO_SLOT;
    }
    memcpy(slots[chosen].path, r.path, sizeof r.path);
    slots[chosen].writes = writes;
    return chosen;
}

void StorageService::end(FileRequest& r, bool worked, uint8_t slot) {
    if (slot != NO_SLOT) {
        uses = uses + 1;
        slots[slot].lastUse = uses;
        r.sizeBytes = card.size(slot);
    } else {
        r.sizeBytes = 0;
    }
    if (!worked) {
        r.got = 0;
    }
    RequestList& list = lists[uint8_t(r.priority)];
    const uint32_t held = lock();
    list.head = r.next;
    if (list.head == nullptr) {
        list.tail = nullptr;
    }
    // The results are in place before the caller sees the end. Nothing touches r after it.
    r.state.store(worked && !r.dropped ? FileStatus::SUCCEEDED : FileStatus::FAILED,
                  std::memory_order_release);
    unlock(held);
}

void StorageService::failure() {
    failures = uint8_t(failures + 1);
    DEBUG_LOG("storage: card error %lu, %u in a row\n",
              static_cast<unsigned long>(lastErrorCode.load(std::memory_order_relaxed)),
              unsigned(failures));
    closeAll();
    if (failures >= FAILURES || !card.remount()) {
        down.store(true, std::memory_order_relaxed);
        DEBUG_LOG("storage: the card counts as failed\n");
    }
}

bool StorageService::recover() {
    if (!failedCard()) {
        return true;
    }
    if (!wasMounted) {
        return false;
    }
    closeAll();
    if (!card.remount()) {
        lastErrorCode.store(card.errorCode(), std::memory_order_relaxed);
        return false;
    }
    failures = 0;
    down.store(false, std::memory_order_relaxed);
    DEBUG_LOG("storage: the card works again\n");
    return true;
}

void StorageService::closeAll() {
    for (uint8_t i = 0; i < CardFiles::SLOTS; ++i) {
        if (slots[i].path[0] != '\0') {
            card.close(i);
            slots[i].path[0] = '\0';
        }
    }
}

bool FileRequest::ended() const {
    const FileStatus s = status();
    return s == FileStatus::SUCCEEDED || s == FileStatus::FAILED;
}

size_t FileRequest::bytes() const {
    return status() == FileStatus::SUCCEEDED ? got : 0;
}

uint32_t FileRequest::fileSize() const {
    return ended() ? sizeBytes : 0;
}

bool FileRequest::wait() const {
    if (service == nullptr || status() == FileStatus::IDLE) {
        DEBUG_LOG("storage: wait() on a request that never started\n");
        return false;
    }
    if (!service->callerMayWait()) {
        DEBUG_LOG("storage: wait() from an interrupt or from the storage task refused\n");
        return false;
    }
    while (status() == FileStatus::PENDING) {
        service->pause();
    }
    return status() == FileStatus::SUCCEEDED;
}

FileRequest::~FileRequest() {
    if (status() == FileStatus::PENDING) {
        wait();
    }
}

void CardFile::write(uint32_t at, const uint8_t* data, size_t n) {
    file.write(at, data, n, request);
}

void CardFile::rewrite(const uint8_t* data, size_t n) {
    file.rewrite(data, n, request);
}

AppendStatus CardFile::status() const {
    switch (request.status()) {
    case FileStatus::IDLE:
        return AppendStatus::IDLE;
    case FileStatus::PENDING:
        return AppendStatus::PENDING;
    case FileStatus::SUCCEEDED:
        return AppendStatus::SUCCEEDED;
    case FileStatus::FAILED:
        return AppendStatus::FAILED;
    }
    return AppendStatus::FAILED;
}
