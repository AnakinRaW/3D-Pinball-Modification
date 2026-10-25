# Part list (BOM)

Every part needed to build the modification is listed here, with a link to what is known about it.

*Parts marked * are optional.*

## Base kit

| Item | Qty |
|---|---|
| Robotime ROKR Pinball Machine EG01 | 1 |

## Electronics

### Breakout boards

| Part | Qty | Description | Spec | Supplier |
|---|---|---|---|---|
| Teensy 4.1 | 1 | Microcontroller | [`teensy-4.1`](https://www.pjrc.com/store/teensy41.html) | [PJRC](https://www.pjrc.com/store/teensy41.html) |
| Pololu D24V5F3 | 1 | 3.3 V step-down module, plugged into the IR mainboard, fed from the power board | [`parts/ir-reflective/design.md`](parts/ir-reflective/design.md#supply-and-input-filter) | [Pololu](https://www.pololu.com/product/2842) |
| Adafruit 2090 | 1 | 2.8" TFT with capacitive touch, ILI9341 and CST026, in the diorama | [`parts/display/design.md`](parts/display/design.md) | [Adafruit 2090](https://www.adafruit.com/product/2090) |
| Adafruit 757 | 1 | Bidirectional logic level converter, BSS138, in the display's touch bus | [`parts/display/design.md`](parts/display/design.md#touch) | [Adafruit 757](https://www.adafruit.com/product/757) |
| DFRobot DFR0954 | 1 | I²S class D amplifier module, MAX98357A | [`datasheets/MAX98357A-MAX98357B-Maxim.pdf`](datasheets/MAX98357A-MAX98357B-Maxim.pdf), [module schematic](datasheets/DFR0954-DFRobot-schematic.pdf) | [DFRobot DFR0954](https://www.dfrobot.com/product-2614.html) |
| Seeed 101020692 | 1 | Grove magnetic rotary position sensor, an AS5600 on I²C | [`datasheets/AS5600-ams.pdf`](datasheets/AS5600-ams.pdf), [module schematic](datasheets/Grove-AS5600-Seeed-schematic.pdf) | [Seeed Studio 101020692](https://www.seeedstudio.com/Grove-12-bit-Magnetic-Rotary-Position-Sensor-AS5600-p-4192.html) |

### Passive components

| Part | Qty | Description | Spec | Supplier |
|---|---|---|---|---|
| Rubycon 16ZLH1000MEFC8X20 | 4 | 1000 µF 16 V electrolytic, the bulk at the power board's input (C1, C2) and at each of the two lighting boards (C2 there) | [`parts/power/design.md`](parts/power/design.md#input), [`datasheets/ZLH-Rubycon.pdf`](datasheets/ZLH-Rubycon.pdf) | [DigiKey](https://www.digikey.de/en/products/result?keywords=16ZLH1000MEFC8X20) |
| KEMET ESL107M016AC3AA | 2 | 100 µF 16 V electrolytic, the bulk at the solenoid board (C91) and the servo (C1) | [`parts/solenoid/design.md`](parts/solenoid/design.md#supply), [`parts/servo/design.md`](parts/servo/design.md#supply) | [DigiKey](https://www.digikey.de/en/products/result?keywords=ESL107M016AC3AA) |
| Vishay K104K15X7RF5TL2 | 11 | 100 nF 50 V X7R ceramic, radial: the decoupling at the buffers, the converters, the isolator and the port expander, and the solenoid board's sense filters | [`parts/lighting/design.md`](parts/lighting/design.md#level-shifting), [`parts/solenoid/design.md`](parts/solenoid/design.md#sense), [`parts/ir-reflective/design.md`](parts/ir-reflective/design.md#supply-and-input-filter), [`parts/port-expander/design.md`](parts/port-expander/design.md#supply), [`parts/servo/design.md`](parts/servo/design.md#supply) | [DigiKey](https://www.digikey.de/en/products/result?keywords=K104K15X7RF5TL2) |
| TDK FG11X7R1C226MRT06 | 2 | 22 µF 16 V X7R ceramic, radial, the IR mainboard's C5 and C6 | [`parts/ir-reflective/design.md`](parts/ir-reflective/design.md#supply-and-input-filter) | [DigiKey](https://www.digikey.de/en/products/result?keywords=FG11X7R1C226MRT06) |
| T2A fuse, 5 × 20 mm* | 1 | 2 A time-lag, F1 on the power board | [`parts/power/design.md`](parts/power/design.md#input-protection) | |
| justPi resistor kit JUS-19729 | 1 | 200 carbon-film resistors, 20 each of ten values, 5 %, 0.25 W: every resistor of the build | [Botland product page](https://botland.de/widerstands-kits/19729-satz-justpi-widerstaende-cf-tht-14w-beschrieben-200-stk-5903351243117.html) | [Botland](https://botland.de/widerstands-kits/19729-satz-justpi-widerstaende-cf-tht-14w-beschrieben-200-stk-5903351243117.html) |

The build takes these values from the resistor kit:

| Value | Qty | Where |
|---|---|---|
| 47 Ω | 1 | R39 on the IR mainboard |
| 100 Ω | 3 | R1 to R16 on the IR mainboard, for the three stock sensor boards |
| 220 Ω | up to 14 | R1 to R16 on the IR mainboard for the rebuilt sensor boards, R2 on the Teensy motherboard for the break beam's emitter |
| 330 Ω | 7 | R4 to R6 on each lighting board, R91 on the solenoid sense board |
| 1.2 kΩ | 8 | R1 on the Teensy motherboard, R11 to R14 on the solenoid coil board, R2 on the servo board, R37 and R38 on the IR mainboard |
| 2.2 kΩ | 9 | R1 to R3 on each lighting board, R3 on the servo board, R33 and R34 on the IR mainboard |
| 4.7 kΩ | up to 18 | R17 to R32 on the IR mainboard, R5 and R6 on the Teensy motherboard |
| 10 kΩ | 13 | R1* on the power board with Q1, R21 to R24 on the solenoid coil board, R31 to R33 and R41 to R43 on the sense board, R1 on the servo board, R36 on the IR mainboard |
| 100 kΩ | 3 | R3 and R4 on the Teensy motherboard, R35 on the IR mainboard |

### Semiconductors

| Part | Qty | Description | Spec | Supplier |
|---|---|---|---|---|
| TI CD74HCT125E | 3 | Quad buffer, DIP-14: U1 on each of the two lighting boards and U1 on the servo board | [TI CD74HCT125](https://www.ti.com/lit/ds/symlink/cd74hct125.pdf) | [DigiKey](https://www.digikey.de/en/products/result?keywords=CD74HCT125E) |
| Microchip MCP3008, PDIP-16 | 2 | Ten-bit 8-channel SPI converter, U1 and U2 on the IR mainboard | [`datasheets/MCP3004-3008-Microchip.pdf`](datasheets/MCP3004-3008-Microchip.pdf) | [DigiKey](https://www.digikey.de/en/products/result?keywords=MCP3008-I%2FP) |
| TI ISO7761FDWR, SOIC-16 wide | 1 | Six-channel digital isolator, U3 on the IR mainboard | [`datasheets/ISO7761-TI.pdf`](datasheets/ISO7761-TI.pdf) | [DigiKey](https://www.digikey.de/en/products/result?keywords=ISO7761FDWR) |
| SparkFun COM-28391 | 1 | 8 MB PSRAM, an AP Memory APS6404L-3SQR, soldered to the Teensy's smaller memory pads | [`parts/psram/design.md`](parts/psram/design.md) | [SparkFun COM-28391](https://www.sparkfun.com/products/28391) |
| Microchip MCP23008-E/P, PDIP-18 | 1 | I²C port expander with eight inputs, U1 on the Teensy motherboard, which reads the targets | [`parts/port-expander/design.md`](parts/port-expander/design.md), [`datasheets/MCP23008-MCP23S08-Microchip.pdf`](datasheets/MCP23008-MCP23S08-Microchip.pdf) | [Microchip MCP23008](https://www.microchip.com/en-us/product/MCP23008) |
| Infineon IRL540N, TO-220 | 5 | Logic-level N-channel MOSFET: Q1 to Q4 on the solenoid coil board and Q1 on the IR mainboard | [`datasheets/IRL540N-Infineon.pdf`](datasheets/IRL540N-Infineon.pdf) | [DigiKey](https://www.digikey.de/en/products/result?keywords=IRL540NPBF) |
| ST STP160N3LL, TO-220* | 1 | N-channel MOSFET, Q1 on the power board, the reverse-polarity switch | [`datasheets/STP160N3LL-ST.pdf`](datasheets/STP160N3LL-ST.pdf) | [DigiKey](https://www.digikey.de/en/products/result?keywords=STP160N3LL) |
| Taiwan Semiconductor 1N5819, DO-41 | 6 | Schottky diode: D1 to D4 across the coils, D1 on the Teensy motherboard in front of VIN, D2 across the cut VUSB-VIN trace on the Teensy's underside | [`datasheets/1N5817-1N5819-TSC.pdf`](datasheets/1N5817-1N5819-TSC.pdf) | [DigiKey](https://www.digikey.de/en/products/detail/taiwan-semiconductor-corporation/1N5819-R0G/7357079) |
| onsemi QRE1113 | up to 13 | Reflective sensor, through-hole, for the rebuilt IR sensor boards | [`parts/ir-reflective/design.md`](parts/ir-reflective/design.md#rebuilt-sensor-boards) | [DigiKey](https://www.digikey.de/en/products/detail/onsemi/QRE1113/2175990) |
| TT Electronics OPB606A | up to 13 | Reflective sensor, through-hole, for the rebuilt IR sensor boards; the OPB606C is its less sensitive grade | [`parts/ir-reflective/design.md`](parts/ir-reflective/design.md#rebuilt-sensor-boards) | [DigiKey](https://www.digikey.de/en/products/detail/tt-electronics-optek-technology/OPB606A/498715) |
| Kingbright AM4457F3C | 1 | Side-looking 940 nm infrared LED, the emitter of the ball drain gate | [`datasheets/AM4457F3C-Kingbright.pdf`](datasheets/AM4457F3C-Kingbright.pdf) | [DigiKey 754-2088-ND](https://www.digikey.de/en/products/detail/kingbright/AM4457F3C/7318897) |
| Vishay TEKT5400S | 1 | Side-looking phototransistor with daylight filter, the receiver of the ball drain gate | [`datasheets/TEKT5400S-Vishay.pdf`](datasheets/TEKT5400S-Vishay.pdf) | [DigiKey TEKT5400S-ND](https://www.digikey.de/en/products/detail/vishay-semiconductor-opto-division/TEKT5400S/1681176) |

Both reflective sensors are bought, since neither their performance nor their fit in the machine is known yet. The rebuilt sensor boards carry the one that does better.

### Other

| Part | Qty | Description | Spec | Supplier |
|---|---|---|---|---|
| SEZO WS2812B strip | 1 | 2 m, 160 LEDs/m, 2.7 mm wide: the perimeters, the ceiling and the playfield lights | [`parts/lighting/design.md`](parts/lighting/design.md#leds) | [Amazon](https://amzn.eu/d/0jcgB8mw) |
| DFRobot SER0049 | 1 | 9 g micro servo, 180°, with clutch and stall protection | [`datasheets/SER0049-DFRobot.pdf`](datasheets/SER0049-DFRobot.pdf) | [DFRobot SER0049](https://www.dfrobot.com/product-2120.html) |
| Adafruit 3992 | 1 | Push-pull solenoid, the scoop's coil | [`parts/solenoid/design.md`](parts/solenoid/design.md) | [Adafruit 3992](https://www.adafruit.com/product/3992) |
| Adafruit 1207 | 1 | SOIC-16 to DIP adapter that carries U3, the ISO7761F, in the through-hole build | [`parts/ir-reflective/design.md`](parts/ir-reflective/design.md#part-list) | [Adafruit 1207](https://www.adafruit.com/product/1207) |
| Eyelet terminal | 6 | Three for M2.5 or larger at the bumper shells, one for M3 or larger at the bumper foil, where a conductive screw holds it, two of any size at the bells | [`parts/solenoid/design.md`](parts/solenoid/design.md#connectors), [`parts/targets/design.md`](parts/targets/design.md#bells) | |
| Fuse holder, 5 × 20 mm* | 1 | Holds F1 on the power board | [`parts/power/design.md`](parts/power/design.md#input-protection) | |
| Disc magnet | 1 | 6 mm, diametrically magnetized, in the cap on the seal rod's end | [`parts/magnetic-rotary/design.md`](parts/magnetic-rotary/design.md#part-list) | |
| 5 V adapter | 1 | 3 A or 6 A, short-circuit protected | [`parts/power/design.md`](parts/power/design.md#adapter) | |
| microSD card | 1 | Class A1 or A2, in the Teensy's slot | [`firmware/drivers/storage.md`](firmware/drivers/storage.md), [`parts/display/design.md`](parts/display/design.md#picture) | |

## Mechanical / 3D-printed

| Part | Qty | Material | Spec | Source file |
|---|---|---|---|---|
| Printed light diffuser | one per new playfield light in sight | | [`parts/lighting/design.md`](parts/lighting/design.md#mounting) | |

## Consumables

| Item | Qty | Spec | Supplier |
|---|---|---|---|
| Wire, 16 AWG, from the barrel jack to J-IN | a pair of up to 30 cm | [`parts/power/design.md`](parts/power/design.md#input) | |
| Wire, 22 AWG, from the power board's outputs | one pair per output | [`parts/power/design.md`](parts/power/design.md#wiring) | |
| Dupont cables, 26 AWG, point to point from the boards to the small consumers | as needed | [`parts/power/design.md`](parts/power/design.md#wiring) | |
| Perfboard, 2.54 mm pitch, in several sizes, with 2.54 mm pin headers | as needed | | |
