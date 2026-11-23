# Card upload

The card upload puts the repository's files for the memory card onto the card while it sits in the Teensy. The [machine config](config.md) and the games change on a computer and reach the machine over the USB cable, so the card never leaves its socket.

`python -m platformio run -t uploadcard` copies every file of [`firmware/src/resources/`](../../../firmware/src/resources/) but its README to the card's root and every file under `games/` to `/games/`, each with its path below that folder. [`firmware/tools/card-upload.py`](../../../firmware/tools/card-upload.py) does the work and takes single files as well. A file the upload writes replaces the file on the card. Every other file on the card stays. The Teensy restarts at the end and starts with the new files. The upload runs only on that command.

## Transfer

The computer sends a file in chunks of up to 4 KB. Each chunk carries its CRC-32, the checksum of Python's `zlib.crc32()`. The firmware writes a chunk through the [storage driver](../drivers/storage.md) and answers once it is on the card. The computer sends the next chunk only then. A chunk whose checksum does not match is refused and sent again.

| Command | Answer |
|---|---|
| `CARD PUT <path> <bytes>` | `CARD READY`, or `CARD DONE 0` for an empty file |
| `CARD DATA <bytes> <crc32>`, followed by the bytes | `CARD OK <bytes so far>`, or `CARD DONE <bytes>` after the last chunk |
| `CARD RESTART` | `CARD RESTARTING`, and the restart 100 ms later |

A refused command, a failed write and 2 s without a byte inside a file each answer `CARD FAIL` with the reason. A failure other than a checksum ends the file, which then holds the chunks written before it.

The main loop takes the bytes that have arrived and never waits for the computer or the card. The answers leave with the [debug output](../logging.md#debug-output), so they never mix with a debug line.

## Rejected alternative

The Teensy's USB type MTP would show the card as a drive on the computer. The computer and the storage task would then write the same file system at once. Either can damage what the other wrote.

## Interface

[`components/card-upload.h`](../../../firmware/src/components/card-upload.h) holds `CardUpload`. [`io/usb-upload-link.h`](../../../firmware/src/io/usb-upload-link.h) connects it to USB serial.
