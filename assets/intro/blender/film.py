"""Animates the intro film on the machine and the lights that scene.py built, and ends it on the logo of logo.py.

Run inside Blender after scene.py, with BUILD set to this folder. scene.py, lights.py and logo.py lie in
../../cgi/blender, which both films share. 25 frames a second, frames 1 to 260:
  1-34    fade in, the camera ball lies in the scoop and looks at the bumpers; at 38 the other ball hits bumper 2
  50-71   the scoop throws the camera ball out, it hits bumper 3, the ambient light flashes gold
  71-97   it rolls on, looking back at the bumpers and ramps; at 97 the other ball hits it from behind
  97-160  it watches the other ball roll away, turns round and rolls between the flippers into the drain while the
          player flips in a panic; fade to black
  160-167 black
  167-199 a light trail rises from the drain and writes PINBALL
  199-235 the flipper hits the ball, it rolls along the gold line and ANAKINRAW lights letter by letter
  235-260 the logo stands while the sound's last hit rings out, then fades to black
The keys are written on a 175-frame plan and then stretched onto these, so the end of the ride and the logo take
their time; the stretch grows after the collision, so the camera ball keeps its speed through it.
Positions are in millimeters of the drawing, as in scene.py.
"""
import bpy, importlib, math, os, sys
from mathutils import Vector

CGI = os.path.join(os.path.dirname(os.path.dirname(BUILD)), "cgi", "blender")
if CGI not in sys.path: sys.path.insert(0, CGI)
import common, lights, logo
for _m in (common, lights, logo): importlib.reload(_m)
from common import coll, fcurves_of, set_interp

sc = bpy.context.scene
FPS = 25
# the film's keys are set on a 175-frame plan; this table stretches them, from the collision on, onto the film
RETIME = [(1, 1), (97, 97), (106, 108), (111, 124), (117, 140), (125, 160), (126, 167), (131, 175), (150, 199),
          (175, 235)]
HOLD = 25               # frames the finished logo stands and fades after the stretched plan
END = RETIME[-1][1] + HOLD


def retime(f):
    """A frame of the 175-frame plan on the film's timeline."""
    for (a0, b0), (a1, b1) in zip(RETIME, RETIME[1:]):
        if f <= a1:
            return b0 + (f - a0) * (b1 - b0) / (a1 - a0)
    return RETIME[-1][1] + f - RETIME[-1][0]


def remap_keys():
    """Moves every key the film set onto the stretched timeline; the flippers' keys are already on it."""
    blocks = [o for o in bpy.data.objects if not o.name.startswith("film-flipper-")]
    blocks += [m.node_tree for m in bpy.data.materials if m.node_tree] + list(bpy.data.materials)
    blocks += list(bpy.data.lights) + list(bpy.data.cameras) + list(bpy.data.curves)
    done = set()
    for block in blocks:
        ad = getattr(block, "animation_data", None)
        if not ad or not ad.action or ad.action.name in done:
            continue
        done.add(ad.action.name)
        for fc in fcurves_of(block):
            for kp in fc.keyframe_points:
                kp.co.x = retime(kp.co.x)
                kp.handle_left.x = retime(kp.handle_left.x); kp.handle_right.x = retime(kp.handle_right.x)
            fc.update()
RB = 4.5
BUMPER = {1: (69.634, 55.664), 2: (104.979, 74.788), 3: (71.218, 95.647)}
CONTACT = 16.0  # the ball meets the edge of F3 and the dish from outside, clear of the cap when the coil pulls it down
SCOOP = (31.87, 123.66)
DRAIN = (108.54, 387.04)


def mm(x, y, z=0.0):
    return Vector((x / 10, -y / 10, z / 10))


def key_path(obj, keys):
    """keys: [(frame, (x, y, z) in mm)]"""
    for f, p in keys:
        obj.location = mm(*p); obj.keyframe_insert("location", frame=f)


def clear_film():
    # parts a film empty carries go back to the machine first
    tilt = bpy.data.objects.get("machine-tilt")
    for o in bpy.data.objects:
        if o.parent and o.parent.name.startswith("film-") and not o.name.startswith("film-"):
            o.parent = tilt; o.matrix_parent_inverse.identity()
    for name in [o.name for o in bpy.data.objects if o.name.startswith(("preview-", "film-", "logo-"))]:
        bpy.data.objects.remove(bpy.data.objects[name], do_unlink=True)
    for c in [c for c in bpy.data.collections if c.name in ("film", "logo")]:
        bpy.data.collections.remove(c)
    for block in (bpy.data.cameras, bpy.data.lights, bpy.data.curves):
        for b in list(block):
            if b.users == 0: block.remove(b)
    sc.timeline_markers.clear()


# ---------------------------------------------------------------- the ride
def contact(k, normal_deg):
    """The ball's center where it touches bumper k, on the side the angle (degrees, drawing frame) faces."""
    bx, by_ = BUMPER[k]; a = math.radians(normal_deg); d = CONTACT
    return (bx + d * math.cos(a), by_ + d * math.sin(a))


def ride():
    c = coll("film")
    hit2 = contact(2, 80.0)          # the other ball comes up from below, between bumper 3 and the blue ramp's entrance
    hit3 = contact(3, 116.8)         # the camera ball hits bumper 3 on its lower left side
    # the balls meet at a glancing angle: the other ball comes in from the right, 60 degrees off the camera's course
    meet_cam = (100.0, 200.0)
    head = Vector((meet_cam[0] - hit3[0], meet_cam[1] - hit3[1])).normalized()
    right = Vector((-head.y, head.x)) if -head.y > 0 else Vector((head.y, -head.x))
    n = (head * math.cos(math.radians(60)) + right * math.sin(math.radians(60))).normalized()
    meet_other = (meet_cam[0] + 9.0 * n.x, meet_cam[1] + 9.0 * n.y)

    other = bpy.data.objects["ball-other"]
    other.animation_data_clear()
    # it keeps clear of the blue ramp's entrance lane, the seal and the crocodile: in past the seal's right side,
    # off bumper 2 nearly straight back down, along the gap between the lane's mouth and the seal, then round
    # the seal's right side and down past the crocodile; it comes back up from behind the camera ball, clips it
    # and rolls on up the playfield, into the view of the camera that looks back
    key_path(other, [(1, (158.0, 200.0, RB)), (14, (142.0, 160.0, RB)), (21, (121.0, 150.0, RB)), (38, (*hit2, RB)),
                     (49, (112.0, 122.0, RB)), (61, (117.0, 153.0, RB)), (69, (140.0, 153.0, RB)),
                     (78, (155.0, 195.0, RB)), (84, (148.0, 232.0, RB)), (97, (*meet_other, RB)),
                     (112, (96.0, 155.0, RB)), (125, (90.0, 128.0, RB)), (175, (90.0, 128.0, RB))])
    set_interp(other, "LINEAR")

    ball = bpy.data.objects["ball-camera"]
    ball.hide_render = True; ball.hide_viewport = True

    cam_d = bpy.data.cameras.new("film-cam"); cam_d.lens = 16; cam_d.clip_start = 0.02; cam_d.clip_end = 400
    cam_d.dof.use_dof = True; cam_d.dof.aperture_fstop = 8.0
    cam = bpy.data.objects.new("film-cam", cam_d); c.objects.link(cam)
    path = [(1, (SCOOP[0], SCOOP[1], 3.0)), (50, (SCOOP[0], SCOOP[1], 3.0)), (54, (SCOOP[0] + 0.4, SCOOP[1] - 0.2, 5.2)),
            (58, (SCOOP[0] + 1.5, SCOOP[1] - 0.6, RB)), (71, (*hit3, RB)), (73, (hit3[0] + 0.4, hit3[1] + 1.4, RB + 0.6)),
            # it keeps an even pace, 3 to 4 mm a frame on the stretched timeline; the collision throws it about 45
            # degrees to the left, away from the other ball, and it curves back to the drain
            (97, (*meet_cam, RB)), (106, (82.0, 240.0, RB)), (111, (88.0, 295.0, RB)), (117, (106.5, 350.0, RB)),
            (121, (DRAIN[0], 384.0, RB)), (123, (DRAIN[0], 400.0, RB)), (125, (DRAIN[0], 416.0, RB))]
    key_path(cam, path); set_interp(cam, "LINEAR")
    for f in (73, 106, 111):
        for fc in fcurves_of(cam):
            for kp in fc.keyframe_points:
                if int(kp.co.x) == f: kp.interpolation = "BEZIER"

    target = bpy.data.objects.new("film-target", None); c.objects.link(target)
    # after bumper 3 the camera rolls on down but looks back at the bumpers and the ramps; the other ball hits it
    # from behind, rolls away up the playfield in view, then the camera turns by the right toward the drain
    key_path(target, [(1, (98.0, 76.0, 9.0)), (50, (100.0, 78.0, 9.0)), (56, (75.0, 98.0, 10.0)), (71, (71.2, 96.0, 9.0)),
                      (77, (80.0, 72.0, 20.0)), (90, (95.0, 68.0, 24.0)), (97, (100.0, 110.0, 12.0)),
                      (101, (99.0, 150.0, 5.0)), (106, (95.0, 140.0, 5.0)), (108, (150.0, 290.0, 5.0)),
                      (111, (106.0, 380.0, 2.0)), (117, (DRAIN[0], 395.0, 3.0)), (121, (DRAIN[0], 410.0, RB)),
                      (125, (DRAIN[0], 430.0, RB))])
    set_interp(target, "BEZIER")
    tilt = bpy.data.objects["machine-tilt"]
    cam.parent = tilt; target.parent = tilt
    tc = cam.constraints.new("TRACK_TO"); tc.target = target; tc.track_axis = "TRACK_NEGATIVE_Z"; tc.up_axis = "UP_Y"
    cam_d.dof.focus_object = target

    # the hits: the coil pulls the whole bumper down until its dish almost rests on the board, the spring
    # sends it back up
    tilt = bpy.data.objects["machine-tilt"]
    for k, t0 in ((2, 38), (3, 71)):
        coil = bpy.data.objects.new(f"film-bumper-{k}-coil", None); c.objects.link(coil); coil.parent = tilt
        for o in list(bpy.data.objects):
            if o.name.startswith(f"bumper-{k}-") and not any(w in o.name for w in ("foil", "cutter", "well")):
                o.parent = coil
        for f, z in ((1, 0.0), (t0, 0.0), (t0 + 1, -4.0), (t0 + 3, -4.0), (t0 + 7, 0.0)):
            coil.location = (0.0, 0.0, z / 10); coil.keyframe_insert("location", index=2, frame=f)
        set_interp(coil, "LINEAR")
    # the gold flash of the ambient LEDs as the camera ball hits bumper 3
    gold = lights.area("film-gold", mm(115, 220, 260), 0.0, (1.0, 0.72, 0.25), 40, mm(115, 220, 0), c)
    for f, v in ((1, 0.0), (70, 0.0), (72, 22000.0), (84, 0.0)):
        gold.data.energy = v; gold.data.keyframe_insert("energy", frame=f)

    flippers(c, tilt)
    events()
    display_screen()
    return cam


def display_screen():
    """The diorama's display plays the frames display.py draws, glowing behind its glass."""
    folder = os.path.join(os.path.dirname(BUILD), "display")
    m = bpy.data.materials.new("film-display"); m.use_nodes = True; nt = m.node_tree
    tex = nt.nodes.new("ShaderNodeTexImage")
    img = bpy.data.images.load(os.path.join(folder, "d0001.png")); img.source = "SEQUENCE"
    tex.image = img
    # display.py draws the frames of the stretched plan; the logo's hold does not show the diorama
    tex.image_user.frame_duration = RETIME[-1][1]; tex.image_user.frame_start = 1; tex.image_user.frame_offset = 0
    tex.image_user.use_auto_refresh = True
    b = nt.nodes["Principled BSDF"]
    nt.links.new(tex.outputs["Color"], b.inputs["Base Color"]); nt.links.new(tex.outputs["Color"], b.inputs["Emission Color"])
    b.inputs["Emission Strength"].default_value = 2.5; b.inputs["Roughness"].default_value = 0.2
    bpy.data.objects["display-screen"].data.materials[0] = m


def flippers(c, tilt):
    """The player hammers the buttons in a panic as the ball drains and misses it: quick flips, left and right
    out of step, before the ball reaches the flippers and after it has passed between their tips. Each flipper
    turns about its pivot screw."""
    from mathutils import Matrix
    pivots = {"left": (74.77, 363.97, 1.0), "right": (142.305, 363.97, -1.0)}
    for side, (x, y, turn) in pivots.items():
        pivot = bpy.data.objects.new(f"film-flipper-{side}", None); c.objects.link(pivot)
        pivot.parent = tilt; pivot.location = mm(x, y)
        for o in bpy.data.objects:
            if o.type != "MESH" or not o.name.startswith(("flipper", "flipper-screw")) or o.parent != tilt: continue
            cx = sum(v.co.x for v in o.data.vertices) / max(1, len(o.data.vertices)) * 10
            if (cx < 108.0) == (side == "left"):
                o.parent = pivot; o.matrix_parent_inverse = Matrix.Translation(-pivot.location)
        # three flips a side on the film's timeline, out of step: up within a frame, held for one or two,
        # falling back over two; two while the ball comes down, one after it has passed between the tips
        flips = {"left": ((128, 2), (134, 1), (152, 2)), "right": ((130, 1), (136, 1), (155, 1))}[side]
        keys = {1: 0.0}
        for f, hold in flips:
            keys[f - 1] = 0.0; keys[f] = 48.0; keys[f + hold] = 48.0; keys[min(f + hold + 2, 160)] = 0.0
        for f, a in sorted(keys.items()):
            pivot.rotation_euler = (0.0, 0.0, math.radians(a * turn)); pivot.keyframe_insert("rotation_euler", index=2, frame=f)
        set_interp(pivot, "LINEAR")


def events():
    """The machine's lights answer the film's events, as the firmware's lamps would. Each insert glows in the
    color lights.py gives it by its place on the playfield; the strips run through the palette."""
    # the strips chase through the colors and flare when the camera ball hits bumper 3
    lights.key_chase(range(1, 130, 9))
    lights.key_glow("perimeter", [(1, 6.0), (70, 6.0), (72, 30.0), (76, 15.0), (86, 6.0)])
    lights.key_ceiling([(1, 1.0), (70, 1.0), (72, 3.0), (76, 2.0), (86, 1.0)])
    # the hits: the bumper's base and ring glow and fade
    for k, t0 in ((2, 38), (3, 71)):
        for part, peak in (("base", 10.0), ("ring", 20.0)):
            lights.key_glow(f"bumper-{k}-{part}-lit", [(1, 0.0), (t0, 0.0), (t0 + 1, peak), (t0 + 10, 0.0)])
    # the diffusers flash at each hit and at the collision, in a rainbow from the top of the playfield down
    lights.key_glow("diffuser-lit", [(1, 6.0), (37, 6.0), (39, 25.0), (46, 6.0), (70, 6.0), (72, 30.0), (82, 6.0),
                                     (96, 6.0), (98, 25.0), (106, 6.0)])
    # the score stars flare when the camera ball scores at bumper 3 and settle as it rolls on, left to right
    lights.key_glow("star-lit", [(1, 5.0), (71, 5.0), (74, 18.0), (100, 8.0), (112, 5.0)])
    # the track arrows blink while the camera ball is thrown out of the scoop
    lights.key_glow("arrow-lit", [(1, 5.0), (52, 5.0), (54, 16.0), (57, 5.0), (60, 16.0), (63, 5.0), (66, 16.0),
                                  (69, 5.0)])
    # the judgment light at the drain flares red as the ball drains
    lights.key_glow("diffuser-drain", [(1, 8.0), (113, 8.0), (119, 30.0), (125, 8.0)])
    lights.key_color("diffuser-drain", [(1, None), (113, "#ff6a4a"), (125, None)])


def shake(cam):
    """A slight hand-held shake while the ball rolls: the look-at point wanders by about 1 mm, the camera by 0.3 mm.
    The collision jolts it hard: the look-at point swings by up to 8 mm, about 5 degrees of view, the camera by
    1.5 mm, and both ring out over 12 frames.

    Noise modifiers on the location curves, on the stretched timeline, from the kick to the fade before the logo;
    the jolt is a sine added on each axis, out of step from axis to axis, whose influence falls to nothing."""
    target = bpy.data.objects["film-target"]
    for obj, strength, scale, phase in ((target, 0.25, 7.0, 1.0), (cam, 0.06, 4.0, 41.0)):
        for fc in fcurves_of(obj):
            if fc.data_path != "location":
                continue
            m = fc.modifiers.new("NOISE")
            m.strength = strength; m.scale = scale; m.phase = phase + 13.0 * fc.array_index
            m.use_restricted_range = True; m.frame_start = 54; m.frame_end = 160; m.blend_in = 4; m.blend_out = 4
    hit = round(retime(97))
    for obj, amp in ((target, 0.8), (cam, 0.15)):
        for fc in fcurves_of(obj):
            if fc.data_path != "location":
                continue
            m = fc.modifiers.new("FNGENERATOR")
            m.function_type = "SIN"; m.use_additive = True
            w = 2 * math.pi / (4.0 + fc.array_index)              # a swing every 4 to 6 frames
            m.amplitude = amp * (1.0, 0.6, 0.8)[fc.array_index]
            m.phase_multiplier = w; m.phase_offset = -w * hit
            m.use_restricted_range = True; m.frame_start = hit; m.frame_end = hit + 12
            m.blend_in = 0.0; m.blend_out = 11.0


def build_film():
    clear_film()
    film_cam = ride()
    logo_cam = logo.build()
    remap_keys()
    shake(film_cam)
    sc.frame_start, sc.frame_end = 1, END
    sc.render.fps = FPS
    sc.camera = film_cam
    m1 = sc.timeline_markers.new("ride", frame=1); m1.camera = film_cam
    m2 = sc.timeline_markers.new("logo", frame=round(retime(126))); m2.camera = logo_cam
    lights.look()
    vs = sc.view_settings
    vs.exposure = 0.0
    # fade in, fade out over the drain, black, then the logo, which fades out over the hold's last 15 frames
    for f, v in ((1, -9.0), (34, 0.0), (round(retime(120)), 0.0), (160, -9.0), (166, -9.0), (167, 0.0),
                 (END - 15, 0.0), (END, -9.0)):
        vs.exposure = v; vs.keyframe_insert("exposure", frame=f)
    r = sc.render
    r.engine = "CYCLES"; sc.cycles.samples = 48; sc.cycles.use_denoising = True
    r.resolution_x, r.resolution_y, r.resolution_percentage = 640, 480, 100
    r.use_motion_blur = True; r.motion_blur_shutter = 0.3
    r.image_settings.file_format = "PNG"
    r.filepath = os.path.join(os.path.dirname(BUILD), "frames", "f")
    return {"frames": [sc.frame_start, sc.frame_end], "objects": len(bpy.data.objects)}


result = build_film()
