// where a finger touches the screen, in the picture's pixels
struct Finger {
    bool     down;              // a finger touches the screen
    uint16_t x;                 // 0 at the left edge
    uint16_t y;                 // 0 at the top edge
};

class Touch : public Driver {
public:
    // sets up Wire2, checks once whether the touch controller answers, attaches to the driver tick
    // and registers with the device monitor; false when one of the two is full
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
    static constexpr uint32_t kErrors   = LPI2C_MSR_NDF | LPI2C_MSR_ALF | LPI2C_MSR_FEF | LPI2C_MSR_PLTF;

    // publishes TouchDown, TouchMoved, TouchHeld and TouchUp from a read; turns the position from
    // the panel's portrait into the picture's landscape, as the display driver's MADCTL turns the
    // panel's memory
    void report(const uint8_t* read);

    // ... the members the implementation below names
};

bool Touch::begin() {
    touchOut_ = &events.attach(touchQueue_, kQueueDepth);        // the touch events, which the driver tick publishes
    Wire2.begin();                                                       // the pins, their pull-ups and 100 kHz
    while (nowUs() < kTouchReadyMs * 1000) {}
    Wire2.beginTransmission(kTouchAddress);
    present_ = Wire2.endTransmission() == 0;
    LPI2C4_MCR     = 0;                                                  // the master off while it is set up, as Wire does
    LPI2C4_MCFGR1 |= LPI2C_MCFGR1_AUTOSTOP;                              // a STOP once the four commands are sent
    LPI2C4_MFCR    = LPI2C_MFCR_RXWATER(0) | LPI2C_MFCR_TXWATER(1);      // every received byte asks, Wire leaves every second
    LPI2C4_MCR     = LPI2C_MCR_MEN;
    LPI2C4_MDER    = LPI2C_MDER_RDDE;                                    // each received byte asks the DMA channel
    dma_.source((volatile uint8_t&)LPI2C4_MRDR);
    dma_.destinationBuffer(read_, kTouchBytes);
    dma_.triggerAtHardwareEvent(DMAMUX_SOURCE_LPI2C4);
    dma_.disableOnCompletion();                                          // five bytes, then the next tick enables it
    lastGood_ = nowUs();

    if (!driverTick.attach([this] { tick(); }, kTickUs)) return false;
    return deviceMonitor.watch(Device::Touch, *this);
}

// the driver tick, every kTickUs: collects the last read and starts the next
void Touch::tick() {
    if (!present_) return;
    const uint32_t msr = LPI2C4_MSR;
    if (msr & kErrors) {                                                 // no answer, lost bus, FIFO error, line low
        lastError_ = msr & kErrors;
        if (msr & LPI2C_MSR_PLTF) recover();                             // clocks SCL until SDA is free
        LPI2C4_MCR |= LPI2C_MCR_RTF | LPI2C_MCR_RRF;
        LPI2C4_MSR  = 0x00007F00;                                        // clears every flag, as Wire does
    } else if (msr & LPI2C_MSR_MBF) {
        return;                                                          // the last read is still on the bus
    } else if (dma_.complete()) {
        dma_.clearComplete();
        lastGood_ = nowUs();
        report(read_);                                                   // TouchDown, TouchMoved, TouchHeld, TouchUp
    }
    dma_.disable();                                                      // a read that broke off leaves the channel part-way,
    dma_.destinationBuffer(read_, kTouchBytes);                          // so every read starts again at the buffer's first byte
    dma_.enable();                                                       // the next five bytes
    LPI2C4_MTDR = LPI2C_MTDR_CMD_START | kTouchAddress << 1;             // the address, to write
    LPI2C4_MTDR = LPI2C_MTDR_CMD_TRANSMIT | kTdStatus;                   // TD_STATUS, the first of the five
    LPI2C4_MTDR = LPI2C_MTDR_CMD_START | kTouchAddress << 1 | 1;         // the address again, to read
    LPI2C4_MTDR = LPI2C_MTDR_CMD_RECEIVE | (kTouchBytes - 1);            // five bytes, then AUTOSTOP
}

Fault Touch::failed() const {
    return {present_ && elapsedMs(lastGood_, kFailMs), lastError_};
}
