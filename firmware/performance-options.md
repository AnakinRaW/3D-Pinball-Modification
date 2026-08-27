# Performance options

Reserves for the case where the main loop runs out of time. None is implemented, and none is needed at the figures in [`constraints.md`](constraints.md).

## Where the time goes

Eight blocking `analogRead` calls cost about 160 µs of each 300 µs phase, so roughly 53 % of the cycle. Almost all of it is wait — the conversion runs in the ADC hardware while the core sits on it. The datasheet puts one conversion at 1.25 µs at the slowest sample setting, so four averages come to about 5 µs of converter time; the rest is per-call overhead.

That is throughput, not latency. A single call stalls the loop for 20 µs, far under the ≈ 3 ms that loses a ball.

## Levers, cheapest first

| Lever | Gives back | Costs |
|---|---|---|
| Non-blocking reads | ≈ 50 % of the cycle | a few lines of code |
| `analogReadAveraging(1)` | 20 µs → 5.7 µs per channel | noise margin |
| Longer cycle | proportional to the increase | detected hits, and a missed pass cannot be recovered |

The last one goes last.

## Non-blocking reads

From the ADC library that ships with Teensyduino ([pedvide/ADC](https://github.com/pedvide/ADC)):

```cpp
bool          startSingleRead(uint8_t pin, int8_t adc_num = -1);  // returns immediately
volatile bool isComplete();                                       // per module: adc->adc0->isComplete()
int           readSingle(int8_t adc_num = -1);
```

The RT1062 carries two converters, so a pair of channels can run at the same time — four rounds instead of eight:

```cpp
bool        startSynchronizedSingleRead(uint8_t pin0, uint8_t pin1);
Sync_result readSynchronizedSingle();
```

The two pins of a pair must sit on different converters. Whether S1–S8 split into four such pairs is unchecked; the core's analog pin table settles it.

`enableInterrupts(isr)` and `enableDMA()` exist as well. The interrupt buys little against a 300 µs phase, and the DMA call only raises the request — sequencing eight channels means building that with ADC_ETC.
