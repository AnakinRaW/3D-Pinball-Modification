# Performance options

Reserves for the case where the main loop runs out of time. None is implemented, and none is needed at the figures in [`docs/parts/ir-reflective/design.md`](../docs/parts/ir-reflective/design.md).

## Where the time goes

Blocking `analogRead` calls take the largest share of an IR phase, and almost all of it is wait: the conversion runs in the ADC hardware while the core sits on it. Converter time is a small part of a call, the rest being per-call overhead.

That is throughput, not latency. A single call stalls the loop for a fraction of what a missed ball costs.

## Levers, cheapest first

| Lever | Gives back | Costs |
|---|---|---|
| Non-blocking reads | most of the wait | a few lines of code |
| `analogReadAveraging(1)` | most of the per-read time | noise margin |
| Longer cycle | proportional to the increase | detected hits, and a missed pass cannot be recovered |

The last one goes last.

## Non-blocking reads

From the ADC library that ships with Teensyduino ([pedvide/ADC](https://github.com/pedvide/ADC)):

```cpp
bool          startSingleRead(uint8_t pin, int8_t adc_num = -1);  // returns immediately
volatile bool isComplete();                                       // per module: adc->adc0->isComplete()
int           readSingle(int8_t adc_num = -1);
```

The RT1062 carries two converters, so a pair of channels can run at the same time, halving the number of rounds:

```cpp
bool        startSynchronizedSingleRead(uint8_t pin0, uint8_t pin1);
Sync_result readSynchronizedSingle();
```

The two pins of a pair must sit on different converters. Whether the sensor pins split into such pairs is unchecked; the core's analog pin table settles it.

`enableInterrupts(isr)` and `enableDMA()` exist as well. The interrupt buys little against a phase of this length, and the DMA call only raises the request, so sequencing the channels means building that with ADC_ETC.
