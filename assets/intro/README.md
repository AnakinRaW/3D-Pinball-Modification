# Intro

The machine's intro film: a ride as the ball through a multiball, into the drain, and on to the logo. It runs 260 frames at 25 a second. `intro.rgb` holds the frames at 320 × 240 in RGB565, and [`intro.wav`](intro.wav) holds the sound. The screen plays the pair as `video/intro.rgb` and `audio/intro.wav`, as [`media.md`](../media.md) gives.

## The film

The scripts in [`blender/`](blender/) animate the ride on the machine that [`cgi/`](../cgi/) builds, and render it. A script that runs inside Blender reads the folder it lies in from `BUILD`.

| Step | Script | Runs in | Makes |
|---|---|---|---|
| 1 | the steps of [`cgi/`](../cgi/) | Python and Blender | the machine and its lights at rest |
| 2 | `display.py <out>` | Python | `display/`: what the diorama's screen shows in each frame |
| 3 | `film.py` | Blender | the ride, the lights' keys, the logo's place in the film and the render settings |
| 4 | `render_frames.py` of `cgi/`, with `BUILD` set to `blender/` | Blender, with `FIRST` and `LAST` | `frames/`: the film in Cycles, at 640 × 480 |
| 5 | `convert.py <frames> <out>` | Python | `intro.rgb`, and `preview.gif` to look at |

## The sound

[`soundtrack.py`](blender/soundtrack.py) mixes `intro.wav` from the sources in [`sounds/`](sounds/) and from the balls' paths that [`paths.py`](blender/paths.py) writes in Blender to `blender/out/ball-paths.json`. The sound scripts run in WSL.

| Source | Made by |
|---|---|
| `acestep-circusmarch-sft.wav`, the circus march | `march.py`, with ACE-Step 1.5 |
| `multiball-am_michael.wav`, the announcer's call | `voice.py` with Kokoro, then `announcer.py` |
| `sfx/`, the bumper, the kickout and the riser | `sfx.py`, with Stable Audio Open Small |
| `roll-recording.wav`, `flipper-recording.wav`, `clack-recording.wav` | recordings of the kit: a steel ball rolling on plywood, a flipper, two steel balls knocked together |

[`roll.py`](blender/roll.py) lays a ball's roll out of the rolling recording, following the ball's speed. Everything else, from the chimes to the logo's kick, is synthesized in `soundtrack.py`.
