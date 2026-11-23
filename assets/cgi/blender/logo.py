"""The logo that both films end on, far behind the machine and facing -y, before a black backdrop.

A trail of light rises and writes PINBALL in red neon tubes. A flipper hits a steel ball along a gold line, and
ANAKINRAW lights letter by letter as the ball passes. build() keys it on the intro's plan of 175 frames, from 126 to
175, and a film moves the keys onto its frames. Positions are Blender units in the world. The layout's are pixels
of a 320 x 240 picture, 10 to a unit, with y running down.
"""
import bpy, bmesh, math
from mathutils import Vector
from common import coll, fcurves_of, hexcol, set_interp


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


def build():
    """Builds the logo and its camera on the intro's plan, frames 126 to 175, and returns the camera."""
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
    # a steel ball: dark polished metal that mirrors the neon, with a faint glow so it reads on black
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
