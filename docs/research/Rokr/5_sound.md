# Sound

## Speaker

One speaker sits in the stock machine, wired to the mainboard through a 2-pin connector. Its impedance measures 8 Ω. Its share of the 5 V rail is budgeted in [`1_power-supply.md`](1_power-supply.md).

> [!WARNING]
> **TODO: Measure speaker power. Should be approx 0.5 W - 1 W, given its size and quality.**

## Gameplay sounds

Seven distinct sounds were heard.

| Sound | Plays when |
|---|---|
| Background music | Continuously, unless switched off with P26 |
| Win music | A game is won |
| Loss music | A game is lost |
| Drain | The ball drains |
| Bumper hit | A bumper fires |
| Track effect A | A 50 point track award |
| Track effect B | A 100 point track award |

A switch on P26 enables and disables the background music.

Whether the audio is synthesized at runtime or played back from samples is unknown.

## Playback

Only one sound plays at a time. Any effect sound interrupts the background music, and the background music starts again from its beginning once the effect has finished. Switching P26 back on restarts it from the beginning as well.