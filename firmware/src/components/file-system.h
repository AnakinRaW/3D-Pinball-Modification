// Turns paths into files on the SD card and keeps a game inside its directory. The game's path
// "best" becomes "/games/<id>/best". A path that leaves the directory gives a file whose every
// request fails. A full path reaches any place of the card for the components and the host.
// Getting a file touches no card.
//
// The files and their requests come from drivers/storage-service.h.
//
// Design: docs/firmware/components/file-system.md.
#pragma once
#include "drivers/storage-service.h"

class FileSystem {
public:
    // Creates the files of a storage driver. No game is named yet.
    // storage is the driver whose files the component gives.
    explicit FileSystem(StorageService& storage) : storage(storage) {}

    // Gets a file by its path inside the running game's directory. Touches no card.
    // path names the file inside the directory. "best" gives "/games/<id>/best".
    // Returns a file whose every request fails for an empty path, a path from the card's root, a
    // path with "..", a full path too long for the storage driver, and before use() named a game.
    FileWriter open(const char* path) const;

private:
    friend class GameHost;
    friend class Logger;
    friend class Config;

    // Gets a file at any place of the card. Touches no card. The host, the logger and the config
    // only.
    // path is the file's full path, such as "/config.yml".
    FileWriter openFull(const char* path) const;

    // Names the game in whose directory open() opens files. The host only, while no file request
    // of the game before is pending.
    // game is the game's id, such as "a". It stays in place while the game runs.
    void use(const char* game) { running = game; }

    StorageService& storage;
    const char* running = nullptr;   // the running game's id
};
