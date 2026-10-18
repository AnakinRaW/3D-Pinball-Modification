# Power supply

The power board takes the 5 V of an external adapter and passes it to every subsystem of the machine. It holds the rail through load steps. An optional switch protects the machine against plus and minus swapped at its input.

## Requirements

- The machine runs from a 5 V adapter of 3 A up to 6 A.
- Every subsystem takes its 5 V and its ground over a separate pair from one point on the board.
- The rail stays inside the range every subsystem accepts, at power-up and at every load step.

## Adapter

The adapter needs short-circuit protection, since no output has a fuse and a short on any output is the adapter's to interrupt. At its full current it has to deliver at least 5.0 V at the jack. With no load it may put out up to 5.25 V. The 5.0 V are the starting point of every subsystem's lowest supply voltage.

## Loads

| Load | Current |
|---|---|
| The Teensy, the amplifier, the IR mainboard and the display, which draw all the time | 874 mA |
| The lighting at its firmware cap, with the 3 A adapter | 2.0 A |
| The lighting at its firmware cap, with the 6 A adapter | 3.3 A |
| Two bumpers and the scoop pulling at once, for at most 30 ms | 2.16 A |
| The servo stalled | 0.8 A |
| The most that draws at once, with the 3 A adapter's cap | 5.04 A |
| The most that draws at once, with the 6 A adapter's cap | 6.34 A |

One ball triggers at most two bumpers within one pull-in, however fast it is, while a second ball may sit in the scoop and be ejected at the same moment. Two bumpers and the scoop are therefore the coil case. The servo moves only while no ball is in play.

## Circuit

![Power board schematic](power-schematic.svg)

### Input

The adapter's plug goes into the machine's barrel jack. A 16 AWG pair of at most 30 cm takes the 5 V from the jack to J-IN on the board.

C1 and C2 keep the rail steady when the load steps. They are low-impedance electrolytics which keep the rail above every subsystem's lowest supply voltage when the lighting, the coils or the servo start. Their 2000 µF keep the rail under the 5.5 V the Teensy's VIN takes when the whole load stops. They sit within 20 mm of J-IN.

### Input protection

The board proposes an optional protection against swapped polarity. Q1 protects the machine against swapped polarity at J-IN. It is an N-channel MOSFET in the ground line, with its drain at the adapter's negative and its source on the board's ground. R1 ties its gate to the 5 V bus, so Q1 conducts with the right polarity and stays off with swapped polarity. Q1 and R1 are optional. Without them a wire joins J-IN's negative to the board's ground. Every figure of this design is derived with Q1 fitted, and leaving it out only raises the bus.

> [!WARNING]
> Without Q1, swapped polarity at J-IN reaches every subsystem. It destroys the Teensy and the ICs on the boards.

> [!WARNING]
> The board has no overvoltage protection. An adapter above 5.5 V destroys the Teensy.

> [!WARNING]
> The board also has no fuse. The board relies on the adapter's overcurrent protection.

With the Teensy on USB, the USB cable connects the machine's ground to the computer's ground. If the computer and the adapter are both earthed, part of the machine's current flows back through the USB cable instead of the adapter's cable. The computer can then lose the USB connection to the Teensy, but nothing gets damaged.

### Outputs

| Output | To | Current at most |
|---|---|---|
| J-LIGHT | the lighting board | 2.0 A with the 3 A adapter, 3.3 A with the 6 A adapter |
| J-SOL | the solenoids' coil board | 2.84 A |
| J-SERVO | the servo board | 0.8 A |
| J-AMP | the amplifier board | 129 mA |
| J-TEENSY | the Teensy's VIN | 350 mA |
| J-IR | the D24V5F3 at the IR mainboard | 198 mA |
| J-DISP | the display board | 198 mA |

### Ground

Every output carries its own ground wire back to one point at C1 and C2. The coils, the LEDs and the servo return their currents there.

The adapter's negative reaches the machine through Q1 or its wire alone.

### Teensy Supply

J-TEENSY carries 5 V to the Teensy's VIN. 

The Teensy's VUSB-VIN trace is cut, as [`teensy-4.1.md`](../../research/teensy-4.1.md#powering-from-something-other-than-usb) requires for an external supply.

### Wiring

Every output is rated for 22 AWG wiring. A 22 AWG wire carries at most 7 A, more than the 6 A adapter is rated for. 

For better performance, thicker wires can be used for DC-IN-to-board, J-LIGHT-to-board and the ground star point. 

For better convenience, thinner wires such as 26 AWG can be used for low current consumers.


Both adapters deliver less than the machine draws at its peak. With the 3 A adapter, the lighting at its cap leaves no room for a coil. With the 6 A adapter, the lighting at its cap, two bumpers and the scoop together ask for 0.34 A more than the adapter delivers, for at most 30 ms. For that moment the adapter limits its current, so the voltage drops.

## Part list

| Qty | Part | Where |
|---|---|---|
| 1 | STP160N3LL, N-channel MOSFET, TO-220 | Q1, the optional reverse-polarity switch in the ground line |
| 1 | 10 kΩ resistor | R1, with Q1, from its gate to the 5 V bus |
| 2 | Rubycon 16ZLH1000MEFC8X20, 1000 µF, 16 V | C1 and C2, the bulk at J-IN |

## Appendix: derivations

[`figures.py`](figures.py) recomputes every figure below from its inputs. `tools/figcheck.py` checks each one against its line and each drawn figure through its `data-fig` anchor.

### Loads

```
LOADS     the smaller adapter                      3 A
          the larger adapter                       6 A
          the lighting's cap with the 3 A adapter  2.0 A
          and with the 6 A adapter                 3.3 A
          all four coils at once                   2.84 A
          one bumper coil                          0.68 A
          one pull-in, at most                     30 ms
          the servo stalled                        800 mA
          the amplifier at its capped output       129 mA
          the Teensy itself, PJRC's figure         100 mA
          its 3V3 pin at its full rating           250 mA
          the Teensy's VIN, behind a linear
          regulator                              = 350 mA
          the IR mainboard, sixteen channels       222 mA
          at its module's highest output           3.432 V
          the module's efficiency, assumed         80 %
          the module's input current at its
          lowest input, which bounds J-IR's drop = 281 mA
          the module's input current             = 198 mA
          the display                              198 mA
          the permanent consumers                = 874 mA
          the most that draws at once, with the
          3 A adapter                            = 5.04 A
          with the 6 A adapter                   = 6.34 A
          the lighting at its 6 A cap with two
          bumpers and the scoop, past the adapter= 0.34 A
```

### Input path

```
INPUT     the adapter at the jack, at least        5.0 V
          the pair to the board                    30 cm
          16 AWG                                   13.17 mΩ/m
          the pair, both wires                   = 7.9 mΩ
          Q1's R_DS(on) at 25 °C                   4.2 mΩ
          the gate voltage that row needs          4.5 V
          Figure 10 at                             95 °C
          its factor there                       ≤ 1.4
          Q1's R_DS(on) below that junction      = 5.88 mΩ
          what Q1 dissipates at 6 A              = 0.22 W
          Q1's thermal resistance                  62.5 °C/W
          the air around it                        40 °C
          Q1's junction at 6 A                   = 53.3 °C
          Q1's leakage when it blocks              1 µA
          the board's wiring, both conductors,
          estimated                                5 mΩ
          the bus at 3 A, at least               = 4.94 V
          the bus at 6 A, at least               = 4.88 V
```

### Rail ceiling

```
RAIL      the adapter with no load, assumed        5.25 V
          the adapter's cable, from                1.0 m
          up to                                    2.0 m
          18 AWG, assumed for that cable           20.95 mΩ/m
          a loose pair's inductance                1 µH/m
          the cables at their longest            = 2.3 µH
          C1 and C2, each                          1000 µF
          together                               = 2000 µF
          each, its impedance                      29 mΩ
          together                               = 14.5 mΩ
          the whole 3 A released at once         = 5.36 V
          the whole 6 A released at once         = 5.46 V
          the same with one capacitor            = 5.54 V
          what the Teensy's VIN takes              5.5 V
          the cables at their shortest           = 54.8 mΩ
          the damping of a live plug-in          = 1.07
          the charging current at plug-in        = 72.2 A
          what Figure 12 plots Q1's body diode
          to                                       80 A
          the gate voltage at which Q1 takes
          over from its body diode, at most        2.5 V
```

### Supply floors

```
FLOORS    what the lighting's buffers need         4.5 V
          the lighting's buffers at 3 A          = 4.75 V
          the lighting's buffers at 6 A          = 4.58 V
          one wire of a cable with its contacts    0.1 Ω
          the servo's buffer while it stalls,
          at 3 A                                 = 4.77 V
          at 6 A                                 = 4.71 V
          two bumpers and the scoop at once        2.16 A
          the solenoids' coil board while they pull,
          at 3 A                                 = 4.48 V
          at 6 A                                 = 4.42 V
          what a stock coil kicks down to          4.4 V
          the Teensy's VIN at 6 A                = 4.81 V
          what it needs                            3.6 V
          the IR mainboard's module at 6 A       = 4.83 V
          what it needs                            3.4 V
          the display at 6 A                     = 4.84 V
          what its regulator needs, at least       3.49 V
```

## Sources

- [ST STP160N3LL](../../datasheets/STP160N3LL-ST.pdf), DocID025073 Rev 3: the ratings in Table 2, the thermal resistance in Table 3, R_DS(on), the gate threshold and the leakage in Table 4, and Figure 10 for R_DS(on) against temperature
- [Pololu D24V5F3](https://www.pololu.com/product/2842): the typical efficiency of the IR mainboard's module
- [Rubycon ZLH](../../datasheets/ZLH-Rubycon.pdf): C1's and C2's impedance, 16ZLH1000MEFC8X20, read from [DigiKey's listing](https://www.digikey.de/en/products/result?keywords=16ZLH1000MEFC8X20)
- [PowerStream, wire gauge and current limits](https://www.powerstream.com/Wire_Size.htm): the chassis wiring current of 16 and 22 AWG
- ASTM B258: the resistance of 16 and 18 AWG annealed copper
- [`research/teensy-4.1.md`](../../research/teensy-4.1.md): VIN's range, the 3V3 pin's rating, the Teensy's own draw and the VUSB-VIN trace
- [`parts/lighting/design.md`](../lighting/design.md), [`parts/solenoid/design.md`](../solenoid/design.md), [`parts/servo/design.md`](../servo/design.md), [`parts/audio/design.md`](../audio/design.md), [`parts/ir-reflective/design.md`](../ir-reflective/design.md), [`parts/display/design.md`](../display/design.md): what each subsystem draws and the supply range it needs
