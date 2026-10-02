# IR Ball Sensing Driver

The driver turns the IR channels into ball events, and takes its key information from [`docs/parts/ir-reflective/design.md`](../../docs/parts/ir-reflective/design.md).

## What the driver does

The emitters pulse in two phases, lit and dark, and every registered channel is read in both. A ball changes the difference between the two readings, and the room light drops out of it. A channel reports a ball once that difference crosses its own threshold twice in a row, and reports the ball gone once it falls under the release threshold twice in a row.

## Driver events

The driver publishes two kinds of events. Each names the channel as its source, carries the instant of detection as its timestamp, and goes into the queue that [`input-handling.md`](../input-handling.md) describes.

| Event | When | Payload |
|---|---|---|
| `Detected` | a ball has arrived over the channel | none |
| `Released` | the ball has gone again | the dwell in milliseconds |

Game logic can still poll the channel state with `State getState(channel, out ms)`.

## Reading the channels

Reading goes on continuously, in phases of one fixed length `T`. A phase toggles the emitters at its start, waits while the sensors answer that toggle, reads every registered channel, and evaluates what it read. The channels hang on converters that the Teensy reads over SPI. Reading one channel is one conversion: the converter samples the voltage at that channel and shifts a ten-bit number back over that bus.

A free-running counter in the Teensy controls those phases, FlexPWM3.1, driven from the 150 MHz peripheral clock and recorded in [`docs/pin-assignment.md`](../../docs/pin-assignment.md). It drives the CLOCK conductor, which switches every sensor LED to on and off. One cycle is `2T`, a dark phase and then a lit one at 50 % duty. The driver owns channels 2 and 3 of QuadTimer3, so no other code may use them, `analogWrite()` included.

The firmware never stops or stretches that counter, so a phase is a clock the driver cannot hold.

### Phase length

The phase length `T` is not a constant of the firmware. At initialisation the driver computes it from the number of channels registered with it. The parameters that enter it:

```
D         a ball's dwell over a sensor            = 3 ms, estimated from the stock machine's measured emitter period
k         readings in a row a hit has to survive  = 2
f_SPI     the rate the ADCs tick                  = 1.35 MHz
t_conv    one channel, 24 ticks, three bytes      = 17.78 µs
t_ovh     the firmware's own cost per channel     = 2 µs, reserved, not yet measured
t_inv     the earliest a channel may be read, τ · ln 2 = 48.5 µs
jit       reserve kept at the end of the phase     = 10 µs, a placeholder
N         channels registered with the driver
t_budget  N · (t_conv + t_ovh), up to the next 10 µs    how long reading every channel takes
t_first   T − t_budget − jit                     when sensor reading has to start
```

**The dwell sets the longest possible phase length.** The two readings a hit has to survive have to fit inside that dwell. One value is the lit reading less the mean of the two dark ones either side of it, so it spans three phases, `[dark, lit, dark_next]`, which gives `(2k + 1) · T ≤ D`. At `k = 2` two readings therefore span five phases.

**The read sets the shortest possible phase length.** Every registered channel has to be read inside the current phase, one after another. None of them may be read right at the start of a phase, where a sensor's output still sits near the level of the previous one. The read block therefore starts no earlier than `t_inv` and ends with the phase. A phase can hold `N` channels only if `t_budget + t_inv + jit ≤ T`.

Between the two bounds, a longer phase leaves the sensors more time to settle and returns more signal, and it demands a longer dwell, which is to say a slower ball. `T_max` is a price rather than a wall: it follows from the dwell this design assumes, and the fastest ball a phase still confirms is the ball's diameter over `(2k + 1) · T`, 3 m/s at the phase this design runs. At initialisation the driver works through these steps:

1. `T_max`, the longest phase the dwell allows: `D / (2k + 1)`, down to the next 10 µs.
2. `t_budget`, what reading its own `N` channels costs: `N · (t_conv + t_ovh)`, up to the next 10 µs.
3. The range that works at all, `t_budget + t_inv + jit` up to `T_max`. An empty range means `N` does not fit, and the driver refuses to run.
4. `T`, by walking that range in steps of 10 µs and keeping the phase where the first channel has settled furthest and a pass still delivers `k` readings.
5. `t_first`, when the read block starts: `T − t_budget − jit`.

```
T_max     = 3 ms / 5, down to the next 10 µs               = 600 µs
t_budget  = 16 · (17.78 µs + 2 µs), up to the next 10 µs   = 320 µs
range       320 µs + 48.5 µs + 10 µs ≤ 600 µs              holds, so sixteen channels fit
T         = 600 µs, the longest phase in that range
t_first   = 600 µs − 320 µs − 10 µs                        = 270 µs
```

[`channel-model`](../../docs/parts/ir-reflective/channel-model/index.html) picks the phase on further criteria, among them the false-report target and the weakest sensor's margin. The driver can follow it later with the same arithmetic.

### Execution and timing

No stage of a phase runs in the main loop. The Teensy's hardware components are responsible for holding the exact timing.

| Stage | Constraint | Hardware |
|---|---|---|
| Phase start | a constant phase length | channel 2 of QuadTimer3, the counter that controls the phases |
| Settling | no channel read before `t_inv` into the phase | channel 3, which channel 2 restarts at every phase start and whose compare at `t_first` starts the transfer |
| Reading | all channels read before the phase ends | a DMA transfer, with the core free for the loop |
| Evaluating | finished before the next block's interrupt arrives | the interrupt of the last conversion |

The core spends one interrupt at `t_first` to start the transfer, one per conversion, and one evaluation per phase, whatever the loop is doing. The evaluation may run past the end of its phase. Its deadline is the start of the next read, `jit + t_first` after the last conversion, and nothing touches the sensors in between.

At sixteen channels, the conversions hold the sensors' SPI bus for 320 µs of every phase.

### Reading sensor values

The MCP3008 limits the SPI clock to **1.35 MHz**, and one conversion takes 24 clocks at that rate, 17.78 µs. On top of its clock time, each conversion costs 2 µs for the chip's own timing and for interrupting on completion.

`SPI.transfer(buf, retbuf, count, event)` on the Teensy 4.x core runs the transfer asynchronously and calls back through an `EventResponder` when it is done. The core runs the main loop meanwhile.

Every conversion runs as its own transfer. The MCP3008 samples only when it is addressed anew, so one transfer spanning the whole block would return a single reading.

The driver keeps starting conversions while `t_conv + t_ovh` still fits in what is left of the phase, and stops at the first one that does not. The channels after that point go unread, and the driver logs the miss and margin. The evaluation stage is activated regardless.

> [!WARNING]
> ### TODO: measure `t_ovh`
>
> Read all `N` channels, time the block with `ARM_DWT_CYCCNT`, and take `t_ovh = block / N − t_conv`. Measure under load and keep the maximum over a long run. The constraint is `t_ovh <= (T − jit − t_inv) / N − t_conv`.

## Calibration

### The build step

The build step is performed once with the sensors already in the playfield, and the driver has its results from then on. It is repeated whenever the playfield changes under a sensor.

**The build step records two figures per channel.** The detector's collector current spreads from 60 µA to 410 µA under identical conditions, so a pair that fits one channel misses another by several times over. [`docs/parts/ir-reflective/design.md`](../../docs/parts/ir-reflective/design.md#driver) holds what a channel is expected to deliver.

The pair also depends on where a channel sits in the read order as the sensors are still rising when the read block starts. A channel read early returns a smaller difference for the same ball. Both effects are fixed per channel, so the build step settles the pair and the read order together. Adding a channel or moving one voids them and needs the build step run again. The ranking puts the weakest sensor last, where the swing is largest.

One row per sensor:

| Field | |
|---|---|
| channel | the number the game uses for that sensor |
| `clear_build` | the channel's value over a clear track, in converter steps |
| `ball_build` | the same with a ball on it |
| read order index | the index of the read order for that channel |

The installation has to give every channel a clear-track difference of at least 1.5× its noise, because the start-up calibration scales each channel by that difference. Averaged over two hundred readings, the factor then stays within 5 %. The build step measures the noise the way the start-up calibration does and checks this for every channel.

### At start-up

**A build step cannot cover drift.** The supply voltage sets how much light a channel puts out, and it can sit anywhere in its tolerance band from one boot to the next. Ambient temperature moves the detector's collector current. The emitter ages over the years. All three scale the clear reading and the ball reading together, so a threshold built from two fixed numbers walks off the middle between them. The startup calibration therefore measures a factor per channel, from the average of two hundred readings, because a single reading would carry its noise into the channel's threshold. It waits five cycles first, while the sensor outputs and the converters' reference settle.

**Reporting and release use two thresholds.** A ball is reported once two readings in a row exceed `threshold`, and released once two in a row fall under `release`. Thus, a ball resting near `threshold` does not report and release on noise alone. 

The release margin `m_r`, how far `release` sits under `threshold`, is defined for one false release a year, counted in multiples of the noise `σ` a channel's value carries with nothing moving over it. A ball resting exactly on `threshold` is released only when the noise falls under `−m_r σ` twice in a row. `Q(m_r)² × readings per second = 1 / year` settles `m_r`, where `Q` is the tail of the normal distribution. `release` also has to stay 2 σ above `clear`, or a ball that leaves is never released. For a channel that means the margin condition, `(ball − clear) / 2 ≥ (m_r + 2) σ`.

At calibration, a channel that misses the margin condition restarts the driver with a longer phase. With a longer phase `ball − clear` grows while `m_r` falls. The price is lower ball speed for detection. If the first phase adjustment still fails the margin condition, a second driver restart falls back to 1.5 ms, the phase the stock machine runs. There is no third attempt, and a channel still failing is reported.

**The startup calibration also reads the noise floor.** The same two hundred readings, a quarter of a second, estimate σ on one clear channel to within 5 %. Less accuracy would cost: a σ 10 % low turns one false release a year into one every ten days. σ sets `release` alone, because a phase tuned to it would fit the machine to the room it booted in.

> [!WARNING]
> ### TODO: measure σ on the finished board
>
> The σ in [`channel-model`](../../docs/parts/ir-reflective/channel-model/index.html) is an estimate, so the phase and the margin condition rest on one. The start-up calibration measures σ on every boot, but only to set `release`, and nothing carries it back. Run it once on the finished board with the lighting subsystem running and the room lit by a flickering LED lamp rather than by daylight, and put the σ it reports into `channel-model`.

**A ball lying on a channel at power-up needs no special handling.** That channel reads about five times its clear value, while drift moves a reading only by tens of per cent, so the calibration tells the two cases apart and scales against `ball_build`.

## Start-up

```
begin():
    out = events.attach(queue, kQueueDepth)
    csA, csB = HIGH, HIGH          # before SPI, or both converters answer at once
    ledGate  = LOW                 # never INPUT_PULLUP, the isolator reads that as a high
    SPI.begin()

    planPhase(n)                   # T and t_first from the channel count, or refuse to run
    startPhaseCounter()            # QuadTimer3 channels 2 and 3, free-running from here
    discard(SETTLING_CYCLES)       # sensors and the converters' reference settle
    calibrate()
    deviceMonitor.watch(IrSensing, self)     # asks failed() every 100 ms from here on

calibrate():
    v, sigma = averages()          # every channel's mean and one clear channel's σ, from the same readings

    for c in channels:
        if   v[c] near c.clear_build:  c.scale = v[c] / c.clear_build
        elif v[c] near c.ball_build:   c.scale = v[c] / c.ball_build
        else:                          c.scale = 1;  c.calFailed = true

        c.clear, c.ball = c.clear_build * c.scale, c.ball_build * c.scale
        c.threshold     = (c.clear + c.ball) / 2
        c.release       = c.threshold - m_r * sigma
        c.lastGoodMs    = millis()

    if not marginHolds():          # (ball - clear) / 2 >= (m_r + 2) * sigma
        raisePhase() or fallBackToStockPhase() or report(Degraded)
        restart()
```

## Device faults

A channel counts as failed once its lit-minus-dark difference has stayed under half its clear value for 1 s. A ball over the sensor reflects more light and makes the difference larger, never smaller. A difference that stays low therefore means a channel.

The evaluation notes the time whenever a channel's difference reaches half its clear value. [`failed()`](../error-handling.md#device-faults) then checks for each channel whether that time is more than 1 s ago. A channel whose clear difference is small against its noise may not report a lost emitter, because the noise alone then keeps reaching half its clear value.

At start-up, `calibrate()` expects every channel to read close to its clear value, or close to its ball value when a ball lies on it. A channel that reads neither counts as failed until the next start.

```
note(c, diff):                     # after the ball logic, for every new difference
    if diff >= c.clear / 2:        # a ball only raises the difference
        c.lastGoodMs = millis()

failed():                          # one bit per channel, for the device monitor
    parts = 0
    for c in channels:
        if c.calFailed or elapsedMs(c.lastGoodMs, 1 s):  parts |= 1 << c
    return Fault(parts, 0)            # no error code
```
