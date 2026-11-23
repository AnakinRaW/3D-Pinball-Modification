"""The machine's lights and the room's, as scene.py builds them at rest, and the keys a film sets on them.

At rest each insert glows in a color of the palette. A point light of the same color sits just over it, as the
RGB LED under the insert throws its light onto the board around it. The perimeter strips in the side plastic and
the diorama's ceiling strip glow warm white. Two area lights carry the perimeter strips' light onto the playfield,
warm from the left and cool from the right. The drain's judgment light glows warm white. Each pop
bumper's base and ring get a separate copy of their material, so a film lights each bumper alone. The diorama's
display is a dark glass in its bezel until a film puts a picture on it.

The room is dark. A dim key comes from the room's lamp, a cool rim from the top, a warm fill from the left, and a
warm fill from inside the diorama onto the playfield's back end.

Positions are millimeters of the drawing, as in scene.py.
"""
import re
import bpy, bmesh
from mathutils import Matrix, Vector
from common import coll, hexcol

# the colors of the RGB LEDs: red, orange, yellow, green, cyan, blue, magenta
PALETTE = ["#ff3048", "#ff8a1f", "#ffd21f", "#3dff6a", "#22e0ff", "#3a6bff", "#e040ff"]
INSERT_LIGHT = 0.3      # W of an insert's point light for each unit of the insert's emission strength
# the inserts' materials as scene.py names them: the axis along which the inserts take the palette's colors in
# turn, the offset into the palette, and the emission strength at rest
INSERTS = {"diffuser-lit": ("y", 0, 6.0), "star-lit": ("x", 2, 5.0), "arrow-lit": ("x", 4, 5.0)}
PERIMETER = 6.0         # the perimeter strips' emission strength at rest
DRAIN = 8.0             # the drain's judgment light's emission strength at rest
# the ceiling strip: 26 RGB LEDs 6.25 mm apart on a white tape 2.7 mm wide, 9 mm behind the frame
CEILING_N, CEILING_PITCH, CEILING_TAPE, CEILING_Y = 26, 6.25, 2.7, 9.0
CEILING_LED, CEILING_W = 25.0, 3000.0       # the LEDs' emission strength and the strip's light in W at rest
# the display's glass, 3 mm inside the bezel, which lies 87.6 to 147.6 mm across and 77.8 to 121.5 mm high in the
# back layer: x0, x1, z0, z1, and y just in front of the back layer at -16 mm
DISPLAY = (92.4, 142.7, 80.8, 118.5, -15.6)


def mm(x, y, z=0.0):
    return Vector((x / 10, -y / 10, z / 10))


def area(name, loc, energy, color, size, aim, c):
    """An area light at loc, shining toward aim."""
    d = bpy.data.lights.new(name, "AREA"); d.energy = energy; d.color = color; d.size = size
    o = bpy.data.objects.new(name, d); c.objects.link(o); o.location = loc
    o.rotation_euler = (Vector(aim) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
    return o


def principled(material):
    return bpy.data.materials[material].node_tree.nodes["Principled BSDF"]


# ---------------------------------------------------------------- at rest
def build():
    """Builds the lights at rest. scene.py calls it once the machine stands, tilted, with its backbox."""
    room()
    inserts()
    strips()
    bumpers()
    principled("diffuser-drain").inputs["Emission Strength"].default_value = DRAIN
    display()


def room():
    c = coll("room")
    area("room-key", mm(115, 150, 420), 8000, (1.0, 0.82, 0.62), 30, mm(115, 200, 0), c)
    area("room-rim", mm(115, -60, 120), 500, (0.62, 0.74, 1.0), 12, mm(100, 120, 0), c)
    area("room-fill", mm(-40, 200, 60), 250, (1.0, 0.9, 0.8), 25, mm(80, 120, 0), c)
    area("room-backfill", mm(115, -4, 70), 6000, (1.0, 0.82, 0.55), 20, mm(115, 120, 0), c)


def centre(o):
    """The middle of a part's mesh in millimeters of the drawing, (x, y)."""
    vs = o.data.vertices; n = max(1, len(vs))
    return sum(v.co.x for v in vs) / n * 10, -sum(v.co.y for v in vs) / n * 10


def inserts():
    """Gives every insert a copy of its material in a color of the palette, and a point light of that color 1.5 mm
    over it."""
    tilt = bpy.data.objects["machine-tilt"]
    c = coll("lights")
    for material, (axis, shift, strength) in INSERTS.items():
        base = bpy.data.materials[material]
        users = [o for o in bpy.data.objects if o.type == "MESH" and o.data.materials and o.data.materials[0] == base]
        users.sort(key=lambda o: centre(o)["xy".index(axis)])
        for i, o in enumerate(users):
            m = base.copy(); m.name = f"{material}-c{i}"; o.data.materials[0] = m
            b = m.node_tree.nodes["Principled BSDF"]
            col = hexcol(PALETTE[(i + shift) % len(PALETTE)])
            b.inputs["Emission Color"].default_value = col
            b.inputs["Base Color"].default_value = tuple(0.35 + 0.65 * v for v in col[:3]) + (1.0,)
            m.diffuse_color = b.inputs["Base Color"].default_value          # the solid view shows the insert's color
            b.inputs["Emission Strength"].default_value = strength
            d = bpy.data.lights.new(f"lamp-{material}-{i}", "POINT")
            d.color = col[:3]; d.shadow_soft_size = 0.3; d.energy = INSERT_LIGHT * strength
            light = bpy.data.objects.new(d.name, d); c.objects.link(light); light.parent = tilt
            light.location = mm(*centre(o), 1.5)


def strips():
    """The perimeter strips with the two area lights that carry their light onto the playfield, and the diorama's
    ceiling strip, glued under the red ceiling just behind the frame and shining down on the scene. The perimeter
    and the ceiling strip are one ambient group and glow in one color."""
    c = coll("lights")
    per = principled("perimeter")
    per.inputs["Emission Strength"].default_value = PERIMETER
    area("lamp-perimeter-left", mm(4, 200, 12), 2500, (1.0, 0.62, 0.3), 6, mm(60, 200, 0), c)
    area("lamp-perimeter-right", mm(226, 200, 12), 2500, (0.45, 0.6, 1.0), 6, mm(170, 200, 0), c)

    ceil = bpy.data.objects["diorama-ceiling"]
    z = min((ceil.matrix_world @ Vector(v)).z for v in ceil.bound_box) * 10       # mm, the ceiling's underside
    length = CEILING_PITCH * CEILING_N
    x0 = 115.0 - CEILING_PITCH * (CEILING_N - 1) / 2

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
    boxes("lamp-ceiling-tape", [((115.0, CEILING_Y, z - 0.1), (length, CEILING_TAPE, 0.2))], tape)
    led = material("ceiling-led", (1.0, 1.0, 1.0, 1.0), CEILING_LED)
    led.node_tree.nodes["Principled BSDF"].inputs["Emission Color"].default_value = per.inputs["Emission Color"].default_value
    boxes("lamp-ceiling-leds", [((x0 + i * CEILING_PITCH, CEILING_Y, z - 0.6), (2.0, 2.0, 0.8)) for i in range(CEILING_N)], led)

    d = bpy.data.lights.new("lamp-ceiling-strip", "AREA")
    d.shape = "RECTANGLE"; d.size = length / 10; d.size_y = CEILING_TAPE / 10
    d.color = per.inputs["Emission Color"].default_value[:3]; d.energy = CEILING_W
    light = bpy.data.objects.new("lamp-ceiling-strip", d); c.objects.link(light)
    light.location = mm(115.0, CEILING_Y, z - 1.1)                     # an area light shines along its -Z, down


def bumpers():
    for k in (1, 2, 3):
        for part in ("base", "ring"):
            o = bpy.data.objects[f"bumper-{k}-{part}"]
            m = o.data.materials[0].copy(); m.name = f"bumper-{k}-{part}-lit"; o.data.materials[0] = m


def display():
    """The diorama's display: a dark glass, with the UV map that a film's picture takes."""
    x0, x1, z0, z1, y = DISPLAY
    me = bpy.data.meshes.new("display-screen"); bm = bmesh.new()
    f = bm.faces.new([bm.verts.new(mm(x, y, z)) for x, z in ((x0, z0), (x1, z0), (x1, z1), (x0, z1))])
    uv = bm.loops.layers.uv.new()
    for loop, q in zip(f.loops, ((0, 0), (1, 0), (1, 1), (0, 1))):
        loop[uv].uv = q
    bm.to_mesh(me); bm.free()
    m = bpy.data.materials.new("display-glass"); m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (0.01, 0.012, 0.02, 1.0); b.inputs["Roughness"].default_value = 0.1
    m.diffuse_color = (0.02, 0.03, 0.06, 1.0)
    me.materials.append(m)
    o = bpy.data.objects.new("display-screen", me); coll("lights").objects.link(o)


def display_centre():
    """The middle of the display's glass in the world."""
    x0, x1, z0, z1, y = DISPLAY
    return mm((x0 + x1) / 2, y, (z0 + z1) / 2)


def look():
    """AgX with its punchy look, and a bloom on everything bright: the lamps, the lit bumpers and the logo's neon."""
    sc = bpy.context.scene
    vs = sc.view_settings
    vs.view_transform = "AgX"
    try: vs.look = "AgX - Punchy"
    except Exception: pass
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


# ---------------------------------------------------------------- the keys a film sets
def key_glow(material, keys):
    """Keys a light's emission strength, keys [(frame, strength)]. An insert's material keys every copy, and the
    point light over each copy follows its strength."""
    copies = [m for m in bpy.data.materials if re.fullmatch(re.escape(material) + r"-c\d+", m.name)]
    for m in copies or [bpy.data.materials[material]]:
        sock = m.node_tree.nodes["Principled BSDF"].inputs["Emission Strength"]
        light = bpy.data.lights.get(f"lamp-{material}-{m.name.rsplit('-c', 1)[1]}") if copies else None
        for f, v in keys:
            sock.default_value = v; sock.keyframe_insert("default_value", frame=f)
            if light:
                light.energy = INSERT_LIGHT * v; light.keyframe_insert("energy", frame=f)


def key_color(material, keys):
    """Keys a light's emission color, keys [(frame, hex)], where None stands for its color at rest."""
    sock = principled(material).inputs["Emission Color"]
    rest = tuple(sock.default_value)
    for f, h in keys:
        sock.default_value = rest if h is None else hexcol(h); sock.keyframe_insert("default_value", frame=f)


def key_chase(frames, palette=None):
    """Runs the perimeter strips and the ceiling strip through a palette together, PALETTE unless another is
    given, to the next color at each of frames."""
    palette = palette or PALETTE
    socks = [principled(n).inputs["Emission Color"] for n in ("perimeter", "ceiling-led")]
    light = bpy.data.lights["lamp-ceiling-strip"]
    for k, f in enumerate(frames):
        col = hexcol(palette[k % len(palette)])
        for s in socks:
            s.default_value = col; s.keyframe_insert("default_value", frame=f)
        light.color = col[:3]; light.keyframe_insert("color", frame=f)


def key_ceiling(keys):
    """Keys the ceiling strip's brightness, keys [(frame, factor)] of its brightness at rest."""
    sock = principled("ceiling-led").inputs["Emission Strength"]
    light = bpy.data.lights["lamp-ceiling-strip"]
    for f, v in keys:
        sock.default_value = CEILING_LED * v; sock.keyframe_insert("default_value", frame=f)
        light.energy = CEILING_W * v; light.keyframe_insert("energy", frame=f)
