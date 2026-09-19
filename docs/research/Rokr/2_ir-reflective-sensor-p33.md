# Stock IR reflective sensor board (P33)

The stock ROKR EG01 senses the ball with three-wire boards such as the **P33**, carrying one reflective photointerrupter and one 1.585 kΩ resistor. Drive and evaluation sit on the mainboard.

The photointerrupter is consistent with a **Sharp GP2S700HCP** ([datasheet](../../datasheets/IR-reflective-gp2s700hcp_e.pdf)). The identification rests on package visuals and geometry as well as on the pad functions matching the datasheet's internal connection diagram pin for pin; the part number is not readable on the package.

## Sensor board

![Reconstructed schematic of the P33 sensor board](2_P33-IR-Reflective-Schematic.svg)

| Pin | Function | Same node as |
|---|---|---|
| 1 | +5 V in — feeds the IR LED anode and R1 | Pad D, R1 near end |
| 2 | Phototransistor emitter — output, and the photocurrent's return path | Pad A |
| 3 | IR LED cathode — return path, switched on the mainboard | Pad C |

R1 = 1.585 kΩ bridges Pin 1 and the phototransistor collector (Pad B) as the collector load.

- **No ground pin.** The LED current returns on Pin 3, the photocurrent on Pin 2.
- **No current-limiting resistor for the IR LED.** Pin 1 connects straight to the anode; the limit sits on the mainboard side of Pin 3.

### Pad naming

The four pads were labelled by position rather than by datasheet pin number, because the part's orientation on the board was unknown when the measurements were taken.

| Label | Position as viewed | Datasheet pin | Datasheet function | Function derived from the measurements |
|---|---|---|---|---|
| A | top left | 1 | Emitter | Emitter |
| B | bottom left | 2 | Collector | Collector |
| C | bottom right | 3 | Cathode | Cathode |
| D | top right | 4 | Anode | Anode |

All four agree. A 180° rotation would map A to the cathode and B to the anode, so the measurements also fix the orientation: the part sits as the datasheet's top view draws it.

`R1L` and `R1R` denote the left and right terminal of the resistor in the same view.

## What the readings show

Four near-zero readings group the points into nets:

| Net | Points on it | Evidence |
|---|---|---|
| Pin 1 | Pad D, R1's near terminal | `1 → D` and `1 → R1R` = 0.10 Ω |
| Pin 2 | Pad A | `2 → A` = 0.10 Ω |
| Pin 3 | Pad C | `3 → C` = 0.0 Ω |
| Collector | Pad B, R1's far terminal | `1 → B` and `1 → R1L` both = 1.585 kΩ |

That leaves three things to identify.

**The LED runs D → C.** `1 → 3` reads 1.082 V in the diode range one way and OL the other: a single junction, anode towards Pin 1. The same junction appears as `D → C` (1.077 V) and `1 → C` (1.078 V). An IR LED fits, since the datasheet's V<sub>F</sub> is 1.2 V at 20 mA and the meter's test current is far below that. In the resistance range the same path reads 12.5 MΩ, because the ohms source cannot forward-bias an LED.

**The phototransistor runs B (collector) → A (emitter), NPN.** `1 → 2` conducts at 6 k…35 kΩ and tracks illumination; `2 → 1` reads 46 M…OL. A light-controlled device that conducts only with its Pin 1 side positive has its collector on that side.

**R1 sits between Pin 1 and the collector.** `B → D` reads 1.365 V in *both* directions, which no junction does. It is the diode range's reading across 1.585 kΩ, implying a test current of 1.365 V ÷ 1585 Ω ≈ 0.86 mA — which fits the ≈1.08 V measured across the LED.

Two further readings follow from that layout. `B → C` = 2.066 V is R1 and the LED in series; their separate readings sum to 2.442 V, and the diode range falls short of it because its test current sags near the compliance voltage. Everything above a few MΩ — `3 → A/B/D/R1`, `2 → B`, `1 → C` — is reverse leakage at the meter's floor, which the datasheet permits up to 10 µA at V<sub>R</sub> = 6 V. An IR LED also acts as a photodiode, which is why some of those readings move with light.

## Measurements

- `X → Y` means the red lead on X, the black lead on Y.
- Several values separated by `…` are one measurement repeated under varying illumination of the sensor face, ordered **from fully lit to fully shielded**.
- `OL` is over-range / open (written `0L` in the raw notes). Combinations not listed read OL.
- `undefined` means the meter auto-ranged between kΩ and MΩ without settling.

### Diode range, connector pins

| Reading | Value |
|---|---|
| 1 → 3 | 1.082 V |
| all other combinations | OL |

### Resistance, connector pins

| Reading | Value |
|---|---|
| 1 → 2 | 6 k … 35 kΩ |
| 1 → 3 | 12.5 MΩ |
| 2 → 1 | 46 M … 60 M … OL |
| 2 → 3 | 46 M … 60 M … OL |

### Diode range, pad to pad

| Reading | Value |
|---|---|
| B → D | 1.365 V |
| B → C | 2.066 V |
| B → R1R | 1.365 V |
| B → R1L | 0.0 V |
| D → A | 3.3 … 1.5 … OL |
| D → B | 1.365 V |
| D → C | 1.077 V |
| D → R1R | 0.0 V |
| D → R1L | 1.365 V |

### Diode range, pin to pad

| Reading | Value |
|---|---|
| 1 → A | 1.5 … 3.3 … OL |
| 1 → B | 1.365 V |
| 1 → R1L | 1.365 V |
| 1 → C | 1.078 V |
| 1 → D | 0.0 V |
| 2 → A | 0.0 V |
| 3 → C | 0.0 V |

### Resistance, pin to pad

| Reading | Value |
|---|---|
| 1 → A | 2.4 k … 55 k … undefined |
| 1 → B | 1.585 kΩ |
| 1 → R1L | 1.585 kΩ |
| 1 → C | 18 M … 12 MΩ |
| 1 → D | 0.10 Ω |
| 1 → R1R | 0.10 Ω |
| 2 → A | 0.10 Ω |
| 2 → B | 0.7 M … 60 M … OL |
| 2 → R1L and R1R | 0.7 M … 60 M … OL |
| 2 → C | 17 M … 60 M … OL |
| 3 → A | 25 M … OL |
| 3 → B | 25 M … OL |
| 3 → D | 25 M … OL |
| 3 → R1L and R1R | 25 M … OL |
| 3 → C | 0.0 Ω |

### In operation

Taken on the fully assembled machine while it was running normally. The harness wires to the sensor were cut and the probes spliced into the cuts, so the sensor stayed connected to the mainboard and every trace includes the mainboard's loading. Scope ground went to the ground of the 5.5/2.2 mm DC input jack, so the levels below are absolute against the machine's system ground.

| Pin | Observation |
|---|---|
| 1 | 5 V, smooth. 10 mA — a current reading rather than a scope trace, and consistent with the average of the pulsed LED current rather than its peak |
| 2 | Near 0 V up to 0.6 V, roughly rectangular, corners rounded at top left and bottom right (bottom right more so). 1.5 ms per level. A closer object raises the amplitude, never above 0.6 V |
| 3 | Flat plateaus, no curve, between 3.5 V and 4 V, 1.5 ms each. No transition visible between the two levels |
| 2 and 3 | Exactly inverted — Pin 2 HIGH coincides with Pin 3 LOW |

![Sketch: IR-Reflective sensor in operation](2_pin-waveforms.svg)

## Discrepancies in the record

Two internal contradictions, each resolvable. None of them changes the topology.

**The illumination order is reversed in one row.** `D → A` is recorded as 3.3 … 1.5 … OL and `1 → A` as 1.5 … 3.3 … OL. Pin 1 and Pad D are the same net, so these are the same measurement. More light lowers the phototransistor's collector-emitter resistance and therefore the diode range's displayed voltage, so the monotone order is 1.5 V lit → 3.3 V partly shaded → OL dark: the `1 → A` row is the correct one.

**Two illumination sweeps disagree in magnitude.** `1 → A` gives 2.4 k … 55 kΩ and `1 → 2` gives 6 k … 35 kΩ across the same net pair. Different ambient light between the two sessions; only the trend carries information.

## Mainboard

The four resistors were read and measured against the connector pins. The transistor wiring below is a reconstruction from the part markings and the oscilloscope traces.

![Reconstructed mainboard schematic, one sensor channel](2_IR-Reflective-Mainboard-Schematic.svg)

Read from the packages, per channel: two NPN transistors marked **J3Y**, the SOT-23 marking for the S8050 ([datasheet](../../datasheets/S8050.PDF)), and four resistors.

| | Marking | Code | Value | In circuit |
|---|---|---|---|---|
| R7 | `163` | three-digit EIA, 16 × 10³ | 16 kΩ | 14.5 kΩ to Pin 2 |
| R9 | `12B` | EIA-96, index 12 = 130, multiplier B = × 10 | 1.30 kΩ, 1 % | 1.294 kΩ to Pin 1 |
| R22 | `181` | three-digit EIA, 18 × 10¹ | 180 Ω | 181 Ω to Pin 3 |
| R24 | `01B` | EIA-96, index 01 = 100, multiplier B = × 10 | 1.00 kΩ, 1 % | 0.995 kΩ, reaches no connector pin |

The three-digit scheme puts a decade exponent in the last character, so `01B` and `12B` have no reading there. EIA-96 carries the 1 % values three digits cannot express, and both schemes appear on this board. R7 reads below its marking because the rest of the channel sits in parallel with it, about 155 kΩ worth. The designators belong to the channel that was probed, and the other two channels repeat the same parts under different numbers.

The reconstruction has Q1 switching the LED cathode to ground through R22 and driven from the controller through R24. Q2 takes Pin 2 straight onto its base, with R7 from Pin 2 to ground across that junction, R9 as its collector pull-up, and the output inverted to a 0–5 V level for the controller.

### What the waveforms confirm

Three independent numbers follow from the reconstruction and match what was measured.

| Prediction | Arithmetic | Measured |
|---|---|---|
| Pin 2 has a hard ceiling near 0.6 V | Pin 2 sits on Q2's base-emitter junction, which clamps at one V<sub>BE</sub> | ≤ 0.6 V, flat, at every distance |
| Pin 3's LOW plateau is 5 V − V<sub>F</sub> | V<sub>F</sub> = 1.23 V measured, and the LED current at (5 − 1.25 − 0.2) V ÷ 180 Ω = 19.7 mA | 3.5 V, and 3.62 V on a later capture. Both read the plateau against a rail that sags while the emitters are lit |
| Pin 1 draws roughly 10 mA average | LED at 50 % duty: 19.7 mA ÷ 2 = 9.9 mA. Phototransistor branch, saturated upper bound (5 − 0.6 − 0.2) V ÷ 1585 Ω = 2.65 mA, at 50 % duty ≤ 1.3 mA. Total 9.9…11.2 mA | 10 mA |

The flat ceiling on Pin 2 is the strongest of the three. R7 and R1 divide the 5 V rail to 5 V × 16 / (1.585 + 16) = 4.55 V, so photocurrent alone would carry the node into the volts. A closer object raising the amplitude while never pushing past 0.6 V is what a forward-biased junction does, and it also means Q2's base is connected to Pin 2 with no series resistor. Below the ceiling the node is R7 times the photocurrent, which puts the 0.6 V clamp at 0.6 V ÷ 16 kΩ = 37.5 µA. The rounded corners fit the same picture: the falling edge is the more rounded of the two, and Fig. 6 separates t<sub>f</sub> from t<sub>r</sub> at the bottom of its load axis with t<sub>f</sub> the slower, which is where the board's 1.585 kΩ sits.

The LED current of 19.7 mA sits 2.5× inside the 50 mA absolute maximum, and the phototransistor's ≤ 2.65 mA well inside its 20 mA I<sub>C</sub> maximum.

### V<sub>F</sub> and the LED current, measured

Two paths at the connector, both against a rail measured at 5.14 V.

| Path | Wiring | Reading | Result |
|---|---|---|---|
| Current | 49.5 Ω in the Pin 3 line, board still driven by the mainboard | 399 mV, a meter's average over both half-periods | 16.1 mA in the lit half with the shunt in circuit |
| V<sub>F</sub> | Pin 3 off the mainboard, a 220 Ω from Pin 3 to ground, LED lit continuously | 3.91 V across it, and it measures 223 Ω | 17.5 mA, V<sub>F</sub> = 1.23 V |

V<sub>F</sub> from the second path predicts 16.2 mA for the first against the 16.1 mA read, so the reconstruction holds: the first path solves to 181 Ω against the mainboard resistor's 180 Ω marking, and the fourth resistor is not in the LED branch. 1.23 V at 17.5 mA sits on Figure 3's 75 °C curve, which gives 1.25 V there.

Without the shunt the machine drives 19.7 mA at a 5.00 V rail and 20.4 mA at the 5.14 V measured. Pin 3's lit plateau reads 3.62 V on the scope against 3.91 V on the meter, the rail sagging about 0.29 V during the lit half, which is 5 Ω of source resistance at the 57 mA three emitters draw together.

### Ball signal and settling, measured

Sensor at its installed position over the track, supply **3.37 V**, **4.7 kΩ** from Pin 2 to ground, room lit. Both are this design's, not the machine's. Taken twice, once at each emitter resistor the design considered; the 100 Ω row is the one the three stock channels run.

| R<sub>E</sub> | | clear track | ball | signal |
|---|---|---|---|---|
| 220 Ω | LED on | 20.0 mV | 106 mV | **88.1 mV** |
| | LED off | 2.1 mV | 0.0 to 0.1 mV | |
| 100 Ω | LED on | 46.9 mV | 215 mV | **171.9 mV** |
| | LED off | 3.8 mV | 0.0 mV | |

Subtracting each dark reading from its lit one, and the two results from each other, gives the signal column. Through the 4.7 kΩ that is **36.6 µA** at 100 Ω and 18.7 µA at 220 Ω.

**The signal grew by 1.95 where the emitter current grew by 2.14**, so it rises a little more slowly than the current driving it. 2.20 V across the 220 Ω is 10.0 mA at V<sub>F</sub> = 1.17 V, which puts 100 Ω near 21.4 mA. Neither figure can be scaled to a third current, because Sharp publishes I<sub>C</sub> at I<sub>F</sub> = 4 mA only, 60 to 410 µA, and no curve against forward current.

**The difference cancels ambient light.** Brighter room light at 220 Ω took the dark reading from 2.1 to 13 mV, a factor of six, while what the emitter returns held at 17.9 against 20.0 mV.

**A ball lowers the dark reading instead of raising it**, because it covers the slot. The shift at 100 Ω is 3.8 mV against a 171.9 mV signal.

**τ is 53 µs**, taken at the same node with a ball on the track, on the falling edge, where the curve passes 108 mV × e⁻¹ = 39.7 mV. It comes off a screen photograph and carries about ±20 %. Only the falling edge was needed: from about 2 kΩ upward Figure 6 draws t<sub>r</sub> and t<sub>f</sub> as one curve. The 53 µs sit between the 32 µs Figure 6 reads there and the 62 µs its shape applied to the 100 µs maximum gives, and rule out the 214 µs that scaling that maximum linearly with the load would give.

![Falling edge at the 4.7 kΩ node with a ball on the track. Rigol DS1202Z-E, 50 µs/div, 50 mV/div, cursors at 108.0 mV and 0 V. The wire bounced on lifting, so the trace falls, returns to 108 mV and falls again; the second edge is the one read.](2_ir_swing_measurement.jpg)

### Q1's base drive

The controller drives Q1's base through the 1 kΩ. This ensures the pin only has to deliver what the resistor lets through, instead of the much larger current a bare base would draw from it. 1 kΩ satisfies both sides of that: small enough to switch Q1 fully on, large enough to keep the pin's current low.

The datasheet measures V<sub>CE(sat)</sub> at I<sub>C</sub> = 500 mA with I<sub>B</sub> = 50 mA, a collector-to-base ratio of 10. Q1 switches 19.7 mA with 3.8 mA into its base, a ratio of 5. The more base current a transistor gets, the less voltage it drops while it is on. Q1 gets twice as much as the datasheet's test point, so the 0.2 V used for that drop above is safe.

### Timing

1.5 ms on plus 1.5 ms off is a 3.0 ms period — **333 Hz at 50 % duty**. Against t<sub>r</sub>/t<sub>f</sub> of 20 µs typ and 100 µs max, the sensor settles within a fifteenth of each half-period even at the worst-case figure, so 333 Hz is a mainboard choice rather than a sensor limit.

### Why Pin 3's HIGH plateau is ≈4 V and not 5 V

**Estimate, not a datasheet value.** With Q1 off, the only paths from the 5 V rail through the LED are the scope probe's input resistance and Q1's own off-state leakage — a few microamps between them. Datasheet Fig. 3 stops at 1 mA (≈1.0 V); extrapolating down two or three decades puts V<sub>F</sub> around 0.7–0.9 V, which places Pin 3 at 4.1–4.3 V against the ≈4 V observed.

The HIGH plateau is therefore set by leakage rather than by any circuit node, and it moves with whatever loads Pin 3. Loaded still more lightly, Pin 3 floats to essentially the full Pin 1 rail.

## Why the LED is pulsed

Inference, not documented by Robotime.

The detector is sensitive from 700 to 1200 nm and peaks at 930 nm, so daylight and incandescent lighting land squarely in its band, and the datasheet's design guide names external light as the main cause of false detection. The resistance readings show the size of the problem: with the IR LED completely unpowered, `1 → 2` moved from 6 kΩ to 35 kΩ on room light alone — an ambient response of the same order as the signal the sensor is meant to produce. A continuously lit LED gives the controller no way to separate the two. Pulsing does: every 3 ms there is a 1.5 ms window with the LED off, in which whatever the detector reports is ambient by definition.

The hardware only makes that possible. Q2 delivers a bare threshold decision with no analog subtraction, so comparing the two windows has to happen in firmware. Two cases are distinguishable there: a ball present shows up as a difference between the LED-on and LED-off windows, while ambient IR strong enough to push Pin 2 past Q2's threshold on its own saturates the output in *both* windows. The second case is detectable but not recoverable — the reading is lost until the light drops.

The rate follows from the same reasoning. A baseline is only worth subtracting if ambient has not moved between the two windows, and mains-driven lighting flickers at 100 or 120 Hz. At 333 Hz the windows sit 1.5 ms apart, roughly a sixth of a flicker cycle. At a tenth of the rate they would be more than a whole flicker cycle apart and the baseline would mean nothing.

## Sources

- [Sharp GP2S700HCP datasheet](../../datasheets/IR-reflective-gp2s700hcp_e.pdf) — Sheet No. D3-A02201EN. Ratings, characteristics, internal connection diagram, design guide
- Meter and oscilloscope readings on the stock machine
- [BL Galaxy Electrical S8050 datasheet](../../datasheets/S8050.PDF) — document BL/SSSTC079 Rev. A: J3Y in the ordering information, V<sub>BE(sat)</sub>, V<sub>CE(sat)</sub>
- [alldatasheet marking index for J3Y](https://www.alldatasheet.net/view_marking.jsp?Searchword=J3Y) — independent corroboration that J3Y is the SOT-23 marking of the S8050
- [EIA-96 resistor code table](https://www.hobby-hour.com/electronics/eia96-smd-resistors.php) — `01B` resolves as index 01 (100) × 10
