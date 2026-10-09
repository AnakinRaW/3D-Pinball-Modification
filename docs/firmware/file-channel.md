# File channel

A `FileChannel` reads files from the SD card for one driver, one file after another, through a buffer that refills itself. It is made for drivers that take a file's bytes in their interrupt. Since the interrupt itself cannot wait for the card, the file channel automatically reads ahead and keeps the file's next part in RAM.

A file channel reads each file from front to back, and starts it over when told to loop. When the next part has not arrived yet, the driver gets fewer bytes and asks again on its next run. 

Code in the main loop reads a large file with `read()` in pieces instead.

## Ring buffer

The file channel keeps the bytes it has read ahead in a ring buffer. The card fills the ring in chunks of equal size one after another. The driver empties the buffer in the same order. The ring belongs to the driver that reads the file. That driver sets how many chunks the ring holds when it declares the channel, as in `FileChannel<4>` for four chunks, and the size of a chunk in the constructor. A chunk is free again once `take()` has taken all its bytes.

## Reading a file

`start()` sets the file to read and begins reading it from its beginning at once. With `loop` set, the file starts over whenever it ends. A new `start()` replaces the file at once, and the bytes of the previous file in the ring are gone. `stop()` drops the file in the same way and reads nothing. A piece of the old file that the card is still reading is thrown away when it arrives.

`take(to, n)` copies up to `n` bytes from the ring into `to` and returns the number it copied. That number may be smaller than `n` when the ring holds fewer bytes. With `to` set to `nullptr`, `take()` skips the bytes instead.

`available()` returns how many bytes `take()` could take right now.

Every call of `take()` or `available()` starts the next read when a chunk is free and no read is running. 

The constructor can take an interrupt, which the channel raises whenever a read ends.

`ended()` tells that the file has run out or a read has failed, and that every byte before has been taken. `ended()` is also true for a missing file, for a channel that was never started and after `stop()`.

## Interrupts and priority

Calls that come from the driver's interrupt return at once. That interrupt must not be more urgent than priority 208, as the [storage driver](drivers/storage.md#requests) requires. 

A file channel supports setting a higher priority, so its reads are scheduled before other reads and writes.

## Interface

```cpp
// reads files one after another from the card into a ring of chunks, for the one place that takes
// the bytes; every call comes from that place and returns at once
template <uint8_t Chunks>                  // the chunks the ring holds, such as FileChannel<4>
class FileChannel {
    static_assert(Chunks > 0, "a ring holds at least one chunk");

public:
    // a ring of Chunks chunks of chunk bytes each at ring, which starts on a 32-byte boundary, read
    // with priority
    FileChannel(uint8_t* ring, uint32_t chunk, Storage::Priority priority = Storage::Priority::Normal);

    // the same, and every read raises the interrupt wake when it ends
    FileChannel(uint8_t* ring, uint32_t chunk, IRQ_NUMBER_t wake,
              Storage::Priority priority = Storage::Priority::Normal);

    // reads file from its beginning, and from its beginning again at its end when loop is set; the
    // bytes of the file read before are gone at once
    void start(const FileReader& file, bool loop);

    void stop();                           // ends the file it reads; its bytes are gone at once

    // copies up to n bytes from the front of the ring into to, across chunks, or skips them when to
    // is nullptr; starts the next read where a chunk is free and gets how many it took
    size_t take(void* to, size_t n);

    // gets how many bytes take() could take now; also starts the next read where a chunk is free
    size_t available();

    bool ended() const;                    // gets whether the file has run out or a read has failed and every byte before is taken, or no file is read
};
```
