#include "i2c-bus.h"
#include "logging/debug-trace.h"

namespace io {

constexpr uint8_t PRIORITY = 160;   // general-design.md: the I²C buses' row
/// RM LPI2C, MSR: every flag the master clears by writing 1, as Wire clears them
constexpr uint32_t FLAGS = 0x00007F00;
/// RM LPI2C, MSR: no acknowledge, lost arbitration, a FIFO error and a line held low past the
/// timeout Wire configures
constexpr uint32_t ERRORS = LPI2C_MSR_NDF | LPI2C_MSR_ALF | LPI2C_MSR_FEF | LPI2C_MSR_PLTF;
constexpr uint32_t ERROR_IRQS =
    LPI2C_MIER_NDIE | LPI2C_MIER_ALIE | LPI2C_MIER_FEIE | LPI2C_MIER_PLTIE;
constexpr uint32_t NO_COMMAND =
    0xFFFFFFFF;   // past a transfer's STOP, which no command word equals
/// RM LPI2C, PARAM: MTXFIFO, the transmit FIFO's size as a power of two
constexpr uint32_t PARAM_TX_FIFO = 0x0F;
/// RM LPI2C, MFSR: TXCOUNT in bits 2 to 0 and RXCOUNT in bits 18 to 16
constexpr uint32_t COUNT_MASK = 0x07;
constexpr uint32_t RX_COUNT_SHIFT = 16;
/// RM IOMUXC: ALT5 is the GPIO, with SION set as Wire's force_clock() sets it
constexpr uint32_t MUX_GPIO = 5 | 0x10;
/// UM10204: a device that stopped in a byte lets go of SDA within its eight bits and the
/// acknowledge
constexpr uint8_t RECOVERY_CLOCKS = 9;
constexpr uint32_t RECOVERY_HALF_US = 5;      // half a clock at 100 kHz, as Wire's force_clock()
constexpr uint32_t RECOVERY_SETTLE_US = 10;   // the lines settle on the GPIO, as in force_clock()
constexpr uint8_t WIRE_SDA = 18;              // pin-assignment.md: SDA on Wire
constexpr uint8_t WIRE_SCL = 19;              // pin-assignment.md: SCL on Wire
constexpr uint8_t WIRE2_SDA = 25;             // pin-assignment.md: SDA to the touch controller
constexpr uint8_t WIRE2_SCL = 24;             // pin-assignment.md: SCL to the touch controller

void I2cBus::attach(I2cTransfer& t) {
    if (t.attached) {
        return;
    }
    if (first == nullptr) {
        setUp();
    }
    t.attached = true;
    t.next = nullptr;
    // One store links the transfer in, so the interrupt never sees half of it.
    if (last != nullptr) {
        last->next = &t;
    } else {
        first = &t;
    }
    last = &t;
}

void I2cBus::request(I2cTransfer& t) {
    if (!t.attached) {
        DEBUG_TRACE("i2c unattached", t.address, 0);
        return;
    }
    t.requested = true;
    // The interrupt starts the transfer once the bus is free, so only the interrupt touches the
    // controller.
    NVIC_SET_PENDING(irq);
}

uint32_t I2cBus::requestAndWait(I2cTransfer& t) {
    attach(t);
    request(t);
    while (t.requested || t.running) {
    }
    return t.errors;
}

void I2cBus::serve() {
    const uint32_t msr = port.MSR;
    if (current != nullptr) {
        if ((msr & ERRORS) != 0) {
            if ((msr & LPI2C_MSR_PLTF) != 0) {
                recover();
            }
            // The transfer's leftovers leave the FIFOs. Wire sends a STOP after every error but a
            // FIFO error, after which it found a STOP harmful.
            port.MCR |= LPI2C_MCR_RTF | LPI2C_MCR_RRF;
            if ((msr & LPI2C_MSR_FEF) == 0) {
                port.MTDR = LPI2C_MTDR_CMD_STOP;
            }
            finish(msr & ERRORS);
        } else {
            drain();
            feed();
            // The STOP has gone out, so every byte has arrived and drain() has taken it.
            if ((msr & LPI2C_MSR_SDF) != 0) {
                finish(0);
            }
        }
    }
    if (current != nullptr) {
        return;
    }
    if ((port.MSR & LPI2C_MSR_MBF) != 0) {
        // A STOP still goes out, and its end calls again.
        port.MIER = LPI2C_MIER_SDIE;
        return;
    }
    startNext();
}

/// Lets Wire set up the pins, their pull-ups, the 100 kHz clock and the timeout of a line held low,
/// then takes the controller over with its interrupt.
void I2cBus::setUp() {
    wire.begin();
    // RM LPI2C, MFCR: a free command slot raises TDF, and every received byte raises RDF.
    port.MCR = 0;
    port.MFCR = LPI2C_MFCR_TXWATER(fifoWords() - 1) | LPI2C_MFCR_RXWATER(0);
    port.MIER = 0;
    port.MSR = FLAGS;
    port.MCR = LPI2C_MCR_MEN;
    attachInterruptVector(irq, isr);
    NVIC_SET_PRIORITY(irq, PRIORITY);
    NVIC_ENABLE_IRQ(irq);
}

uint32_t I2cBus::fifoWords() const {
    return 1u << (port.PARAM & PARAM_TX_FIFO);
}

uint32_t I2cBus::command(const I2cTransfer& t, uint32_t step) {
    // A transfer with nothing to read writes, even with nothing to write, so a probe sends its
    // address alone.
    if (t.outCount > 0 || t.inCount == 0) {
        if (step == 0) {
            return LPI2C_MTDR_CMD_START | uint32_t(t.address) << 1;
        }
        if (step <= t.outCount) {
            return LPI2C_MTDR_CMD_TRANSMIT | t.out[step - 1];
        }
        step -= t.outCount + 1;
    }
    if (t.inCount > 0) {
        if (step == 0) {
            return LPI2C_MTDR_CMD_START | uint32_t(t.address) << 1 | 1;
        }
        // RM LPI2C, MTDR: RECEIVE takes the count less one.
        if (step == 1) {
            return LPI2C_MTDR_CMD_RECEIVE | uint32_t(t.inCount - 1);
        }
        step -= 2;
    }
    return step == 0 ? LPI2C_MTDR_CMD_STOP : NO_COMMAND;
}

/// Starts the requested transfer that follows the one started last, so each transfer waits for at
/// most one run of every other.
void I2cBus::startNext() {
    I2cTransfer* t = ran;
    for (I2cTransfer* n = first; n != nullptr; n = n->next) {
        t = (t != nullptr && t->next != nullptr) ? t->next : first;
        if (t->requested) {
            start(*t);
            return;
        }
    }
    // Idle until a request raises the interrupt in software.
    port.MSR = FLAGS;
    port.MIER = 0;
}

void I2cBus::start(I2cTransfer& t) {
    t.running = true;
    // A request from here on runs the transfer once more.
    t.requested = false;
    current = &t;
    ran = &t;
    step = 0;
    got = 0;
    port.MSR = FLAGS;
    port.MIER =
        ERROR_IRQS | LPI2C_MIER_SDIE | LPI2C_MIER_TDIE | (t.inCount > 0 ? LPI2C_MIER_RDIE : 0);
    feed();
}

/// Writes the transfer's next commands while the FIFO has room.
void I2cBus::feed() {
    const uint32_t room = fifoWords();
    uint32_t c = command(*current, step);
    while (c != NO_COMMAND && (port.MFSR & COUNT_MASK) < room) {
        port.MTDR = c;
        ++step;
        c = command(*current, step);
    }
    if (c == NO_COMMAND) {
        port.MIER &= ~LPI2C_MIER_TDIE;
    }
}

void I2cBus::drain() {
    while (((port.MFSR >> RX_COUNT_SHIFT) & COUNT_MASK) > 0) {
        const uint8_t byte = uint8_t(port.MRDR);
        if (got < current->inCount) {
            current->in[got++] = byte;
        }
    }
}

void I2cBus::finish(uint32_t errors) {
    I2cTransfer& t = *current;
    current = nullptr;
    t.errors = errors;
    t.running = false;
    if (t.done) {
        t.done(errors);
    }
}

/// Clocks SCL until the device lets go of SDA, as Wire's force_clock() does.
///
/// The pads stay open-drain, as Wire set them, so a pin set high only lets go of its line.
void I2cBus::recover() {
    DEBUG_TRACE("i2c held low", sda, scl);
    const uint32_t sdaMux = *portConfigRegister(sda);
    const uint32_t sclMux = *portConfigRegister(scl);
    for (const uint8_t pin : {sda, scl}) {
        *portConfigRegister(pin) = MUX_GPIO;
        *portSetRegister(pin) = digitalPinToBitMask(pin);
        *portModeRegister(pin) |= digitalPinToBitMask(pin);
    }
    delayMicroseconds(RECOVERY_SETTLE_US);
    for (uint8_t i = 0; i < RECOVERY_CLOCKS && !(digitalReadFast(sda) && digitalReadFast(scl));
         ++i) {
        digitalWriteFast(scl, LOW);
        delayMicroseconds(RECOVERY_HALF_US);
        digitalWriteFast(scl, HIGH);
        delayMicroseconds(RECOVERY_HALF_US);
    }
    *portConfigRegister(sda) = sdaMux;
    *portConfigRegister(scl) = sclMux;
}

static void serveWire();
static void serveWire2();

I2cBus wireBus(Wire, IMXRT_LPI2C1, IRQ_LPI2C1, WIRE_SDA, WIRE_SCL, serveWire);
I2cBus wire2Bus(Wire2, IMXRT_LPI2C4, IRQ_LPI2C4, WIRE2_SDA, WIRE2_SCL, serveWire2);

static void serveWire() {
    wireBus.serve();
}

static void serveWire2() {
    wire2Bus.serve();
}

}   // namespace io
