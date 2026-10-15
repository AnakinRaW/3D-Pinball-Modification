# Magnetic rotary sensor driver

The driver reports how far the seal rod turns. It reads the AS5600 that [`docs/parts/magnetic-rotary/design.md`](../../parts/magnetic-rotary/design.md) describes, publishes every movement, and leaves the degrees to the [rotating seal component](../components/rotating-seal.md).

## Driver events

| Field | Content |
|---|---|
| Type | `DriverEventType::RotorMoved` while the rod turns, `DriverEventType::RotorStopped` once it stands still |
| Source | The sensor, always `0` in this build |
| Timestamp | `nowUs()` in the tick that saw the movement, or the standstill |
| Payload | The position in steps since `begin()`, rising in the direction the module's DIR switch counts up. It is a `uint32_t` that wraps, so a receiver compares two positions as `(int32_t)(a - b)`. A turn has 4096 steps |

The driver publishes the position once it lies 12 steps from the one it last published, which is about 1°. A resting seal therefore stays silent, even when the machine's vibration rocks it within the rod's play. If the queue is full, the driver sends the event in the next tick instead, with the position it has by then. No movement goes missing, because every event carries the whole position and not only the steps since the previous event.

After a movement, the driver publishes `RotorStopped` once the position has stayed within those 12 steps for 4 ticks, which take 20 ms. The event carries the position the rod came to rest at.

`position()` gives the same count whenever the game wants it.

## Reading the sensor

The Teensy talks to the AS5600 over I²C at 100 kHz. The sensor gives the angle as a number from 0 to 4095 for one turn. After a full turn it starts again at 0. Each tick computes the difference to the last reading modulo 4096. This difference is the angle the rod turned between the two readings.

The driver adds each step to the sum of all previous steps, so the rotating seal component keeps the right position even when an event gets lost to a full queue. A component that added up raw angles itself would miscount a whole turn in that case.

The processor never waits for the sensor. Each 5 ms [driver tick](../driver-design.md#driver-intervaltimer) collects the angle that the previous tick asked for and then starts the next read. A read occupies the bus for 290 µs. The controller finishes it long before the next tick, and the two bytes wait in its receive buffer until then.

## Start-up

`begin()` lets `Wire` set up the pins, their pull-ups and the 100 kHz clock, and reads the AS5600's STATUS and AGC registers once. STATUS says whether a magnet was detected and whether it is too weak or too strong, and AGC sits mid-range at the right gap. `magnet()` reports both, so every start shows whether the gap is right.

`begin()` then reads the first angle, points the AS5600 at RAW ANGLE and attaches its read to the driver tick. From then on the driver drives the LPI2C1 I²C controller itself, and `Wire` is not used again.

`begin()` returns false when the AS5600 does not answer. A missing magnet leaves the driver running and silent, and `magnet()` says why.

## Bus

No other device may use `Wire`, because the driver drives its controller itself after `begin()`.

A tick that finds an error drops the read and asks again. The errors are a missing answer, lost arbitration, a FIFO error and a line held low past the timeout `Wire` configures. If a fault stops the sensor in the middle of a byte, the sensor can keep the data line low and block the bus. The tick then sends up to nine clock pulses at once, one for each bit of a byte and its acknowledge. That usually lets the sensor finish its byte and release the line. `Wire` frees the bus the same way. If the line is still low afterwards, the driver tries again once the controller reports the fault again. If the previous read is still running when a tick comes, the tick starts no new one.

## Device faults

The sensor counts as failed once no angle has arrived for 1 s. Error cases are a sensor that does not answer, a line held low or a lost bus. The error code [`failed()`](../error-handling.md#device-faults) returns is the controller's error flags of the last failed read.

## Existing libraries

The driver drives the I²C controller itself, since the AS5600 libraries wait in `Wire` until each read has ended. Those libraries are [RobTillaart/AS5600](https://github.com/RobTillaart/AS5600) and [Seeed_Arduino_AS5600](https://github.com/Seeed-Studio/Seeed_Arduino_AS5600). At 100 kHz a read costs 490 µs of waiting, 9.8 % of the processor's time at one read every 5 ms. Seeed's library also waits without a time limit, so a sensor that does not answer stops the whole firmware.

[teensy4_i2c](https://github.com/Richard-Gemmell/teensy4_i2c) reads without waiting, but takes an interrupt per byte, is a beta and has an [unanswered lock-up report](https://github.com/Richard-Gemmell/teensy4_i2c/issues/36).

## Driver

```cpp
// what the AS5600 said about the magnet at begin()
struct MagnetState {
    bool    detected;    // STATUS MD: a magnet is there
    bool    tooWeak;     // STATUS ML: the gain is at its maximum, the magnet is too far away
    bool    tooStrong;   // STATUS MH: the gain is at its minimum, the magnet is too close
    uint8_t agc;         // the gain the AS5600 settled on, mid-range at the right gap
};

class MagneticRotaryDriver : public Driver {
public:
    // attaches the queue, sets up Wire, reads STATUS, AGC and the first angle, points
    // the AS5600 at RAW ANGLE, attaches its read to the driver tick and registers with
    // the device monitor; false when the AS5600 does not answer or either of the two is full
    bool begin() override;

    // steps since begin(), 4096 to a turn
    int32_t position() const;

    // what STATUS and AGC said at begin(): magnet found, too weak, too strong
    MagnetState magnet() const;

    // gets which parts have failed, bit 0 once no read has worked for 1 s, with the
    // controller's error flags of the last failed read as the error code
    Fault failed() const override;
};
```
