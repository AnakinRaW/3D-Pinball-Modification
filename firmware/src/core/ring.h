// A queue of elements of type T between one writer and one reader that never makes either of them
// wait or take a lock. A record of one or more elements goes in whole or not at all. The reader
// takes the elements in the order they came, in as few pieces as the end of the buffer allows.
// The caller hands in the buffer, so the ring allocates nothing and can lie in any memory.
#pragma once
#include <atomic>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <type_traits>

/// One writer and one reader may use the ring at the same time from different tasks or
/// interrupts. Several writers take turns under a lock. Neither side ever waits.
/// T is copied as its bytes, so it has to be trivially copyable.
template <typename T> class Ring {
    static_assert(std::is_trivially_copyable_v<T>, "the ring copies an element as its bytes");

public:
    /// Creates a ring without a buffer, which refuses every record until begin() gives it one.
    Ring() = default;

    /// Creates a ring in @p buffer, as begin() sets it up.
    ///
    /// @param buffer  Holds the elements.
    /// @param n       The buffer's size in elements.
    Ring(T* buffer, size_t n) { begin(buffer, n); }

    /// Gives the ring a buffer and empties it.
    ///
    /// The ring uses the largest power of two that fits, so a buffer of 1000 elements holds 512.
    /// A ring of 0 elements or without a buffer refuses every record.
    ///
    /// @param buffer  Holds the elements and stays with the ring until the next begin().
    /// @param n       The buffer's size in elements.
    /// @note          Before the writer and the reader use the ring.
    void begin(T* buffer, size_t n) {
        storage = buffer;
        capacity = buffer != nullptr ? floorPow2(n) : 0;
        written.store(0, std::memory_order_relaxed);
        read.store(0, std::memory_order_relaxed);
    }

    /// Adds a record of @p n elements behind the elements already in the ring.
    ///
    /// @param data  The elements of the record.
    /// @param n     The number of elements in the record.
    /// @return      False when the record does not fit into the free space. Nothing is added
    ///              then.
    /// @note        Never waits.
    bool append(const T* data, size_t n) {
        const size_t head = written.load(std::memory_order_relaxed);
        if (capacity == 0 || n > capacity - (head - read.load(std::memory_order_acquire))) {
            return false;
        }
        const size_t at = head & (capacity - 1);
        const size_t first = n < capacity - at ? n : capacity - at;
        memcpy(storage + at, data, first * sizeof(T));
        memcpy(storage, data + first, (n - first) * sizeof(T));
        // The elements are in place before the reader sees them.
        written.store(head + n, std::memory_order_release);
        return true;
    }

    /// Gets the oldest elements that lie in one piece of the buffer, without taking them.
    ///
    /// Where the elements run past the end of the buffer, only those up to the end come. The next
    /// call gets the rest.
    ///
    /// @param n  Set to their count, 0 when the ring is empty.
    const T* peek(size_t& n) const {
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

    /// Frees the @p n oldest elements once the reader is done with them.
    ///
    /// The writer may overwrite them from then on.
    ///
    /// @param n  At most what peek() last gave.
    void consume(size_t n) {
        read.store(read.load(std::memory_order_relaxed) + n, std::memory_order_release);
    }

    /// Gets how many elements the ring holds.
    size_t used() const {
        return written.load(std::memory_order_acquire) - read.load(std::memory_order_acquire);
    }

    /// Gets how many elements the ring holds at most.
    size_t size() const { return capacity; }

    /// Gets how many elements were added since begin(). The count wraps at the range of size_t.
    size_t appended() const { return written.load(std::memory_order_acquire); }

    /// Gets how many elements were consumed since begin(). The count wraps at the range of size_t.
    size_t consumed() const { return read.load(std::memory_order_acquire); }

    Ring(const Ring&) = delete;
    Ring& operator=(const Ring&) = delete;

private:
    /// Gets the largest power of two not above @p n, 0 for 0.
    ///
    /// A power of two keeps the place of an element right when the counts below wrap.
    ///
    /// @param n  The number to round down.
    static size_t floorPow2(size_t n) {
        size_t p = 1;
        while (n != 0 && p <= n / 2) {
            p = p * 2;
        }
        return n == 0 ? 0 : p;
    }

    T* storage = nullptr;
    size_t capacity = 0;
    /// The elements added so far. Only the writer moves this count.
    std::atomic<size_t> written{0};
    /// The elements consumed so far. Only the reader moves this count.
    std::atomic<size_t> read{0};
};
