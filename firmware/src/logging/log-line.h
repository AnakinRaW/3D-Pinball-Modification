// The form of a line in the machine's log and in the debug output: the milliseconds since the
// start, a tag that names who wrote it and the text, formatted like printf.
#pragma once
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

// The longest line with its line break. A longer line is cut to this length.
constexpr size_t LOG_LINE_BYTES = 160;

// Formats one line, such as "73021 W lamps no lamp 12\n". Gets its length.
// out takes LOG_LINE_BYTES bytes. The line ends in one line break and in no 0 byte.
// ms is the time in front of the line, such as millis().
// tag stands between the time and the text. nullptr leaves it out.
// format and args are formatted like vprintf. Line breaks at the end of the text are replaced by
// the line's one break. A line longer than LOG_LINE_BYTES ends in "...\n" in its last bytes.
size_t formatLogLine(char* out, uint32_t ms, const char* tag, const char* format, va_list args)
    __attribute__((format(printf, 4, 0)));
