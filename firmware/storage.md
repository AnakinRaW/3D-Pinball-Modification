# Storage driver

The storage driver controls the Teensy's built-in SD card. It is a small asynchronous wrapper around [SdFat](https://github.com/greiman/SdFat), the file system library Teensyduino ships, so that no call waits for the card. The firmware needs it because several consumers using the card at once without a guard are likely to crash the Teensy ([PJRC forum](https://forum.pjrc.com/index.php?posts/302916/)). The card holds the games' assets, such as sounds, and each game's settings and scores.

## Files

Every file on the card is read and written the same way. `useGame()` makes the running game's directory, `/games/<game>`, the root of every path, so a game opens its files as `audio/bumper.wav` or `save/best`. A path that starts with `/` still counts from the card's root. 

`open()` takes such a path and gives a `File` without reading the card. 

`read()` and `write()` on that `File` return at once, and the card interrupt moves the bytes. 

A consumer in the main loop, such as the host or a game, knows that a read has finished from the event `FileRead`. A driver passes a handler instead. The card interrupt calls it as soon as the bytes are in the buffer, so the driver that asked can use them without waiting for the main loop. The handler runs inside the card interrupt and has to stay short. 

Requests on files are handled in the order they were made, just like ordinary synchronous blocking calls.

## Card access

The card interrupt is a software interrupt on the free vector `IRQ_Reserved2` that handles all card accesses one after another at the lowest priority of 240. This ensures every other driver interrupt goes first.

The card carries large files such as sounds and images, which have to stream without gaps, so it should be of class A1 or A2.

## Finding a file

`begin()` looks up every file of every game once and remembers where it lies on the card, so `open()` needs no search through the directories. A file that does not exist yet, such as the high score before the first game, can still be opened. Reading it gives 0 bytes, and its first `write()` creates it on the card.

## The event

| Field | Content |
|---|---|
| Type | `EventType::FileRead` |
| Source | The tag the caller gave `read()` |
| Timestamp | `micros()` in the card interrupt that finished the read |
| Payload | The bytes read, `0` for a file the card does not hold |

## The driver

```cpp
using Done = void (*)(void* context, size_t bytes);   // runs in the card interrupt once the bytes are in; keep it short

// one file on the card; every call returns at once, and requests are handled in the order they were made
class File {
public:
    // starts reading the next size bytes into buffer, and FileRead reaches the queue with
    // the tag once they are there. The buffer has to stay valid until then
    void read(void* buffer, size_t size, uint8_t tag);

    // the same for a driver: done runs in the card interrupt instead of the event
    void read(void* buffer, size_t size, Done done, void* context);

    // copies size bytes into the driver's own buffer and starts writing them at the
    // position; data is free again when the call returns
    void write(const void* data, size_t size);

    void     seek(uint32_t position);    // sets where the next read or write starts
    uint32_t size() const;               // gets the file's size, 0 for one not written yet
};

class Storage {
public:
    // mounts the card, indexes the files of every game and sets up the card
    // interrupt; false when the card is missing
    bool begin(EventQueue::Producer& out);

    // makes one game's directory, /games/<game>, the root of every relative path;
    // no card access
    void useGame(const char* game);

    // gets a file by its path from the index, such as "save/best"; one missing there
    // is created by its first write. No card access
    File open(const char* path);
};
```

## Inside the driver

```cpp
// one request, as the card interrupt handles it
struct Request {
    FsFile*  file;               // the open SdFat file it works on
    uint32_t position;           // where it starts, fixed when the call was made
    uint8_t* buffer;             // where a read lands, or the driver's copy of a write
    size_t   size;
    bool     write;
    Done     done;               // the calling driver's handler, or nullptr for FileRead
    void*    context;
    uint8_t  tag;
};

// the card in FIFO mode, and the card interrupt at the lowest priority
bool Storage::begin(EventQueue::Producer& out) {
    out_ = &out;
    if (!sd_.begin(SdioConfig(FIFO_SDIO))) return false;
    index();                                        // every game's directory, once
    attachInterruptVector(IRQ_Reserved2, card0);
    NVIC_SET_PRIORITY(IRQ_Reserved2, 240);
    NVIC_ENABLE_IRQ(IRQ_Reserved2);
    return true;
}

// read() and write() end here: they queue the request and raise the card interrupt
void Storage::queue(const Request& r) {
    noInterrupts();                                 // callers in the main loop and in interrupts both queue
    requests_.push(r);
    interrupts();
    NVIC_SET_PENDING(IRQ_Reserved2);
}

// the card interrupt: every request, one after another, in the order they were made
void Storage::card() {
    Request r;
    while (next(r)) {                               // next() takes the oldest request under noInterrupts()
        r.file->seekSet(r.position);
        const size_t n = r.write ? r.file->write(r.buffer, r.size)
                                 : r.file->read(r.buffer, r.size);
        if (r.write)     release(r.buffer);         // the copy write() made
        else if (r.done) r.done(r.context, n);      // a driver's read
        else             out_->publish(PinballEvent{micros(), EventType::FileRead, r.tag, (uint32_t)n});
    }
}
```
