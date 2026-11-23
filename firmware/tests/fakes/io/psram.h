// The free PSRAM of firmware/src/io/psram.h for the tests that run on the computer: a block in
// the computer's memory, of the size a test sets before the screen's begin().
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace io {

/// The size of the block takeFreePsram() hands out, 0 for a Teensy without PSRAM.
inline size_t fakePsramBytes = 7 * 1024 * 1024;

inline uint8_t* takeFreePsram(size_t& bytes) {
    static std::vector<uint8_t> block;
    bytes = 0;
    if (!block.empty() || fakePsramBytes == 0) {
        return nullptr;
    }
    block.resize(fakePsramBytes);
    bytes = block.size();
    return block.data();
}

}   // namespace io
