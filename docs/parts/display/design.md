# Display

## Use cases

The display can be used to show various things such as:

- The player changes the game and machine settings on the display by touch.
- The host shows device faults and debug values on the display, also over a running game.
- A game shows videos from the SD card in a part of the screen or over the whole screen, with or without its own drawings in front.
- A full-screen movie plays at 25 frames a second while the sounds keep playing.

## Hardware

This modification uses an Adafruit 2090 2.8" capacitive touch display. The panel uses an ILI9341 controller. It takes commands and pixels over an 8-bit bus. Eight data lines carry one byte at a time. The display takes the byte when the write line WR goes high. D/C tells whether the byte is a command or the data that follows one. The display listens only while CS is low. A pixel takes two bytes of RGB565, high byte first.

The touch controller is a CST026 at the I²C address 0x38.

## Installation & Mounting

The module measures 81.3 mm by 62.5 mm. A printed frame holds the module by its four holes of 3 mm. The holes sit 76.2 mm by 57.2 mm apart. The module gets screwed into the diorama's background plate. The screen faces the player in landscape. 

The part B20 from the stock machine either needs to be cut, removed or replicated with a different motif to not cover parts of the screen.

## Circuit

![Display schematic](display-schematic.svg)

### Bus

The data lines D0 to D7 run from pins [22](../../pin-assignment.md "D0 to the display"), [23](../../pin-assignment.md "D1 to the display"), [20](../../pin-assignment.md "D2 to the display"), [21](../../pin-assignment.md "D3 to the display"), [38](../../pin-assignment.md "D4 to the display"), [39](../../pin-assignment.md "D5 to the display"), [26](../../pin-assignment.md "D6 to the display") and [27](../../pin-assignment.md "D7 to the display"). 

FlexIO3 is a programmable I/O block of the i.MX RT. FlexIO3's timer drives WR on pin [17](../../pin-assignment.md "WR to the display") and puts one byte on the data lines with each strobe. CS on pin [28](../../pin-assignment.md "CS to the display controller") and D/C on pin [29](../../pin-assignment.md "D/C to the display") are plain digital outputs.

The driver sets the bus pins to their weakest drive and their slow slew rate. WR then rises too gently to ring, so the display takes each byte exactly once. That edge is still fast enough for FlexIO3's clock.

The display only receives, so RD is wired to Vin. CCS is tied to Vin too, which keeps the module's card socket off. MOSI is tied to GND. RST, Lite and the touch controller's IRQ stay unconnected.

### Touch

The touch controller sits on `Wire2`, with SDA on pin [25](../../pin-assignment.md "SDA to the touch controller") and SCL on pin [24](../../pin-assignment.md "SCL to the touch controller"). The bus runs at the Standard-mode rate of 100 kHz.

### Reset

After power-up, the module holds the panel and the touch controller in reset for up to 280 ms. Until then, both ignore every command.

### Supply

The module takes its supply Vin from the Teensy's 3V3 pin. The module draws at most 191 mA of the 250 mA the 3V3 pin supplies.

## Teensy pins

| Signal | What the pin has to be |
|---|---|
| D0 to D7 | Eight pins that FlexIO3 numbers in a row, since FlexIO3 puts out a byte on eight such pins at once. FlexIO3's pins 8 to 15 are such a run. No other subsystem needs them |
| WR | A FlexIO3 pin outside that run, which FlexIO3's timer drives. Here FlexIO3's pin 6 |
| CS, D/C | Plain digital outputs |
| SDA, SCL | An I²C port other than `Wire`, which belongs to the rotary sensor's driver. Sharing `Wire` would save the two pins and cost that driver DMA or reads split across ticks |

[`pin-assignment.md`](../../pin-assignment.md) lists the pins and what they cost elsewhere in the build.

## Firmware

The firmware builds the picture from three layers that lie on top of each other. From the front, they are the host's layer, the game's layer and the background. A video plays in a viewport on the background.

The [display driver](../../firmware/drivers/display.md) combines them into the picture it sends over FlexIO3. The [touch driver](../../firmware/drivers/touch.md) reads the touch panel.

The [screen component](../../firmware/components/screen.md) manages the game's layer, the background and the host's layer. The [video player](../../firmware/components/video-player.md) plays the videos.

## Limitations

Fast motion in a video can show a tear, since the module gives no signal when the panel refreshes.

The video rate rests on an estimate of the card's speed. A slower card makes a video stutter before it touches the sound.

## Part list

| Qty | Part | Where |
|---|---|---|
| 1 | Adafruit 2090, 2.8" TFT with capacitive touch, ILI9341 and CST026 | in the diorama |

## Appendix: derivations

Every figure below is recomputed by [`figures.py`](figures.py) from the inputs it names. `tools/figcheck.py` compares each one against the line that states it.

### Bus

```
BUS       the FlexIO3 clock                        120 MHz
          its clocks for one write                 10 clocks
          one write on the bus                   = 83.3 ns
          the ILI9341's write cycle, at least      66 ns
          WR low, then WR high, each             = 41.7 ns
          the ILI9341's WR low and WR high, each
          at least                                 15 ns
          a buffer's delay on the module, from     1.5 ns
          up to                                    6.3 ns
          the skew between WR and the data       = 4.8 ns
          setup and hold at the panel, each at
          least                                  = 36.8 ns
          the ILI9341's setup and hold, each at
          least                                    10 ns
EDGE      a bus pin at its weakest drive, typical  157 Ω
          the cable, at most                       50 cm
          one wire of the cable                    50 pF
          a buffer input on the module             5.5 pF
          an edge between 10 % and 90 %          = 19.2 ns
          that edge per volt of the swing        = 7.26 ns/V
          what the buffers allow, at most          10 ns/V
LEVELS    the Teensy's rail                        3.3 V
          its high, at least                     = 3.15 V
          the buffers' high input level            2 V
          the margin of the high                 = 1.15 V
          the Teensy's low, at most                0.15 V
          the buffers' low input level             0.8 V
          the margin of the low                  = 0.65 V
```

### Picture

```
FRAME     the panel in landscape, pixels across  = 320
          and rows down                          = 240
          one pixel on the bus                     2 bytes
          one frame                              = 150 KB
          that frame on the bus                  = 12.8 ms
          the full screen anew, at most          = 78.1 Hz
          one burst, eight shifters of four
          bytes                                  = 32 bytes
          that burst on the bus                  = 2.67 µs
          the processor's clock                    600 MHz
          its clocks for one refill, estimated     350 clocks
          its share while the bus sends          = 21.9 %
          blending one partly opaque pixel,
          estimated                                20 clocks
          a refill that blends the front and the
          middle layer at every pixel            = 990 clocks
          the clocks one burst gives a refill    = 1600 clocks
          the share while the bus sends then     = 61.9 %
          one layer, one index a pixel           = 75 KB
          the three layers and the movie's ring in
          RAM2                                   = 289 KB
MOVIE     the frame rate of every movie            25 Hz
          one frame                              = 40 ms
          the display's tick                       40 ms
          the chunk the card is read in            16 KB
          a full-screen frame, in chunks         = 9.4
          the movie's ring in RAM2                 64 KB
CARD      one 4 KB read, at an estimated 1500 a
          second                                 = 0.667 ms
          the SD bus's High Speed clock            50 MHz
          4 KB on that bus                       = 0.164 ms
          one audio chunk on the Teensy,
          estimated                              = 0.831 ms
          16 KB on that bus                      = 0.655 ms
          one movie chunk on the Teensy,
          estimated                              = 1.33 ms
          a full-screen frame from the card      = 12.4 ms
          the four audio streams meanwhile       = 2.87 ms
          the card's load at 25 frames a second  = 38.2 %
          the same on the A1 class alone, every
          4 KB a random read                     = 85.1 %
```

### Touch bus

```
TOUCH     the bus clock                            100 kHz
          one byte on the bus                      9 bits
          one read, the address, the register,
          the address again and five bytes       = 750 µs
          the touch driver's tick                  20 ms
          its share of the bus at one read a tick= 3.75 %
          the module's pull-up on SDA and SCL      10 kΩ
          each bus line, estimated                 100 pF
          a rising edge between 0.3 and 0.7 of
          the rail                               = 848 ns
          what Standard mode allows                1000 ns
```

### Supply

```
SUPPLY    the Teensy's rail, at most             = 3.333 V
          each LED's resistor                      10 Ω
          an LED's forward voltage, at least       2.9 V
          one LED, at most                       = 43.3 mA
          the four LEDs                          = 173.2 mA
          the ILI9341, assumed                     10 mA
          the touch controller, at most            5 mA
          the module from the 3V3 pin, at most   = 191 mA
          what the 3V3 pin supplies                250 mA
VCC       the Teensy's rail, at least            = 3.267 V
          the regulator's dropout at that
          current, at most                       = 127 mV
          VCC, at least                          = 3.14 V
          the ILI9341's lowest supply              2.5 V
          the supervisor's threshold, below        3 V
          the ILI9341's highest supply             3.3 V
          VCC from the module's regulator on 5 V,
          at most                                = 3.35 V
START     the supervisor's reset after power-up,
          at most                                  280 ms
          the ILI9341, from its reset to its
          first command                            5 ms
          the driver's first command, after
          power-up                               = 285 ms
          the ILI9341, from Sleep Out to running   120 ms
          the touch controller, from its reset to
          its first answer                         300 ms
          the driver's first touch read, after
          power-up                               = 580 ms
```

## Sources

- [`datasheets/ILI9341-Ilitek.pdf`](../../datasheets/ILI9341-Ilitek.pdf), ILI9341_DS_V1.11: the 8080-I write timing in 18.3.1, the supply range and the input levels in 18.2.1, the reset timing in 15.4, Sleep Out, the RGB565 transfer and the frame memory
- [`datasheets/CH280QV10-CT-ChengHao.pdf`](../../datasheets/CH280QV10-CT-ChengHao.pdf), Rev. D: the panel Adafruit names for the 2090, its active area, its four backlight LEDs and their forward voltage, and the CST026 as its touch controller
- [Adafruit, 2090 product page](https://www.adafruit.com/product/2090): the module's size, its holes and weight, and the CST026
- [Adafruit, EAGLE files of the 2090](https://github.com/adafruit/Adafruit-2.8-TFT-with-Capacitive-Touch-PCB), rev D: the buffers and their direction, the pull-ups on RD, SDA, SCL and Lite, the backlight's resistors, the supervisor and the unconnected TE
- [Adafruit, 2.8" and 3.2" Color TFT Touchscreen Breakout v2](https://cdn-learn.adafruit.com/downloads/pdf/adafruit-2-8-and-3-2-color-tft-touchscreen-breakout-v2.pdf): Vin from 3 V, the touch controller's address
- [`datasheets/SN74LVC245A-TI.pdf`](../../datasheets/SN74LVC245A-TI.pdf), SCAS218X: the input levels, the transition rate, the input capacitance and the delay, standing for the module's 74LVC245
- [`datasheets/AP2112-Diodes.pdf`](../../datasheets/AP2112-Diodes.pdf), DS39724: the dropout, the output current and the accuracy
- [`datasheets/APX803-Diodes.pdf`](../../datasheets/APX803-Diodes.pdf), DS32131: the reset timeout, the threshold variants and their hysteresis
- [`datasheets/CST026-Hynitron.pdf`](../../datasheets/CST026-Hynitron.pdf): the touch controller's supply current and its I²C clock
- [FocalTech, FT6x06 datasheet](https://cdn-shop.adafruit.com/datasheets/FT6x06%20Datasheet_V0.1_Preliminary_20120723.pdf), V0.1: the time from reset to the first I²C answer
- [`datasheets/IMXRT1060CEC.pdf`](../../datasheets/IMXRT1060CEC.pdf), Rev. 4: Table 27, the pads' drive strengths
- [i.MX RT1060 reference manual](https://www.pjrc.com/teensy/IMXRT1060RM_rev3.pdf), Rev. 3: Table 14-5, the FlexIO clock's limit; 50.3.3.1, the shifters that drive several pins; Table 4-3, no DMA request for FlexIO3
- [SD Specifications Part 1, Physical Layer Simplified Specification](https://www.sdcard.org/downloads/pls/), version 6.00: the A1 class in 4.16, the source of the estimated read rate, and its clocks in Table 4-72
- [`datasheets/UM10204-NXP.pdf`](../../datasheets/UM10204-NXP.pdf): the Standard-mode rise time
- [`research/teensy-4.1.md`](../../research/teensy-4.1.md): the pins are not 5 V tolerant, and the 3V3 pin's budget
- [`pin-assignment.md`](../../pin-assignment.md): the display's pins and their cost
