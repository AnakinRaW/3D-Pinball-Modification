# Trailer

The machine's trailer: one game on the machine that [`cgi/`](../cgi/) builds, cut down to the features that are new in [`features.md`](../../docs/features.md). It runs from the power-up through the last drain to the display and the logo, at 25 frames a second in 1920 × 1080.

## The film

The scripts in [`blender/`](blender/) play the game on the machine, film it and render it. A script that runs inside Blender reads the folder it lies in from `BUILD`.

| Step | Script | Runs in | Makes |
|---|---|---|---|
| 1 | the steps of [`cgi/`](../cgi/) | Python and Blender | the machine and its lights at rest |
| 2 | `solve.py` | Blender | [`game.json`](blender/game.json): the parameters of each segment of the game, searched with `game.py`, `physics.py` and `runs.py` |
| 3 | `trailer.py` | Blender | `trailer.blend`: the game played again from `game.json`, its shots, the lights' keys, the callouts and the logo, and `blender/out/display.json` |
| 4 | `display.py` | Python | `display/`: what the diorama's screen shows in each frame |
| 5 | `render_frames.py` of `cgi/`, with `BUILD` set to `blender/` and `FRAMES_DIR` to `renders/<name>` | Blender, with `FIRST` and `LAST` | `renders/<name>/`: the film's frames |

The game is chaotic, so a run on a changed machine comes out differently. Step 2 runs again after every change to the machine, to `game.py`, to `physics.py` or to `runs.py`.
