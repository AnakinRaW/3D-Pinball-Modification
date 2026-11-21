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
| Reads a picture into the screen's picture memory without waiting | a game through the screen |
| Writes the log's lines at a chosen place of a file, or starts a log file anew, without waiting, at most once a second unless the log's buffer is half full | the logger |
| Reads up to four files at once and restarts any of them at any moment, and no sound runs dry | the audio driver's interrupt |
| Reads one file at 25 frames a second, once or in a loop, and every frame arrives in time | the video player |
| Turns a name into a file without touching the card | the sounds and the screen components |

## Storage task

All card work runs in the storage task, one of the FreeRTOS [tasks](../general-design.md#execution-model). It is the only task that calls SdFat. It has the main task's priority, and the two take turns every tick of 1 ms. A card that holds SdFat in a wait therefore takes at most every second millisecond from the main loop.

The task serves the requests one after another. A request of a higher priority goes before every waiting request of a lower one. Requests of one priority go in the order they were started, so a read started after a write of the same priority returns the new data.

While SdFat waits for the card, it keeps calling the Arduino function `yield()`. The firmware replaces `yield()`. Called from within the storage task, the new `yield()` hands the processor to the main task until the next tick. Everywhere else, it calls freertos-teensy's `default_yield()`, which handles serial events and PJRC's `EventResponder` as on any Teensy. The main loop calls `yield()` once per pass, because the Arduino `loop()` is not used under FreeRTOS.

## Files

A file is addressed by its full path from the card's root, such as `/games/<id>/best`. `storage.open()` gives a `FileReader`, which only reads. `storage.openWriter()` gives a `FileWriter`, which writes as well and which the [file system component](../components/file-system.md) hands to the games. Both give only a handle, and the card is touched with the first read or write.

A path cannot be longer than 63 characters. A longer one gives a file whose every read and write fails.

A missing file reads as 0 bytes, and its first write creates it.

## Requests

Every read and every write is carried by a `FileRequest`. A call of `read()`, `write()` or `rewrite()` starts the request and returns at once.

`status()` is `PENDING` until the request has ended as `SUCCEEDED` or `FAILED`. A request that was not started yet is `IDLE`.

A request carries one read or write at a time. Starting a second operation on a pending request fails both.

`fileSize()` returns the file's size as the request left it. It is 0 for a missing file, after a card error and while the card counts as failed.

A request that is destroyed while still pending waits in its destructor until it has ended, so the storage task never writes into memory that has gone. A request started from an interrupt lives as long as the firmware, since an interrupt cannot wait.

`wait()` waits until the request has ended and returns whether it succeeded. From an interrupt and from the storage task it returns false at once, and the request goes on.

A request's priority is set when it is made: `HIGHEST`, `RAISED` or `NORMAL`. A request made without one has `NORMAL`.

An interrupt that starts requests has a priority number of 208 or more, since only such an interrupt may call FreeRTOS. A request started from a more urgent interrupt fails at once.

## Reading

`read()` reads up to `size` bytes from position `at` of the file into `buffer`. A read stops at the end of the file. It brings 0 bytes from a missing or empty file and from a position at or past the end. `bytes()` gives how many bytes it brought.

SdFat reads a file one cluster at a time. In its DMA mode each cluster costs one command to the card, so a large read is as fast as the card's clusters are large. The card is formatted FAT32 with 32 KB clusters. A full-screen video frame then arrives within the time the [display's appendix](../../parts/display/design.md#appendix-derivations) measures.

A file opened for reading goes through SdFat's `contiguousRange()` once. A FAT32 file loses on open the mark that its clusters lie in one piece, and without it SdFat asks the FAT for each next cluster. Marked again, a full-screen frame read in 16.65 ms instead of 17.35 ms on the bench.

## Writing

`write()` writes `size` bytes at position `at` of the file. It takes the data directly from `data`, so the caller leaves `data` unchanged until the request has ended. A write that starts past the end of the file fails. A write into a missing directory creates the directory first. The bytes are on the card once the request has succeeded.

`rewrite()` replaces the file's content with `data`. Its data stays unchanged until the request has ended, as a write's does.

A power cut during a write can damage the file. If the file's size changes, it can damage the card's file system as well.

The [card appender](../logging.md) writes through a `CardFile`, which carries one request for a `FileWriter`.

## Device faults

A request fails with a card error when a transfer fails or the card does not answer within 1 s. After a card error the driver closes every file and starts the card anew.

Before every start of the card, the driver reads the socket's DAT3 line with a pull-down. A card pulls the line high. Without a card the start ends at once, since SdFat would look for a card for 3 s.

The card counts as failed when `begin()` finds no card or no file system it can read, after three card errors in a row, or when the card does not come back after a card error. [`failed()`](../error-handling.md#device-faults) reports it with SdFat's code of the last card error. While the card counts as failed, every request fails at once.

Once a second the driver starts a failed card again. The card that `begin()` found works again as soon as it answers. Another card stays unused until a restart. So does a card that `begin()` could not set up.

On the bench a read that met a pulled card failed after 1 s. The main task meanwhile waited at most 1.00 ms in a row, and the card put back worked again on the next try.

## Limitations

The first save into a new file takes longer than one over an existing file. On the bench a save of 4 KB took at most 15.80 ms into a new file and 4.69 ms over an existing one, from the PSRAM as from RAM2.

Under full load a movie frame took 21.35 ms in the mean from its request to its end. A read requested while a write runs waits for that write. Right after a large file was written, the card held one save for 953 ms on the bench.

## Existing libraries

SdFat reads and writes the card. No library offers a prioritized request queue that callers can poll. The storage task provides one.

[FreeRTOS+FAT](https://github.com/FreeRTOS/Lab-Project-FreeRTOS-FAT) has no driver for the i.MX RT's SD controller and cannot read exFAT. SdFat already brings that driver for the Teensy 4.1.

PJRC's SD library mounts the built-in card in SdFat's FIFO mode only, which moves every byte through the processor.

NXP's [SD stack](https://github.com/nxp-mcuxpresso/mcux-sdk-middleware-sdmmc) is not part of Teensyduino. Porting it would take a lot of work and gain nothing new. Its calls block as well, so the storage task would still be needed.

## Interface

[`drivers/storage.h`](../../../firmware/src/drivers/storage.h) holds the driver and `storage`. [`drivers/storage-service.h`](../../../firmware/src/drivers/storage-service.h) holds the files, the requests and their order.
