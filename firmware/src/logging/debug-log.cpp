#include "debug-log.h"
#include "logging/debug-output.h"
#include "logging/debug-trace.h"
#include "logging/log-line.h"
#include <Arduino.h>

#if defined(DEBUG)

// Lines waiting for USB. 16 KB hold a burst of a hundred lines and more while no host reads.
constexpr size_t DEBUG_BUFFER_BYTES = 16384;
// The trace notes one call of sendDebugLog() turns into lines. It bounds the main loop's pass.
constexpr uint32_t TRACES_PER_SEND = 32;

EXTMEM static uint8_t debugBuffer[DEBUG_BUFFER_BYTES];
static DebugOutput output(debugBuffer, sizeof(debugBuffer));

void debugLog(const char* format, ...) {
    va_list args;
    va_start(args, format);
    output.write(format, args);
    va_end(args);
}

void debugLogLine(const char* line, size_t n) {
    output.writeLine(line, n);
}

size_t sendDebugLog() {
    TraceRecord note;
    for (uint32_t i = 0; i < TRACES_PER_SEND && takeDebugTrace(note); ++i) {
        char line[LOG_LINE_BYTES];
        output.writeLine(line, formatTraceLine(line, millis(), note));
    }
    return output.send();
}

#else

void debugLog(const char*, ...) {}

void debugLogLine(const char*, size_t) {}

size_t sendDebugLog() {
    return 0;
}

#endif
