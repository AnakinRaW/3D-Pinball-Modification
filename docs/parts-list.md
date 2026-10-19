# Part list (BOM)

Every part needed to build the modification is listed here, with a link to what is known about it.

## Base kit

| Item | Qty |
|---|---|
| Robotime ROKR Pinball Machine EG01 | 1 |

## Electronics

| Part | Qty | Description | Spec | Supplier |
|---|---|---|---|---|
| Teensy 4.1 | 1 | Microcontroller | [`teensy-4.1`](https://www.pjrc.com/store/teensy41.html) | [PJRC](https://www.pjrc.com/store/teensy41.html) |
| D24V5F3 | 1 | 3.3 V step-down module at the IR mainboard's J-PWR, fed from the power board | [`parts/ir-reflective/design.md`](parts/ir-reflective/design.md#supply-and-input-filter) | [Pololu](https://www.pololu.com/product/2842) |
| SunLED XTNI04W | 1 | Side-looking 940 nm infrared LED, the emitter of the ball drain gate | [`datasheets/XTNI04W-SunLED.pdf`](datasheets/XTNI04W-SunLED.pdf) | [DigiKey 1497-XTNI04W-ND](https://www.digikey.de/en/products/result?keywords=XTNI04W) |
| Vishay TEKT5400S | 1 | Side-looking phototransistor with daylight filter, the receiver of the ball drain gate | [`datasheets/TEKT5400S-Vishay.pdf`](datasheets/TEKT5400S-Vishay.pdf) | [DigiKey TEKT5400S-ND](https://www.digikey.de/en/products/detail/vishay-semiconductor-opto-division/TEKT5400S/1681176) |
| Seeed 101020692 | 1 | Grove magnetic rotary position sensor, an AS5600 on I²C | [`datasheets/AS5600-ams.pdf`](datasheets/AS5600-ams.pdf), [module schematic](datasheets/Grove-AS5600-Seeed-schematic.pdf) | [Seeed Studio 101020692](https://www.seeedstudio.com/Grove-12-bit-Magnetic-Rotary-Position-Sensor-AS5600-p-4192.html) |
| DFRobot SER0049 | 1 | 9 g micro servo, 180°, with clutch and stall protection | [`datasheets/SER0049-DFRobot.pdf`](datasheets/SER0049-DFRobot.pdf) | [DFRobot SER0049](https://www.dfrobot.com/product-2120.html) |
| Adafruit 757 | 1 | Bidirectional logic level converter, BSS138, in the display's touch bus | [`parts/display/design.md`](parts/display/design.md#touch) | [Adafruit 757](https://www.adafruit.com/product/757) |
| DFRobot DFR0954 | 1 | I²S class D amplifier module, MAX98357A | [`datasheets/MAX98357A-MAX98357B-Maxim.pdf`](datasheets/MAX98357A-MAX98357B-Maxim.pdf), [module schematic](datasheets/DFR0954-DFRobot-schematic.pdf) | [DFRobot DFR0954](https://www.dfrobot.com/product-2614.html) |

## Mechanical / 3D-printed

| Part | Qty | Material | Spec | Source file |
|---|---|---|---|---|
| _TBD_ | | | | |

## Consumables

| Item | Qty | Spec | Supplier |
|---|---|---|---|
| _TBD_ | | | |
