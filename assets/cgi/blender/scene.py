"""Builds the machine's parts in Blender from the playfield drawing.

Run inside Blender with BUILD set to this folder; extract.py has written its inputs to BUILD/out.
One Blender unit is 10 mm. The drawing's x runs to the right and its y down the playfield, so a point
(x, y) in millimeters lies at (x / 10, -y / 10). Positions and outlines come from the drawing. Heights are
estimates read off the photos of the kit's manual and are collected in H. Everything but the backbox
hangs from the empty "machine-tilt", which tips the playfield by 4 degrees toward the flippers. lights.py adds the
machine's lights and the room's, at rest.
"""
import bpy, bmesh, importlib, json, math, os, sys
from mathutils import Matrix, Vector

if BUILD not in sys.path: sys.path.insert(0, BUILD)
import common, lights
for _m in (common, lights): importlib.reload(_m)
from common import coll, hexcol

OUT = os.path.join(BUILD, "out")
DATA = json.load(open(os.path.join(OUT, "shapes.json")))
SHAPES = DATA["shapes"]
VB = DATA["viewBox"]
TILT_DEG = 4.0

# Heights in mm, estimated from the manual's photos.
H = {
    "wall_back": 61.0, "wall_front": 31.0,          # cabinet walls above the playfield at its back and front edge, off the side panels
    "wood_lo": 3.5, "plastic": 7.0, "wood_hi": 10.5, "cover": 13.5,   # the side stack, layer tops
    "post": 12.0, "sleeve_lo": 2.0, "sleeve_hi": 10.0, "rubber_z": 6.0,
    "fence": 25.0, "fence_end": 12.0, "blade": 20.0,
    "flipper": 8.0, "sling": 13.0, "rail": 10.0,
    "sling_lo": 10.0,                               # the slingshot's wooden cover, its underside 1 mm over the 9 mm ball
    "apron_lo": 10.5, "apron_hi": 13.5,             # the apron rests on the outlane walls' stack
    "bell_rim": 4.0, "bell_body": 12.0, "bell_top": 18.0, "foil": 0.03, "plate": 1.0,
    "ramp_wall": 9.0, "ramp_floor": 1.6,
    "gray_end": 30.0,                               # the gray lane's floor at its end under the blue ramp
    "shooter_top": 24.0, "shooter_wall": 30.0,      # the shooter lane's floor at its top end, its red wall
}
# The pop bumper after the manual: a flat metal dish on its spring, as wide as the cap, floating over a hole
# in the board that the coil pulls it into until the dish almost rests on the board. At rest the ball wedges
# between the foil on the board and the dish's slanted side. Then the cap of F3, the ribs F5/F6 and F4.
BUMPER = {"dish_lo": 5.0, "dish_hi": 7.5, "dish_r_lo": 8.0,
          "f3_hi": 9.3, "ribs_hi": 11.5, "f4_hi": 13.3, "ribs_r_lo": 10.0, "foil_r": 13.2}
RBALL = 4.5


def wall_top(y):
    """Height (mm) of the cabinet walls' top edge above the playfield at y: it falls faster than the playfield."""
    t = max(0.0, min(1.0, (y + 8.4) / 444.1))
    return H["wall_back"] + (H["wall_front"] - H["wall_back"]) * t


def W(x, y, z=0.0):
    return (x / 10.0, -y / 10.0, z / 10.0)


def mat(name, color, rough=0.45, metal=0.0, coat=0.0, alpha=1.0, transmission=0.0, emit=None, strength=0.0):
    m = bpy.data.materials.get(name)
    if m: return m
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = hexcol(color) if isinstance(color, str) else color
    b.inputs["Roughness"].default_value = rough; b.inputs["Metallic"].default_value = metal
    b.inputs["Coat Weight"].default_value = coat
    if transmission:
        b.inputs["Transmission Weight"].default_value = transmission; b.inputs["IOR"].default_value = 1.49
    if alpha < 1:
        b.inputs["Alpha"].default_value = alpha
    if emit:
        b.inputs["Emission Color"].default_value = hexcol(emit); b.inputs["Emission Strength"].default_value = strength
    m.diffuse_color = hexcol(color) if isinstance(color, str) else color
    return m


def link(obj, c, material=None):
    c.objects.link(obj)
    if material: obj.data.materials.append(material)
    return obj


def clean(pts):
    out = []
    for p in pts:
        if not out or (abs(p[0] - out[-1][0]) > 1e-3 or abs(p[1] - out[-1][1]) > 1e-3):
            out.append(tuple(p))
    if len(out) > 2 and abs(out[0][0] - out[-1][0]) < 1e-3 and abs(out[0][1] - out[-1][1]) < 1e-3:
        out.pop()
    return out


def prism(name, pts, z0, z1, c, material, zfn=None, lo_fn=None, hi_fn=None, cap_only=False):
    """Extrudes a closed outline (mm) from z0 to z1 (mm). zfn lifts both faces; lo_fn and hi_fn lift one."""
    lo_fn = lo_fn or zfn; hi_fn = hi_fn or zfn
    pts = clean(pts)
    area = sum(pts[i][0] * pts[i - 1][1] - pts[i - 1][0] * pts[i][1] for i in range(len(pts)))
    if area > 0: pts = pts[::-1]
    me = bpy.data.meshes.new(name); bm = bmesh.new()
    lo = [bm.verts.new(W(x, y, z0 + (lo_fn(x, y) if lo_fn else 0))) for x, y in pts]
    hi = [bm.verts.new(W(x, y, z1 + (hi_fn(x, y) if hi_fn else 0))) for x, y in pts]
    bm.faces.new(hi)
    if not cap_only: bm.faces.new(lo[::-1])
    if not cap_only:
        n = len(pts)
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new((lo[i], lo[j], hi[j], hi[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me); bm.free()
    return link(bpy.data.objects.new(name, me), c, material)


def surface(name, pts, zfn, thickness, c, material, cuts=1, step=None):
    """A ramp floor: the outline triangulated and finely subdivided, every vertex lifted to zfn, then given
    its thickness downward, so the floor follows its height between the outline's points as well. A step (mm)
    first sets a point on the outline at least that often, so a long floor follows a curved climb closely."""
    pts = clean(pts)
    if step:
        dense = []
        for (x0, y0), (x1, y1) in zip(pts, pts[1:] + pts[:1]):
            n = max(1, math.ceil(math.hypot(x1 - x0, y1 - y0) / step))
            dense += [(x0 + (x1 - x0) * k / n, y0 + (y1 - y0) * k / n) for k in range(n)]
        pts = dense
    me = bpy.data.meshes.new(name); bm = bmesh.new()
    vs = [bm.verts.new((x / 10, -y / 10, 0.0)) for x, y in pts]
    f = bm.faces.new(vs)
    bmesh.ops.triangulate(bm, faces=[f], quad_method="BEAUTY", ngon_method="BEAUTY")
    for _ in range(2):
        bmesh.ops.subdivide_edges(bm, edges=bm.edges[:], cuts=cuts, use_grid_fill=True)
    for v in bm.verts:
        v.co.z = zfn(v.co.x * 10, -v.co.y * 10) / 10
    bm.normal_update()
    for face in bm.faces:
        if face.normal.z < 0: face.normal_flip()
    # the thickness hangs straight down, so the underside follows the top without spikes at the edges
    ret = bmesh.ops.extrude_face_region(bm, geom=bm.faces[:], use_keep_orig=True)
    new = [g for g in ret["geom"] if isinstance(g, bmesh.types.BMVert)]
    bmesh.ops.translate(bm, verts=new, vec=(0.0, 0.0, -thickness / 10))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me); bm.free()
    if cuts > 1 or step:
        # a finely curved floor shades smooth, its edges stay sharp
        me.shade_smooth(); me.set_sharp_from_angle(angle=math.radians(30))
    o = link(bpy.data.objects.new(name, me), c, material)
    return o


def centerline(outline):
    """The middle line and width (mm) of a thin closed outline drawn around a wall. Such an outline runs along
    one side and back along the other, so point i faces point k - i for one offset k; the pairs are averaged."""
    p = outline; n = len(p)
    def spread(k):
        d = sorted(math.dist(p[i], p[(k - i) % n]) for i in range(0, n, max(1, n // 60)))
        return d[len(d) // 2]
    k = min(range(0, min(n, 40)), key=spread)
    step = max(1, n // 400)
    pairs = [(p[i % n], p[(k - i) % n]) for i in range(k // 2, (k + n) // 2 + 1, step)]
    widths = sorted(math.dist(a, b) for a, b in pairs)
    # pairs on a wall's end cap face each other along the cap, not across the wall, and would bend its end
    # aside; the line starts and stops where the pairs span the full width
    full = [j for j, (a, b) in enumerate(pairs) if math.dist(a, b) >= 0.75 * widths[len(widths) // 2]]
    if len(full) >= 2:
        pairs = pairs[full[0]:full[-1] + 1]
    mid = [((a[0] + b[0]) / 2, (a[1] + b[1]) / 2) for a, b in pairs]
    # the pairs wobble a little, which would tilt the wall's faces in turn; smooth, then space evenly
    k = 4
    smooth = [mid[0]] + [(sum(q[0] for q in mid[max(0, i - k):i + k + 1]) / len(mid[max(0, i - k):i + k + 1]),
                          sum(q[1] for q in mid[max(0, i - k):i + k + 1]) / len(mid[max(0, i - k):i + k + 1]))
                         for i in range(1, len(mid) - 1)] + [mid[-1]]
    acc = [0.0]
    for i in range(1, len(smooth)): acc.append(acc[-1] + math.dist(smooth[i - 1], smooth[i]))
    n_out = max(2, int(acc[-1] / 1.5))
    even, j = [], 1
    for m in range(n_out + 1):
        d = acc[-1] * m / n_out
        while j < len(acc) - 1 and acc[j] < d: j += 1
        t = (d - acc[j - 1]) / max(acc[j] - acc[j - 1], 1e-9)
        even.append((smooth[j - 1][0] + (smooth[j][0] - smooth[j - 1][0]) * t, smooth[j - 1][1] + (smooth[j][1] - smooth[j - 1][1]) * t))
    return even, widths[len(widths) // 2]


def offset_line(pts, d, closed):
    """Moves every vertex of a polyline sideways by d (mm), mitered; positive d goes to the left of travel."""
    n = len(pts); out = []
    for i in range(n):
        a = Vector(pts[i - 1] if (i > 0 or closed) else pts[i]); b = Vector(pts[i]); c2 = Vector(pts[(i + 1) % n] if (i < n - 1 or closed) else pts[i])
        d1 = (b - a).normalized() if (b - a).length > 1e-6 else (c2 - b).normalized()
        d2 = (c2 - b).normalized() if (c2 - b).length > 1e-6 else d1
        t = d1 + d2; t = t.normalized() if t.length > 1e-6 else d1
        nrm = Vector((-t.y, t.x)); k = max(0.4, nrm.dot(Vector((-d1.y, d1.x))))
        out.append(b + nrm * (d / k))
    return out


def ribbon(name, pts, width, z0, z1, c, material, closed=False, zfn=None, uv=False, top_fn=None):
    """Turns a polyline (mm) into a wall of the given width from z0 to z1; uv maps length and height in mm."""
    pts = clean(pts)
    if len(pts) < 2: return None
    n = len(pts)
    left = offset_line(pts, width / 2, closed); right = offset_line(pts, -width / 2, closed)
    acc = [0.0]
    for i in range(1, n): acc.append(acc[-1] + math.dist(pts[i - 1], pts[i]))
    me = bpy.data.meshes.new(name); bm = bmesh.new()
    def v(p, z): return bm.verts.new(W(p.x, p.y, z + (zfn(p.x, p.y) if zfn else 0)))
    def top(q): return top_fn(q[0], q[1]) if top_fn else z1
    L0 = [v(p, z0) for p in left]; L1 = [v(p, top(q)) for p, q in zip(left, pts)]
    R0 = [v(p, z0) for p in right]; R1 = [v(p, top(q)) for p, q in zip(right, pts)]
    lay = bm.loops.layers.uv.new() if uv else None
    m = n if closed else n - 1
    for i in range(m):
        j = (i + 1) % n
        faces = [((L1[i], L1[j], R1[j], R1[i]), None), ((L0[i], R0[i], R0[j], L0[j]), None),
                 ((L0[i], L0[j], L1[j], L1[i]), (i, j, j, i)), ((R0[j], R0[i], R1[i], R1[j]), (j, i, i, j))]
        for verts, idx in faces:
            f = bm.faces.new(verts)
            if lay and idx:
                for loop, k, vert in zip(f.loops, idx, verts):
                    loop[lay].uv = (acc[k], (top(pts[k]) if vert in (L1[k], R1[k]) else z0))
    if not closed:
        bm.faces.new((L0[0], L1[0], R1[0], R0[0])); bm.faces.new((R0[-1], R1[-1], L1[-1], L0[-1]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me); bm.free()
    return link(bpy.data.objects.new(name, me), c, material)


def tube(name, pts, radius, z, c, material, closed=False, zfn=None):
    cu = bpy.data.curves.new(name, "CURVE"); cu.dimensions = "3D"; cu.bevel_depth = radius / 10.0; cu.bevel_resolution = 3
    cu.use_fill_caps = not closed
    sp = cu.splines.new("POLY"); pts = clean(pts); sp.points.add(len(pts) - 1)
    for p, (x, y) in zip(sp.points, pts): p.co = (*W(x, y, z + (zfn(x, y) if zfn else 0)), 1)
    sp.use_cyclic_u = closed
    return link(bpy.data.objects.new(name, cu), c, material)


def cyl(name, x, y, r, z0, z1, c, material, verts=64, r_top=None):
    me = bpy.data.meshes.new(name); bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=verts, radius1=r / 10, radius2=(r if r_top is None else r_top) / 10, depth=(z1 - z0) / 10)
    bmesh.ops.translate(bm, verts=bm.verts, vec=Vector(W(x, y, (z0 + z1) / 2)))
    bm.to_mesh(me); bm.free()
    o = link(bpy.data.objects.new(name, me), c, material)
    for p in o.data.polygons: p.use_smooth = abs(p.normal.z) < 0.9
    return o


def sphere(name, x, y, z, r, c, material):
    me = bpy.data.meshes.new(name); bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=48, v_segments=24, radius=r / 10)
    bm.to_mesh(me); bm.free()
    o = link(bpy.data.objects.new(name, me), c, material); o.location = W(x, y, z)
    for p in o.data.polygons: p.use_smooth = True
    return o


def flat_ring(name, x, y, angle_deg, zc, r_in, r_out, z_cut, bar, thick, c, material, n=32):
    """An upright ring flattened at the bottom: cut off at z_cut, with a bar of height bar across the cut that the
    ramp's floor rests on. It stands at (x, y), centred at height zc (mm), its plane turned by angle_deg."""
    a = math.radians(angle_deg); ux, uy = math.cos(a), math.sin(a); nx, ny = -uy, ux
    z_bar = z_cut + bar
    to = math.asin(max(-1.0, min(1.0, (z_cut - zc) / r_out)))
    ti = math.asin(max(-1.0, min(1.0, (z_bar - zc) / r_in)))
    outer = [(r_out * math.cos(to + (math.pi - 2 * to) * k / n), zc + r_out * math.sin(to + (math.pi - 2 * to) * k / n)) for k in range(n + 1)]
    inner = [(r_in * math.cos(ti + (math.pi - 2 * ti) * k / n), zc + r_in * math.sin(ti + (math.pi - 2 * ti) * k / n)) for k in range(n + 1)]
    quads = [(outer[k], outer[k + 1], inner[k + 1], inner[k]) for k in range(n)]
    xo = outer[0][0]
    quads.append(((-xo, z_cut), (xo, z_cut), (xo, z_bar), (-xo, z_bar)))
    me = bpy.data.meshes.new(name); bm = bmesh.new()
    for q in quads:
        f = [bm.verts.new(W(x + u * ux + nx * thick / 2, y + u * uy + ny * thick / 2, z)) for u, z in q]
        b = [bm.verts.new(W(x + u * ux - nx * thick / 2, y + u * uy - ny * thick / 2, z)) for u, z in q]
        bm.faces.new(f); bm.faces.new(b[::-1])
        for i in range(4):
            j = (i + 1) % 4; bm.faces.new((f[i], f[j], b[j], b[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me); bm.free()
    return link(bpy.data.objects.new(name, me), c, material)


def hoop_ring(name, x, y, angle_deg, zc, r_in, r_out, thick, c, material, n=48):
    """A flat ring standing upright at (x, y), centred at height zc (mm), its plane turned by angle_deg."""
    a = math.radians(angle_deg); ux, uy = math.cos(a), math.sin(a); nx, ny = -uy, ux
    me = bpy.data.meshes.new(name); bm = bmesh.new()
    def ring(r, side):
        return [bm.verts.new(W(x + r * math.cos(2 * math.pi * k / n) * ux + nx * side, y + r * math.cos(2 * math.pi * k / n) * uy + ny * side,
                               zc + r * math.sin(2 * math.pi * k / n))) for k in range(n)]
    of, inf, ob, ib = ring(r_out, thick / 2), ring(r_in, thick / 2), ring(r_out, -thick / 2), ring(r_in, -thick / 2)
    for k in range(n):
        j = (k + 1) % n
        bm.faces.new((of[k], of[j], inf[j], inf[k])); bm.faces.new((ob[k], ib[k], ib[j], ob[j]))
        bm.faces.new((of[k], ob[k], ob[j], of[j])); bm.faces.new((inf[k], inf[j], ib[j], ib[k]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me); bm.free()
    o = link(bpy.data.objects.new(name, me), c, material)
    for p in o.data.polygons: p.use_smooth = True
    return o


def figure(name, x, y, angle_deg, profile, thick, c, material, z0=0.0):
    """An upright plywood cutout: profile is (u, z) in mm, standing on (x, y), its plane turned by angle_deg."""
    a = math.radians(angle_deg); ux, uy = math.cos(a), math.sin(a); nx, ny = -uy, ux
    me = bpy.data.meshes.new(name); bm = bmesh.new()
    front = [bm.verts.new(W(x + u * ux + nx * thick / 2, y + u * uy + ny * thick / 2, z0 + z)) for u, z in profile]
    back = [bm.verts.new(W(x + u * ux - nx * thick / 2, y + u * uy - ny * thick / 2, z0 + z)) for u, z in profile]
    bm.faces.new(front); bm.faces.new(back[::-1])
    for i in range(len(profile)):
        j = (i + 1) % len(profile); bm.faces.new((front[i], front[j], back[j], back[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me); bm.free()
    return link(bpy.data.objects.new(name, me), c, material)


def annulus(name, x, y, r_in, r_out, z, c, material, n=64):
    """A flat ring at height z (mm), built from quads so its face has no seam."""
    me = bpy.data.meshes.new(name); bm = bmesh.new()
    outer = [bm.verts.new(W(x + r_out * math.cos(2 * math.pi * k / n), y + r_out * math.sin(2 * math.pi * k / n), z)) for k in range(n)]
    inner = [bm.verts.new(W(x + r_in * math.cos(2 * math.pi * k / n), y + r_in * math.sin(2 * math.pi * k / n), z)) for k in range(n)]
    for k in range(n):
        bm.faces.new((outer[k], outer[(k + 1) % n], inner[(k + 1) % n], inner[k]))
    bm.normal_update()
    for f in bm.faces:
        if f.normal.z < 0: f.normal_flip()
    bm.to_mesh(me); bm.free()
    return link(bpy.data.objects.new(name, me), c, material)


def ring_outline(x, y, r_in, r_out, n=48):
    """A flat ring as one outline: around the outside, along a seam, back around the inside."""
    out = [(x + r_out * math.cos(2 * math.pi * k / n), y + r_out * math.sin(2 * math.pi * k / n)) for k in range(n + 1)]
    return out + [(x + r_in * math.cos(2 * math.pi * k / n), y + r_in * math.sin(2 * math.pi * k / n)) for k in range(n, -1, -1)]


def inside(pt, poly):
    """Whether the point lies inside the polygon (even-odd rule)."""
    x, y = pt; result = False
    for i in range(len(poly)):
        (x1, y1), (x2, y2) = poly[i - 1], poly[i]
        if (y1 > y) != (y2 > y) and x < (x2 - x1) * (y - y1) / (y2 - y1) + x1: result = not result
    return result


def hull(pts):
    """The convex hull of points (x, y), in order round it (Andrew's monotone chain)."""
    pts = sorted(set(pts))
    def half(seq):
        out = []
        for p in seq:
            while len(out) >= 2 and (out[-1][0] - out[-2][0]) * (p[1] - out[-2][1]) - (out[-1][1] - out[-2][1]) * (p[0] - out[-2][0]) <= 0:
                out.pop()
            out.append(p)
        return out[:-1]
    return half(pts) + half(pts[::-1])


def placed(pid):
    return [s for s in SHAPES if pid in s["chain"]]


def by(pid, cls):
    return [s for s in placed(pid) if cls in s["cls"].split()]


def centroid(pts):
    return (sum(p[0] for p in pts) / len(pts), sum(p[1] for p in pts) / len(pts))


def polyline_param(line):
    """Returns f(x, y) giving the arc-length fraction of the point on the line nearest to (x, y)."""
    if len(line) > 240:                  # a long drawn path is thinned for speed; its shape survives
        step = len(line) // 200 + 1
        line = line[::step] + ([line[-1]] if (len(line) - 1) % step else [])
    acc = [0.0]
    for i in range(1, len(line)):
        acc.append(acc[-1] + math.dist(line[i - 1], line[i]))
    total = acc[-1]
    def f(x, y):
        best, bs = 1e18, 0.0
        for i in range(1, len(line)):
            ax, ay = line[i - 1]; bx, by_ = line[i]
            dx, dy = bx - ax, by_ - ay; L2 = dx * dx + dy * dy or 1e-9
            t = max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / L2))
            d = (ax + t * dx - x) ** 2 + (ay + t * dy - y) ** 2
            if d < best: best, bs = d, acc[i - 1] + t * math.sqrt(L2)
        return bs / total
    return f


def profile(points):
    """Piecewise linear height over a parameter: points is [(s, z_mm), ...] sorted by s."""
    def f(s):
        if s <= points[0][0]: return points[0][1]
        for (s0, z0), (s1, z1) in zip(points, points[1:]):
            if s <= s1: return z0 + (z1 - z0) * (s - s0) / (s1 - s0)
        return points[-1][1]
    return f


def idw(anchors):
    """Height (mm) by inverse distance weighting over anchors [(x, y, z)]."""
    def f(x, y):
        ws = [1 / max(1e-6, (x - ax) ** 2 + (y - ay) ** 2) ** 1.5 for ax, ay, _ in anchors]
        return sum(w * a[2] for w, a in zip(ws, anchors)) / sum(ws)
    return f


GRAY_START_Y = 136.4   # mm, where the gray launch lane takes over from the shooter lane


def shooter_z(x, y):
    """The shooter lane's floor after the side photo: level at the plunger, a steep first rise that flattens
    toward the top end, where the gray lane begins."""
    t = max(0.0, min(1.0, (420.0 - y) / (420.0 - 100.0)))
    return H["shooter_top"] * (1 - (1 - t) ** 1.8)


BLUE_FLOOR = [s["pts"] for s in SHAPES if "ramp-blue" in s["chain"] and s["cls"] == "ramp-blue"]
GREEN_LINE = [tuple(p) for p in next(s for s in SHAPES if "ramp-orbit-left" in s["chain"] and s["cls"] == "ramp-shine")["pts"]]
if GREEN_LINE[0][1] < 120:
    GREEN_LINE = GREEN_LINE[::-1]        # run from the entry on the playfield to the end above the right bell
green_param = polyline_param(GREEN_LINE)
GREEN_PEAK = 0.45                        # where along its course the green board is highest


def green_z(x, y):
    """The green track's floor: one flat board leaving the playfield flush and level at its entry, a hair above
    it, climbing without a bend to a round top and falling toward its end."""
    t = green_param(x, y)
    lo, top, end = 0.02, H["fence"] + H["ramp_floor"], H["fence_end"] + H["ramp_floor"]
    if t <= GREEN_PEAK:
        v = t / GREEN_PEAK
        return lo + (top - lo) * v * v * (3 - 2 * v)
    return top - (top - end) * ((t - GREEN_PEAK) / (1 - GREEN_PEAK)) ** 1.6


def fence_top(x, y):
    """The U lane's fence carries the green board, flush under it; where the board lies on the playfield, the
    fence has no height."""
    return max(0.0, green_z(x, y) - H["ramp_floor"])


def reset():
    for o in list(bpy.data.objects): bpy.data.objects.remove(o, do_unlink=True)
    for c in list(bpy.data.collections): bpy.data.collections.remove(c)
    for block in (bpy.data.meshes, bpy.data.curves, bpy.data.materials, bpy.data.images, bpy.data.cameras, bpy.data.lights):
        for b in list(block): block.remove(b)
    sc = bpy.context.scene
    sc.timeline_markers.clear()
    sc.view_settings.exposure = 0.0
    for idb in (sc, sc.view_settings):
        if getattr(idb, "animation_data", None): idb.animation_data_clear()


# ---------------------------------------------------------------- materials
def floor_material():
    m = bpy.data.materials.new("playfield"); m.use_nodes = True; nt = m.node_tree; n = nt.nodes
    img = bpy.data.images.load(os.path.join(OUT, "floor.png"))
    tc = n.new("ShaderNodeTexCoord"); sep = n.new("ShaderNodeSeparateXYZ"); nt.links.new(tc.outputs["Object"], sep.inputs[0])
    u = n.new("ShaderNodeMath"); u.operation = "MULTIPLY_ADD"; u.inputs[1].default_value = 10 / VB[2]; u.inputs[2].default_value = -VB[0] / VB[2]
    v = n.new("ShaderNodeMath"); v.operation = "MULTIPLY_ADD"; v.inputs[1].default_value = 10 / VB[3]; v.inputs[2].default_value = 1 + VB[1] / VB[3]
    nt.links.new(sep.outputs[0], u.inputs[0]); nt.links.new(sep.outputs[1], v.inputs[0])
    comb = n.new("ShaderNodeCombineXYZ"); nt.links.new(u.outputs[0], comb.inputs[0]); nt.links.new(v.outputs[0], comb.inputs[1])
    it = n.new("ShaderNodeTexImage"); it.image = img; it.interpolation = "Cubic"; it.extension = "EXTEND"
    nt.links.new(comb.outputs[0], it.inputs[0])
    b = n["Principled BSDF"]; nt.links.new(it.outputs["Color"], b.inputs["Base Color"])
    # painted wood: matte, no lacquer
    b.inputs["Roughness"].default_value = 0.7
    return m


def stripes_material(name, a, b_, period):
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; n = nt.nodes
    tc = n.new("ShaderNodeTexCoord"); sep = n.new("ShaderNodeSeparateXYZ"); nt.links.new(tc.outputs["Object"], sep.inputs[0])
    w = n.new("ShaderNodeMath"); w.operation = "WRAP"; w.inputs[1].default_value = 0; w.inputs[2].default_value = period / 10
    nt.links.new(sep.outputs[0], w.inputs[0])
    gt = n.new("ShaderNodeMath"); gt.operation = "GREATER_THAN"; gt.inputs[1].default_value = period / 20
    nt.links.new(w.outputs[0], gt.inputs[0])
    mix = n.new("ShaderNodeMix"); mix.data_type = "RGBA"
    mix.inputs[6].default_value = hexcol(a); mix.inputs[7].default_value = hexcol(b_)
    nt.links.new(gt.outputs[0], mix.inputs[0])
    bs = n["Principled BSDF"]; nt.links.new(mix.outputs[2], bs.inputs["Base Color"])
    bs.inputs["Roughness"].default_value = 0.35; bs.inputs["Coat Weight"].default_value = 0.3
    return m


def fence_material():
    """The U lane's wall painted as a picket fence: pickets and gaps along its length, two rails across."""
    m = bpy.data.materials.new("fence"); m.use_nodes = True; nt = m.node_tree; n = nt.nodes
    uvn = n.new("ShaderNodeUVMap"); sep = n.new("ShaderNodeSeparateXYZ"); nt.links.new(uvn.outputs[0], sep.inputs[0])
    w = n.new("ShaderNodeMath"); w.operation = "WRAP"; w.inputs[1].default_value = 0; w.inputs[2].default_value = 4.0
    nt.links.new(sep.outputs[0], w.inputs[0])
    gap = n.new("ShaderNodeMath"); gap.operation = "GREATER_THAN"; gap.inputs[1].default_value = 2.6
    nt.links.new(w.outputs[0], gap.inputs[0])
    def band(lo, hi):
        a = n.new("ShaderNodeMath"); a.operation = "GREATER_THAN"; a.inputs[1].default_value = lo; nt.links.new(sep.outputs[1], a.inputs[0])
        b = n.new("ShaderNodeMath"); b.operation = "LESS_THAN"; b.inputs[1].default_value = hi; nt.links.new(sep.outputs[1], b.inputs[0])
        mm = n.new("ShaderNodeMath"); mm.operation = "MULTIPLY"; nt.links.new(a.outputs[0], mm.inputs[0]); nt.links.new(b.outputs[0], mm.inputs[1])
        return mm
    r1, r2 = band(7.0, 9.5), band(17.0, 19.5)
    rails = n.new("ShaderNodeMath"); rails.operation = "MAXIMUM"; nt.links.new(r1.outputs[0], rails.inputs[0]); nt.links.new(r2.outputs[0], rails.inputs[1])
    mix1 = n.new("ShaderNodeMix"); mix1.data_type = "RGBA"
    mix1.inputs[6].default_value = hexcol("#b07a45"); mix1.inputs[7].default_value = hexcol("#4b2b14")
    nt.links.new(gap.outputs[0], mix1.inputs[0])
    mix2 = n.new("ShaderNodeMix"); mix2.data_type = "RGBA"; mix2.inputs[7].default_value = hexcol("#7a4a26")
    nt.links.new(mix1.outputs[2], mix2.inputs[6]); nt.links.new(rails.outputs[0], mix2.inputs[0])
    bs = n["Principled BSDF"]; nt.links.new(mix2.outputs[2], bs.inputs["Base Color"]); bs.inputs["Roughness"].default_value = 0.55
    return m


def node_math(nt, op, *vals):
    n = nt.nodes.new("ShaderNodeMath"); n.operation = op
    for i, v in enumerate(vals):
        if v is None: continue
        if isinstance(v, (int, float)): n.inputs[i].default_value = v
        else: nt.links.new(v, n.inputs[i])
    return n.outputs[0]


def node_mix(nt, fac, a, b):
    m = nt.nodes.new("ShaderNodeMix"); m.data_type = "RGBA"
    for v, i in ((a, 6), (b, 7)):
        if isinstance(v, str): m.inputs[i].default_value = hexcol(v)
        else: nt.links.new(v, m.inputs[i])
    nt.links.new(fac, m.inputs[0])
    return m.outputs[2]


def side_wall_material():
    """The cabinet's inner faces: red above the playfield, plywood below it, the dark top edge."""
    m = bpy.data.materials.new("cabinet-wall"); m.use_nodes = True; nt = m.node_tree
    tc = nt.nodes.new("ShaderNodeTexCoord"); sep = nt.nodes.new("ShaderNodeSeparateXYZ"); nt.links.new(tc.outputs["Object"], sep.inputs[0])
    z = node_math(nt, "MULTIPLY", sep.outputs[2], 10.0)
    y = node_math(nt, "MULTIPLY", sep.outputs[1], -10.0)            # mm down the playfield, 0 at the back
    t = node_math(nt, "MINIMUM", node_math(nt, "MAXIMUM", node_math(nt, "DIVIDE", node_math(nt, "ADD", y, 8.4), 444.1), 0.0), 1.0)
    top = node_math(nt, "ADD", H["wall_back"], node_math(nt, "MULTIPLY", t, H["wall_front"] - H["wall_back"]))
    col = node_mix(nt, node_math(nt, "GREATER_THAN", z, 0.0), "#e8dcc0", "#b3242d")
    col = node_mix(nt, node_math(nt, "GREATER_THAN", z, node_math(nt, "SUBTRACT", top, 2.0)), col, "#2a2d33")
    # the outer faces of the two side walls are the gray base board; build_outer_sides lays the paint on it
    geo = nt.nodes.new("ShaderNodeNewGeometry"); gsep = nt.nodes.new("ShaderNodeSeparateXYZ"); nt.links.new(geo.outputs["Normal"], gsep.inputs[0])
    side_sign = node_math(nt, "SIGN", node_math(nt, "SUBTRACT", node_math(nt, "MULTIPLY", sep.outputs[0], 10.0), 115.0))
    outer = node_math(nt, "GREATER_THAN", node_math(nt, "MULTIPLY", gsep.outputs[0], side_sign), 0.5)
    col = node_mix(nt, outer, col, "#4a4d54")
    b = nt.nodes["Principled BSDF"]; nt.links.new(col, b.inputs["Base Color"]); b.inputs["Roughness"].default_value = 0.6
    return m


def back_wall_material():
    """The partition under the backbox: dark brown above, plywood below, black uprights."""
    m = bpy.data.materials.new("back-wall"); m.use_nodes = True; nt = m.node_tree
    tc = nt.nodes.new("ShaderNodeTexCoord"); sep = nt.nodes.new("ShaderNodeSeparateXYZ"); nt.links.new(tc.outputs["Object"], sep.inputs[0])
    z = node_math(nt, "MULTIPLY", sep.outputs[2], 10.0)
    x = node_math(nt, "MULTIPLY", sep.outputs[0], 10.0)
    col = node_mix(nt, node_math(nt, "GREATER_THAN", z, 0.0), "#e8dcc0", "#4a2616")
    post = node_math(nt, "LESS_THAN", node_math(nt, "WRAP", x, 46.0, 0.0), 4.0)
    col = node_mix(nt, post, col, "#121215")
    b = nt.nodes["Principled BSDF"]; nt.links.new(col, b.inputs["Base Color"]); b.inputs["Roughness"].default_value = 0.55
    return m


def wood():
    return mat("plywood", "#d9bf8a", rough=0.6)


SCOOP_SEAT = -5.5     # mm: the face of the scoop's kicker, on which the ball rests with its middle 1 mm under the board


def scoop_mechanism(c, x, y, r, dark):
    """The scoop under its hole in the 4 mm board: a dark cup, and at its floor the face of the solenoid's armature,
    which kicks the ball up and out. The solenoid hangs under the board and pulls upward; gravity returns the
    armature. Its size is estimated from the product's photos."""
    me = bpy.data.meshes.new("scoop-cup"); bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=False, segments=48, radius1=r / 10, radius2=r / 10, depth=1.0,
                          matrix=Matrix.Translation(W(x, y, -9.0)))
    bm.to_mesh(me); bm.free()
    link(bpy.data.objects.new("scoop-cup", me), c, dark)
    steel = mat("kicker-steel", "#b9bec6", rough=0.25, metal=1.0)
    cyl("scoop-kicker", x, y, 3.6, SCOOP_SEAT - 1.0, SCOOP_SEAT, c, steel, verts=32)
    cyl("scoop-kicker-rod", x, y, 2.5, -26.0, SCOOP_SEAT - 1.0, c, steel, verts=24)
    frame = mat("solenoid-frame", "#5d6168", rough=0.4, metal=0.8)
    copper = mat("solenoid-coil", "#b0652c", rough=0.35, metal=0.9)
    prism("scoop-solenoid", [(x - 12, y - 7.5), (x + 12, y - 7.5), (x + 12, y + 7.5), (x - 12, y + 7.5)], -44.0, -16.0, c, frame)
    cyl("scoop-solenoid-coil", x, y, 8.5, -41.0, -19.0, c, copper, verts=32)


# ---------------------------------------------------------------- the machine
def build_board():
    c = coll("board")
    me = bpy.data.meshes.new("playfield"); bm = bmesh.new()
    x0, x1, y0, y1 = -8.0, 238.0, -8.0, 433.0   # inside the cabinet walls
    # counterclockwise seen from above, so the face points up and the board's thickness lies below it
    vs = [bm.verts.new(W(x, y, 0)) for x, y in ((x0, y1), (x1, y1), (x1, y0), (x0, y0))]
    bm.faces.new(vs); bm.to_mesh(me); bm.free()
    pf = link(bpy.data.objects.new("playfield", me), c, floor_material())
    so = pf.modifiers.new("solid", "SOLIDIFY"); so.thickness = 0.4; so.offset = -1.0
    # holes: the scoop, and one under each bumper, which the coil pulls the bumper into; both are dark
    pit = mat("pit", "#050506", rough=0.9)
    glow = pit
    holes = [("scoop", 31.87, 123.66, 5.5, pit)]
    for s in SHAPES:
        if s["cls"] == "bumper-base" and "circle" in s:
            # concentric with the bumper; the drawing's skirt circle gives the radius
            holes.append((s["chain"][-1], s["circle"][0], s["circle"][1], 11.55, glow))
    for name, x, y, r, well in holes:
        cutter = cyl(f"{name}-cutter", x, y, r, -8, 2, c, None)
        cutter.hide_render = True; cutter.hide_viewport = True
        bo = pf.modifiers.new(name, "BOOLEAN"); bo.object = cutter; bo.operation = "DIFFERENCE"
        if name == "scoop":
            scoop_mechanism(c, x, y, r, well)
        else:
            cyl(f"{name}-well", x, y, r, -30, -0.2, c, well)
    # the drain's disk is a light diffuser flush with the board, its face over the foil's height
    cyl("drain-diffuser", 108.54, 387.04, 7.73, 0.0, H["foil"] + 0.1, c, mat("diffuser-drain", "#d9e4ea", rough=0.3, transmission=0.3, emit="#fff1d6", strength=2.5))
    wall = side_wall_material()
    top = back_wall_material()
    for s in placed("cabinet"):
        if s["cls"] in ("wall", "wall-top"):
            # the partition at the back is the diorama's front, up to its base; the side walls run to the wall top
            hi = (lambda x, y: wall_top(y)) if s["cls"] == "wall" else (lambda x, y: DIORAMA_BASE)
            prism("cabinet-wall", s["pts"], -110, 0.0, c, wall if s["cls"] == "wall" else top, hi_fn=hi)


DIORAMA_WIDTH = 275.0                           # mm, the manual's assembly size: the towers' width
# mm above the playfield's back edge: the partition's ledge, from the photos, where the towers' base plates rest on
# the side walls' top
DIORAMA_BASE = 48.0


def build_backbox():
    """The diorama at the cabinet's back: two towers that stand from the table on the cabinet's back corners,
    which reach into them, and the box between them. The drawing's layers stand one behind the other on the partition's ledge:
    the frame with towers and curtains in front, the scene, the new part B20 and the sky at the back. The drawing's
    elevation is scaled to the manual's width; the depths are estimates from the photos and the manual."""
    c = coll("backbox")
    y_back = -8.4
    height = -VB[1] + y_back                        # mm of picture in the drawing
    s = DIORAMA_WIDTH / 317.8                       # the drawing's towers span 317.8 mm
    bottom = DIORAMA_BASE + 8.4 * math.sin(math.radians(TILT_DEG))
    top = bottom + height * s
    X = lambda x: 115.0 + (x - 115.0) * s
    vb = height / VB[3]
    def layer(name, y):
        img = bpy.data.images.load(os.path.join(OUT, name + ".png"))
        m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree
        tex = nt.nodes.new("ShaderNodeTexImage"); tex.image = img
        b = nt.nodes["Principled BSDF"]; nt.links.new(tex.outputs["Color"], b.inputs["Base Color"])
        nt.links.new(tex.outputs["Alpha"], b.inputs["Alpha"]); b.inputs["Roughness"].default_value = 0.6
        nt.links.new(tex.outputs["Color"], b.inputs["Emission Color"]); b.inputs["Emission Strength"].default_value = 0.35
        me = bpy.data.meshes.new(name); bm = bmesh.new()
        vs = [bm.verts.new((X(x) / 10, -y / 10, z / 10)) for x, z in ((VB[0], bottom), (VB[0] + VB[2], bottom), (VB[0] + VB[2], top), (VB[0], top))]
        f = bm.faces.new(vs); uv = bm.loops.layers.uv.new()
        for loop, (uu, vv) in zip(f.loops, [(0, 1 - vb), (1, 1 - vb), (1, 1), (0, 1)]):
            loop[uv].uv = (uu, vv)
        bm.to_mesh(me); bm.free()
        return link(bpy.data.objects.new(name, me), c, m)
    # the towers reach 10 mm past the brown partition's front (4.3 mm, from the drawing) over the cabinet's back
    # corners, where only the low side stacks end in them; the frame's picture stands just in front of them, the
    # sky at the casing's back
    front, back = 14.95, -18.0
    for name, y in (("backbox-back", back + 2.0), ("backbox-b20", -10.0), ("backbox-scene", -5.0), ("backbox-frame-body", front)):
        layer(name, y)
    def box(name, x0, x1, y0, y1, z0, z1, material):
        me = bpy.data.meshes.new(name); bm = bmesh.new()
        bmesh.ops.create_cube(bm, size=1.0)
        bmesh.ops.scale(bm, vec=((x1 - x0) / 10, (y1 - y0) / 10, (z1 - z0) / 10), verts=bm.verts)
        bmesh.ops.translate(bm, vec=((x0 + x1) / 20, -(y0 + y1) / 20, (z0 + z1) / 20), verts=bm.verts)
        bm.to_mesh(me); bm.free()
        return link(bpy.data.objects.new(name, me), c, material)
    stage = bottom                                  # the partition's ledge, where the picture stands
    cream = mat("diorama-cream", "#efe8d8", rough=0.6)
    gold_line = mat("diorama-gold", "#d9b04a", rough=0.35, metal=0.5)
    panel_gray = mat("diorama-panel", "#4a4d54", rough=0.6)
    def framed_panel(xo, out, y0, y1, z0, z1):
        """A gray panel with a thin gold frame laid on the outer face of a tower (xo, facing out)."""
        xa, xb = sorted((xo, xo + out * 0.4))
        box("diorama-tower-frame", xa, xb, y0, y1, z0, z1, gold_line)
        xa, xb = sorted((xo + out * 0.4, xo + out * 0.7))
        box("diorama-tower-panel", xa, xb, y0 + 0.8, y1 - 0.8, z0 + 0.8, z1 - 0.8, panel_gray)
    # the towers from the drawing: 42.2 mm wide round their middles at -15.6 and 245.6 mm
    left, right = (X(-36.7), X(5.5)), (X(224.5), X(266.5))
    stick = mat("diorama-stick", "#a07850", rough=0.6)
    def star(r_out, r_in, zc):
        return [((r_out if k % 2 == 0 else r_in) * math.cos(math.radians(90 + 36 * k)),
                 zc + (r_out if k % 2 == 0 else r_in) * math.sin(math.radians(90 + 36 * k))) for k in range(10)]
    def tower_top(x0, x1, y0, y1):
        """The tower's top from the photos, heights and widths from the drawing (mm below the elevation's top and
        across, scaled): the red cover plate (79 to 74.5, 56.8 wide), the red box with gold stripes between four
        dark posts (74.5 to 52.5, 45.8), a gray base and a gray plate (52.5 to 40, 36.8 and 39.2), a red plate with
        gold pegs (40 to 38), the dark pedestal (38 to 28), a gray and a red disc (28 to 22), the wooden stick
        (22 to 12) and the gold star (12 to 0)."""
        cx, cy, d = (x0 + x1) / 2, (y0 + y1) / 2, y1 - y0
        zd = lambda depth: top - depth * s
        def slab(name, sw, sd, z0, z1, material):
            box(name, cx - sw / 2, cx + sw / 2, cy - sd / 2, cy + sd / 2, z0, z1, material)
        cw = 56.8 * s
        slab("diorama-tower-cover", cw, d + (cw - (x1 - x0)), zd(79.0), zd(74.5), red)
        pw = 45.8 * s; vw = pw - 6.0; vd = d - (x1 - x0 - vw)
        slab("diorama-tower-vent", vw, vd, zd(74.5), zd(52.5), red)
        for zz in (0.25, 0.45, 0.65):
            z = zd(74.5) + (zd(52.5) - zd(74.5)) * zz
            box("diorama-tower-vent-stripe", cx - vw / 2 + 2, cx + vw / 2 - 2, cy + vd / 2, cy + vd / 2 + 0.15, z, z + 0.8, gold_line)
        for sx in (-1, 1):
            for sy in (-1, 1):
                px, py = cx + sx * (vw / 2 + 1.5), cy + sy * (vd / 2 + 1.5)
                box("diorama-tower-post", px - 1.5, px + 1.5, py - 1.5, py + 1.5, zd(74.5), zd(43.5), dark)
        slab("diorama-tower-gray", 36.8 * s, vd, zd(52.5), zd(43.5), panel_gray)
        gw = 39.2 * s
        slab("diorama-tower-gray", gw, gw * d / (x1 - x0), zd(43.5), zd(40.0), panel_gray)
        rw = gw - 5.0
        slab("diorama-tower-cross", rw, rw * d / (x1 - x0), zd(40.0), zd(38.0), red)
        for sx in (-1, 1):
            for sy in (-1, 1):
                cyl("diorama-tower-peg", cx + sx * (rw / 2 - 2.5), cy + sy * (rw * d / (x1 - x0) / 2 - 2.5), 1.0, zd(38.0), zd(37.2), c, gold_line, 12)
        cyl("diorama-tower-pedestal", cx, cy, 8.4 * s, zd(38.0), zd(28.0), c, dark, 4, r_top=6.9 * s * 0.6)
        cyl("diorama-tower-disc", cx, cy, 7.5 * s, zd(28.0), zd(25.0), c, panel_gray, 32)
        cyl("diorama-tower-disc", cx, cy, 5.0 * s, zd(25.0), zd(22.0), c, red, 32)
        cyl("diorama-tower-stick", cx, cy, 0.9, zd(22.0), zd(11.0), c, stick, 12)
        figure("diorama-tower-star", cx, cy, 0.0, star(6.0 * s, 2.6 * s, zd(6.0)), 1.8, c, gold_line)
        figure("diorama-tower-star-hole", cx, cy, 0.0, star(2.7 * s, 1.1 * s, zd(6.0)), 2.0, c, dark)
    dark = mat("diorama-column", "#1c1c20", rough=0.6)
    gray = mat("diorama-gray", "#4a4d54", rough=0.6)
    red = mat("diorama-red", "#b3242d", rough=0.55)
    wall = 62.0                                     # the side walls' top at the cabinet's back end, tilted
    # the tower in the drawing, from the elevation's top down: its top to 79 mm, then the red tower box, its foot
    # at 163.5 mm
    box_top, box_foot = top - 79.0 * s, top - 163.5 * s
    for x0, x1 in (left, right):
        # the column and the tower above it share one nearly square footprint: the tower's width from the picture,
        # 40 mm deep from the frame back; the cabinet's back corner reaches into it. From the photo of its side,
        # heights in mm above the table: a red foot, a stepped plinth, a tall part with a gold-framed gray panel,
        # dark ledges with a small slotted plate between them, and the ledge at the side wall's top
        y0, y1 = front - 40.5, front - 0.5
        box("diorama-tower", x0, x1, y0, y1, TABLE_Z, box_top, red)
        out = -1.0 if x0 < 115 else 1.0
        xo = x0 if out < 0 else x1
        def ledge(z0, z1, grow, material):
            box("diorama-tower-ledge", x0 - grow, x1 + grow, y0 - grow, y1 + grow, z0, z1, material)
        for h0, h1, grow, material in ((18, 21, 2.0, dark), (21, 23, 3.0, cream), (23, 25, 1.5, dark), (106, 111, 2.0, dark),
                                       (119, 121, 1.5, dark), (131, 135, 2.0, dark)):
            ledge(TABLE_Z + h0, TABLE_Z + h1, grow, material)
        ledge(wall, box_foot - 3.5, 2.0, dark)
        framed_panel(xo, out, y0 + 3, y1 - 3, TABLE_Z + 30, TABLE_Z + 101)
        mid = (y0 + y1) / 2
        xa, xb = sorted((xo, xo + out * 0.5))
        box("diorama-tower-plate-slot", xa, xb, mid - 6, mid + 6, TABLE_Z + 123, TABLE_Z + 129, gray)
        xa, xb = sorted((xo + out * 0.5, xo + out * 0.6))
        box("diorama-tower-slot", xa, xb, mid - 2.5, mid + 2.5, TABLE_Z + 125.5, TABLE_Z + 126.5, dark)
        # the tower above: a gold-framed gray panel on its side, the frame's picture on its front, the dark cap
        # stack on its top
        framed_panel(xo, out, y0 + 3, y1 - 3, box_foot + 4, box_top - 6)
        tower_top(x0, x1, y0, y1)
    # the sky is painted on the casing's back board; behind it the board is bare wood
    box("diorama-back-board", left[1], right[0], back + 0.6, back + 1.8, stage, top - 78.7 * s + 1.5, mat("diorama-board", "#8a6a44", rough=0.6))
    # the casing's back below the sky
    box("diorama-back", left[0], right[1], back - 1.0, back, TABLE_Z, stage, mat("diorama-back", "#4a2616", rough=0.6))
    # the scene stands on a red stage on the partition, from the frame back to the sky; its top lies flush with the
    # frame's dark bottom bar, which is the stage's front edge
    box("diorama-stage", left[1], right[0], back, front - 0.1, stage, stage + 3.0, red)
    # the crown on the frame's top bar: its outline from the drawing, 3 mm thick, the picture on its front and
    # red round its edge, a blue and gold star standing out of its middle
    crown = json.load(open(os.path.join(OUT, "crown.json")))["outline"]
    pic = bpy.data.materials.new("crown-picture"); pic.use_nodes = True; nt = pic.node_tree
    tex = nt.nodes.new("ShaderNodeTexImage"); tex.image = bpy.data.images.load(os.path.join(OUT, "backbox-frame.png"), check_existing=True)
    b = nt.nodes["Principled BSDF"]; nt.links.new(tex.outputs["Color"], b.inputs["Base Color"])
    nt.links.new(tex.outputs["Color"], b.inputs["Emission Color"]); b.inputs["Emission Strength"].default_value = 0.35
    me = bpy.data.meshes.new("diorama-crown"); bm = bmesh.new(); uvl = bm.loops.layers.uv.new()
    pts = [(X(x) / 10, (top - dep * s) / 10, (x - VB[0]) / VB[2], 1 - dep / VB[3]) for x, dep in crown]
    yf, yb = -(front + 1.2) / 10, -(front - 1.8) / 10
    fv = [bm.verts.new((x, yf, z)) for x, z, _, _ in pts]
    bv = [bm.verts.new((x, yb, z)) for x, z, _, _ in pts]
    face = bm.faces.new(fv[::-1]); face.material_index = 0
    for loop in face.loops:
        i = fv.index(loop.vert); loop[uvl].uv = (pts[i][2], pts[i][3])
    bm.faces.new(bv).material_index = 1
    for i in range(len(pts)):
        j = (i + 1) % len(pts); bm.faces.new((fv[i], fv[j], bv[j], bv[i])).material_index = 1
    bm.normal_update(); bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new("diorama-crown", me); c.objects.link(o)
    me.materials.append(pic); me.materials.append(red)
    zc = top - 32.0 * s
    figure("diorama-crown-star", 115.0, front + 1.7, 0.0, star(4.6, 2.0, zc), 1.0, c, mat("crown-star-blue", "#8fb4dc", rough=0.4))
    figure("diorama-crown-star", 115.0, front + 2.4, 0.0, star(2.8, 1.2, zc), 0.6, c, gold_line)
    # the box's red ceiling meets the sky's top edge, 78.7 mm below the elevation's top in the drawing; between
    # the ledge and the towers' boxes, side walls close the box
    ceiling = top - 78.7 * s
    box("diorama-ceiling", left[1], right[0], back, front, ceiling, ceiling + 1.5, red)
    for x in (left[1], right[0]):
        box("diorama-wall", x - 1.0, x + 1.0, back, front, stage, box_foot, mat("diorama-wall", "#7a1a20", rough=0.6))


def foil_material():
    return mat("foil", "#cfd2d6", rough=0.4, metal=0.85)


def build_bumpers():
    c = coll("bumpers")
    B = BUMPER
    skirt = mat("bumper-skirt", "#2a2a30", rough=0.5, emit="#9a5cff", strength=0.8)
    spring = mat("spring", "#c9ccd2", rough=0.25, metal=1.0)
    dish = mat("bumper-dish", "#8d9198", rough=0.55, metal=1.0)
    base = mat("bumper-base", "#8e1d27", rough=0.3, coat=0.8, emit="#ff4a3a", strength=0.0)
    ring = mat("bumper-ring", "#e7d9a8", rough=0.35, emit="#ffd77a", strength=0.0)
    cap = mat("bumper-cap", "#2f5b46", rough=0.25, coat=0.9)
    gold = mat("gold", "#e8c04a", rough=0.25, metal=1.0)
    star = mat("bumper-star", "#f6e7a1", rough=0.3, emit="#f6e7a1", strength=0.3)
    for k in (1, 2, 3):
        pid = f"bumper-{k}"
        for s in placed(pid):
            cl = s["cls"]
            if cl == "bumper-skirt":
                # the foil sheet around the bumpers leaves a ring of the board free around each hole
                x, y = next(t["circle"][:2] for t in placed(pid) if t["cls"] == "bumper-base")
                cut = cyl(f"{pid}-foil-cutter", x, y, B["foil_r"], -1.0, 1.0, c, None)
                cut.hide_render = True; cut.hide_viewport = True
            elif cl == "bumper-base":
                x, y, r = s["circle"]
                cyl(f"{pid}-spring", x, y, 2.0, -12.0, B["dish_lo"], c, spring, 24)
                cyl(f"{pid}-dish", x, y, B["dish_r_lo"], B["dish_lo"], B["dish_hi"], c, dish, r_top=r)
                cyl(f"{pid}-base", x, y, r, B["dish_hi"], B["f3_hi"], c, base)
            elif cl == "bumper-ring":
                x, y, r = s["circle"]
                cyl(f"{pid}-hub", x, y, 2.5, B["f3_hi"], B["ribs_hi"], c, base, 24)
                for k in range(6):
                    rib = [(2.0, 0.0), (B["ribs_r_lo"], 0.0), (r, B["ribs_hi"] - B["f3_hi"]), (2.0, B["ribs_hi"] - B["f3_hi"])]
                    figure(f"{pid}-rib", x, y, 60.0 * k + 30.0, rib, 1.5, c, base, z0=B["f3_hi"])
                cyl(f"{pid}-top", x, y, r, B["ribs_hi"], B["f4_hi"], c, base)
                annulus(f"{pid}-ring", x, y, 6.33, r, B["f4_hi"] + 0.05, c, ring)
            elif cl == "bumper-cap":
                x, y, r = s["circle"]
                cyl(f"{pid}-cap-rim", x, y, r + 0.35, B["f4_hi"], B["f4_hi"] + 0.1, c, gold)
                cyl(f"{pid}-cap", x, y, r, B["f4_hi"] + 0.1, B["f4_hi"] + 0.2, c, cap)
            elif cl == "bumper-dot": prism(f"{pid}-dot", s["pts"], B["f3_hi"], B["f3_hi"] + 0.2, c, gold)
            elif cl == "bumper-star": prism(f"{pid}-star", s["pts"], B["f4_hi"] + 0.2, B["f4_hi"] + 0.35, c, star)
    # the conductive foil under the bumpers lies flush on the board, a satin silver against the painted wood
    for s in placed("bumper-plate"):
        o = prism("bumper-plate", s["pts"], 0, H["foil"], c, foil_material())
        for k in (1, 2, 3):
            bo = o.modifiers.new(f"bumper-{k}", "BOOLEAN"); bo.object = bpy.data.objects[f"bumper-{k}-foil-cutter"]; bo.operation = "DIFFERENCE"


def blue_height():
    """The blue ramp's floor height (mm) over a point of its course; its end rests on the left inlane's separator."""
    blue_line = [tuple(p) for p in by("ramp-blue", "ramp-shine")[0]["pts"]]
    f = polyline_param(blue_line)
    crown_first = blue_line[0][1] < 150
    pb = profile([(0, 30), (0.15, 42), (0.6, 44), (1.0, H["rail"] + H["ramp_floor"])])
    return lambda x, y: pb(f(x, y) if crown_first else 1 - f(x, y))


def build_posts_and_rubbers():
    c = coll("posts")
    post = mat("post", "#17171b", rough=0.4, coat=0.5)
    rubber = mat("rubber", "#f4f1ea", rough=0.7)
    sleeve = mat("sleeve", "#e9eef2", rough=0.6)
    blue_z = blue_height()
    for s in SHAPES:
        ch = s["chain"]
        if not ch or ch[0] != "layer-0": continue
        cl = s["cls"]
        if cl == "post" and "circle" in s:
            if "inlane-separators" in ch: continue          # the red inlane separators carry no posts
            x, y, r = s["circle"]
            # where the blue ramp's low end lies on the post, the post is the screw that holds it: it runs up
            # through the ramp's floor, and its head sits on top
            under = any(inside((x + dx, y + dy), o) for o in BLUE_FLOOR for dx, dy in ((0, 0), (3, 0), (-3, 0), (0, 3), (0, -3)))
            top = blue_z(x, y) if under and blue_z(x, y) - H["ramp_floor"] < H["post"] + 2.2 else H["post"]
            # a slingshot's screw runs up through its cover, and its head sits on top
            if any(inside((x, y), t["pts"]) for t in SHAPES if t["cls"] == "sling"):
                top = H["sling"] + 0.2
            cyl("post", x, y, r, 0, top, c, post, verts=24)
            cyl("post-head", x, y, r + 0.6, top, top + 1.2, c, post, verts=24)
        elif cl == "sleeve":
            # a round rubber ring stretched around the two posts: it lies on the posts and reaches out as far as
            # the drawing's band, so its cord is the band's width less the post, halved
            (ax, ay), (bx, by_) = s["pts"][0], s["pts"][-1]
            rp = min((t["circle"] for t in SHAPES if t["cls"] == "post" and "circle" in t),
                     key=lambda q: math.hypot(q[0] - ax, q[1] - ay))[2]
            cord = (s["sw"] / 2 - rp) / 2
            d = math.atan2(by_ - ay, bx - ax); r = rp + cord
            loop = [(bx + r * math.cos(d - math.pi / 2 + math.pi * i / 12), by_ + r * math.sin(d - math.pi / 2 + math.pi * i / 12)) for i in range(13)]
            loop += [(ax + r * math.cos(d + math.pi / 2 + math.pi * i / 12), ay + r * math.sin(d + math.pi / 2 + math.pi * i / 12)) for i in range(13)]
            tube("rubber", loop, cord, H["rubber_z"], c, rubber, closed=True)
        elif cl == "rubber":
            # the drawing's stroke is the rubber's full width
            tube("rubber", s["pts"], s["sw"] / 2, H["rubber_z"], c, rubber, closed=s["closed"])
        elif cl == "comb-back":
            ribbon("fence", s["pts"], s["sw"], 0, H["fence"], c, fence_material(), uv=True, top_fn=fence_top)
        elif cl == "blade-wood":
            # the lane guide's blade stays under the green board that passes over it
            ribbon("blade", s["pts"], s["sw"], 0, H["blade"], c, mat("wood-dark", "#7a4a26", rough=0.6),
                   top_fn=lambda x, y: max(0.0, min(H["blade"], green_z(x, y) - H["ramp_floor"] - 0.5)))
        elif cl == "dome":
            # the bell hangs from a center rod, clear of the board so it can ring: a tube with the dome on top
            x, y, r = s["circle"]
            dome = mat("bell", "#1a1a1e", rough=0.15, metal=0.9)
            cyl("bell-rod", x, y, 1.5, 0, H["bell_top"], c, mat("rod", "#9aa0a8", rough=0.4, metal=1.0), 16)
            me = bpy.data.meshes.new("bell-tube"); bm = bmesh.new()
            bmesh.ops.create_cone(bm, cap_ends=False, segments=48, radius1=r / 10, radius2=r / 10, depth=(H["bell_body"] - H["bell_rim"]) / 10)
            bm.to_mesh(me); bm.free()
            o = link(bpy.data.objects.new("bell-tube", me), c, dome)
            o.location = W(x, y, (H["bell_rim"] + H["bell_body"]) / 2)
            for p in o.data.polygons: p.use_smooth = True
            o.modifiers.new("shell", "SOLIDIFY").thickness = 0.08
            me = bpy.data.meshes.new("bell-dome"); bm = bmesh.new()
            bmesh.ops.create_uvsphere(bm, u_segments=48, v_segments=24, radius=r / 10)
            bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.z < -1e-4], context="VERTS")
            bm.to_mesh(me); bm.free()
            o = link(bpy.data.objects.new("bell-dome", me), c, dome)
            o.location = W(x, y, H["bell_body"]); o.scale.z = (H["bell_top"] - H["bell_body"]) / r
            for p in o.data.polygons: p.use_smooth = True
            o.modifiers.new("shell", "SOLIDIFY").thickness = 0.08
            sphere("bell-screw", x, y, H["bell_top"], 1.6, c, mat("screw", "#d9dde2", rough=0.2, metal=1.0)).scale.z = 0.5
            # the conductive tape around it, flush with the board
            annulus("bell-foil", x, y, r + 0.05, r + 6.06, H["foil"], c, foil_material())
        elif cl == "target-base" and any(ch.startswith("target-") for ch in ch):
            prism("target-base", s["pts"], 0, 15.0, c, mat("target-base", "#2b2d33", rough=0.5))
        elif cl == "target-face" and any(ch.startswith("target-") for ch in ch):
            prism("target-face", s["pts"], 2.0, 13.0, c, mat("target-face", "#f2c230", rough=0.35, coat=0.5))
        elif cl == "ir-eye" and "ir-sensors" in ch:
            # a small hole in the board where the IR sensor looks up through it
            x, y, r = s["circle"]
            cyl("ir-hole", x, y, 1.5, 0.0, 0.05, c, mat("pit", "#050506", rough=0.9), 16)
        elif cl == "wall-red":
            # the red wall between the shooter lane and the playfield runs on down to the front wall
            xs = [p[0] for p in s["pts"]]; ys = [p[1] for p in s["pts"]]
            wall = [(min(xs), min(ys)), (max(xs), min(ys)), (max(xs), 427.3), (min(xs), 427.3)]
            prism("shooter-wall", wall, 0, H["shooter_wall"], c, mat("shooter-wall", "#b8323a", rough=0.35, coat=0.6))


# side views of the two figures, (u, z) in mm, u along the plane
ELEPHANT = [(-4.5, 0), (-1.2, 0), (-0.8, 4), (0.8, 4), (1.2, 0), (4.5, 0), (4.6, 6), (5.4, 9.5), (4.8, 12.5), (3.2, 14),
            (4.4, 15.2), (6.2, 17.2), (7.2, 20.2), (6.2, 20.6), (5.0, 18.4), (3.2, 17.0), (1.6, 18.2), (-0.6, 18.2),
            (-2.4, 16.6), (-2.8, 14.6), (-4.8, 13.4), (-5.4, 10.6), (-4.4, 9.0), (-4.9, 6.0)]
# the seal sits facing -u, its nose up under the ball, its tail curled up behind it
# the elephant's paint: a red blanket with a gold trim on its back, a red headdress with a gold plume, the eye
ELEPHANT_BLANKET = [(-4.9, 6.4), (3.4, 6.4), (3.1, 11.6), (-4.9, 11.6)]
ELEPHANT_TRIM = [(-4.9, 5.8), (3.5, 5.8), (3.5, 6.8), (-4.9, 6.8)]
ELEPHANT_HEADDRESS = [(-0.6, 17.4), (1.8, 17.4), (1.4, 18.8), (-0.2, 18.8)]
ELEPHANT_PLUME = [(0.0, 18.6), (1.2, 18.6), (0.6, 21.0)]
ELEPHANT_EYE = [(3.2 + 0.5 * math.cos(2 * math.pi * k / 12), 15.4 + 0.5 * math.sin(2 * math.pi * k / 12)) for k in range(12)]
SEAL = [(-4.2, 0), (5.0, 0), (6.6, 1.2), (8.0, 3.2), (9.6, 6.4), (8.6, 6.2), (7.6, 4.8), (6.2, 4.4), (4.4, 4.6),
        (2.6, 6.2), (1.6, 8.4), (1.2, 10.6), (0.2, 12.2), (-1.4, 12.8), (-3.2, 13.8), (-4.2, 13.4), (-3.4, 11.6),
        (-3.6, 9.4), (-3.2, 7.2), (-4.0, 5.0), (-5.6, 3.0), (-4.6, 2.4), (-3.8, 2.8)]
SEAL_HAT = [(-1.6, 12.4), (1.4, 11.4), (1.0, 16.0)]
SEAL_BOW = [(-3.6, 8.2), (-2.4, 8.9), (-1.2, 8.2), (-1.2, 9.6), (-2.4, 8.9), (-3.6, 9.6)]


def build_lower():
    c = coll("lower")
    black = mat("flipper", "#121215", rough=0.35, coat=0.6)
    red = mat("sling-red", "#b8323a", rough=0.35, coat=0.6)
    panel = mat("sling-panel", "#5a6370", rough=0.5)
    disc = mat("disc-red", "#c92a35", rough=0.3, coat=0.7)
    rail = mat("rail-red", "#c42c38", rough=0.35, coat=0.6)
    gem = mat("gem", "#8e99a6", rough=0.2, metal=0.5)
    gem_y = mat("gem-yellow", "#f1c94a", rough=0.2, emit="#f1c94a", strength=0.5)
    screw = mat("screw", "#d9dde2", rough=0.2, metal=1.0)
    rubber = mat("rubber", "#f4f1ea", rough=0.7)
    for s in SHAPES:
        ch = s["chain"]
        if not ch: continue
        cl = s["cls"]
        if "flippers" in ch or "inlane-rails" in ch or "inlane-separators" in ch:
            if cl == "flipper": prism("flipper", s["pts"], 0.5, H["flipper"], c, black)
            elif cl == "sling": prism("sling", s["pts"], H["sling_lo"], H["sling"], c, red)
            elif cl == "sling-rubber":
                # under the wooden cover the rubber runs once around three round clear spacers, one at each of the
                # slingshot's screws. The drawing gives the rubber's width and its strand along the kicking face,
                # and the strand's distance from the screws, less half that width, is the spacers' radius.
                r = s["sw"] / 2
                (ax, ay), (bx, by_) = s["pts"][0], s["pts"][-1]
                mid = ((ax + bx) / 2, (ay + by_) / 2)
                sling = min((t for t in SHAPES if t["cls"] == "sling"),
                            key=lambda t: math.dist(mid, (sum(p[0] for p in t["pts"]) / len(t["pts"]), sum(p[1] for p in t["pts"]) / len(t["pts"]))))
                screws = [t["circle"] for t in SHAPES if t["cls"] == "post" and "circle" in t and inside(t["circle"][:2], sling["pts"])]
                d = Vector((bx - ax, by_ - ay)).normalized()
                spacer = min(abs((x - ax) * d.y - (y - ay) * d.x) for x, y, _ in screws) - r
                for x, y, _ in screws:
                    cyl("sling-spacer", x, y, spacer, 0, H["sling_lo"], c,
                        mat("spacer-clear", "#eef3f6", rough=0.15, transmission=0.6), verts=48)
                # the ring is the hull of the spacers, widened by half the rubber's width
                around = [(x + (spacer + r) * math.cos(k * math.tau / 96), y + (spacer + r) * math.sin(k * math.tau / 96))
                          for x, y, _ in screws for k in range(96)]
                tube("sling-rubber", hull(around), r, H["rubber_z"], c, rubber, closed=True)
            elif cl == "sling-panel": prism("sling-panel", s["pts"], H["sling"], H["sling"] + 0.2, c, panel)
            elif cl == "disc":
                x, y, r = s["circle"]
                cyl("elephant-stand", x, y, r, H["sling"], H["sling"] + 2.0, c, disc)
                mirror = x > 108.5
                flip = (lambda pts: [(-u, z) for u, z in pts]) if not mirror else (lambda pts: pts)
                z0 = H["sling"] + 2.0
                figure("elephant", x, y, 0.0, flip(ELEPHANT), 3.0, c, mat("elephant", "#9097a0", rough=0.5), z0=z0)
                for name, prof, colr in (("elephant-blanket", ELEPHANT_BLANKET, "#c8323a"), ("elephant-trim", ELEPHANT_TRIM, "#e3c46c"),
                                         ("elephant-headdress", ELEPHANT_HEADDRESS, "#c8323a"), ("elephant-plume", ELEPHANT_PLUME, "#e3c46c"),
                                         ("elephant-eye", ELEPHANT_EYE, "#141416")):
                    figure(name, x, y, 0.0, flip(prof), 3.4, c, mat(name, colr, rough=0.45), z0=z0)
            elif cl == "rail": prism("rail", s["pts"], 0, H["rail"], c, rail)
            elif cl == "rail-gem": prism("rail-gem", s["pts"], H["rail"], H["rail"] + 0.3, c, gem)
            elif cl == "rail-gem-y": prism("rail-gem", s["pts"], H["rail"], H["rail"] + 0.3, c, gem_y)
            elif cl == "screw" and "circle" in s: cyl("flipper-screw", *s["circle"], H["flipper"], H["flipper"] + 0.4, c, screw, 24)
    # the apron is a plate the ball rolls under on its way into the drain
    stripes = stripes_material("apron", "#e2505b", "#c8323f", 14.9)
    band = mat("apron-band", "#3c4250", rough=0.4)
    side = mat("apron-side", "#555c69", rough=0.4)
    star = mat("apron-star", "#d9b65a", rough=0.3, metal=0.8)
    for s in placed("apron"):
        cl = s["cls"]
        if cl == "apron": prism("apron", s["pts"], H["apron_lo"], H["apron_hi"], c, stripes)
        elif cl == "apron-band": prism("apron-band", s["pts"], 0, H["apron_hi"] + 2, c, band)
        elif cl == "apron-side": prism("apron-side", s["pts"], H["apron_hi"], H["apron_hi"] + 0.4, c, side)
        elif cl == "apron-star": prism("apron-star", s["pts"], H["apron_hi"] + 2, H["apron_hi"] + 2.3, c, star)
    for s in placed("drain-wall-lower"):
        if s["cls"] == "wood-wall": ribbon("drain-wall", s["pts"], s["sw"], 0, H["apron_lo"], c, wood())
    # the rotating seal: blue rotor with its rubber, the clear crossbar, the seal balancing a ball
    rotor_m = mat("seal-rotor", "#17171b", rough=0.4)
    rotor_top = mat("seal-rotor-top", "#3d8fd6", rough=0.3, coat=0.6)
    rod = mat("rod", "#9aa0a8", rough=0.4, metal=1.0)
    x, y = 120.43, 169.65
    # the rotor turns on a metal rod, clear of the board
    z = 3.5                                              # the rotor's underside, just above the ball's middle
    cyl("seal-rod", x, y, 1.2, 0, z + 5.6, c, rod, 16)
    for s in placed("seal"):
        cl = s["cls"]
        if cl == "seal-rotor":
            # wood, the rubber band between, blue wood, then the seal
            prism("seal-rotor", s["pts"], z, z + 1.5, c, rotor_m)
            prism("seal-rotor-top", s["pts"], z + 3.0, z + 4.5, c, rotor_top)
        elif cl == "seal-rubber": tube("seal-rubber", s["pts"], s["sw"] / 2, z + 2.25, c, mat("clear-band", "#f2f4f6", rough=0.2, transmission=0.7), closed=True)
        elif cl == "seal-pivot": cyl("seal-pivot", *s["circle"], z + 4.5, z + 5.1, c, screw, 24)
    ang = 36.6
    figure("seal", x, y, ang, SEAL, 3.0, c, mat("seal", "#e4e4e0", rough=0.5), z0=z + 4.5)
    figure("seal-hat", x, y, ang, SEAL_HAT, 3.4, c, mat("seal-hat", "#3d6fc4", rough=0.4), z0=z + 4.5)
    figure("seal-bow", x, y, ang, SEAL_BOW, 3.4, c, mat("seal-bow", "#d8323a", rough=0.4), z0=z + 4.5)
    a = math.radians(ang)
    sphere("seal-pompom", x + 1.0 * math.cos(a), y + 1.0 * math.sin(a), z + 4.5 + 16.3, 0.9, c, mat("seal-bow", "#d8323a"))
    sphere("seal-ball", x - 3.9 * math.cos(a), y - 3.9 * math.sin(a), z + 4.5 + 16.2, 2.3, c, mat("seal-ball", "#e8c84a", rough=0.45))


def build_shooter_lane():
    c = coll("shooter")
    lane = mat("shooter-floor", "#3a3f4b", rough=0.4)
    lit = mat("shooter-arrow", "#f2e7c4", rough=0.3, emit="#ffe9b0", strength=1.5)
    floor_z = shooter_z
    for s in placed("shooter-lane-floor"):
        if s["cls"] == "shooter":
            # the lane's floor ends where the gray lane begins, flush with it
            pts = [(x, max(y, GRAY_START_Y)) for x, y in s["pts"]]
            prism("shooter-floor", pts, -1.6, 0.0, c, lane, zfn=shooter_z)
            # the floor's outline has its corners only at the lane's two ends, so its top is a straight climb
            # between them; the arrows lie on that climb
            y0, y1 = min(p[1] for p in pts), max(p[1] for p in pts)
            z0, z1 = shooter_z(0.0, y0), shooter_z(0.0, y1)
            floor_z = lambda x, y: z0 + (z1 - z0) * (y - y0) / (y1 - y0)
    for s in SHAPES:
        if s["chain"] and any(ch.startswith("shooter-arrow-") for ch in s["chain"]):
            prism("shooter-arrow", s["pts"], 0.05, 0.2, c, lit, zfn=floor_z, cap_only=True)


def build_sides():
    """The side stacks: wood, clear plastic with the perimeter lights inside, wood, the gray cover on top."""
    c = coll("sides")
    plastic = mat("perimeter", "#eef3f6", rough=0.15, transmission=0.6, emit="#ffe2a8", strength=1.2)
    gray = mat("panel-gray", "#717a8a", rough=0.5)
    paint = {"paint-red": mat("paint-red", "#e2505e", rough=0.4), "paint-gold": mat("paint-gold", "#e3c46c", rough=0.3, metal=0.6),
             "chev-red": mat("chev-red", "#e2505e", rough=0.4), "chev-gold": mat("chev-gold", "#e3c46c", rough=0.4),
             "chev-green": mat("chev-green", "#3fa66b", rough=0.4)}
    for pid in ("guide-left", "guide-right", "arch-clear"):
        for s in by(pid, "plastic"):
            prism(pid + "-wood", s["pts"], 0, H["wood_lo"], c, wood())
            prism(pid + "-plastic", s["pts"], H["wood_lo"], H["plastic"], c, plastic)
    # behind the apron the outlanes are walled by the same stack, wood, clear plastic, wood and the apron on top;
    # the perimeter strips end at the apron, so this plastic carries no light
    dark_plastic = mat("stack-plastic", "#eef3f6", rough=0.15, transmission=0.6)
    for pid in ("outlane-left", "outlane-right"):
        for s in by(pid, "clear"):
            prism(pid + "-wood", s["pts"], 0, H["wood_lo"], c, wood())
            prism(pid + "-plastic", s["pts"], H["wood_lo"], H["plastic"], c, dark_plastic)
            prism(pid + "-wood-top", s["pts"], H["plastic"], H["wood_hi"], c, wood())
    # between a side's guide and its outlane the strip runs on along the red wall, the cabinet's on the left and
    # the shooter lane's on the right, at the plastic's height
    for name, x0, x1, y0, y1 in (("gap-left", 0.0, 3.0, 296.0, 334.0), ("gap-right", 213.8, 216.8, 296.0, 337.0)):
        prism(name + "-plastic", [(x0, y0), (x1, y0), (x1, y1), (x0, y1)], H["wood_lo"], H["plastic"], c, plastic)
    for pid in ("cover-left", "panel-corner-right", "panel-guide-right"):
        for s in placed(pid):
            cl = s["cls"]
            if cl == "panel":
                prism(pid + "-wood", s["pts"], H["plastic"], H["wood_hi"], c, wood())
                prism(pid, s["pts"], H["wood_hi"], H["cover"], c, gray)
            elif cl in paint:
                prism(pid + "-paint", s["pts"], H["cover"], H["cover"] + 0.15, c, paint[cl], cap_only=True)
    for s in placed("block-top-left"):
        if s["cls"] == "block": prism("block-top-left", s["pts"], 0, H["cover"], c, mat("block-red", "#a3232c", rough=0.4))


def fire_hoop(c, x, y, z_floor, span_deg=90.0):
    """The burning hoop on the blue ramp: an upright ring across the ramp with flames around its top."""
    r_in, r_out, flames = 9.5, 12.0, 9
    n = 72
    outer = []
    for i in range(n + 1):
        t = math.pi * i / n
        frac = (i / n) * flames % 1.0
        rr = r_out + (5.0 * (1 - abs(2 * frac - 1)) ** 1.5 if 0.15 < t < math.pi - 0.15 else 0.0)
        outer.append((rr * math.cos(t), r_in + rr * math.sin(t)))
    inner = [(r_in * math.cos(math.pi * i / n), r_in + r_in * math.sin(math.pi * i / n)) for i in range(n, -1, -1)]
    # right leg up, over the top with the flames, left leg down, then back along the opening
    prof = [(r_out, 0.0)] + outer + [(-r_out, 0.0), (-r_in, 0.0)] + inner + [(r_in, 0.0)]
    flame = mat("fire-hoop", "#c8322a", rough=0.4, emit="#ff7a1a", strength=2.5)
    return figure("fire-hoop", x, y, span_deg, prof, 2.0, c, flame, z0=z_floor)


def entrance_walls(c, floor_z):
    """The side plates of the blue ramp's entrance lane, from the photo of the part: on the right a gray plate
    with a red rim, a salmon wave crest along its top, small lit windows, jester caps and circus tents; on the
    left a taller red plate with a flame crest; under the high end a gold stand."""
    red = mat("lane-red", "#b3242d", rough=0.5)
    gray = mat("lane-gray", "#6b6f78", rough=0.55)
    salmon = mat("lane-salmon", "#ec8f7c", rough=0.5)
    gold = mat("lane-gold", "#c9a43a", rough=0.45, metal=0.3)
    lamp = mat("lane-window", "#e8c35a", rough=0.3, emit="#ffd27a", strength=2.5)
    lines = []
    for sh in by("ramp-blue", "center-wall"):
        mid, width = centerline(sh["pts"])
        lines.append((sum(p[0] for p in mid) / len(mid), mid, max(width, 1.5) + 0.5))   # covers the floor's edge
    lines.sort()
    (_, left, wl), (_, right, wr) = lines[0], lines[-1]
    def outward(line, sign):
        """The offset (mm) to the side away from the lane, for the line's own direction of travel."""
        i = len(line) // 2
        probe = offset_line(line, 1.0, False)[i]
        return 1.0 if (probe.x - line[i][0]) * sign > 0 else -1.0
    # the left plate: red, its crest a row of flames that grows toward the high end
    def flame_top(x, y):
        t = max(0.0, min(1.0, (146.5 - y) / 64.5))
        k = ((146.5 - y) / 9.0) % 1.0
        return 8.5 + 3.0 * t + 3.5 * (1.0 - abs(2.0 * k - 1.0)) ** 1.6
    ribbon("lane-wall-left", left, wl, -3.0, 8.5, c, red, zfn=floor_z, top_fn=flame_top)
    # the right plate: red body, gray face on its outer side, salmon waves on top
    ribbon("lane-wall-right", right, wr, -3.0, 7.5, c, red, zfn=floor_z)
    out = outward(right, 1.0)
    face = offset_line(right, out * (wr / 2 + 0.05), False)
    ribbon("lane-wall-face", [(p.x, p.y) for p in face], 0.1, -2.0, 6.6, c, gray, zfn=floor_z)
    wave = lambda x, y: 7.5 + 2.6 * abs(math.sin(math.pi * (146.5 - y) / 9.0))
    ribbon("lane-wall-crest", right, wr, 7.4, 7.5, c, salmon, zfn=floor_z, top_fn=wave)
    # along the gray face: lit windows, and between them jester caps and tents in turn
    acc = [0.0]
    for p0, p1 in zip(right, right[1:]): acc.append(acc[-1] + math.dist(p0, p1))
    cap = [(-1.8, 0), (1.8, 0), (2.4, 2.2), (1.0, 1.2), (0.0, 2.8), (-1.0, 1.2), (-2.4, 2.2)]
    tent = [(-1.9, 0), (1.9, 0), (1.9, 1.2), (0.0, 3.2), (-1.9, 1.2)]
    k = 0
    for d in range(8, int(acc[-1]) - 4, 9):
        i = min(range(len(acc)), key=lambda j: abs(acc[j] - d))
        a0, a1 = right[max(i - 1, 0)], right[min(i + 1, len(right) - 1)]
        ang = math.degrees(math.atan2(a1[1] - a0[1], a1[0] - a0[0]))
        fx, fy = face[i].x, face[i].y
        nx, ny = -math.sin(math.radians(ang)), math.cos(math.radians(ang))
        sgn = 1.0 if ((fx - right[i][0]) * nx + (fy - right[i][1]) * ny) > 0 else -1.0
        x, y = fx + nx * sgn * 0.1, fy + ny * sgn * 0.1
        z = floor_z(x, y)
        if k % 2 == 0:
            figure("lane-window", x, y, ang, [(-0.8, 0), (0.8, 0), (0.8, 1.6), (-0.8, 1.6)], 0.15, c, lamp, z0=z + 1.8)
        else:
            figure("lane-icon", x, y, ang, cap if (k // 2) % 2 == 0 else tent, 0.15, c, salmon if (k // 2) % 2 == 0 else red, z0=z + 1.2)
        k += 1
    # the lane's feet: two gold plates along the lane, each with a tab in one of the inner slots of the board
    # (135.5 and 144.8 mm across, 120.1 mm down, 9.8 mm long), their tops carrying the floor
    for fx in (135.5, 144.8):
        prof = [(8.0, 0.0), (4.9, 0.0), (4.9, -3.0), (-4.9, -3.0), (-4.9, 0.0), (-9.0, 0.0), (-9.0, 3.5), (-14.0, 3.5),
                (-14.0, 0.0), (-20.0, 0.0)]
        prof += [(u, floor_z(fx, 120.1 + u) - H["ramp_floor"]) for u in (-20.0, -14.0, -8.0, -2.0, 4.0, 8.0)]
        figure("lane-stand", fx, 120.1, 90.0, prof, 2.0, c, gold)
    # a low picket fence in a U from one outer slot to the other, round the back of the lane: it closes the space
    # under the high part, so no ball rolls in beneath the ramp
    arc = [(140.1 - 10.7 * math.cos(math.pi * k / 16), 102.0 - 10.7 * math.sin(math.pi * k / 16)) for k in range(17)]
    fence_path = [(129.4, 118.4), (129.4, 108.0)] + arc + [(150.8, 108.0), (150.8, 118.4)]
    ribbon("lane-fence", fence_path, 1.5, -2.0, 5.0, c, fence_material(), uv=True)


def build_ramps():
    c = coll("ramps")

    def ramp(pid, floor_cls, wall_cls, zfn, color_floor, color_wall, wall_h=H["ramp_wall"], wall_lo=-H["ramp_floor"],
             cuts=1, step=None):
        fm = mat(pid + "-floor", color_floor, rough=0.3, coat=0.5)
        wm = mat(pid + "-wall", color_wall, rough=0.3, coat=0.5)
        outlines = [sh["pts"] for sh in by(pid, floor_cls)]
        if floor_cls == "launch-floor":
            # drawn even-odd: the largest outline is the floor, the smaller one the slot into the rollover lanes
            def area(ps): return abs(sum(ps[i][0] * ps[i - 1][1] - ps[i - 1][0] * ps[i][1] for i in range(len(ps)))) / 2
            outlines.sort(key=area, reverse=True)
            fl = surface(pid + "-floor", outlines[0], zfn, H["ramp_floor"], c, fm)
            for h_pts in outlines[1:]:
                cut = prism(pid + "-hole", h_pts, -H["ramp_floor"] - 3, 3, c, None, zfn=zfn)
                cut.hide_render = True; cut.hide_viewport = True
                bo = fl.modifiers.new("hole", "BOOLEAN"); bo.object = cut; bo.operation = "DIFFERENCE"
        else:
            for pts in outlines:
                surface(pid + "-floor", pts, zfn, H["ramp_floor"], c, fm, cuts=cuts, step=step)
        for s in by(pid, wall_cls):
            # a wall is drawn as a thin outline around it; it is built along its middle line
            mid, width = centerline(s["pts"])
            ribbon(pid + "-wall", mid, max(width, 1.0), wall_lo, wall_h, c, wm, zfn=zfn)

    # the blue ramp climbs from the playfield through the fire hoop, turns, and ends in the air above the left inlane
    blue_z = blue_height()
    # its rails are flat strips lying 3 mm above the floor at the ball's middle, held only by the hoops
    ramp("ramp-blue", "ramp-blue", "blue-wall", blue_z, "#3f7fc4", "#1d3c7a", wall_h=6.0, wall_lo=3.0)
    # the entrance lane leaves the playfield flush and level, a hair above it so the two surfaces do not fight,
    # and meets the blue ramp's floor at the fire hoop with that floor's own slope, 0.2 mm a mm, so the climb
    # has no bend anywhere: a cubic through both ends with both slopes
    def centre_z(x, y):
        run = 146.5 - 85.5                      # to the seam, where the blue floor starts at 30 mm
        u = max(0.0, min(1.0, (146.5 - y) / run))
        return 0.02 + 29.98 * (3 * u * u - 2 * u ** 3) + 0.2 * run * (u ** 3 - u * u)
    ramp("ramp-blue", "ramp-blue-dark", "none", centre_z, "#4fb0e0", "#1f4674", cuts=5)   # a dense net for the curve
    entrance_walls(c, centre_z)
    # the chevrons are flat; lifted 0.15 mm, they clear the floor's bend under them
    for s in by("ramp-blue", "chevron"): prism("chevron", s["pts"], 0.15, 0.35, c, mat("chevron", "#22476f"), zfn=centre_z)
    crown = [p for s in by("ramp-blue", "crown") for p in s["pts"]]
    cx, cy = centroid(crown)
    fire_hoop(c, cx, cy, centre_z(cx, cy), span_deg=0.0)
    # painted along the course: the fire hoop at the top, then red, yellow, yellow and green-gold
    hoop_paint = {"ramp-blue-bracket": mat("hoop-red", "#c8323a", rough=0.4), "ramp-blue-ring-1": mat("hoop-yellow", "#f1c94a", rough=0.4),
                  "ramp-blue-ring-2": mat("hoop-yellow", "#f1c94a", rough=0.4), "ramp-blue-ring-3": mat("hoop-green-gold", "#a8a03c", rough=0.35, metal=0.4)}
    for cls in ("ring", "bracket-blue"):
        for s in by("ramp-blue", cls):
            material = hoop_paint[next(ch for ch in s["chain"] if ch in hoop_paint)]
            pts = s["pts"]
            cx, cy = centroid(pts)
            sxx = sum((x - cx) ** 2 for x, _ in pts); syy = sum((y - cy) ** 2 for _, y in pts)
            sxy = sum((x - cx) * (y - cy) for x, y in pts)
            a = 0.5 * math.atan2(2 * sxy, sxx - syy)
            r_out = max(abs((x - cx) * math.cos(a) + (y - cy) * math.sin(a)) for x, y in pts)
            # a closed ring across the ramp, its centre just above the floor; the ramp's walls sit in it
            h = blue_z(cx, cy)
            # the ring the ramp's segments plug into: the floor rests on its flat bottom, the walls sit inside it,
            # and above the floor it leaves room for the ball
            # the drawing gives the hoop's outer radius; its inner edge meets the walls' outer faces at the hoop's
            # widest point, just above the floor, and leaves the ball room above
            # its clamp reaches 2.5 mm under the ramp's floor; where the inlanes run under the ramp's end it stops
            # 10 mm above the board, clear of the 9 mm ball
            cut = max(h - H["ramp_floor"] - 2.5, min(h - H["ramp_floor"], 10.0))
            flat_ring("ramp-hoop", cx, cy, math.degrees(a), h + 2.0, r_out - 2.5, r_out, cut, 2.5, 2.2, c, material)

    # the green track runs on top of the U lane's fence after a short climb from its entry
    ramp("ramp-orbit-left", "ramp-green", "green-wall", green_z, "#3f5e48", "#2f4a39", wall_h=H["ramp_wall"] / 2,
         step=2.0)

    # the gray launch lane continues the shooter lane, crosses over the green track and ends under the blue ramp
    # a constant, flat climb along its course, continuing the shooter lane's floor
    gray_course = polyline_param([(224.0, GRAY_START_Y), (222.0, 90.0), (208.0, 50.0), (172.0, 38.0), (132.0, 52.0)])
    gray_start = shooter_z(224.0, GRAY_START_Y)
    gray_z = lambda x, y: gray_start + gray_course(x, y) * (H["gray_end"] - gray_start)
    # its walls stay low, so they pass under the blue ramp where the lane ends beneath it
    ramp("ramp-orbit-right", "launch-floor", "launch-wall", gray_z, "#3a404b", "#6a7382", wall_h=H["ramp_wall"] / 2)

    # the crocodile: the ball climbs its lower jaw, turns in a half loop at the back of the mouth and leaves
    # on the red track, under the open upper jaw that carries the eyes
    green = mat("crocodile", "#3f6b45", rough=0.5)
    red = mat("track-red", "#e0414c", rough=0.45)
    kick = clean(by("ramp-kicker", "kicker")[0]["pts"])
    hinge = ((kick[0][0] + kick[1][0]) / 2, (kick[0][1] + kick[1][1]) / 2)
    snout = max(kick, key=lambda q: q[1])
    span = math.dist(hinge, snout)
    u = Vector(((hinge[0] - snout[0]) / span, (hinge[1] - snout[1]) / span))
    jaw_top, loop_r = 10.0, 8.0
    def croc_z(x, y):
        """The lower jaw leaves the playfield flush and level at the snout, a hair above it, and climbs without a
        bend to the hinge, where it has the slope of a straight jaw of the same height."""
        v = max(0.0, min(1.0, ((x - snout[0]) * u.x + (y - snout[1]) * u.y) / span))
        return 0.02 + (jaw_top - 0.02) * (3 * v * v - 2 * v ** 3) + jaw_top * (v ** 3 - v * v)
    surface("crocodile-jaw", kick, croc_z, 2.0, c, green, cuts=4)
    # the head is closed down to the board: a block fills it under the jaw, so no ball rolls in beneath it from
    # behind; its outline in steps of 1 mm keeps its top inside the 2 mm jaw along the jaw's curve. It stands
    # 1.5 mm inside the jaw's edge, so a ball beside the head rolls with its lower half under the jaw. The jaw's
    # back corner lies closer to the rubber at post 022 than the ball is wide, and a ball held off by a block
    # flush with the edge rests there.
    dense = []
    for a, b in zip(kick, kick[1:] + kick[:1]):
        k = max(1, math.ceil(math.dist(a, b)))
        dense += [(a[0] + (b[0] - a[0]) * i / k, a[1] + (b[1] - a[1]) * i / k) for i in range(k)]
    def area(ps): return abs(sum(ps[i][0] * ps[i - 1][1] - ps[i - 1][0] * ps[i][1] for i in range(len(ps))))
    dense = min((offset_line(dense, d, True) for d in (1.5, -1.5)), key=area)
    prism("crocodile-base", [(p[0], p[1]) for p in dense], 0.0, 0.0, c, green, hi_fn=lambda x, y: max(0.05, croc_z(x, y) - 2.1))
    half = math.dist(kick[0], kick[1]) / 2
    ties = [t["pts"] for t in by("wire-ramp", "tie")]
    ties.sort(key=lambda t: (t[0][1] + t[-1][1]) / 2)
    first = ((ties[0][0][0] + ties[0][-1][0]) / 2, (ties[0][0][1] + ties[0][-1][1]) / 2)
    a2 = Vector(hinge) - u * loop_r
    z_a = croc_z(a2.x, a2.y)
    z_top = z_a + 2 * loop_r
    # the half loop and the straight back over the jaw to the track, as (point, tangent, up) in mm
    u3 = Vector((u.x, u.y, 0.0)); Z = Vector((0, 0, 1)); side = u3.cross(Z).normalized()
    centre = Vector((a2.x, a2.y, z_a + loop_r))
    path = []
    for k in range(25):
        t = math.pi * k / 24
        tan = (u3 * math.cos(t) + Z * math.sin(t)).normalized()
        path.append((centre + (u3 * math.sin(t) - Z * math.cos(t)) * loop_r, tan, side.cross(tan)))
    top = centre + Z * loop_r
    end3 = Vector((first[0], first[1], z_top))
    for k in range(1, 9):
        q = top.lerp(end3, k / 8)
        path.append((q, (end3 - top).normalized(), Z))
    def sweep(name, part, material, walls, w=6.0):
        """A track along part of the path: a floor 2 w wide, with or without its two side walls."""
        me = bpy.data.meshes.new(name); bm = bmesh.new()
        rows = []
        for q, tan, up in part:
            sd = tan.cross(up).normalized()
            sec = [q + sd * w + up * 4, q + sd * w, q - sd * w, q - sd * w + up * 4] if walls else [q + sd * w, q - sd * w]
            rows.append([bm.verts.new(W(v.x, v.y, v.z)) for v in sec])
        for r0, r1 in zip(rows, rows[1:]):
            for m in range(len(r0) - 1):
                bm.faces.new((r0[m], r0[m + 1], r1[m + 1], r1[m]))
        bm.to_mesh(me); bm.free()
        o = link(bpy.data.objects.new(name, me), c, material)
        o.modifiers.new("solid", "SOLIDIFY").thickness = 0.12
        return o
    # the half loop is the back of the head: a kerf-bent green panel between the two side plates
    kerf = mat("crocodile-kerf", "#6a9a88", rough=0.55)
    sweep("crocodile-loop", path[:25], kerf, walls=False, w=half - 1.5)
    # the upper jaw over the whole head, opening toward the snout
    upper_z = z_top + 0.5
    open_fn = lambda x, y: (1 - max(0.0, min(1.0, ((x - snout[0]) * u.x + (y - snout[1]) * u.y) / span))) * 8.0
    prism("crocodile-upper-jaw", kick, upper_z, upper_z + 2.5, c, green, zfn=open_fn)
    # each side of the head is one plywood plate: the lower jaw along the long edge to the open tip, the cheek
    # up to the upper jaw, and the round back of the head around the half loop
    def side_plate(back, tip):
        e = Vector((tip[0] - back[0], tip[1] - back[1])); length = e.length; e.normalize()
        at = lambda d: (back[0] + e.x * d, back[1] + e.y * d)
        floor = lambda d: croc_z(*at(d))
        centre_u = loop_r
        reach, mouth = 34.0, 40.0
        prof = [(length, floor(length) - 2.0), (centre_u, floor(centre_u) - 2.0)]
        prof += [(centre_u - (loop_r + 0.5) * math.cos(t), z_a + loop_r + (loop_r + 0.5) * math.sin(t))
                 for t in (math.radians(-90 + 10 * k) for k in range(19))]
        prof += [(reach, upper_z + open_fn(*at(reach))), (mouth, floor(mouth) + 6.0), (length, floor(length) + 3.0)]
        n = Vector((-e.y, e.x))
        if n.dot(Vector((sum(q[0] for q in kick) / len(kick) - back[0], sum(q[1] for q in kick) / len(kick) - back[1]))) < 0:
            n = -n
        figure("crocodile-side", back[0] + n.x * 0.75, back[1] + n.y * 0.75, math.degrees(math.atan2(e.y, e.x)), prof, 1.5, c, green)
    side_plate(kick[1], kick[2])
    side_plate(kick[0], kick[-1])
    for hx, hy, hr in [t["circle"] for t in by("ramp-kicker", "hole") if "circle" in t]:
        cyl("crocodile-nostril", hx, hy, hr, upper_z + 2.5 + open_fn(hx, hy), upper_z + 2.55 + open_fn(hx, hy), c, mat("pit", "#050506"))
    eye = [(2.6 * math.cos(2 * math.pi * k / 24), 2.6 + 2.6 * math.sin(2 * math.pi * k / 24)) for k in range(24)]
    pupil = [(0.9 * math.cos(2 * math.pi * k / 16), 3.0 + 0.9 * math.sin(2 * math.pi * k / 16)) for k in range(16)]
    ang = math.degrees(math.atan2(side.y, side.x))
    for d in (-4.5, 4.5):
        ex, ey = hinge[0] - u.x * 5 + side.x * d, hinge[1] - u.y * 5 + side.y * d
        figure("crocodile-eye", ex, ey, ang, eye, 1.5, c, mat("eye-white", "#f2efe6", rough=0.4), z0=upper_z + 2.5 + open_fn(ex, ey))
        figure("crocodile-pupil", ex, ey, ang, pupil, 1.7, c, mat("pupil", "#1a3a8a", rough=0.4), z0=upper_z + 2.5 + open_fn(ex, ey))

    # the red track: a wooden floor between two bent wooden walls, falling from the mouth onto the right
    # inlane's separator, where the ball drops through the round hole at its end
    y0 = first[1]
    z_mouth = z_a + loop_r
    # inside the mouth the red track runs from the half loop out to where the drawing's track begins
    tie_w = math.dist(ties[0][0], ties[0][-1]) / 2
    # one smooth curve: leaving the loop toward the snout, arriving along the track's own direction
    second = ((ties[1][0][0] + ties[1][-1][0]) / 2, (ties[1][0][1] + ties[1][-1][1]) / 2)
    p0 = Vector((a2.x - u.x * 4.0, a2.y - u.y * 4.0, z_mouth))
    p3 = Vector((first[0], first[1], z_mouth))
    t_in = Vector((-u.x, -u.y, 0.0)); t_out = Vector((second[0] - first[0], second[1] - first[1], 0.0)).normalized()
    reach = (p3 - p0).length * 0.45
    p1, p2 = p0 + t_in * reach, p3 - t_out * reach
    curve = []
    for k in range(21):
        t = k / 20; s_ = 1 - t
        q = p0 * s_ ** 3 + p1 * 3 * s_ * s_ * t + p2 * 3 * s_ * t * t + p3 * t ** 3
        d = (p1 - p0) * 3 * s_ * s_ + (p2 - p1) * 6 * s_ * t + (p3 - p2) * 3 * t * t
        curve.append((q, d.normalized(), Vector((0, 0, 1))))
    sweep("crocodile-mouth-track", curve, red, walls=True, w=tie_w)
    # it stays above the right side stack's cover until the cover ends, then drops onto the separator
    over_cover = H["cover"] + H["ramp_floor"] + 0.4
    track_profile = profile([(y0, z_mouth), (297.0, over_cover), (306.0, H["rail"] + H["ramp_floor"])])
    track_z = lambda x, y: track_profile(y)
    for s in by("wire-ramp", "rail-red"):
        ys = [p[1] for p in s["pts"]]
        if max(ys) - min(ys) < 30:
            prism("track-end-wall", s["pts"], -1.6, 4.0, c, red, zfn=track_z)
            end_y = min(ys)
    # the floor (F15) has a slot along its middle from the mouth to the end piece; the ball runs on its edges
    me = bpy.data.meshes.new("track-floor"); bm = bmesh.new()
    prev = None; rows = []
    for t in ties:
        a, b = t[0], t[-1]
        if prev and math.dist(a, prev[0]) + math.dist(b, prev[1]) > math.dist(a, prev[1]) + math.dist(b, prev[0]):
            a, b = b, a
        f = min(0.19, 3.5 / math.dist(a, b))           # half the slot over the floor's width: 38 %, at most 7 mm
        sa = (a[0] + (b[0] - a[0]) * (0.5 - f), a[1] + (b[1] - a[1]) * (0.5 - f))
        sb = (a[0] + (b[0] - a[0]) * (0.5 + f), a[1] + (b[1] - a[1]) * (0.5 + f))
        rows.append([bm.verts.new(W(*q, track_z(*q))) for q in (a, sa, sb, b)])
        prev = (a, b)
    # the slot is closed at the mouth: the floor's first 8 mm are whole
    along = [0.0]
    for r0, r1 in zip(rows, rows[1:]):
        along.append(along[-1] + (r1[0].co - r0[0].co).length * 10)
    for i, (r0, r1) in enumerate(zip(rows, rows[1:])):
        if along[i + 1] <= 8.0:
            bm.faces.new((r0[0], r0[3], r1[3], r1[0]))
        else:
            bm.faces.new((r0[0], r0[1], r1[1], r1[0]))
            bm.faces.new((r0[2], r0[3], r1[3], r1[2]))
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    bm.normal_update()
    for f in bm.faces:
        if f.normal.z < 0: f.normal_flip()
    bm.to_mesh(me); bm.free()
    floor = link(bpy.data.objects.new("track-floor", me), c, red)
    edges = ([], [])
    prev = None
    for t in ties:
        a, b = tuple(t[0]), tuple(t[-1])
        if prev and math.dist(a, prev[0]) + math.dist(b, prev[1]) > math.dist(a, prev[1]) + math.dist(b, prev[0]):
            a, b = b, a
        edges[0].append(a); edges[1].append(b); prev = (a, b)
    # past the last strip the floor runs on to the end wall, with the round hole the ball drops through
    la, lb = edges[0][-1], edges[1][-1]
    dy = end_y - (la[1] + lb[1]) / 2
    ea, eb = (la[0], la[1] + dy), (lb[0], lb[1] + dy)
    hole = next(t["circle"] for t in by("wire-ramp", "drop-ring") if "circle" in t)
    hx, hy, hr = hole
    a0 = 0.0 if lb[0] > la[0] else math.pi   # the slit into the hole leaves from the side lb lies on
    rim = [(hx + hr * math.cos(a0 + 2 * math.pi * k / 32), hy + hr * math.sin(a0 + 2 * math.pi * k / 32)) for k in range(33)]
    end_floor = prism("track-end-floor", [la, ea, eb, lb], -1.6, 0.0, c, red, zfn=track_z)
    cut = cyl("track-hole-cutter", hx, hy, hr, track_z(hx, hy) - 5, track_z(hx, hy) + 5, c, None)
    cut.hide_render = True; cut.hide_viewport = True
    bo = end_floor.modifiers.new("hole", "BOOLEAN"); bo.object = cut; bo.operation = "DIFFERENCE"
    edges[0].append(ea); edges[1].append(eb)
    for edge in edges:
        ribbon("track-wall", edge, 1.5, -1.6, 4.0, c, red, zfn=track_z)
    floor.modifiers.new("solid", "SOLIDIFY").thickness = H["ramp_floor"] / 10


def build_balls():
    c = coll("balls")
    chrome = mat("chrome", (0.85, 0.86, 0.9, 1), rough=0.05, metal=1.0)
    sphere("ball-camera", 31.87, 123.66, RBALL, RBALL, c, chrome)
    sphere("ball-other", 140.0, 60.0, RBALL, RBALL, c, chrome)


def build_inserts():
    """The light inserts and diffusers in the board, flush with it, each kind with a material that lights.py colors and a film keys."""
    c = coll("inserts")
    lit = {"insert": mat("diffuser-lit", "#e8eef2", rough=0.3, emit="#fff1d6", strength=0.4),
           "insert-star": mat("star-lit", "#7d8a9a", rough=0.3, emit="#ffe9b0", strength=0.0),
           "insert-arrow": mat("arrow-lit", "#c9d0d8", rough=0.3, emit="#fff1d6", strength=0.3)}
    for s in SHAPES:
        if s["chain"] and s["chain"][0] == "layer-0" and s["cls"] in lit:
            prism("insert", s["pts"], 0.0, H["foil"] + 0.1, c, lit[s["cls"]], cap_only=True)


TABLE_Z = -84.4   # mm, the coffee table's top: the playfield's front edge lies 54 mm above it, its back edge 85 mm


def build_room():
    """A living room as a sky dome from Blender's own interior light probe, and a coffee table under the machine."""
    sc = bpy.context.scene
    w = sc.world or bpy.data.worlds.new("world"); sc.world = w; w.use_nodes = True
    nt = w.node_tree
    for n in list(nt.nodes): nt.nodes.remove(n)
    out = nt.nodes.new("ShaderNodeOutputWorld"); bg = nt.nodes.new("ShaderNodeBackground")
    env = nt.nodes.new("ShaderNodeTexEnvironment")
    env.image = bpy.data.images.load(os.path.join(bpy.utils.system_resource("DATAFILES"), "studiolights", "world", "interior.exr"))
    tc = nt.nodes.new("ShaderNodeTexCoord"); mp = nt.nodes.new("ShaderNodeMapping")
    mp.inputs["Rotation"].default_value = (0.0, 0.0, math.radians(200.0))
    nt.links.new(tc.outputs["Generated"], mp.inputs["Vector"]); nt.links.new(mp.outputs[0], env.inputs["Vector"])
    nt.links.new(env.outputs["Color"], bg.inputs["Color"]); bg.inputs["Strength"].default_value = 0.035   # a dark room
    nt.links.new(bg.outputs[0], out.inputs["Surface"])
    c = coll("room")
    me = bpy.data.meshes.new("coffee-table"); bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bm.to_mesh(me); bm.free()
    t = link(bpy.data.objects.new("coffee-table", me), c, mat("walnut", "#5a3a22", rough=0.35, coat=0.5))
    t.scale = (90.0, 70.0, 2.5); t.location = (11.5, -21.0, TABLE_Z / 10 - 1.25)


# ---------------------------------------------------------------- the cabinet's painted outer sides
# Layouts measured off a photo of the left side, rectified so that u runs 0..1768 from the back end of the
# wall to its front end and v runs 0..460 from the wall's top edge down to the table.
SIDE_W, SIDE_H = 1768.0, 460.0
SIDE_WINDOWS = [(37, 58, 360), (80, 100, 345), (122, 147, 320), (168, 195, 282), (212, 240, 240), (265, 295, 275),
                (320, 348, 310), (375, 402, 332), (430, 460, 343), (490, 518, 340), (548, 580, 325), (610, 642, 298),
                (673, 710, 255), (745, 780, 293), (815, 852, 318), (890, 925, 328), (970, 1010, 322), (1055, 1095, 285),
                (1140, 1185, 318)]
SIDE_TABS = [(75, 180, 425, 440), (310, 430, 425, 440), (590, 735, 418, 436), (930, 1110, 415, 432)]
# the lower edge of the curtain, from its back end to the plain red field before the drummer's niche
SWAG_EDGE = [(10, 292), (60, 270), (120, 235), (160, 195), (185, 160), (205, 150), (240, 150), (280, 200), (330, 230),
             (400, 250), (450, 255), (520, 250), (580, 225), (630, 190), (660, 172), (690, 168), (725, 168), (780, 198),
             (850, 222), (920, 232), (990, 228), (1040, 210), (1080, 200), (1110, 190)]
MEDALLIONS = [(205, 78, 38), (685, 92, 38), (1083, 110, 45)]
NICHE_X0, NICHE_X1 = 1340.0, 1712.0


def _homography(src, dst):
    """The projective map that takes the four points src to the four points dst."""
    import numpy as np
    rows, rhs = [], []
    for (x, y), (X, Y) in zip(src, dst):
        rows.append([x, y, 1, 0, 0, 0, -X * x, -X * y]); rhs.append(X)
        rows.append([0, 0, 0, x, y, 1, -Y * x, -Y * y]); rhs.append(Y)
    h = list(np.linalg.solve(np.array(rows, float), np.array(rhs, float))) + [1.0]
    return lambda x, y: ((h[0] * x + h[1] * y + h[2]) / (h[6] * x + h[7] * y + 1), (h[3] * x + h[4] * y + h[5]) / (h[6] * x + h[7] * y + 1))


def _side_height(y):
    """Height of the side wall's top edge above the table at y (mm): the edge follows the tilted playfield."""
    a = math.radians(TILT_DEG)
    return -y * math.sin(a) + wall_top(y) * math.cos(a) - TABLE_Z


# The side panel is a trapezoid: its bottom stands on the table, its top falls with the playfield. The layout's
# rectangle maps onto it projectively, which undoes the photo's perspective and keeps the paint undistorted.
SIDE_Y0, SIDE_Y1 = -8.4, 433.7
_side_map = _homography([(0, 0), (SIDE_W, 0), (SIDE_W, SIDE_H), (0, SIDE_H)],
                        [(SIDE_Y0, _side_height(SIDE_Y0)), (SIDE_Y1, _side_height(SIDE_Y1)), (SIDE_Y1, 0.0), (SIDE_Y0, 0.0)])


def side_point(u, v):
    """A point of the side layout as (y, z) in mm, z relative to the playfield as for the side walls."""
    y, h = _side_map(u, v)
    a = math.radians(TILT_DEG)
    return y, (TABLE_Z + h + y * math.sin(a)) / math.cos(a)


def side_plate(name, pts_uv, d0, d1, c, material):
    """A flat piece laid on the outer face of both side walls, from d0 to d1 mm out from the face."""
    pts = [side_point(u, v) for u, v in pts_uv]
    area = sum(pts[i][0] * pts[i - 1][1] - pts[i - 1][0] * pts[i][1] for i in range(len(pts)))
    for face, out in ((-8.38, -1.0), (238.38, 1.0)):
        me = bpy.data.meshes.new(name); bm = bmesh.new()
        a = [bm.verts.new(W(face + out * d0, y, z)) for y, z in pts]
        b = [bm.verts.new(W(face + out * d1, y, z)) for y, z in pts]
        bm.faces.new(b); bm.faces.new(a[::-1])
        for i in range(len(pts)):
            j = (i + 1) % len(pts); bm.faces.new((a[i], a[j], b[j], b[i]))
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        bm.to_mesh(me); bm.free()
        link(bpy.data.objects.new(name, me), c, material)


def arc_uv(cx, cy, r, a0, a1, n=16):
    return [(cx + r * math.cos(math.radians(a0 + (a1 - a0) * k / n)), cy - r * math.sin(math.radians(a0 + (a1 - a0) * k / n)))
            for k in range(n + 1)]


def normal_edge(edge, d):
    """The edge of the side layout moved d units up across its course (v grows downward)."""
    out = []
    for i, (u, v) in enumerate(edge):
        a, b = edge[max(i - 1, 0)], edge[min(i + 1, len(edge) - 1)]
        du, dv = b[0] - a[0], b[1] - a[1]; ln = math.hypot(du, dv) or 1.0
        nu, nv = dv / ln, -du / ln
        if nv > 0: nu, nv = -nu, -nv
        out.append((u + nu * d, v + nv * d))
    return out


def v_at(poly, u):
    """The lowest point of a polyline of the side layout at u, or None where it does not reach u."""
    vs = [a[1] + (b[1] - a[1]) * (u - a[0]) / (b[0] - a[0]) for a, b in zip(poly, poly[1:])
          if min(a[0], b[0]) <= u <= max(a[0], b[0]) and a[0] != b[0]]
    return max(vs) if vs else None


def build_outer_sides():
    c = coll("outer-sides")
    red = mat("side-red", "#b3242d", rough=0.55)
    # the windows are lit red from behind
    window = mat("side-window", "#c81e28", rough=0.6, emit="#ff2a24", strength=3.0)
    rail = mat("side-rail", "#b8935c", rough=0.6)
    inset = mat("side-rail-inset", "#d2b07a", rough=0.6)
    gold = mat("side-gold", "#d9b04a", rough=0.35, metal=0.5)
    white = mat("side-white", "#f2efe6", rough=0.5)
    blue = mat("side-blue", "#3d6fc4", rough=0.5)
    black = mat("side-black", "#141416", rough=0.5)
    ochre = mat("side-ochre", "#b8903a", rough=0.6)
    skin = mat("side-skin", "#f0d2b4", rough=0.6)
    # windows in the gray board, the red layer behind showing through, and the board's tab slots
    for u0, u1, vt in SIDE_WINDOWS:
        top = arc_uv((u0 + u1) / 2, vt + (u1 - u0) / 4, (u1 - u0) / 2, 0, 180, 8)
        side_plate("side-window", [(u0, 398)] + [(u1, 398)] + top, 0.0, 0.2, c, window)
    for u0, u1, v0, v1 in SIDE_TABS:
        # a strip plugged through the slot, standing proud of the board: black cut edges, a red face
        side_plate("side-tab", [(u0, v0), (u1, v0), (u1, v1), (u0, v1)], 0.0, 2.5, c, black)
        side_plate("side-tab-face", [(u0 + 3, v0 + 3), (u1 - 3, v0 + 3), (u1 - 3, v1 - 3), (u0 + 3, v1 - 3)], 2.5, 2.6, c, red)
    # the curtain: one red board from the top band down to its swags, then the plain field and the strip
    # before the niche, which reach to the table
    curtain = [(10, 25), (NICHE_X0, 25), (NICHE_X0, 445), (1290, 445), (1290, 225)]
    curtain += arc_uv(1245, 225, 45, 0, 90, 8)[1:]
    curtain += SWAG_EDGE[::-1]
    side_plate("side-curtain", curtain, 0.0, 1.2, c, red)
    # along the swags' lower edge, measured across it: the gold fringe, the blue line on top of it, and higher
    # up the white line
    edge = SWAG_EDGE[:-3]
    for name, lo, hi, material in (("side-fringe", 0, 24, gold), ("side-line-blue", 24, 30, blue), ("side-line-white", 62, 68, white)):
        side_plate(name, normal_edge(edge, lo) + normal_edge(edge, hi)[::-1], 1.2, 1.4, c, material)
    # stripes hang from under the rail down to the white line, long in a swag's middle and short at its ends:
    # blue in the back swag, yellow in the middle and blue again in the front one
    white_top = normal_edge(edge, 68)
    rail_bottom = lambda u: 100 + 25 * u / NICHE_X0
    for u0, u1, colr in ((12, 200, blue), (210, 680, gold), (690, 1075, blue)):
        for u in range(u0, u1 - 11, 22):
            ends = [v_at(white_top, q) for q in (u, u + 11)]
            if None in ends or min(e - rail_bottom(q) for e, q in zip(ends, (u, u + 11))) < 4: continue
            side_plate("side-stripe", [(u, rail_bottom(u) - 2), (u + 11, rail_bottom(u + 11) - 2), (u + 11, ends[1] + 1), (u, ends[0] + 1)],
                       1.2, 1.3, c, colr)
    # the wooden rail across the top, with its lighter insets
    rail_v = lambda u: (50 + 20 * u / NICHE_X0, 100 + 25 * u / NICHE_X0)
    pts = [(10, rail_v(10)[0]), (NICHE_X0, rail_v(NICHE_X0)[0]), (NICHE_X0, rail_v(NICHE_X0)[1]), (10, rail_v(10)[1])]
    side_plate("side-rail", pts, 1.2, 3.2, c, rail)
    for u0, u1 in ((25, 165), (255, 625), (740, 1010), (1170, 1320)):
        t0, b0 = rail_v(u0); t1, b1 = rail_v(u1)
        side_plate("side-rail-inset", [(u0, t0 + 13), (u1, t1 + 13), (u1, b1 - 15), (u0, b0 - 15)], 3.2, 3.4, c, inset)
    # three star medallions, each with a pennant and its fringe hanging below
    for cu, cv, r in MEDALLIONS:
        top, bottom, w = cv + r * 0.8, cv + r * 3.2, r * 0.65
        side_plate("side-pennant", [(cu - w, top), (cu + w, top), (cu + w, bottom - 18), (cu, bottom), (cu - w, bottom - 18)], 2.0, 3.0, c, red)
        side_plate("side-pennant-fringe", [(cu - w, bottom - 45), (cu + w, bottom - 45), (cu + w, bottom - 18), (cu, bottom), (cu - w, bottom - 18)], 3.0, 3.2, c, gold)
        side_plate("side-medallion", arc_uv(cu, cv, r, 0, 360, 32)[:-1], 3.4, 4.4, c, black)
        side_plate("side-medallion-red", arc_uv(cu, cv, r * 0.8, 0, 360, 32)[:-1], 4.4, 4.6, c, red)
        star = [(cu + (r * 0.6 if k % 2 == 0 else r * 0.25) * math.cos(math.radians(90 + 36 * k)),
                 cv - (r * 0.6 if k % 2 == 0 else r * 0.25) * math.sin(math.radians(90 + 36 * k))) for k in range(10)]
        side_plate("side-star", star, 4.6, 4.8, c, gold)
    # the drummer's niche at the front: a wooden arch frame, the ochre niche, the drummer on his blue stand
    cx = (NICHE_X0 + NICHE_X1) / 2
    spring = 256.0   # where the niche's arch springs from its posts
    frame = [(NICHE_X0, 445), (NICHE_X0, spring)] + arc_uv(cx, spring, (NICHE_X1 - NICHE_X0) / 2, 180, 0, 24)[1:-1] + [(NICHE_X1, spring), (NICHE_X1, 445)]
    side_plate("side-niche-frame", frame, 0.0, 2.4, c, rail)
    inner = [(NICHE_X0 + 45, 430), (NICHE_X0 + 45, spring)] + arc_uv(cx, spring, (NICHE_X1 - NICHE_X0) / 2 - 45, 180, 0, 24)[1:-1] + [(NICHE_X1 - 45, spring), (NICHE_X1 - 45, 430)]
    side_plate("side-niche-red", inner, 2.4, 2.6, c, red)
    niche = [(NICHE_X0 + 60, 430), (NICHE_X0 + 60, spring)] + arc_uv(cx, spring, (NICHE_X1 - NICHE_X0) / 2 - 60, 180, 0, 24)[1:-1] + [(NICHE_X1 - 60, spring), (NICHE_X1 - 60, 430)]
    side_plate("side-niche", niche, 2.6, 2.8, c, ochre)
    side_plate("side-stand", [(1410, 400), (1590, 400), (1590, 430), (1410, 430)], 2.8, 4.0, c, blue)
    side_plate("side-drum", arc_uv(1470, 250, 48, 0, 360, 32)[:-1], 2.8, 4.2, c, black)
    side_plate("side-drum-head", arc_uv(1470, 250, 40, 0, 360, 32)[:-1], 4.2, 4.4, c, white)
    side_plate("side-drummer-coat", [(1500, 215), (1565, 215), (1575, 320), (1495, 320)], 2.8, 4.0, c, red)
    side_plate("side-drummer-legs", [(1500, 320), (1570, 320), (1600, 395), (1580, 400), (1535, 345), (1505, 400), (1480, 395)], 2.8, 4.0, c, white)
    side_plate("side-drummer-head", arc_uv(1532, 190, 22, 0, 360, 20)[:-1], 2.8, 4.0, c, skin)
    side_plate("side-drummer-hat", [(1512, 172), (1555, 172), (1560, 130), (1508, 130)], 4.0, 4.4, c, red)
    side_plate("side-drummer-plume", [(1545, 132), (1575, 100), (1585, 108), (1556, 140)], 4.0, 4.4, c, gold)
    side_plate("side-ledge", [(1290, 425), (NICHE_X1, 425), (NICHE_X1, 450), (1290, 450)], 0.0, 4.5, c, mat("side-ledge", "#7a1a20", rough=0.5))


# ---------------------------------------------------------------- the cabinet's front
# Layout measured off a photo of the front panel: u runs from its left edge (80) to its right edge (1420),
# v down from the top of the gray cover strip (1005). One scale for both directions, from the panel's width.
FRONT_U0, FRONT_U1, FRONT_V0 = 80.0, 1420.0, 1005.0
FRONT_FACE = 435.71                     # mm, the front wall's outer face
FRONT_SCALE = 246.76 / (FRONT_U1 - FRONT_U0)
BRIDGE_TOP = [(300, 1335), (400, 1310), (500, 1280), (600, 1245), (680, 1215), (730, 1196), (745, 1188), (760, 1188),
              (775, 1196), (825, 1215), (905, 1245), (1005, 1280), (1105, 1310), (1215, 1335)]
FRONT_WINDOWS = [(320, 365), (410, 455), (500, 545), (590, 635), (678, 722), (765, 810), (850, 895), (938, 983),
                 (1025, 1070), (1112, 1157)]


def front_point(u, v):
    """A point of the front layout as (x, z) in mm, z relative to the playfield."""
    # across: the panel's width; down: from the wall's top edge to the table, which the panel stands on
    a = math.radians(TILT_DEG)
    table = (TABLE_Z + FRONT_FACE * math.sin(a)) / math.cos(a)
    top = wall_top(FRONT_FACE)
    return -8.38 + (u - FRONT_U0) * FRONT_SCALE, top - (v - FRONT_V0) / (1430.0 - FRONT_V0) * (top - table)


def front_plate(name, pts_uv, d0, d1, c, material):
    """A flat piece laid on the front wall's outer face, from d0 to d1 mm out from it."""
    pts = [front_point(u, v) for u, v in pts_uv]
    me = bpy.data.meshes.new(name); bm = bmesh.new()
    a = [bm.verts.new(W(x, FRONT_FACE + d0, z)) for x, z in pts]
    b = [bm.verts.new(W(x, FRONT_FACE + d1, z)) for x, z in pts]
    bm.faces.new(b); bm.faces.new(a[::-1])
    for i in range(len(pts)):
        j = (i + 1) % len(pts); bm.faces.new((a[i], a[j], b[j], b[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me); bm.free()
    return link(bpy.data.objects.new(name, me), c, material)


def _bridge_top_at(u):
    for (u0, v0), (u1, v1) in zip(BRIDGE_TOP, BRIDGE_TOP[1:]):
        if u0 <= u <= u1:
            return v0 + (v1 - v0) * (u - u0) / (u1 - u0)
    return BRIDGE_TOP[-1][1]


def build_front():
    c = coll("front")
    red = mat("side-red", "#b3242d", rough=0.55)
    gray = mat("front-gray", "#4a4d54", rough=0.6)
    window = mat("front-window", "#e8705a", rough=0.6, emit="#ff3a2a", strength=3.0)
    dark = mat("front-opening", "#1a1412", rough=0.8)
    wood_m = mat("front-mount", "#8a6a44", rough=0.5)
    knob = mat("plunger-knob", "#120c0c", rough=0.2, coat=0.8)
    L, R, B = FRONT_U0, FRONT_U1, 1430.0
    # a gray board lies behind the red panel; the bridge with its lit windows, the small tower on its apex and
    # the star are cut out of the red panel, so the gray board shows through them, set back behind the red
    front_plate("front-back", [(L, 1060), (R, 1060), (R, B), (L, B)], 0.0, 0.6, c, gray)
    panel = front_plate("front-panel", [(L, 1060), (R, 1060), (R, B), (L, B)], 0.6, 4.1, c, red)
    apex = BRIDGE_TOP.index((745, 1188))
    bridge = [(290, B + 5), (300, 1335)] + BRIDGE_TOP[1:apex] + [(735, 1188), (740, 1170), (765, 1170), (770, 1188)]
    bridge += BRIDGE_TOP[apex + 2:] + [(1225, 1335), (1225, B + 5)]
    star = [(752 + (36 if k % 2 == 0 else 15) * math.cos(math.radians(90 + 36 * k)),
             1132 - (36 if k % 2 == 0 else 15) * math.sin(math.radians(90 + 36 * k))) for k in range(10)]
    # the cut edges are laser-dark: the cutter hands its material to the faces it leaves
    for name, pts in (("bridge", bridge), ("star", star)):
        cut = front_plate(f"front-cut-{name}", pts, 0.3, 4.5, c, dark)
        cut.hide_render = True; cut.hide_viewport = True
        bo = panel.modifiers.new(name, "BOOLEAN"); bo.object = cut; bo.operation = "DIFFERENCE"; bo.material_mode = "TRANSFER"
    for u0, u1 in FRONT_WINDOWS:
        top = min(_bridge_top_at(u0), _bridge_top_at(u1)) + 28
        front_plate("front-window", [(u0, top), (u1, top), (u1, 1415), (u0, 1415)], 0.6, 0.7, c, window)
    front_plate("front-cover", [(L, FRONT_V0), (R, FRONT_V0), (R, 1060), (L, 1060)], 0.0, 5.6, c, gray)
    # the small wooden box in the opening stands a little proud of the panel; its cover overlaps the opening
    # and carries a pull tab in its middle
    drawer_wood = mat("drawer-wood", "#a87e4e", rough=0.6)
    front_plate("front-drawer-box", [(167, 1182), (398, 1182), (398, 1273), (167, 1273)], 0.6, 6.1, c, drawer_wood)
    front_plate("front-drawer-cover", [(157, 1172), (408, 1172), (408, 1283), (157, 1283)], 6.1, 7.6, c, drawer_wood)
    front_plate("front-drawer-tab", [(262, 1222), (303, 1222), (303, 1233), (262, 1233)], 7.6, 11.6, c, drawer_wood)
    # the plunger's knob on its wooden mount, at the shooter lane
    pu = FRONT_U0 + (224.4 + 8.38) / FRONT_SCALE
    front_plate("front-plunger-mount", [(pu - 40, 1090), (pu + 10, 1090), (pu + 10, 1240), (pu - 40, 1240)], 4.1, 6.6, c, wood_m)
    x, z = front_point(pu, 1160)
    o = sphere("plunger-knob", x, FRONT_FACE + 14.6, z, 8.0, c, knob)
    cyl_y = bpy.data.meshes.new("plunger-rod"); bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=16, radius1=0.2, radius2=0.2, depth=0.8)
    bmesh.ops.rotate(bm, verts=bm.verts, cent=(0, 0, 0), matrix=__import__("mathutils").Matrix.Rotation(math.pi / 2, 3, "X"))
    bm.to_mesh(cyl_y); bm.free()
    rod = link(bpy.data.objects.new("plunger-rod", cyl_y), c, mat("rod", "#9aa0a8", rough=0.4, metal=1.0))
    rod.location = W(x, FRONT_FACE + 6.6, z)


def build_decorations():
    """Decorations from the photos of the machine: the star stands over the three top-lane guides, the bullseye
    targets behind the left guide's rubber links and beside the top bell, and the spinner in the left lane."""
    c = coll("decorations")
    wood_m = mat("deco-wood", "#9a6a40", rough=0.6)
    gold = mat("deco-gold", "#d9b04a", rough=0.35, metal=0.5)
    dark = mat("deco-dark", "#1a1412", rough=0.8)
    red = mat("deco-red", "#c8323a", rough=0.45)
    white = mat("deco-white", "#f2efe6", rough=0.5)
    def star(r_out, r_in, cu, cz):
        return [(cu + (r_out if k % 2 == 0 else r_in) * math.cos(math.radians(90 + 36 * k)),
                 cz + (r_out if k % 2 == 0 else r_in) * math.sin(math.radians(90 + 36 * k))) for k in range(10)]
    def circle(r, cu, cz, n=32):
        return [(cu + r * math.cos(2 * math.pi * k / n), cz + r * math.sin(2 * math.pi * k / n)) for k in range(n)]
    def behind(ax, ay, bx, by_, d):
        """The midpoint of a post pair moved d mm away from the playfield, and the pair's direction in degrees."""
        ang = math.atan2(by_ - ay, bx - ax); nx, ny = -math.sin(ang), math.cos(ang)
        if ny > 0: nx, ny = -nx, -ny
        return (ax + bx) / 2 + nx * d, (ay + by_) / 2 + ny * d, math.degrees(ang)
    # two stars on a wooden stand behind each top-lane guide: a coloured one and a gold one
    for pid, colr in (("top-lane-guide-1", "#3f8f4f"), ("top-lane-guide-2", "#3d6fc4"), ("top-lane-guide-3", "#c8323a")):
        line = next(sh["pts"] for sh in placed(pid) if sh["cls"] == "sleeve")
        (ax, ay), (bx, by_) = line[0], line[-1]
        x, y, ang = behind(ax, ay, bx, by_, 0.0)    # the stand's foot sits between the rubber's two strands
        half = math.dist((ax, ay), (bx, by_)) / 2
        figure("deco-star-stand", x, y, ang, [(-2.0, 0), (2.0, 0), (2.0, 15), (half, 17), (half, 19), (-half, 19), (-half, 17), (-2.0, 15)], 1.2, c, wood_m)
        figure("deco-star", x, y, ang, star(5.0, 2.2, -half, 24.0), 2.4, c, mat("deco-star-" + pid, colr, rough=0.4))
        figure("deco-star-hole", x, y, ang, star(2.0, 0.9, -half, 24.0), 2.6, c, dark)
        figure("deco-star-gold", x, y, ang, star(5.5, 2.4, half, 24.5), 2.4, c, gold)
        figure("deco-star-hole", x, y, ang, star(2.2, 1.0, half, 24.5), 2.6, c, dark)
    # bullseye targets: a red arch with white rings and stars
    def bullseye(x, y, ang, w, h, leg):
        top_legs = H["post"] + 1.0
        arch = [(-leg / 2, 0), (leg / 2, 0), (leg / 2, top_legs), (w / 2, top_legs), (w / 2, h - w / 2)]
        arch += [(w / 2 * math.cos(math.pi * k / 16), h - w / 2 + w / 2 * math.sin(math.pi * k / 16)) for k in range(1, 16)]
        arch += [(-w / 2, h - w / 2), (-w / 2, top_legs), (-leg / 2, top_legs)]
        figure("deco-target", x, y, ang, arch, 1.2, c, red)
        cz = h - w / 2 - 1.0
        for i, (r, m) in enumerate(((w * 0.42, white), (w * 0.31, red), (w * 0.2, white), (w * 0.09, red))):
            figure("deco-target-ring", x, y, ang, circle(r, 0.0, cz), 2.2 + 0.2 * i, c, m)
        figure("deco-target-star", x, y, ang, star(w * 0.16, w * 0.07, -w * 0.22, cz + w * 0.3), 3.2, c, gold)
        figure("deco-target-star", x, y, ang, star(w * 0.1, w * 0.045, w * 0.32, cz + w * 0.18), 3.2, c, mat("deco-silver", "#c9ccd2", rough=0.3, metal=0.6))
    links = {n: next(sh["pts"] for sh in placed("lane-guides-left") if sh["cls"] == "sleeve" and n in sh["chain"]) for n in ("left-link-1", "left-link-2")}
    for name in ("left-link-1", "left-link-2"):
        (ax, ay), (bx, by_) = links[name][0], links[name][-1]
        x, y, ang = behind(ax, ay, bx, by_, 0.0)
        span = math.dist((ax, ay), (bx, by_))
        # the legs stand inside the rubber's loop, clear of the posts and the loop's ends; the arch spans the posts
        w = min(span + 2.0, 24.0)
        bullseye(x, y, ang, w, H["post"] + 1.0 + w / 2 + 1.0, span - 2 * (1.86 + 2.6) - 0.6)
    # the small round target on its stick beside the top bell
    sx, sy = 74.0, 17.0
    figure("deco-target-stick", sx, sy, 0.0, [(-0.8, 0), (0.8, 0), (0.8, 13), (-0.8, 13)], 1.5, c, gold)
    for i, (r, m) in enumerate(((5.0, white), (3.6, red), (2.2, white), (1.0, red))):
        figure("deco-target-small", sx, sy, 0.0, circle(r, 0.0, 17.0), 1.8 + 0.2 * i, c, m)
    figure("deco-target-star", sx, sy, 0.0, star(1.6, 0.7, -2.6, 19.6), 2.8, c, gold)
    # the spinner across the left lane: two side plates, the metal rod with clear caps, the blue flap
    hgt = 18.0
    lane = 75.0; rod = math.radians(lane + 90.0); rx, ry = math.cos(rod), math.sin(rod)
    # from (30, 165) the lane runs 14.75 mm toward the clear guide and 7.25 mm toward the green track's wall;
    # the left plate stands 1 mm off the guide
    x, y = 30.0 + rx * 5.5, 165.0 + ry * 5.5
    plate = [(-2.5, 0), (2.5, 0), (2.5, hgt - 2.5)] + [(2.5 * math.cos(math.pi * k / 8), hgt - 2.5 + 2.5 * math.sin(math.pi * k / 8)) for k in range(1, 8)] + [(-2.5, hgt - 2.5)]
    for d, m in ((-7.5, red), (7.5, mat("deco-red-dark", "#7a1a20", rough=0.5))):
        figure("deco-spinner-side", x + rx * d, y + ry * d, lane, plate, 1.5, c, m)
    axle = hgt - 3.0
    figure("deco-spinner-rod", x, y, lane, circle(0.6, 0.0, axle, 12), 21.0, c, mat("rod", "#9aa0a8", rough=0.4, metal=1.0))
    for d in (-9.6, 9.6):
        figure("deco-spinner-cap", x + rx * d, y + ry * d, lane, circle(1.2, 0.0, axle, 16), 1.5, c, mat("clear-plastic", "#eef3f6", rough=0.05, transmission=0.9))
    flap = [(-6.0, axle - 12.5), (6.0, axle - 12.5), (6.0, axle - 1.5), (-6.0, axle - 1.5)]
    figure("deco-spinner-flap", x, y, lane + 90.0, flap, 1.5, c, mat("deco-blue", "#2f7fb0", rough=0.4))
    figure("deco-spinner-star", x, y, lane + 90.0, star(3.0, 1.3, 0.0, axle - 6.0), 1.7, c, dark)
    figure("deco-spinner-top", x, y, lane + 90.0, [(-2.0, axle - 1.8), (2.0, axle - 1.8), (2.0, axle + 1.5), (-2.0, axle + 1.5)], 2.5, c, mat("deco-yellow", "#f1c94a", rough=0.4))


# ---------------------------------------------------------------- under the board: the ball store and the sensors
# the seesaw rocks about a pin under the board between its two fingers; tilted by SEESAW_TILT, one finger stands
# FINGER_UP above the board and the other lies just under it. The start position holds the front ball back.
SEESAW_TILT, SEESAW_PIN, FINGER_UP, FINGER_DOWN = 38.0, -9.0, 5.5, -0.5


def store_frame():
    """The ball store's frame from the drawing: the seesaw's pin, the unit vector along the track toward the
    plunger lane, the one across it, and the two fingers' middles, front first (mm)."""
    fingers = []
    for t in by("drain-ball-store", "seesaw"):
        (ax, ay), (bx, by_) = t["pts"][0], t["pts"][-1]
        fingers.append(((ax + bx) / 2, (ay + by_) / 2))
    fingers.sort(key=lambda q: -q[0])
    (fx, fy), (bx, by_) = fingers
    along = Vector((fx - bx, fy - by_)).normalized()
    return Vector(((fx + bx) / 2, (fy + by_) / 2)), along, Vector((-along.y, along.x)), fingers


def box(name, centre, along, across, size, c, material, rot=None):
    """A box of size (along, across, up) in mm around centre (x, y, z in mm), its length along the unit vector
    along in the board's plane, turned about its middle by rot, a quaternion in Blender's frame."""
    X = Vector((along.x, -along.y, 0.0)); Y = Vector((across.x, -across.y, 0.0)); Z = Vector((0, 0, 1))
    if X.cross(Y).dot(Z) < 0: Y = -Y
    basis = Matrix((X, Y, Z)).transposed().to_4x4()
    if rot is not None: basis = rot.to_matrix().to_4x4() @ basis
    me = bpy.data.meshes.new(name); bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0, matrix=Matrix.Translation(W(*centre)) @ basis
                          @ Matrix.Diagonal((size[0] / 10, size[1] / 10, size[2] / 10, 1.0)))
    bm.to_mesh(me); bm.free()
    return link(bpy.data.objects.new(name, me), c, material)


def build_store():
    """The ball store under the apron: two balls wait in a row on the board behind the seesaw, whose fingers rise
    through slots in the board. A DFRobot SER0049 micro servo rocks it through a pushrod. In the start position
    the front finger stands up and holds the front ball. In the end position the rear finger holds the back ball,
    and the front one rolls on into the plunger lane. A copper foil lies under each ball's place, a third one on
    the drain wall behind them, and the break beam's emitter and receiver face each other across the drain. The
    sizes the drawing does not give, the servo's and the beam parts', are estimates."""
    from mathutils import Quaternion
    c = coll("store")
    pin, along, across, fingers = store_frame()
    steel = mat("kicker-steel", "#b9bec6", rough=0.25, metal=1.0)
    plastic = mat("store-plastic", "#2a2d33", rough=0.5)
    white = mat("servo-white", "#e8e8e4", rough=0.4)
    blue = mat("servo-blue", "#2f5d9e", rough=0.45)
    tilt = Quaternion(Vector((across.x, -across.y, 0.0)).normalized(), math.radians(SEESAW_TILT))
    # the beam in its start pose, turned so that its front end, toward the plunger lane, rises
    if (tilt @ Vector((along.x, -along.y, 0.0))).z < 0: tilt = tilt.inverted()
    box("store-seesaw-beam", (pin.x, pin.y, SEESAW_PIN), along, across, (12.0, 3.0, 2.0), c, steel, rot=tilt)
    box("store-seesaw-bracket", (pin.x, pin.y, (SEESAW_PIN - 4.0) / 2 - 2.0), along, across, (3.0, 12.0, 8.0), c, plastic)
    pf = bpy.data.objects["playfield"]
    for k, (fx, fy) in enumerate(fingers):
        name = "store-finger-front" if k == 0 else "store-finger-back"
        lift = math.dist((fx, fy), pin) * math.sin(math.radians(SEESAW_TILT)) * (1 if k == 0 else -1)
        bottom = SEESAW_PIN + lift
        top = FINGER_UP if k == 0 else FINGER_DOWN
        box(name, (fx, fy, (bottom + top) / 2), along, across, (1.3, 10.0, top - bottom), c, steel)
        cut = box(name + "-slot", (fx, fy, -1.0), along, across, (1.8, 10.6, 8.0), c, None)
        cut.hide_render = True; cut.hide_viewport = True
        bo = pf.modifiers.new(name + "-slot", "BOOLEAN"); bo.object = cut; bo.operation = "DIFFERENCE"
    # the servo lies under the board past the seesaw toward the plunger lane, its horn turned toward the beam
    servo_at = pin + along * 18.0
    box("store-servo", (servo_at.x, servo_at.y, -20.5), along, across, (23.0, 12.2, 22.5), c, blue)
    box("store-servo-tabs", (servo_at.x, servo_at.y, -12.0), along, across, (32.0, 12.2, 2.5), c, blue)
    shaft = servo_at - along * 6.0
    cyl("store-servo-shaft", shaft.x, shaft.y, 2.4, -9.5, -7.5, c, white, verts=24)
    horn_tip = shaft - along * 6.0
    mid = (shaft + horn_tip) / 2
    box("store-servo-horn", (mid.x, mid.y, -7.0), along, across, (7.5, 3.0, 1.2), c, white)
    beam_end = pin + along * 6.0
    end_z = SEESAW_PIN + 6.0 * math.sin(math.radians(SEESAW_TILT))
    cu = bpy.data.curves.new("store-pushrod", "CURVE"); cu.dimensions = "3D"; cu.bevel_depth = 0.05
    cu.use_fill_caps = True
    sp = cu.splines.new("POLY"); sp.points.add(1)
    sp.points[0].co = (*W(horn_tip.x, horn_tip.y, -7.0), 1); sp.points[1].co = (*W(beam_end.x, beam_end.y, end_z), 1)
    link(bpy.data.objects.new("store-pushrod", cu), c, steel)
    # the foils: copper under each ball's place, and the strip on the drain wall behind them
    copper = mat("foil-copper", "#c27a45", rough=0.3, metal=0.9)
    (fx, fy), (bx, by_) = fingers
    front_mid = Vector(((fx + bx) / 2, (fy + by_) / 2))
    flat = H["foil"] + 0.1
    box("store-foil-front", (front_mid.x, front_mid.y, flat / 2), along, across, (7.5, 4.0, flat), c, copper)
    back_mid = Vector((bx, by_)) - along * 5.0
    box("store-foil-back", (back_mid.x, back_mid.y, flat / 2), along, across, (7.5, 4.0, flat), c, copper)
    wall = next(t for t in SHAPES if t["cls"] == "wood-wall" and "drain-wall-lower" in t["chain"])
    seg = [q for q in wall["pts"] if bx - 9.0 <= q[0] <= fx + 3.0]
    face = min((offset_line(seg, d, False) for d in (wall["sw"] / 2 + 0.1, -wall["sw"] / 2 - 0.1)),
               key=lambda l: sum(q.y for q in l))
    ribbon("store-foil-wall", [(q.x, q.y) for q in face], 0.15, 1.0, 7.0, c, copper)
    # the store hands its front ball through an opening in the foot of the plunger lane's wall
    gate = box("store-gate-cutter", (219.5, 399.0, 4.0), Vector((1, 0)), Vector((0, 1)), (7.0, 11.0, 10.0), c, None)
    gate.hide_render = True; gate.hide_viewport = True
    bo = bpy.data.objects["shooter-wall"].modifiers.new("store-gate", "BOOLEAN"); bo.object = gate; bo.operation = "DIFFERENCE"
    # the break beam across the drain, 39 mm from emitter to receiver, at the ball's middle
    led = mat("ir-led", "#3a1418", rough=0.2)
    black = mat("pit", "#050506", rough=0.9)
    for name, x, m, out in (("drain-beam-emitter", 89.65, led, -1.0), ("drain-beam-receiver", 128.65, black, 1.0)):
        box(name + "-holder", (x + 3.0 * out, 405.0, 4.5), Vector((1, 0)), Vector((0, 1)), (6.0, 7.0, 9.0), c, plastic)
        tube(name, [(x, 405.0), (x + 2.5 * out, 405.0)], 1.5, 4.5, c, m)


def build_sensors():
    """The IR sensors: one under each sensor eye in the drawing, looking up through it. A track's sensor sits
    against the track's floor, found by looking down from above the eye; every other sensor sits against the
    board's underside. A small green board carries each one. Its size is an estimate."""
    c = coll("sensors")
    dg = bpy.context.evaluated_depsgraph_get()
    pcb = mat("pcb", "#1f6b3a", rough=0.5)
    body = mat("sensor-black", "#111214", rough=0.4)
    eyes = [(t["circle"][0], t["circle"][1], any(ch.startswith("ir-ramp-") for ch in t["chain"]))
            for t in SHAPES if t["cls"] == "ir-eye" and "ir-sensors" in t["chain"]]
    for t in by("drain-ball-store", "exit-sensor"):
        xs = [q[0] for q in t["pts"]]; ys = [q[1] for q in t["pts"]]
        eyes.append(((min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2, False))
    for x, y, track in eyes:
        under = -4.0
        if track:
            hit, loc, nor, idx, ob, mtx = bpy.context.scene.ray_cast(dg, Vector(W(x, y, 120.0)), Vector((0, 0, -1)))
            if hit and ob.name != "playfield" and loc.z * 10 > 2.0:
                under = loc.z * 10 - H["ramp_floor"]
        prism("ir-sensor", [(x - 2.3, y - 2.0), (x + 2.3, y - 2.0), (x + 2.3, y + 2.0), (x - 2.3, y + 2.0)],
              under - 3.0, under, c, body)
        prism("ir-sensor-board", [(x - 6.0, y - 5.0), (x + 6.0, y - 5.0), (x + 6.0, y + 5.0), (x - 6.0, y + 5.0)],
              under - 4.6, under - 3.0, c, pcb)


def tilt_machine():
    """Hangs everything but the backbox from one empty that tips the playfield toward the flippers."""
    e = bpy.data.objects.new("machine-tilt", None); bpy.context.scene.collection.objects.link(e)
    e.rotation_euler = (math.radians(TILT_DEG), 0.0, 0.0)
    for o in bpy.data.objects:
        if o is e or o.parent or o.name == "backbox": continue
        o.parent = e
    return e


def textured_view():
    """Sets every 3D view's solid shading to show the image textures, so the playfield and the backbox show their
    prints."""
    for w in bpy.context.window_manager.windows:
        for a in w.screen.areas:
            if a.type == "VIEW_3D":
                a.spaces.active.shading.color_type = "TEXTURE"


def build_all():
    reset()
    sc = bpy.context.scene
    sc.unit_settings.system = "METRIC"; sc.unit_settings.scale_length = 0.01
    build_board(); build_bumpers(); build_posts_and_rubbers(); build_lower(); build_shooter_lane()
    build_sides(); build_ramps(); build_balls(); build_inserts(); build_outer_sides(); build_front(); build_decorations()
    build_store(); build_sensors()
    tilt_machine(); build_backbox(); build_room(); lights.build()
    textured_view()
    return {"objects": len(bpy.data.objects)}


result = build_all()
