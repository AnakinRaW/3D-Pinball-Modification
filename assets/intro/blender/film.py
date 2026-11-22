"""Animates the intro film on the parts scene.py built, and builds the logo it ends on.

Run inside Blender after scene.py, with BUILD set to this folder. 25 frames a second, frames 1 to 260:
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
import bpy, bmesh, math, os
from mathutils import Vector

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


def hexcol(h):
    h = h.lstrip("#"); srgb = [int(h[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    return tuple(c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4 for c in srgb) + (1.0,)


def key_path(obj, keys):
    """keys: [(frame, (x, y, z) in mm)]"""
    for f, p in keys:
        obj.location = mm(*p); obj.keyframe_insert("location", frame=f)


def key_value(socket_owner, path, keys, interp="BEZIER"):
    for f, v in keys:
        setattr(socket_owner, path, v); socket_owner.keyframe_insert(path, frame=f)


def fcurves_of(idblock):
    ad = idblock.animation_data
    if not ad or not ad.action: return []
    act = ad.action
    if hasattr(act, "layers") and act.layers:
        out = []
        for layer in act.layers:
            for strip in layer.strips:
                for slot in act.slots:
                    cb = strip.channelbag(slot)
                    if cb: out += list(cb.fcurves)
        return out
    return list(act.fcurves)


def set_interp(idblock, interp):
    for fc in fcurves_of(idblock):
        for kp in fc.keyframe_points: kp.interpolation = interp


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


def coll(name):
    c = bpy.data.collections.get(name) or bpy.data.collections.new(name)
    if c.name not in sc.collection.children: sc.collection.children.link(c)
    return c


# ---------------------------------------------------------------- the ride
def contact(k, normal_deg):
    """The ball's center where it touches bumper k, on the side the angle (degrees, drawing frame) faces."""
    bx, by_ = BUMPER[k]; a = math.radians(normal_deg); d = CONTACT
    return (bx + d * math.cos(a), by_ + d * math.sin(a))


def ride():
    c = coll("film")
    # bumpers 2 and 3 get materials of their own, so each can light up alone
    flash = {}
    for k in (2, 3):
        for part in ("base", "ring"):
            o = bpy.data.objects[f"bumper-{k}-{part}"]
            m = o.data.materials[0].copy(); m.name = f"bumper-{k}-{part}-lit"; o.data.materials[0] = m
            flash[(k, part)] = m.node_tree.nodes["Principled BSDF"].inputs["Emission Strength"]

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
    # the hits: the bumper's base and ring glow and fade
    for (k, part), sock in flash.items():
        t0 = 38 if k == 2 else 71
        peak = 20.0 if part == "ring" else 10.0
        for f, v in ((1, 0.0), (t0, 0.0), (t0 + 1, peak), (t0 + 10, 0.0)):
            sock.default_value = v; sock.keyframe_insert("default_value", frame=f)

    # lights: a warm key above the playfield, a cool rim from the top, the gold flash of the ambient LEDs
    def area(name, loc, energy, color, size, aim):
        d = bpy.data.lights.new(name, "AREA"); d.energy = energy; d.color = color; d.size = size
        o = bpy.data.objects.new(name, d); c.objects.link(o); o.location = loc
        o.rotation_euler = (Vector(aim) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
        return o
    # the room is dark: a dim key from the room's lamp, the rest of the light comes from the machine
    area("film-key", mm(115, 150, 420), 8000, (1.0, 0.82, 0.62), 30, mm(115, 200, 0))
    area("film-rim", mm(115, -60, 120), 500, (0.62, 0.74, 1.0), 12, mm(100, 120, 0))
    area("film-fill", mm(-40, 200, 60), 250, (1.0, 0.9, 0.8), 25, mm(80, 120, 0))
    gold = area("film-gold", mm(115, 220, 260), 0.0, (1.0, 0.72, 0.25), 40, mm(115, 220, 0))
    for f, v in ((1, 0.0), (70, 0.0), (72, 22000.0), (84, 0.0)):
        gold.data.energy = v; gold.data.keyframe_insert("energy", frame=f)
    # a soft warm fill from inside the diorama onto the playfield's back end
    area("film-backfill", mm(115, -4, 70), 6000, (1.0, 0.82, 0.55), 20, mm(115, 120, 0))
    ceiling_strip(c)
    # the perimeter strips light the playfield from its sides, warm on the left, cool on the right
    area("film-perimeter-left", mm(4, 200, 12), 2500, (1.0, 0.62, 0.3), 6, mm(60, 200, 0))
    area("film-perimeter-right", mm(226, 200, 12), 2500, (0.45, 0.6, 1.0), 6, mm(170, 200, 0))

    flippers(c, tilt)
    events()
    display_screen(c)
    return cam


def ceiling_strip(c):
    """The diorama's ceiling strip: 26 RGB LEDs 6.25 mm apart on a white tape 2.7 mm wide, glued under the red
    ceiling just behind the frame and shining down on the scene. As one ambient group with the perimeter strips,
    it runs through the palette with them and flares with them at bumper 3."""
    from mathutils import Matrix
    ceil = bpy.data.objects["diorama-ceiling"]
    z = min((ceil.matrix_world @ Vector(v)).z for v in ceil.bound_box) * 10       # mm, the ceiling's underside
    y, n, pitch = 9.0, 26, 6.25
    length = pitch * n
    x0 = 115.0 - pitch * (n - 1) / 2

    def material(name, color, strength):
        m = bpy.data.materials.new(name); m.use_nodes = True
        b = m.node_tree.nodes["Principled BSDF"]
        b.inputs["Base Color"].default_value = color; b.inputs["Roughness"].default_value = 0.4
        b.inputs["Emission Color"].default_value = color; b.inputs["Emission Strength"].default_value = strength
        return m

    def boxes(name, parts, mat):
        bm = bmesh.new()
        for loc, size in parts:
            bmesh.ops.create_cube(bm, size=1.0, matrix=Matrix.LocRotScale(mm(*loc), None, Vector(size) / 10))
        me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free(); me.materials.append(mat)
        o = bpy.data.objects.new(name, me); c.objects.link(o)
        return o

    tape = material("ceiling-tape", (0.8, 0.8, 0.78, 1.0), 0.0)
    boxes("film-ceiling-tape", [((115.0, y, z - 0.1), (length, 2.7, 0.2))], tape)
    led = material("ceiling-led", (1.0, 1.0, 1.0, 1.0), 25.0)
    boxes("film-ceiling-leds", [((x0 + i * pitch, y, z - 0.6), (2.0, 2.0, 0.8)) for i in range(n)], led)

    d = bpy.data.lights.new("film-ceiling-strip", "AREA")
    d.shape = "RECTANGLE"; d.size = length / 10; d.size_y = 0.27
    light = bpy.data.objects.new("film-ceiling-strip", d); c.objects.link(light)
    light.location = mm(115.0, y, z - 1.1)                       # an area light shines along its -Z, down
    sock = led.node_tree.nodes["Principled BSDF"].inputs["Emission Color"]
    for k, f in enumerate(range(1, 130, 9)):
        col = hexcol(PALETTE[k % len(PALETTE)])
        sock.default_value = col; sock.keyframe_insert("default_value", frame=f)
        d.color = col[:3]; d.keyframe_insert("color", frame=f)
    strength = led.node_tree.nodes["Principled BSDF"].inputs["Emission Strength"]
    for f, v in ((1, 1.0), (70, 1.0), (72, 3.0), (76, 2.0), (86, 1.0)):
        strength.default_value = 25.0 * v; strength.keyframe_insert("default_value", frame=f)
        d.energy = 3000.0 * v; d.keyframe_insert("energy", frame=f)


def display_screen(c):
    """The diorama's display plays the frames display.py draws: a glowing 4:3 screen inside its bezel. The
    bezel lies 87.6..147.6 mm across and 77.8..121.5 mm high in the back layer; the screen sits 3 mm inside it."""
    folder = os.path.join(os.path.dirname(BUILD), "display")
    x0, x1, z0, z1 = 92.4, 142.7, 80.8, 118.5
    y = -(-15.6) / 10                                    # just in front of the back layer at y -16
    me = bpy.data.meshes.new("film-display"); bm = bmesh.new()
    vs = [bm.verts.new((x / 10, y, z / 10)) for x, z in ((x0, z0), (x1, z0), (x1, z1), (x0, z1))]
    f = bm.faces.new(vs); uv = bm.loops.layers.uv.new()
    for loop, q in zip(f.loops, ((0, 0), (1, 0), (1, 1), (0, 1))):
        loop[uv].uv = q
    bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new("film-display", me); c.objects.link(o)
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
    me.materials.append(m)


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


def flash(material, keys, color=None):
    """Keys the emission of a material: keys is [(frame, strength)], color an optional hex at the flash."""
    sock = bpy.data.materials[material].node_tree.nodes["Principled BSDF"].inputs["Emission Strength"]
    for f, v in keys:
        sock.default_value = v; sock.keyframe_insert("default_value", frame=f)
    if color:
        col = bpy.data.materials[material].node_tree.nodes["Principled BSDF"].inputs["Emission Color"]
        base = tuple(col.default_value)
        for f, v in ((keys[0][0], base), (keys[1][0], hexcol(color)), (keys[-1][0], base)):
            col.default_value = v; col.keyframe_insert("default_value", frame=f)


# the colors of the RGB LEDs: red, orange, yellow, green, cyan, blue, magenta
PALETTE = ["#ff3048", "#ff8a1f", "#ffd21f", "#3dff6a", "#22e0ff", "#3a6bff", "#e040ff"]


def colorize(material, keys, order, shift=0):
    """Gives every part that uses the material a copy in a color of the palette, taken in the order order(x, y)
    sorts their centres (mm) into, and keys each copy's emission strength as flash() does."""
    base = bpy.data.materials[material]
    users = [o for o in bpy.data.objects if o.type == "MESH" and o.data.materials and o.data.materials[0]
             and (o.data.materials[0] == base or o.data.materials[0].name.startswith(material + "-c"))]
    def centre(o):
        vs = o.data.vertices; n = max(1, len(vs))
        return sum(v.co.x for v in vs) / n * 10, -sum(v.co.y for v in vs) / n * 10
    users.sort(key=lambda o: order(*centre(o)))
    for i, o in enumerate(users):
        old = o.data.materials[0]
        m = base.copy(); m.name = f"{material}-c{i}"; o.data.materials[0] = m
        if old is not base and old.users == 0: bpy.data.materials.remove(old)
        b = m.node_tree.nodes["Principled BSDF"]
        col = hexcol(PALETTE[(i + shift) % len(PALETTE)])
        b.inputs["Emission Color"].default_value = col
        b.inputs["Base Color"].default_value = tuple(0.35 + 0.65 * c for c in col[:3]) + (1.0,)
        sock = b.inputs["Emission Strength"]
        for f, v in keys:
            sock.default_value = v; sock.keyframe_insert("default_value", frame=f)


def events():
    """The machine's lights answer the film's events, as the firmware's lamps would. Each lamp takes a color from
    the palette by its place on the playfield; the perimeter strips run through the palette."""
    # the perimeter strips in the side plastic chase through the colors and flare when the camera ball hits bumper 3
    flash("perimeter", [(1, 6.0), (70, 6.0), (72, 30.0), (76, 15.0), (86, 6.0)])
    col = bpy.data.materials["perimeter"].node_tree.nodes["Principled BSDF"].inputs["Emission Color"]
    for k, f in enumerate(range(1, 130, 9)):
        col.default_value = hexcol(PALETTE[k % len(PALETTE)]); col.keyframe_insert("default_value", frame=f)
    # the diffusers flash at each hit and at the collision, in a rainbow from the top of the playfield down
    colorize("diffuser-lit", [(1, 3.0), (37, 3.0), (39, 20.0), (46, 3.0), (70, 3.0), (72, 25.0), (82, 3.0),
                              (96, 3.0), (98, 20.0), (106, 3.0)], lambda x, y: y)
    # the score stars light when the camera ball scores at bumper 3 and fade as it rolls on, left to right
    colorize("star-lit", [(1, 0.0), (71, 0.0), (74, 14.0), (100, 6.0), (112, 0.0)], lambda x, y: x, shift=2)
    # the track arrows blink while the camera ball is thrown out of the scoop
    colorize("arrow-lit", [(1, 2.0), (52, 2.0), (54, 14.0), (57, 2.0), (60, 14.0), (63, 2.0), (66, 14.0), (69, 2.0)],
             lambda x, y: x, shift=4)
    # the judgment light at the drain flares as the ball drains
    flash("diffuser-drain", [(1, 8.0), (113, 8.0), (119, 30.0), (125, 8.0)], color="#ff6a4a")


# ---------------------------------------------------------------- the logo
def arc(cx, cy, r, a0, a1, n=14):
    return [(cx + r * math.cos(math.radians(a0 + (a1 - a0) * i / n)), cy + r * math.sin(math.radians(a0 + (a1 - a0) * i / n))) for i in range(n + 1)]


# the tube letters, strokes in a box one unit high, y downward
GLYPH = {
    "P": [[(0, 1), (0, 0)] + arc(0.3, 0.25, 0.25, -90, 90) + [(0, 0.5)]],
    "I": [[(0, 0), (0, 1)]],
    "N": [[(0, 1), (0, 0), (0.58, 1), (0.58, 0)]],
    "B": [[(0, 1), (0, 0)] + arc(0.3, 0.25, 0.25, -90, 90) + [(0, 0.5)], [(0, 0.5)] + arc(0.33, 0.75, 0.25, -90, 90) + [(0, 1)]],
    "A": [[(0, 1), (0.3, 0), (0.6, 1)], [(0.15, 0.62), (0.45, 0.62)]],
    "L": [[(0, 0), (0, 1), (0.5, 1)]],
    "K": [[(0, 0), (0, 1)], [(0.5, 0), (0.04, 0.55), (0.5, 1)]],
    "R": [[(0, 1), (0, 0)] + arc(0.3, 0.25, 0.25, -90, 90) + [(0, 0.5)], [(0.25, 0.5), (0.55, 1)]],
    "W": [[(0, 0), (0.17, 1), (0.36, 0.38), (0.55, 1), (0.72, 0)]],
}
WIDTH = {"P": 0.55, "I": 0.0, "N": 0.58, "B": 0.58, "A": 0.6, "L": 0.5, "K": 0.5, "R": 0.55, "W": 0.72}
FLIPPER = [(-15.676, -7.49), (-15.576, -8.54), (-15.266, -9.56), (-14.756, -10.49), (-14.086, -11.31), (-13.276, -11.98),
           (-12.336, -12.48), (-11.326, -12.79), (-10.276, -12.89), (-9.216, -12.79), (-8.206, -12.48), (-7.266, -11.98),
           (-6.846, -11.66), (15.074, 7.79), (15.414, 8.14), (15.694, 8.55), (15.884, 9.01), (15.974, 9.48), (15.974, 9.98),
           (15.884, 10.46), (15.694, 10.92), (15.414, 11.33), (15.074, 11.68), (14.664, 11.95), (14.204, 12.14),
           (13.724, 12.23), (13.234, 12.23), (12.744, 12.14), (12.294, 11.95), (-12.816, -2.73), (-13.696, -3.32),
           (-14.446, -4.07), (-15.036, -4.95), (-15.436, -5.93), (-15.646, -6.96)]


def word(text, h, gap, xstretch=1.0, slant=0.0):
    letters, x = [], 0.0
    for ch in text:
        letters.append([[(x + (px * xstretch + (1 - py) * slant) * h, py * h) for px, py in s] for s in GLYPH[ch]])
        x += (WIDTH[ch] * xstretch + gap) * h
    return letters, x - gap * h


def chamfer(stroke, d):
    """Cuts every corner sharper than 60 degrees by the same 45 degree chamfer of length d."""
    pts = [stroke[0]]
    for i in range(1, len(stroke) - 1):
        a, b, c = stroke[i - 1], stroke[i], stroke[i + 1]
        t1 = math.atan2(b[1] - a[1], b[0] - a[0]); t2 = math.atan2(c[1] - b[1], c[0] - b[0])
        turn = abs((t2 - t1 + math.pi) % (2 * math.pi) - math.pi)
        if turn > math.radians(60):
            d1, d2 = math.dist(a, b), math.dist(b, c)
            pts.append((b[0] - (b[0] - a[0]) * d / d1, b[1] - (b[1] - a[1]) * d / d1))
            pts.append((b[0] + (c[0] - b[0]) * d / d2, b[1] + (c[1] - b[1]) * d / d2))
        else:
            pts.append(b)
    pts.append(stroke[-1])
    return pts


BALL_R = 0.85    # Blender units: the logo's ball, 8.5 px of the layout
LOGO_Y = 300.0  # Blender units: the logo stands far behind the machine, facing -y


def px(x, y, z=0.0):
    """A point of the 320 x 240 logo layout (y downward) in Blender units; 10 px are one unit."""
    return Vector(((x - 160) / 10, LOGO_Y - z / 10, -(y - 120) / 10))


def neon_material(name, color, strength):
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; n = nt.nodes
    for k in list(n): n.remove(k)
    out = n.new("ShaderNodeOutputMaterial"); em = n.new("ShaderNodeEmission")
    em.inputs["Color"].default_value = color; em.inputs["Strength"].default_value = strength
    nt.links.new(em.outputs[0], out.inputs["Surface"])
    return m, em


def halo_material(name, color, strength):
    """A soft glow around a tube: emission that fades toward the tube's silhouette."""
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; n = nt.nodes
    for k in list(n): n.remove(k)
    out = n.new("ShaderNodeOutputMaterial"); em = n.new("ShaderNodeEmission"); tr = n.new("ShaderNodeBsdfTransparent")
    lw = n.new("ShaderNodeLayerWeight"); inv = n.new("ShaderNodeMath"); inv.operation = "SUBTRACT"; inv.inputs[0].default_value = 1.0
    pw = n.new("ShaderNodeMath"); pw.operation = "POWER"; pw.inputs[1].default_value = 3.0
    mix = n.new("ShaderNodeMixShader")
    em.inputs["Color"].default_value = color; em.inputs["Strength"].default_value = strength
    nt.links.new(lw.outputs["Facing"], inv.inputs[1]); nt.links.new(inv.outputs[0], pw.inputs[0])
    nt.links.new(pw.outputs[0], mix.inputs[0]); nt.links.new(tr.outputs[0], mix.inputs[1]); nt.links.new(em.outputs[0], mix.inputs[2])
    nt.links.new(mix.outputs[0], out.inputs["Surface"])
    m.blend_method = "BLEND" if hasattr(m, "blend_method") else None
    return m, em


def tube(name, pts, radius, material, c):
    cu = bpy.data.curves.new(name, "CURVE"); cu.dimensions = "3D"; cu.bevel_depth = radius; cu.bevel_resolution = 4
    cu.use_fill_caps = True; cu.bevel_factor_mapping_end = "SPLINE"
    sp = cu.splines.new("POLY"); sp.points.add(len(pts) - 1)
    for p, v in zip(sp.points, pts): p.co = (*v, 1)
    o = bpy.data.objects.new(name, cu); c.objects.link(o); o.data.materials.append(material)
    return o


def write_on(objs, f0, f1):
    """Draws the tubes in order, each over its share of the frames by length."""
    lens = [sum((o.data.splines[0].points[i].co.xyz - o.data.splines[0].points[i - 1].co.xyz).length
                for i in range(1, len(o.data.splines[0].points))) for o in objs]
    total = sum(lens); acc = 0.0
    for o, L in zip(objs, lens):
        a = f0 + (f1 - f0) * acc / total; b = f0 + (f1 - f0) * (acc + L) / total; acc += L
        for f, v in ((f0 - 1, 0.0), (a, 0.0), (b, 1.0)):
            o.data.bevel_factor_end = v; o.data.keyframe_insert("bevel_factor_end", frame=f)
        set_interp(o.data, "LINEAR")
    return lens


def point_along(strokes, frac):
    total = sum(math.dist(a, b) for s in strokes for a, b in zip(s, s[1:])); left = total * frac
    for s in strokes:
        for a, b in zip(s, s[1:]):
            d = math.dist(a, b)
            if left <= d: t = left / max(d, 1e-9); return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)
            left -= d
    return strokes[-1][-1]


def logo():
    c = coll("logo")
    RED, GOLD, CREAM = hexcol("#d63a48"), hexcol("#e8c04a"), hexcol("#fff4d6")
    # PINBALL: upright, 1.3 times wide, every corner chamfered alike (variant 3)
    H_, GAP, XS = 42.0, 0.34, 1.3
    letters, right = word("PINBALL", H_, GAP, XS)
    dx, dy = (320 - right) / 2, 84.0
    strokes = [[(x + dx, y + dy) for x, y in chamfer(s, 0.16 * H_)] for L in letters for s in L]
    core_m, _ = neon_material("logo-red-core", (1.0, 0.55, 0.55, 1), 18.0)
    halo_m, _ = halo_material("logo-red-halo", RED, 6.0)
    cores = [tube("logo-pinball", [px(*p) for p in s], 0.12, core_m, c) for s in strokes]
    halos = [tube("logo-pinball-halo", [px(*p, 1.0) for p in s], 0.55, halo_m, c) for s in strokes]
    write_on(cores, 131, 150); write_on(halos, 131, 150)

    # the writing light: rises out of the drain below the frame, then draws the strokes
    tip_m, _ = neon_material("logo-tip", CREAM, 40.0)
    me = bpy.data.meshes.new("logo-tip")
    bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=24, v_segments=12, radius=0.28); bm.to_mesh(me); bm.free()
    tip = bpy.data.objects.new("logo-tip", me); c.objects.link(tip); me.materials.append(tip_m)
    start = strokes[0][0]
    for f, p in ((126, (160.0, 270.0)), (131, start)):
        tip.location = px(*p, -1.0); tip.keyframe_insert("location", frame=f)
    for f in range(132, 151):
        tip.location = px(*point_along(strokes, (f - 131) / 19), -1.0); tip.keyframe_insert("location", frame=f)
    for f, s in ((150, 1.0), (152, 0.0)):
        tip.scale = (s, s, s); tip.keyframe_insert("scale", frame=f)
    set_interp(tip, "LINEAR")

    # ANAKINRAW, small, spaced and slanted, dark until the ball passes
    small, rs = word("ANAKINRAW", 14.0, 0.55, 1.0, 0.15)
    dxs = (320 - rs - 0.15 * 14) / 2
    lx0, lx1, line_y = 64.0, 284.0, 168.0
    for i, L in enumerate(small):
        m, em = neon_material(f"logo-gold-{i}", GOLD, 0.0)
        hm, hem = halo_material(f"logo-gold-halo-{i}", GOLD, 0.0)
        lit = 157 + round((i + 1) / 9 * 13)
        for s in L:
            pts = [px(x + dxs, y + 50) for x, y in s]
            for o in (tube("logo-anakinraw", pts, 0.065, m, c), tube("logo-anakinraw-halo", [px(x + dxs, y + 50, 1.0) for x, y in s], 0.3, hm, c)):
                for f, hidden in ((1, True), (151, False)):
                    o.hide_render = hidden; o.keyframe_insert("hide_render", frame=f)
        for sock, peak, dim in ((em.inputs["Strength"], 14.0, 0.15), (hem.inputs["Strength"], 4.0, 0.0)):
            for f, v in ((150, 0.0), (152, dim), (lit - 1, dim), (lit + 1, peak * 1.6), (lit + 5, peak)):
                sock.default_value = v; sock.keyframe_insert("default_value", frame=f)

    # the gold line, drawn out from the flipper
    gm, _ = neon_material("logo-line", GOLD, 10.0)
    ghm, _ = halo_material("logo-line-halo", GOLD, 3.0)
    # it starts past the tip of the flipped flipper, which reaches to x 92
    ls = lx0 + 31
    line = [tube("logo-line", [px(ls, line_y), px(lx1, line_y)], 0.1, gm, c),
            tube("logo-line-halo", [px(ls, line_y, 1.0), px(lx1, line_y, 1.0)], 0.4, ghm, c)]
    for o in line:
        for f, v in ((150, 0.0), (151, 0.0), (155, 1.0)):
            o.data.bevel_factor_end = v; o.data.keyframe_insert("bevel_factor_end", frame=f)

    # the flipper of flipper.svg, black with a gold rim, its pivot screw at the origin
    s = 1.6 * 0.1
    pivot = (-10.276, -7.49)
    loc = [((x - pivot[0]) * s, 0.0, -(y - pivot[1]) * s) for x, y in FLIPPER]
    me = bpy.data.meshes.new("logo-flipper"); bm = bmesh.new()
    lo = [bm.verts.new((x, 0.15, z)) for x, _, z in loc]; hi = [bm.verts.new((x, -0.15, z)) for x, _, z in loc]
    bm.faces.new(hi); bm.faces.new(lo[::-1])
    for i in range(len(loc)):
        j = (i + 1) % len(loc); bm.faces.new((lo[i], lo[j], hi[j], hi[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces); bm.to_mesh(me); bm.free()
    flip = bpy.data.objects.new("logo-flipper", me); c.objects.link(flip)
    black = bpy.data.materials.new("logo-flipper-black"); black.use_nodes = True
    b = black.node_tree.nodes["Principled BSDF"]; b.inputs["Base Color"].default_value = (0.006, 0.006, 0.008, 1); b.inputs["Roughness"].default_value = 0.3
    me.materials.append(black)
    rim_m, _ = neon_material("logo-flipper-rim", GOLD, 8.0)
    rim = tube("logo-flipper-rim", [Vector(p) + Vector((0, -0.16, 0)) for p in loc + loc[:1]], 0.05, rim_m, c)
    rim.parent = flip
    flip.location = px(lx0 - 20, line_y - 10, 0.0)
    # image angles turn clockwise on the screen, which is a turn about +y for a logo that faces -y
    for f, a in ((150, 25.0), (155, 25.0), (156, -18.0), (175, -18.0)):
        flip.rotation_euler = (0.0, math.radians(a), 0.0); flip.keyframe_insert("rotation_euler", frame=f)
    for f, sc_ in ((150, 0.0), (152, 1.0)):
        flip.scale = (sc_, sc_, sc_); flip.keyframe_insert("scale", frame=f)

    # the ball: rolls from the flipper along the line and stops at its end
    bm_ = bpy.data.materials.new("logo-ball"); bm_.use_nodes = True
    # a steel ball: dark polished metal that mirrors the neon, with a faint glow of its own so it reads on black
    b = bm_.node_tree.nodes["Principled BSDF"]; b.inputs["Base Color"].default_value = (0.42, 0.44, 0.47, 1)
    b.inputs["Metallic"].default_value = 1.0; b.inputs["Roughness"].default_value = 0.12
    b.inputs["Emission Color"].default_value = (0.5, 0.52, 0.56, 1); b.inputs["Emission Strength"].default_value = 0.12
    me = bpy.data.meshes.new("logo-ball"); bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=32, v_segments=16, radius=BALL_R)
    bm.to_mesh(me); bm.free(); me.materials.append(bm_)
    for p in me.polygons: p.use_smooth = True
    ballo = bpy.data.objects.new("logo-ball", me); c.objects.link(ballo)
    halo_b, _ = halo_material("logo-ball-halo", (0.6, 0.62, 0.66, 1), 0.5)
    me2 = bpy.data.meshes.new("logo-ball-halo"); bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=32, v_segments=16, radius=BALL_R * 1.13)
    bm.to_mesh(me2); bm.free(); me2.materials.append(halo_b)
    for p in me2.polygons: p.use_smooth = True
    hb = bpy.data.objects.new("logo-ball-halo", me2); c.objects.link(hb); hb.parent = ballo
    # it starts against the flipper's tip and stops with its edge at the line's end
    x0, x1 = ls + BALL_R * 10 - 3.5, lx1 - BALL_R * 10
    for f, x in ((150, x0), (156, x0), (170, x1), (175, x1)):
        # it rests on the line, its lowest point just above the line's upper edge, and in front of the line's glow
        ballo.location = px(x, line_y - BALL_R * 10 - 1.4, 7.0); ballo.keyframe_insert("location", frame=f)
        ballo.rotation_euler = (0.0, (x - x0) / 10 / BALL_R, 0.0); ballo.keyframe_insert("rotation_euler", frame=f)
    for f, sc_ in ((150, 0.0), (152, 1.0)):
        ballo.scale = (sc_, sc_, sc_); ballo.keyframe_insert("scale", frame=f)
    for fc in fcurves_of(ballo):
        for kp in fc.keyframe_points:
            if int(kp.co.x) == 156: kp.interpolation = "LINEAR"
            if int(kp.co.x) == 170: kp.easing = "EASE_IN"

    # the camera faces the logo; a slow push in keeps it alive
    # a black backdrop behind the logo hides the room
    me = bpy.data.meshes.new("logo-backdrop"); bm = bmesh.new()
    vs = [bm.verts.new((x, LOGO_Y + 20.0, z)) for x, z in ((-80, -60), (80, -60), (80, 60), (-80, 60))]
    bm.faces.new(vs); bm.to_mesh(me); bm.free()
    bd = bpy.data.objects.new("logo-backdrop", me); c.objects.link(bd)
    black_m = bpy.data.materials.new("logo-black"); black_m.use_nodes = True
    bb = black_m.node_tree.nodes["Principled BSDF"]; bb.inputs["Base Color"].default_value = (0.0, 0.0, 0.0, 1); bb.inputs["Roughness"].default_value = 1.0
    bb.inputs["Specular IOR Level"].default_value = 0.0
    me.materials.append(black_m)
    cd = bpy.data.cameras.new("logo-cam"); cd.lens = 50; cd.clip_start = 0.1; cd.clip_end = 200
    cam = bpy.data.objects.new("logo-cam", cd); c.objects.link(cam)
    cam.rotation_euler = (math.radians(90), 0.0, 0.0)
    for f, d, x in ((126, 47.5, -0.6), (175, 43.2, 0.3)):
        cam.location = (x, LOGO_Y - d, 0.0); cam.keyframe_insert("location", frame=f)
    # the logo and its black backdrop exist only for the logo's frames, so the room stays open during the ride
    for o in c.objects:
        if o.animation_data and o.animation_data.action and any(fc.data_path == "hide_render" for fc in fcurves_of(o)):
            continue
        for f, hidden in ((1, True), (126, False)):
            o.hide_render = hidden; o.keyframe_insert("hide_render", frame=f)
    return cam


def compositor():
    """Bloom on everything bright: the neon, the lit bumpers, the gold flash."""
    tree = bpy.data.node_groups.get("intro-comp") or bpy.data.node_groups.new("intro-comp", "CompositorNodeTree")
    for n in list(tree.nodes): tree.nodes.remove(n)
    if not tree.interface.items_tree:
        tree.interface.new_socket("Image", in_out="OUTPUT", socket_type="NodeSocketColor")
    rl = tree.nodes.new("CompositorNodeRLayers")
    gl = tree.nodes.new("CompositorNodeGlare")
    gl.inputs["Type"].default_value = "Bloom"
    gl.inputs["Quality"].default_value = "High"
    gl.inputs["Threshold"].default_value = 1.0
    gl.inputs["Strength"].default_value = 0.6
    gl.inputs["Size"].default_value = 0.6
    out = tree.nodes.new("NodeGroupOutput")
    tree.links.new(rl.outputs["Image"], gl.inputs["Image"]); tree.links.new(gl.outputs["Image"], out.inputs[0])
    sc.compositing_node_group = tree
    sc.render.use_compositing = True


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
    logo_cam = logo()
    remap_keys()
    shake(film_cam)
    sc.frame_start, sc.frame_end = 1, END
    sc.render.fps = FPS
    sc.camera = film_cam
    m1 = sc.timeline_markers.new("ride", frame=1); m1.camera = film_cam
    m2 = sc.timeline_markers.new("logo", frame=round(retime(126))); m2.camera = logo_cam
    vs = sc.view_settings
    vs.view_transform = "AgX"
    try: vs.look = "AgX - Punchy"
    except Exception: pass
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
    compositor()
    return {"frames": [sc.frame_start, sc.frame_end], "objects": len(bpy.data.objects)}


result = build_film()
