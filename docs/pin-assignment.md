# Teensy 4.1 pin assignment

## Allocation

| Pin | Signal | Peripheral | Subsystem | Settled in |
|---|---|---|---|---|
| 2 | DIN to the DFR0954 | I²S2 TX_DATA | Audio | — |
| 3 | LRC to the DFR0954 | I²S2 TX_SYNC | Audio | — |
| 4 | BCLK to the DFR0954 | I²S2 TX_BCLK | Audio | — |
| 14 (A0) | S1 | ADC | IR ball sensing | [ir-reflective](parts/ir-reflective/design.md) |
| 15 (A1) | S2 | ADC | IR ball sensing | [ir-reflective](parts/ir-reflective/design.md) |
| 16 (A2) | S3 | ADC | IR ball sensing | [ir-reflective](parts/ir-reflective/design.md) |
| 17 (A3) | S4 | ADC | IR ball sensing | [ir-reflective](parts/ir-reflective/design.md) |
| 18 (A4) | S5 | ADC | IR ball sensing | [ir-reflective](parts/ir-reflective/design.md) |
| 33 | MCLK, unused by the amplifier | I²S2 MCLK | Audio | — |
| 38 (A14) | CLOCK, common LED pulse | plain digital output | IR ball sensing | [ir-reflective](parts/ir-reflective/design.md) |
| 39 (A15) | S6 | ADC | IR ball sensing | [ir-reflective](parts/ir-reflective/design.md) |
| 40 (A16) | S7 | ADC | IR ball sensing | [ir-reflective](parts/ir-reflective/design.md) |
| 41 (A17) | S8 | ADC | IR ball sensing | [ir-reflective](parts/ir-reflective/design.md) |

### What this allocation costs

| Lost | To |
|---|---|
| I²S2 | Audio, pins 2, 3, 4, 33 |
| Serial3, S/PDIF | Sensors, pins 14 and 15 |
| Serial4, Wire1 | Sensors, pins 16 and 17 |
| Wire | Sensors, pin 18 — it needs 18 and 19 together, so taking one kills it. `Wire2` on 24/25 stays free |
| 7 of 27 PWM channels | Audio 2, 3, 4, 33; sensors 14, 15, 18 |
| 9 of 18 analog inputs | Sensors, pins 14–18 and 38–41 |

I²S1 is unused, so Serial2, Serial5 and CAN1 stay free.

## Signal names

| Name | What it is |
|---|---|
| **RX**, **TX** | Receive and transmit. RX is a pin data arrives on, TX one it leaves on. Two devices are wired crossed: TX to RX, RX to TX |
| **Serial** (RX, TX) | UART — a byte stream between exactly two devices, one wire per direction. There is no addressing, so each connected device needs its own port |
| **SPI** (SCK, MOSI, MISO, CS) | Synchronous bus sharing three wires across several devices: SCK the clock, MOSI (master out, slave in) data leaving the Teensy, MISO (master in, slave out) data arriving. Each device needs its own CS (chip select) in addition, pulled low to address it. Fast; costs 3 pins plus one per device |
| **Wire** (SDA, SCL) | I²C — two wires total, SDA for data and SCL for the clock, shared by any number of devices, each answering to its own address. Slower than SPI, and the pin cost stays at two however many devices hang on it |
| **CAN** (RX, TX) | Automotive differential bus. Needs an external transceiver chip |
| **PWM** | A square wave whose duty cycle the hardware varies by itself. LED brightness, motor and solenoid drive |
| **Analog in** (A0–A17) | The pin reads a voltage between 0 and 3.3 V as a number instead of only high or low |
| **S/PDIF** | Digital audio in and out |
| **LED** | The orange LED soldered to the board, on pin 13 |

## Shared resources

Read `RX 0, TX 1` as: this port's RX signal sits on pin 0, its TX on pin 1. A slash lists alternatives — `CS 10 / 36 / 37` means any one of those three can serve as CS.

A bus needs all of its pins at the same time. Using pin 19 as a plain output therefore kills `Wire` even though pin 18 stays free, because `Wire` needs both. The cost of spending a pin is every peripheral that needed it, not the one pin.

| Resource | Pins |
|---|---|
| Serial1 | RX 0, TX 1 |
| Serial2 | RX 7, TX 8 |
| Serial3 | RX 15, TX 14 |
| Serial4 | RX 16, TX 17 |
| Serial5 | RX 21, TX 20 |
| Serial6 | RX 25, TX 24 |
| Serial7 | RX 28, TX 29 |
| Serial8 | RX 34, TX 35 |
| SPI | MOSI 11, MISO 12, SCK 13, CS 10 / 36 / 37 |
| SPI1 | MOSI 26, MISO 1 / 39, SCK 27, CS 0 / 38 |
| SPI2 | MOSI 43 / 50, MISO 42 / 54, SCK 45 / 49, CS 44 — none on the edge headers |
| Wire | SDA 18, SCL 19 |
| Wire1 | SDA 17, SCL 16 |
| Wire2 | SDA 25, SCL 24 |
| CAN1 | RX 23, TX 22 / 11 |
| CAN2 | RX 0, TX 1 |
| CAN3 | RX 30, TX 31 |
| I²S1 | MCLK 23, BCLK 21, LRCLK 20, data out 7 / 32 / 9 / 6, data in 8 / 38 — the Teensy Audio library's pin set, plus `Wire` (18, 19) for the codec |
| I²S2 | MCLK 33, BCLK 4, LRCLK 3, data out 2, data in 5 |

## Pins claimed by two peripherals

| Pins | Claimed by |
|---|---|
| 0, 1 | Serial1 and CAN2 |
| 11 | SPI MOSI and CAN1 TX |
| 13 | SPI SCK and the onboard LED |
| 14, 15 | Serial3 and S/PDIF out/in |
| 16, 17 | Serial4 and Wire1 |
| 24, 25 | Serial6 and Wire2 |

## Capabilities on the edge headers

| Capability | Pins | Count |
|---|---|---|
| PWM | 0–15, 18, 19, 22–25, 28, 29, 33, 36, 37 | 27 |
| Analog in | A0–A13 = 14–27 in order, A14–A17 = 38, 39, 40, 41 | 18 |
| Interrupt | all digital pins | — |

Pins 14–27 and 38–41 are the analog-capable ones, so any digital use of those costs an analog input.

## Sources

- [Teensy 4.1 pin assignment card, front](https://www.pjrc.com/teensy/card11a_rev4_web.pdf) and [back](https://www.pjrc.com/teensy/card11b_rev4_web.pdf) — rev 4, dated 2021-09-20. Every pin figure above
- [Teensy 4.1 product page](https://www.pjrc.com/store/teensy41.html) — pin counts, microSD via SDIO, Ethernet PHY, USB host
- [`research/teensy-4.1.md`](research/teensy-4.1.md) — electrical limits per pin

Cross-check against PJRC's headline counts: 42 header pins + 6 microSD + 7 bottom pads = 55 total I/O, and 27 PWM on the headers + 8 on the underside = 35 PWM. Both match the product page. The [technical specifications table](https://www.pjrc.com/teensy/techspecs.html) lists 2 SPI ports where the product page says 3; SPI2 falls entirely on pins 42–54, which accounts for the difference.
