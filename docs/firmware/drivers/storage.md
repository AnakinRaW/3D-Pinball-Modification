# Storage driver

The storage driver controls the Teensy's built-in SD card. It mounts the card and finds the files using [SdFat](https://github.com/PaulStoffregen/SdFat). The card stores its data in sectors of 512 bytes.

## Requirements and Use Cases

- Large reads and writes do not block the rest of the firmware.
- The machine needs to work with no SD card installed. Consumers should work normally but can learn that their reads and writes failed.
- The driver must be the only user of the SD card to prevent crashes ([PJRC forum](https://forum.pjrc.com/index.php?posts/302916/)). File consumers use this driver solely.

The firmware uses the card at start-up, while it runs and for the streams of sound and picture.

| Use Case | Consumer |
|---|---|
| Reads small files and single sectors at chosen places and may wait for them | drivers, the logger and the game host at start-up |
| Reads and writes small files without waiting, and learns later whether a write worked. A further save waits until the previous one has ended | a game and the host after start-up |
| Reads a picture into a layer without waiting | a game through the screen |
| Writes once a second at a chosen place of the log without waiting | the logger |
| Reads up to four files at once and restarts any of them at any moment, and no sound runs dry | the audio driver's interrupt |
| Reads one file at 25 frames a second, once or in a loop, and every frame arrives in time | the display driver's interrupt |
| Turns a name into a file without touching the card | the sounds and the screen components |

## Storage task

All card work runs in the storage task, one of the FreeRTOS [tasks](../general-design.md#execution-model). It is the only task that calls SdFat. It has a higher priority than the main task, so the next transfer starts as soon as one ends. It wakes for every new request and carries the requests out one after another, in the order they were started. A read started after a write therefore returns the new data. While the card moves the data by DMA, the task sleeps and the main loop runs.

## Files

A file is addressed by its full path from the card's root, such as `/games/<id>/best`. `storage.open()` gives a `FileReader`, which only reads. The [file system component](../components/file-system.md)'s `open()` gives a `FileWriter`, which writes as well. Both give only a handle, and card IO operations start with the first read or write. 

A path cannot be longer than 63 characters. A longer one gives a file whose every read and write fails.

A missing file reads as 0 bytes, and its first write creates it.

## Requests

Every read and every write is represented by a `FileRequest`. A call of `read()`, `write()` or `rewrite()` starts the request and returns at once.

`status()` is `Pending` until the request has ended. A completed request is either `Succeeded` or `Failed`, and the mask `Completed` covers both. A request that was not yet started returns `Idle`.

A request carries one read or write at a time. While a request is `Pending`, starting a second file operation on the same request fails both requests.

`fileSize()` returns the file's size as the request left it, and 0 where the request met a card error or the card counts as failed.

If a request is destroyed while still pending, its destructor waits until the operation is completed, to ensure memory safety. 
> Destroying a pending request in an interrupt deadlocks because the storage task cannot run until the interrupt returns.

`wait()` waits until the request has ended and tells whether the waiting worked. `false` is retured immediately when trying to wait from an interrupt, or from a task whose priority is not below the storage task's. The request continues normally.

A request can have `Priority::High`, which puts it ahead of every waiting request of normal priority. It can also name an interrupt, which it raises when it ends. These features are only available using a [file channel](../file-channel.md).

An interrupt that starts requests has a priority number of 208 or more (higher value here means less priority), since only such an interrupt may call FreeRTOS. A request started from a more urgent interrupt fails at once.

## Reading

`read()` reads up to `size` bytes from position `at` of the file into `buffer`. A read stops at the end of the file. It brings 0 bytes from a missing or empty file and from a position at or past the end. `bytes()` gives how many bytes it brought. `fileSize()` gives the file’s size after every read, also when the read brought 0 bytes.

## Writing

`write()` writes `size` bytes at position `at` of the file. It therefore takes the data directly from `data`, so the caller leaves `data` unchanged until the request has ended. A write that starts past the end of the file fails. A write into a missing directory creates the directory first.

`rewrite()` truncates the file with the content of `data`. Its data stays unchanged until the request has ended, as a write's does.

A power cut during a write can damage the file. If the file's size changes, it can damage the card's file system as well.

## Driver events

The driver publishes `WritingStarted` when it begins a write while none ran, and `WritingEnded` once no write runs or waits any more. Between the two, the card is being written.

| Field | Content |
|---|---|
| Type | `DriverEventType::WritingStarted` or `DriverEventType::WritingEnded` |
| Source | `0`, the one card |
| Timestamp | `micros()` in the storage task |
| Payload | None |

Each request reports its own end. A failing card reaches the event queue through the [device monitor](../error-handling.md#device-faults).

## Device faults

A request fails with a card error when the card reports its transfer as failed or does not answer within 1 s.

The card counts as failed when `begin()` finds no card or no file system it can read, after three card errors in a row, or when the file system does not come back after a card error. 

[`failed()`](../error-handling.md#device-faults) reports it with the code of the last card error. While the card counts as failed, every request fails at once. 

The driver asks the card for its status once a second and clears the fault as soon as the card answers and its file system reads. 

A card that `begin()` could not set up, and a card pulled out and put back, stay failed until a restart.

## Limitations

When the card gives a broken answer to a command that starts a transfer, the main loop can be stuck for one second.

## Driver Interface

```cpp
// how a request stands
enum class FileStatus : uint8_t {
    Idle      = 0,                       // not started yet
    Pending   = 1,                       // started, and not ended yet
    Succeeded = 2,                       // ended and worked
    Failed    = 4,                       // ended and failed, or could not start
    Completed = Succeeded | Failed       // a mask of both ends, never a status itself
};

// gets whether status s is one of the states in mask, such as status() & FileStatus::Completed
constexpr bool operator&(FileStatus s, FileStatus mask) { return (uint8_t)s & (uint8_t)mask; }

class FileReader;
class FileWriter;
class FileRequest;

class Storage : public Driver {
public:
    enum class Priority : uint8_t { High, Normal };   // a High request goes before every Normal one

    static constexpr uint8_t kPath = 64;  // the longest path, with its end

    // starts the storage task, which mounts the card through SdFat in its DMA mode, and waits
    // for the mount; the main task only. False when the device monitor is full, and when the
    // card is missing or its file system does not read, which the device monitor then reports
    bool begin() override;

    // gets a file that reads, by its full path such as "/games/<id>/audio/bumper.wav", without
    // touching the card; a path too long gives a file whose requests fail
    FileReader open(const char* path);

    // gets which parts have failed, bit 0 for the card, and SdFat's error code of its last failure
    Fault failed() const override;
};

// a file on the card by its full path, which reads. It holds only the path, so it can be copied
// and let go at any time
class FileReader {
public:
    FileReader() = default;                  // a file whose requests fail

    // starts reading up to size bytes from position at into buffer and returns at once
    void read(uint32_t at, void* buffer, size_t size, FileRequest& r) const;

    const char* name() const;                // gets the full path
};

// a file on the card that reads and writes, which the file system component gives
class FileWriter : public FileReader {
public:
    // starts writing size bytes from data at position at and returns at once; the data stays
    // unchanged until r has ended
    void write(uint32_t at, const void* data, size_t size, FileRequest& r) const;

    // starts writing data as the file's whole content and returns at once; the data stays
    // unchanged until r has ended
    void rewrite(const void* data, size_t size, FileRequest& r) const;
};

// one read or write on the card. Starting it links it into a list, and the storage task writes its
// results into it, so it can be neither copied nor moved. The buffer or data it names stays in place
// until it has ended
class FileRequest {
public:
    FileRequest() = default;                 // a request of normal priority, which raises no interrupt

    // waits for the end of a request that has not ended, so the storage task never writes into
    // memory that has gone; an interrupt's requests live as long as the firmware
    ~FileRequest();

    FileRequest(const FileRequest&)            = delete;
    FileRequest& operator=(const FileRequest&) = delete;

    FileStatus status() const;                       // gets how the request stands
    size_t     bytes() const;                        // gets the bytes a read brought, 0 until it has succeeded
    uint32_t   fileSize() const;                     // gets the file's size as the request left it; 0 until it has ended, after a card error and while the card counts as failed

    // waits until the request has ended and gets whether it worked; false at once from an
    // interrupt or a task whose priority is not below the storage task's
    bool wait() const;
};

extern Storage storage;                      // the one storage driver, defined in the main file
```
