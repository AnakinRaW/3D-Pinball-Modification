# Power simulator

A budget calculator for the 5 V rail. The fixed consumers are entered once, then the solenoids and the LEDs are varied to find where the headroom runs out.

Figures from the [lighting design](../parts/lighting/design.md), the [audio design](../parts/audio/design.md) and [`research/Rokr/1_power-supply.md`](../research/Rokr/1_power-supply.md).

| Fixed | |
|---|---|
| Teensy and logic | 0.30 A |
| Audio, capped at 0.5 W | 0.13 A |
| Supply | 5 V, 3 A |

| Varied | |
|---|---|
| Solenoids energised | 0 to 3, 0.68 A each |
| Lit positions and their brightness | up to 162, at the currents the [lighting design](../parts/lighting/design.md#current-ceiling) measured |

Output: the sum against the supply rating, and the brightness at which the two meet.
