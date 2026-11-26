// What the firmware's code takes from the Teensy core's Arduino.h, for the tests that run on the
// computer. Interrupts do not exist there, and Serial prints to the console. A pin keeps what the
// code did to it in fakePins, and a test calls the function attachInterrupt() took.
#pragma once
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

/// GPT2's counter, which nowUs() reads. A test sets it to the time it wants nowUs() to give.
inline volatile uint32_t GPT2_CNT = 0;

/// The milliseconds millis() gives. A test sets it to the time it wants.
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

// The pin levels, modes and interrupt modes stand as macros, as the Teensy core defines them, so a
// library that defines them as well finds them taken.
#ifndef LOW
#define LOW 0
#endif
#ifndef HIGH
#define HIGH 1
#endif
#ifndef INPUT
#define INPUT 0
#endif
#ifndef CHANGE
#define CHANGE 4
#endif

/// The pins of the Teensy 4.1, 0 to 54.
constexpr uint8_t FAKE_PINS = 55;

/// What the code under test did to a pin, and the level a test gives it.
struct FakePin {
    uint8_t level = LOW;       // what digitalReadFast() gives, set by a test
    int mode = -1;             // the last pinMode(), -1 before any
    uint32_t pad = 0;          // the pad's control register, which pinMode() writes anew
    void (*isr)() = nullptr;   // the function attachInterrupt() took
    int interruptMode = -1;    // the mode attachInterrupt() took, -1 before any
};
inline FakePin fakePins[FAKE_PINS];

/// The pad's hysteresis bit, as the Teensy core names it.
constexpr uint32_t IOMUXC_PAD_HYS = 1u << 16;

inline void pinMode(uint8_t pin, int mode) {
    fakePins[pin].mode = mode;
    fakePins[pin].pad = 0;
}

inline uint8_t digitalReadFast(uint8_t pin) {
    return fakePins[pin].level;
}

inline volatile uint32_t* portControlRegister(uint8_t pin) {
    return &fakePins[pin].pad;
}

inline uint8_t digitalPinToInterrupt(uint8_t pin) {
    return pin;
}

/// Takes the function and the mode, and turns the pad's hysteresis on, as the Teensy core does.
inline void attachInterrupt(uint8_t pin, void (*isr)(), int mode) {
    fakePins[pin].pad |= IOMUXC_PAD_HYS;
    fakePins[pin].isr = isr;
    fakePins[pin].interruptMode = mode;
}

/// Serial prints printf() to the console. write() collects its bytes in sent. It takes no more than
/// room bytes, as USB serial takes no more than its free buffers.
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
