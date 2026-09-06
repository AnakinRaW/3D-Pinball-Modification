# Teensy 4.1

Notes on the controller selected for the modification (`A1` in the [part list](../parts-list.md)).

Figures are quoted from PJRC's [product page](https://www.pjrc.com/store/teensy41.html) and [technical specifications table](https://www.pjrc.com/teensy/techspecs.html).

## Specifications

| Property | Value |
|---|---|
| Processor | NXP i.MX RT1062, ARM Cortex-M7 @ 600 MHz, FPU for 32- and 64-bit |
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

This is the most likely way to destroy the board. Every signal arriving from outside (a sensor, a switch pulled up to 5 V, the output of another board) must be confirmed to sit at or below 3.3 V, or pass through a level shifter first.

Overvoltage on a CMOS input forces current through the chip's internal protection diodes. That degrades the input over time instead of killing it outright, so a circuit that appears to work is not evidence that it is within limits. Intermittent faults weeks later are the usual symptom.

### Driving a pin while the board is unpowered

PJRC's answer is no. Paul Stoffregen, asked whether 3.3 V may be applied to a Teensy 4 I/O pin while the Teensy is off: "No, you should not do this." The chip is not to be driven while its power is off, "either that GPIO section or the whole chip", and unlike parts designed to be high impedance when unpowered, "the Teensy 4.0 pins are not." Current entering an unpowered pin passes through its ESD diode into the 3.3 V rail, and powering up with voltage already present on a pin risks CMOS latch-up. Two Teensy 4.1 boards left connected over RX/TX to a powered router while unpowered came back with 3V3 shorted to GND; PJRC could not attribute that failure from the report alone.

NXP says the same in [IMXRT1060CEC](../datasheets/IMXRT1060CEC.pdf) Rev. 4. Section 4.2.1.3 on page 31 forbids driving an I/O pin externally while that pin's supply NVCC_xxx is off, and names internal latch-up and malfunction from reverse current flows as the mechanism. Section 4.2.1.1 lists irreversible damage to the processor as the worst case of a sequencing violation.

**The only quantified limit at a pin is a voltage.** Table 7 on page 24 gives `Vin/Vout` from −0.5 V to `OVDD + 0.31 V`, with OVDD the I/O supply voltage, so the ceiling falls to 0.31 V once that supply sits at zero. An injection-current rating, the per-pin milliamp allowance many microcontrollers publish, appears nowhere in the document.

Remedies, in the order PJRC ranks them:

| Remedy | Standing |
|---|---|
| A buffer specified as high impedance while unpowered, such as the 74LCX125, between the external signal and the pin | The proper solution for digital lines. A logic buffer cannot carry an analog signal, so an analog input needs the source removed or clamped instead |
| A series resistor, 1 kΩ as the starting point | "At least limits the current"; under 1 mA is "very unlikely to cause harm", which PJRC states does not follow NXP's guidance |

A series resistor bounds the current. Fixing the potential takes a resistor from the net to a rail, because a pin that draws nothing puts no drop across a series element and follows whatever its net does.

Sources: [Can 3.3 V be safely applied to a Teensy 4 I/O pin while the Teensy 4 power is off?](https://forum.pjrc.com/threads/60528-Can-3-3-V-be-safely-applied-to-a-Teensy-4-I-O-pin-while-the-Teensy-4-power-is-off), posts by Paul Stoffregen; [2x Teensy 4.1 3v3 is shorted to GND](https://forum.pjrc.com/index.php?threads/2x-teensy-4-1-3v3-is-shorted-to-gnd.71511/), post by Paul Stoffregen.

### Power

| Property | Value | Source |
|---|---|---|
| VIN input range | 3.6 to 5.5 V | PJRC pin assignment card 11a rev4 |
| **Recommended maximum output current per pin** | **4 mA** | [Teensy 4.1 product page](https://www.pjrc.com/store/teensy41.html) |
| Output pin figure in the comparison table | 10 mA at 3.3 V | [PJRC technical specifications table](https://www.pjrc.com/teensy/techspecs.html) |
| 3.3 V rail available to external circuits | 250 mA | PJRC pin assignment card 11a rev4 |
| Current draw @ 600 MHz | ≈ 100 mA | PJRC. **Published for the Teensy 4.0, not the 4.1** |

Three open points:

- The ≈ 100 mA figure belongs to the Teensy 4.0. PJRC gives no equivalent number for the 4.1 in the same place, and the 4.1 carries additional hardware. Measure the real draw before sizing a supply.
- **Two PJRC figures for the same property disagree.** The Teensy 4.1 product page states 4 mA as the recommended maximum output current; the technical specifications table, which spans every Teensy generation in one grid, lists 10 mA. Design against 4 mA: it is the board-specific figure and the conservative one. 10 mA is the value the older generations carry.
- 4 mA is a recommended maximum, not an operating point. Drive LEDs and any real load through a transistor or a driver IC.

### ADC source impedance

**The maximum source resistance R<sub>AS</sub> is 1 kΩ**, at 12 bit, f<sub>ADCK</sub> = 40 MHz and the 150 ns sample window, from [IMXRT1060CEC](../datasheets/IMXRT1060CEC.pdf) Rev. 4, Table 54, page 65. That datasheet covers the MIMXRT1062DVJ6 in both silicon revisions.

The 1 kΩ belongs to that one sample setting. A longer window admits a higher resistance; Figures 36 to 38 on pages 68 and 69 plot the minimum sample time against source resistance and run to 10 kΩ. Setting the window is [`firmware/constraints.md`](../../firmware/constraints.md).

**PJRC publishes no figure.** [pjrc.com/teensy/adc.html](https://www.pjrc.com/teensy/adc.html) carries a "Source Impedance Problems" heading whose body reads "TODO: write this section". A 10 kΩ figure circulates in secondary sources without a citation; the charts ending there are the likely origin.

### Powering from something other than USB

The VUSB-VIN trace on the underside must be cut when an external supply is used. PJRC's pinout card labels it "cut to separate VIN from VUSB, if using external power." Left intact, the external supply is tied to the USB host's 5 V rail.

### I²S1 spends three analog-capable pins

`config_i2s()` muxes pins 23, 20 and 21 for I²S1:

```c
if (!only_bclk)
{
  CORE_PIN23_CONFIG = 3;  //1:MCLK
  CORE_PIN20_CONFIG = 3;  //1:RX_SYNC  (LRCLK)
}
CORE_PIN21_CONFIG = 3;  //1:RX_BCLK
```

The `only_bclk` path drops LRCLK together with MCLK, so it is no route to freeing pin 23 for an amplifier that needs LRCLK. Pin 23 is muxed to MCLK whether or not the amplifier uses it, and a MAX98357A does not.

Pins 20, 21 and 23 are A6, A7 and A9, so I²S1 costs three of the 18 analog inputs. I²S2 (MCLK 33, BCLK 4, LRCLK 3, data out 2) costs none. `AudioOutputI2S2` is compiled under `#if defined(__IMXRT1062__)`, the Teensy 4.x part, and its `config_i2s2()` muxes `CORE_PIN33` (SAI2_MCLK), `CORE_PIN4` (TX_BCLK), `CORE_PIN3` (TX_SYNC) and `CORE_PIN2` (TX_DATA). `AudioOutputI2S2slave` omits MCLK but needs an external clock master, which the MAX98357A is not.

Pin 23 carries the only CAN1 RX, so I²S1 and CAN1 cannot coexist.

Source: [PaulStoffregen/Audio, `output_i2s.cpp`](https://github.com/PaulStoffregen/Audio/blob/master/output_i2s.cpp), the library's own source.

## Sources

- [Teensy 4.1 product page](https://www.pjrc.com/store/teensy41.html): specifications and the 5 V tolerance warnings
- [Teensy technical specifications comparison table](https://www.pjrc.com/teensy/techspecs.html): per-pin output rating
