# File system component

The file system component turns paths into files on the SD card and keeps a game inside its directory. The games, the host, the logger, the config, the sounds and the screen get their files from it.

Each file is a [`FileWriter`](../drivers/storage.md#files) of the [storage driver](../drivers/storage.md).

## Paths

`open` opens a game its files by their path inside its directory `/games/<id>`. The path `best.txt` stands for `/games/<id>/best.txt`. The component puts the running game's directory in front of the path. 

A path that is empty, starts with `/`, holds `..` or is too long gives a file whose every request fails. The host names the running game with `use()`.

Before the first game the host names the card's root with `useRoot()`, where the machine's files lie, such as the [boot film](../game-abstraction.md#host). A path then counts from the root, so `video/intro.rgb` stands for `/video/intro.rgb`. The same paths are refused. The next `use()` puts a game's directory in front again.

`openFull()` opens files by its full path. This operation is not available to games but only other components and the host.

## Interface

[`components/file-system.h`](../../../firmware/src/components/file-system.h) holds `FileSystem`.
