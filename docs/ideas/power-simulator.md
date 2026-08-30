# Power simulator

A budget calculator for the 5 V rail. The fixed consumers are entered once, then the solenoids and the LEDs are varied to find where the headroom runs out.

Figures from the [lighting design](../parts/lighting/design.md) and [`research/Rokr/1_power-supply.md`](../research/Rokr/1_power-supply.md).

| Fixed | |
|---|---|
| Teensy and logic | 0.15 A |
| Audio at full output | 0.45 A |
| Supply | 5 V, 6 A |

| Varied | |
|---|---|
| Solenoids energised | 0 to 3, 0.68 A each |
| Lit positions and their brightness | up to 135, 60 mA at full |

Output: the sum against the supply rating, and the brightness at which the two meet.

The 20 mA per die the lighting design works from is convention rather than measurement. Measuring a cut length of strip at a few brightness values replaces it, and until that happens the result carries an unknown factor.
