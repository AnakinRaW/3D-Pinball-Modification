# File system component

The file system component gives the games, the host and the logger the files they read and write on the SD card. 

Each file is a [`FileWriter`](../drivers/storage.md#files) of the [storage driver](../drivers/storage.md).

## Paths

`open` opens a game its files by their path inside its directory `/games/<id>`. The path `best.txt` stands for `/games/<id>/best.txt`. The component puts the running game's directory in front of the path. 

A path that is empty, starts with `/`, holds `..` or is too long gives a file whose every request fails. The host names the running game with `use()`.

`openFull()` opens files by its full path. This operation is not available to games but only other components and the host.

## Interface

```cpp
// the files on the card, as the games, the host and the logger see them
class FileSystem {
public:
    explicit FileSystem(Storage& storage);

    // gets a file by its path inside the running game's directory, such as "best"; an empty
    // path, one from the card's root, one with .. or one too long gives a file whose requests
    // fail. No card access
    FileWriter open(const char* path);

private:
    friend class GameHost;
    friend class Logger;

    // gets a file by its full path, such as "/log.txt"; the host and the logger only. No card access
    FileWriter openFull(const char* path);

    // sets the game in whose directory open() opens files; the host only
    void use(const char* game);
};
```
