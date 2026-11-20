// What the firmware's code takes from the Teensy core's Arduino.h, for the tests that run on the
// computer. Interrupts do not exist there, and Serial prints to the console.
#pragma once
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

// GPT2's counter, which nowUs() reads. A test sets it to the time it wants nowUs() to give.
inline volatile uint32_t GPT2_CNT = 0;

// The milliseconds millis() gives. A test sets it to the time it wants.
inline uint32_t fakeMillis = 0;

inline uint32_t millis() {
    return fakeMillis;
}

// The computer has no RAM2 and no PSRAM, so a buffer that names them lies with the other
// variables.
#define DMAMEM
#define EXTMEM

inline void noInterrupts() {}
inline void interrupts() {}

// Serial prints printf() to the console. write() collects its bytes in sent. It takes no more than
// room bytes, as USB serial takes no more than its free buffers.
struct FakeSerial {
    std::string sent;
    int room = 1 << 20;

    int availableForWrite() { return room; }

    size_t write(const uint8_t* bytes, size_t n) {
        const size_t taken = room <= 0 ? 0 : (n < size_t(room) ? n : size_t(room));
        sent.append(reinterpret_cast<const char*>(bytes), taken);
        room = room - int(taken);
        return taken;
    }

    __attribute__((format(printf, 2, 3))) int printf(const char* format, ...) {
        va_list args;
        va_start(args, format);
        const int written = std::vprintf(format, args);
        va_end(args);
        return written;
    }
};
inline FakeSerial Serial;
