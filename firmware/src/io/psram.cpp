#include "io/psram.h"
#include <Arduino.h>

constexpr size_t BYTES_PER_MB = 1024 * 1024;
/// The bytes given up per try below the free size, so the allocator's bookkeeping in front of the
/// block fits; a cache line of the Cortex-M7
constexpr size_t STEP_BYTES = 32;
/// The tries before the PSRAM counts as full, 2 KB below the free size at the last
constexpr size_t TRIES = 64;

// startup.c of the Teensy core: the PSRAM's size in MB, 0 without a chip
extern "C" uint8_t external_psram_size;
// the linker script's EXTMEM section at the beginning of the PSRAM; the PSRAM's allocator takes
// what lies behind it
extern "C" uint8_t _extram_start;
extern "C" uint8_t _extram_end;

namespace io {

uint8_t* takeFreePsram(size_t& bytes) {
    static bool taken = false;
    bytes = 0;
    if (taken || external_psram_size == 0) {
        return nullptr;
    }
    taken = true;
    const size_t used = size_t(&_extram_end - &_extram_start);
    const size_t freeBytes = size_t(external_psram_size) * BYTES_PER_MB - used;
    // The allocator keeps a header in front of every block whose size Teensyduino does not
    // publish, so the request steps down from the free size until it fits.
    for (size_t i = 1; i <= TRIES && i * STEP_BYTES < freeBytes; ++i) {
        const size_t size = freeBytes - i * STEP_BYTES;
        void* block = extmem_malloc(size);
        if (block != nullptr) {
            bytes = size;
            return static_cast<uint8_t*>(block);
        }
    }
    return nullptr;
}

}   // namespace io
