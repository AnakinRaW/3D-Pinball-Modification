namespace io {

// a transfer on an I²C bus: bytes written to one device, then bytes read from it
struct I2cTransfer {
    using Done = teensy::inplace_function<void(uint32_t errors), 16>;   // a lambda capturing this fits

    uint8_t        address;    // the device's address
    const uint8_t* out;        // the bytes written first, such as a register address
    uint8_t        outCount;   // the number of bytes written, 0 for a read alone
    uint8_t*       in;         // where the bytes read go
    uint8_t        inCount;    // the number of bytes read, 0 for a write alone
    Done           done;       // called in the bus's interrupt when the transfer has ended, with the
                               // controller's error flags, 0 when it worked

    // kept by the bus
    bool              attached_  = false;
    I2cTransfer*      next_      = nullptr;   // the next attached transfer
    volatile bool     requested_ = false;     // set by request(), cleared when the transfer starts
    volatile bool     running_   = false;
    volatile uint32_t errors_    = 0;         // the error flags the transfer last ended with
};

// an I²C port that runs the transfers of every driver on its bus, one after another, in its interrupt
class I2cBus {
public:
    // the bus on wire, run by the controller port with the interrupt irq; sda and scl are its pins,
    // and isr is the handler that calls serve()
    I2cBus(TwoWire& wire, IMXRT_LPI2C_t& port, IRQ_NUMBER_t irq, uint8_t sda, uint8_t scl,
           void (*isr)())
        : wire_(wire), port_(port), irq_(irq), sda_(sda), scl_(scl), isr_(isr) {}

    // adds a transfer the bus runs on request, and sets the bus up at the first; before the main
    // loop only, where the main task calls the drivers' begin() one after another, so the set-up
    // needs no lock
    void attach(I2cTransfer& t) {
        if (!first_) setUp();
        t.attached_ = true;
        t.next_     = nullptr;
        if (last_) last_->next_ = &t;                    // one store links it, so the interrupt never sees half of it
        else first_ = &t;
        last_ = &t;
    }

    // asks the bus to run the transfer once and then call its done function; returns at once,
    // from any interrupt
    void request(I2cTransfer& t) {
        t.requested_ = true;
        NVIC_SET_PENDING(irq_);                          // the interrupt starts it once the bus is free
    }

    // attaches the transfer if it is not yet, runs it and waits until it has ended; before the main
    // loop only. Gets the controller's error flags, 0 when it worked
    uint32_t requestAndWait(I2cTransfer& t) {
        if (!t.attached_) attach(t);
        request(t);
        while (t.requested_ || t.running_) {}
        return t.errors_;
    }

    // the port's interrupt: moves the running transfer on, ends it and starts the next requested one
    void serve() {
        const uint32_t msr = port_.MSR;
        if (current_) {
            if (msr & kErrors) {                         // no answer, lost bus, FIFO error, line low
                if (msr & LPI2C_MSR_PLTF) recover();
                port_.MCR |= LPI2C_MCR_RTF | LPI2C_MCR_RRF;   // drops what the transfer left in the FIFOs
                port_.MTDR = LPI2C_MTDR_CMD_STOP;        // frees the bus, as Wire does
                finish(msr & kErrors);
            } else {
                drain();
                feed();
                if (msr & LPI2C_MSR_SDF) finish(0);      // the STOP has gone out, every byte is in
            }
        }
        if (current_) return;
        if (port_.MSR & LPI2C_MSR_MBF) {                 // a STOP is still going out
            port_.MIER = LPI2C_MIER_SDIE;                // its end calls again
            return;
        }
        startNext();
    }

private:
    static constexpr uint8_t  kPriority = 160;           // the I²C buses' row in general-design.md
    static constexpr uint32_t kFlags    = 0x00007F00;    // every flag the master clears by writing 1, as Wire clears them
    static constexpr uint32_t kErrors   = LPI2C_MSR_NDF | LPI2C_MSR_ALF | LPI2C_MSR_FEF | LPI2C_MSR_PLTF;
    static constexpr uint32_t kErrorIrqs = LPI2C_MIER_NDIE | LPI2C_MIER_ALIE | LPI2C_MIER_FEIE | LPI2C_MIER_PLTIE;
    static constexpr uint32_t kNone     = 0xFFFFFFFF;    // no command left, which no command word equals

    // sets up the pins, their pull-ups and the 100 kHz clock through Wire, then the port's interrupt
    void setUp() {
        wire_.begin();                                   // the pins, their pull-ups and 100 kHz
        port_.MCR  = 0;                                  // the master off while it is set up, as Wire does
        port_.MFCR = LPI2C_MFCR_TXWATER(fifo() - 1) | LPI2C_MFCR_RXWATER(0);   // a free command slot and every byte ask
        port_.MIER = 0;
        port_.MCR  = LPI2C_MCR_MEN;
        attachInterruptVector(irq_, isr_);
        NVIC_SET_PRIORITY(irq_, kPriority);
        NVIC_ENABLE_IRQ(irq_);
    }

    // gets how many words each of the controller's FIFOs holds, from its PARAM register
    uint32_t fifo() const { return 1u << (port_.PARAM & 0x0F); }
    uint32_t txCount() const { return port_.MFSR & 0x07; }
    uint32_t rxCount() const { return (port_.MFSR >> 16) & 0x07; }

    // gets the command word of step i of the transfer, or kNone past its STOP. A transfer with
    // neither part writes the address alone, which checks whether the device answers
    static uint32_t command(const I2cTransfer& t, uint32_t i) {
        if (t.outCount || !t.inCount) {
            if (i == 0) return LPI2C_MTDR_CMD_START | t.address << 1;
            if (i <= t.outCount) return LPI2C_MTDR_CMD_TRANSMIT | t.out[i - 1];
            i -= t.outCount + 1;
        }
        if (t.inCount) {
            if (i == 0) return LPI2C_MTDR_CMD_START | t.address << 1 | 1;
            if (i == 1) return LPI2C_MTDR_CMD_RECEIVE | (t.inCount - 1);
            i -= 2;
        }
        return i == 0 ? LPI2C_MTDR_CMD_STOP : kNone;
    }

    // starts the next requested transfer after the one that ran last, so every transfer gets its turn
    void startNext() {
        I2cTransfer* t = ran_;
        for (I2cTransfer* n = first_; n; n = n->next_) {    // once round the list at most
            t = (t && t->next_) ? t->next_ : first_;
            if (t->requested_) return start(*t);
        }
        port_.MSR  = kFlags;
        port_.MIER = 0;                                  // idle until a request raises the interrupt
    }

    void start(I2cTransfer& t) {
        t.running_   = true;
        t.requested_ = false;                            // a request from here on runs it once more
        current_ = ran_ = &t;
        step_ = got_ = 0;
        port_.MSR  = kFlags;                             // the flags the last transfer left
        port_.MIER = kErrorIrqs | LPI2C_MIER_SDIE | LPI2C_MIER_TDIE | (t.inCount ? LPI2C_MIER_RDIE : 0);
        feed();
    }

    // writes the transfer's next commands while the FIFO has room
    void feed() {
        uint32_t c;
        while (txCount() < fifo() && (c = command(*current_, step_)) != kNone) {
            port_.MTDR = c;
            step_++;
        }
        if (command(*current_, step_) == kNone) port_.MIER &= ~LPI2C_MIER_TDIE;
    }

    // moves the bytes received so far into the transfer
    void drain() {
        while (rxCount() && got_ < current_->inCount) current_->in[got_++] = port_.MRDR;
    }

    void finish(uint32_t errors) {
        I2cTransfer& t = *current_;
        current_   = nullptr;
        t.errors_  = errors;
        t.running_ = false;
        if (t.done) t.done(errors);
    }

    // clocks SCL until the device lets go of SDA, as Wire's force_clock() does. The pads stay
    // open-drain, as Wire set them, so a pin set high only lets go of its line
    void recover() {
        const uint32_t sdaMux = *portConfigRegister(sda_), sclMux = *portConfigRegister(scl_);
        for (const uint8_t pin : {sda_, scl_}) {
            *portConfigRegister(pin) = 5 | 0x10;          // GPIO
            *portSetRegister(pin)    = digitalPinToBitMask(pin);
            *portModeRegister(pin)  |= digitalPinToBitMask(pin);
        }
        delayMicroseconds(10);
        for (int i = 0; i < 9 && !(digitalReadFast(sda_) && digitalReadFast(scl_)); i++) {
            digitalWriteFast(scl_, LOW);
            delayMicroseconds(5);
            digitalWriteFast(scl_, HIGH);
            delayMicroseconds(5);
        }
        *portConfigRegister(sda_) = sdaMux;               // back to the controller
        *portConfigRegister(scl_) = sclMux;
    }

    TwoWire&        wire_;
    IMXRT_LPI2C_t&  port_;
    IRQ_NUMBER_t    irq_;
    uint8_t         sda_, scl_;
    void          (*isr_)();
    I2cTransfer*    first_   = nullptr;              // the attached transfers, in order
    I2cTransfer*    last_    = nullptr;
    I2cTransfer*    ran_     = nullptr;              // the transfer started last
    I2cTransfer*    current_ = nullptr;              // the transfer on the bus
    uint32_t        step_    = 0;                    // the next command of current_
    uint8_t         got_     = 0;                    // the bytes of current_ read so far
};

extern I2cBus wireBus;    // the bus on Wire, run by LPI2C1, defined in the main file
extern I2cBus wire2Bus;   // the bus on Wire2, run by LPI2C4, defined in the main file

}
