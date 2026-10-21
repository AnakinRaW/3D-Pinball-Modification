# PSRAM

The PSRAM adds 8 MB of memory to the Teensy. PSRAM is memory that the processor reads and writes like its own RAM, held in a separate chip. The chip is the AP Memory APS6404L-3SQR.

## Circuit

The chip sits on the smaller memory pads on the Teensy's underside, under the SD socket. Teensyduino finds a single chip only on these pads.

### Signal

The processor reaches the chip through FlexSPI2, its controller for external memory. [`pin-assignment.md`](../../pin-assignment.md) lists the pads FlexSPI2 takes.

### Supply

The chip runs from the Teensy's 3.3 V regulator, as the card socket does. Its current therefore comes out of the same 250 mA. It draws at most 17.6 mA. The datasheet gives 7 mA for the chip itself. Charging the four data lines adds at most 10.6 mA, with the most load the datasheet allows on each line.

## Firmware

Variables declared with `EXTMEM` and memory from `extmem_malloc()` lie in the PSRAM. The processor's cache serves it the way it serves RAM2.

## Part list

| Qty | Part | Where |
|---|---|---|
| 1 | SparkFun COM-28391, an AP Memory APS6404L-3SQR | on the smaller memory pads under the Teensy |

## Appendix: derivations

Every figure below is recomputed by [`figures.py`](figures.py) from the inputs it names, and `tools/figcheck.py` compares each one against the line that states it.

### Chip

```
PSRAM     the chip's size                          8 MB
          its current while it reads or writes,
          at most                                  7 mA
          the clock its datasheet allows at
          3.3 V                                    109 MHz
          FlexSPI2's clock                         105.6 MHz
          the most load the datasheet allows on
          each of the four data lines              15 pF
          the Teensy's 3.3 V at most               3.333 V
          the current that charges them, each
          flipping on every clock                = 10.6 mA
          the chip on the Teensy's 3.3 V rail    = 17.6 mA
```

## Sources

- [APS6404L-3SQR datasheet](../../datasheets/APS6404L-3SQR-APMemory.pdf), Rev. 2.3: the PSRAM's size, its current, its clock at 3.3 V and the load it allows on each pin
- [PJRC, PSRAM chip for Teensy 4.1](https://www.pjrc.com/store/psram.html): the pads a single chip goes on
- [PaulStoffregen/cores, `teensy4/startup.c`](https://github.com/PaulStoffregen/cores/blob/1.62/teensy4/startup.c), `configure_external_ram()`: the chip's detection, its pads and FlexSPI2's clock
- [`research/teensy-4.1.md`](../../research/teensy-4.1.md): the 250 mA of the Teensy's 3.3 V rail and the card socket on it
