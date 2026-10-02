# Break beam at the ball drain

A break-beam gate across the drain reports the ball leaving play. The pair is an [Adafruit 2167](../../research/Sensors/adafruit-2167-ir-break.md), emitter and receiver in separate bodies carrying 3 mm LEDs, facing each other across the drain at a gap of 2 cm.

## Circuit & Signal

Both bodies are powered directly from the Teensy's own 3V3 and GND pins. 

The receiver's output is an open collector, and the pull-up inside the Teensy holds it high, switched on with `INPUT_PULLUP`. A simple digital pin with no further requirements can be used.

A LOW signal means a ball is breaking the IR beam. The output transistor pulls the line to ground while the beam is blocked and lets go when it clears.

## Firmware

A driver interrupts when a ball breaks the IR beam and publishes it to the event queue [`event queue`](../../../firmware/input-handling.md). [`firmware/drivers/break-beam.md`](../../../firmware/drivers/break-beam.md) describes the driver.

## Mounting

The two bodies of the sensor module are mounted at the same location as in the stock machine. However, the two bodies do not fit into the original wood and plastic cut outs of the stock machine. The cut outs need to be modified to fit.

## Part list

One Adafruit 2167 break-beam sensor.

## Appendix: derivations

Every figure below is recomputed by [`figures.py`](figures.py) from the inputs it names, and `tools/figcheck.py` compares each one against the line that states it.

**The gate.** What the module brings, what the Teensy brings, and what follows from the two.

```
V_3V3     the Teensy's 3.3 V rail, which both
          bodies run from                         3.3 V
          the module's supply range starts at     3.0 V
R_PU      the pull-up inside the Teensy           22 kΩ
I_PU      V_3V3 / R_PU, what the output sinks
          while the beam is blocked             = 150 µA
          the output is rated to sink             100 mA
I_SUP     the emitter                             10 mA
          the receiver, bounded at the emitter's  10 mA
          the pair                              = 20 mA
          what the Teensy's 3V3 pin allows        250 mA
V_OL      output low, read on the bench           11 mV
D_BALL    the steel ball                          9 mm
V_MAX     the receiver's response time            2 ms
          D_BALL / that time, the fastest ball
          the gate still reports                = 4.5 m/s
          the fastest ball the build assumes      3 m/s
T_BLOCK   D_BALL / that speed, how long it
          blocks the beam                       = 3 ms
```

## Sources

- [`research/Sensors/hd-ds25cm-3mm.md`](../../research/Sensors/adafruit-2167-ir-break.md): the module's ratings, its lead colours, and the bench reading of the two output levels
- [`research/teensy-4.1.md`](../../research/teensy-4.1.md): what reaches an unpowered pin, and the 3.3 V rail available to external circuits
- [`pin-assignment.md`](../../pin-assignment.md): which pins were free, and what pin [31](../../pin-assignment.md "Receiver output of the ball drain gate") costs
