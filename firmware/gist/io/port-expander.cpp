namespace io {

// the eight pins E-1 to E-8, which the MCP23008 on wireBus reads
class PortExpander {
public:
    // what a driver gets for every change on its pins: the pins that changed, bit n for E-(n+1),
    // every pin's level after the change, 1 for high, and the moment INT fell
    using Change = teensy::inplace_function<void(uint8_t changed, uint8_t levels, uint32_t time), 16>;

    static constexpr uint8_t kPins = 8;   // E-1 to E-8

    // calls fn in the bus's interrupt for every change on the pins in mask, and sets the port
    // expander up at the first call; before the main loop only. False when the MCP23008 does not
    // answer or another driver holds a pin in mask
    bool attach(uint8_t mask, Change fn) {
        if (!setUpDone_) {
            setUpDone_ = true;
            answers_   = setUp();
        }
        if (!answers_ || (mask & held_)) return false;
        noInterrupts();                                  // the bus's interrupt must not see a half-written slot
        attached_[count_] = Attached{mask, fn};
        count_ = count_ + 1;
        interrupts();
        held_ |= mask;
        return true;
    }

    uint8_t  levels() const   { return levels_; }     // gets every pin's level as the last read found it, 1 for high
    uint32_t lastRead() const { return lastRead_; }   // gets nowUs() at the end of the last read that worked
    uint32_t errors() const   { return errors_; }     // gets the controller's error flags of the last failed read

private:
    static constexpr uint8_t  kAddress = 0x20;      // A0 to A2 at GND, docs/parts/port-expander
    static constexpr uint8_t  kGpinten = 0x02;
    static constexpr uint8_t  kGppu    = 0x06;
    static constexpr uint8_t  kIntf    = 0x07;      // INTF, then INTCAP and GPIO, while IOCON keeps SEQOP at 0
    static constexpr uint8_t  kInt     = 1;         // pin-assignment.md: Interrupt from the port expander
    static constexpr uint32_t kPollUs  = 100000;    // a read every 100 ms without INT

    struct Attached { uint8_t mask; Change fn; };   // the pins one driver holds, and what it gets

    // switches on every pin's pull-up and interrupt-on-change, reads the pins once, arms the pin
    // interrupt on INT and attaches the read to the driver tick; false when the MCP23008 does not answer
    bool setUp() {
        // writes value into the register reg; false when the MCP23008 does not answer
        auto write = [this](uint8_t reg, uint8_t value) {
            command_[0]     = reg;
            command_[1]     = value;
            setup_.outCount = 2;
            setup_.inCount  = 0;
            return wireBus.requestAndWait(setup_) == 0;
        };
        if (!write(kGppu, 0xFF) || !write(kGpinten, 0xFF)) return false;
        command_[0]     = kIntf;                         // INTF, INTCAP and GPIO, which clears an INT a restart left low
        setup_.outCount = 1;
        setup_.inCount  = sizeof(regs_);
        if (wireBus.requestAndWait(setup_) != 0) return false;
        levels_   = regs_[2];                            // GPIO
        lastRead_ = nowUs();

        wireBus.attach(read_);
        pinMode(kInt, INPUT);                            // U1 drives INT push-pull
        attachInterrupt(digitalPinToInterrupt(kInt), [] { expander.fell(); }, FALLING);
        return driverTick.attach([this] { wireBus.request(read_); }, kPollUs);
    }

    // the pin interrupt on INT's falling edge: notes the time and requests a read
    void fell() {
        fellAt_ = nowUs();
        wireBus.request(read_);
    }

    // the end of a read, in the bus's interrupt: hands the changes INTCAP and GPIO show to the drivers
    void done(uint32_t errors) {
        if (errors) {                                    // INT stays low, and the tick's read repeats it
            errors_ = errors;
            return;
        }
        lastRead_ = nowUs();
        const uint8_t intf = regs_[0], intcap = regs_[1], gpio = regs_[2];
        if (intf) take(intcap, fellAt_);                 // the pins at the change, a short one included
        take(gpio, intf ? fellAt_ : lastRead_);          // the pins at the read
    }

    // takes the pins' levels at time t and calls every driver whose pins changed
    void take(uint8_t levels, uint32_t t) {
        const uint8_t changed = levels ^ levels_;
        levels_ = levels;
        for (uint8_t i = 0; i < count_; i++)
            if (changed & attached_[i].mask) attached_[i].fn(changed & attached_[i].mask, levels, t);
    }

    uint8_t           command_[2] = {};              // a register, and the value setup_ writes into it
    uint8_t           regs_[3]    = {};              // INTF, INTCAP and GPIO
    I2cTransfer       setup_{kAddress, command_, 2, regs_, 0, {}};
    I2cTransfer       read_{kAddress, &kIntf, 1, regs_, sizeof(regs_), [this](uint32_t e) { done(e); }};
    Attached          attached_[kPins];              // one driver for each pin at most
    volatile uint8_t  count_      = 0;
    uint8_t           held_       = 0;               // the pins the drivers hold
    bool              setUpDone_  = false;
    bool              answers_    = false;           // whether the set-up found the MCP23008
    volatile uint32_t fellAt_     = 0;               // when INT last fell
    volatile uint8_t  levels_     = 0xFF;            // the pins the last read found, 1 for high
    volatile uint32_t lastRead_   = 0;               // when a read last worked
    volatile uint32_t errors_     = 0;               // the error flags of the last failed read
};

extern PortExpander expander;   // defined in the main file

}
