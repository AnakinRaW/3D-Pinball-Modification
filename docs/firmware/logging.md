# Logging

The logger writes the machine's log to `/log.txt` on the SD card. A game, a component or the host calls `write()` to create a log entry. Each line starts with the milliseconds since the machine started.

## Size limit

The log file is limited to 1 MB size. When the file is about to exceed that size, the logger empties it and starts logging all over.

## Buffered writes

Writing to the log should never wait for the SD card. Therefore the logger collects the lines it wants to log in a buffer of 2 KB in RAM. `update()` writes the collected lines once a second. 

`begin()` finds the end of the file, where the log goes on. It waits for the card, so the main task calls it right after the storage driver's `begin()`.

A line is lost when the buffer is full. `lost()` counts these lines. A failed write loses its lines too. A crash or a power cut loses the lines of the last second.

## Interface

```cpp
// the machine's log on the card; a game, a component and the host write into it from the main loop
class Logger {
public:
    explicit Logger(FileSystem& files);

    // opens /log.txt and goes on at its end; the main task only, right after the storage
    // driver's begin()
    void begin();

    // formats a line like printf, puts the time since the machine started and who in front of it
    // and collects it; returns at once. False when the line is lost
    bool write(const char* who, const char* format, ...);

    // writes the collected lines once a second has passed and new lines have come in; the main
    // loop calls it in every pass
    void update(uint32_t now);

    uint32_t lost() const;                   // gets how many lines were lost

    static constexpr size_t kBuffer = 2048;  // the lines collected between two writes, and so the longest line
};
```
