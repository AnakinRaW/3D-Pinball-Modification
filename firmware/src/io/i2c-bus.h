// Lets several drivers share one I²C port of the Teensy 4.1 without waiting for it. Each driver
// hands its transfers to the bus and is called back from the bus's interrupt once a transfer has
// ended, so a driver that reads a sensor costs the processor only the moments the controller asks
// for its next byte.
//
// wireBus runs Wire and wire2Bus runs Wire2. Wire sets up the pins, their pull-ups and the 100 kHz
// clock at the first attach(). From then on the bus alone programs the port's controller.
//
// Design: docs/firmware/io/i2c-bus.md.
#pragma once
#include <Arduino.h>
#include <IntervalTimer.h>
#include <Wire.h>

namespace io {

/// One exchange with one device: bytes written to it, then bytes read from it.
class I2cTransfer {
public:
    using Done =
        teensy::inplace_function<void(uint32_t errors), 16>;   // a lambda capturing this fits

    /// Prepares a transfer, which the bus runs once per request.
    ///
    /// A transfer with neither bytes to write nor bytes to read writes the address alone, which
    /// tells whether the device answers.
    ///
    /// @param address   The device's 7-bit address.
    /// @param out       The bytes written first, such as a register address. They stay in place.
    /// @param outCount  The number of bytes written, 0 for a read alone.
    /// @param in        Takes the bytes read.
    /// @param inCount   The number of bytes read, 0 for a write alone.
    /// @param done      Runs in the bus's interrupt when the transfer has ended, with the
    ///                  controller's error flags, which are 0 when it worked. It has to be short
    ///                  and must never wait.
    I2cTransfer(uint8_t address, const uint8_t* out, uint8_t outCount, uint8_t* in, uint8_t inCount,
                Done done)
        : address(address), out(out), outCount(outCount), in(in), inCount(inCount), done(done) {}

private:
    friend class I2cBus;

    const uint8_t address;
    const uint8_t* const out;
    const uint8_t outCount;
    uint8_t* const in;
    const uint8_t inCount;
    const Done done;

    bool attached = false;
    I2cTransfer* next = nullptr;       // the transfer attached after this one
    volatile bool requested = false;   // set by request(), cleared when the transfer starts
    volatile bool running = false;
    volatile uint32_t errors = 0;   // the error flags the transfer last ended with
};

/// One I²C port that runs the transfers of every driver on it, one after another, in its interrupt.
class I2cBus {
public:
    /// Describes the port a bus runs.
    ///
    /// @param wire  The Wire object that sets up the pins and the clock.
    /// @param port  The port's controller.
    /// @param irq   The controller's interrupt.
    /// @param sda   The Teensy pin of SDA.
    /// @param scl   The Teensy pin of SCL.
    /// @param isr   The handler of @p irq, which calls serve().
    I2cBus(TwoWire& wire, IMXRT_LPI2C_t& port, IRQ_NUMBER_t irq, uint8_t sda, uint8_t scl,
           void (*isr)())
        : wire(wire), port(port), irq(irq), sda(sda), scl(scl), isr(isr) {}

    /// Adds a transfer that the bus runs on request.
    ///
    /// The first call sets the bus up. A transfer attached twice stays attached once.
    ///
    /// @param t  The transfer. It stays in place for as long as the firmware runs.
    /// @note     Before the main loop only.
    void attach(I2cTransfer& t);

    /// Asks the bus to run a transfer once and then call its done function.
    ///
    /// A request for a transfer that is still running makes the bus run it once more afterwards,
    /// so the next run reads what changed meanwhile. Several requests during one run add one run.
    /// A transfer that was never attached is ignored.
    ///
    /// @param t  An attached transfer.
    /// @note     Returns at once. Any interrupt and any task may call it.
    void request(I2cTransfer& t);

    /// Attaches a transfer where it is not yet attached, runs it and waits until it has ended.
    ///
    /// @param t  The transfer.
    /// @return   The controller's error flags, 0 when the transfer worked.
    /// @note     Before the main loop only.
    uint32_t requestAndWait(I2cTransfer& t);

    /// Moves the running transfer on, ends it, and starts the next requested one.
    ///
    /// @note  The port's interrupt calls it, and nothing else does.
    void serve();

private:
    void setUp();
    void startNext();
    void start(I2cTransfer& t);
    void feed();
    void drain();
    void finish(uint32_t errors);
    void recover();
    uint32_t fifoWords() const;

    /// Gets the command word of step @p step of a transfer, or NO_COMMAND past its STOP.
    ///
    /// @param step  The step, counted from 0 at the first START.
    static uint32_t command(const I2cTransfer& t, uint32_t step);

    TwoWire& wire;
    IMXRT_LPI2C_t& port;
    const IRQ_NUMBER_t irq;
    const uint8_t sda;
    const uint8_t scl;
    void (*const isr)();
    I2cTransfer* first = nullptr;   // the attached transfers, in order
    I2cTransfer* last = nullptr;
    I2cTransfer* ran = nullptr;       // the transfer started last
    I2cTransfer* current = nullptr;   // the transfer on the bus
    uint32_t step = 0;                // the next command of current
    uint8_t got = 0;                  // the bytes of current read so far
};

extern I2cBus wireBus;    // the bus on Wire, run by LPI2C1
extern I2cBus wire2Bus;   // the bus on Wire2, run by LPI2C4

}   // namespace io
