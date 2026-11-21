// A fake of the I²C bus for the tests that run on the computer. It keeps the transfers attached to
// it and counts the requests of each. A test ends a transfer with end(), which hands over the bytes
// read and the error flags, as the bus's interrupt would.
#pragma once
#include <Arduino.h>
#include <IntervalTimer.h>
#include <string.h>

/// RM LPI2C, MSR: no acknowledge, the flag a missing device leaves
constexpr uint32_t LPI2C_MSR_NDF = 1u << 10;

namespace io {

class I2cTransfer {
public:
    using Done = teensy::inplace_function<void(uint32_t errors), 16>;

    I2cTransfer(uint8_t address, const uint8_t* out, uint8_t outCount, uint8_t* in, uint8_t inCount,
                Done done)
        : address(address), out(out), outCount(outCount), in(in), inCount(inCount), done(done) {}

    const uint8_t address;
    const uint8_t* const out;
    const uint8_t outCount;
    uint8_t* const in;
    const uint8_t inCount;
    const Done done;

    bool attached = false;
    uint32_t requests = 0;   // request() and requestAndWait() calls since the test began
};

class I2cBus {
public:
    void attach(I2cTransfer& t) {
        t.attached = true;
        if (t.inCount > 0) {
            reading = &t;
        }
    }

    void request(I2cTransfer& t) {
        if (t.attached) {
            t.requests = t.requests + 1;
        }
    }

    /// Answers with waitErrors, as the bus answers a transfer once it has ended.
    uint32_t requestAndWait(I2cTransfer& t) {
        attach(t);
        request(t);
        return waitErrors;
    }

    /// Ends a transfer: copies @p bytes into what it reads and calls its done function.
    void end(I2cTransfer& t, const uint8_t* bytes, uint32_t errors) {
        if (errors == 0 && bytes != nullptr) {
            memcpy(t.in, bytes, t.inCount);
        }
        if (t.done) {
            t.done(errors);
        }
    }

    uint32_t waitErrors = 0;          // what requestAndWait() answers
    I2cTransfer* reading = nullptr;   // the transfer attached last that reads bytes
};

}   // namespace io
