// DEBUG_LOG() of firmware/CLAUDE.md: prints in a debug build, and the compiler removes it in a
// release build, so no Serial.print() stays behind in the firmware for the machine.
#pragma once
#include <Arduino.h>

#if defined(DEBUG) && defined(NDEBUG)
#error "DEBUG and NDEBUG are both defined"
#endif

#if defined(DEBUG)
#define DEBUG_LOG(...) Serial.printf(__VA_ARGS__)
#else
// The call stands behind if (false), so its arguments stay checked and count as used, and the
// compiler removes it.
#define DEBUG_LOG(...)                                                                             \
    do {                                                                                           \
        if (false) {                                                                               \
            Serial.printf(__VA_ARGS__);                                                            \
        }                                                                                          \
    } while (0)
#endif
