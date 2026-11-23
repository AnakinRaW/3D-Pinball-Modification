#include "card-upload.h"
#include "core/clock.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/// CRC-32 of IEEE 802.3 with its bits reversed, the checksum Python's zlib.crc32() computes.
constexpr uint32_t CRC_POLYNOMIAL = 0xEDB88320;
constexpr uint32_t CRC_START = 0xFFFFFFFF;
constexpr uint8_t BITS = 8;
constexpr size_t LINE_STEP_BYTES = 64;   // the bytes of a command one step takes at most
constexpr int RADIX_HEX = 16;
constexpr int RADIX_DECIMAL = 10;
constexpr char PREFIX[] = "CARD ";
constexpr char PUT[] = "PUT ";
constexpr char DATA[] = "DATA ";
constexpr char RESTART[] = "RESTART";

/// Gets the CRC-32 of a run of bytes.
static uint32_t crc32(const uint8_t* data, size_t n) {
    uint32_t crc = CRC_START;
    for (size_t i = 0; i < n; ++i) {
        crc ^= data[i];
        for (uint8_t b = 0; b < BITS; ++b) {
            crc = (crc >> 1) ^ (CRC_POLYNOMIAL & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

/// Reads a number that fills a text up to its end or a space.
///
/// @param text  Where the number starts.
/// @param base  10 or 16.
/// @param end   Set to the character after the number.
/// @return      False where no digit stands at the start or the number exceeds 32 bits.
static bool readNumber(const char* text, int base, uint32_t& value, const char*& end) {
    char* after = nullptr;
    const unsigned long v = strtoul(text, &after, base);
    if (after == text || (*after != '\0' && *after != ' ') || v > UINT32_MAX || text[0] == '-') {
        return false;
    }
    value = uint32_t(v);
    end = after;
    return true;
}

void CardUpload::update() {
    if (restarting) {
        // The wait lets the answer leave over USB before the restart cuts it off.
        if (elapsedMs(restartUs, RESTART_MS)) {
            restart();
        }
        return;
    }
    if (state == State::WRITING) {
        if (!writing.ended()) {
            return;
        }
        if (writing.status() != FileStatus::SUCCEEDED) {
            fail("the card did not take the chunk");
            return;
        }
        done += uint32_t(chunkBytes);
        heardUs = nowUs();
        state = State::LINE;
        if (done == size) {
            inFile = false;
            reply("CARD DONE %lu\n", static_cast<unsigned long>(done));
        } else {
            reply("CARD OK %lu\n", static_cast<unsigned long>(done));
        }
        return;
    }
    if (state == State::DATA) {
        const size_t got = link.read(chunk + chunkGot, chunkBytes - chunkGot);
        if (got > 0) {
            chunkGot += got;
            heardUs = nowUs();
        }
        if (chunkGot < chunkBytes) {
            if (elapsedMs(heardUs, QUIET_MS)) {
                fail("no byte for too long");
            }
            return;
        }
        state = State::LINE;
        if (crc32(chunk, chunkBytes) != chunkCrc) {
            reply("CARD FAIL checksum\n");
            return;
        }
        // The first chunk replaces what the file held, so a shorter file leaves nothing behind.
        if (done == 0) {
            file.rewrite(chunk, chunkBytes, writing);
        } else {
            file.write(done, chunk, chunkBytes, writing);
        }
        state = State::WRITING;
        return;
    }
    // A command is read one byte at a time, so the bytes of a chunk behind it stay for DATA.
    for (size_t i = 0; i < LINE_STEP_BYTES; ++i) {
        uint8_t c = 0;
        if (link.read(&c, 1) == 0) {
            break;
        }
        heardUs = nowUs();
        if (c == '\n') {
            line[lineLength] = '\0';
            const bool whole = !lineTooLong;
            lineLength = 0;
            lineTooLong = false;
            if (whole && strncmp(line, PREFIX, sizeof PREFIX - 1) == 0) {
                command(line + sizeof PREFIX - 1);
            }
            return;
        }
        if (c == '\r') {
            continue;
        }
        if (lineLength + 1 < LINE_BYTES) {
            line[lineLength++] = char(c);
        } else {
            lineTooLong = true;
        }
    }
    if (inFile && elapsedMs(heardUs, QUIET_MS)) {
        fail("no byte for too long");
    }
}

void CardUpload::command(const char* text) {
    if (strncmp(text, PUT, sizeof PUT - 1) == 0) {
        const char* path = text + sizeof PUT - 1;
        const char* space = strrchr(path, ' ');
        uint32_t bytes = 0;
        const char* end = nullptr;
        const size_t length = space != nullptr ? size_t(space - path) : 0;
        if (space == nullptr || !readNumber(space + 1, RADIX_DECIMAL, bytes, end) || *end != '\0') {
            fail("PUT takes a path and a size");
            return;
        }
        char full[FILE_PATH_BYTES];
        if (length < 2 || length >= sizeof full || path[0] != '/') {
            fail("the path is no full path on the card");
            return;
        }
        memcpy(full, path, length);
        full[length] = '\0';
        if (strstr(full, "..") != nullptr || strchr(full, ' ') != nullptr) {
            fail("the path is no full path on the card");
            return;
        }
        file = files.openFull(full);
        inFile = true;
        size = bytes;
        done = 0;
        heardUs = nowUs();
        if (size == 0) {
            // An empty file needs no chunk. The rewrite of nothing empties it.
            chunkBytes = 0;
            file.rewrite(chunk, 0, writing);
            state = State::WRITING;
            return;
        }
        reply("CARD READY\n");
        return;
    }
    if (strncmp(text, DATA, sizeof DATA - 1) == 0) {
        uint32_t bytes = 0;
        uint32_t crc = 0;
        const char* end = nullptr;
        if (!inFile) {
            reply("CARD FAIL no file is being received\n");
            return;
        }
        if (!readNumber(text + sizeof DATA - 1, RADIX_DECIMAL, bytes, end) || *end != ' ' ||
            !readNumber(end + 1, RADIX_HEX, crc, end) || *end != '\0') {
            fail("DATA takes a size and a checksum");
            return;
        }
        if (bytes == 0 || bytes > CHUNK_BYTES || bytes > size - done) {
            fail("the chunk is empty, too large or runs past the file");
            return;
        }
        chunkBytes = bytes;
        chunkGot = 0;
        chunkCrc = crc;
        state = State::DATA;
        return;
    }
    if (strcmp(text, RESTART) == 0) {
        if (inFile) {
            fail("the restart ends the file");
        }
        reply("CARD RESTARTING\n");
        restarting = true;
        restartUs = nowUs();
        return;
    }
    reply("CARD FAIL unknown command\n");
}

void CardUpload::fail(const char* reason) {
    reply("CARD FAIL %s\n", reason);
    inFile = false;
    size = 0;
    done = 0;
    state = State::LINE;
}

void CardUpload::reply(const char* format, ...) {
    char text[LINE_BYTES];
    va_list args;
    va_start(args, format);
    const int n = vsnprintf(text, sizeof text, format, args);
    va_end(args);
    if (n > 0) {
        link.answer(text, size_t(n) < sizeof text ? size_t(n) : sizeof text - 1);
    }
}
