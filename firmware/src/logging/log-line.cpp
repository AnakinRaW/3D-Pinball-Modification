#include "log-line.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

constexpr char CUT_MARK[] = "...\n";
constexpr size_t CUT_BYTES = sizeof(CUT_MARK) - 1;

size_t formatLogLine(char* out, uint32_t ms, const char* tag, const char* format, va_list args) {
    // One byte more than the line, for the 0 byte that snprintf always writes.
    char text[LOG_LINE_BYTES + 1];
    int n = tag != nullptr ? snprintf(text, sizeof(text), "%" PRIu32 " %s ", ms, tag)
                           : snprintf(text, sizeof(text), "%" PRIu32 " ", ms);
    size_t length = n < 0 ? 0 : static_cast<size_t>(n);
    bool cut = length >= sizeof(text);
    if (!cut) {
        n = vsnprintf(text + length, sizeof(text) - length, format, args);
        if (n > 0) {
            cut = length + static_cast<size_t>(n) >= sizeof(text);
            length = cut ? sizeof(text) - 1 : length + static_cast<size_t>(n);
        }
    }
    if (cut) {
        memcpy(out, text, LOG_LINE_BYTES - CUT_BYTES);
        memcpy(out + LOG_LINE_BYTES - CUT_BYTES, CUT_MARK, CUT_BYTES);
        return LOG_LINE_BYTES;
    }
    while (length > 0 && (text[length - 1] == '\n' || text[length - 1] == '\r')) {
        length = length - 1;
    }
    // A line that fits leaves room for its break, since text holds one byte more than a line.
    if (length == LOG_LINE_BYTES) {
        memcpy(out, text, LOG_LINE_BYTES - CUT_BYTES);
        memcpy(out + LOG_LINE_BYTES - CUT_BYTES, CUT_MARK, CUT_BYTES);
        return LOG_LINE_BYTES;
    }
    memcpy(out, text, length);
    out[length] = '\n';
    return length + 1;
}
