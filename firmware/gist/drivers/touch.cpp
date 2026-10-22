// where a finger touches the screen, in the picture's pixels
struct Finger {
    bool     down;              // a finger touches the screen
    uint16_t x;                 // 0 at the left edge
    uint16_t y;                 // 0 at the top edge
};

class Touch : public Driver {
public:
    // attaches its transfer to the bus on Wire2, checks once whether the touch controller answers,
    // attaches to the driver tick and registers with the device monitor; false when one of the two
    // is full
    bool begin() override;

    bool   present() const;     // gets whether the machine has a touch controller, which begin() finds out once
    Finger finger() const;      // gets where a finger touches the screen

    // gets which parts have failed, bit 0 once the controller that answered at begin() has brought
    // no read for kFailMs, with the LPI2C4 error flags of the last failed read as the error code
    Fault failed() const override;

private:
    static constexpr uint32_t kTickUs       = 20000;   // the driver's period on the driver tick
    static constexpr uint8_t  kTouchBytes   = 5;       // TD_STATUS and the first touch point
    static constexpr uint8_t  kSlop         = 10;      // pixels a finger moves before TouchMoved
    static constexpr uint8_t  kHoldTicks    = 50;      // ticks a finger rests before TouchHeld, 1 s
    static constexpr uint32_t kFailMs       = 100;     // 100 ms without a working read, five reads, and the controller has failed
    static constexpr uint32_t kTouchReadyMs = 580;     // after power-up, docs/parts/display
    static constexpr uint8_t  kTouchAddress = 0x38;    // the CST026
    static constexpr uint8_t  kTdStatus     = 0x02;    // the number of touches, then the first touch point

    // the end of a read, in the bus's interrupt: the touch events from the five bytes
    void done(uint32_t errors);

    // publishes TouchDown, TouchMoved, TouchHeld and TouchUp from a read; turns the position from
    // the panel's portrait into the picture's landscape, as the display driver's MADCTL turns the
    // panel's memory
    void report(const uint8_t* read);

    uint8_t     read_[kTouchBytes] = {};
    io::I2cTransfer probe_{kTouchAddress, nullptr, 0, nullptr, 0, {}};   // the address alone
    io::I2cTransfer touch_{kTouchAddress, &kTdStatus, 1, read_, kTouchBytes, [this](uint32_t e) { done(e); }};
    // ... the other members the implementation below names
};

bool Touch::begin() {
    touchOut_ = &events.attach(touchQueue_, kQueueDepth);        // the touch events, which the reads publish
    io::wire2Bus.attach(probe_);
    io::wire2Bus.attach(touch_);
    while (nowUs() < kTouchReadyMs * 1000) {}
    present_  = io::wire2Bus.requestAndWait(probe_) == 0;            // the controller acknowledges its address
    lastGood_ = nowUs();

    if (!driverTick.attach([this] { if (present_) io::wire2Bus.request(touch_); }, kTickUs)) return false;
    return deviceMonitor.watch(Device::Touch, *this);
}

void Touch::done(uint32_t errors) {
    if (errors) {                                                // no answer, lost bus, FIFO error, line low
        lastError_ = errors;                                     // the next tick requests a new read
        return;
    }
    lastGood_ = nowUs();
    report(read_);                                               // TouchDown, TouchMoved, TouchHeld, TouchUp
}

Fault Touch::failed() const {
    return {present_ && elapsedMs(lastGood_, kFailMs), lastError_};
}
