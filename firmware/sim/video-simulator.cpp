// The video player in the Arcade's WebAssembly module. The browser puts files on a memory card held
// in JavaScript, the storage service reads them for the file system, and the firmware's video
// player streams their frames into the screen's picture memory as on the machine. The sounds run
// on the computer tests' audio driver, which plays nothing, so a video plays without its sound and
// takes its time from the clock.
#include "video-simulator.h"
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <map>
#include <string>
#include "components/sounds.h"
#include "core/clock.h"
#include "drivers/audio.h"
#include "drivers/storage-service.h"
#include "logging/debug-log.h"

using emscripten::val;

namespace {

// A memory card whose files the browser holds, each a Uint8Array under its path. A read copies
// only the bytes it asks for into the module's memory.
struct BrowserCard : CardFiles {
    std::map<std::string, val> files;
    std::string slotPath[SLOTS];

    bool mount() override { return true; }
    bool remount() override { return true; }
    uint32_t failedTransfers() const override { return 0; }
    uint32_t errorCode() const override { return 0; }
    bool open(uint8_t slot, const char* path, bool writes) override {
        if (writes || files.count(path) == 0) {
            return false;
        }
        slotPath[slot] = path;
        return true;
    }
    void close(uint8_t slot) override { slotPath[slot].clear(); }
    uint32_t size(uint8_t slot) override {
        const auto f = files.find(slotPath[slot]);
        return f == files.end() ? 0 : f->second["length"].as<uint32_t>();
    }
    bool read(uint8_t slot, uint32_t at, uint8_t* to, size_t n, size_t& got) override {
        const auto f = files.find(slotPath[slot]);
        if (f == files.end()) {
            return false;
        }
        const uint32_t length = f->second["length"].as<uint32_t>();
        got = at >= length ? 0 : (n < length - at ? n : length - at);
        if (got != 0) {
            val view = val(emscripten::typed_memory_view(got, to));
            view.call<void>("set", f->second.call<val>("subarray", at, uint32_t(at + got)));
        }
        return true;
    }
    bool write(uint8_t, uint32_t, const uint8_t*, size_t) override { return false; }
    bool cut(uint8_t, uint32_t) override { return false; }
    bool commit(uint8_t) override { return false; }
};

// The storage service on the browser's card. The simulator runs one task, which serves the reads
// itself, so the defaults that let every caller start and wait stand.
struct BrowserService : StorageService {
    explicit BrowserService(CardFiles& card) : StorageService(card) {}
};

BrowserCard card;
BrowserService service(card);
FileSystem files(service);
AudioDriver audio;
Sounds sounds(audio, files);
VideoPlayer* player = nullptr;   // made in videoBegin(), once the screen has begun

// Puts a file on the card, such as "/video/intro.rgb", which a later play reads. A file of the same
// path is replaced.
void cardFile(std::string path, val bytes) {
    card.files[path] = bytes;
}

}   // namespace

void videoBegin(Screen& screen) {
    if (player != nullptr) {
        return;
    }
    if (!service.mount()) {
        DEBUG_LOG("simulator: the card does not mount\n");
    }
    sounds.begin();
    static VideoPlayer made(screen, files, sounds);
    player = &made;
    player->begin();
}

void videoPass() {
    if (player == nullptr) {
        return;
    }
    while (service.serveNext()) {
    }
    PinballEvents out;
    player->update(nowUs(), out);
}

FileSystem& videoFiles() {
    return files;
}

VideoPlayer* videoPlayer() {
    return player;
}

EMSCRIPTEN_BINDINGS(video) {
    emscripten::function("cardFile", &cardFile);
}
