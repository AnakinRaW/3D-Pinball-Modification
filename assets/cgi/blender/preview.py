"""Preview cameras and a plain light for checking the modeled parts. Run inside Blender after scene.py.

VIEW names the camera: "overview" looks down the playfield from above the apron, "scoop" sits at ball
height in the scoop and looks at the bumpers, "drain" looks down the lane between the flippers.
"""
import bpy, math
from mathutils import Vector

sc = bpy.context.scene


def mm(x, y, z):
    return Vector((x / 10, -y / 10, z / 10))


def cam(name, loc, target, lens):
    d = bpy.data.cameras.get(name) or bpy.data.cameras.new(name)
    d.lens = lens; d.clip_start = 0.05; d.clip_end = 500
    o = bpy.data.objects.get(name) or bpy.data.objects.new(name, d)
    if o.name not in sc.collection.objects: sc.collection.objects.link(o)
    o.location = loc
    o.rotation_euler = (target - loc).to_track_quat("-Z", "Y").to_euler()
    return o


VIEWS = {
    "overview": (mm(115, 580, 380), mm(115, 200, 0), 32),
    "scoop": (mm(31.87, 123.66, 4.5), mm(105.0, 72.0, 8.0), 16),
    "drain": (mm(108.5, 300.0, 12.0), mm(108.5, 387.0, 0.0), 18),
}
loc, target, lens = VIEWS[VIEW]
sc.camera = cam("preview-" + VIEW, loc, target, lens)
if not bpy.data.objects.get("preview-sun"):
    l = bpy.data.lights.new("preview-sun", "SUN"); l.energy = 3.5; l.angle = 0.3
    o = bpy.data.objects.new("preview-sun", l); sc.collection.objects.link(o)
    o.rotation_euler = (math.radians(35), math.radians(15), math.radians(-30))
w = sc.world or bpy.data.worlds.new("world"); sc.world = w; w.use_nodes = True
w.node_tree.nodes["Background"].inputs[0].default_value = (0.05, 0.05, 0.06, 1)
sc.render.engine = "BLENDER_EEVEE"
sc.render.resolution_x, sc.render.resolution_y, sc.render.resolution_percentage = 960, 720, 100
sc.view_settings.view_transform = "AgX"
result = {"camera": sc.camera.name}
