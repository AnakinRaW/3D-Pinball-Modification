// Writes files that a computer sends over USB onto the card in the Teensy 4.1's built-in socket.
// firmware/tools/card-upload.py flashes it, sends the files and flashes the firmware back.
//
// Every line it sends starts with CARD. "CARD READY" greets once the card is mounted. The computer
// sends "CARD PUT <path> <bytes>", which "CARD OPEN" answers, then "CARD DATA <bytes> <crc32>"
// with that many bytes of the file for each chunk. "CARD OK <bytes so far>" answers a chunk, and
// "CARD DONE <bytes>" the last one. "CARD FAIL <reason>" answers a failure. After a checksum that
// does not match, the computer sends the chunk again. Every other failure ends the file.

#include <SdFat.h>

constexpr size_t CHUNK_BYTES = 4096;      // the largest chunk, the size card-upload.py sends
constexpr size_t LINE_BYTES = 128;        // the longest command
constexpr uint32_t QUIET_MS = 2000;       // the longest wait for the bytes of a chunk
/// CRC-32 of IEEE 802.3 with its bits reversed, the checksum Python's zlib.crc32() computes.
constexpr uint32_t CRC_POLYNOMIAL = 0xEDB88320;
constexpr int RADIX_DECIMAL = 10;
constexpr int RADIX_HEX = 16;

static SdFs sd;
static FsFile file;
static uint32_t size = 0;   // the size of the file being written
static uint32_t done = 0;   // its bytes on the card
static uint8_t chunk[CHUNK_BYTES];
static char line[LINE_BYTES];

/// Gets the CRC-32 of a run of bytes.
static uint32_t crc32(const uint8_t* data, size_t n) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < n; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc >> 1) ^ (CRC_POLYNOMIAL & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

/// Ends the file being written after a failure, with an answer naming the reason.
static void fail(const char* reason) {
    Serial.printf("CARD FAIL %s\n", reason);
    file.close();
    size = 0;
    done = 0;
}

/// Opens a file for writing, with the directories above it, and empties it.
static void put(char* args) {
    char* space = strrchr(args, ' ');
    char* end = nullptr;
    const unsigned long bytes = space != nullptr ? strtoul(space + 1, &end, RADIX_DECIMAL) : 0;
    if (space == nullptr || end == space + 1 || *end != '\0' || args[0] != '/' ||
        strstr(args, "..") != nullptr) {
        fail("PUT takes a full path and a size");
        return;
    }
    *space = '\0';
    char* slash = strrchr(args, '/');
    if (slash != args) {
        *slash = '\0';
        const bool made = sd.exists(args) || sd.mkdir(args, true);
        *slash = '/';
        if (!made) {
            fail("the directory does not open");
            return;
        }
    }
    file.close();
    if (!file.open(args, O_WRONLY | O_CREAT | O_TRUNC)) {
        fail("the file does not open");
        return;
    }
    size = uint32_t(bytes);
    done = 0;
    if (size == 0) {
        file.close();
        Serial.printf("CARD DONE 0\n");
        return;
    }
    Serial.printf("CARD OPEN\n");
}

/// Takes one chunk of the file being written.
static void data(char* args) {
    char* end = nullptr;
    const unsigned long bytes = strtoul(args, &end, RADIX_DECIMAL);
    const char* sum = end;
    const unsigned long crc = *sum == ' ' ? strtoul(sum + 1, &end, RADIX_HEX) : 0;
    if (!file.isOpen()) {
        Serial.printf("CARD FAIL no file is being written\n");
        return;
    }
    if (*sum != ' ' || *end != '\0' || bytes == 0 || bytes > CHUNK_BYTES || bytes > size - done) {
        fail("the chunk is malformed, too large or runs past the file");
        return;
    }
    Serial.setTimeout(QUIET_MS);
    if (Serial.readBytes(reinterpret_cast<char*>(chunk), bytes) != bytes) {
        fail("no byte for too long");
        return;
    }
    if (crc32(chunk, bytes) != crc) {
        Serial.printf("CARD FAIL checksum\n");
        return;
    }
    if (file.write(chunk, bytes) != bytes) {
        fail("the card did not take the chunk");
        return;
    }
    done += uint32_t(bytes);
    if (done < size) {
        Serial.printf("CARD OK %lu\n", static_cast<unsigned long>(done));
        return;
    }
    if (!file.close()) {
        fail("the file did not close");
        return;
    }
    Serial.printf("CARD DONE %lu\n", static_cast<unsigned long>(done));
}

void setup() {
    Serial.begin(115200);
    while (!Serial) {
    }
    if (!sd.begin(SdioConfig(FIFO_SDIO))) {
        Serial.printf("CARD FAIL no card answers\n");
        return;
    }
    Serial.printf("CARD READY\n");
}

void loop() {
    static size_t length = 0;
    while (Serial.available() > 0) {
        const char c = char(Serial.read());
        if (c == '\r') {
            continue;
        }
        if (c != '\n') {
            if (length + 1 < LINE_BYTES) {
                line[length++] = c;
            }
            continue;
        }
        line[length] = '\0';
        length = 0;
        if (strncmp(line, "CARD PUT ", 9) == 0) {
            put(line + 9);
        } else if (strncmp(line, "CARD DATA ", 10) == 0) {
            data(line + 10);
            return;
        } else if (strncmp(line, "CARD ", 5) == 0) {
            Serial.printf("CARD FAIL unknown command\n");
        }
    }
}
