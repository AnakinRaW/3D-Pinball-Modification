// A queue of bytes between one writer and one reader that never makes either of them wait or take
// a lock. A record goes in whole or not at all. The reader takes the bytes in the order they came,
// in as few pieces as the end of the buffer allows. The caller hands in the buffer, so the ring
// allocates nothing and can lie in any memory.
#pragma once
#include <atomic>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// One writer and one reader may use the ring at the same time from different tasks. Several
// writers take turns under a lock. Neither side ever waits.
class ByteRing {
public:
    // Creates a ring in buffer.
    // buffer holds the bytes and stays with the ring for its whole life.
    // bytes is the buffer's size. The ring uses the largest power of two that fits, so a buffer of
    // 1000 bytes holds 512. A ring of 0 bytes refuses every record.
    ByteRing(uint8_t* buffer, size_t bytes)
        : storage(buffer), capacity(buffer ? floorPow2(bytes) : 0) {}

    // Adds a record behind the bytes already in the ring. Never waits.
    // Returns false when the record does not fit into the free space. Nothing is added then.
    bool append(const void* data, size_t n) {
        const size_t head = written.load(std::memory_order_relaxed);
        if (capacity == 0 || n > capacity - (head - read.load(std::memory_order_acquire))) {
            return false;
        }
        const size_t at = head & (capacity - 1);
        const size_t first = n < capacity - at ? n : capacity - at;
        memcpy(storage + at, data, first);
        memcpy(storage, static_cast<const uint8_t*>(data) + first, n - first);
        // The bytes are in place before the reader sees them.
        written.store(head + n, std::memory_order_release);
        return true;
    }

    // Gets the oldest bytes that lie in one piece of the buffer, without taking them.
    // n is set to their count, 0 when the ring is empty. Where the bytes run past the end of the
    // buffer, only those up to the end come. The next call gets the rest.
    const uint8_t* peek(size_t& n) const {
        const size_t tail = read.load(std::memory_order_relaxed);
        const size_t used = written.load(std::memory_order_acquire) - tail;
        if (used == 0) {
            n = 0;
            return storage;
        }
        const size_t at = tail & (capacity - 1);
        n = used < capacity - at ? used : capacity - at;
        return storage + at;
    }

    // Frees the n oldest bytes once the reader is done with them.
    // n is at most what peek() last gave. The writer may overwrite them from then on.
    void consume(size_t n) {
        read.store(read.load(std::memory_order_relaxed) + n, std::memory_order_release);
    }

    // Gets how many bytes the ring holds.
    size_t used() const {
        return written.load(std::memory_order_acquire) - read.load(std::memory_order_acquire);
    }

    // Gets the ring's size in bytes.
    size_t size() const { return capacity; }

    // Gets how many bytes were added since the ring was created. The count wraps at the range
    // of size_t.
    size_t appended() const { return written.load(std::memory_order_acquire); }

    // Gets how many bytes were consumed since the ring was created. The count wraps at the range
    // of size_t.
    size_t consumed() const { return read.load(std::memory_order_acquire); }

private:
    // Gets the largest power of two not above n, 0 for 0. A power of two keeps the place of a byte
    // right when the counts below wrap.
    static size_t floorPow2(size_t n) {
        size_t p = 1;
        while (n != 0 && p <= n / 2) {
            p = p * 2;
        }
        return n == 0 ? 0 : p;
    }

    uint8_t* const storage;
    const size_t capacity;
    // The bytes added so far. Only the writer moves this count.
    std::atomic<size_t> written{0};
    // The bytes consumed so far. Only the reader moves this count.
    std::atomic<size_t> read{0};
};
