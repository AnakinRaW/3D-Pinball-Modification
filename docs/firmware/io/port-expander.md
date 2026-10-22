# Port expander

The port expander gives the drivers eight more input pins, E-1 to E-8. The MCP23008 that [`docs/parts/port-expander/design.md`](../../parts/port-expander/design.md) describes reads them on the [I²C bus](i2c-bus.md) `wireBus`. A driver attaches to the pins its hardware is wired to. The port expander calls the driver back whenever one of those pins changes its level. The driver decides what the change means.

## Reading the pins

When a pin changes, the MCP23008 pulls its interrupt line INT low. At that moment it also takes a snapshot of all eight pins. INT stays low until the Teensy has read the chip.

The interrupt handler notes the time and requests one read from the I²C bus. The read returns the snapshot and the pins as they are now. A pin that changed back before the read still shows its change in the snapshot. The port expander reports the changes in the snapshot first and then those after it, each with the time of the interrupt.

The read takes 570 µs on the bus.

A read that fails leaves INT low, so no new edge comes. Every 100 ms the [driver tick](../driver-design.md#driver-intervaltimer) therefore requests a read as well.

## Set-up

The first `attach()` sets the expander up. It writes GPPU and GPINTEN, which switch on the pull-up and the interrupt on change at every pin. Every other setting keeps its power-on value. Every pin is then an input. A change at any pin pulls INT low.

The set-up then reads the chip once to learn every pin's starting level. Later changes are found against these levels. Last, the set-up arms the pin interrupt, which reacts when INT falls.

`attach()` works before the main loop only, so the set-up needs no lock. It returns false when the MCP23008 does not answer or another driver holds one of the pins.

## Errors

The port expander does not report faults itself. It remembers when a read last worked. It also keeps the error code of the last read that failed. A driver that uses the expander's pins checks both in its `failed()`. When no read has worked for longer than the driver allows, the driver counts as failed and reports that error code.

## Existing libraries

The port expander works the MCP23008 through the I²C bus, because the MCP23008 libraries wait in `Wire` until each transfer has ended. Those libraries are [RobTillaart/MCP23008](https://github.com/RobTillaart/MCP23008) and the MCP23X08 class of [Adafruit's MCP23017 library](https://github.com/adafruit/Adafruit-MCP23017-Arduino-Library).

## Interface

```cpp
namespace io {

// the eight pins E-1 to E-8, which the MCP23008 on wireBus reads
class PortExpander {
public:
    // what a driver gets for every change on its pins: the pins that changed, bit n for E-(n+1),
    // every pin's level after the change, 1 for high, and the moment INT fell
    using Change = teensy::inplace_function<void(uint8_t changed, uint8_t levels, uint32_t time), 16>;

    static constexpr uint8_t kPins = 8;   // E-1 to E-8

    // calls fn in the bus's interrupt for every change on the pins in mask, and sets the port
    // expander up at the first call; before the main loop only. False when the MCP23008 does not
    // answer or another driver holds a pin in mask
    bool attach(uint8_t mask, Change fn);

    uint8_t  levels() const;     // gets every pin's level as the last read found it, 1 for high
    uint32_t lastRead() const;   // gets nowUs() at the end of the last read that worked
    uint32_t errors() const;     // gets the controller's error flags of the last failed read
};

extern PortExpander expander;   // defined in the main file

}
```
