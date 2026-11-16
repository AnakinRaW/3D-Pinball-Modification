// What the firmware's code takes from the Teensy core's Arduino.h, for the tests that run on the
// computer. Interrupts do not exist there, and Serial prints to the console.
#pragma once
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>

// GPT2's counter, which nowUs() reads. A test sets it to the time it wants nowUs() to give.
inline volatile uint32_t GPT2_CNT = 0;

inline void noInterrupts() {}
inline void interrupts() {}

struct FakeSerial {
    __attribute__((format(printf, 2, 3))) int printf(const char* format, ...) {
        va_list args;
        va_start(args, format);
        const int written = std::vprintf(format, args);
        va_end(args);
        return written;
    }
};
inline FakeSerial Serial;
