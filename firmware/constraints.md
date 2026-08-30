# Firmware constraints

Rules the firmware has to keep, derived from the hardware design. Rough notes, not a specification.

## Nothing in the main loop may block

The IR sensing is planned to run as a non-blocking state machine, 300 µs per phase, 600 µs per full cycle over all eight channels, see [`docs/parts/ir-reflective/design.md`](../docs/parts/ir-reflective/design.md). That only holds if **every other participant in the loop is also non-blocking**. A ball dwells in a sensor's detection window for around 3 ms, so anything that stalls the loop for longer than that loses a hit outright, and it does so intermittently.

**RGB LEDs are the likely offender.** The protocol needs 1.25 µs per bit, 24 bits per LED:

| Strip | Blocked for |
|---|---|
| 60 LEDs | 1.8 ms |
| 160 LEDs | 4.8 ms |
| 320 LEDs | **9.6 ms**, longer than three ball passes |

Which library decides whether that time is blocked or not:

| Library | Behaviour |
|---|---|
| [OctoWS2811](https://www.pjrc.com/teensy/td_libs_OctoWS2811.html) | DMA, near-zero CPU, interrupts stay enabled. **Use this** |
| [WS2812Serial](https://www.pjrc.com/non-blocking-ws2812-led-library/) | Also non-blocking, PJRC's own |
| FastLED | Software-timed on its own clockless output, disruptable by other interrupts. Over `USE_OCTOWS2811` it hands the transfer to OctoWS2811 instead |
| Adafruit NeoPixel | Disables all interrupts for the whole frame. **Do not use** |

**FastLED is the frontend, OctoWS2811 the output engine.** FastLED holds the pixel array, the effects and the power limit, and OctoWS2811 does the DMA transfer.

The same rule applies to anything else that arrives later: displays, SD card writes, audio buffer refills. If it can stall for milliseconds, it needs DMA or chunking.

## ADC budget

Eight channels are read in each phase, and the reads have to finish inside it.

| Averaging | Per channel | Eight channels | Of a 300 µs phase |
|---|---|---|---|
| four, the Teensy 4 default | ≈ 20 µs | 160 µs | 53 % |
| `analogReadAveraging(1)` | ≈ 5.7 µs | 46 µs | 15 % |

The default is what this design uses. It fits, and the averaging buys noise margin that the 9 mm ball's weak signal needs.

What eats the budget: another channel costs a further 20 µs, and each doubling of the averaging costs the whole read again. At 53 % neither is free.

The 53 % is time spent waiting, not computing: a blocking `analogRead` holds the core while the ADC converts. It costs throughput rather than latency, since one call stalls the loop for 20 µs against the ≈ 3 ms that loses a ball. Should the loop ever run short of time, [`performance-options.md`](performance-options.md) lists what can be traded for it.

## Sampling instant

Sample at the **end** of each phase, not the start. The phototransistor is still settling, so the reading depends on when in the phase it is taken. One phase leaves it 75 % settled in the worst case, and reading early costs more than that.

## Sample window

An IR channel is a 5.7 kΩ source ([design.md](../docs/parts/ir-reflective/design.md)). NXP allows 1 kΩ at the fast sample setting ([IMXRT1060CEC](../docs/datasheets/IMXRT1060CEC.pdf) Rev. 1, Table 54, page 63), so use the long one:

```cpp
adc->adc0->setSamplingSpeed(ADC_SAMPLING_SPEED::VERY_LOW_SPEED);
```

Without it the readings come out too low, by an amount that depends on which channel was read before, so detection would quietly vary with where the balls are. The setting costs 600 ns per reading against a 300 µs phase.
