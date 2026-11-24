"""Writes where everything the sound follows stands in every frame of the film, for soundtrack.py.

Run inside Blender after scene.py and film.py, with BUILD set to this folder. out/ball-paths.json holds the frame
rate and, keyed by frame, the world position of the camera ball (cam), the other ball (other), the logo's ball
(logo) and the logo's writing light (tip), the writing light's scale (tip_scale) and how far the logo's gold line
has drawn out (line, 0 to 1). The camera ball's path is taken without the camera's shake and the collision's
jolt, which move the view and not the ball.
"""
import bpy, json, os

sc = bpy.context.scene
keep = sc.frame_current
cam, other = bpy.data.objects["film-cam"], bpy.data.objects["ball-other"]
shake = [m for fc in (cam.animation_data.action.layers[0].strips[0].channelbag(cam.animation_data.action.slots[0]).fcurves
                      if cam.animation_data.action.layers else cam.animation_data.action.fcurves) for m in fc.modifiers]
for m in shake:
    m.mute = True
logo, tip, line = bpy.data.objects["logo-ball"], bpy.data.objects["logo-tip"], bpy.data.objects["logo-line"]
d = {"fps": sc.render.fps, "cam": {}, "other": {}, "logo": {}, "tip": {}, "tip_scale": {}, "line": {}}
for f in range(sc.frame_start, sc.frame_end + 1):
    sc.frame_set(f)
    k = str(f)
    for key, o in (("cam", cam), ("other", other), ("logo", logo), ("tip", tip)):
        d[key][k] = list(o.matrix_world.translation)
    d["tip_scale"][k] = tip.matrix_world.to_scale()[0]
    d["line"][k] = line.data.bevel_factor_end
for m in shake:
    m.mute = False
sc.frame_set(keep)
json.dump(d, open(os.path.join(BUILD, "out", "ball-paths.json"), "w"))
result = {"frames": sc.frame_end - sc.frame_start + 1}
