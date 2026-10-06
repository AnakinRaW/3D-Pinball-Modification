# Sound

## Speaker

One speaker sits in the stock machine, wired to the mainboard through a 2-pin connector. Its impedance measures 8 Ω. It carries no rating, and its size and build suggest 0.5 W to 1 W. Its share of the 5 V rail is budgeted in [`1_power-supply.md`](1_power-supply.md).

### Drive

Both of the speaker's wires carry a switching output of the mainboard, and neither is at ground. A multimeter on its DC range gave these readings while the background music played.

| Meter | Reading |
|---|---|
| Red probe on the red speaker wire, black probe on the supply's ground | 200 mV to 500 mV, jumping with the music |
| Red probe on the red speaker wire, black probe on the black speaker wire | −4 mV to −9 mV |
| In series with the red speaker wire, on its µA range | −100 µA to −180 µA |

Each wire averages a few hundred millivolts, where a linear amplifier would hold it at 2.5 V.

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