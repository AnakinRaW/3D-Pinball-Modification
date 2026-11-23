"""Renders the film's frames one by one from a timer, so Blender stays reachable between frames.

Run inside Blender with BUILD set to the film's blender folder. The frames go to frames/ in the film's folder, or
to FRAMES_DIR there. A frame whose PNG exists is skipped, so a stopped run picks up where it ended. FIRST and LAST
limit the range. The run writes its progress to progress.txt beside the frames.
"""
import bpy, os, time

FRAMES = os.path.join(os.path.dirname(BUILD), globals().get("FRAMES_DIR", "frames"))
os.makedirs(FRAMES, exist_ok=True)
sc = bpy.context.scene
todo = [f for f in range(FIRST, LAST + 1) if not os.path.exists(os.path.join(FRAMES, f"f{f:04d}.png"))]
state = {"t0": time.time()}


def step():
    if not todo:
        open(os.path.join(FRAMES, "progress.txt"), "w").write(f"done {round(time.time() - state['t0'])} s\n")
        return None
    f = todo.pop(0)
    sc.frame_set(f)
    sc.render.filepath = os.path.join(FRAMES, f"f{f:04d}.png")
    bpy.ops.render.render(write_still=True)
    open(os.path.join(FRAMES, "progress.txt"), "w").write(f"frame {f}, {len(todo)} left\n")
    return 0.2


bpy.app.timers.register(step, first_interval=0.5)
result = {"queued": len(todo)}
