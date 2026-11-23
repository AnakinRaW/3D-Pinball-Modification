// Takes all PSRAM still free at the start with one extmem_malloc(). The screen keeps its pictures
// and the video's frames in that block.
//
// Design: docs/firmware/components/screen.md, Pictures.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace io {

/// Takes the free PSRAM as one block.
///
/// @param bytes  Set to the block's size, 0 when there is no block.
/// @return       The block's first byte, nullptr when Teensyduino found no PSRAM at start-up and
///               on every call after the first.
/// @note         The main task calls it once at the start.
uint8_t* takeFreePsram(size_t& bytes);

}   // namespace io
