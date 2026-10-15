// the files on the card, as the games, the host and the logger see them
class FileSystem {
public:
    explicit FileSystem(Storage& storage) : storage_(storage) {}

    // gets a file by its path inside the running game's directory, such as "best"; an empty
    // path, one from the card's root, one with .. or one too long gives a file whose requests
    // fail. No card access
    FileWriter open(const char* path);

private:
    friend class GameHost;
    friend class Logger;

    FileWriter openFull(const char* path);                 // gets a file by its full path, such as "/log.txt"
    void use(const char* game);                      // sets the game in whose directory open() opens files

    Storage&    storage_;
    const char* game_ = nullptr;                     // the running game's id
};

FileWriter FileSystem::open(const char* path) {
    if (!game_ || !path[0] || path[0] == '/' || strstr(path, "..")) return FileWriter{};   // a game stays inside its directory
    char full[Storage::kPath];
    if (snprintf(full, Storage::kPath, "/games/%s/%s", game_, path) >= Storage::kPath) return FileWriter{};   // too long
    return openFull(full);
}

FileWriter FileSystem::openFull(const char* path) {
    return storage_.openFile(path);                                       // the file keeps a copy of the path
}

void FileSystem::use(const char* game) { game_ = game; }
