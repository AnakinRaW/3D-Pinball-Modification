# CGI

The machine in 3D that the [intro](../intro/) and the [trailer](../trailer/) are filmed on, with its lights, the room it stands in and the logo both films end on. The scripts in [`blender/`](blender/) build it from the [playfield drawing](../playfield/). Both films run them, so a change here reaches both. A script that runs inside Blender reads the folder it lies in from `BUILD`.

| Step | Script | Runs in | Makes |
|---|---|---|---|
| 1 | `extract.py <out>` | Python | `blender/out/`: the drawing's shapes and its textures |
| 2 | `frame_body.py <out>` | Python | the crown's outline and the CIRCUS board in the back of the diorama |
| 3 | `scene.py` | Blender | the machine and the room, with the lights of [`lights.py`](blender/lights.py) at rest, and what lies under the board, the apron and the slingshots' wooden covers: the scoop's kicker and solenoid, the ball store with its seesaw and servo, the IR sensors, the slingshots' clear spacers with their rubber rings |

A film's script runs after `scene.py`. It keys the lights with the functions of `lights.py` and builds the logo with [`logo.py`](blender/logo.py), which keys it on the intro's frames 126 to 175 for the film to move. [`render_frames.py`](blender/render_frames.py) renders a film's frames from a timer, with `BUILD` set to the film's `blender/` folder. `preview.py` sets up cameras for checking the modeled parts. `common.py` holds the helpers for colors, collections and keys.

[`export.py`](blender/export.py) runs in Blender after a change to the model and writes [`machine.glb`](machine.glb), the machine for the [Arcade](../../tools/arcade/)'s 3D view: one node for each placement of the drawing, carrying the placement's id as `drawing_id`, in meters with Y up.
