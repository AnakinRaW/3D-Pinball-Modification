# Teensy motherboard

The Teensy motherboard carries the Teensy, the DFR0954 amplifier module and the port expander. It hands the Teensy's rails on to the subsystems.

The Teensy's edge headers offer two 3V3 pins and three GND pins. Six subsystems run from the Teensy's 3V3. Eight cables and the hit targets carry the Teensy's ground. The board spreads both over a rail each.

## Circuit

![Teensy motherboard schematic](motherboard-schematic.svg)

### Supply

J-PWR takes 5 V and GND from the power board's [J-MAIN](../power/design.md#outputs). Its GND joins the GND rail.

The Teensy runs from this 5 V or from USB alone. Its trace between VUSB and VIN is cut. Two diodes feed VIN from either source and keep the sources apart:

- D1 is a 1N5819 between J-PWR's 5 V and VIN. While the machine is off, it keeps USB from feeding the power board.
- D2 is a 1N5819 across the cut on the Teensy's underside. It keeps the power board from feeding USB.

The DFR0954 takes its 5 V ahead of D1, so it stays off on USB alone. The node behind D1 feeds both the Teensy's VIN and R2 of the break beam.

### 3V3 rail

The 3V3 rail starts at the Teensy's two 3V3 pins and feeds these subsystems.

| Subsystem | What the 3V3 feeds |
|---|---|
| [IR sensing](../ir-reflective/design.md) | the isolator's Teensy side, over the mainboard's J-T |
| [Solenoids](../solenoid/design.md) | the foil, over the sense board's J-T |
| [Break beam](../break-beam/design.md) | R3, the receiver's pull-up |
| [Magnetic rotary sensor](../magnetic-rotary/design.md) | the module |
| [Display](../display/design.md) | the touch converter's LV side, over the touch cable |
| [Port expander](../port-expander/design.md) | U1 and the bus pull-ups R5 and R6 |

### GND rail

The GND rail joins the Teensy's three GND pins, the DFR0954's GND, the port expander's GND and J-PWR's GND. These cables take their ground from it: the IR mainboard's J-T, the solenoid sense board's J-T, the lighting data cable, the rotary sensor's cable, the display bus, the break beam's emitter, its receiver, the toggle switch and the hit targets.

### Toggle switch

R1 sits on the motherboard in series between pin [32](../../pin-assignment.md "Toggle switch") and the toggle switch. The [controls design](../controls/design.md) sets its value.

### Break beam

R2 feeds the break beam's emitter from the supply behind D1. R3 pulls pin [31](../../pin-assignment.md "Receiver output of the ball drain gate") up to the 3V3 rail. The [break-beam design](../break-beam/design.md) sets both values.

### Audio

The DFR0954 sits beside the Teensy. Pins [2](../../pin-assignment.md "DIN to the DFR0954"), [3](../../pin-assignment.md "LRC to the DFR0954") and [4](../../pin-assignment.md "BCLK to the DFR0954") drive it over I²S on the board. R4 sits between its GAIN and VCC pins. The [audio design](../audio/design.md) sets its value.

### Port expander

U1 reads the pins E-1 to E-8. It sits on `Wire` with SDA on pin [18](../../pin-assignment.md "SDA on Wire") and SCL on pin [19](../../pin-assignment.md "SCL on Wire"). Its INT goes to pin [1](../../pin-assignment.md "Interrupt from the port expander"). R5 and R6 pull SDA and SCL up to the 3V3 rail. C1 decouples U1's VDD. The [port expander design](../port-expander/design.md) sets their values.

### Hit targets

The two bells come in at E-1 and E-2. They are [hit targets](../targets/design.md). Their foil takes its ground from the GND rail.
