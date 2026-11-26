#include "card-appender.h"
#include "core/clock.h"
#include "logging/debug-log.h"

void CardAppender::begin(AppendFile& f, uint32_t sizeBytes) {
    file = &f;
    position = sizeBytes;
    fresh = false;
    lastTryUs = nowUs();
    nextFileBytes = sizeBytes;
    hasFile = true;
}

bool CardAppender::start(AppendFile& f) {
    if (nextFile.load(std::memory_order_acquire) != nullptr) {
        DEBUG_LOG("CardAppender::start: the switch before still waits for its records\n");
        return false;
    }
    nextAt.store(ring.appended(), std::memory_order_relaxed);
    // nextAt is in place before the reader sees the new file.
    nextFile.store(&f, std::memory_order_release);
    hasFile = true;
    nextFileBytes = 0;
    return true;
}

bool CardAppender::append(const void* data, size_t n) {
    if (!hasFile) {
        DEBUG_LOG("CardAppender::append: no file yet\n");
    } else if (ring.append(static_cast<const uint8_t*>(data), n)) {
        nextFileBytes = nextFileBytes + static_cast<uint32_t>(n);
        return true;
    } else {
        DEBUG_LOG("CardAppender::append: %u bytes find the buffer full\n",
                  static_cast<unsigned>(n));
    }
    lostSince = lostSince + 1;
    lostTotal = lostTotal + 1;
    return false;
}

uint32_t CardAppender::takeLost() {
    const uint32_t n = lostSince;
    lostSince = 0;
    return n;
}

void CardAppender::update() {
    if (inFlight != 0) {
        const AppendStatus s = file->status();
        if (s == AppendStatus::PENDING) {
            return;
        }
        const bool failed = s != AppendStatus::SUCCEEDED;
        if (failed && !lastFailed) {
            DEBUG_LOG("CardAppender: a write of %u bytes failed, it is tried again in a second\n",
                      static_cast<unsigned>(inFlight));
        } else if (!failed && lastFailed) {
            DEBUG_LOG("CardAppender: the card takes the writes again\n");
        }
        lastFailed = failed;
        if (!lastFailed) {
            ring.consume(inFlight);
            position = position + static_cast<uint32_t>(inFlight);
            fresh = false;
        }
        // A failed write keeps its records in the ring. The next write tries them again at
        // the same place.
        inFlight = 0;
    }
    AppendFile* const turn = nextFile.load(std::memory_order_acquire);
    if (turn != nullptr && ring.consumed() == nextAt.load(std::memory_order_relaxed)) {
        // Every record of the old file is on the card.
        file = turn;
        position = 0;
        fresh = true;
        lastFailed = false;
        nextFile.store(nullptr, std::memory_order_release);
    }
    if (file != nullptr) {
        startWrite();
    }
}

void CardAppender::startWrite() {
    size_t n = 0;
    const uint8_t* bytes = ring.peek(n);
    size_t due = ring.used();
    const AppendFile* const turn = nextFile.load(std::memory_order_acquire);
    if (turn != nullptr) {
        // The records of the new file wait until the old file has all of its records.
        const size_t left = nextAt.load(std::memory_order_relaxed) - ring.consumed();
        n = n < left ? n : left;
        due = due < left ? due : left;
    }
    if (n == 0) {
        return;
    }
    const bool urgent =
        restOwed || flushWanted.load(std::memory_order_relaxed) || ring.used() >= ring.size() / 2;
    // After a failure the card gets a second's rest, however full the buffer is.
    if (!(urgent && !lastFailed) && !elapsedMs(lastTryUs, WRITE_PERIOD_MS)) {
        return;
    }
    flushWanted.store(false, std::memory_order_relaxed);
    // Records that run past the end of the buffer take two writes, and the second follows the
    // first at once.
    restOwed = n < due;
    lastTryUs = nowUs();
    inFlight = n;
    if (fresh) {
        file->rewrite(bytes, n);
    } else {
        file->write(position, bytes, n);
    }
}
