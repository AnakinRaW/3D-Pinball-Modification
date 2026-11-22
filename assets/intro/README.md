# Intro

The machine's intro film: a ride as the ball through a multiball, into the drain, and on to the logo. It runs 260 frames at 25 a second. `intro.rgb` holds the frames at 320 × 240 in RGB565, and [`intro.wav`](intro.wav) holds the sound. The screen plays the pair as `video/intro.rgb` and `audio/intro.wav`, as [`media.md`](../media.md) gives.

## The film

The scripts in [`blender/`](blender/) build the machine from the [playfield drawing](../playfield/), animate the ride and render it. A script that runs inside Blender reads the folder it lies in from `BUILD`.

| Step | Script | Runs in | Makes |
|---|---|---|---|
| 1 | `extract.py <out>` | Python | `blender/out/`: the drawing's shapes and its textures |
| 2 | `frame_body.py <out>` | Python | the diorama's frame without the parts built in 3D, and the crown's outline |
| 3 | `display.py <out>` | Python | `display/`: what the diorama's screen shows in each frame |
| 4 | `scene.py` | Blender | the machine |
| 5 | `film.py` | Blender | the ride, the lights, the logo and the render settings |
| 6 | `render_frames.py` | Blender, with `FIRST` and `LAST` | `frames/`: the film in Cycles, at 640 × 480 |
| 7 | `convert.py <frames> <out>` | Python | `intro.rgb`, and `preview.gif` to look at |

`preview.py` sets up cameras for checking the modeled parts.

## The sound

[`soundtrack.py`](blender/soundtrack.py) mixes `intro.wav` from the sources in [`sounds/`](sounds/) and from the balls' paths that [`paths.py`](blender/paths.py) writes in Blender to `blender/out/ball-paths.json`. The sound scripts run in WSL.

| Source | Made by |
|---|---|
| `acestep-circusmarch-sft.wav`, the circus march | `march.py`, with ACE-Step 1.5 |
| `multiball-am_michael.wav`, the announcer's call | `voice.py` with Kokoro, then `announcer.py` |
| `sfx/`, the bumper, the kickout and the riser | `sfx.py`, with Stable Audio Open Small |
| `roll-recording.wav`, `flipper-recording.wav`, `clack-recording.wav` | recordings of the kit: a steel ball rolling on plywood, a flipper, two steel balls knocked together |

[`roll.py`](blender/roll.py) lays a ball's roll out of the rolling recording, following the ball's speed. Everything else, from the chimes to the logo's kick, is synthesized in `soundtrack.py`.
