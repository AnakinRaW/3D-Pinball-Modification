# Teensy 4.1

Notes on the controller selected for the modification (`A1` in the [part list](../parts-list.md)).

Figures are quoted from PJRC's [product page](https://www.pjrc.com/store/teensy41.html) and [technical specifications table](https://www.pjrc.com/teensy/techspecs.html).

## Specifications

| Property | Value |
|---|---|
| Processor | NXP i.MX RT1062 — ARM Cortex-M7 @ 600 MHz, FPU for 32- and 64-bit |
| Flash | 7936 KB |
| RAM | 1024 KB, of which 512 KB tightly coupled |
| EEPROM | 4 KB (emulated) |
| Digital I/O | 55 pins, 42 of them breadboard-accessible |
| Analog inputs | 18 |
| PWM outputs | 35 |
| Serial / SPI / I²C | 8 / 3 / 3 |
| Logic level | 3.3 V |
| Onboard peripherals | microSD socket, USB host header, 10/100 Ethernet (needs an external magjack) |

## Constraints this part imposes on the design

### Not 5 V tolerant

PJRC states it directly on the product page: the pins are not 5 V tolerant, and no digital pin may be driven above 3.3 V. The warning is repeated separately for the analog pins.

This is the most likely way to destroy the board. Every signal arriving from outside — a sensor, a switch pulled up to 5 V, the output of another board — must be confirmed to sit at or below 3.3 V, or pass through a level shifter first.

Overvoltage on a CMOS input forces current through the chip's internal protection diodes. That degrades the input over time instead of killing it outright, so a circuit that appears to work is not evidence that it is within limits. Intermittent faults weeks later are the usual symptom.

### Power

| Property | Value | Source |
|---|---|---|
| VIN input range | 3.6 – 5.5 V | PJRC pin assignment card 11a rev4 |
| **Recommended maximum output current per pin** | **4 mA** | [Teensy 4.1 product page](https://www.pjrc.com/store/teensy41.html) |
| Output pin figure in the comparison table | 10 mA at 3.3 V | [PJRC technical specifications table](https://www.pjrc.com/teensy/techspecs.html) |
| 3.3 V rail available to external circuits | 250 mA | PJRC pin assignment card 11a rev4 |
| Current draw @ 600 MHz | ≈ 100 mA | PJRC — **published for the Teensy 4.0, not the 4.1** |

Three open points:

- The ≈ 100 mA figure belongs to the Teensy 4.0. PJRC gives no equivalent number for the 4.1 in the same place, and the 4.1 carries additional hardware. Measure the real draw before sizing a supply.
- **Two PJRC figures for the same property disagree.** The Teensy 4.1 product page states 4 mA as the recommended maximum output current; the technical specifications table, which spans every Teensy generation in one grid, lists 10 mA. Design against 4 mA: it is the board-specific figure and the conservative one. 10 mA is the value the older generations carry.
- 4 mA is a recommended maximum, not an operating point. Drive LEDs and any real load through a transistor or a driver IC.

### Powering from something other than USB

The VUSB–VIN trace on the underside must be cut when an external supply is used — PJRC's pinout card labels it "cut to separate VIN from VUSB, if using external power." Left intact, the external supply is tied to the USB host's 5 V rail.

## Sources

- [Teensy 4.1 product page](https://www.pjrc.com/store/teensy41.html) — specifications and the 5 V tolerance warnings
- [Teensy technical specifications comparison table](https://www.pjrc.com/teensy/techspecs.html) — per-pin output rating
