// Reads files from the SD card one after another for a driver that takes their bytes in its
// interrupt. The channel reads ahead into a ring of chunks, so the driver finds the next bytes in
// memory and never waits for the card. A file can start at any byte and can start over at its end.
//
// Design: docs/firmware/file-channel.md.
#pragma once
#include "drivers/storage-service.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/// Reads files one after another from the card into a ring of chunks, for the one place that takes
/// the bytes.
///
/// The card fills the chunks in turn, and take() empties them in the same order. A chunk is free
/// again once take() has taken all its bytes.
///
/// @note  Every call comes from the one place that takes the bytes, a task or an interrupt at
///        priority 208 or a lower priority, and returns at once.
template <uint8_t CHUNKS> class FileChannel {
    static_assert(CHUNKS > 0, "a ring holds at least one chunk");

public:
    /// Creates a channel on a ring of CHUNKS chunks.
    ///
    /// @param ring      The ring, CHUNKS times @p chunk bytes. A ring in RAM2 or the PSRAM takes
    ///                  the card's bytes without a copy when it starts on 32 bytes and @p chunk is
    ///                  a multiple of 32.
    /// @param chunk     The bytes of one chunk, which one read of the card brings.
    /// @param priority  The priority of the channel's reads among the card's requests.
    FileChannel(uint8_t* ring, uint32_t chunk, FilePriority priority = FilePriority::NORMAL)
        : request(priority), ring(ring), chunk(chunk) {}

    /// Reads a file from a byte on, and starts it over at its end when @p loop is set.
    ///
    /// The bytes of the file read before are gone at once. A read of that file still on its way
    /// is dropped when it ends.
    ///
    /// @param file  The file. One with no name reads nothing, and the channel counts as ended.
    /// @param loop  Starts the file over from its first byte at its end.
    /// @param at    The byte the first read starts at. A read from a sector of 512 bytes takes the
    ///              card's bytes without a copy.
    void start(const FileReader& file, bool loop, uint32_t at = 0) {
        reader = file;
        looping = loop;
        stale = reading;
        failedRead = false;
        done = reader.name()[0] == '\0';
        next = at;
        front = 0;
        back = 0;
        filled = 0;
        taken = 0;
        pump();
    }

    /// Ends the file being read. Its bytes are gone at once.
    void stop() { start(FileReader{}, false); }

    /// Copies bytes from the front of the ring, across chunks.
    ///
    /// Starts the next read where a chunk is free and no read is on its way.
    ///
    /// @param to  Takes the bytes. nullptr skips them.
    /// @param n   The most bytes to take.
    /// @return    How many bytes it took. Fewer than @p n while the card has not brought more.
    size_t take(void* to, size_t n) {
        pump();
        size_t got = 0;
        while (got < n && filled > 0) {
            const size_t left = bytes[front] - taken;
            const size_t m = left < n - got ? left : n - got;
            if (to != nullptr) {
                memcpy(static_cast<uint8_t*>(to) + got, ring + front * chunk + taken, m);
            }
            taken = taken + m;
            got = got + m;
            if (taken == bytes[front]) {
                taken = 0;
                front = uint8_t((front + 1) % CHUNKS);
                filled = uint8_t(filled - 1);
                pump();
            }
        }
        return got;
    }

    /// Gets how many bytes take() could take now.
    ///
    /// Starts the next read where a chunk is free and no read is on its way.
    size_t available() {
        pump();
        size_t n = 0;
        for (uint8_t i = 0, c = front; i < filled; ++i, c = uint8_t((c + 1) % CHUNKS)) {
            n = n + bytes[c];
        }
        return n - taken;
    }

    /// Gets whether nothing is left to take: the file has run out or a read has failed, and every
    /// byte before is taken.
    ///
    /// It is true as well for a missing file, for a channel never started and after stop().
    bool ended() const { return (failedRead || done) && filled == 0; }

private:
    /// Takes a read that has ended into its chunk, and starts the next one while a chunk is free.
    ///
    /// A read that brings fewer bytes than a chunk has reached the end of the file.
    void pump() {
        if (reading) {
            const FileStatus s = request.status();
            if (s == FileStatus::PENDING) {
                return;
            }
            reading = false;
            if (stale) {
                // The read belonged to the file before. Its chunk counts as free.
                stale = false;
            } else if (s == FileStatus::FAILED) {
                failedRead = true;
                return;
            } else {
                const uint32_t got = uint32_t(request.bytes());
                if (got > 0) {
                    bytes[back] = got;
                    back = uint8_t((back + 1) % CHUNKS);
                    filled = uint8_t(filled + 1);
                }
                next = next + got;
                const uint32_t size = request.fileSize();
                if (next >= size) {
                    if (looping && size > 0) {
                        next = 0;
                    } else {
                        done = true;
                    }
                }
            }
        }
        if (done || failedRead || filled == CHUNKS) {
            return;
        }
        reading = true;
        reader.read(next, ring + back * chunk, chunk, request);
    }

    FileRequest request;   // the one read on its way
    uint8_t* const ring;
    const uint32_t chunk;
    FileReader reader;   // the file it reads, one with no name for none
    bool looping = false;
    bool reading = false;      // request runs into the chunk at back
    bool stale = false;        // request belongs to the file read before, so its bytes are dropped
    bool failedRead = false;   // a read of the file has failed
    bool done = true;          // no read is left, since the file has run out or none is read
    uint32_t next = 0;         // the byte of the file the next read starts at
    uint8_t front = 0;         // the chunk take() takes from
    uint8_t back = 0;          // the chunk the next read fills
    uint8_t filled = 0;        // the chunks that hold bytes
    size_t taken = 0;          // the bytes of the front chunk take() has taken
    uint32_t bytes[CHUNKS] = {};   // the bytes each chunk holds
};
