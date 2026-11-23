# Logging

The machine keeps a log on the SD card, so a fault that happened during a game can be read on a computer afterwards. A game, a component, the host or the watchdog's report writes a line into it. A debug build also writes debug lines to USB serial.

No writer ever waits for the card or for USB. A writer only puts its line into a buffer in memory. The main loop passes the waiting lines on to the card once a second and to USB as fast as USB takes them.

## Lines

A line of the log reads `73021 W lamps no lamp 12`: the milliseconds since the start, the level, the writer's name and the text. The level is `I` for a fact worth keeping and `W` for something that went wrong. The milliseconds come from `millis()`. A line takes at most 160 bytes with its line break. A longer one is cut and ends in `...`.

A debug line carries the milliseconds, the letter `D` and the text, such as `73021 D ServoDriver::begin: the driver tick refused its step`.

## Writers

Every task may write a line, the main task and the storage task alike, through `logWarning()` or `logInfo()`. Before the log has begun, a debug build shows such a line on USB alone. Writing a line only puts it into the buffer. The file on the card is written by the storage task alone, once `update()` hands it the waiting lines. Lines from two tasks never mix, because a line enters the buffer whole under a lock. An interrupt never writes: its line is lost and counted. A driver reports a fault as an event. The host writes the line.

## Buffer

The card serves the music and the movies as well. The SD specification allows a card up to half a second for each write. The log therefore collects its lines and writes them in few, large writes. The buffer is sized so that a burst of about a thousand lines, or a card that stalls for several seconds, loses no line. It lies in the PSRAM, since RAM2 is planned almost to its end.

The log's buffer takes 64 KB in the PSRAM. `update()` in the main loop commands to write the waiting lines to the card once a second, or at once when half the buffer is taken. A write that fails keeps its lines in the buffer. The next write a second later tries them again at the same place.

A line is lost only when it finds the buffer full. `lost()` counts it. Once a line fits again, the log states the count in a line such as `80211 W logger 3 lines lost`. A lost line is a fault that the host reports. A crash or a power cut loses the lines of the last second.

## Files

The log rolls over two files of up to 1 MB each, `/log-a.txt` and `/log-b.txt`. When the current file is full, the log starts the other one anew. The latest megabyte of lines therefore always stays on the card.

Each file starts with a head line that numbers it, such as `0 I logger file 7`. `begin()` goes on in the file with the higher number. Without any head line it goes on in `/log-a.txt`. The main task reads the start of both files right after the storage driver's `begin()` and hands it to `begin()`. Without a card the log does not begin, and every line is lost.
## Debug output

`DEBUG_LOG()` writes a debug line in a debug build. In a release build the compiler removes every `DEBUG_LOG()` call. The line waits in a separate buffer of 16 KB in the PSRAM. `sendDebugLog()` in the main loop hands USB only what it takes without waiting.

An interrupt notes an event with `DEBUG_TRACE(label, a, b)`: a label, two numbers and the time from `nowUs()`. A note takes a few cycles, formats no text and never waits, so any interrupt at any priority may note. `sendDebugLog()` turns the waiting notes into debug lines such as `73021 D trace ir late 3 -12 at 73020512 us`. The trace holds 256 notes. A note that finds it full is lost. The debug output states what it and the trace had to drop in a line such as `73021 D trace 44 notes lost`.

In a debug build every line of the log goes to the debug output as well. The buffer stands in a release build too, where it carries the answers of the [card upload](components/card-upload.md) alone.

## Interface

| Header | Holds |
|---|---|
| [`logging/logger.h`](../../firmware/src/logging/logger.h) | `Logger`, the log on the card |
| [`io/card-appender.h`](../../firmware/src/io/card-appender.h) | `CardAppender` and `AppendFile`, which the storage driver's `CardFile` provides |
| [`logging/debug-log.h`](../../firmware/src/logging/debug-log.h) | `DEBUG_LOG()` and `sendDebugLog()` |
| [`logging/debug-output.h`](../../firmware/src/logging/debug-output.h) | `DebugOutput`, the buffer of debug lines behind them |
| [`logging/debug-trace.h`](../../firmware/src/logging/debug-trace.h) | `DEBUG_TRACE()`, the notes of interrupts |
| [`logging/log-line.h`](../../firmware/src/logging/log-line.h) | the form of a line |
