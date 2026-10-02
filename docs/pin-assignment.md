# Teensy 4.1 pin assignment

## Allocation

| Pin | Signal | Peripheral | Subsystem |
|---|---|---|---|
| 0 | Servo signal | FlexPWM1.1 channel X | [Servo](parts/servo/design.md) |
| 2 | DIN to the DFR0954 | I²S2 TX_DATA | [Audio](parts/audio/design.md) |
| 3 | LRC to the DFR0954 | I²S2 TX_SYNC | [Audio](parts/audio/design.md) |
| 4 | BCLK to the DFR0954 | I²S2 TX_BCLK | [Audio](parts/audio/design.md) |
| 11 | MOSI, data to the converters | SPI MOSI | [IR ball sensing](parts/ir-reflective/design.md) |
| 12 | MISO, data from the converters | SPI MISO | [IR ball sensing](parts/ir-reflective/design.md) |
| 13 | SCK, clock to the converters | SPI SCK | [IR ball sensing](parts/ir-reflective/design.md) |
| 14 | CLOCK, common LED pulse | QuadTimer3 channel 2 | [IR ball sensing](parts/ir-reflective/design.md) |
| 15 | CS-A, converter for channels 1 to 8 | plain digital output | [IR ball sensing](parts/ir-reflective/design.md) |
| 16 | CS-B, converter for channels 9 to 16 | plain digital output | [IR ball sensing](parts/ir-reflective/design.md) |
| 18 | SDA to the rotary sensors | Wire SDA | [Magnetic rotary sensor](parts/magnetic-rotary/design.md) |
| 19 | SCL to the rotary sensors | Wire SCL | [Magnetic rotary sensor](parts/magnetic-rotary/design.md) |
| 31 | Receiver output of the ball drain gate | plain digital input | [Break beam](parts/break-beam/design.md) |
| 32 | Toggle switch, its function set by each game | plain digital input | [Controls](parts/controls/design.md) |
| 33 | MCLK, unused by the amplifier | I²S2 MCLK | [Audio](parts/audio/design.md) |
| 34 | Solenoid sense 3 | plain digital input | [Solenoids](parts/solenoid/design.md) |
| 35 | Solenoid sense 2 | plain digital input | [Solenoids](parts/solenoid/design.md) |
| 36 | Solenoid sense 1 | plain digital input | [Solenoids](parts/solenoid/design.md) |
| 37 | Solenoid trigger 4 | plain digital output | [Solenoids](parts/solenoid/design.md) |
| 38 | Solenoid trigger 3 | plain digital output | [Solenoids](parts/solenoid/design.md) |
| 39 | Solenoid trigger 2 | plain digital output | [Solenoids](parts/solenoid/design.md) |
| 40 | Solenoid trigger 1 | plain digital output | [Solenoids](parts/solenoid/design.md) |

### What this allocation costs

A port or a bus needs all of its pins at once, so taking one of its pins costs the whole port.

| Lost | To |
|---|---|
| I²S2 | Audio, pins 2, 3, 4, 33 |
| The whole SPI bus | Sensors, pins 11, 12, 13, 15 and 16. The mainboard's isolator drives MISO whenever the Teensy is powered, so no second device can share the bus |
| QuadTimer3, all four channels | Sensors, pin 14. Channel 3 starts the read block and has no pin, and channels 0 and 1 stay unused because all four share one interrupt |
| Serial3 and S/PDIF | Sensors, pins 14 and 15 |
| Wire1 and Serial4 | Sensors, pin 16 |
| Wire | Rotary sensor, pins 18 and 19 |
| Serial1 and CAN2 | Servo, pin 0 |
| FlexPWM1.1, at 50 Hz | Servo, pin 0. Pins 42 and 43 share its frequency and carry the SD card instead |
| CAN3 | Break beam, pin 31 |
| Serial8 | Solenoids, pins 34 and 35 |
| 14 of 27 PWM pins | Audio 2, 3, 4, 33; sensors 11, 12, 13, 14, 15; rotary sensor 18, 19; servo 0; solenoids 36, 37 |
| 8 of 18 analog inputs | A0, A1, A2 sensors; A4, A5 rotary sensor; A14, A15, A16 solenoids |

CLOCK takes pin 14 because it sits beside SCK on 13 and the chip selects on 15 and 16, so the whole sensor cable leaves the Teensy from one group of pins. The price is QuadTimer3, which drives pin 14. No other code may use QuadTimer3, `analogWrite()` on pins 14, 15, 18 and 19 included, as [`general-design.md`](../firmware/general-design.md#peripherals-a-driver-owns) sets out for the firmware.

## Reserved

A reserved pin is held for a subsystem whose design is not finished yet. It is not free, and taking it means moving the subsystem that holds it.

| Pin | Signal | Peripheral | Subsystem |
|---|---|---|---|
| 1 | MISO from the display | SPI1 MISO | Display |
| 5 | LED chain 1 | plain digital output | [Lighting](parts/lighting/design.md) |
| 6 | LED chain 2 | plain digital output | [Lighting](parts/lighting/design.md) |
| 7 | LED chain 3 | plain digital output | [Lighting](parts/lighting/design.md) |
| 8 | LED chain 4 | plain digital output | [Lighting](parts/lighting/design.md) |
| 9 | LED chain 5 | plain digital output | [Lighting](parts/lighting/design.md) |
| 10 | LED chain 6 | plain digital output | [Lighting](parts/lighting/design.md) |
| 20 | Target sense 1 | plain digital input | Targets |
| 21 | Target sense 2 | plain digital input | Targets |
| 22 | Target sense 3 | plain digital input | Targets |
| 23 | Target sense 4 | plain digital input | Targets |
| 24 | SCL to the touch controller | Wire2 SCL | Display |
| 25 | SDA to the touch controller | Wire2 SDA | Display |
| 26 | MOSI to the display | SPI1 MOSI | Display |
| 27 | SCK to the display | SPI1 SCK | Display |
| 28 | CS to the display controller | plain digital output | Display |
| 29 | D/C to the display | plain digital output | Display |

### What the reserved pins cost

| Lost | To |
|---|---|
| Serial2 | Lighting, pins 7 and 8 |
| I²S1 | Targets, pins 20, 21 and 23 |
| Serial5 | Targets, pins 20 and 21 |
| CAN1 | Targets, pins 22 and 23 |
| Wire2 and Serial6 | Display, pins 24 and 25 |
| Serial7 | Display, pins 28 and 29 |
| 8 of 18 analog inputs | A6 to A9 targets; A10 to A13 display. Free: A3 on 17, A17 on 41 |

The display's touch controller sits on `Wire2`, pins 24 and 25, because `Wire` belongs to the rotary sensor's driver. Sharing `Wire` would save the two pins and cost DMA or reads split across ticks in that driver.

A second AS5600 needs a TCA9548A multiplexer on pins 18 and 19, because the AS5600's address 0x36 is fixed. The multiplexer carries up to eight sensors and costs no further pin.

Three edge pins remain, 17, 30 and 41, and none of them can carry PWM. Beyond them, the QSPI memory pads on the underside carry pins 48 to 54, which need soldering.

## Signal names

| Name | What it is |
|---|---|
| **RX**, **TX** | Receive and transmit. RX is a pin data arrives on, TX one it leaves on. Two devices are wired crossed, TX to RX and RX to TX |
| **Serial** (RX, TX) | UART, a byte stream between exactly two devices with one wire per direction. It has no addressing, so each connected device needs its own port |
| **SPI** (SCK, MOSI, MISO, CS) | Synchronous bus sharing three wires across several devices. SCK carries the clock, MOSI (master out, slave in) the data leaving the Teensy, MISO (master in, slave out) the data arriving. Each device needs its own CS (chip select) in addition, pulled low to address it. Fast, and costs 3 pins plus one per device |
| **Wire** (SDA, SCL) | I²C, two wires shared by any number of devices, SDA for data and SCL for the clock. Each device answers to its own address. Slower than SPI, and the pin cost stays at two however many devices hang on it |
| **CAN** (RX, TX) | Automotive differential bus. Needs an external transceiver chip |
| **PWM** | A square wave the hardware generates at a set duty cycle. LED brightness, motor and solenoid drive |
| **Analog in** (A0–A17) | The pin reads a voltage between 0 and 3.3 V as a number instead of only high or low |
| **S/PDIF** | Digital audio in and out |
| **LED** | The orange LED soldered to the board, on pin 13, which also carries SPI SCK. It takes roughly 3 of the 4 mA recommended per pin whenever the pin is high |

## Shared resources

`RX 0, TX 1` means that the port's RX signal sits on pin 0 and its TX on pin 1. A slash lists alternatives, so `CS 10 / 36 / 37` means that any one of the three can serve as CS.

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
| SPI2 | MOSI 43 / 50, MISO 42 / 54, SCK 45 / 49, CS 44 (none on the edge headers) |
| Wire | SDA 18, SCL 19 |
| Wire1 | SDA 17, SCL 16 |
| Wire2 | SDA 25, SCL 24 |
| CAN1 | RX 23, TX 22 / 11 |
| CAN2 | RX 0, TX 1 |
| CAN3 | RX 30, TX 31 |
| I²S1 | MCLK 23, BCLK 21, LRCLK 20, data out 7 / 32 / 9 / 6, data in 8 / 38 (the Teensy Audio library's pin set) |
| I²S2 | MCLK 33, BCLK 4, LRCLK 3, data out 2, data in 5 |
| S/PDIF | out 14, in 15 |

## Capabilities on the edge headers

| Capability | Pins | Count |
|---|---|---|
| PWM | 0–15, 18, 19, 22–25, 28, 29, 33, 36, 37 | 27 |
| Analog in | A0–A13 = 14–27 in order, A14–A17 = 38, 39, 40, 41 | 18 |
| Interrupt | all digital pins | — |

Any digital use of an analog-capable pin costs its analog input.

### Timers behind the PWM pins

The bracket names the channel of the timer that drives the pin: A, B or X for a FlexPWM submodule, 0 to 3 for a QuadTimer.

| Timer | Pins, with the channel driving each |
|---|---|
| FlexPWM1.0 | 1 (X), 44 (B), 45 (A) |
| FlexPWM1.1 | 0 (X), 42 (B), 43 (A) |
| FlexPWM1.2 | 24 (X), 46 (B), 47 (A) |
| FlexPWM1.3 | 7 (B), 8 (A), 25 (X) |
| FlexPWM2.0 | 4 (A), 33 (B) |
| FlexPWM2.1 | 5 (A) |
| FlexPWM2.2 | 6 (A), 9 (B) |
| FlexPWM2.3 | 36 (A), 37 (B) |
| FlexPWM3.0 | 54 (A) |
| FlexPWM3.1 | 28 (B), 29 (A) |
| FlexPWM3.3 | 51 (B) |
| FlexPWM4.0 | 22 (A) |
| FlexPWM4.1 | 23 (A) |
| FlexPWM4.2 | 2 (A), 3 (B) |
| QuadTimer1 | 10 (0), 12 (1), 11 (2) |
| QuadTimer2 | 13 (0) |
| QuadTimer3 | 19 (0), 18 (1), 14 (2), 15 (3) |

## Sources

- [Teensy 4.1 pin assignment card, front](https://www.pjrc.com/teensy/card11a_rev4_web.pdf) and [back](https://www.pjrc.com/teensy/card11b_rev4_web.pdf), rev 4: every pin figure above
- [Teensy 4.1 schematic](https://www.pjrc.com/teensy/schematic41.png): the LED and its series resistor on the pin 13 net, and that no buffer stands between them and the pin
- [Teensy 4.1 product page](https://www.pjrc.com/store/teensy41.html): pin counts, microSD via SDIO, Ethernet PHY, USB host
- [PWM and tone on Teensy](https://www.pjrc.com/teensy/td_pulse.html): the PWM pin to timer table, that pins on one timer share a frequency, and that a lower frequency buys resolution
- [`teensy4/pwm.c`](https://github.com/PaulStoffregen/cores/blob/master/teensy4/pwm.c) in PJRC's core: the timer and channel behind each PWM pin of the Teensy 4.1
- [`research/teensy-4.1.md`](research/teensy-4.1.md): electrical limits per pin

Cross-check against PJRC's headline counts: 42 header pins + 6 microSD + 7 bottom pads = 55 total I/O, and 27 PWM on the headers + 8 on the underside = 35 PWM. Both match the product page. The [technical specifications table](https://www.pjrc.com/teensy/techspecs.html) lists 2 SPI ports where the product page says 3. SPI2 falls entirely on pins 42–54, which accounts for the difference.
