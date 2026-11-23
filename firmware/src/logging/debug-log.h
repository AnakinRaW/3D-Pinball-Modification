// The debug output over USB serial. DEBUG_LOG() writes a line in a debug build. In a release build
// the compiler removes every DEBUG_LOG() call, so no output stays behind in the firmware for the
// machine.
// A line goes into a buffer and leaves it from the main loop, so no task ever waits for USB.
// Lines from two tasks never mix.
#pragma once
#include <stddef.h>
#include <stdint.h>

#if defined(DEBUG) && defined(NDEBUG)
#error "DEBUG and NDEBUG are both defined"
#endif

/// Writes one line of debug output, formatted like printf, with the milliseconds since the start
/// and the letter D in front.
///
/// A line break at the end of the text may be left out. A line from an interrupt is dropped. When
/// the buffer is full, the oldest waiting lines give way to the new one. Does nothing in a release
/// build.
///
/// @param format  The format of the line, as for printf, followed by the values it takes.
/// @note          Any task may call it, before and after FreeRTOS starts. It never waits.
void debugLog(const char* format, ...) __attribute__((format(printf, 1, 2)));

/// Adds a line that is formatted already, such as a line of the machine's log.
///
/// Otherwise as debugLog().
///
/// @param line  The line with its line break.
/// @param n     The length of the line.
void debugLogLine(const char* line, size_t n);

/// Turns the waiting notes of DEBUG_TRACE() into lines, then sends as many waiting lines to USB
/// serial as it takes without waiting.
///
/// Does nothing in a release build.
///
/// @return  The count of bytes sent.
/// @note    The main loop calls it in every pass. Never from an interrupt.
size_t sendDebugLog();

#if defined(DEBUG)
#define DEBUG_LOG(...) debugLog(__VA_ARGS__)
#else
// The call stands behind if (false), so its arguments stay checked and count as used. The
// compiler removes it.
#define DEBUG_LOG(...)                                                                             \
    do {                                                                                           \
        if (false) {                                                                               \
            debugLog(__VA_ARGS__);                                                                 \
        }                                                                                          \
    } while (0)
#endif
