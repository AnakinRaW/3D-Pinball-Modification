class Display : public Driver {
public:
    using Sent = teensy::inplace_function<void(void), 16>;   // a lambda capturing a pointer fits

    // sets up FlexIO3 and the panel, leaves the screen black and attaches to the driver tick;
    // false when the tick is full
    bool begin() override;
    void restart();                                          // sets the panel up again after a supervisor reset, holding rectangles back until it is; returns at once
    bool restarting() const;                                 // gets whether a restart still holds the bus

    // sends the rectangle at x, y from pixels, RGB565 high byte first, and returns at once; calls
    // sent from the interrupt once the last pixel is out; false for a rectangle off the 16-pixel
    // steps, beyond the screen, or while one is on its way
    bool send(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t* pixels, Sent sent);

private:
    enum class Step : uint8_t { Idle, Reset, SleepOut };   // where a restart stands; the FlexIO3 interrupt alone steps it

    static constexpr uint16_t kWidth        = 320;
    static constexpr uint16_t kHeight       = 240;
    static constexpr uint8_t  kShifters     = 8;       // FlexIO3's shifters, one buffer of a burst
    static constexpr uint8_t  kDataLines    = 8;       // D0 to D7 on one shifter
    static constexpr uint8_t  kBurst        = 32;      // bytes in one burst, 16 pixels
    static constexpr uint8_t  kDivider      = 10;      // FlexIO3 clocks per write, 83.3 ns at 120 MHz
    static constexpr uint8_t  kFirstData    = 8;       // the FlexIO3 signal of D0
    static constexpr uint8_t  kWr           = 6;       // the FlexIO3 signal of WR
    static constexpr uint8_t  kPriority     = 224;     // the FlexIO3 row in general-design.md
    static constexpr uint32_t kTickUs       = 40000;   // the driver's period on the driver tick, which steps a restart
    static constexpr uint32_t kPanelReadyMs = 285;     // after power-up, docs/parts/display
    static constexpr uint32_t kResetMs      = 120;     // after a software reset, which a restart may send to an awake panel
    static constexpr uint32_t kSleepOutMs   = 120;     // after Sleep Out
    static constexpr uint32_t kLastFlag     = 1 << (kShifters - 1);   // shifter 7, whose data starts a burst
    static constexpr uint32_t kEndWaitNs    = 3000;    // from the start of the last burst to its end, with a reserve over the burst
    static constexpr uint8_t  kCs = 28;     // pin-assignment.md: CS to the display controller
    static constexpr uint8_t  kDc = 29;     // pin-assignment.md: D/C to the display
    static constexpr uint8_t  kBus[] = {22, 23, 20, 21, 38, 39, 26, 27, 17};  // pin-assignment.md: D0 to the display, D1 to the display, D2 to the display, D3 to the display, D4 to the display, D5 to the display, D6 to the display, D7 to the display, WR to the display

    // ... the members the implementation below names
};

// FlexIO3 at 120 MHz: PLL3's 480 MHz divided by 2 and by 2. The pads drive at their weakest
void Display::setUpBus() {
    CCM_CSCMR2 = (CCM_CSCMR2 & ~CCM_CSCMR2_FLEXIO2_CLK_SEL(3)) | CCM_CSCMR2_FLEXIO2_CLK_SEL(3);
    CCM_CS1CDR = (CCM_CS1CDR & ~(CCM_CS1CDR_FLEXIO2_CLK_PRED(7) | CCM_CS1CDR_FLEXIO2_CLK_PODF(7)))
               | CCM_CS1CDR_FLEXIO2_CLK_PRED(1) | CCM_CS1CDR_FLEXIO2_CLK_PODF(1);
    CCM_CCGR7 |= CCM_CCGR7_FLEXIO3(CCM_CCGR_ON);
    for (const uint8_t pin : kBus) {
        *portConfigRegister(pin)  = 9;                                    // ALT9, FlexIO3
        *portControlRegister(pin) = IOMUXC_PAD_DSE(1) | IOMUXC_PAD_SPEED(0);   // 157 ohms, slow slew
    }
    for (const uint8_t pin : {kCs, kDc}) {
        pinMode(pin, OUTPUT);
        *portControlRegister(pin) = IOMUXC_PAD_DSE(1) | IOMUXC_PAD_SPEED(0);
    }
    digitalWriteFast(kCs, LOW);                                           // the panel is the bus's only device
}

// the eight shifters chained into one buffer, and timer 0 clocking a burst of 32 writes
void Display::bursts() {
    IMXRT_FLEXIO_t& io = IMXRT_FLEXIO3_S;
    io.CTRL = 0;                                                          // off, and FASTACC stays off
    for (int i = 0; i < kShifters; ++i) {
        io.SHIFTCFG[i] = FLEXIO_SHIFTCFG_INSRC | FLEXIO_SHIFTCFG_PWIDTH(kDataLines - 1);
        io.SHIFTCTL[i] = FLEXIO_SHIFTCTL_TIMSEL(0) | FLEXIO_SHIFTCTL_SMOD(2)
                       | (i == 0 ? FLEXIO_SHIFTCTL_PINCFG(3) | FLEXIO_SHIFTCTL_PINSEL(kFirstData) : 0);
    }
    io.TIMCMP[0] = (kBurst * 2 - 1) << 8 | (kDivider / 2 - 1);
    io.TIMCFG[0] = FLEXIO_TIMCFG_TIMDIS(2) | FLEXIO_TIMCFG_TIMENA(2);    // a burst on data, its end on the compare
    io.TIMCTL[0] = FLEXIO_TIMCTL_TRGSEL((kShifters - 1) << 2 | 1) | FLEXIO_TIMCTL_TRGPOL
                 | FLEXIO_TIMCTL_TRGSRC | FLEXIO_TIMCTL_PINCFG(3) | FLEXIO_TIMCTL_PINSEL(kWr)
                 | FLEXIO_TIMCTL_PINPOL | FLEXIO_TIMCTL_TIMOD(1);         // WR active low, from shifter 7's flag
    io.CTRL = FLEXIO_CTRL_FLEXEN;
}

// shifter 0 alone and one write per trigger, for the commands; bursts() sets the chain up again
void Display::single();

// sends a command and its parameters one byte at a time, D/C low for the command; the
// interrupt calls it at the start of a run after single(), and each wait lasts one write
void Display::command(uint8_t cmd, const uint8_t* params, uint8_t count);

// the power, frame rate and gamma commands the ILI9341 libraries send after a reset
void Display::startUpTable();

// fills the whole panel with black one write at a time, since the driver holds no buffer; begin()
// only, before the interrupt runs, and as long as a full screen takes on the bus
void Display::blackScreen();

// asks the FlexIO3 interrupt for the panel's setup: the main loop is the only writer of restarts_
void Display::restart() {
    restarts_ = restarts_ + 1;
    NVIC_SET_PENDING(IRQ_FLEXIO3);
}

// sends what follows the software reset up to Sleep Out, for begin() and a restart alike
void Display::setUp() {
    startUpTable();                                              // the ILI9341 libraries' power and gamma table
    const uint8_t rgb565 = 0x55, landscape = 0x28;
    command(0x3A, &rgb565, 1);                                   // COLMOD, 16 bits a pixel
    command(0x36, &landscape, 1);                                // MADCTL, 320 across, matched to the mounting
    command(0x11, nullptr, 0);                                   // Sleep Out
}

// takes the next step of a restart between two rectangles: the software reset, the setup once
// the panel's 120 ms after it have passed, and Display On once its 120 ms after Sleep Out have
// passed. The driver tick raises the interrupt while a step waits. Gets whether the restart still
// holds the bus, which keeps a rectangle that comes meanwhile waiting
bool Display::stepRestart() {
    if (step_ == Step::Idle) {
        if (restarts_ == restartsTaken_) return false;
        restartsTaken_ = restarts_;
        single();
        command(0x01, nullptr, 0);                               // software reset
        stepAt_ = nowUs();
        step_   = Step::Reset;
        return true;
    }
    const uint32_t waited = nowUs() - stepAt_;
    if (step_ == Step::Reset) {
        if (waited < kResetMs * 1000) return true;
        setUp();
        stepAt_ = nowUs();
        step_   = Step::SleepOut;
        return true;
    }
    if (waited < kSleepOutMs * 1000) return true;
    command(0x29, nullptr, 0);                                   // Display On
    bursts();
    step_ = Step::Idle;
    return false;
}

// from the request until Display On; the screen runs no LVGL meanwhile
bool Display::restarting() const {
    return restarts_ != restartsTaken_ || step_ != Step::Idle;
}

// the driver tick, every kTickUs: the next step of a restart
void Display::tick() {
    if (step_ != Step::Idle) NVIC_SET_PENDING(IRQ_FLEXIO3);      // the interrupt takes the step
}

// sets the panel's window to the rectangle, starts the memory write and loads the first burst;
// the main loop is the only caller, and sending_ keeps it from a second rectangle
bool Display::send(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t* pixels, Sent sent) {
    if (sending_ || x % (kBurst / 2) || width % (kBurst / 2) || width == 0 || height == 0
        || x + width > kWidth || y + height > kHeight) return false;
    sending_ = true;
    at_      = pixels;
    left_    = (uint32_t)width * height * 2 / kBurst;            // whole bursts, since the width is
    sent_    = sent;
    NVIC_SET_PENDING(IRQ_FLEXIO3);                              // the interrupt sets the window and starts
    return true;
}

// writes the next 32 bytes into the shifter buffers, shifter 7 last since it starts the burst
void Display::refill() {
    const uint32_t* words = reinterpret_cast<const uint32_t*>(at_);
    IMXRT_FLEXIO_t& io = IMXRT_FLEXIO3_S;
    for (int i = 0; i < kShifters; ++i) io.SHIFTBUF[i] = words[i];
    at_ += kBurst;
}

// the FlexIO3 interrupt: starts a rectangle, refills it, or ends it after its last burst and
// calls the caller's function
void Display::isr() {
    IMXRT_FLEXIO_t& io = IMXRT_FLEXIO3_S;
    if (!running_) {
        if (stepRestart()) return;                               // the panel's setup holds the bus until it is back
        if (!sending_) return;
        window();                                                // single(), the window, Memory Write, bursts()
        running_ = true;
    }
    if (!(io.SHIFTSTAT & kLastFlag)) return;                     // the buffers still wait for their burst
    if (left_ > 0) {
        refill();
        left_ = left_ - 1;
        io.SHIFTSIEN = kLastFlag;
        return;
    }
    io.SHIFTSIEN = 0;
    delayNanoseconds(kEndWaitNs);                                // the last burst has left the bus after this
    running_ = false;
    sending_ = false;
    if (sent_) sent_();                                          // LVGL's flush_ready, which the screen hands in
}

bool Display::begin() {
    setUpBus();
    while (nowUs() < kPanelReadyMs * 1000) {}                    // the supervisor's reset is over
    single();
    command(0x01, nullptr, 0);                                   // software reset
    delay(kResetMs);
    setUp();
    delay(kSleepOutMs);
    blackScreen();                                               // the panel's memory holds noise after power-up
    command(0x29, nullptr, 0);                                   // Display On
    bursts();
    self_ = this;
    attachInterruptVector(IRQ_FLEXIO3, [] { self_->isr(); });
    NVIC_SET_PRIORITY(IRQ_FLEXIO3, kPriority);
    NVIC_ENABLE_IRQ(IRQ_FLEXIO3);
    return driverTick.attach([this] { tick(); }, kTickUs);
}
