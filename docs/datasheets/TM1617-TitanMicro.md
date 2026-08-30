**Translation.** Rendered into English by Claude (Opus 5) from the Chinese original, Titan Micro document [TM1617_V1.2.pdf](https://akizukidenshi.com/goodsaffix/TM1617_V1.2.pdf). Titan Micro publishes no English revision of this part. Values, symbols and pin names are reproduced from the original; the wording is a translation with no second source to check it against.

# TM1617 — LED driver control IC with key-scan interface

## 1. Overview

The TM1617 is an LED display driver control IC with a keyboard scan interface. It integrates an MCU digital interface, data latches, LED drivers and key-scan circuitry. Intended for household appliances (smart water heaters, microwave ovens, washing machines, air conditioners, induction cookers), set-top boxes, electronic scales, smart meters and other seven-segment or LED display equipment.

## 2. Features

- CMOS process
- Display modes from 8 segments × 2 digits to 7 segments × 3 digits
- Key matrix of up to 6 × 1
- Brightness control, 8 duty-cycle steps
- Serial interface: CLK, STB, DIO
- Internal RC oscillator
- Internal power-on reset
- Internal data latch
- SOP16 package

## 3. Pin configuration

SOP16, top view:

| Pin | Name | | Pin | Name |
|---|---|---|---|---|
| 1 | DIO | | 16 | GRID1 |
| 2 | CLK | | 15 | GRID2 |
| 3 | STB | | 14 | GND |
| 4 | K1 | | 13 | GRID7/SEG12 |
| 5 | VDD | | 12 | SEG11 |
| 6 | SEG5/KS5 | | 11 | KS10/SEG10 |
| 7 | SEG6/KS6 | | 10 | KS9/SEG9 |
| 8 | SEG7/KS7 | | 9 | KS8/SEG8 |

## 4. Pin functions

| Symbol | Name | Pin | Description |
|---|---|---|---|
| DIO | Data input/output | 1 | Serial data is read in on the rising clock edge, LSB first, and driven out on the falling clock edge, LSB first. The output is a **P-channel open drain** |
| CLK | Clock input | 2 | Serial data is read on the rising edge and output on the falling edge |
| STB | Chip select input | 3 | A falling edge initialises the serial interface, which then waits for an instruction. The first byte after STB goes low is taken as the instruction, and any processing then in progress is aborted. While STB is high, CLK is ignored |
| SEG5/KS5 – SEG10/KS10 | Output (segment) | 6–11 | Segment outputs, also used as key-scan outputs. **P-channel open drain** |
| K1 | Key-scan data input | 4 | Data on this pin is latched at the end of the display cycle |
| SEG11 | Output (segment) | 12 | Segment output. **P-channel open drain** |
| GRID1 – GRID2 | Output (digit) | 15–16 | Digit outputs. **N-channel open drain** |
| SEG12/GRID7 | Output (segment/digit) | 13 | Multiplexed segment or digit output; one or the other, not both |
| VDD | Logic supply | 5 | Supply positive |
| GND | Logic ground | 14 | System ground |

The original prints the symbols as `SGE5/KS5` and `SEG12/DRID7` in this table; both are typographic errors for `SEG5/KS5` and `SEG12/GRID7`.

## 5. Instruction set

Instructions set the display mode and the state of the LED driver. The first byte sent on DIO after the falling edge of STB is taken as an instruction; bits B7 and B6 select which one.

| B7 | B6 | Instruction |
|---|---|---|
| 0 | 0 | Display mode setting |
| 0 | 1 | Data command setting |
| 1 | 0 | Display control command setting |
| 1 | 1 | Address command setting |

If STB is driven high during an instruction or data transfer, the serial communication is reinitialised and the instruction or data in flight is discarded. Whatever was transferred before it stays in effect.

### 5.1 Display mode setting

Selects the number of segments and digits (2–3 digits, 8–7 segments). Executing this instruction forces the display off. While the display mode is unchanged the contents of the display memory are not altered; the display control command switches the display on and off.

| B7 | B6 | B5–B2 | B1 | B0 | Display mode |
|---|---|---|---|---|---|
| 0 | 0 | don't care, write 0 | 0 | 0 | 2 digits, 8 segments |
| 0 | 0 | don't care, write 0 | 1 | 1 | 3 digits, 7 segments |

### 5.2 Data command setting

Sets data writing and reading. B1 and B0 must not be set to 01 or 11.

| B7 | B6 | B5–B2 | B1 | B0 | Function | Description |
|---|---|---|---|---|---|---|
| 0 | 1 | don't care, write 0 | 0 | 0 | Read/write mode setting | Write data to the display register |
| 0 | 1 | don't care, write 0 | 1 | 0 | Read/write mode setting | Read key-scan data |
| 0 | 1 | don't care, write 0 | — | 0 | Address mode setting | Automatic address increment |
| 0 | 1 | don't care, write 0 | — | 1 | Address mode setting | Fixed address |
| 0 | 1 | don't care, write 0 | 0 | — | Test mode setting (internal use) | Normal mode |
| 0 | 1 | don't care, write 0 | 1 | — | Test mode setting (internal use) | Test mode |

### 5.3 Display control command setting

Switches the display on and off and sets the brightness. Eight brightness steps are available.

| B7 | B6 | B5–B3 | B2 | B1 | B0 | Function |
|---|---|---|---|---|---|---|
| 1 | 0 | don't care, write 0 | 0 | 0 | 0 | Pulse width 1/16 |
| 1 | 0 | don't care, write 0 | 0 | 0 | 1 | Pulse width 2/16 |
| 1 | 0 | don't care, write 0 | 0 | 1 | 0 | Pulse width 4/16 |
| 1 | 0 | don't care, write 0 | 0 | 1 | 1 | Pulse width 10/16 |
| 1 | 0 | don't care, write 0 | 1 | 0 | 0 | Pulse width 11/16 |
| 1 | 0 | don't care, write 0 | 1 | 0 | 1 | Pulse width 12/16 |
| 1 | 0 | don't care, write 0 | 1 | 1 | 0 | Pulse width 13/16 |
| 1 | 0 | don't care, write 0 | 1 | 1 | 1 | Pulse width 14/16 |

| B7 | B6 | Bit | Function |
|---|---|---|---|
| 1 | 0 | 0 | Display off |
| 1 | 0 | 1 | Display on |

### 5.4 Address command setting

Sets the display register address. The chip has at most six valid addresses: C0H, C1H, C2H, C3H, CCH, CDH. At power-up the first address defaults to C0H.

| B7 | B6 | B5–B4 | B3–B0 | Display address |
|---|---|---|---|---|
| 1 | 1 | don't care, write 0 | 0000 | C0H |
| 1 | 1 | don't care, write 0 | 0001 | C1H |
| 1 | 1 | don't care, write 0 | 0010 | C2H |
| 1 | 1 | don't care, write 0 | 0011 | C3H |
| 1 | 1 | don't care, write 0 | 1100 | CCH |
| 1 | 1 | don't care, write 0 | 1101 | CDH |

## 6. Display register addresses

The register holds the data the TM1617 receives over the serial interface from an external device. At most six addresses are valid — C0H, C1H, C2H, C3H, CCH, CDH — each mapping to the chip's SEG and GRID pins. Display data is written address by address from low to high, and byte by byte from the low bit to the high bit.

| Low byte | High byte | Digit |
|---|---|---|
| C0H<sub>L</sub> | C0H<sub>U</sub>, C1H<sub>L</sub>, C1H<sub>U</sub> | GRID1 |
| C2H<sub>L</sub> | C2H<sub>U</sub>, C3H<sub>L</sub>, C3H<sub>U</sub> | GRID2 |
| CCH<sub>L</sub> | CCH<sub>U</sub>, CDH<sub>L</sub>, CDH<sub>U</sub> | GRID7 |

Bits B0 to B7 of the low byte and B0 to B7 of the high byte map onto SEG5 through SEG12 in order, with four don't-care bits at each end of the pair.

**At power-up the display register holds a random value.** Sending a display-on command straight away can therefore produce garbage on the display. Titan Micro recommends clearing the register on power-up by writing `0x00` to all six addresses.

## 7. Display

Section heading in the original: *driving a common-cathode display.*

Figure 7 shows the connection for a common-cathode seven-segment display. To display a "0", write `0xF0` LSB first to address 00H (GRID1) and `0x03` LSB first to address 01H (GRID1).

> **Note: when driving a common-cathode display, the SEG pins may be connected only to the LED anodes, and GRID only to the LED cathodes. They must not be reversed.**

## 8. Key scanning and the key data register

The chip supports a key matrix of up to 1 × 6 bit, on KS5, KS6, KS7, KS8, KS9 and KS10 against K1.

After the read-key command is sent, five bytes of key data are read out, BYTE1 to BYTE5, LSB first. B7 and B6 are invalid bits and always read 0. When the key at the intersection of the K pin and a KS pin is pressed, the corresponding bit in the byte is 1.

| | B0 | B1 | B2 | B3 | B4 | B5 | B6 | B7 |
|---|---|---|---|---|---|---|---|---|
| Header | K1 | X | X | K1 | X | X | X | X |
| BYTE1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| BYTE2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| BYTE3 | KS5 | 0 | 0 | KS6 | 0 | 0 | 0 | 0 |
| BYTE4 | KS7 | 0 | 0 | KS8 | 0 | 0 | 0 | 0 |
| BYTE5 | KS9 | 0 | 0 | KS10 | 0 | 0 | 0 | 0 |

Notes from the original:

1. At most five bytes may be read from the TM1617; reading further is not permitted.
2. The bytes must be read in order from BYTE1 to BYTE5; bytes may not be skipped. To read the key on K1 × KS10, for example, the read has to continue to bit 4 of the fifth byte.

**Combined key presses.** SEG1/KS1 to SEG10/KS10 are shared between display and key scanning. Taking figure 12 as the example: showing D1 lit and D2 dark requires SEG1 at "0" and SEG2 at "1". If S1 and S2 are pressed at the same time, SEG1 and SEG2 are shorted together and both D1 and D2 light. The remedy in figure 14 is a diode in series with each key.

## 9. Keys

Key scanning runs automatically inside the TM1617 and is not under user control; the user only reads the key value in the correct sequence. One complete key scan takes two display cycles, and one display cycle takes roughly **T = 4 ms**. If two different keys are pressed within 8 ms, both reads return the value of the key pressed first. The valid outputs are SEG5–SEG10.

From the internal scan waveform, figure 10: SEG5/KS5 to SEG8/KS8 are scanned in one cycle and SEG9/KS9 to SEG10/KS10 in the next. The waveform is annotated 66 µs and 264 µs against the 4 ms cycle. When the read-key command is sent and the scan's high level reaches K1 through a closed key, the chip recognises it and sets the corresponding bit in the five key bytes.

> **Note: the display cycle depends on the IC's oscillator frequency, which is not identical from part to part. The figures above are for reference; measurement governs.**

## 10. Serial data transfer format

One bit is read or received on each rising clock edge.

Writing: STB goes low, then eight bits B0 to B7 are clocked in on DIO.

Reading: the read-key command is sent, then after a wait the key bytes are clocked out.

> **Note: when reading, a wait time T<sub>wait</sub> of at least 2 µs is required between setting the instruction on the eighth rising edge of CLK and reading data on the falling edge of CLK.**

## 11. Serial transfers in an application

### 11.1 Automatic address increment

The address command sets the start address of the data stream. Once it has been sent, STB stays low and the data follows immediately, up to 14 bytes. STB goes high only when the transfer is complete.

| Step | Content |
|---|---|
| Command 1 | Set display mode |
| Command 2 | Set data command |
| Command 3 | Set display address |
| Data 1…n | Display data into the address from command 3 and the addresses after it, at most 14 bytes |
| Command 4 | Display control command |

### 11.2 Fixed address

The address command sets the address for the single byte that follows. STB stays low, one byte is sent, and the address for the next byte is then set again. At most 6 bytes are transferred before STB goes high.

| Step | Content |
|---|---|
| Command 1 | Set display mode |
| Command 2 | Set data command |
| Command 3 | Set display address 1 |
| Data 1 | Display data 1 into the address from command 3 |
| Command 4 | Set display address 2 |
| Data 2 | Display data 2 into the address from command 4 |
| Command 5 | Display control command |

### 11.3 Key read sequence

Command 1 sets the read-key command, then Data 1 to Data 5 are read out.

### 11.4 Program flow

The worked example in the original uses these command bytes: display mode `03H`, write display memory with automatic address increment `40H` or with fixed address `44H`, start address `0C0H`, display control at pulse width 11/16 `8CH`, and read key data `42H`.

## 12. Application circuit

Figure 18 shows the TM1617 driving three common-cathode seven-segment digits: SEG5–SEG11 to segments a to g of each digit, and GRID1, GRID2 and GRID7 to the three commons.

Notes from the original:

1. The filter capacitor between VDD and GND is to be placed as close to the TM1617 as the layout allows.
2. Three 100 pF capacitors on the DIO, CLK and STB lines reduce interference on the communication port.
3. A blue seven-segment display has a forward drop of about 3 V, so the TM1617 is to be supplied from 5 V.

The circuit also shows 10 kΩ pull-ups on DIO, CLK and STB towards the MCU, and 100 µF with 100 nF across the supply.

## 13. Electrical parameters

### Absolute maximum ratings

T<sub>a</sub> = 25 °C, V<sub>SS</sub> = 0 V.

| Parameter | Symbol | Range | Unit |
|---|---|---|---|
| Logic supply voltage | V<sub>DD</sub> | −0.5 … +7.0 | V |
| Logic input voltage | V<sub>I1</sub> | −0.5 … V<sub>DD</sub> + 0.5 | V |
| LED SEG drive output current | I<sub>O1</sub> | −50 | mA |
| LED GRID drive output current | I<sub>O2</sub> | +200 | mA |
| Power dissipation | P<sub>D</sub> | 400 | mW |
| Operating temperature | T<sub>opt</sub> | −40 … +80 | °C |
| Storage temperature | T<sub>stg</sub> | −65 … +150 | °C |
| ESD, machine model | | 200 | V |
| ESD, human body model | | 2000 | V |

### Normal operating range

T<sub>a</sub> = −20 … +80 °C, V<sub>SS</sub> = 0 V.

| Parameter | Symbol | Min | Typ | Max | Unit |
|---|---|---|---|---|---|
| Logic supply voltage | V<sub>DD</sub> | — | 5 | — | V |
| High-level input voltage | V<sub>IH</sub> | 0.7 V<sub>DD</sub> | — | V<sub>DD</sub> | V |
| Low-level input voltage | V<sub>IL</sub> | 0 | — | 0.3 V<sub>DD</sub> | V |

The supply row carries a value in the typical column only; the minimum and maximum columns are empty in the original.

### Electrical characteristics

T<sub>a</sub> = −20 … +80 °C, V<sub>DD</sub> = 5 V, V<sub>SS</sub> = 0 V.

| Parameter | Symbol | Min | Typ | Max | Unit | Test condition |
|---|---|---|---|---|---|---|
| High-level output current | I<sub>OH1</sub> | 20 | 30 | 60 | mA | SEG5–SEG11, V<sub>O</sub> = V<sub>DD</sub> − 3 V |
| High-level output current | I<sub>OH1</sub> | 20 | 25 | 50 | mA | SEG5–SEG11, V<sub>O</sub> = V<sub>DD</sub> − 2 V |
| Low-level output current | I<sub>OL</sub> | 80 | 120 | — | mA | GRID1, GRID2, V<sub>O</sub> = 0.3 V |
| Low-level output current | I<sub>dout</sub> | 3 | — | — | mA | V<sub>O</sub> = 0.4 V, DOUT |
| High-level output current tolerance | I<sub>tolsg</sub> | — | — | 5 | % | SEG5–SEG11, V<sub>O</sub> = V<sub>DD</sub> − 3 V |
| High-level input voltage | V<sub>IH</sub> | 0.7 V<sub>DD</sub> | — | — | V | CLK, DIO, STB |
| Low-level input voltage | V<sub>IL</sub> | — | — | 0.3 V<sub>DD</sub> | V | CLK, DIO, STB |

The I<sub>OL</sub> row is labelled "low-level **input** current" in the original, against a symbol and a test condition that both belong to the GRID output. Read as the GRID sink current.

### Switching characteristics

T<sub>a</sub> = −20 … +80 °C, V<sub>DD</sub> = 5 V.

| Parameter | Symbol | Min | Max | Unit | Test condition |
|---|---|---|---|---|---|
| Propagation delay | t<sub>PLZ</sub> | — | 300 | ns | CLK → DIO |
| Propagation delay | t<sub>PZL</sub> | — | 100 | ns | C<sub>L</sub> = 15 pF, R<sub>L</sub> = 10 kΩ |
| Rise time | t<sub>TZH1</sub> | 1 | 2 | µs | SEG5–SEG11, C<sub>L</sub> = 300 pF |
| Rise time | t<sub>TZH2</sub> | — | 0.5 | µs | SEG12/GRID7, C<sub>L</sub> = 300 pF |
| Fall time | t<sub>THZ</sub> | — | 1.5 | µs | SEGn, GRIDn, C<sub>L</sub> = 300 pF |
| Maximum input clock frequency | F<sub>max</sub> | — | 1 | MHz | 50 % duty cycle |
| Input capacitance | C<sub>I</sub> | — | 15 | pF | |

### Timing characteristics

T<sub>a</sub> = −20 … +80 °C, V<sub>DD</sub> = 5 V.

| Parameter | Symbol | Min | Unit | Test condition |
|---|---|---|---|---|
| Clock pulse width | PW<sub>CLK</sub> | 500 | ns | |
| Strobe pulse width | PW<sub>STB</sub> | 1 | µs | |
| Data setup time | t<sub>SETUP</sub> | 100 | ns | |
| Data hold time | t<sub>HOLD</sub> | 100 | ns | |
| CLK to STB time | t<sub>CLK-STB</sub> | 1 | µs | CLK ↑ → STB ↑ |

## 14. Package

SOP16. The original gives the package outline with its dimensions as a figure; the dimension table is not reproduced here.

The original closes with: all specifications and applications shown are subject to change without prior notice.
