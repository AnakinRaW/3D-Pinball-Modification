# SMD experiment: the whole mod on one board

A side experiment in KiCad 10. All of the mod's boards are merged onto one two-layer SMD board of 108 × 90 mm, with every part on the front. The Teensy 4.1 and the DFRobot DFR0954 amplifier plug into female headers. Everything off the board connects through 38 top-entry JST connectors. The build of record stays through-hole on perfboards (rule 17).

Where it departs from the real design:

- One SN74LVC4245A replaces the two CD74HCT125 level shifters.
- An AMS1117-3.3 replaces the Pololu D24V5F3 for the IR rail.
- The isolator is gone, with 2.2 kΩ series resistors in its SPI lines.
- The board has a single ground.

ERC and DRC are clean. The board was never ordered or tested.
