#pragma once
#include <stddef.h>
#include <stdint.h>

// A list of at most N entries, ordered by priority with the highest first. Between equal
// priorities the later insert lies higher. The entries stay where insert() put them, so a pointer
// to one stays valid until it is removed; only the order changes. No heap.
//
// T needs a priorityOf(const T&) that returns its priority, found by argument-dependent lookup:
// a friend inside T, or a free function beside it.
template <typename T, size_t N>
class PriorityList {
    static_assert(N <= 255, "the order is kept in bytes");

public:
    // puts a copy of item at its priority, above the entries of equal priority; nullptr when the
    // list already holds N entries
    T* insert(const T& item) {
        if (count_ == N) return nullptr;
        uint8_t slot = 0;
        while (used_[slot]) ++slot;
        items_[slot] = item;
        used_[slot]  = true;
        const auto prio = priorityOf(items_[slot]);
        size_t at = 0;
        while (at < count_ && priorityOf(items_[order_[at]]) > prio) ++at;
        for (size_t k = count_; k > at; --k) order_[k] = order_[k - 1];
        order_[at] = slot;
        ++count_;
        return &items_[slot];
    }

    // removes every entry pred holds for, and keeps the order of the others
    template <typename Pred>
    void removeIf(Pred pred) {
        size_t kept = 0;
        for (size_t k = 0; k < count_; ++k) {
            const uint8_t slot = order_[k];
            if (pred(items_[slot])) used_[slot] = false;
            else order_[kept++] = slot;
        }
        count_ = kept;
    }

    // gets the highest entry pred holds for; nullptr for none
    template <typename Pred>
    T* find(Pred pred) {
        for (size_t k = 0; k < count_; ++k) if (pred(items_[order_[k]])) return &items_[order_[k]];
        return nullptr;
    }

    void   clear()       { for (bool& u : used_) u = false; count_ = 0; }
    size_t size() const  { return count_; }
    bool   full() const  { return count_ == N; }

    // gets the entry at place i, 0 the highest and size() - 1 the lowest
    T&       operator[](size_t i)       { return items_[order_[i]]; }
    const T& operator[](size_t i) const { return items_[order_[i]]; }

private:
    T       items_[N] = {};
    bool    used_[N]  = {};
    uint8_t order_[N] = {};   // the slots of the entries, highest priority first
    size_t  count_    = 0;
};
