#include "picture-memory.h"

static uint32_t roundUp(size_t bytes) {
    const size_t rounded = (bytes + PictureMemory::ALIGN_BYTES - 1) / PictureMemory::ALIGN_BYTES *
                           PictureMemory::ALIGN_BYTES;
    return uint32_t(rounded);
}

void PictureMemory::begin(uint8_t* memory, size_t bytes) {
    table.count = 0;
    base = nullptr;
    total = 0;
    if (memory == nullptr) {
        return;
    }
    // The block starts wherever the allocator put it, so its first room moves up to 32 bytes.
    const uintptr_t at = reinterpret_cast<uintptr_t>(memory);
    const uintptr_t aligned = (at + ALIGN_BYTES - 1) / ALIGN_BYTES * ALIGN_BYTES;
    const size_t lost = size_t(aligned - at);
    if (bytes <= lost) {
        return;
    }
    base = reinterpret_cast<uint8_t*>(aligned);
    const size_t usable = (bytes - lost) / ALIGN_BYTES * ALIGN_BYTES;
    total = uint32_t(usable);
}

int64_t PictureMemory::Table::place(uint32_t size, uint32_t total, bool large) const {
    if (count == ROOMS) {
        return -1;
    }
    // Gap i lies between room i - 1 and room i, gap 0 at the bottom end and gap count at the top.
    for (size_t k = 0; k <= count; ++k) {
        const size_t gap = large ? count - k : k;
        const uint32_t start = gap == 0 ? 0 : rooms[gap - 1].at + rooms[gap - 1].size;
        const uint32_t end = gap == count ? total : rooms[gap].at;
        if (end - start >= size) {
            return large ? int64_t(end - size) : int64_t(start);
        }
    }
    return -1;
}

void PictureMemory::Table::insert(Room room) {
    size_t i = count;
    while (i > 0 && rooms[i - 1].at > room.at) {
        rooms[i] = rooms[i - 1];
        --i;
    }
    rooms[i] = room;
    ++count;
}

bool PictureMemory::Table::remove(uint32_t at) {
    for (size_t i = 0; i < count; ++i) {
        if (rooms[i].at == at) {
            for (size_t k = i + 1; k < count; ++k) {
                rooms[k - 1] = rooms[k];
            }
            --count;
            return true;
        }
    }
    return false;
}

uint8_t* PictureMemory::take(size_t bytes, bool large) {
    if (base == nullptr || bytes == 0 || bytes > total) {
        return nullptr;
    }
    const uint32_t size = roundUp(bytes);
    const int64_t at = table.place(size, total, large);
    if (at < 0) {
        return nullptr;
    }
    table.insert({uint32_t(at), size});
    return base + at;
}

void PictureMemory::give(const uint8_t* room) {
    if (base == nullptr || room < base || room >= base + total) {
        return;
    }
    table.remove(uint32_t(room - base));
}

bool PictureMemory::fits(size_t count, size_t bytes, uint8_t* const* freed, size_t nFreed) const {
    if (base == nullptr || bytes == 0 || bytes > total) {
        return false;
    }
    // A copy of the table takes the rooms, so the check changes nothing.
    Table copy = table;
    for (size_t i = 0; i < nFreed; ++i) {
        if (freed[i] != nullptr && freed[i] >= base && freed[i] < base + total) {
            copy.remove(uint32_t(freed[i] - base));
        }
    }
    const uint32_t size = roundUp(bytes);
    for (size_t i = 0; i < count; ++i) {
        const int64_t at = copy.place(size, total, true);
        if (at < 0) {
            return false;
        }
        copy.insert({uint32_t(at), size});
    }
    return true;
}
