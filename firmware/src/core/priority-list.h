// A list of at most N entries, ordered by priority with the highest first. Between equal priorities
// the entry put in later lies higher. An entry stays in the slot insert() put it in until it is
// removed, so a pointer to it stays valid, and only the order changes. The list takes no memory
// from the heap.
//
// T needs a priorityOf(const T&) that gets its priority, found by argument-dependent lookup: a
// friend inside T or a free function beside it.
//
// Design: docs/firmware/general-design.md, Priority lists.
#pragma once
#include <stddef.h>
#include <stdint.h>

template <typename T, size_t N> class PriorityList {
    static_assert(N <= 255, "the order is kept in bytes");

public:
    /// Puts a copy of @p item at its priority, above the entries of equal priority.
    ///
    /// @param item  The entry to copy.
    /// @return      nullptr when the list already holds N entries.
    T* insert(const T& item) {
        if (count == N) {
            return nullptr;
        }
        uint8_t slot = 0;
        while (used[slot]) {
            ++slot;
        }
        items[slot] = item;
        used[slot] = true;
        const auto priority = priorityOf(items[slot]);
        size_t at = 0;
        while (at < count && priorityOf(items[order[at]]) > priority) {
            ++at;
        }
        for (size_t k = count; k > at; --k) {
            order[k] = order[k - 1];
        }
        order[at] = slot;
        ++count;
        return &items[slot];
    }

    /// Removes every entry @p pred holds for and keeps the order of the others.
    ///
    /// @param pred  Holds for the entries to remove.
    template <typename Pred> void removeIf(Pred pred) {
        size_t kept = 0;
        for (size_t k = 0; k < count; ++k) {
            const uint8_t slot = order[k];
            if (pred(items[slot])) {
                used[slot] = false;
            } else {
                order[kept++] = slot;
            }
        }
        count = kept;
    }

    /// Gets the highest entry @p pred holds for, nullptr for none.
    ///
    /// @param pred  Holds for the entries that count.
    template <typename Pred> T* find(Pred pred) {
        for (size_t k = 0; k < count; ++k) {
            if (pred(items[order[k]])) {
                return &items[order[k]];
            }
        }
        return nullptr;
    }

    void clear() {
        for (bool& u : used) {
            u = false;
        }
        count = 0;
    }

    size_t size() const { return count; }
    bool full() const { return count == N; }

    /// Gets the entry at place @p i, 0 the highest and size() - 1 the lowest.
    ///
    /// @param i  The place of the entry.
    T& operator[](size_t i) { return items[order[i]]; }
    const T& operator[](size_t i) const { return items[order[i]]; }

private:
    T items[N] = {};
    bool used[N] = {};
    uint8_t order[N] = {};   // the slots of the entries, highest priority first
    size_t count = 0;
};
