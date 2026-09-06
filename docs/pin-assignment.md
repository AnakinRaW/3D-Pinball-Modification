# Teensy 4.1 pin assignment

## Allocation

| Pin | Signal | Peripheral | Subsystem | Settled in |
|---|---|---|---|---|
| 2 | DIN to the DFR0954 | I²S2 TX_DATA | Audio | — |
| 3 | LRC to the DFR0954 | I²S2 TX_SYNC | Audio | — |
| 4 | BCLK to the DFR0954 | I²S2 TX_BCLK | Audio | — |
| 11 | MOSI, shared SPI bus | SPI MOSI | IR ball sensing | [ir-reflective](parts/ir-reflective/design.md) |
| 12 | MISO, shared SPI bus | SPI MISO | IR ball sensing | [ir-reflective](parts/ir-reflective/design.md) |
| 13 | SCK, shared SPI bus | SPI SCK | IR ball sensing | [ir-reflective](parts/ir-reflective/design.md) |
| 29 | CLOCK, common LED pulse | plain digital output | IR ball sensing | [ir-reflective](parts/ir-reflective/design.md) |
| 33 | MCLK, unused by the amplifier | I²S2 MCLK | Audio | — |
| 36 | CS-A, converter for channels 1 to 8 | SPI CS | IR ball sensing | [ir-reflective](parts/ir-reflective/design.md) |
| 37 | CS-B, converter for channels 9 to 16 | SPI CS | IR ball sensing | [ir-reflective](parts/ir-reflective/design.md) |

### What this allocation costs

| Lost | To |
|---|---|
| I²S2 | Audio, pins 2, 3, 4, 33 |
| CAN1 TX on pin 11 | Sensors. CAN1 TX has 22 as its alternative, so CAN1 survives |
| Serial7 | Sensors, pin 29. It needs 28 and 29 together, so taking one kills it, and 28 stays free as a plain pin |
| The whole SPI bus | Sensors, pins 11, 12, 13, 36 and 37. The mainboard's buffer drives MISO whenever that board is powered, so no second device can share the bus. Pin 10, the third chip select of the set, stays free and can serve a device on another bus |
| SPI1, by consequence | Sensors. With SPI reserved, the next SPI device lands on SPI1, which the display reservation below now holds |
| 10 of 27 PWM channels | Audio 2, 3, 4, 33; sensors 11, 12, 13, 29, 36, 37. Every pin in the allocation is PWM-capable |

## Reserved

Held for a subsystem that is designed but not yet built. A reserved pin is not free: taking it means moving the subsystem that holds it.

| Pin | Signal | Peripheral | Subsystem | Held for |
|---|---|---|---|---|
| 0 | Bumper trigger 4 | plain digital output | Bumpers | — |
| 1 | Bumper sense 1 | plain digital input | Bumpers | — |
| 5 | LED chain 1 | plain digital output | Lighting | [lighting](parts/lighting/design.md) |
| 6 | LED chain 2 | plain digital output | Lighting | [lighting](parts/lighting/design.md) |
| 7 | LED chain 3 | plain digital output | Lighting | [lighting](parts/lighting/design.md) |
| 8 | LED chain 4 | plain digital output | Lighting | [lighting](parts/lighting/design.md) |
| 9 | LED chain 5 | plain digital output | Lighting | [lighting](parts/lighting/design.md) |
| 10 | LED chain 6 | plain digital output | Lighting | [lighting](parts/lighting/design.md) |
| 14 | Bumper sense 2 | plain digital input | Bumpers | — |
| 15 | Bumper sense 3 | plain digital input | Bumpers | — |
| 18 | SDA to the rotary sensors | Wire SDA | Hall | — |
| 19 | SCL to the rotary sensors | Wire SCL | Hall | — |
| 20 | Target sense 1 | plain digital input | Targets | — |
| 21 | Target sense 2 | plain digital input | Targets | — |
| 22 | Foil drive, 3.3 V | plain digital output | Bumpers | — |
| 23 | Target sense 3 | plain digital input | Targets | — |
| 24 | Servo signal | PWM | Servo | — |
| 26 | MOSI to the display | SPI1 MOSI | Display | — |
| 27 | SCK to the display | SPI1 SCK | Display | — |
| 28 | CS to the display controller | plain digital output | Display | — |
| 30 | CS to the touch controller | plain digital output | Display | — |
| 31 | D/C to the display | plain digital output | Display | — |
| 32 | Bumper trigger 1 | plain digital output | Bumpers | — |
| 34 | Bumper trigger 2 | plain digital output | Bumpers | — |
| 35 | Bumper trigger 3 | plain digital output | Bumpers | — |
| 38 | Target sense 4 | plain digital input | Targets | — |
| 39 | MISO from the display | SPI1 MISO | Display | — |

### What the reservations cost

| Lost | To |
|---|---|
| Serial2 | Lighting, pins 7 and 8 |
| The third SPI chip select | Lighting, pin 10 |
| CAN3 | Display, pins 30 and 31 |
| Serial1 and CAN2 | Bumpers, pins 0 and 1 |
| Serial8 | Bumpers, pins 34 and 35 |
| Serial3 and S/PDIF | Bumpers, pins 14 and 15 |
| CAN1 | Targets, pin 23, the only CAN1 RX; and the foil, pin 22, which is CAN1 TX's last alternative once pin 11 carries MOSI |
| Wire2 and Serial6 | Servo, pin 24. Both need the pair 24 and 25 |
| I²S1 | Targets and the foil, pins 20, 21 and 23. Audio runs on I²S2, so nothing wanted it |
| 13 of 18 analog inputs | A0, A1 bumpers; A4, A5 hall; A6, A7, A9 and A14 targets; A8 foil; A10 servo; A12, A13, A15 display. Free: A2, A3 on pins 16 and 17, A11 on 25, A16, A17 on 40 and 41 |

**One AS5600, on `Wire`.** Its address is fixed at 0x36 with no address pins, so a second one on the same two wires collides with the first. A second sensor takes `Wire1` on pins 16 and 17, or a TCA9548A multiplexer on pins 18 and 19, which carries up to eight and costs no further pin.

**Where the next pins come from.** Five edge pins remain: 16, 17, 25, 40 and 41, all analog-capable, and pin 25 is the last one that can carry PWM. Beyond them lie the bottom-pad pins 42 to 54, which need soldering to the underside. A port expander on the `Wire` bus adds sixteen inputs for no further pin.

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
| 13 | SPI SCK and the onboard LED. The LED takes roughly 3 of the 4 mA recommended per pin whenever the pin is high |
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

- [Teensy 4.1 pin assignment card, front](https://www.pjrc.com/teensy/card11a_rev4_web.pdf) and [back](https://www.pjrc.com/teensy/card11b_rev4_web.pdf) — rev 4. Every pin figure above
- [Teensy 4.1 schematic](https://www.pjrc.com/teensy/schematic41.png) — the LED and its series resistor on the pin 13 net, and that no buffer stands between them and the pin
- [Teensy 4.1 product page](https://www.pjrc.com/store/teensy41.html) — pin counts, microSD via SDIO, Ethernet PHY, USB host
- [`research/teensy-4.1.md`](research/teensy-4.1.md) — electrical limits per pin

Cross-check against PJRC's headline counts: 42 header pins + 6 microSD + 7 bottom pads = 55 total I/O, and 27 PWM on the headers + 8 on the underside = 35 PWM. Both match the product page. The [technical specifications table](https://www.pjrc.com/teensy/techspecs.html) lists 2 SPI ports where the product page says 3; SPI2 falls entirely on pins 42–54, which accounts for the difference.
