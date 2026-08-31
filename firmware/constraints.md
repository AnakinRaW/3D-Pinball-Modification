# Firmware constraints

Rules the firmware has to keep. The figures they are checked against live in the design documents and are read from there: [`docs/parts/`](../docs/parts/), one directory per subsystem.

## Nothing in the main loop may block

The IR sensing runs as a non-blocking state machine, see [`docs/parts/ir-reflective/design.md`](../docs/parts/ir-reflective/design.md) for its phase and cycle times. That only holds if **every other participant in the loop is also non-blocking**. A ball dwells in a sensor's detection window for the time that document derives, and anything stalling the loop for longer loses a hit outright, intermittently.

**RGB LEDs are the likely offender.** A clockless strip's frame time grows with the number of LEDs, and past some strip length it exceeds the dwell. Compute it from the LED count in the lighting design and hold it against the dwell before a strip is chosen.

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

Every pulsed channel is read in both phases, and those reads have to finish inside the phase. The sum is the channel count times the per-read cost, which follows from the averaging setting; both, and the phase length they have to fit into, come from the design document.

The default averaging is what the design assumes, because it buys noise margin that the 9 mm ball's weak signal needs. `analogReadAveraging(1)` is the reserve to spend if the reads stop fitting.

A blocking `analogRead` holds the core while the ADC converts, so this budget is time spent waiting. It costs throughput rather than latency: one call stalls the loop for a fraction of what a missed ball costs. Should the loop run short of time, [`performance-options.md`](performance-options.md) lists what can be traded for it.

## Sampling instant

Sample at the **end** of each phase. The phototransistor is still settling after the emitters switch, so a reading taken earlier is smaller than the design assumes.

## Sample window

A channel's source impedance is well above what the ADC's fast sample setting tolerates ([IMXRT1060CEC](../docs/datasheets/IMXRT1060CEC.pdf) Rev. 1, Table 54, page 63), so use the long one:

```cpp
adc->adc0->setSamplingSpeed(ADC_SAMPLING_SPEED::VERY_LOW_SPEED);
```

Without it the readings come out too low, by an amount that depends on which channel was read before, so detection would quietly vary with where the balls are.
