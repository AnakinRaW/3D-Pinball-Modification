# Magnetic rotary sensor driver

The driver reports how far the seal rod turns. It reads the AS5600 that [`docs/parts/magnetic-rotary/design.md`](../docs/parts/magnetic-rotary/design.md) describes, publishes every movement, and leaves the degrees to the [rotating seal component](components/rotating-seal.md).

## Driver events

| Field | Content |
|---|---|
| Type | `DriverEventType::RotorMoved` while the rod turns, `DriverEventType::RotorStopped` once it stands still |
| Source | The sensor, always `0` in this build |
| Timestamp | `micros()` in the tick that saw the movement, or the standstill |
| Payload | The position in steps since `begin()`, 4096 to a turn, rising in the direction the module's DIR switch counts up. It is a `uint32_t` that wraps, so a receiver compares two positions as `(int32_t)(a - b)` |

The driver publishes the position once it lies 12 steps, about 1°, from the one it last published. A resting seal therefore stays silent, even when the machine's vibration rocks it within the rod's play. If the queue is full, the driver sends the event in the next tick instead, with the position it has by then. No movement goes missing, because every event carries the whole position and not only the steps since the previous event. 

Once the position has stayed within those 12 steps for 4 ticks, 20 ms, after a movement, the driver publishes `RotorStopped` with the position the rod came to rest at. 

`position()` gives the same count whenever the game wants it.

## Reading the sensor

The Teensy talks to the AS5600 over I²C at 100 kHz. The sensor gives the angle as a number from 0 to 4095 for one turn. After a full turn it starts again at 0. Each tick computes the difference to the last reading modulo 4095. This difference is the angle the rod turned between the two readings. 

The driver then adds the new value to the sum of all previous readings so that the seal rotary component can correctly compute on multiple sensor readings and still keep the right position when an event gets lost. An event gets lost when the queue is full, and the next event then carries the right position anyway. A component that added up raw angles itself would miscount a whole turn in that case.

The sensor operates using an `IntervalTimer` that ticks every 5 ms at priority 96. The processor never waits for the sensor. Each tick reads the angle of the previous tick and starts the next sensor read. 

Reading a sensor consists of three commands: address the sensor, receive two bytes, stop. The controller finishes the read long before the next tick, and the two bytes wait in its receive buffer until then. A read occupies the bus for 290 µs.

## Start-up

`begin()` lets `Wire` set up the pins, their pull-ups and the 100 kHz clock, and reads the AS5600's STATUS and AGC registers once. STATUS says whether a magnet was detected and whether it is too weak or too strong, and AGC sits mid-range at the right gap. `magnet()` reports both, so every start shows whether the gap is right.

`begin()` then reads the first angle, points the AS5600 at RAW ANGLE, takes an `IntervalTimer` and starts ticking. From then on the driver drives the LPI2C1 I²C controller itself, and `Wire` is not used again. 

`begin()` returns false when the AS5600 does not answer or no `IntervalTimer` is free. A missing magnet leaves the driver running and silent, and `magnet()` says why.

## The bus

No other device may use `Wire`, because the driver drives its controller itself after `begin()`.

A tick that finds an error drops the read, clears the controller's flags and both FIFOs as `Wire` does, and asks again. The errors are a missing answer, lost arbitration, a FIFO error and a line held low past the timeout `Wire` configures. If a fault stops the sensor in the middle of a byte, the sensor can keep the data line low and block the bus. The tick then sends up to nine clock pulses at once, one for each bit of a byte and its acknowledge. That usually lets the sensor finish its byte and release the line. `Wire` frees the bus the same way. If the line is still low afterwards, the driver tries again once the controller reports the fault again. If the previous read is still running when a tick comes, the tick starts no new one.

## Device faults

When no read has worked for 1 s, the driver reports `DeviceFailed` once, with the controller's error flags as payload, and keeps trying. Error cases are a sensor that does not answer, a line held low or a lost bus. The first read that works afterwards brings `DeviceRecovered`.

## Existing libraries

The driver drives the I²C controller itself, since the AS5600 libraries, [RobTillaart/AS5600](https://github.com/RobTillaart/AS5600) and [Seeed_Arduino_AS5600](https://github.com/Seeed-Studio/Seeed_Arduino_AS5600), wait in `Wire` until each read has ended. At 100 kHz a read costs 490 µs of waiting, 9.8 % of the processor's time at one read every 5 ms. Seeed's library also waits without a time limit, so a sensor that does not answer stops the whole firmware.

[teensy4_i2c](https://github.com/Richard-Gemmell/teensy4_i2c) reads without waiting, but takes an interrupt per byte, is a beta and has an [unanswered lock-up report](https://github.com/Richard-Gemmell/teensy4_i2c/issues/36).

## The driver

```cpp
// what the AS5600 said about the magnet at begin()
struct MagnetState {
    bool    detected;    // STATUS MD: a magnet is there
    bool    tooWeak;     // STATUS ML: the gain is at its maximum, the magnet is too far away
    bool    tooStrong;   // STATUS MH: the gain is at its minimum, the magnet is too close
    uint8_t agc;         // the gain the AS5600 settled on, mid-range at the right gap
};

class MagneticRotaryDriver {
public:
    // sets up Wire, reads STATUS, AGC and the first angle, points the AS5600 at
    // RAW ANGLE and starts the tick; false when the AS5600 does not answer or no
    // IntervalTimer is free
    bool begin(EventQueue::Producer& out) {
        out_ = &out;
        Wire.begin();                                        // the pins, their pull-ups and 100 kHz

        // reads n bytes from the register reg, high byte first; -1 when the AS5600 does not answer
        auto readRegister = [](uint8_t reg, uint8_t n) -> int32_t {
            Wire.beginTransmission(kAddress);
            Wire.write(reg);
            if (Wire.endTransmission() != 0 || Wire.requestFrom(kAddress, n) != n) return -1;
            int32_t value = 0;
            while (Wire.available()) value = (value << 8) | Wire.read();
            return value;
        };
        const int32_t status = readRegister(kStatus, 1);
        const int32_t agc    = readRegister(kAgc, 1);
        const int32_t raw    = readRegister(kRawAngle, 2);   // read last, so the pointer stays on RAW ANGLE
        if (status < 0 || agc < 0 || raw < 0) return false;

        magnet_.detected  = status & 0x20;                   // MD
        magnet_.tooWeak   = status & 0x10;                   // ML
        magnet_.tooStrong = status & 0x08;                   // MH
        magnet_.agc       = agc;
        last_             = raw & 0x0FFF;                    // the 12 bits of RAW ANGLE

        timer_.priority(kPriority);
        return timer_.begin([this] { tick(); }, kTickUs);    // tick() every 5 ms from here on
    }

    // steps since begin(), 4096 to a turn
    int32_t position() const { return (int32_t)position_; }

    // what STATUS and AGC said at begin(): magnet found, too weak, too strong
    MagnetState magnet() const { return magnet_; }

private:
    static constexpr uint8_t  kAddress  = 0x36;
    static constexpr uint8_t  kStatus   = 0x0B;
    static constexpr uint8_t  kAgc      = 0x1A;
    static constexpr uint8_t  kRawAngle = 0x0C;
    static constexpr uint32_t kTickUs   = 5000;
    static constexpr uint8_t  kPriority = 96;     // general-design.md
    static constexpr int32_t  kSteps    = 4096;
    static constexpr int32_t  kDeadband = 12;
    static constexpr int32_t  kStill    = 4;      // ticks without a degree of movement
    static constexpr int32_t  kFail     = 200;    // ticks without a working read before DeviceFailed
    static constexpr uint8_t  kSda      = 18;   // pin-assignment.md: SDA to the rotary sensors
    static constexpr uint8_t  kScl      = 19;   // pin-assignment.md: SCL to the rotary sensors
    static constexpr uint32_t kErrors   = LPI2C_MSR_NDF | LPI2C_MSR_ALF | LPI2C_MSR_FEF | LPI2C_MSR_PLTF;

    // the IntervalTimer: collect the read the last tick started, start the next
    void tick() {
        const uint32_t msr = LPI2C1_MSR;
        if (!down_ && ++idle_ >= kFail)                      // a second without a working read
            down_ = out_->publish(DriverEvent{micros(), DriverEventType::DeviceFailed, (uint8_t)Device::RotarySensor, msr & kErrors});
        if (msr & kErrors) {                                 // no answer, lost bus, FIFO error, line low
            if (msr & LPI2C_MSR_PLTF) recover();
            LPI2C1_MCR |= LPI2C_MCR_RTF | LPI2C_MCR_RRF;     // drop the read
            LPI2C1_MSR  = 0x00007F00;                        // clear every flag, as Wire does
        } else if (msr & LPI2C_MSR_MBF) {
            return;                                          // the last read is still on the bus
        } else if (((LPI2C1_MFSR >> 16) & 0x07) >= 2) {      // the two bytes of RAW ANGLE
            const uint32_t high = LPI2C1_MRDR & 0x0F;
            const uint32_t low  = LPI2C1_MRDR & 0xFF;
            const int32_t  raw  = (high << 8) | low;
            if (down_ && out_->publish(DriverEvent{micros(), DriverEventType::DeviceRecovered, (uint8_t)Device::RotarySensor, 0}))
                down_ = false;
            idle_ = 0;
            const int32_t  step = (raw - last_ + kSteps + kSteps / 2) % kSteps - kSteps / 2;
            last_      = raw;
            position_ += (uint32_t)step;                   // wraps without harm
            const int32_t moved = (int32_t)(position_ - published_);
            if (moved >= kDeadband || moved <= -kDeadband) {
                if (out_->publish(DriverEvent{micros(), DriverEventType::RotorMoved, 0, position_})) {
                    published_ = position_;                 // a full queue: the next tick tries again
                    moving_    = true;
                    still_     = 0;
                }
            } else if (moving_ && ++still_ >= kStill) {     // the rod stands still
                if (out_->publish(DriverEvent{micros(), DriverEventType::RotorStopped, 0, position_})) {
                    published_ = position_;
                    moving_    = false;
                }
            }
        }
        LPI2C1_MTDR = LPI2C_MTDR_CMD_START | (kAddress << 1) | 1;   // the address, to read
        LPI2C1_MTDR = LPI2C_MTDR_CMD_RECEIVE | (2 - 1);             // two bytes
        LPI2C1_MTDR = LPI2C_MTDR_CMD_STOP;
    }

    // clocks SCL until the device lets go of SDA, as Wire's force_clock() does
    static void recover() {
        for (const uint8_t pin : {kSda, kScl}) {
            *portConfigRegister(pin) = 5 | 0x10;             // GPIO, released high
            *portSetRegister(pin)    = digitalPinToBitMask(pin);
            *portModeRegister(pin)  |= digitalPinToBitMask(pin);
        }
        delayMicroseconds(10);
        for (int i = 0; i < 9 && !(digitalReadFast(kSda) && digitalReadFast(kScl)); i++) {
            digitalWriteFast(kScl, LOW);
            delayMicroseconds(5);
            digitalWriteFast(kScl, HIGH);
            delayMicroseconds(5);
        }
        for (const uint8_t pin : {kSda, kScl}) *portConfigRegister(pin) = 3 | 0x10;   // back to LPI2C1
    }

    IntervalTimer                timer_;
    EventQueue::Producer*        out_       = nullptr;
    int32_t                      last_      = 0;       // the first angle, set by begin()
    volatile uint32_t            position_  = 0;
    uint32_t                     published_ = 0;       // the position the last event carried
    bool                         moving_    = false;   // a RotorMoved has come since the last RotorStopped
    int32_t                      still_     = 0;       // ticks since the last RotorMoved
    int32_t                      idle_      = 0;       // ticks since the last working read
    bool                         down_      = false;   // DeviceFailed has gone out
    MagnetState                  magnet_{};
};
```
