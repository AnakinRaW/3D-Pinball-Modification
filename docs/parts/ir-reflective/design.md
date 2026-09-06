# IR ball sensing
## Requirements

The IR channels are how the game knows where the ball is.

| | |
|---|---|
| **Moving ball** | Report a ball crossing a point on the track, fast enough that a plunger launch is not missed |
| **Resting ball** | Report a ball at rest in a position, a lock or the drain trough, for as long as it stays there |
| **Robustness** | Hold both while environmental light changes during a game |
| **Sensor count** | Up to sixteen positions on one board. Sixteen is the case every figure is derived at, and a smaller count only relaxes it |
| **Isolation** | No conductor drives current into a Teensy pin while the Teensy is unpowered, in any state and in any switching order. |

## TL;DR from the stock machine

The stock board, its measurements and its reconstructed circuit are documented in [`research/Rokr/2_ir-reflective-sensor-p33.md`](../../research/Rokr/2_ir-reflective-sensor-p33.md). Everything below builds on the interface established there:

- Pin 1 supply,
- Pin 2 phototransistor emitter output,
- Pin 3 IR LED cathode

There is no ground pin, and no current limit for the LED on the sensor board.

The stock machine pulses the emitters to keep ambient light out of the reading.

## Design

IR sensors are the main driver of this modification to detect if a ball has passed a point on the playfield or sits stationary at some point.  

### Sensor boards

Three sensor boards come out of the stock machine. This modification wans to keep these boards, as they are already shaped for their install locations. To avoid designing multiple IR-sensing strategies it is planned that each new sensor board is build as a 1:1 copy of the stock boards.

### Mainboard

It is planned to have a central IR-sensing mainboard which provides the whole electrical infrastructure to interconnect sensor boards to the Teensy microcontroller. The mainboard therefore shall be provide a bench of connectors to the sensor boards. It shall also be responsible for IR board power supply and management.   

### Power Supply Design: 5 V vs 3.3 V

Either supply design can carry IR-reflective-based ball sensing. The stock machine supplies these boards from 5 V, and nothing on them technically asks for more than 3.3 V.

This modification chooses a **3.3 V supply design**. Teensy pins are not 5 V tolerant, so a 3.3 V board keeps the whole low-voltage side in one domain. This comes at the cost of having a voltage regulator assembly at some place to convert 5 V DC input to 3.3 V.

A 5 V board was evaluated and rejected. It would drop the need for a regulator assembly, but there are two things that count against it:

| |  |
|---|---|
| What one failure can reach | a 3.3 V board carries 5 V on two pins of one connector, so nothing on it reaches a Teensy pin above its own rail, its regulator shorting through included. A 5 V board carries it on every net, and a 5 V input no longer recognises a 3.3 V high. The Teensy's survival then rests on a translating device behaving correctly. |
| Measurement | the reading is measured against the supply, so when the supply moves the reading moves with it. A 5 V board takes its supply straight from the machine. [Lighting](../lighting/design.md) or solenoids would then directly affect the reading. A regulator holds the rail steady |

A reverse-polarity guard is planned for safety reasons, mostly to protect the Teensy from assembly blunders and short circuits.

## Design A: Continuously lit LED, rejected

In this design the emitter burns continuously and the level itself is the measurement: a ball is a rise above what the empty track returns. Nothing cancels ambient light out of that reading, so the location has to be dark.

Pin 3 sits permanently at GND through 220 Ω, and Pin 2 reaches the input across a 10 kΩ pull-down to GND. The emitter runs at 10.0 mA nominal and 10.7 mA at V<sub>OUT</sub> max with V<sub>F</sub> min, and the node ceiling is 2.98 V.

![Design A: continuously lit LED, rejected](continuous-schematic.svg)

The phototransistor cannot tell where the IR came from, so the input carries the sum of the board's own reflection and all ambient IR: daylight, lamps, light bounced off nearby objects.

| Objection | Evidence |
|---|---|
| Ambient movement swamps the signal | In the test build, small movements near the sensor shifted the level by several times what the 9 mm ball produces. A fixed threshold no longer separates ball from disturbance |
| Not calibratable | The baseline moves with time of day, room lighting and playfield surroundings. A threshold calibrated at build time is wrong an hour later, and direct sunlight can saturate the phototransistor outright |
| The stock machine does not do it either | It pulses at 333 Hz and subtracts, [as measured](../../research/Rokr/2_ir-reflective-sensor-p33.md) |

In permanent darkness (tunnel or under playfield) the design works and it delivers 4.5× the signal of a pulsed channel. A pulsed channel with 10 kΩ and a 150 Ω emitter reaches about a third of that. That gain does not pay for carrying two designs at once, so every channel uses Design B.

## Design B: Pulsing IR measurement

This follows the principle of the stock machine. The emitter is pulsed and every channel is read twice, once lit and once dark. The difference is what the emitter's own light did, so ambient light cancels out of the reading whatever it happens to be doing.

One transistor switches all emitters together, so a single Teensy pin controls the whole bus. Each phototransistor works into its own pull-down and is read independently in both phases.

![Chosen variant: pulsed LED with differential measurement](pulsed-schematic.svg)

| Net | Wiring |
|---|---|
| Supply | Pin 1 of every sensor board to the board's 3.3 V |
| Signal, per channel | Pin 2 of each board to an analog input, with a 4.7 kΩ pull-down to GND at the sensor-side node. The pull-down converts the photocurrent into a voltage; without it the output carries no measurable signal |
| LED drive, common | Pin 3 of each board through its own 220 Ω to a shared LED bus. The bus goes to the drain of one switching transistor, source to GND, gate driven through a 1 kΩ from the clock pin, with a 100 kΩ pull-down to GND at the gate |
| Ground | No ground line runs to the sensor boards. The returns are Pin 2 through its pull-down and Pin 3 through its 220 Ω and the transistor |

Every value here is derived in [the appendix](#appendix-derivations).

Everything here is per channel and holds at any channel count. What the bus and the supply add up to depends on how many channels are populated, and is in [supply](#supply-and-input-protection).

> [!WARNING]
> ### TODO: measure what a ball returns
>
>Nothing in this design computes how much extra photocurrent a ball returns. The appendix assumes 50 µA, and three figures rest on that: the 4.7 kΩ, the 235 mV across it and the 70 steps of the ten-bit reading.
>
>The datasheet characterises the sensor against a flat aluminium mirror at 4 mm. What a 9 mm ball returns depends on its curvature, its surface and its distance, and the datasheet has no curve for that.
>
>**Measure it before the mainboard is laid out.** One sensor board, two resistors and a multimeter answer the question. The 4.7 kΩ is then chosen from a measurement, and a channel that turns out short of signal costs one resistor instead of one per channel.
>
>**Setup.** A wire to ground does what Q1 does, so no switching transistor is needed. Q1 drops at most 17 mV in the built circuit, which moves the LED current by 0.08 mA.
>
>```
>3.3 V   to Pin 1
>Pin 2   to one end of a 4.7 kΩ, its other end to GND
>Pin 3   through a 220 Ω to a loose wire: on GND the emitter is lit, lifted it is dark
>
>meter   DC volts, red probe where Pin 2 meets the 4.7 kΩ, black probe on GND
>```
>
>**Procedure.** Mount the sensor at the distance it will have over the track. Check the wiring first by holding a hand in front of it with the wire on GND, which must move the reading. Then take four readings:
>
>| | clear track | ball over the sensor |
>|---|---|---|
>| wire on GND | | |
>| wire lifted | | |
>
>Subtract lifted from on-GND in each column, then subtract the two results from each other. That figure is what one channel detects, and 235 mV is what 50 µA predicts.
>
>Far below 235 mV the threshold cannot separate a ball from noise and the emitter goes brighter. At the 2.58 V ceiling the channel has no headroom left and the pull-down goes smaller. Both changes are in the table below.

## Adjustment knobs

What a channel delivers is one number: the LED-on reading minus the LED-off reading. Without a ball it is small, a ball passing makes it jump up, and the firmware compares that jump against a threshold calibrated per channel. Channels may therefore carry different resistor values, and a swap on one moves only that channel's reading.

| Issue | Reason | Change |
|---|---|---|
| One channel catches the ball sometimes and misses it sometimes, while its neighbours are reliable. The jump is there, but too small to threshold against | Too little IR comes back: that sensor sits further from the ball, at a worse angle, or over a duller patch | Make its emitter brighter: that channel's **220 Ω → 150 Ω**, raising it from 9.9 to about 14.6 mA nominal, 15.5 mA worst case |
| One channel sits near the top of its range all the time, ball or no ball, so a ball cannot push it any higher. It either never reports one or reports one permanently | Ambient IR alone already drives the phototransistor to its limit, leaving no headroom for the reflection | Make it less sensitive: that channel's **4.7 kΩ → 2.2 kΩ**. The same photocurrent then produces less voltage, so the range opens up again |

### Limits

A swap has to pass two checks.

**1. For the channel itself**:

| Resistor | Range | Set by |
|---|---|---|
| Emitter | <u>47 Ω</u> … 434 Ω | **Below:** the emitter burns out, past its 50 mA I<sub>F</sub> absolute maximum at V<sub>OUT</sub> max and V<sub>F</sub> min. **Above:** the emitter drops below the 4 mA the detector is characterised at, and the datasheet stops saying what comes back |
| Pull-down | 1 kΩ … 7 kΩ | **Below:** the gap between a ball and a clear track shrinks into the noise, 50 µA giving about 15 steps of a ten-bit read at 1 kΩ. **Above:** a reading picks up part of the channel read before it, and room light alone fills the channel's range |

*The <u>underlined</u> bound destroys hardware when crossed; the others stop the channel from working and damage nothing.*

**2. For the whole board**:
```
Σ (2332 mV / R_emitter)  +  Σ (3449 mV / (1585 Ω + R_pull-down))  +  1.9 mA  MUST BE ≤  250 mA
```

## Pin allocation

Which pins this subsystem takes, and what each one locks out, is in [`pin-assignment.md`](../../pin-assignment.md). What constrained the choice:

- **SPI is a bus.** SCK, MOSI and MISO are shared with whatever else arrives later, and each device adds only its own chip select.
- **Two chip selects and one clock line need no special function.** Three plain digital outputs, and none of them analog-capable, so the analog inputs stay free.
- **Every SPI conductor passes a buffer**, MISO through A1 on the Teensy's rail and SCK, MOSI and the two chip selects through A2 on the board's. CLOCK reaches Q1's gate directly.

## Software

### Design

The IR sensing is delivered as a library. The library exposes a public-facing API for initialization and reporting. The library also acts a the driver and owns things such as IR pulsing, timing constraints, and calibration and evaluation logic.

The library publishes events. Two event kinds, one level query and initialisation are the whole interface.

| | |
|---|---|
| `Blocked` | a ball has arrived over a channel |
| `Released` | it has gone again |
| `bool isBlocked(channel, out heldMs)` | the current level, and how long the channel has held it, for logic that asks rather than reacts |

Both events carry the channel and the moment of detection, and go into the message bus that [`input-handling.md`](../../../firmware/input-handling.md) describes.

### Driver

The driver measures, compares and reports. It reads every channel in two phases, once with the emitters lit and once with them dark, two phases to a cycle. The difference between lit and dark is the value to compare against the channel's threshold.

While this simple dark and lit comparison can be used to neutralize ambient light it can not neutralize flickering caused from e.g. LEDs lights used in this build or from the users room. The solution is to include the dark reading of the next cycle to the comparison. This way the effect of light flickering in between cycles can be reduced. The driver starts on a dark phase. Thus the sensor reading is completed after 3 phases and always in the next cycle of the read lit phase.

```
dark        mean(dark_now, dark_next)
correction  1.0125                      a rail correction factor
value       lit − correction × dark
```

*NB: The value needs a `correction` factor because the rail drifts down while the LED bus is on. This would influence the value's precision and ambient could not be cancelled exactly. Thus, the remainder is subtracted. The value `1.0125` is the midpoint of the 1.0 % typical and 1.5 % worst case from the voltage regulator.*

The threshold is one number per channel because no two sensors return the same value due to their different positions in the machine. The value sits halfway between what that channel reads over a clear track and what it reads with a ball on it. A ball over a sensor raises the read value. 

```
clear      value over a clear track,     measured at build time, per channel
ball       value with a ball on it,      measured at build time, per channel
scale      how much of that the channel still returns, read at each start
threshold  scale × (clear + ball) / 2
```
`clear` and `ball` get hardcoded per channel. `scale` is measured at initialisation: the ratio between what a channel returns now and what it returned when those two were taken. It takes out what has changed since, dust on the sensor or its ageing. Since a channel may hold a ball at the time of initialization, the driver simply calibrates against whichever of `clear` and `ball` the reading at that moment sits nearer to.

A ball is reported once two readings in a row rise above the threshold. The second reading is an additional choice against false positives. The same applies to reporting a ball has left the channel, where two readings in a row have to fall below it.

The driver is a state machine, ticked from a timer. That tick is what keeps the measurement independent of the main loop, and has to happen on time.

Every channel is read as late in the phase as it can be. The sensor is still settling after the LED switches, so a later reading carries more signal. The driver knows how many channels are fitted, and it places the read block to end with the phase at initialisation. Reading must be completed at the phase's end and must not extend into the next phase. Additionally, the read block must not start before `τ · ln 2` into the phase, where τ is the settling time constant of the sensor, because until then a channel still carries more of the previous phase than of this one.

The stock machine bounds the phase from above. Its emitter pulses with a measured 3 ms period, and it needs both windows of that period to tell a ball from ambient light, so a ball it catches stays over the sensor for at least one full period. The driver is responsible to set an appropriate phase period depending on the installed hardware (that is channels and their resistors) and weighting sensor settlement and noise elimination.

Sensors differ, either by their collector current or their assembly of the playfield. Thus, one channel can be stronger or weaker than the others. The driver sorts channel reading from strongest first to weakest channel last.

[`IR-Firmware.md`](../../../firmware/constraints.md) documents the driver initialization model and startup calibration, as well as other constraints in detail. [`channel-model`](channel-model/index.html) is an interactive, static webpage that computes the phase, the pull-down and the detection margin that follows from them.

## IR sensor mainboard

One board carries everything the channels need. It sits between the Teensy, the power distribution and the sensor boards, and it holds **up to sixteen channel positions**, each the circuit from Design B.

![IR sensor mainboard schematic](ir-sensor-mainboard.svg)

### The converters sit on the mainboard

Sixteen channels read by the Teensy directly would need sixteen of its eighteen analog pins, and those same pins carry every I²C bus and most of the serial ports. Two converters on the mainboard bring the pin cost down to a shared SPI bus and two chip selects.

**The MCP3008** ([datasheet](../../datasheets/MCP3004-3008-Microchip.pdf)) is an eight-channel ten-bit converter with an SPI interface, an internal channel multiplexer and its own sample-and-hold. Two of them cover sixteen positions.

| Multiplexer signal | Wiring |
|---|---|
| Y0 … Y7 | eight channel nodes each, sixteen across the pair |
| S0, S1, S2 | SEL0, SEL1 and SEL2 through 1 kΩ each, both devices in parallel |
| Z | through 1 kΩ to an analog input, one per device |
| E | GND. The device conducts only while E is low, and nothing drives it |
| VCC, VEE, GND | 3.3 V, GND, GND. VEE is the negative end of the switching range, and the analog signals sit at GND |

Every signal crossing between the two boards carries a 1 kΩ, so no pin passes more than 3.3 mA whichever of the two is powered.

A single 16:1 device was rejected because sixteen sequential reads take 320 µs and no longer fit the 300 µs phase. Stretching the phase by those 20 µs would be negligible in itself. The real issue is that all sixteen are then read in sequence, so the first and the last channel are sampled at very different points of the phototransistor's settling. The first channel is the weakest one, and the weakest channel decides whether the design detects reliably. At a 400 µs phase the first channel reaches 44 % of the swing with two 8:1 devices. With a single 16:1 it is sampled so early that lit and dark are still inverted, and the reading carries no meaning.

### Supply

With sixteen channels populated, the worst case for power supply is, at the regulator's output tolerance and therefore for variant A:

```
I_LED     per channel, V_OUT max and V_F min      = 10.6 mA
I_BUS     16 × I_LED                              =  170 mA
I_C       per channel, at the 4.7 kΩ pull-down    = 0.55 mA
I_MUX     both multiplexers, static and switching = 0.18 mA
I_TOT     I_BUS + 16 × I_C + I_MUX                =  179 mA
```

A smaller pull-down draws more: 0.91 mA at 2.2 kΩ and 1.33 mA at 1 kΩ. Sixteen channels at 1 kΩ, the hard bound, come to 191 mA, so that knob alone cannot break the budget.

Two variants for power supply are planned:

| Variant | Supply |
|---|---|
| **A**, production ready | Own LDO on the board, fed with 5 V from a central power distribution board |
| **B**, prototype, breadboard | 3V3 from the Teensy, no regulator, no J-PWR |

> [!WARNING]
> ### In variant A the two boards power up separately
>
> The board runs off the machine's 5 V, so either side can be live while the other is dark, and both directions push current through a protection diode:
>
> | Live | Dark | What flows |
> |---|---|---|
> | board | Teensy | 0.86 mA per selected channel into the ADC pins, back-feeding the 3.3 V rail |
> | Teensy | board | the select and clock pins into the multiplexer inputs, which would otherwise be asked to supply the whole board |
>
> Their 1 kΩ bounds each of those at 3.3 mA, against the 4 mA per Teensy pin and the ±20 mA I_IK of the 74HC4051. Two rules follow anyway.
>
> 1. Never plug or unplug the Teensy while the machine is on.
> 2. If the Teensy is fed from USB, plug the USB in first, and switch the machine off before pulling the USB out.
>
> Cutting the VUSB and VIN pads apart retires rule 2. Rule 1 still applies.

**NOTE: Variant B has no fault isolation.** A short on a sensor cable reaches the regulator inside the Teensy. In variant A the same short trips the MCP1700's own overcurrent fold-back, and the Teensy carries on.



### Regulator

**MCP1700-3302E**, TO-92 or SOT-89 - [datasheet](../../datasheets/MCP1700-Microchip.pdf). Pin order 1 = GND, 2 = V_IN, 3 = V_OUT.

The 5 V rail is shared with switching loads on other boards, each of which buffers its own inrush and clamps its own transients locally.

At sixteen channels the 5 V rail must stay above 4.18 V for the 3.3 V output to hold, the series Schottky's 0.43 V included. Upwards the rail is bounded at 5.5 V, which is what the Teensy tolerates on VIN and therefore what the machine may carry at all; the kit ships without a supply, so nothing narrower is specified.

The part carries its own protection. Overcurrent folds the pass transistor off and retries, and thermal shutdown trips at 140 °C typical.

### Connections

**To the Teensy:** MUX-A, MUX-B, SEL0, SEL1, SEL2 and CLOCK. Six pins carry sixteen channels: two analog inputs for the multiplexer outputs, four plain digital lines. Pin allocation is defined in [`pin-assignment.md`](../../pin-assignment.md).

GND depends on the power supply variant used. For **Variant A** GND is provided by 2-pin power connector. For **Variant B**, GND is provied by a Teensy pin.

**To each sensor board:** three conductors, identically wired:

| Wire | Sensor pin | Function |
|---|---|---|
| red | 1 | 3.3 V to the sensor board |
| black | 2 | Signal from the sensor board, **not GND** |
| yellow | 3 | LED cathode, through 220 Ω onto the LED bus |

### One LED driver for the whole bus

The stock machine, apparently, gives every channel its own LED driver transistor; this board shares one. That is one transistor and one Teensy pin instead of one of each per channel, and the signal path stays separate per channel either way.

Every emitter therefore pulses together. Every sensor except P31 from the stock machine sits under the track facing upwards, and none of them is in another's field of view, so nothing is lost by pulsing them at once.

## Notes for PCB Building

- **Q1 becomes an AO3400A in SOT-23** ([datasheet](https://www.aosmd.com/res/datasheets/AO3400A.pdf)). Its R_DS(on) is specified at V_GS = 2.5 V, so 3.3 V drive is a datasheet condition instead of an extrapolation.
- **The regulator** is the MCP1700-3302E in **SOT-89** or **TO-92**. Sixteen channels put 320 mW into it and both packages reach 125 °C only above 90 °C ambient, so the package is a layout choice rather than a thermal one.
- **The multiplexers** are the 74HC4051 in **SO-16** or **TSSOP-16**; the DIP-16 is the breadboard package. Both devices sit next to each other so the select lines run as one short bus.
- **C3 is X7R or X5R.** A ceramic loses capacitance under DC bias, and the regulator needs 1 µF *effective* at 3.3 V on its output. Y5V is excluded on its temperature coefficient alone, +22/−82 % across its rated range, before any bias loss is counted.

## Component list

Additional to the Teensy 4.1 and the sensor boards, grouped by type and sorted by value. Where the type designation depends on the package, both are named.

| Qty | Part | Through-hole | SMD | Use |
|---|---|---|---|---|
| up to 16 | Resistor 220 Ω | | | LED current limit, one per channel. Mandatory, the sensor board has none |
| 6 | Resistor 1 kΩ | | | One on each multiplexer output, one on each select line, and the gate resistor for Q1 |
| up to 16 | Resistor 4.7 kΩ | | | Signal pull-down, one per channel position, fitted whether or not a sensor is connected |
| 1 | Resistor 100 kΩ | | | Gate pull-down, holds Q1 off during boot |
| 2 | Analog multiplexer 8:1 | 74HC4051, DIP-16 | 74HC4051, SO-16 or TSSOP-16 | **U2, U3.** Sixteen channels onto two analog inputs |
| 5 | Ceramic 100 nF | | | **C2, C4** at the regulator input and output; **C6** at the 3.3 V entry, variant B; **C7, C8** at each multiplexer's VCC |
| 1 | Aluminium electrolytic 10 µF, ≥ 10 V | | | **C1.** Regulator input. Meets the 10 Ω source impedance of datasheet section 5.1, whose 1 µF figure is qualified for loads up to 100 mA and does not cover this board's 179 mA |
| 1 | Ceramic 10 µF X7R, ≥ 6.3 V | | | **C3.** Regulator output. 1 µF minimum and 0 to 2.0 Ω ESR per datasheet section 5.2, which a small aluminium electrolytic misses. The 10 µF hold the phase-start droop near 285 mV, and 450 mV once DC bias derates them, where 1 µF would give about 900 mV. See the derivations |
| 1 | Aluminium electrolytic 22–47 µF, ≥ 6.3 V | | | **C5.** Bulk decoupling at the 3.3 V entry, variant B only. On variant A's rail a power-up with the LED bus already on takes the start-up demand to 313 mA, see the derivations |
| 1 | Schottky diode | 1N5819, DO-41 | SS14, SMA | **D1.** Reverse polarity protection on the 5 V input |
| 1 | Logic-level MOSFET | IRL540N, TO-220 | AO3400A, SOT-23 | Shared LED switch **Q1**. An S8050, BC337 or 2N2222 substitutes for it — see the note under the derivations |
| 1 | LDO MCP1700-3302E | TO-92 | SOT-89 | **U1.** 3.3 V from the 5 V supply (variant A only). Pin order 1 = GND, 2 = V_IN, 3 = V_OUT |

### Rebuilt sensor boards

The sensors beyond the three that come out of the stock machine are rebuilt as copies of that board. The photointerrupter has no through-hole equivalent, so these are SMD.

| Qty | Part | Through-hole | SMD | Use |
|---|---|---|---|---|
| 5 | Reflective photointerrupter | — | GP2S700HCP | Emitter and detector. The stock part is presumed to be this type, see [`research/Rokr/2_ir-reflective-sensor-p33.md`](../../research/Rokr/2_ir-reflective-sensor-p33.md) |
| 5 | Resistor 1.6 kΩ | | | **R1**, collector load. The stock boards measure 1.585 kΩ, which is not a stock value; 1.6 kΩ moves the channel ceiling by 6 mV |

## Appendix: derivations

**Base quantities.** Everything below is built from these.

```
V_OUT     ±3.0 % envelope at 100 µA over T_J   3.201 … 3.399 V, 3.3 V nominal
          load regulation ±1.5 % over
          0.1 … 250 mA, so covering the bus     ±50 mV
          V_OUT max with the bus on             = 3.449 V
          V_OUT min with the bus on             = 3.151 V
V_F       LED forward voltage                 1.1 V   see the note under the 220 Ω
V_DS      drop across Q1, a full bus of
          sixteen channels at 0.1 Ω          ≤ 17 mV

U_R       across the 220 Ω, three cases, V_DS at that bound:
          3.300 − 1.1 − 0.017                 = 2.183 V  nominal
          3.449 − 1.1 − 0.017                 = 2.332 V  most current: V_OUT max, V_F min
          3.151 − 1.4 − 0.017                 = 1.734 V  least current: V_OUT min, V_F max

I_LED     U_R / 220 Ω                          =  9.9 mA nominal
                                               = 10.6 mA worst case
                                               =  7.9 mA least
          a shorter bus raises these by up to 0.08 mA, since V_DS falls with it
I_C       V_OUT max / (1.585 + 4.7) kΩ         = 0.55 mA per channel
I_BUS     N × I_LED worst case                 =  170 mA at sixteen channels
I_TOT     I_BUS + N × I_C                      =  179 mA at sixteen channels
```

V_DS is taken at the bus current it helps produce. The loop closes in one step because 17 mV is 0.8 % of U_R.

**220 Ω, LED series resistor.**

```
R_min = U_R most / 50 mA (I_F max)            ≈ 47 Ω   → 220 Ω is 4.7× above it
P     = I_LED worst² × 220 Ω                  ≈ 25 mW  → ¼ W is 10× that
```

V_F is 1.2 V typ and 1.4 V max at I_F = 20 mA per the datasheet, and lower at 10 mA; 1.1 V is the value the stock board's own diode-range reading supports. I_F absolute maximum is 50 mA, so the worst-case 10.6 mA sits 4.7× inside it.

R_min pairs the **lowest** plausible V_F with the **highest** regulator output: both push the current up, and the LED is what must survive it. At V_F = 1.4 V the same 47 Ω passes 44 mA, at 1.1 V it passes 50 mA.

**Q1 is an IRL540N because it is on hand**, a 36 A part switching 170 mA. The IRL540N specifies R_DS(on) at three gate voltages, all as maxima at I_D = 15–18 A: 0.044 Ω at 10 V, 0.053 Ω at 5.0 V, 0.063 Ω at 4.0 V. Nothing is given at 3.3 V, and the curve steepens as V_GS falls — the last volt costs as much as the previous five. **0.1 Ω is an estimate above the 4.0 V figure, not a reading.** At 1 Ω instead, the drop would be 166 mV, 7 % of U_R, which the per-channel calibration absorbs. V_GS(th) is 1.0 V min and 2.0 V max, so 3.3 V turns the part on with 1.3 V over the worst-case threshold.

**MCP1700-3302E, supply margin and heat.**

```
Dropout               350 mV max at 250 mA, so at I_TOT too
Schottky V_F          0.40 V + 0.150 Ω × I, from the 1N5819's
                      0.55 V at 1 A and 0.85 V at 3 A     = 0.43 V max at I_TOT
                                                          = 0.30 V min, its threshold alone
Output leaves 3.3 V   3.399 + 0.35 + 0.43                 = 4.18 V  at the rail
                      V_R + 3 %, dropout and the Schottky, per Note 1
Dissipation           (5.50 − 0.30 − 3.151) V × 156 mA    = 320 mW
                      rail max and V_F min maximise it. The bus current falls
                      with V_OUT, so 156 mA, taken at V_F min, applies there rather than 179 mA
θ_JA                  92 °C/W TO-92, 104 °C/W SOT-89, both per JESD51-7
T_J at 25 °C ambient  25 + 0.320 × 92                      =   54 °C  TO-92
                      25 + 0.320 × 104                     =   58 °C  SOT-89
T_J reaches 125 °C at                                     =   96 °C  ambient, TO-92
                                                          =   92 °C  ambient, SOT-89
```

The part states no fixed dissipation limit, so the ceiling follows from T_J and θ_JA and moves with the ambient. At 25 °C that is `(125 − 25) / 92` = 1.09 W in TO-92, which the 320 mW sits 3.4× inside.

**Start-up demand.**

Section 5.3 controls the output rise at 500 µs typical, 10 % to 90 % of V_R, which is the ramp of the internal reference; the output follows it as far as current allows. The pass device therefore supplies the rail capacitance times that slope, plus the load standing during the ramp.

Variant A lets the Teensy be live while the board comes up, so CLOCK can already be high and the LED bus stands through the whole ramp. That case governs, and it is taken at 90 % of 3.3 V, the top of the specified ramp.

```
dV/dt      0.8 × 3.3 V / 500 µs                       = 5280 V/s
C_rail     variant A as built: C3 10 µF,
           C4, C7, C8 at 100 nF                       = 10.3 µF
I_load     LED bus off: 16 × I_C + I_MUX               =  9.0 mA
           LED bus on at 2.97 V, V_F min and Q1 at
           17 mV: 16 × (2.97 − 1.1 − 0.017) V / 220 Ω  =  135 mA
           plus 16 × I_C at that rail, and I_MUX       =  143 mA
I_start    C_rail × dV/dt + I_load, bus off            =   63 mA   4.0× inside the 250 mA
                                     bus on            =  197 mA   1.3× inside it
           C5 at 22 µF added to the rail, bus on       =  313 mA   over it
```

The 500 µs is a typical with no stated limit, so I_start is an estimate rather than a bound, which those margins carry. Crossing the 250 mA does not destroy the part: section 6.5 makes it a maximum average continuous rating and puts the current limit at 550 mA typical, so the ramp stretches instead. Being above a specified figure is what keeps C5 off variant A's rail, and 22 µF is the bottom of C5's range, so no value of it fits.

The ceiling for the ramp to fit inside 250 mA is `(250 − 9.0) mA / 5280 V/s` = 45.6 µF of rail capacitance with the LED bus off, and `(250 − 143) mA / 5280 V/s` = 20.3 µF with it on.

**Phase-start droop.**

Turning the LED bus on steps the rail load from 9.0 mA to 179 mA, and C3 carries that step until the loop responds. The datasheet specifies no transient figure, so the response time is read off its own dynamic load step curves and the droop follows from it.

```
Figure 2-19   1 µF ceramic, 100 mA step, 500 mV/div:
              dip ≈ 1.0 division                       =  530 mV
              charge C × ΔV                            = 0.53 µC
              t_eff = charge / step                    =  5.3 µs
Figure 2-22   22 µF at 1 Ω ESR, 200 mA step:
              dip 350 mV, less its 200 mV ESR step     =  150 mV
              t_eff                                    = 16.5 µs
ΔI            179 mA − 9.0 mA                          =  170 mA
Droop         t_eff rises with √C across those two
              points, so the droop scales as ΔI / √C:
              530 mV × 170/100 / √10, C3 nominal        =  285 mV
              at 4 µF effective under 3.3 V DC bias     =  450 mV
              at 1 µF in place of C3                    =  900 mV
```

Both dips are read off a printed graph, so the droop is an estimate. It costs LED drive while it lasts: 450 mV against the 2332 mV across the 220 Ω is 19 % of the emitter current, over the first tens of µs of the lit phase, which the settling figure for the first channel takes no account of. Load release overshoots by the same order, putting the rail near 3.75 V and the signal node at `3.75 V × 4.7 / 6.285` = 2.80 V, inside the 3.3 V pin limit.

**1 kΩ, series resistor in the signal line.**

```
Channel ceiling   V_OUT max × 4.7 / (1.585 + 4.7)     = 2.58 V
                  the multiplexer's R_ON carries no steady
                  current, so it moves this by nothing
Unpowered pin     clamps at                          ≈ 0.7 V
                  node solves from
                  (3.449 − V)/1.585 = V/4.7 + (V − 0.7)/1, in kΩ and mA
                  V                                   = 1.56 V
Into the pin      (1.56 − 0.7) V / 1 kΩ               = 0.86 mA  per channel
                  without the resistor, the board's own 1.585 kΩ is the only limit:
                  (3.449 − 0.7)/1.585 − 0.7/4.7       = 1.59 mA
Source impedance  sensor dark: 1 kΩ + 4.7 kΩ          = 5.7 kΩ
                  4.7 kΩ from the signal, 1 kΩ from the protection
```

**74HC4051, what the part has to survive.**

| Quantity | Limit | In this design | Datasheet |
|---|---|---|---|
| Supply voltage | 2.0 … 10.0 V | 3.3 V | Table 5 |
| Switch voltage | GND to VCC | the channel node reaches 2.58 V | Table 5 |
| Switch current | ±25 mA absolute maximum | a channel delivers at most 2.75 mA, Pin 1 shorted to Pin 2 with the Teensy dark: (3.449 − 0.7) V / 1 kΩ | Table 4 |

R_ON is specified at 2.0 V and 4.5 V but not at 3.3 V, and no margin rests on it. An ADC input takes charging current for its sample capacitor and no steady current, so the switch resistance produces no offset in the reading, only slower charging, which the long sample setting already covers. Leakage rests on nothing either: it is static, so it sits in the lit and the dark reading alike and cancels in the difference.

**1 kΩ, gate resistor.** A MOSFET gate is a capacitor. No current flows through it while it is held on, but each switching edge has to charge it, and only this resistor limits that.

```
I_peak = 3.3 V / 1 kΩ                               = 3.3 mA  → inside the 4 mA figure
t      ≈ Q_g / (I_peak / 2) = 64 nC / 1.65 mA       ≈ 39 µs   = 13 % of a 300 µs phase
  with 100 Ω instead: 3.3 V / 100 Ω                 = 33 mA   → 8× the 4 mA figure
```

Q_g = 64 nC max, [IRL540N datasheet](http://www.redrok.com/MOSFET_IRL540N_100V_36A_44mO_Vth2.0_TO-220.pdf). The 39 µs edge costs nothing, because the earliest channel is sampled 240 µs into its phase, by which time the LED has been at full current for 200 µs. V_GS(th) is 1–2 V max, so 3.3 V does turn the part on.

**4.7 kΩ, signal pull-down.** The phototransistor delivers a current; the pull-down turns it into the measured voltage, `U = I_photo · R`. Four requirements set the value, against the board's internal 1.585 kΩ:

```
Headroom          U_max = 3.449 V × 4.7 / (1.585 + 4.7)    ≈ 2.58 V  at V_OUT max
Resolution        50 µA × 4.7 kΩ = 0.235 V                 ≈ 73 steps of a 10-bit ADC
                  × 48 % swing at the first read           ≈ 35 steps, worst case
Ambient headroom  saturates at 3.151 V / 6.285 kΩ          ≈ 501 µA  at V_OUT min
                  at 10 kΩ instead                         ≈ 272 µA  → 4.7 kΩ has 1.8× the margin
Settling          t_r/t_f max 100 µs at R_L = 1 kΩ         → ≈ 470 µs at 4.7 kΩ (10–90 %)
                  τ = 470 µs / 2.2                         = 214 µs  worst case
                  typical part: 20 µs → 94 µs              =  43 µs
                  swing at the first read, taken at 240 µs of
                  300 µs from an assumed 60 µs read block:
                                                              48 % worst case
                                                              99 % typical
                  sign inverts below τ · ln(2 / (1 + x))   = 101 µs  → 2.4×
                  that instant rises with τ towards T/2, so a
                  read past the phase midpoint holds its sign
                  for any part
```

**The swing is the periodic one, not a single step.** At 50 % duty neither phase reaches its endpoint, so the phase-to-phase difference stays under what one step from rest would give. With `x = e^(−T/τ)` the two endpoints settle at `A / (1 + x)` and `A · x / (1 + x)`, and the difference at time `t` into either phase is

```
swing(t) = A · [ 1 − 2 · e^(−t/τ) / (1 + x) ]

at T = 300 µs, τ = 214 µs, t = 240 µs:  x = 0.246
  swing = A · [ 1 − 2 · 0.326 / 1.246 ]   = 0.48 A
zero at e^(−t/τ) = (1 + x) / 2, so t     = τ · ln(2 / (1 + x))
```

Below that instant the decaying dark trace still sits above the rising lit trace and the difference carries the opposite sign. The figures take the LED current as a step; the 39 µs gate edge derived above is 0.18 τ, which makes them slightly optimistic.

**Where the 50 µA comes from.** The datasheet characterises I_C at 60 µA minimum and 410 µA maximum, at I_F = 4 mA, V_CE = 2 V and d = 4 mm against an aluminium-evaporated mirror on glass. Two factors separate that condition from this design and pull in opposite directions: the emitters run at 9.9 mA, 2.5× the characterising current, and the 9 mm ball returns less than a mirror. The datasheet gives no curve of I_C against forward current, so neither factor can be computed. The 50 µA sits below the datasheet minimum at a quarter of the drive current. The measurement that settles it is in [TODO: measure what a ball returns](#todo-measure-what-a-ball-returns).

A smaller value is faster and more tolerant of ambient light, and less sensitive. At the 240 µs read instant those two effects cancel from 4.7 kΩ upwards, so 4.7 kΩ carries the most ambient headroom available at full signal.

**The 48 % is accepted, not fixed by a longer phase.** Settling and sample count pull against each other, and the amplitude loss is the cheaper one to pay: 112 mV is still 35 steps of a 10-bit read, whereas a missed pass cannot be recovered.

| Phase | Cycle | Swing at the first read, worst case | Cycles per pass, 3 mm window at 2 m/s | 6 mm at 1 m/s |
|---|---|---|---|---|
| 300 µs | 0.6 ms | 48 % | 2.5 | 10 |
| 650 µs | 1.3 ms | 83 % | 1.2 | 4.6 |
| 1000 µs | 2.0 ms | 95 % | 0.75 | 3 |

**1 kΩ, base resistor of the LED transistor (NPN variant).**

```
I_B  = (3.3 − 0.7) V / 1 kΩ           = 2.6 mA
       Teensy recommended maximum       4 mA   → 2.6 mA fits, a smaller R would not
h_FE needed = 156.3 mA / 2.6 mA       ≈ 60    worst case, V_OUT max, sixteen channels
       S8050 specifies 85 minimum             → over-driven by 1.4×
```

The gate needs a **100 kΩ pull-down to GND**, so the LEDs cannot switch on undefined while the Teensy boots with its pins still high-impedance.

**An NPN works as a substitution.** Same resistors in the same places — the 1 kΩ becomes the base resistor at I_B = 2.6 mA, the 100 kΩ still holds the base down during boot. The one difference is the saturation voltage: ≈0.2 V against the MOSFET's 17 mV, which drops the LED current from 9.9 to 9.1 mA per channel. Either accept that 8 %, which the per-channel calibration absorbs, or fit **200 Ω** instead of 220 Ω, which lands on 10.0 mA, a shade over the MOSFET’s 9.9 mA. The MOSFET is the documented build because it needs neither change, and at a full sixteen-channel bus the NPN's headroom is thin: 1.4× on current gain, with V_CE(sat) rising as I_C does.

**Why Pin 1 needs no external resistor.**

```
I_C,max = 3.449 V / (1.585 + 4.7) kΩ ≈ 0.55 mA  ≪ 20 mA (datasheet I_C maximum), V_OUT max
```

The board's internal 1.585 kΩ already limits the phototransistor branch. Only the LED path on Pin 3 has no limit on the board and needs one externally.

**Bounds on the adjustment knobs.** Each is the point at which the design stops being safe or stops working.

```
220 Ω  lower bounds take the largest U_R, V_OUT max and V_F min:
                             U_R = 3.449 − 1.1 − 0.017             = 2.332 V
        lower, one channel   R = 2.332 V / 50 mA (I_F max)         =   47 Ω
        lower, every channel R = N × U_R / (250 mA − N × I_C − I_MUX)
                             at N = 8                              =   76 Ω
                             at N = 16                             =  155 Ω
        the upper bound takes the smallest U_R, V_OUT min and V_F max:
                             U_R = 3.151 − 1.4 − 0.017             = 1.734 V
        upper                datasheet characterises I_C at I_F = 4 mA:
                             R = 1.734 V / 4 mA                    =  434 Ω

4.7 kΩ lower                 resolution at a 50 µA delta:
                             1 kΩ → 50 mV → 15.6 steps of 3.2 mV
        upper                not signal loss — see the table below.
                             ambient headroom halves and the reading
                             becomes drift-prone

1 kΩ   lower                R = 3.3 V / 4 mA                       = 825 Ω
gate    upper                t = Q_g × 2R / 3.3 V ≤ 100 µs
                             → R = 100 µs × 3.3 V / (2 × 64 nC)    = 2.6 kΩ

100 kΩ lower                 gate voltage = 3.3 V × R / (R + 1 kΩ)
                             must clear V_GS(th) max 2.0 V
                             1.5 kΩ → 1.98 V, too close; 10 kΩ → 3.00 V
        upper                gate leakage × R stays below the threshold:
                             100 nA × 1 MΩ = 100 mV

300 µs lower                 swing at 0.8 of the phase, τ = 214 µs:
                             150 µs → 24 %, 200 µs → 32 %, 300 µs → 48 %
                             the 0.8 rule clears the sign inversion by
                             1.9× or better at any phase length, since
                             both instants scale with τ
        upper                three cycles inside the 3 ms dwell
                             → cycle ≤ 1 ms → phase                  = 500 µs

```

One standard value inside each of those walls:

```
100 Ω    above the 47 Ω single-channel bound  → 23 mA on that one channel
390 Ω    below the 434 Ω characterising point → 4.4 mA at the weakest corner
2.2 kΩ   twice the 1 kΩ resolution floor      → 91 mV, 28 steps per 50 µA
10 kΩ    the ambient wall, and 0.32 V under the pin limit
```

**The pull-down's upper bound is ambient headroom.** The effective signal is `I × R × swing(R)`, and τ grows with R, so from 4.7 kΩ upwards the rising resistance and the falling swing cancel and the signal flattens while the headroom keeps halving:

| R | τ | swing at the first read | effective signal at 50 µA | ambient light that maxes the channel out |
|---|---|---|---|---|
| 1 kΩ | 46 µs | 99 % | 49 mV | 1219 µA |
| 2.2 kΩ | 100 µs | 83 % | 91 mV | 832 µA |
| **4.7 kΩ** | 214 µs | 48 % | 112 mV | 501 µA |
| 10 kΩ | 455 µs | 22 % | 111 mV | 272 µA |

The 100 nA gate leakage is the usual I_GSS specification for this class of MOSFET, not a figure read from the IRL540N datasheet. It only sets the upper bound of a value that is already an order of magnitude away, so the conclusion does not rest on it.

## Sources

- [`research/Rokr/2_ir-reflective-sensor-p33.md`](../../research/Rokr/2_ir-reflective-sensor-p33.md) — the stock board's circuit, the measured 1.585 kΩ, the 333 Hz pulsing and the ambient-light reasoning behind it
- [`research/teensy-4.1.md`](../../research/teensy-4.1.md) — the 3.3 V input limit and the 4 mA per-pin recommendation
- [Sharp GP2S700HCP datasheet](../../datasheets/IR-reflective-gp2s700hcp_e.pdf) — V_F, I_F and I_C maximums, the I_C transfer characteristic at I_F = 4 mA, t_r/t_f against load resistance, optimal sensing distance
- [Changjiang S8050 datasheet via LCSC](https://datasheet.lcsc.com/lcsc/Changjiang-Electronics-Tech-CJ-S8050_C2146.pdf) — minimum current gain
- [Microchip MCP1700 datasheet](../../datasheets/MCP1700-Microchip.pdf), DS20001826E. Output tolerance, load regulation, dropout, θ_JA per package, the capacitor and ESR requirements of sections 5.1 and 5.2, the output rise time of section 5.3, the dynamic load step curves of Figures 2-19 and 2-22, the protection features and the pinning
- [ST 1N5817/18/19 datasheet](../../datasheets/1n5817.pdf) — DocID6262 Rev 5. Forward voltage at 1 A and 3 A, from which the V_F bracket at I_TOT follows
- [IRL540N datasheet](http://www.redrok.com/MOSFET_IRL540N_100V_36A_44mO_Vth2.0_TO-220.pdf) — gate threshold voltage and total gate charge
