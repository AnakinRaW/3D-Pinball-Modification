# Adafruit 2167 IR break-beam pair

The through-beam sensor [Adafruit 2167](https://www.adafruit.com/product/2167) is used for ball drain detection. The actual sensor name is HD-DS25CM-3MM.

Emitter and receiver are separate bodies with 3 mm LEDs, each on flying leads, and they face each other across the ball path at a gap of 2 cm.

## Output polarity

Read on the bench at a 3.3 V supply, with 10 kΩ from the white lead to the supply: **3.3 V with the beam clear, 10 to 11 mV with the beam blocked.** The output transistor pulls the line to ground while the beam is blocked, so a controller input reads LOW for a ball in the gate.

A broken lead, a dead receiver and an unpowered emitter all leave the pull-up holding the input high, which reads as a clear beam.

## Ball speed the response time allows

The gate reports a 9 mm ball for as long as it blocks the beam. The 2 ms response time of the receiver bounds the fastest ball that can be reported at 4.5 m/s.

## Sources

- [Adafruit 2167 product page](https://www.adafruit.com/product/2167): the pull-up requirement, and that the output is low while the beam is blocked
- [HD-DS25CM-3MM datasheet](../../datasheets/HD-DS25CM-3MM-Adafruit.pdf): the 2 ms response time, and the lead colours
- Meter readings on the bench
