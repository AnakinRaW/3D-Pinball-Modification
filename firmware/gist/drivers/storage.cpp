// how a request stands
enum class FileStatus : uint8_t {
    Idle      = 0,                       // not started yet
    Pending   = 1,                       // started, and not ended yet
    Succeeded = 2,                       // ended and worked
    Failed    = 4,                       // ended and failed, or could not start
    Completed = Succeeded | Failed       // a mask of both ends, never a status itself
};

// gets whether status s is one of the states in mask, such as status() & FileStatus::Completed
constexpr bool operator&(FileStatus s, FileStatus mask) { return (uint8_t)s & (uint8_t)mask; }

class FileReader;
class FileWriter;
class FileRequest;

class Storage : public Driver {
public:
    enum class Priority : uint8_t { High, Normal };   // a High request goes before every Normal one

    static constexpr uint8_t kPath = 64;  // the longest path, with its end

    // starts the storage task, which mounts the card through SdFat in its DMA mode, and waits
    // for the mount; the main task only. False when the device monitor is full, and when the
    // card is missing or its file system does not read, which the device monitor then reports
    bool begin() override;

    // gets a file that reads, by its full path such as "/games/<id>/audio/bumper.wav", without
    // touching the card; a path too long gives a file whose requests fail
    FileReader open(const char* path);

    // gets which parts have failed, bit 0 for the card, and SdFat's error code of its last failure
    Fault failed() const override;

private:
    friend class FileReader;
    friend class FileWriter;
    friend class FileRequest;
    friend class FileSystem;                 // the only caller of openFile()

    struct OpenFile;
    enum Kind : uint8_t { Read, Write, Rewrite };   // what a request does

    // gets a file that reads and writes, by its full path, for the file system component
    FileWriter openFile(const char* path);

    DriverEvent           queue_[kQueueDepth];   // WritingStarted and WritingEnded, which the storage task publishes
    EventQueue::Producer* out_     = nullptr;
    bool                  writing_ = false;      // a write runs or waits, as WritingStarted reported

    // ... the members the implementation below names
};

// a file on the card by its full path, which reads. It holds only the path, so it can be copied
// and let go at any time
class FileReader {
public:
    FileReader() = default;                  // a file whose requests fail

    // starts reading up to size bytes from position at into buffer and returns at once
    void read(uint32_t at, void* buffer, size_t size, FileRequest& r) const;

    const char* name() const { return path_; }   // gets the full path

protected:
    friend class Storage;

    char path_[Storage::kPath] = {};         // empty for a file whose requests fail
};

// a file on the card that reads and writes, which the file system component gives
class FileWriter : public FileReader {
public:
    // starts writing size bytes from data at position at and returns at once; the data stays
    // unchanged until r has ended
    void write(uint32_t at, const void* data, size_t size, FileRequest& r) const;

    // starts writing data as the file's whole content and returns at once; the data stays
    // unchanged until r has ended
    void rewrite(const void* data, size_t size, FileRequest& r) const;
};

// one read or write on the card. start() links it into a list, and the storage task writes its
// results into it, so it can be neither copied nor moved. The buffer or data it names stays in place
// until it has ended
class FileRequest {
public:
    FileRequest() = default;                 // a request of normal priority, which raises no interrupt

    // waits for the end of a request that has not ended, so the storage task never writes into
    // memory that has gone; an interrupt's requests live as long as the firmware
    ~FileRequest() { wait(); }

    FileRequest(const FileRequest&)            = delete;
    FileRequest& operator=(const FileRequest&) = delete;

    FileStatus status() const { return status_; }   // gets how the request stands
    size_t     bytes() const;                        // gets the bytes a read brought, 0 until it has succeeded
    uint32_t   fileSize() const;                     // gets the file's size as the request left it; 0 until it has ended, after a card error and while the card counts as failed

    // waits until the request has ended and gets whether it worked; false at once from an
    // interrupt or a task whose priority is not below the storage task's
    bool wait() const;

private:
    friend class Storage;
    template <uint8_t> friend class FileChannel;   // the one caller that sets a priority or an interrupt

    // a file channel's request of priority, which raises the interrupt wake whenever it ends when
    // wakes is set
    FileRequest(Storage::Priority priority, bool wakes, IRQ_NUMBER_t wake)
        : priority_(priority), wakes_(wakes), wake_(wake) {}

    const Storage::Priority priority_ = Storage::Priority::Normal;
    const bool          wakes_ = false;
    const IRQ_NUMBER_t  wake_  = IRQ_NUMBER_t(0);
    FileRequest*        next_  = nullptr;    // the request behind it in its list
    char                path_[Storage::kPath];   // a copy of its file's path
    Storage::Kind       kind_;
    uint32_t            at_;                 // where it starts in the file
    uint8_t*            buffer_;             // where a read lands, or the data a write takes
    size_t              size_;
    volatile FileStatus status_   = FileStatus::Idle;   // the storage task sets the end once the results are in; start() sets it for a request that cannot start
    bool                dropped_  = false;   // another call came before the end, so the request ends as failed
    size_t              bytes_    = 0;
    uint32_t            fileSize_ = 0;
};

extern Storage storage;                      // the one storage driver, defined in the main file

constexpr uint8_t     kFailures     = 3;         // card errors in a row, and the card has failed
constexpr uint32_t    kSector       = 512;       // the card's sector
constexpr uint32_t    kBounce       = 4096;      // the read buffer in RAM1
constexpr uint8_t     kOpen         = 16;        // files the storage task keeps open at once
constexpr uint32_t    kProbeMs      = 1000;      // from one status query of a failed card to the next
constexpr uint32_t    kStack        = 8192;      // the storage task's stack in bytes; a test build measures its deepest call chain at 1.66 KB
constexpr UBaseType_t kTaskPriority = 2;         // above the main task's 1, so the next transfer starts as soon as one ends
constexpr uint8_t     kKernel       = 208;       // the lowest priority number that may call the kernel, configMAX_SYSCALL_INTERRUPT_PRIORITY
constexpr UBaseType_t kRequest      = 0;         // the storage task's notification of a new request, and the main task's of the mount
constexpr UBaseType_t kCard         = 1;         // the storage task's notification of the SD controller's interrupt
constexpr uint32_t    kPauseMs      = 50;        // before a transfer that follows a failed one, so the main loop runs between two of SdFat's waits
constexpr uint32_t    kLandMs       = 100;       // the longest a transfer that still moves data gets before its line is reset, as Linux's driver for this controller waits
constexpr uint32_t    kCmdBusy      = 0x1;       // PRES_STATE CIHB, the SD controller holds its command line
constexpr uint32_t    kDataBusy     = 0x2;       // PRES_STATE CDIHB, the SD controller holds its data line
constexpr uint32_t    kCmdReset     = 0x02000000;   // SYS_CTRL RSTC, which frees the command line
constexpr uint32_t    kDataReset    = 0x04000000;   // SYS_CTRL RSTD, which frees the data line and stops its DMA
constexpr uint32_t    kNoAnswer     = 0x00010000;   // INT_STATUS CTOE, a command the card did not answer

// resets the SD controller's command line, its data line or both, as lines names them, and keeps
// the interrupt enables and the protocol settings across the reset
static void resetLines(uint32_t lines) {
    const uint32_t reset    = (lines & kCmdBusy ? kCmdReset : 0) | (lines & kDataBusy ? kDataReset : 0);
    const uint32_t enabled  = USDHC1_INT_STATUS_EN;
    const uint32_t protocol = USDHC1_PROT_CTRL;
    USDHC1_SYS_CTRL |= reset;
    while (USDHC1_SYS_CTRL & reset) {}             // a few of the controller's clocks
    USDHC1_INT_STATUS_EN = enabled;
    USDHC1_PROT_CTRL     = protocol;
}

// frees the SD controller's command line from a command that a card error left hanging, and
// switches off the interrupt that only SdFat's transfers use; between two calls into SdFat none of
// its commands is on its way
static void freeCommandLine() {
    USDHC1_INT_SIGNAL_EN = 0;                      // SdFat switches it on again for each transfer
    if (USDHC1_PRES_STATE & kCmdBusy) resetLines(kCmdBusy);
    USDHC1_INT_STATUS = USDHC1_INT_STATUS;         // writing a status bit back clears it
}

// frees the SD controller from a command or a transfer that a card error left hanging. A transfer
// that still moves data gets kLandMs to finish first, since a reset in its middle sends the rest of
// its data to a wrong address
static void freeController() {
    freeCommandLine();
    for (uint32_t ms = 0; (USDHC1_PRES_STATE & kDataBusy) && ms < kLandMs; ms = ms + 1) vTaskDelay(pdMS_TO_TICKS(1));
    if (USDHC1_PRES_STATE & kDataBusy) resetLines(kDataBusy);
    USDHC1_INT_STATUS = USDHC1_INT_STATUS;
}

// SdFat's card as the file system sees it: gets the SD controller ready before every transfer,
// forwards every sector and counts the transfers that fail, which tells a card error from a file
// that is not there
class CountingCard : public FsBlockDevice {
public:
    explicit CountingCard(SdioCard& card) : card_(card) {}

    uint32_t errors() const { return errors_; }   // gets the transfers that have failed so far

    bool     isBusy() override      { ready(); return card_.isBusy(); }
    uint32_t sectorCount() override { return card_.sectorCount(); }
    bool     syncDevice() override  { ready(); return count(card_.syncDevice()); }
    bool     readSector(uint32_t s, uint8_t* d) override              { ready(); return count(card_.readSector(s, d)); }
    bool     readSectors(uint32_t s, uint8_t* d, size_t n) override   { ready(); return count(card_.readSectors(s, d, n)); }
    bool     writeSector(uint32_t s, const uint8_t* d) override       { ready(); return count(card_.writeSector(s, d)); }
    bool     writeSectors(uint32_t s, const uint8_t* d, size_t n) override { ready(); return count(card_.writeSectors(s, d, n)); }

private:
    // pauses after a failed transfer, so the main loop runs between two of SdFat's waits, and frees
    // the SD controller
    void ready() {
        if (paused_ != errors_) {
            paused_ = errors_;
            vTaskDelay(pdMS_TO_TICKS(kPauseMs));
        }
        freeController();
    }

    bool count(bool worked) {
        if (!worked) errors_ = errors_ + 1;
        return worked;
    }

    SdioCard& card_;
    uint32_t  errors_ = 0;
    uint32_t  paused_ = 0;                // errors_ at the last pause
};

// a file the storage task keeps open; nothing but the storage task touches the table
struct Storage::OpenFile {
    FsFile   sd;                     // SdFat's file
    char     path[Storage::kPath];   // empty for a free entry
    uint32_t used;                   // nowUs() of its last request; a full table closes the entry used longest ago
    bool     writes;                 // opened for writing, which also creates a missing file
};
Storage::OpenFile Storage::files_[kOpen];   // the table

// the requests waiting for the storage task, in the order it serves them, one list for each
// priority; a caller adds at the tail, and the storage task takes the head away once it has ended
struct RequestList {
    FileRequest* head = nullptr;
    FileRequest* tail = nullptr;
};
static RequestList lists_[2];         // indexed by Storage::Priority, High first

// SdFat's card, the counting card the file system reads it through, and the file system
static SdioCard     card_;
static CountingCard counted_(card_);
static FsVolume     volume_;

// a read that may not land straight in its buffer lands here first; RAM1 has no cache
static uint8_t bounce_[kBounce] __attribute__((aligned(32)));

// the storage task with its stack in RAM1, where SdFat keeps buffers that DMA fills, and SdFat's
// handler of the SD controller's interrupt
static StackType_t  stack_[kStack / sizeof(StackType_t)];
static StaticTask_t task_;
static TaskHandle_t storageTask_ = nullptr;
static void       (*sdfat_)()    = nullptr;

// gets the exception the caller runs in, 0 in a task
static uint32_t exception() {
    uint32_t ipsr;
    asm volatile("mrs %0, ipsr" : "=r"(ipsr));
    return ipsr;
}

// gets whether the caller runs in an interrupt
static bool inInterrupt() { return exception() != 0; }

// gets whether the caller may call the kernel: a task once the kernel runs, or an interrupt with a
// priority number of kKernel or more
static bool mayCall() {
    const uint32_t e = exception();
    if (e == 0) return xTaskGetSchedulerState() == taskSCHEDULER_RUNNING;
    return e >= 16 && NVIC_GET_PRIORITY(e - 16) >= kKernel;
}

// starts the storage task and waits until it has tried the mount. The task runs whatever the
// mount finds, so after a failure there every request ends at once. Without a place in the device
// monitor no task starts, and every request fails at once
bool Storage::begin() {
    if (!deviceMonitor.watch(Device::SdCard, *this)) return false;   // before the mount, so a missing card is reported too
    out_ = &events.attach(queue_, kQueueDepth);
    self_  = this;
    owner_ = xTaskGetCurrentTaskHandle();          // the main task, which the storage task tells of the mount
    xTaskCreateStatic([](void*) { self_->run(); }, "storage", kStack / sizeof(StackType_t), nullptr,
                      kTaskPriority, stack_, &task_);
    ulTaskNotifyTakeIndexed(kRequest, pdTRUE, portMAX_DELAY);   // the storage task has tried the mount
    return !down_;
}

// the storage task: mounts the card, then carries the requests out one after the other and sleeps
// while both lists are empty. While a card that the mount found is down, it asks for its status
// once a second
void Storage::run() {
    storageTask_ = xTaskGetCurrentTaskHandle();     // yield() knows the task from here on, the mount included
    mounted_ = card_.begin(SdioConfig(DMA_SDIO));   // SdFat waits for the card here, through yield()
    down_    = !mounted_ || !volume_.begin(&counted_);   // a file system that does not read is asked again like a failed card
    if (down_) lastError_ = card_.errorCode();
    probed_  = nowUs();
    sdfat_ = _VectorsRam[IRQ_SDHC1 + 16];          // the handler SdFat's begin() attached, which ends a DMA transfer
    attachInterruptVector(IRQ_SDHC1, [] { self_->transferEnded(); });
    NVIC_SET_PRIORITY(IRQ_SDHC1, 240);
    xTaskNotifyGiveIndexed(owner_, kRequest);       // begin() goes on
    for (;;) {
        if (down_ && mounted_ && elapsedMs(probed_, kProbeMs)) probe();
        if (FileRequest* r = next()) {
            if (r->kind_ != Read && !writing_ && !down_) report(DriverEventType::WritingStarted);
            serve(*r);
            if (writing_ && !writeWaits()) report(DriverEventType::WritingEnded);
            continue;
        }
        const uint32_t untilProbe = kProbeMs - min(kProbeMs, (nowUs() - probed_) / 1000);
        ulTaskNotifyTakeIndexed(kRequest, pdTRUE, down_ && mounted_ ? pdMS_TO_TICKS(untilProbe) : portMAX_DELAY);
    }
}

// the SD controller's interrupt at the end of a DMA transfer: SdFat's handler ends the transfer,
// and the storage task wakes. A command the card did not answer starts no transfer, so its error
// stays in the controller for the storage task, which waits for the command's answer there. A
// broken answer goes to SdFat's handler, since the card may still move the transfer's data, and
// SdFat waits out its second while the data lands
void Storage::transferEnded() {
    if (USDHC1_INT_STATUS & kNoAnswer) USDHC1_INT_SIGNAL_EN = 0;
    else                               sdfat_();
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveIndexedFromISR(storageTask_, kCard, &woken);
    portYIELD_FROM_ISR(woken);
}

// SdFat calls yield() while it waits for the card: the storage task sleeps until the SD
// controller's interrupt or the kernel's next tick, and every other caller goes on at once. A
// status query the card left unanswered blocks the command line, which is freed for the next one
// while no transfer runs
void yield() {
    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING || inInterrupt()) return;
    if (xTaskGetCurrentTaskHandle() != storageTask_) return;
    if ((USDHC1_PRES_STATE & (kCmdBusy | kDataBusy)) == kCmdBusy) resetLines(kCmdBusy);
    ulTaskNotifyTakeIndexed(kCard, pdTRUE, 1);
}

// asks a failed card for its status, after stopping a transfer the card may still be in; once it
// answers that it is ready for data and its file system reads again, the fault clears
void Storage::probe() {
    freeCommandLine();
    card_.readStop();
    freeController();
    if ((card_.status() & CARD_STATUS_READY_FOR_DATA) && volume_.begin(&counted_)) {
        down_     = false;
        failures_ = 0;
    }
    probed_ = nowUs();                              // the next query comes a second after this one has ended
}

// publishes WritingStarted or WritingEnded and notes which one it was
void Storage::report(DriverEventType type) {
    writing_ = type == DriverEventType::WritingStarted;
    out_->publish(DriverEvent{nowUs(), type, 0, 0});
}

// gets whether a write waits in either list; callers add at the tails meanwhile
bool Storage::writeWaits() const {
    const UBaseType_t s     = taskENTER_CRITICAL_FROM_ISR();
    bool              waits = false;
    for (const RequestList& l : lists_) {
        for (const FileRequest* q = l.head; q && !waits; q = q->next_) waits = q->kind_ != Read;
    }
    taskEXIT_CRITICAL_FROM_ISR(s);
    return waits;
}

// gets the oldest request of high priority, else the oldest of normal priority; nullptr when
// both lists are empty
FileRequest* Storage::next() {
    for (RequestList& l : lists_) {
        if (l.head) return l.head;
    }
    return nullptr;
}

// carries a request out whole and ends it. While the card is down, the request ends at once as
// failed. A read of a file that fails to open while the card reports no error brings 0 bytes, and
// a write that starts past the end of its file fails without counting as a card error. A request
// that fails without a card error, such as a write to a full card, fails without counting either
void Storage::serve(FileRequest& r) {
    if (down_) {
        end(r, false, nullptr);
        return;
    }
    const uint32_t errors = counted_.errors();
    OpenFile*      f      = find(r);
    bool           worked;
    if (!f) {
        worked = r.kind_ == Read && counted_.errors() == errors;   // reads as empty
    } else if (r.kind_ == Write && r.at_ > f->sd.fileSize()) {
        worked = false;
    } else {
        worked = r.kind_ == Read ? readAll(r, *f) : writeAll(r, *f);
    }
    const bool cardError = counted_.errors() != errors;
    if (cardError) failure(card_.errorCode());   // before recover() sends commands of its own
    else           failures_ = 0;
    end(r, worked, f && !cardError ? f : nullptr);
}

// gets the entry of r's file in the table of open files, opening the file through SdFat where it
// is not open: for a read as it is, for a write with O_RDWR | O_CREAT after creating a missing
// directory, which also opens anew a file opened for reading. Paths compare without regard to
// case, as the card's file system names its files. A full table closes the entry used longest ago.
// nullptr when the file does not open
Storage::OpenFile* Storage::find(FileRequest& r);

// after a card error: stops the card's transfer where one still runs and frees the SD controller,
// closes every open file and reads the file system anew from the same card, so nothing SdFat read
// before the error stays in use; false when the file system does not come back. A card that was
// pulled out and put back is not taken into use
bool Storage::recover() {
    freeCommandLine();
    card_.readStop();
    freeController();                               // the data line, once the card has stopped sending
    for (OpenFile& f : files_) {
        f.sd.close();
        f.path[0] = 0;
    }
    return volume_.begin(&counted_);
}

// gets whether p lies in RAM2 or another memory the cache serves; RAM1 has no cache
static bool cached(const void* p) {
    return (uint32_t)p - 0x20000000u >= 0x80000u;   // RAM1 is the 512 KB from 0x20000000
}

// reads r through SdFat. A buffer in RAM2 takes the data straight when it starts on a 32-byte
// boundary, the read starts at a sector of the file and its size is a multiple of 32; the cache
// over the buffer is cleaned and invalidated before the read and after it, which drops what the
// cache fetched meanwhile and keeps what SdFat copied. Every other read into RAM2 goes through
// bounce_, kBounce bytes at a time
bool Storage::readAll(FileRequest& r, OpenFile& f) {
    FsFile& sd = f.sd;
    if (r.at_ >= sd.fileSize()) return true;        // at the end of the file or past it: 0 bytes
    if (!sd.seekSet(r.at_)) return false;
    const bool inCache  = cached(r.buffer_);
    const bool straight = !inCache || ((uint32_t)r.buffer_ % 32 == 0 && r.at_ % kSector == 0 && r.size_ % 32 == 0);
    if (straight) {
        if (inCache) arm_dcache_flush_delete(r.buffer_, r.size_);
        const int n = sd.read(r.buffer_, r.size_);
        if (inCache) arm_dcache_flush_delete(r.buffer_, r.size_);
        if (n < 0) return false;
        r.bytes_ = n;
        return true;
    }
    while (r.bytes_ < r.size_) {
        const size_t want = min(r.size_ - r.bytes_, (size_t)kBounce);
        const int    n    = sd.read(bounce_, want);
        if (n < 0) return false;
        memcpy(r.buffer_ + r.bytes_, bounce_, n);
        r.bytes_ = r.bytes_ + n;
        if ((size_t)n < want) break;                // the end of the file
    }
    return true;
}

// writes r through SdFat in one call: a rewrite starts at 0 and has SdFat cut the file behind its
// data. Data in RAM2 is cleaned out of the cache first, since SdFat's DMA reads the memory. SdFat
// then writes its cache and the file's entry to the card
bool Storage::writeAll(FileRequest& r, OpenFile& f) {
    const uint8_t* data = r.buffer_;
    const size_t   size = r.size_;
    FsFile&        sd = f.sd;
    const uint32_t at = r.kind_ == Rewrite ? 0 : r.at_;
    if (cached(data)) arm_dcache_flush(const_cast<uint8_t*>(data), size);
    if (!sd.seekSet(at) || sd.write(data, size) != size) return false;
    return r.kind_ == Rewrite ? sd.truncate() : sd.sync();   // truncate() cuts at the position and syncs as well
}

// ends a request: takes it out of its list, reports to it and raises its interrupt. The request
// belongs to its caller again once its status shows the end, so nothing touches it after
void Storage::end(FileRequest& r, bool worked, OpenFile* f) {
    const bool         wakes = r.wakes_;
    const IRQ_NUMBER_t wake  = r.wake_;
    if (f) f->used = nowUs();
    r.fileSize_ = f ? f->sd.fileSize() : 0;         // 0 after a card error
    if (!worked) r.bytes_ = 0;
    RequestList&      l = lists_[(uint8_t)r.priority_];
    const UBaseType_t s = taskENTER_CRITICAL_FROM_ISR();
    l.head = r.next_;                               // r is the head of its list
    if (!l.head) l.tail = nullptr;
    asm volatile("dmb" ::: "memory");               // the results are in before the status shows the end
    r.status_ = worked && !r.dropped_ ? FileStatus::Succeeded : FileStatus::Failed;
    taskEXIT_CRITICAL_FROM_ISR(s);
    if (wakes) NVIC_SET_PENDING(wake);
}

// after a card error: keeps its code, counts it and recovers. The third in a row, or a file system
// that does not come back, takes the card down, and its first status query comes a second later
void Storage::failure(uint32_t code) {
    lastError_ = code;
    failures_  = failures_ + 1;
    if (recover() && failures_ < kFailures) return;
    down_   = true;                                 // from now on every request ends at once
    probed_ = nowUs();
}

// down_ and lastError_ are volatile, since the device monitor reads them on the driver tick. down_
// starts false, so no fault shows while the mount runs
Fault Storage::failed() const {
    return {down_, lastError_};
}

FileReader Storage::open(const char* path) {
    FileReader f;
    if (strlen(path) < kPath) strcpy(f.path_, path);   // a path too long leaves a file whose requests fail
    return f;
}

FileWriter Storage::openFile(const char* path) {
    FileWriter f;
    if (strlen(path) < kPath) strcpy(f.path_, path);
    return f;
}

// wakes the storage task, which runs at once from a task and once every interrupt has ended from
// an interrupt
static void wake() {
    if (!inInterrupt()) {
        xTaskNotifyGiveIndexed(storageTask_, kRequest);
        return;
    }
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveIndexedFromISR(storageTask_, kRequest, &woken);
    portYIELD_FROM_ISR(woken);
}

// every read and every write starts here: fills r in, adds it at the tail of its list and wakes
// the storage task. A call on a pending request makes it end as failed. A caller that may not
// call the kernel leaves a pending request as it is. A file whose requests fail, a storage driver
// without its task and a caller that may not call the kernel end any other request at once as failed
void Storage::start(FileRequest& r, const FileReader& f, Kind kind, uint32_t at,
                    const void* buffer, size_t size) {
    if (r.status_ == FileStatus::Pending) {
        if (!mayCall()) return;                     // only a caller of the kernel may touch a request on its way
        const UBaseType_t s       = taskENTER_CRITICAL_FROM_ISR();
        const bool        pending = r.status_ == FileStatus::Pending;
        if (pending) r.dropped_ = true;             // the call starts nothing, and the request ends as failed
        taskEXIT_CRITICAL_FROM_ISR(s);
        if (pending) return;
    }                                               // a request that ended meanwhile starts afresh
    memcpy(r.path_, f.path_, kPath);
    r.kind_     = kind;
    r.at_       = at;
    r.buffer_   = static_cast<uint8_t*>(const_cast<void*>(buffer));
    r.size_     = size;
    r.bytes_    = 0;
    r.fileSize_ = 0;
    r.next_     = nullptr;
    r.dropped_  = false;
    if (!f.path_[0] || !storageTask_ || !mayCall()) {
        r.status_ = FileStatus::Failed;
        if (r.wakes_) NVIC_SET_PENDING(r.wake_);    // the end of a request raises its interrupt, also this one
        return;
    }
    r.status_ = FileStatus::Pending;
    RequestList&      l = lists_[(uint8_t)r.priority_];
    const UBaseType_t s = taskENTER_CRITICAL_FROM_ISR();
    if (l.tail) l.tail->next_ = &r;
    else        l.head = &r;
    l.tail = &r;
    taskEXIT_CRITICAL_FROM_ISR(s);
    wake();
}

void FileReader::read(uint32_t at, void* buffer, size_t size, FileRequest& r) const {
    Storage::start(r, *this, Storage::Read, at, buffer, size);
}

void FileWriter::write(uint32_t at, const void* data, size_t size, FileRequest& r) const {
    Storage::start(r, *this, Storage::Write, at, data, size);
}

void FileWriter::rewrite(const void* data, size_t size, FileRequest& r) const {
    Storage::start(r, *this, Storage::Rewrite, 0, data, size);
}

// reads the status before the results it guards, so a request that ended always comes with them
size_t FileRequest::bytes() const {
    const FileStatus s = status_;
    asm volatile("dmb" ::: "memory");
    return s == FileStatus::Succeeded ? bytes_ : 0;
}

uint32_t FileRequest::fileSize() const {
    const FileStatus s = status_;
    asm volatile("dmb" ::: "memory");
    return s == FileStatus::Succeeded || s == FileStatus::Failed ? fileSize_ : 0;
}

// the storage task has a higher priority than the main task, so it carries the request out while
// this spins; a task whose priority is not below the storage task's would hold it off, and gets
// false at once
bool FileRequest::wait() const {
    if (inInterrupt() || uxTaskPriorityGet(nullptr) >= kTaskPriority) return false;
    while (status_ == FileStatus::Pending) {}
    return status_ == FileStatus::Succeeded;
}
