// Takes files from a computer over USB and writes them onto the SD card, so the machine config and
// the games change without taking the card out of the Teensy. The computer sends a file in chunks,
// each with a checksum, and waits for each chunk to be on the card before it sends the next. The
// main loop never waits for the computer or the card.
//
// firmware/tools/card-upload.py is the computer's side.
//
// Design: docs/firmware/components/card-upload.md.
#pragma once
#include "components/file-system.h"
#include <stddef.h>
#include <stdint.h>

/// The bytes that arrive from the computer and the lines that answer it.
class UploadLink {
public:
    /// Takes the bytes that have arrived, up to a count.
    ///
    /// @param to  Takes the bytes.
    /// @param n   The most bytes it takes.
    /// @return    How many bytes it took, 0 where none have arrived.
    /// @note      Never waits.
    virtual size_t read(uint8_t* to, size_t n) = 0;

    /// Sends one line to the computer.
    ///
    /// @param line  The line with its line break.
    /// @param n     The length of the line.
    /// @note        Never waits.
    virtual void answer(const char* line, size_t n) = 0;

protected:
    ~UploadLink() = default;
};

class CardUpload {
public:
    static constexpr size_t CHUNK_BYTES = 4096;   // card-upload.md: the largest chunk
    static constexpr uint32_t QUIET_MS = 2000;    // card-upload.md: the longest silence in a file
    static constexpr uint32_t RESTART_MS = 100;   // card-upload.md: the wait before a restart
    static constexpr size_t LINE_BYTES = 96;      // the longest command, with its end

    /// Creates the upload on a link to the computer and the card's files.
    ///
    /// @param link     Brings the computer's bytes and takes the answers.
    /// @param files    Gives the files on the card.
    /// @param restart  Restarts the Teensy, so the machine starts with the new files.
    CardUpload(UploadLink& link, FileSystem& files, void (*restart)())
        : link(link), files(files), restart(restart) {}

    CardUpload(const CardUpload&) = delete;
    CardUpload& operator=(const CardUpload&) = delete;

    /// Takes what the computer has sent and carries the upload on by one step.
    ///
    /// A command is a line. "CARD PUT <path> <bytes>" starts a file at its full path, which
    /// "CARD READY" answers. "CARD DATA <bytes> <crc32>" is followed by that many bytes of the
    /// file and answered by "CARD OK <bytes so far>" once they are on the card, or by
    /// "CARD DONE <bytes>" for the last chunk. "CARD RESTART" restarts the Teensy RESTART_MS after
    /// "CARD RESTARTING". A refused command, a chunk whose checksum does not match, a failed write
    /// and QUIET_MS without a byte inside a file each answer "CARD FAIL <reason>". After a
    /// mismatched checksum the computer sends the chunk again. Every other failure ends the file,
    /// which then holds the chunks written before. A line that does not start with "CARD " is
    /// ignored.
    ///
    /// @note  The main loop calls it in every pass. It never waits.
    void update();

    /// Gets whether a file is being received.
    bool receiving() const { return inFile; }

private:
    /// What update() waits for.
    enum class State : uint8_t {
        LINE,      // the next command
        DATA,      // the bytes of a chunk
        WRITING,   // the card's end of a chunk's write
    };

    /// Carries out a command line.
    void command(const char* line);

    /// Ends the file being received after a failure, with an answer naming the reason.
    void fail(const char* reason);

    /// Sends an answer, formatted like printf.
    void reply(const char* format, ...) __attribute__((format(printf, 2, 3)));

    UploadLink& link;
    FileSystem& files;
    void (*const restart)();
    State state = State::LINE;
    char line[LINE_BYTES] = {};
    size_t lineLength = 0;
    bool lineTooLong = false;   // the current line ran past LINE_BYTES
    FileWriter file;            // the file being received
    bool inFile = false;        // a file is being received
    uint32_t size = 0;          // its size
    uint32_t done = 0;          // its bytes on the card
    uint8_t chunk[CHUNK_BYTES] = {};
    size_t chunkBytes = 0;   // the bytes the current chunk takes
    size_t chunkGot = 0;     // the bytes of it that have arrived
    uint32_t chunkCrc = 0;   // the checksum it has to match
    FileRequest writing;     // the write of the current chunk
    uint32_t heardUs = 0;    // the time the last byte of a file arrived
    bool restarting = false;
    uint32_t restartUs = 0;   // the time "CARD RESTARTING" was answered
};
