// the source of a fault event: the device in the upper four bits and the part in the lower
// four, so a device has at most 16 parts
constexpr uint8_t faultSource(Device d, uint8_t part) { return (uint8_t)((uint8_t)d << 4 | part); }
constexpr Device  faultDevice(uint8_t source)         { return (Device)(source >> 4); }
constexpr uint8_t faultPart(uint8_t source)           { return source & 0x0F; }

// the parts of one device as a set, one bit per part, bit n for part n, started from LSB
using PartMask = uint16_t;

// what a driver's failed() returns
struct Fault {
    PartMask parts;   // one bit per failed part
    uint32_t code;    // the driver's error code, 0 for a driver without error codes
};

// records the failed parts of every device and the error code each device last failed with
class FailedDevices {
public:
    // marks a part as failed and keeps the error code it failed with
    void set(Device d, uint8_t part, uint32_t code) { parts_[(size_t)d] |= 1u << part; codes_[(size_t)d] = code; }

    void     clear(Device d, uint8_t part)          { parts_[(size_t)d] &= ~(1u << part); }      // marks a part as working again
    bool     isFailed(Device d, uint8_t part) const { return parts_[(size_t)d] & (1u << part); } // gets whether a part has failed
    bool     anyFailed(Device d) const              { return parts_[(size_t)d] != 0; }           // gets whether any part of a device has failed
    uint32_t code(Device d) const                   { return codes_[(size_t)d]; }                // gets the error code a device last failed with

    // calls fn(device, part) for every failed part, device by device
    template <typename Fn> void forEach(Fn fn) const {
        for (size_t d = 0; d < (size_t)Device::Count; ++d)
            for (PartMask left = parts_[d]; left != 0; left &= left - 1)   // drops the lowest set bit each round
                fn((Device)d, (uint8_t)__builtin_ctz(left));                // the number of that bit is the part
    }

private:
    PartMask parts_[(size_t)Device::Count] = {};   // one bit per part
    uint32_t codes_[(size_t)Device::Count] = {};   // the error code of each device's last failure
};

// monitors the devices of the registered drivers and reports every part that fails or recovers
class DeviceMonitor {
public:
    // registers a driver, whose failed() every check asks; before the main loop only, and the first call
    // attaches the queue and the check to the driver tick. False when every slot is taken
    bool watch(Device device, const Driver& driver) {
        if (count_ == kSlots) return false;
        if (count_ == 0) {
            out_ = &events.attach(queue_, kDepth);
            if (!driverTick.attach([this] { check(); }, kCheckUs)) return false;
        }
        noInterrupts();                               // the check must not see a half-written slot
        slots_[count_] = Slot{device, &driver, 0};
        count_ = count_ + 1;
        interrupts();
        return true;
    }

private:
    static constexpr uint8_t  kSlots   = 8;
    static constexpr size_t   kDepth   = 32;          // a converter that stops answering fails eight parts at once
    static constexpr uint32_t kCheckUs = 100000;      // 100 ms from one check to the next

    struct Slot { Device device; const Driver* driver; PartMask reported; };

    // the driver tick, every kCheckUs: publishes every part whose bit changed; a full
    // queue leaves the part as reported, so the next check tries again
    void check() {
        for (uint8_t i = 0; i < count_; ++i) {
            Slot&       s = slots_[i];
            const Fault f = s.driver->failed();
            for (PartMask left = f.parts ^ s.reported; left != 0; left &= left - 1) {
                const uint8_t  part = __builtin_ctz(left);   // the lowest part that changed
                const PartMask bit  = 1u << part;
                const auto     type = (s.reported & bit) ? DriverEventType::DeviceRecovered
                                                         : DriverEventType::DeviceFailed;
                const uint32_t code = type == DriverEventType::DeviceFailed ? f.code : 0;
                if (out_->publish(DriverEvent{micros(), type, faultSource(s.device, part), code})) s.reported ^= bit;
            }
        }
    }

    DriverEvent           queue_[kDepth];
    EventQueue::Producer* out_ = nullptr;
    Slot                  slots_[kSlots];
    volatile uint8_t      count_ = 0;
};

extern DeviceMonitor deviceMonitor;

#include "Watchdog_t4.h"         // the WDT_T4 library, github.com/tonton81/WDT_T4

extern "C" void watchdogWarned(const uint32_t* frame);

// the i.MX RT's WDOG1, which restarts the Teensy unless it is fed within 2 s. It notes why it went
// unfed in CrashReport's breadcrumbs, #1 the cause and #2 its detail
class Watchdog {
public:
    // starts the watchdog and its warning; the main task calls it first, right before the
    // solenoids' begin(), so it guards the coils from their first pull
    void begin();

    // feeds the watchdog while every guard holds, and notes the cause where one fails; at the end
    // of every pass of the main loop, and from FreeRTOS's idle task until the main loop runs
    void check();

    // writes the report of the last restart into the log; the main task, right after the logger's begin()
    void report(Logger& logger);

private:
    friend void watchdogWarned(const uint32_t* frame);

    static constexpr unsigned kCauseNone = 0;    // a note the restart did not follow
    static constexpr unsigned kCauseCoil = 1;    // a coil stays on too long; #2 the coils, one bit each
    static constexpr unsigned kCauseHang = 2;    // the firmware hangs; #2 the address of the interrupted code

    // notes why the watchdog goes unfed; the first note stands until the restart
    void note(unsigned cause, unsigned detail);

    static Watchdog* self_;                      // the watchdog the warning interrupt notes in
    WDT_T4<WDT1>     wdt_;                       // the i.MX RT's WDOG1
    volatile bool    noted_ = false;             // a note waits for its restart
};

extern Watchdog watchdog;

Watchdog* Watchdog::self_ = nullptr;

// finds the interrupted code's saved registers, on a task's stack or on the interrupts' stack
__attribute__((naked)) static void watchdogWarning() {
    asm volatile(
        "tst lr, #4          \n"
        "ite eq              \n"
        "mrseq r0, msp       \n"
        "mrsne r0, psp       \n"
        "b watchdogWarned    \n");
}

// the warning, half a second before the restart: notes where the interrupted code stood
extern "C" void watchdogWarned(const uint32_t* frame) {
    Watchdog::self_->note(Watchdog::kCauseHang, frame[6]);   // the program counter the interrupt saved
    WDOG1_WICR |= WDOG_WICR_WTIS;                // clears the warning; the restart follows
}

void Watchdog::begin() {
    self_ = this;
    WDT_timings_t config;
    config.timeout  = 2;         // in seconds
    config.trigger  = 0.5;       // the warning, in seconds before the restart
    config.callback = [] {};     // WDT_T4 enables the warning only with a callback
    wdt_.begin(config);
    attachInterruptVector(IRQ_WDOG1, watchdogWarning);   // replaces WDT_T4's handler, which cannot see the interrupted code
    NVIC_SET_PRIORITY(IRQ_WDOG1, 32);                     // the watchdog's row in general-design.md
}

void Watchdog::check() {
    if (const uint8_t coils = solenoids.overdue()) {   // each guarded component adds its check
        note(kCauseCoil, coils);
        return;
    }
    if (noted_) {                                // fed after all, so the note is void
        CrashReport.breadcrumb(1, kCauseNone);
        noted_ = false;
    }
    wdt_.feed();
}

void Watchdog::note(unsigned cause, unsigned detail) {
    if (noted_) return;
    CrashReport.breadcrumb(1, cause);
    CrashReport.breadcrumb(2, detail);
    noted_ = true;
}

// prints CrashReport's report line by line where the chip records a restart other than a
// power-up; CrashReport clears it once printed
void Watchdog::report(Logger& logger) {
    if (!CrashReport && !(SRC_SRSR & ~SRC_SRSR_IPP_RESET_B)) return;
    struct Lines : Print {
        Logger& log;
        char    line[Logger::kLine + 1];
        size_t  n = 0;
        explicit Lines(Logger& l) : log(l) {}
        size_t write(uint8_t c) override {
            if (c == '\r') return 1;
            if (c != '\n') {
                if (n < Logger::kLine) line[n++] = c;   // the rest of a longer line is cut, as write() does
                return 1;
            }
            line[n] = 0;
            if (n) log.write("restart", "%s", line);
            n = 0;
            return 1;
        }
    } lines{logger};
    lines.print(CrashReport);
}
