class Display : public Driver {
public:
    enum class Layer : uint8_t { Front, Middle, Back };   // from the front; Back shows a playing movie inside its viewport

    // sets up FlexIO3 and the panel and attaches to the driver tick; false when it is full
    bool begin() override;

    uint8_t* pixels(Layer layer);                            // gets a layer's palette indices, row by row
    void     palette(Layer layer, uint8_t index, uint16_t rgb565, uint8_t opacity);   // sets a color of a layer's palette, opacity 0 to 255 in 32 steps
    void     palette(Layer layer, const uint8_t* entries);    // sets all 256 entries of a layer's palette from a picture file's table, entry 0 unused
    uint16_t color(Layer layer, uint8_t index) const;       // gets the RGB565 color of a palette entry, the back layer's with its opacity folded in
    bool     opaque(Layer layer, uint8_t index) const;       // gets whether a palette entry covers what lies behind it, every entry of the back layer
    void     show(uint16_t top, uint16_t bottom);            // marks rows top to bottom - 1 as drawn; returns at once
    void     shown(Layer layer, bool shown);                 // shows or hides a layer and marks every row

    uint32_t play(const FileReader& movie, uint16_t x, uint16_t y, uint16_t width, uint16_t height, bool loop);   // starts a movie in the viewport at x, y on the back layer; returns its number at once, 0 for a viewport off the 16-pixel steps or beyond the screen
    void     stopMovie();                                    // stops the movie; returns at once
    bool     moviePlaying() const;                           // gets whether a movie plays

private:
    static constexpr uint16_t kWidth        = 320;
    static constexpr uint16_t kHeight       = 240;
    static constexpr uint16_t kColors      = 256;     // a palette, index 0 transparent
    static constexpr uint32_t kOpaque       = 31;      // the highest of the 32 opacity steps
    static constexpr uint8_t  kShifters     = 8;       // FlexIO3's shifters, one buffer of a burst
    static constexpr uint8_t  kDataLines    = 8;       // D0 to D7 on one shifter
    static constexpr uint8_t  kBurst        = 32;      // bytes in one burst, 16 pixels
    static constexpr uint8_t  kDivider      = 10;      // FlexIO3 clocks per write, 83.3 ns at 120 MHz
    static constexpr uint8_t  kFirstData    = 8;       // the FlexIO3 signal of D0
    static constexpr uint8_t  kWr           = 6;       // the FlexIO3 signal of WR
    static constexpr uint8_t  kPriority     = 224;     // the FlexIO3 row in general-design.md
    static constexpr uint32_t kTickUs       = 40000;   // the driver's period on the driver tick, one movie frame at 25 frames a second
    static constexpr uint32_t kChunk        = 16384;   // bytes in one read of the card
    static constexpr uint8_t  kChunks       = 4;       // chunks in the movie's ring
    static constexpr uint32_t kPanelReadyMs = 285;     // after power-up, docs/parts/display
    static constexpr uint32_t kResetMs      = 120;     // after a software reset, which a restart may send to an awake panel
    static constexpr uint32_t kSleepOutMs   = 120;     // after Sleep Out
    static constexpr uint32_t kLastFlag     = 1 << (kShifters - 1);   // shifter 7, whose data starts a burst
    static constexpr uint32_t kEndWaitNs    = 3000;    // from the start of a run's last burst to its end, with a reserve over the burst
    static constexpr uint8_t  kCs = 28;     // pin-assignment.md: CS to the display controller
    static constexpr uint8_t  kDc = 29;     // pin-assignment.md: D/C to the display
    static constexpr uint8_t  kBus[] = {22, 23, 20, 21, 38, 39, 26, 27, 17};  // pin-assignment.md: D0 to the display, D1 to the display, D2 to the display, D3 to the display, D4 to the display, D5 to the display, D6 to the display, D7 to the display, WR to the display

    // ... the members the implementation below names
};

DMAMEM uint8_t Display::front_[kWidth * kHeight] __attribute__((aligned(32)));    // the front layer
DMAMEM uint8_t Display::middle_[kWidth * kHeight] __attribute__((aligned(32)));   // the middle layer
DMAMEM uint8_t Display::back_[kWidth * kHeight] __attribute__((aligned(32)));     // the back layer
DMAMEM uint8_t Display::ring_[kChunks * kChunk] __attribute__((aligned(32)));   // the movie's chunks
FileChannel<Display::kChunks> Display::movie_(ring_, kChunk, IRQ_FLEXIO3);            // the movie playing, read ahead into the ring; every read raises FlexIO3
uint32_t       Display::palette_[3][kColors];                                   // RGB565 in the lower half, the opacity step above it; entry 0 transparent

// a play() request: the main loop fills the slot the request word does not name, then publishes
// it, and the interrupt reads the slot the word names
struct MovieRequest {
    FileReader movie;
    uint16_t   x;           // the viewport's left edge, in steps of kBurst / 2 pixels
    uint16_t   y;
    uint16_t   width;       // in steps of kBurst / 2 pixels
    uint16_t   height;
    bool       loop;
};
static MovieRequest requests_[2];

// gets fg over bg, both RGB565, at a of 32: green apart from red and blue in one word, so
// one multiply a color blends all three
static inline uint16_t blend(uint16_t fg, uint16_t bg, uint32_t a) {
    const uint32_t f = (fg | (uint32_t)fg << 16) & 0x07E0F81F;
    const uint32_t b = (bg | (uint32_t)bg << 16) & 0x07E0F81F;
    const uint32_t c = ((f * a + b * (32 - a)) >> 5) & 0x07E0F81F;
    return (uint16_t)(c | c >> 16);
}

// gets a palette entry; the back layer's opacity goes into its color, since only black lies behind it
uint32_t Display::entry(Layer layer, uint16_t rgb565, uint8_t opacity) {
    const uint32_t a = opacity >> 3;                                     // 0 to 31
    if (layer != Layer::Back) return rgb565 | a << 16;
    return (a == kOpaque ? rgb565 : blend(rgb565, 0, a)) | kOpaque << 16;
}

void Display::palette(Layer layer, uint8_t index, uint16_t rgb565, uint8_t opacity) {
    if (index != 0) palette_[(int)layer][index] = entry(layer, rgb565, opacity);   // 0 stays transparent
}

uint16_t Display::color(Layer layer, uint8_t index) const {
    return palette_[(int)layer][index];
}

bool Display::opaque(Layer layer, uint8_t index) const {
    return palette_[(int)layer][index] >> 16 == kOpaque;
}

// a picture file's entry: RGB565 high byte first, the opacity, an unused byte
void Display::palette(Layer layer, const uint8_t* entries) {
    for (int i = 1; i < kColors; ++i) {
        const uint8_t* e = entries + 4 * i;
        palette_[(int)layer][i] = entry(layer, e[0] << 8 | e[1], e[2]);
    }
}

// marks rows top to bottom - 1 as drawn: the main loop is the only writer of drawn_
void Display::show(uint16_t top, uint16_t bottom) {
    for (uint16_t row = top; row < bottom && row < kHeight; ++row) drawn_[row] = drawn_[row] + 1;
    NVIC_SET_PENDING(IRQ_FLEXIO3);
}

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

// a run of rows waiting to be sent
struct Run {
    uint16_t top;               // its first row
    uint16_t rows;              // how many, 0 for none
};

// shifter 0 alone and one write per trigger, for the commands; bursts() sets the chain up again
void Display::single();

// sends a command and its parameters one byte at a time, D/C low for the command; the
// interrupt calls it at the start of a run after single(), and each wait lasts one write
void Display::command(uint8_t cmd, const uint8_t* params, uint8_t count);

// the power, frame rate and gamma commands the ILI9341 libraries send after a reset
void Display::startUpTable();

// the rows the interrupt sends next: the first run of rows whose drawn and sent counters
// differ, outside the movie's rows while a movie plays; no rows when none waits
Run Display::waitingRows();

// sets the panel's window to rows top to bottom - 1, starts the memory write, takes the
// rows' drawn counters into their sent counters, loads the first burst and turns to bursts
void Display::startRun(uint16_t top, uint16_t bottom, bool movie);

// ends a run: once a movie's last frame is out, sets its rows' sent counters one behind their
// drawn counters, so the rows go out again with the layers over the back layer, and publishes
// MovieEnded with the number play() returned
void Display::endRun();

// applies the last play() or stopMovie() of the main loop, which leaves its request in one
// word with a count and, for play(), the slot of requests_; starts or stops the movie_ of that
// slot, and a stop sets the rows' sent counters one behind, as endRun() does
void Display::takeRequests();

// gets whether the movie's next frame is due: the ticks since its start have reached the frame's,
// and the movie has not run out
bool Display::frameDue() const;

// writes the next 16 pixels into the shifter buffers, shifter 7 last since it starts the
// burst; false while the movie's next chunk has not arrived
bool Display::refill() {
    const uint8_t*  front  = shown_[0] ? front_ + at_ : nullptr;          // shown_ and palette_ in the order of Layer
    const uint8_t*  middle = shown_[1] ? middle_ + at_ : nullptr;
    const uint8_t*  back   = shown_[2] ? back_ + at_ : nullptr;
    const uint16_t* movie = nullptr;
    uint16_t        burst[kBurst / 2];                                   // the movie's 16 pixels of this burst
    const uint16_t  col    = at_ % kWidth;                               // the burst's first column; a burst lies wholly inside or outside the viewport
    if (movieRun_ && col >= movieX_ && col < movieX_ + movieWidth_) {
        const size_t ready = movie_.available();
        if (ready > 0 && ready < kBurst) movie_.stop();                  // a file that is no whole number of bursts ends at its tail
        if (movie_.take(burst, kBurst) == kBurst) {
            movie = burst;
        } else if (!movie_.ended()) {
            return false;                                                // the chunk is still on the card
        }                                                                // else the movie has run out: the layers over the background
    }
    uint32_t words[kShifters];
    for (int i = 0; i < kShifters; ++i) {
        uint16_t pair[2];
        for (int k = 0; k < 2; ++k) {
            const int      n = 2 * i + k;
            const uint32_t f = front ? palette_[0][front[n]] : 0;            // index 0 has opacity 0
            uint16_t       c = f;
            if (f >> 16 != kOpaque) {                                        // the front lets something through
                const uint32_t m = middle ? palette_[1][middle[n]] : 0;
                uint16_t       behind = m;
                if (m >> 16 != kOpaque) {
                    const uint16_t base = back && movie ? __builtin_bswap16(movie[n])   // the back layer shows the movie inside its viewport
                                        : back ? (uint16_t)palette_[2][back[n]] : 0;   // black behind everything
                    behind = m >> 16 ? blend(m, base, m >> 16) : base;
                }
                c = f >> 16 ? blend(f, behind, f >> 16) : behind;
            }
            pair[k] = __builtin_bswap16(c);                                  // the high byte leaves first
        }
        words[i] = pair[0] | (uint32_t)pair[1] << 16;
    }
    IMXRT_FLEXIO_t& io = IMXRT_FLEXIO3_S;
    for (int i = 0; i < kShifters; ++i) io.SHIFTBUF[i] = words[i];
    at_ += kBurst / 2;
    return true;
}

// the FlexIO3 interrupt: keeps the movie's reads going, refills the running run, ends it after its
// last burst, or starts the next
void Display::isr() {
    movie_.available();                                                  // starts the movie's next read once the last has ended
    IMXRT_FLEXIO_t& io = IMXRT_FLEXIO3_S;
    if (running_) {
        if (!(io.SHIFTSTAT & kLastFlag)) return;                         // the buffers still wait for their burst
        if (left_ > 0) {
            if (refill()) { left_ = left_ - 1; io.SHIFTSIEN = kLastFlag; }
            else io.SHIFTSIEN = 0;                                       // the storage driver raises the interrupt again
            return;
        }
        io.SHIFTSIEN = 0;                                                // the last burst started when the flag rose
        delayNanoseconds(kEndWaitNs);                                    // and has left the bus after this, however late the interrupt came
        endRun();                                                        // MovieEnded once a movie's last row is out
    }
    takeRequests();                                                      // play() and stopMovie() of the main loop
    if (frameDue()) startRun(movieY_, movieY_ + movieHeight_, true);    // the movie's rows and its next frame
    else if (const Run r = waitingRows(); r.rows) startRun(r.top, r.top + r.rows, false);
}

bool Display::begin() {
    displayOut_ = &events.attach(displayQueue_, kQueueDepth);    // MovieEnded, which the FlexIO3 interrupt publishes
    setUpBus();
    while (nowUs() < kPanelReadyMs * 1000) {}                            // the supervisor's reset is over; nowUs() starts after power-up, so this waits at least as long
    single();
    command(0x01, nullptr, 0);                                           // software reset
    delay(kResetMs);
    startUpTable();                                                      // the ILI9341 libraries' power and gamma table
    const uint8_t rgb565 = 0x55, landscape = 0x28;
    command(0x3A, &rgb565, 1);                                           // COLMOD, 16 bits a pixel
    command(0x36, &landscape, 1);                                        // MADCTL, 320 across, matched to the mounting
    command(0x11, nullptr, 0);                                           // Sleep Out
    delay(kSleepOutMs);
    command(0x29, nullptr, 0);                                           // Display On
    bursts();
    self_ = this;
    attachInterruptVector(IRQ_FLEXIO3, [] { self_->isr(); });
    NVIC_SET_PRIORITY(IRQ_FLEXIO3, kPriority);
    NVIC_ENABLE_IRQ(IRQ_FLEXIO3);
    memset(front_, 0, sizeof front_);                                    // DMAMEM starts with whatever the RAM held
    memset(middle_, 0, sizeof middle_);
    memset(back_, 0, sizeof back_);
    show(0, kHeight);                                                    // black, while every layer is empty

    return driverTick.attach([this] { tick(); }, kTickUs);
}

// the driver tick, every kTickUs: the movie's next frame
void Display::tick() {
    ticks_ = ticks_ + 1;                                                 // frameDue() counts the frames from it
    if (playing_) NVIC_SET_PENDING(IRQ_FLEXIO3);                         // the interrupt starts the frame
}
