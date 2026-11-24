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
from mathutils.geometry import delaunay_2d_cdt

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
    "post": 8.0, "head": 2.5, "sleeve_lo": 2.0, "sleeve_hi": 10.0, "rubber_z": 6.0,   # a post's clear spacer, its screw's head
    "fence": 25.0, "fence_end": 12.0, "blade": 20.0,
    "flipper": 8.0, "sling": 12.0, "rail": 10.0,
    "sling_lo": 9.5,                                # the slingshot's wooden cover, its underside 0.5 mm over the 9 mm ball
    "apron_lo": 10.5, "apron_hi": 13.5,             # the apron rests on the outlane walls' stack
    "bell_rim": 4.0, "bell_top": 18.0, "foil": 0.03, "plate": 1.0,
    "ramp_wall": 9.0, "ramp_floor": 1.6,
    "gray_end": 30.0,                               # the gray lane's floor at its end under the blue ramp
    "shooter_top": 24.0,                            # the shooter lane's floor at its top end
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


SURFACE_STEP = 1.5     # mm between the points of a floor's net


def surface(name, pts, zfn, thickness, c, material):
    """A floor that follows a height: a net of even triangles over the outline, its points SURFACE_STEP apart along
    the outline and on a square grid inside it, every point lifted to zfn, then given its thickness downward."""
    pts = clean(pts)
    edge = []
    for (x0, y0), (x1, y1) in zip(pts, pts[1:] + pts[:1]):
        n = max(1, math.ceil(math.hypot(x1 - x0, y1 - y0) / SURFACE_STEP))
        edge += [(x0 + (x1 - x0) * k / n, y0 + (y1 - y0) * k / n) for k in range(n)]
    xs, ys = [p[0] for p in edge], [p[1] for p in edge]
    grid = []
    for gx in range(math.floor(min(xs) / SURFACE_STEP), math.ceil(max(xs) / SURFACE_STEP) + 1):
        for gy in range(math.floor(min(ys) / SURFACE_STEP), math.ceil(max(ys) / SURFACE_STEP) + 1):
            x, y = gx * SURFACE_STEP, gy * SURFACE_STEP
            # a grid point closer to the outline than half a step would leave a sliver
            if inside((x, y), edge) and min(math.hypot(x - ex, y - ey) for ex, ey in edge) > SURFACE_STEP / 2:
                grid.append((x, y))
    verts = [Vector((x, y)) for x, y in edge + grid]
    out_verts, _, out_faces, *_ = delaunay_2d_cdt(verts, [], [list(range(len(edge)))], 1, 1e-6)
    me = bpy.data.meshes.new(name); bm = bmesh.new()
    vs = [bm.verts.new((v.x / 10, -v.y / 10, zfn(v.x, v.y) / 10)) for v in out_verts]
    for f in out_faces:
        bm.faces.new([vs[k] for k in f])
    bm.normal_update()
    for face in bm.faces:
        if face.normal.z < 0: face.normal_flip()
    # the thickness hangs straight down, so the underside follows the top without spikes at the edges
    ret = bmesh.ops.extrude_face_region(bm, geom=bm.faces[:], use_keep_orig=True)
    new = [g for g in ret["geom"] if isinstance(g, bmesh.types.BMVert)]
    bmesh.ops.translate(bm, verts=new, vec=(0.0, 0.0, -thickness / 10))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me); bm.free()
    # a curved floor shades smooth, its edges stay sharp
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


def clip_band(poly, origin, normal, a0, a1):
    """The part of polygon poly whose distance along normal from origin lies between a0 and a1."""
    def cut(pts, keep):
        out = []
        for i, p in enumerate(pts):
            q = pts[i - 1]
            dp, dq = keep(p), keep(q)
            if (dp >= 0) != (dq >= 0):
                t = dq / (dq - dp)
                out.append((q[0] + (p[0] - q[0]) * t, q[1] + (p[1] - q[1]) * t))
            if dp >= 0:
                out.append(p)
        return out
    d = lambda p: (p[0] - origin[0]) * normal.x + (p[1] - origin[1]) * normal.y
    return cut(cut(list(poly), lambda p: d(p) - a0), lambda p: a1 - d(p))


def lerp2(p, q, t):
    return (p[0] + (q[0] - p[0]) * t, p[1] + (q[1] - p[1]) * t)


def idw(anchors):
    """Height (mm) by inverse distance weighting over anchors [(x, y, z)]."""
    def f(x, y):
        ws = [1 / max(1e-6, (x - ax) ** 2 + (y - ay) ** 2) ** 1.5 for ax, ay, _ in anchors]
        return sum(w * a[2] for w, a in zip(ws, anchors)) / sum(ws)
    return f


GRAY_UNDER_BLUE = 0.3  # mm the gray lane keeps under the blue ramp's floor where it ends beneath it
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
    # the picture lies on the board through its UV map, which build_board sets from the drawing's view box
    img = bpy.data.images.load(os.path.join(OUT, "floor.png"))
    it = n.new("ShaderNodeTexImage"); it.image = img; it.interpolation = "Cubic"; it.extension = "EXTEND"
    b = n["Principled BSDF"]; nt.links.new(it.outputs["Color"], b.inputs["Base Color"])
    # painted wood: matte, no lacquer
    b.inputs["Roughness"].default_value = 0.7
    return m


def tile_material(name, size_px, color, rough, coat=0.0, scale=(1.0, 1.0)):
    """A material painted from a small tile that repeats over the UV map: color(u, v) names the hex color at a
    point of the tile, u and v from 0 to 1; scale maps the UV map's units onto the tile."""
    w, h = size_px
    img = bpy.data.images.get(name + "-tile") or bpy.data.images.new(name + "-tile", w, h)
    img.pixels[:] = [c for j in range(h) for i in range(w)
                     for c in tuple(int(color((i + 0.5) / w, (j + 0.5) / h)[k:k + 2], 16) / 255 for k in (1, 3, 5)) + (1.0,)]
    img.filepath_raw = os.path.join(OUT, name + "-tile.png"); img.file_format = "PNG"; img.save()
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; n = nt.nodes
    uvn = n.new("ShaderNodeUVMap")
    it = n.new("ShaderNodeTexImage"); it.image = img; it.interpolation = "Closest"; it.extension = "REPEAT"
    if scale != (1.0, 1.0):
        mp = n.new("ShaderNodeMapping"); mp.inputs["Scale"].default_value = (scale[0], scale[1], 1.0)
        nt.links.new(uvn.outputs[0], mp.inputs[0]); nt.links.new(mp.outputs[0], it.inputs[0])
    else:
        nt.links.new(uvn.outputs[0], it.inputs[0])
    bs = n["Principled BSDF"]; nt.links.new(it.outputs["Color"], bs.inputs["Base Color"])
    bs.inputs["Roughness"].default_value = rough; bs.inputs["Coat Weight"].default_value = coat
    return m


def stripes_material(name, a, b_):
    """Stripes of two colors across a part whose UV map's u counts stripe periods, the first half a."""
    return tile_material(name, (2, 1), lambda u, v: a if u < 0.5 else b_, 0.35, coat=0.3)


def uv_along_x(o, period):
    """Gives the object a UV map whose u counts periods of period mm along x."""
    me = o.data
    uv = me.uv_layers.new(name="UVMap")
    for loop in me.loops:
        uv.data[loop.index].uv = (me.vertices[loop.vertex_index].co.x * 10 / period, 0.5)


APRON_STRIPE = 14.9                           # mm, the apron's stripes: one red and one darker red
FENCE_PICKET = (4.0, 2.6)                     # mm: the pickets' pitch and their width
FENCE_RAILS = ((7.0, 9.5), (17.0, 19.5))      # mm over the fence's foot: the two rails
FENCE_TILE_H = 25.0                           # mm of fence one tile covers


def fence_material():
    """The U lane's wall painted as a picket fence: pickets and gaps along its length, two rails across. Its
    ribbon's UV map runs in mm, along and up."""
    def color(u, v):
        if any(lo < v * FENCE_TILE_H < hi for lo, hi in FENCE_RAILS):
            return "#7a4a26"
        return "#b07a45" if u * FENCE_PICKET[0] < FENCE_PICKET[1] else "#4b2b14"
    return tile_material("fence", (40, 250), color, 0.55, scale=(1 / FENCE_PICKET[0], 1 / FENCE_TILE_H))


WALL_COLORS = {"plywood": "#e8dcc0", "red": "#b3242d", "edge": "#2a2d33", "outer": "#4a4d54",
               "brown": "#4a2616", "upright": "#121215"}
UPRIGHT_PITCH, UPRIGHT_WIDTH = 46.0, 4.0     # mm, the partition's black uprights


def wall_top_line():
    """The side walls' top edge as a line over the board, z = a + b y in mm, from the back at y = -8.4 to the
    front at 435.7."""
    b = (H["wall_front"] - H["wall_back"]) / 444.1
    return H["wall_back"] + 8.4 * b, b


def paint(o, planes, rule, rough):
    """Cuts the object's mesh along planes, each (point, normal) in mm of the tilted frame, and gives every face the
    flat color rule names for its middle and normal, so each color is a material of one color."""
    bm = bmesh.new(); bm.from_mesh(o.data)
    for co, no in planes:
        geom = bm.verts[:] + bm.edges[:] + bm.faces[:]
        bmesh.ops.bisect_plane(bm, geom=geom, plane_co=Vector(W(*co)), plane_no=Vector((no[0], -no[1], no[2])))
    bm.normal_update()
    names = []
    for f in bm.faces:
        c = f.calc_center_median()
        name = rule((c.x * 10, -c.y * 10, c.z * 10), (f.normal.x, -f.normal.y, f.normal.z))
        if name not in names:
            names.append(name)
        f.material_index = names.index(name)
    o.data.materials.clear()
    for name in names:
        o.data.materials.append(mat("wall-" + name, WALL_COLORS[name], rough=rough))
    bm.to_mesh(o.data); bm.free()


def side_wall_paint(o):
    """The cabinet's side walls: red above the playfield, plywood below it, the dark top edge 2 mm deep, and the
    gray base board on the outer faces, where build_outer_sides lays the paint."""
    a, b = wall_top_line()
    def rule(p, n):
        x, y, z = p
        if n[0] * (1 if x > 115 else -1) > 0.5:
            return "outer"
        if z > a + b * y - 2.0:
            return "edge"
        return "red" if z > 0 else "plywood"
    # the edge's lower line z = a - 2 + b y runs through (0, 0, a - 2), normal to (0, -b, 1)
    paint(o, [((0, 0, 0), (0, 0, 1)), ((0, 0, a - 2.0), (0, -b, 1))], rule, 0.6)


def back_wall_paint(o):
    """The partition under the backbox: dark brown above the playfield, plywood below, black uprights."""
    def rule(p, n):
        x, y, z = p
        if (x % UPRIGHT_PITCH) < UPRIGHT_WIDTH:
            return "upright"
        return "brown" if z > 0 else "plywood"
    planes = [((0, 0, 0), (0, 0, 1))]
    planes += [((k * UPRIGHT_PITCH + d, 0, 0), (1, 0, 0)) for k in range(-1, 7) for d in (0.0, UPRIGHT_WIDTH)]
    paint(o, planes, rule, 0.55)


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
    face = bm.faces.new(vs); uv = bm.loops.layers.uv.new("UVMap")
    for loop in face.loops:
        co = loop.vert.co              # Blender units of 10 mm, y pointing up the drawing
        loop[uv].uv = (co.x * 10 / VB[2] - VB[0] / VB[2], co.y * 10 / VB[3] + 1 + VB[1] / VB[3])
    bm.to_mesh(me); bm.free()
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
    for s in placed("cabinet"):
        if s["cls"] in ("wall", "wall-top"):
            # the partition at the back is the diorama's front, up to its base; the side walls run to the wall top.
            # Every wall stands level on the table: its lower edge, in the tipped machine's frame, climbs toward
            # the back as the board falls toward the front
            hi = (lambda x, y: wall_top(y)) if s["cls"] == "wall" else (lambda x, y: DIORAMA_BASE)
            on_table = lambda x, y: (TABLE_Z + y * math.sin(math.radians(TILT_DEG))) / math.cos(math.radians(TILT_DEG))
            o = prism("cabinet-wall", s["pts"], 0.0, 0.0, c, None, lo_fn=on_table, hi_fn=hi)
            (side_wall_paint if s["cls"] == "wall" else back_wall_paint)(o)


DIORAMA_WIDTH = 275.0                           # mm, the manual's assembly size: the towers' width
# mm above the playfield's back edge: the partition's ledge, from the photos, where the towers' base plates rest on
# the side walls' top
DIORAMA_BASE = 48.0


CIRCUS_INSET = 3.0     # mm the CIRCUS board stands inside the casing's back, as the diorama stands behind the front
CROWN_SIGN = 1.5     # mm the ROKR sign stands out of the crown
CROWN_THICK = 6.0     # mm, the crown's thickness; it stands in the middle of the structure's depth
SHOOTER_WALL_ABOVE = 12.0     # mm the shooter lane's red wall stands over the lane's floor: the ball and 3 mm
TOWER_WALL = 3.0     # mm, the plywood of the towers' column
CABINET_OUTER = (-12.0, 242.0, -16.0)     # mm: the cabinet's outer faces left and right, with its side panels, and its back
DIORAMA_DEPTH = 22.0     # mm from the frame's picture back to the casing's back, the sky 2 mm before it
CURTAIN_THICK = 2.0     # mm, the curtains' boards
FRAME_THICK = 3.0       # mm, the frame's plate
BEAM_PROUD = 0.6        # mm the roof's beams, the ledges and the stars stand out of the plate


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
    def layer(name, y, mirror=False):
        """A picture of the drawing standing at y; mirror shows it the right way round from behind."""
        img = bpy.data.images.load(os.path.join(OUT, name + ".png"), check_existing=True)
        m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree
        tex = nt.nodes.new("ShaderNodeTexImage"); tex.image = img
        b = nt.nodes["Principled BSDF"]; nt.links.new(tex.outputs["Color"], b.inputs["Base Color"])
        nt.links.new(tex.outputs["Alpha"], b.inputs["Alpha"]); b.inputs["Roughness"].default_value = 0.6
        nt.links.new(tex.outputs["Color"], b.inputs["Emission Color"]); b.inputs["Emission Strength"].default_value = 0.35
        me = bpy.data.meshes.new(name); bm = bmesh.new()
        vs = [bm.verts.new((X(x) / 10, -y / 10, z / 10)) for x, z in ((VB[0], bottom), (VB[0] + VB[2], bottom), (VB[0] + VB[2], top), (VB[0], top))]
        f = bm.faces.new(vs); uv = bm.loops.layers.uv.new()
        for loop, (uu, vv) in zip(f.loops, [(0, 1 - vb), (1, 1 - vb), (1, 1), (0, 1)]):
            loop[uv].uv = (1 - uu if mirror else uu, vv)
        bm.to_mesh(me); bm.free()
        return link(bpy.data.objects.new(name, me), c, m)
    # the towers reach 10 mm past the brown partition's front (4.3 mm, from the drawing) over the cabinet's back
    # corners, where only the low side stacks end in them; the frame's picture stands just in front of them, the
    # sky at the casing's back
    front, back = 14.95, 14.95 - DIORAMA_DEPTH
    rear = CABINET_OUTER[2] - TOWER_WALL                # the casing's back, flush with the towers' backs
    for name, y in (("backbox-back", back + 2.0), ("backbox-b20", back + 4.5)):
        layer(name, y)

    def cutout(name, image, outline, y_front, thick, side, alpha=False, u_range=None, mirror=False):
        """A part sawn out along outline, drawing millimetres (x, y of the elevation): its front shows the picture
        image as the layers do, at y_front, and its edge and back are side, thick behind it. Beyond
        u_range, x0 to x1, the front repeats the picture's column at the nearer end. A negative thick puts the
        part on the casing's back, its picture facing backward; mirror turns the picture to read right from
        behind."""
        pts = clean(outline)
        edge = []
        for (x0, y0), (x1, y1) in zip(pts, pts[1:] + pts[:1]):
            n = max(1, math.ceil(math.hypot(x1 - x0, y1 - y0) / 2.0))
            edge += [(x0 + (x1 - x0) * k / n, y0 + (y1 - y0) * k / n) for k in range(n)]
        xs, ys = [p[0] for p in edge], [p[1] for p in edge]
        grid = [(gx * 2.0, gy * 2.0) for gx in range(math.floor(min(xs) / 2), math.ceil(max(xs) / 2) + 1)
                for gy in range(math.floor(min(ys) / 2), math.ceil(max(ys) / 2) + 1)
                if inside((gx * 2.0, gy * 2.0), edge) and min(math.hypot(gx * 2.0 - ex, gy * 2.0 - ey) for ex, ey in edge) > 1.0]
        ov, _, of, *_ = delaunay_2d_cdt([Vector(p) for p in edge + grid], [], [list(range(len(edge)))], 1, 1e-6)
        img = bpy.data.images.load(os.path.join(OUT, image + ".png"), check_existing=True)
        m = bpy.data.materials.get(name + "-picture") or bpy.data.materials.new(name + "-picture"); m.use_nodes = True
        nt = m.node_tree; b = nt.nodes["Principled BSDF"]
        if not any(n.type == "TEX_IMAGE" for n in nt.nodes):
            tex = nt.nodes.new("ShaderNodeTexImage"); tex.image = img
            nt.links.new(tex.outputs["Color"], b.inputs["Base Color"]); b.inputs["Roughness"].default_value = 0.6
            nt.links.new(tex.outputs["Color"], b.inputs["Emission Color"]); b.inputs["Emission Strength"].default_value = 0.35
            if alpha: nt.links.new(tex.outputs["Alpha"], b.inputs["Alpha"])
        me = bpy.data.meshes.new(name); bm = bmesh.new(); uvl = bm.loops.layers.uv.new()
        at = lambda x, yd, y: (X(x) / 10, -y / 10, (top - (yd - VB[1]) * s) / 10)
        fv = [bm.verts.new(at(v.x, v.y, y_front)) for v in ov]
        bv = [bm.verts.new(at(v.x, v.y, y_front - thick)) for v in ov]
        for f in of:
            face = bm.faces.new([fv[k] for k in f]); face.material_index = 0
            for loop, k in zip(face.loops, f):
                ux = ov[k].x if u_range is None else min(max(ov[k].x, u_range[0]), u_range[1])
                uu = (ux - VB[0]) / VB[2]
                loop[uvl].uv = (2 * (115.0 - VB[0]) / VB[2] - uu if mirror else uu, 1 - (ov[k].y - VB[1]) / VB[3])
            bm.faces.new([bv[k] for k in f[::-1]]).material_index = 1
        n = len(edge)
        for i in range(n):
            j = (i + 1) % n
            try: bm.faces.new((fv[i], fv[j], bv[j], bv[i])).material_index = 1
            except ValueError: pass
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        bm.to_mesh(me); bm.free()
        me.materials.append(m); me.materials.append(side)
        o = bpy.data.objects.new(name, me); c.objects.link(o)
        return o
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
        """The tower's top after the photo of the kit, heights and widths from the drawing (mm below the
        elevation's top and across, scaled), a stepped pyramid: the red cover plate (79 to 74.5, 56.8 wide), one
        thick red block with gold stripes (74.5 to 63.5), a cap plate overhanging it (63.5 to 62), a smaller dark
        block (62 to 54), a gap bridged by gold pegs, a smaller red plate (52.5 to 50.5), a round drum in two tiers
        (50.5 to 44.5), the wooden stick up into the gold star around 38.5."""
        cx, cy, d = (x0 + x1) / 2, (y0 + y1) / 2, y1 - y0
        zd = lambda depth: top - depth * s
        def slab(name, sw, sd, z0, z1, material):
            box(name, cx - sw / 2, cx + sw / 2, cy - sd / 2, cy + sd / 2, z0, z1, material)
        cw = 56.8 * s
        slab("diorama-tower-cover", cw, d + (cw - (x1 - x0)), zd(79.0), zd(74.5), red)
        pw = 45.8 * s; vw = pw - 6.0; vd = d - (x1 - x0 - vw)
        slab("diorama-tower-vent", vw, vd, zd(74.5), zd(63.5), red)
        for zz in (0.25, 0.5, 0.75):
            z = zd(74.5) + (zd(63.5) - zd(74.5)) * zz
            box("diorama-tower-vent-stripe", cx - vw / 2 + 2, cx + vw / 2 - 2, cy + vd / 2, cy + vd / 2 + 0.15, z - 0.4, z + 0.4, gold_line)
        slab("diorama-tower-cap", vw + 3.0, vd + 3.0, zd(63.5), zd(62.0), red)
        gw, gd = vw * 0.75, vd * 0.75
        slab("diorama-tower-gray", gw, gd, zd(62.0), zd(54.0), panel_gray)
        rw, rd = vw * 0.6, vd * 0.6
        slab("diorama-tower-cross", rw, rd, zd(52.5), zd(50.5), red)
        for sx in (-1, 1):
            for sy in (-1, 1):
                cyl("diorama-tower-peg", cx + sx * (rw / 2 - 2.5), cy + sy * (rd / 2 - 2.5), 1.0, zd(54.0), zd(52.5), c, gold_line, 12)
        cyl("diorama-tower-disc", cx, cy, 7.5 * s, zd(50.5), zd(47.0), c, panel_gray, 32)
        cyl("diorama-tower-disc", cx, cy, 5.5 * s, zd(47.0), zd(44.5), c, red, 32)
        cyl("diorama-tower-stick", cx, cy, 0.9, zd(44.5), zd(38.5), c, stick, 12)          # up into the star
        figure("diorama-tower-star", cx, cy, 0.0, star(6.0 * s, 2.6 * s, zd(38.5)), 1.8, c, gold_line)
        figure("diorama-tower-star-hole", cx, cy, 0.0, star(2.7 * s, 1.1 * s, zd(38.5)), 2.0, c, dark)
    dark = mat("diorama-column", "#1c1c20", rough=0.6)
    gray = mat("diorama-gray", "#4a4d54", rough=0.6)
    red = mat("diorama-red", "#b3242d", rough=0.55)
    wall = 62.0                                     # the side walls' top at the cabinet's back end, tilted
    # the tower in the drawing, from the elevation's top down: its top to 79 mm, then the red tower box, its foot
    # at 163.5 mm
    box_top, box_foot = top - 79.0 * s, top - 163.5 * s
    for x0, x1 in (left, right):
        # the column and the tower above it share one footprint: the tower's width from the picture, and deep
        # enough from the frame back that the column's back wall, TOWER_WALL thick, stands behind the cabinet's back.
        # From the photo of its side, heights in mm above the table: a red foot, a stepped plinth, a tall part with a
        # gold-framed gray panel, dark ledges with a small slotted plate between them, and the ledge at the side
        # wall's top
        y0, y1 = CABINET_OUTER[2] - TOWER_WALL, front - 0.5
        out = -1.0 if x0 < 115 else 1.0
        xo = x0 if out < 0 else x1
        # below the stage the column is an L of two boards seen from above, its outer walls only: one beside the
        # cabinet's side, one behind its back, and the cabinet's back corner lies inside it. Above the stage the
        # tower is a closed box.
        box("diorama-tower", x0, x1, y0, y1, stage, box_top, red)
        side_x = sorted((xo, CABINET_OUTER[0] if out < 0 else CABINET_OUTER[1]))
        box("diorama-tower", side_x[0], side_x[1], y0, y1, TABLE_Z, stage, red)
        box("diorama-tower", x0, x1, y0, CABINET_OUTER[2], TABLE_Z, stage, red)
        def ledge(z0, z1, grow, material):
            if z0 >= stage:
                box("diorama-tower-ledge", x0 - grow, x1 + grow, y0 - grow, y1 + grow, z0, z1, material)
                return
            box("diorama-tower-ledge", side_x[0] - grow, side_x[1] + grow, y0 - grow, y1 + grow, z0, z1, material)
            box("diorama-tower-ledge", x0 - grow, x1 + grow, y0 - grow, CABINET_OUTER[2] + grow, z0, z1, material)
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
    # the scene stands on a red stage on the partition, from the frame back to the sky; its top meets the sandy
    # ground's lower edge, so the scene stands on it
    ground_low = max(p[1] for t in SHAPES if "dio-scene-ground" in t["chain"] for p in t["pts"])
    box("diorama-stage", left[1], right[0], back, front - 0.1, stage, top - (ground_low - VB[1]) * s, red)
    # the crown on the frame's top bar: its outline from the drawing, CROWN_THICK thick with rounded edges, the
    # picture on its front and its back and red round its edge, a blue and gold star standing out of its middle
    crown = json.load(open(os.path.join(OUT, "crown.json")))["outline"]
    pic = bpy.data.materials.new("crown-picture"); pic.use_nodes = True; nt = pic.node_tree
    tex = nt.nodes.new("ShaderNodeTexImage"); tex.image = bpy.data.images.load(os.path.join(OUT, "backbox-frame.png"), check_existing=True)
    b = nt.nodes["Principled BSDF"]; nt.links.new(tex.outputs["Color"], b.inputs["Base Color"])
    nt.links.new(tex.outputs["Color"], b.inputs["Emission Color"]); b.inputs["Emission Strength"].default_value = 0.35
    me = bpy.data.meshes.new("diorama-crown"); bm = bmesh.new(); uvl = bm.loops.layers.uv.new()
    pts = [(X(x) / 10, (top - dep * s) / 10, (x - VB[0]) / VB[2], 1 - dep / VB[3]) for x, dep in crown]
    crown_y = (front + rear) / 2
    yf, yb = -(crown_y + CROWN_THICK / 2) / 10, -(crown_y - CROWN_THICK / 2) / 10
    fv = [bm.verts.new((x, yf, z)) for x, z, _, _ in pts]
    bv = [bm.verts.new((x, yb, z)) for x, z, _, _ in pts]
    face = bm.faces.new(fv[::-1]); face.material_index = 0
    for loop in face.loops:
        i = fv.index(loop.vert); loop[uvl].uv = (pts[i][2], pts[i][3])
    # the back shows the same picture, mirrored about the crown's middle so it reads right from behind
    u_mid = (115.0 - VB[0]) / VB[2]
    face = bm.faces.new(bv); face.material_index = 0
    for loop in face.loops:
        i = bv.index(loop.vert); loop[uvl].uv = (2 * u_mid - pts[i][2], pts[i][3])
    for i in range(len(pts)):
        j = (i + 1) % len(pts); bm.faces.new((fv[i], fv[j], bv[j], bv[i])).material_index = 1
    bm.normal_update(); bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new("diorama-crown", me); c.objects.link(o)
    me.materials.append(pic); me.materials.append(red)
    bev = o.modifiers.new("round", "BEVEL"); bev.width = 0.08; bev.segments = 3; bev.limit_method = "ANGLE"
    # the blue and the gold star stand out of the crown, one on the other, over the painted stars' middle and as
    # large as they are; the ROKR sign stands out of it as a plate of its own; both on its front and its back
    def crest(cls):
        pts = next(t["pts"] for t in SHAPES if "dio-frame-crest" in t["chain"] and t["cls"] == cls)
        ys = [p[1] for p in pts]
        r = (max(p[0] for p in pts) - min(p[0] for p in pts)) / 2 / math.sin(math.radians(72))
        return top - (max(ys) - r * math.cos(math.radians(36)) - VB[1]) * s, r * s
    zc, r_blue = crest("dio-silver")
    _, r_gold = crest("dio-gold")
    plaque = next(t["pts"] for t in SHAPES if "dio-frame-crest" in t["chain"] and t["cls"] == "dio-plaque")
    for out in (1, -1):              # on its front and its back
        face = crown_y + out * CROWN_THICK / 2
        figure("diorama-crown-star", 115.0, face + out * 0.5, 0.0, star(r_blue, r_blue * 0.43, zc), 1.0, c, mat("crown-star-blue", "#8fb4dc", rough=0.4))
        figure("diorama-crown-star", 115.0, face + out * 1.2, 0.0, star(r_gold, r_gold * 0.43, zc), 0.6, c, gold_line)
        cutout("diorama-crown-sign", "backbox-frame", plaque, face + out * CROWN_SIGN, out * CROWN_SIGN, red, mirror=out < 0)
    # the box's red ceiling meets the sky's top edge, 78.7 mm below the elevation's top in the drawing; between
    # the ledge and the towers' boxes, side walls close the box
    ceiling = top - 78.7 * s
    box("diorama-ceiling", left[1] - 4.0, right[0] + 4.0, rear, front - 0.3, ceiling, ceiling + 1.5, red)
    crown_base = min(top - dep * s for _, dep in crown)
    # the casing reaches back as far as the towers; in the back's window, where the front has the diorama, stands
    # a dark wooden board with CIRCUS on it
    circus = bpy.data.materials.new("diorama-circus"); circus.use_nodes = True; nt = circus.node_tree
    tex = nt.nodes.new("ShaderNodeTexImage"); tex.image = bpy.data.images.load(os.path.join(OUT, "backbox-circus.png"))
    nt.links.new(tex.outputs["Color"], nt.nodes["Principled BSDF"].inputs["Base Color"])
    nt.nodes["Principled BSDF"].inputs["Roughness"].default_value = 0.6
    me = bpy.data.meshes.new("diorama-circus"); bm = bmesh.new(); uvl = bm.loops.layers.uv.new()
    y_board = rear + CIRCUS_INSET
    f = bm.faces.new([bm.verts.new(W(x, y_board, z)) for x, z in ((left[1], stage), (right[0], stage), (right[0], ceiling + 1.5), (left[1], ceiling + 1.5))])
    for loop, q in zip(f.loops, ((1, 0), (0, 0), (0, 1), (1, 1))):           # read from behind
        loop[uvl].uv = q
    bm.to_mesh(me); bm.free(); me.materials.append(circus)
    c.objects.link(bpy.data.objects.new("diorama-circus", me))
    box("diorama-circus-board", left[1], right[0], y_board, y_board + 3.0, stage, ceiling + 1.5, mat("diorama-circus-wood", "#3b2415", rough=0.6))
    for x in (left[1], right[0]):
        box("diorama-wall", x - 1.0, x + 1.0, back, front - 0.3, stage, box_foot, mat("diorama-wall", "#7a1a20", rough=0.6))
    # the scene's parts are sawn out and stand one before the other between the background and the frame: the
    # sandy ground at the back, then the balls, the pins and the cannon's wheel
    plywood = mat("diorama-plywood", "#c8a878", rough=0.7)
    def scene_part(child, cls):
        shs = [t for t in SHAPES if child in t["chain"] and t["closed"] and (cls is None or t["cls"] == cls)]
        def area(ps): return abs(sum(ps[i][0] * ps[i - 1][1] - ps[i - 1][0] * ps[i][1] for i in range(len(ps)))) / 2
        return max(shs, key=lambda t: area(t["pts"]))["pts"]
    depth = lambda k: back + 7.0 + k * 2.0
    # the sand reaches sideways behind the curtains to the box's walls: its ends are drawn out to them
    ground = scene_part("dio-scene-ground", "dio-ground")
    gx0, gx1 = min(p[0] for p in ground), max(p[0] for p in ground)
    wall_x = [115.0 + (x - 115.0) / s for x in (left[1], right[0])]
    ground = [(wall_x[0] if x < gx0 + 0.5 else wall_x[1] if x > gx1 - 0.5 else x, y) for x, y in ground]
    cutout("diorama-ground", "backbox-scene-ground", ground, depth(0), 2.0, plywood, u_range=(gx0 + 1.0, gx1 - 1.0))
    for child in ("dio-ball-1", "dio-ball-2", "dio-ball-big"):
        cutout("diorama-ball", "backbox-scene-balls", scene_part(child, None), depth(1), 2.0, plywood)
    for child in ("dio-pin-1", "dio-pin-2", "dio-pin-3", "dio-pin-4"):
        cutout("diorama-pin", "backbox-scene-pins", scene_part(child, "dio-pin"), depth(2), 2.0, plywood)
    cutout("diorama-wheel", "backbox-scene-wheel", scene_part("dio-wheel", None), depth(3), 2.0, plywood, alpha=True)
    # the frame is built of parts: a plate of FRAME_THICK at the casing's front, its columns and its valance
    # with their picture, and the same columns at the casing's back, their picture read from behind
    Z = lambda yd: top - (yd - VB[1]) * s
    frame = lambda ch, cl: [t for t in SHAPES if ch in t["chain"] and t["cls"] == cl]
    columns = [frame(ch, "dio-red")[0]["pts"] for ch in ("dio-frame-column-left", "dio-frame-column-right")]
    for pts in columns:
        xs, ys = [p[0] for p in pts], [p[1] for p in pts]
        rect = [(min(xs), min(ys)), (max(xs), min(ys)), (max(xs), y_back), (min(xs), y_back)]
        cutout("diorama-frame-column", "backbox-frame-plate", rect, front, FRAME_THICK, red)
        cutout("diorama-frame-column", "backbox-frame-plate", rect, rear, -FRAME_THICK, red, mirror=True)
    cutout("diorama-frame-valance", "backbox-frame-plate", frame("dio-frame-valance", "dio-red")[0]["pts"], front, FRAME_THICK, red)
    # the rope along the valance's lower edge lies on the plate as a cord
    edge = frame("dio-frame-valance", "dio-edge-back")[0]
    cord = edge["sw"] / 2 * s
    cu = bpy.data.curves.new("diorama-frame-rope", "CURVE"); cu.dimensions = "3D"
    cu.bevel_depth = cord / 10; cu.bevel_resolution = 3; cu.use_fill_caps = True
    sp = cu.splines.new("POLY"); sp.points.add(len(edge["pts"]) - 1)
    for pt, (x, yd) in zip(sp.points, edge["pts"]):
        pt.co = (X(x) / 10, -(front + cord) / 10, Z(yd) / 10, 1.0)
    link(bpy.data.objects.new("diorama-frame-rope", cu), c, mat("diorama-rope", "#5c6069", rough=0.6))
    # the roof: a red block as long as the star band, from the ceiling up to the crown, over the casing's whole
    # depth; on both faces the lower beam across the whole frame, its upper half red and its lower half black, and
    # the black upper beam at half its drawn height; the stars stand on the front
    def span(pts):
        return min(p[0] for p in pts), max(p[0] for p in pts), min(p[1] for p in pts), max(p[1] for p in pts)
    roof_black = sorted(frame("dio-frame-roof", "dio-black"), key=lambda t: span(t["pts"])[2])
    bx0, bx1, by0, by1 = span(frame("dio-frame-roof", "dio-red")[0]["pts"])
    box("diorama-roof", X(bx0), X(bx1), rear, front, ceiling + 1.5, crown_base, red)
    for y_face, out in ((front, 1.0), (rear, -1.0)):
        ya, yb = sorted((y_face - out * FRAME_THICK, y_face + out * BEAM_PROUD))
        x0, x1, y0, y1 = span(roof_black[1]["pts"])          # the lower beam
        box("diorama-roof-beam", X(x0), X(x1), ya, yb, Z(y1), Z((y0 + y1) / 2), dark)
        box("diorama-roof-beam", X(x0), X(x1), ya, yb, Z((y0 + y1) / 2), Z(y0), red)
        x0, x1, y0, y1 = span(roof_black[0]["pts"])          # the upper beam
        box("diorama-roof-beam", X(x0), X(x1), ya, yb, Z(y1), Z((y0 + y1) / 2), dark)
        # the dark ledges at the columns' feet
        for ch in ("dio-frame-column-left", "dio-frame-column-right"):
            for t in frame(ch, "dio-black"):
                x0, x1, y0, y1 = span(t["pts"])
                if y0 > -40.0:
                    box("diorama-frame-ledge", X(x0), X(x1), ya, yb, Z(y1), Z(y0), dark)
    silver = mat("diorama-star-silver", "#dcdfe4", rough=0.35, metal=0.6)
    for t in frame("dio-frame-roof", "dio-silver") + frame("dio-frame-roof", "dio-gold"):
        cx = sum(p[0] for p in t["pts"]) / len(t["pts"])
        figure("diorama-roof-star", X(cx), front + BEAM_PROUD / 2, 0.0, [(X(x) - X(cx), Z(y)) for x, y in t["pts"]],
               BEAM_PROUD, c, silver if t["cls"] == "dio-silver" else gold_line)
    # the stage floor's dark edge in front of the stage
    x0, x1, y0, y1 = span(frame("dio-frame-floor", "dio-black")[0]["pts"])
    box("diorama-frame-floor", X(x0), X(x1), front - FRAME_THICK, front + BEAM_PROUD, Z(y1), Z(y0), dark)
    # the curtains are boards of their own, CURTAIN_THICK thick, hanging behind the plate; their outer edges reach
    # in behind the columns and their tops behind the valance
    cloth = mat("diorama-curtain-back", "#8a1a22", rough=0.7)
    for side_id, inner in (("dio-frame-curtain-left", 1.0), ("dio-frame-curtain-right", -1.0)):
        pts = frame(side_id, "dio-curtain")[0]["pts"]
        edge_x = min(p[0] for p in pts) if inner > 0 else max(p[0] for p in pts)
        pts = [(edge_x - inner * 3.0 if abs(x - edge_x) < 0.05 else x, y) for x, y in pts]
        cutout("diorama-curtain", "backbox-frame", pts, front - FRAME_THICK - 0.2, CURTAIN_THICK, cloth)


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


def separator_top():
    """The inlane separators' height, the top of their R, on which the blue ramp and the red track end."""
    return SEPARATOR_HEIGHT


def two_tone(o, face, edge, axis):
    """Paints an upright plywood part: the faces whose normal runs along axis (0 for x, 1 for y) face, every
    other face edge."""
    me = o.data
    me.materials.clear(); me.materials.append(face); me.materials.append(edge)
    for p in me.polygons:
        p.material_index = 0 if abs(p.normal[axis]) > 0.9 else 1
    return o


def rail_arch(name, pts, c):
    """The arch of the manual on the straight upright piece of an inlane rail, the wall between inlane and outlane:
    the wall F30, the thicker plywood, stands closed and flush with the rail's face toward the outlane, red on
    that face and black on its face toward the inlane; the arch F31, a thinner red plywood, lies directly on that
    black face, which shows through its opening. Every edge is black, and their tops are level with the R's."""
    red = mat("separator-red", "#c8323a", rough=0.45)
    black = mat("separator-edge", "#141416", rough=0.5)
    # the upright piece is as wide as the rail's rounded top end; the outlane runs outside it, between the rail
    # and the cabinet's wall or the shooter lane
    y0 = min(p[1] for p in pts)
    straight = [p for p in pts if p[1] < y0 + 3.0]
    x0, x1 = min(p[0] for p in straight), max(p[0] for p in straight)
    right = x0 > 108.5
    outward = 1.0 if right else -1.0
    outlane_face = x1 if right else x0
    length = SEPARATOR_ARCH_LENGTH
    # both arches stand centered on the hole through which the red track drops its ball
    ring = [p for t in SHAPES if "wire-ramp-drop" in t["chain"] for p in t["pts"]]
    front = (min(p[1] for p in ring) + max(p[1] for p in ring)) / 2 + length / 2
    height = separator_top() - H["rail"]
    leg, rise, r = SEPARATOR_LEG, SEPARATOR_ARCH, SEPARATOR_CORNER
    a = (length - 2 * leg) / 2
    corner = lambda cu, cz, t0: [(cu + r * math.cos(t0 + math.pi / 2 * k / 6), cz + r * math.sin(t0 + math.pi / 2 * k / 6)) for k in range(7)]
    rim = corner(length - r, height - r, 0.0) + corner(r, height - r, math.pi / 2)
    opening = [(length / 2 - a * math.cos(math.pi * k / 16), rise * math.sin(math.pi * k / 16)) for k in range(17)]
    tw, ta = SEPARATOR_WALL_THICK, SEPARATOR_ARCH_THICK
    wall = figure(f"{name}-wall", outlane_face - outward * tw / 2, front, -90.0, [(0.0, 0.0), (length, 0.0)] + rim, tw, c, None,
                  z0=H["rail"])
    me = wall.data
    me.materials.clear(); me.materials.append(red); me.materials.append(black)
    for poly in me.polygons:
        poly.material_index = 0 if poly.normal[0] * outward > 0.9 else 1
    arch = figure(f"{name}-arch", outlane_face - outward * (tw + ta / 2), front, -90.0,
                  [(0.0, 0.0)] + opening + [(length, 0.0)] + rim, ta, c, None, z0=H["rail"])
    two_tone(arch, red, black, 0)


def separator(name, pts, c):
    """An inlane separator after the manual (C6, C7): the letter R standing on the board along the drawing's
    separator, and on the right one the cradle C22 that reaches out under the red track. The R is yellow, the
    cradle red, both with black edges."""
    red = mat("separator-red", "#c8323a", rough=0.45)
    edge = mat("separator-edge", "#141416", rough=0.5)
    xs, ys = [p[0] for p in pts], [p[1] for p in pts]
    right = min(xs) > 108.5
    inner = min(xs) if right else max(xs)
    separator_r(name, pts, c, mat("separator-yellow", "#f1c94a", rough=0.45), edge)
    if right:
        top = separator_top()
        # C22 hangs on the R's outer face with its tab in the R, crosses under the red track and ends in a
        # notched tip; its underside is a moon's curve
        reach = SEPARATOR_CRADLE
        under = [(reach - reach * (1 - math.cos(math.pi / 2 * k / 12)) - SEPARATOR_THICK / 2,
                  top - 1.5 - (SEPARATOR_CRADLE_DEPTH - 1.5) * math.sin(math.pi / 2 * k / 12)) for k in range(13)]
        tip = [(reach - 4.0, top), (reach - 4.0, top - 1.5), (reach - 2.5, top - 1.5), (reach - 2.5, top),
               (reach - 1.0, top), (reach - 1.0, top - 1.5), (reach, top - 1.5)]
        cradle_x = inner + SEPARATOR_THICK
        y_mid = (min(ys) + max(ys)) / 2
        profile = [(-SEPARATOR_THICK / 2, top)] + tip + under
        cradle = figure(f"{name}-cradle", cradle_x, y_mid, 0.0, profile, 3.0, c, None)
        two_tone(cradle, red, edge, 1)
        gold = mat("separator-star", "#f1c94a", rough=0.4)
        for u, z, rs in ((3.0, top - 3.2, 1.4), (7.5, top - 3.0, 1.1)):
            star = [(u + (rs if k % 2 == 0 else rs * 0.45) * math.sin(math.pi * k / 5),
                     z + (rs if k % 2 == 0 else rs * 0.45) * math.cos(math.pi * k / 5)) for k in range(10)]
            for side in (1, -1):
                figure(f"{name}-star", cradle_x, y_mid + side * 1.55, 0.0, star, 0.1, c, gold)


def separator_r(name, pts, c, face, edge, z0=0.0):
    """The letter R of an inlane separator in the ROKR sign's typeface, standing along the drawing's separator
    at height z0 (mm), its stem at the front and its bowl toward the diorama, its faces painted face and its
    edges edge."""
    xs, ys = [p[0] for p in pts], [p[1] for p in pts]
    font = next((f for f in SEPARATOR_FONT if os.path.exists(f)), None)
    cu = bpy.data.curves.new(name, "FONT"); cu.body = "R"; cu.extrude = SEPARATOR_THICK / 20
    if font:
        cu.font = bpy.data.fonts.load(font, check_existing=True)
    tmp = bpy.data.objects.new(name + "-text", cu); bpy.context.scene.collection.objects.link(tmp)
    bpy.context.view_layer.update()
    me = bpy.data.meshes.new_from_object(tmp.evaluated_get(bpy.context.evaluated_depsgraph_get()))
    bpy.data.objects.remove(tmp); bpy.data.curves.remove(cu)
    bm = bmesh.new(); bm.from_mesh(me)
    gx0, gx1 = min(v.co.x for v in bm.verts), max(v.co.x for v in bm.verts)
    gy0 = min(v.co.y for v in bm.verts)
    gy1 = max(v.co.y for v in bm.verts)
    k = SEPARATOR_HEIGHT / 10 / (gy1 - gy0)
    # it stands in the middle of the drawing's separator
    front = (max(ys) + min(ys)) / 2 + (gx1 - gx0) * k * 10 / 2
    # its face toward the board's middle stays where the drawing has it, and its thickness lies outside it,
    # so the lane between it and the slingshot keeps its width
    inner = min(xs) if min(xs) > 108.5 else max(xs)
    xc = inner + (SEPARATOR_THICK / 2 if inner > 108.5 else -SEPARATOR_THICK / 2)
    for v in bm.verts:
        gx, gy, gz = v.co
        # the glyph's width runs from the front toward the diorama, its height up, its extrusion across
        v.co = Vector(W(xc + gz * 10, front - (gx - gx0) * k * 10, z0 + (gy - gy0) * k * 10))
    bm.normal_update()
    for f in bm.faces:
        f.material_index = 0 if abs(f.normal.x) > 0.9 else 1
    me.materials.clear(); me.materials.append(face); me.materials.append(edge)
    bm.to_mesh(me); bm.free()
    return link(bpy.data.objects.new(name, me), c)


def bend(z, m_in, m_out, r):
    """A straight floor bending at height z from slope m_in to slope m_out, over r mm either side: a parabola,
    over t mm from the bend, that meets both straight pieces at their slopes."""
    return lambda t: z + m_in * t + (m_out - m_in) * (t + r) ** 2 / (4 * r)


def blue_course():
    """The blue ramp's course: its share of the course at a point, its length in mm, where along it the red hoop
    stands, and the bends at the fire hoop and at the red hoop as functions of mm from each."""
    blue_line = [tuple(p) for p in by("ramp-blue", "ramp-shine")[0]["pts"]]
    f = polyline_param(blue_line)
    crown_first = blue_line[0][1] < 150
    course = lambda x, y: f(x, y) if crown_first else 1 - f(x, y)
    length = sum(math.dist(a, b) for a, b in zip(blue_line, blue_line[1:]))
    red = [p for s in SHAPES if "ramp-blue-bracket" in s["chain"] for p in s["pts"]]
    top_at = course(*centroid(red))
    m_in = (30.0 - 0.02) / (BLUE_ENTRY_Y[0] - BLUE_ENTRY_Y[1])
    m_up = (BLUE_TOP - 30.0) / (top_at * length)
    m_down = (separator_top() + H["ramp_floor"] - BLUE_TOP) / ((1 - top_at) * length)
    return course, length, top_at, bend(30.0, m_in, m_up, BLUE_BEND), bend(BLUE_TOP, m_up, m_down, BLUE_BEND)


def blue_height():
    """The blue ramp's floor height (mm) over a point of its course; its end rests on the left inlane's R."""
    # from the fire hoop it climbs in a straight line to its top at the red hoop and falls from there in one
    # straight line through the other hoops to its end; at both hoops it bends over BLUE_BEND either side
    course, length, top_at, fire, crest = blue_course()
    pb = profile([(0, 30), (top_at, BLUE_TOP), (1.0, separator_top() + H["ramp_floor"])])
    def z(x, y):
        t = course(x, y) * length
        if t < BLUE_BEND:
            return fire(t)
        if abs(t - top_at * length) < BLUE_BEND:
            return crest(t - top_at * length)
        return pb(t / length)
    return z


def build_posts_and_rubbers():
    c = coll("posts")
    post = mat("post", "#17171b", rough=0.4, coat=0.5)
    spacer = mat("spacer-clear", "#eef3f6", rough=0.15, transmission=0.6)
    screw = mat("post-screw", "#3a3d42", rough=0.35, metal=0.9)
    pit = mat("pit", "#050506", rough=0.9)
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
            # a post is a clear spacer round a screw, the screw's black socket head on top, the drawing's circle; a
            # slingshot's screw
            # runs up through its cover inside the slingshot's own spacer, and its head sits on the cover
            sling = any(inside((x, y), t["pts"]) for t in SHAPES if t["cls"] == "sling")
            if sling:
                top = H["sling"] + 0.2
            else:
                cyl("post", x, y, SPACER_R, 0, top, c, spacer, verts=32)
            cyl("post-screw", x, y, 0.8, 0, top, c, screw, verts=12)
            cyl("post-head", x, y, r, top, top + H["head"], c, post, verts=24)
            cyl("post-socket", x, y, 0.7, top + H["head"] - 0.05, top + H["head"] + 0.02, c, pit, verts=6)
        elif cl == "sleeve":
            # a round rubber ring stretched around the two posts' spacers; its cord is the drawing's band less the
            # screw's head, halved
            (ax, ay), (bx, by_) = s["pts"][0], s["pts"][-1]
            rp = min((t["circle"] for t in SHAPES if t["cls"] == "post" and "circle" in t),
                     key=lambda q: math.hypot(q[0] - ax, q[1] - ay))[2]
            cord = (s["sw"] / 2 - rp) / 2
            d = math.atan2(by_ - ay, bx - ax); r = SPACER_R + cord
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
            # the bell hangs from a center rod, clear of the board so it can ring
            x, y, r = s["circle"]
            dome = mat("bell", "#1a1a1e", rough=0.15, metal=0.9)
            cyl("bell-rod", x, y, 1.5, 0, H["bell_top"], c, mat("rod", "#9aa0a8", rough=0.4, metal=1.0), 16)
            # the bell is one casting: its profile, a long straight skirt that rounds over BELL_CORNER into the
            # flat top, turned round its rod
            k = BELL_CORNER
            prof = [(r, H["bell_rim"]), (r, H["bell_top"] - k)]
            prof += [(r - k + k * math.cos(math.radians(a)), H["bell_top"] - k + k * math.sin(math.radians(a))) for a in range(10, 91, 10)]
            prof += [(0.0, H["bell_top"])]
            me = bpy.data.meshes.new("bell"); bm = bmesh.new()
            vs = [bm.verts.new((u / 10, 0.0, z / 10)) for u, z in prof]
            es = [bm.edges.new((a, b)) for a, b in zip(vs, vs[1:])]
            bmesh.ops.spin(bm, geom=vs + es, cent=(0, 0, 0), axis=(0, 0, 1), steps=48, angle=2 * math.pi, use_merge=True)
            bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
            # the faces look outward, so the wall thickens inward and the outside stays on the drawing's circle
            for f in bm.faces:
                ctr = f.calc_center_median()
                if f.normal.dot(Vector((ctr.x, ctr.y, ctr.z - H["bell_top"] / 20))) < 0:
                    f.normal_flip()
            bm.normal_update()
            bm.to_mesh(me); bm.free()
            o = link(bpy.data.objects.new("bell", me), c, dome)
            o.location = W(x, y, 0.0)
            for p in o.data.polygons: p.use_smooth = True
            o.modifiers.new("shell", "SOLIDIFY").thickness = 0.08
            sphere("bell-screw", x, y, H["bell_top"], 1.6, c, mat("screw", "#d9dde2", rough=0.2, metal=1.0)).scale.z = 0.5
            # the conductive tape around it, flush with the board
            annulus("bell-foil", x, y, r + 0.05, r + 6.06, H["foil"], c, foil_material())
        elif cl == "target-base" and any(ch.startswith("target-") for ch in ch):
            face = next(t["pts"] for t in SHAPES if t["cls"] == "target-face" and t["chain"] == s["chain"])
            prism("target-base", target_forward(s["pts"], face), 0, 15.0, c, mat("target-base", "#2b2d33", rough=0.5))
        elif cl == "target-face" and any(ch.startswith("target-") for ch in ch):
            prism("target-face", target_forward(s["pts"], s["pts"]), 2.0, 13.0, c, mat("target-face", "#f2c230", rough=0.35, coat=0.5))
        elif cl == "ir-eye" and "ir-sensors" in ch:
            # a small hole in the board where the IR sensor looks up through it
            x, y, r = s["circle"]
            cyl("ir-hole", x, y, 1.5, 0.0, 0.05, c, mat("pit", "#050506", rough=0.9), 16)
        elif cl == "wall-red":
            # the red wall between the shooter lane and the playfield, F56 on the manual's page 40, runs on down to
            # the front wall; it climbs with the lane's floor, SHOOTER_WALL_ABOVE over it, and stands at least as
            # high as the apron at its low end
            xs = [p[0] for p in s["pts"]]; ys = [p[1] for p in s["pts"]]
            steps = [min(ys) + (427.3 - min(ys)) * k / 60 for k in range(61)]
            wall = [(max(xs), y) for y in steps] + [(min(xs), y) for y in steps[::-1]]
            prism("shooter-wall", wall, 0, 0, c, mat("shooter-wall", "#b8323a", rough=0.35, coat=0.6),
                  hi_fn=lambda x, y: max(H["apron_hi"], shooter_z(x, y) + SHOOTER_WALL_ABOVE))


# side views of the two figures, (u, z) in mm, u along the plane
# the elephant after the manual, reared up and facing +u: its hind legs on the stand, one front foot on a red
# pedestal, the other front leg raised, its trunk curled up over its head
ELEPHANT = [(-4.6, 0), (-1.6, 0), (-1.2, 4.0), (1.0, 6.0), (3.2, 5.4), (3.6, 3.5), (5.8, 3.5), (5.6, 6.8), (6.6, 8.6),
            (8.6, 9.0), (8.8, 10.6), (6.8, 10.8), (6.4, 12.6), (7.6, 14.0), (9.2, 15.0), (10.2, 17.0), (10.4, 19.4),
            (9.8, 21.2), (8.8, 21.6), (8.6, 20.8), (9.4, 20.2), (9.4, 18.0), (8.6, 16.4), (7.6, 15.8)] + [
            (4.8 + 3.4 * math.cos(math.radians(a)), 18.6 + 3.4 * math.sin(math.radians(a))) for a in range(-40, 201, 15)] + [
            (1.2, 15.6),
            (-1.0, 14.4), (-3.6, 11.8), (-5.4, 8.6), (-5.6, 5.4), (-5.0, 2.0)]
ELEPHANT_PEDESTAL = (4.7, 2.0, 1.5, 3.5)   # the front foot's round pedestal, a truncated cone: u, radius at the foot and the top, height
# the seal sits facing -u, its nose up under the ball, its tail curled up behind it
# the elephant's paint: a red blanket with a gold trim on its back, a red headdress with a gold plume, the eye
ELEPHANT_BLANKET = [(-4.6, 6.8), (1.6, 9.2), (1.0, 14.6), (-3.4, 11.6)]
ELEPHANT_TRIM = [(-4.7, 6.2), (1.7, 8.6), (1.7, 9.4), (-4.7, 7.0)]
ELEPHANT_HEADDRESS = [(3.8, 21.4), (5.8, 21.4), (5.6, 22.8), (4.0, 22.8)]
ELEPHANT_PLUME = [(4.4, 22.6), (5.4, 22.6), (4.8, 24.8)]
ELEPHANT_STAND_STEP = 1.5   # mm, how much smaller the stand's upper disc is than the lower one
ELEPHANT_EYE = [(6.4 + 0.5 * math.cos(2 * math.pi * k / 12), 19.0 + 0.5 * math.sin(2 * math.pi * k / 12)) for k in range(12)]
# the big ear on the side of the head, a shade darker, and a short tusk at the trunk's root
ELEPHANT_EAR = [(3.8 + 2.2 * math.cos(2 * math.pi * k / 16), 17.4 + 3.2 * math.sin(2 * math.pi * k / 16)) for k in range(16)]
ELEPHANT_TUSK = [(7.2, 15.0), (8.9, 14.1), (7.5, 15.7)]
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
                # the stand: two discs, a white one and a smaller light blue one on it, each with a gold rim
                rim = mat("elephant-stand-rim", "#d4a640", rough=0.35, metal=0.6)
                z0 = H["sling"]
                for rr, colr in ((r, "#f2efe6"), (r - ELEPHANT_STAND_STEP, "#8fcbe6")):
                    cyl("elephant-stand", x, y, rr, z0, z0 + 2.0, c, rim)
                    cyl("elephant-stand", x, y, rr - 0.8, z0 + 2.0, z0 + 2.2, c, mat("elephant-stand-" + colr[1:], colr, rough=0.45))
                    z0 += 2.2
                mirror = x > 108.5
                flip = (lambda pts: [(-u, z) for u, z in pts]) if mirror else (lambda pts: pts)
                pu, pr0, pr1, ph = ELEPHANT_PEDESTAL
                cyl("elephant-pedestal", x + (-pu if mirror else pu), y, pr0, z0, z0 + ph, c,
                    mat("elephant-pedestal", "#e0414c", rough=0.45), 48, r_top=pr1)
                figure("elephant", x, y, 0.0, flip(ELEPHANT), 3.0, c, mat("elephant", "#9097a0", rough=0.5), z0=z0)
                for name, prof, colr, thick in (("elephant-blanket", ELEPHANT_BLANKET, "#c8323a", 3.4), ("elephant-trim", ELEPHANT_TRIM, "#e3c46c", 3.4),
                                                ("elephant-ear", ELEPHANT_EAR, "#7a828c", 3.4), ("elephant-tusk", ELEPHANT_TUSK, "#f2ead8", 3.4),
                                                ("elephant-headdress", ELEPHANT_HEADDRESS, "#c8323a", 3.4), ("elephant-plume", ELEPHANT_PLUME, "#e3c46c", 1.0),
                                                ("elephant-eye", ELEPHANT_EYE, "#141416", 3.6)):
                    figure(name, x, y, 0.0, flip(prof), thick, c, mat(name, colr, rough=0.45), z0=z0)
            elif cl == "rail" and "inlane-separators" in ch:
                separator(next(n for n in ch if n.startswith("separator-")), s["pts"], c)
            elif cl == "rail":
                prism("rail", s["pts"], 0, H["rail"], c, rail)
                if "inlane-rails" in ch:
                    rail_arch("separator-" + ("right" if min(p[0] for p in s["pts"]) > 108.5 else "left"), s["pts"], c)
            elif cl == "rail-gem": prism("rail-gem", s["pts"], H["rail"], H["rail"] + 0.3, c, gem)
            elif cl == "rail-gem-y": prism("rail-gem", s["pts"], H["rail"], H["rail"] + 0.3, c, gem_y)
            elif cl == "screw" and "circle" in s: cyl("flipper-screw", *s["circle"], H["flipper"], H["flipper"] + 0.4, c, screw, 24)
    # the apron is a plate the ball rolls under on its way into the drain
    stripes = stripes_material("apron", "#e2505b", "#c8323f")
    band = mat("apron-band", "#3c4250", rough=0.4)
    side = mat("apron-side", "#555c69", rough=0.4)
    star = mat("apron-star", "#d9b65a", rough=0.3, metal=0.8)
    for s in placed("apron"):
        cl = s["cls"]
        if cl == "apron": uv_along_x(prism("apron", s["pts"], H["apron_lo"], H["apron_hi"], c, stripes), APRON_STRIPE)
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
                if pid == "cover-left":
                    cover_left_wood(s["pts"], c)
                else:
                    prism(pid + "-wood", s["pts"], H["plastic"], H["wood_hi"], c, wood())
                prism(pid, s["pts"], H["wood_hi"], H["cover"], c, gray)
            elif cl in paint:
                prism(pid + "-paint", s["pts"], H["cover"], H["cover"] + 0.15, c, paint[cl], cap_only=True)
    for s in placed("block-top-left"):
        if s["cls"] == "block": prism("block-top-left", s["pts"], 0, H["cover"], c, mat("block-red", "#a3232c", rough=0.4))


BLUE_BEND = 6.0        # mm either side of the fire hoop and of the red hoop over which the blue floor bends
BLUE_TOP = 34.0        # mm, the blue ramp's floor at its highest point, the red hoop
BLUE_ENTRY_Y = (146.5, 85.5)   # mm: the blue entrance lane's start on the board and its end at the fire hoop
CROC_JAW_TOP = 10.0    # mm, the lower jaw's height at the hinge over its snout, the bent strip F38's shape
CROC_RIBS = 7          # the pedestal's ribs side by side, F48, F47, F40, C29, F39, F50 and F49
CROC_DROP = 18.0       # mm ahead of the half loop's foot where the red track starts in the mouth
CROC_CLEAR = 0.5       # mm the ball keeps from the red track in the mouth, climbing under it and dropping onto it
CROC_TAIL = 12.0       # mm the middle rib runs on behind the hinge as the tail, an estimate from the manual's picture
TRACK_FALL = 0.006     # mm the red track falls for every mm along the board
TRACK_WALL = 2.0       # mm, the red track's walls, as thick as the drawing's end wall
# the inlane separators (C6, C7) are a letter R standing along the separator, its feet in the drawing's two holes
SEPARATOR_FONT = ("C:/Windows/Fonts/georgiab.ttf",)  # the ROKR sign's Georgia Bold, the first one found
SEPARATOR_THICK = 4.5  # mm, the R's thickness, by eye
SEPARATOR_HEIGHT = 17.0  # mm, the R's height, by eye; the blue ramp and the red track end on its top
SEPARATOR_ARCH_LENGTH = 22.7  # mm, the arch F30/F31 along its inlane rail, as long as the R, by eye after the manual
SEPARATOR_LEG = 4.0    # mm, the width of the arch's legs
SEPARATOR_WALL_THICK = 4.0  # mm, the wall F30, by eye
SEPARATOR_ARCH_THICK = 2.0  # mm, the thinner arch F31 on it, by eye
SEPARATOR_ARCH = 6.0   # mm, the height of the opening between them
SEPARATOR_CORNER = 3.0  # mm, the radius of the arch's top corners
SEPARATOR_CRADLE = 14.0  # mm, how far C22 reaches out from the right R's outer face
SEPARATOR_CRADLE_DEPTH = 8.0  # mm, C22's depth below the R's top at the R
SPACER_R = 2.3     # mm, a post's clear spacer: wider than the screw's head, narrower than a slingshot's (estimate)
TARGET_FORWARD = 3.0     # mm a stand-up target stands toward the playfield's middle from the drawing, clear of its posts


def target_forward(pts, face):
    """A stand-up target's outline moved TARGET_FORWARD toward the playfield's middle, in front of the screws and
    rubbers beside it, and shortened along the face so it ends flush with the outer edge of the rear screw's head,
    the one further up the board. face is the target's face outline, which sets the direction along it."""
    d = TARGET_FORWARD if sum(p[0] for p in face) / len(face) < 108.5 else -TARGET_FORWARD
    moved = [(p[0] + d, p[1]) for p in pts]
    f = [(p[0] + d, p[1]) for p in face]
    a, b = max(((p, q) for p in f for q in f), key=lambda pq: math.dist(*pq))
    ax = Vector((b[0] - a[0], b[1] - a[1])).normalized()
    mid = Vector((sum(p[0] for p in f) / len(f), sum(p[1] for p in f) / len(f)))
    posts = sorted((t["circle"] for t in SHAPES if t["cls"] == "post" and "circle" in t),
                   key=lambda q: math.dist(q[:2], mid))[:2]
    rx, ry, rr = min(posts, key=lambda q: q[1])
    t_post = (Vector((rx, ry)) - mid).dot(ax)
    end = t_post + (rr if t_post > 0 else -rr)
    out = []
    for x, y in moved:
        t = (Vector((x, y)) - mid).dot(ax)
        if (end > 0 and t > end) or (end < 0 and t < end):
            x, y = x - ax.x * (t - end), y - ax.y * (t - end)
        out.append((x, y))
    return out


LANE_FENCE_TOP = 10.0     # mm, the top of the picket fence round the blue ramp's entrance lane
BELL_CORNER = 4.0     # mm, the radius where a bell's skirt rounds into its top
COVER_L11_END = 75.0     # mm, where L11 under the left cover ends, after the manual's page 27


def clip_y(pts, y0, y1):
    """The part of a closed outline between y0 and y1 (Sutherland-Hodgman against the two half planes)."""
    def cut(poly, keep, y):
        out = []
        for a, b in zip(poly, poly[1:] + poly[:1]):
            ina, inb = keep(a), keep(b)
            if ina: out.append(a)
            if ina != inb:
                t = (y - a[1]) / (b[1] - a[1])
                out.append((a[0] + (b[0] - a[0]) * t, y))
        return out
    return cut(cut([tuple(p) for p in pts], lambda q: q[1] >= y0, y0), lambda q: q[1] <= y1, y1)


def cover_left_wood(pts, c):
    """The wood under the left gray cover is two plates on pegs, as the manual's page 27 shows: L11 fills the top
    corner under the cover's curved inner edge, and E6 has the outline of the left guide's clear plastic under it."""
    prism("cover-left-wood", clip_y(pts, -1.0, COVER_L11_END), H["plastic"], H["wood_hi"], c, wood())
    for g in by("guide-left", "plastic"):
        prism("cover-left-wood", g["pts"], H["plastic"], H["wood_hi"], c, wood())


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
    """The side plates of the blue ramp's entrance lane, from the photo of the part: on the right a red plate with
    a salmon wave crest along its top, on the left a taller red plate with a flame crest, each with a gray face
    on its outer side that carries small lit windows, jester caps and circus tents; under the high end a gold
    stand."""
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
    # the right plate: red body, salmon waves on top
    ribbon("lane-wall-right", right, wr, -3.0, 7.5, c, red, zfn=floor_z)
    wave = lambda x, y: 7.5 + 2.6 * abs(math.sin(math.pi * (146.5 - y) / 9.0))
    ribbon("lane-wall-crest", right, wr, 7.4, 7.5, c, salmon, zfn=floor_z, top_fn=wave)
    # both plates carry the painting on their outer side: a gray face with lit windows, and between them jester
    # caps and tents in turn
    cap = [(-1.8, 0), (1.8, 0), (2.4, 2.2), (1.0, 1.2), (0.0, 2.8), (-1.0, 1.2), (-2.4, 2.2)]
    tent = [(-1.9, 0), (1.9, 0), (1.9, 1.2), (0.0, 3.2), (-1.9, 1.2)]
    for line, w, sign in ((right, wr, 1.0), (left, wl, -1.0)):
        out = outward(line, sign)
        face = offset_line(line, out * (w / 2 + 0.05), False)
        ribbon("lane-wall-face", [(p.x, p.y) for p in face], 0.1, -2.0, 6.6, c, gray, zfn=floor_z)
        acc = [0.0]
        for p0, p1 in zip(line, line[1:]): acc.append(acc[-1] + math.dist(p0, p1))
        k = 0
        for d in range(8, int(acc[-1]) - 4, 9):
            i = min(range(len(acc)), key=lambda j: abs(acc[j] - d))
            a0, a1 = line[max(i - 1, 0)], line[min(i + 1, len(line) - 1)]
            ang = math.degrees(math.atan2(a1[1] - a0[1], a1[0] - a0[0]))
            fx, fy = face[i].x, face[i].y
            nx, ny = -math.sin(math.radians(ang)), math.cos(math.radians(ang))
            sgn = 1.0 if ((fx - line[i][0]) * nx + (fy - line[i][1]) * ny) > 0 else -1.0
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
    ribbon("lane-fence", fence_path, 1.5, -2.0, LANE_FENCE_TOP, c, fence_material(), uv=True)


def build_ramps():
    c = coll("ramps")

    def ramp(pid, floor_cls, wall_cls, zfn, color_floor, color_wall, wall_h=H["ramp_wall"], wall_lo=-H["ramp_floor"],
             clip=lambda pts: pts):
        fm = mat(pid + "-floor", color_floor, rough=0.3, coat=0.5)
        wm = mat(pid + "-wall", color_wall, rough=0.3, coat=0.5)
        outlines = [clip(sh["pts"]) for sh in by(pid, floor_cls)]
        built = []
        if floor_cls == "launch-floor":
            # drawn even-odd: the largest outline is the floor, the smaller one the slot into the rollover lanes
            def area(ps): return abs(sum(ps[i][0] * ps[i - 1][1] - ps[i - 1][0] * ps[i][1] for i in range(len(ps)))) / 2
            outlines.sort(key=area, reverse=True)
            fl = surface(pid + "-floor", outlines[0], zfn, H["ramp_floor"], c, fm)
            built.append(fl)
            for h_pts in outlines[1:]:
                cut = prism(pid + "-hole", h_pts, -H["ramp_floor"] - 3, 3, c, None, zfn=zfn)
                cut.hide_render = True; cut.hide_viewport = True
                bo = fl.modifiers.new("hole", "BOOLEAN"); bo.object = cut; bo.operation = "DIFFERENCE"
        else:
            for pts in outlines:
                built.append(surface(pid + "-floor", pts, zfn, H["ramp_floor"], c, fm))
        for s in by(pid, wall_cls):
            # a wall is drawn as a thin outline around it; it is built along its middle line
            mid, width = centerline(clip(s["pts"]))
            built.append(ribbon(pid + "-wall", mid, max(width, 1.0), wall_lo, wall_h, c, wm, zfn=zfn))
        return [o for o in built if o]

    # the blue ramp climbs from the playfield through the fire hoop, turns, and ends in the air above the left inlane
    blue_z = blue_height()
    # its rails are flat strips lying 3 mm above the floor at the ball's middle, held only by the hoops
    ramp("ramp-blue", "ramp-blue", "blue-wall", blue_z, "#3f7fc4", "#1d3c7a", wall_h=6.0, wall_lo=3.0)
    # the entrance lane climbs in a straight line from the playfield, a hair above it so the two surfaces do
    # not fight, to the fire hoop, where the blue floor starts at 30 mm
    fire = blue_course()[3]
    def centre_z(x, y):
        if BLUE_ENTRY_Y[1] - y > -BLUE_BEND:
            return fire(BLUE_ENTRY_Y[1] - y)
        u = max(0.0, min(1.0, (BLUE_ENTRY_Y[0] - y) / (BLUE_ENTRY_Y[0] - BLUE_ENTRY_Y[1])))
        return 0.02 + 29.98 * u
    ramp("ramp-blue", "ramp-blue-dark", "none", centre_z, "#4fb0e0", "#1f4674")
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
    # it ends where the fence it rests on ends, square to its course
    fence = next(t["pts"] for t in SHAPES if t["cls"] == "comb-back")
    end = Vector(max(fence, key=lambda q: q[0]))
    beyond = Vector(GREEN_LINE[-1]) - end; beyond.normalize()
    def at_fence_end(pts):
        keep = lambda q: q[0] < end.x - 40.0 or (Vector(q) - end).dot(beyond) <= 0.0
        out = []
        for a, b in zip(pts, pts[1:] + pts[:1]):
            a, b = Vector(a), Vector(b)
            if keep(a): out.append((a.x, a.y))
            if keep(a) != keep(b):
                t = (end - a).dot(beyond) / (b - a).dot(beyond)
                q = a + (b - a) * t
                out.append((q.x, q.y))
        return out
    ramp("ramp-orbit-left", "ramp-green", "green-wall", green_z, "#3f5e48", "#2f4a39", wall_h=H["ramp_wall"] / 2,
         clip=at_fence_end)

    # the gray launch lane continues the shooter lane, crosses over the green track and ends under the blue ramp
    # a constant, flat climb along its course, continuing the shooter lane's floor
    gray_course = polyline_param([(224.0, GRAY_START_Y), (222.0, 90.0), (208.0, 50.0), (172.0, 38.0), (132.0, 52.0)])
    gray_start = shooter_z(224.0, GRAY_START_Y)
    gray_z = lambda x, y: gray_start + gray_course(x, y) * (H["gray_end"] - gray_start)
    # its walls stay low; where it ends under the blue ramp, whatever of it would reach into the blue floor is cut
    # away, GRAY_UNDER_BLUE under that floor
    gray = ramp("ramp-orbit-right", "launch-floor", "launch-wall", gray_z, "#3a404b", "#6a7382", wall_h=H["ramp_wall"] / 2)
    for k, poly in enumerate(BLUE_FLOOR):
        # the cutter follows the blue floor on the same net, so its underside bends with the floor's
        cut = surface(f"ramp-blue-cutter-{k}", poly, lambda x, y: blue_z(x, y) + 10.0, 10.0 + H["ramp_floor"] + GRAY_UNDER_BLUE, c, None)
        cut.hide_render = True; cut.hide_viewport = True
        for o in gray:
            bo = o.modifiers.new(f"under-blue-{k}", "BOOLEAN"); bo.object = cut; bo.operation = "DIFFERENCE"

    # the crocodile: the ball climbs its lower jaw, turns in a half loop at the back of the mouth and leaves
    # on the red track, under the open upper jaw that carries the eyes
    green = mat("crocodile", "#3f6b45", rough=0.5)
    red = mat("track-red", "#e0414c", rough=0.45)
    kick = clean(by("ramp-kicker", "kicker")[0]["pts"])
    hinge = ((kick[0][0] + kick[1][0]) / 2, (kick[0][1] + kick[1][1]) / 2)
    # the head's axis runs from the hinge through the middle of the two nostrils, which sit on it side by side;
    # the snout is the jaw's farthest point along it
    nostrils = [t["circle"] for t in by("ramp-kicker", "hole") if "circle" in t]
    nose = (sum(h[0] for h in nostrils) / len(nostrils), sum(h[1] for h in nostrils) / len(nostrils))
    out = Vector((nose[0] - hinge[0], nose[1] - hinge[1])).normalized()
    span = max((q[0] - hinge[0]) * out.x + (q[1] - hinge[1]) * out.y for q in kick)
    snout = (hinge[0] + out.x * span, hinge[1] + out.y * span)
    u = -out
    # the red track falls in one straight line from the mouth to its end on the right inlane's R, TRACK_FALL for
    # every mm; the lower jaw climbs as high as the half loop needs to set the ball onto it at the mouth
    ties = [t["pts"] for t in by("wire-ramp", "tie")]
    ties.sort(key=lambda t: (t[0][1] + t[-1][1]) / 2)
    first = ((ties[0][0][0] + ties[0][-1][0]) / 2, (ties[0][0][1] + ties[0][-1][1]) / 2)
    y0 = first[1]
    # its underside rests on the R's top where the R ends at the front
    rail = next(t["pts"] for t in SHAPES if "inlane-separators" in t["chain"] and t["cls"] == "rail" and t["pts"][0][0] > 108)
    rest_y = max(p[1] for p in rail)
    z_mouth = separator_top() + H["ramp_floor"] + TRACK_FALL * (rest_y - y0)
    loop_r = 8.0
    # the crocodile stands on a pedestal of ribs (F48, F47, F40, C29, F39, F50, F49 on F42, manual p. 35) that
    # tips the whole head back about its snout: the lower jaw keeps its shape and its tip stays flush on the
    # board, and the head rises until the half loop sets the ball onto the red track at the mouth
    jaw_top = CROC_JAW_TOP
    jaw_v = lambda x, y: max(0.0, min(1.0, ((x - snout[0]) * u.x + (y - snout[1]) * u.y) / span))
    jaw_shape = lambda v: 0.02 + (jaw_top - 0.02) * (3 * v * v - 2 * v ** 3) + jaw_top * (v ** 3 - v * v)
    v_a = jaw_v(hinge[0] - u.x * loop_r, hinge[1] - u.y * loop_r)
    # the ball climbs the lower jaw, runs over the palate in a half loop and on under the upper jaw, and drops
    # onto the red track, which starts in the mouth CROC_DROP ahead of the loop's foot: there it lies midway
    # between the climbing ball's top and the upper jaw's ball's bottom, the pedestal raising the head until
    # that middle is the track's height. Both bounds rise alike as the head tips, so the gap is fixed.
    v_d = jaw_v(hinge[0] - u.x * (loop_r + CROC_DROP), hinge[1] - u.y * (loop_r + CROC_DROP))
    climb = jaw_shape(v_d) + 2 * RBALL + H["ramp_floor"] + CROC_CLEAR
    under = jaw_shape(v_a) + 2 * loop_r + 8.0 * min(1.0, CROC_DROP / span) - 2 * RBALL - CROC_CLEAR
    assert under > climb, "the red track has no room in the mouth"
    jaw_tip = (z_mouth - (climb + under) / 2) / v_d
    # every other part of the head tips with the jaw: it rises by the tip's share at its place along the jaw,
    # measured from the half loop's foot, where the head's heights are taken
    jaw_dz = lambda x, y: jaw_tip * (jaw_v(x, y) - v_a)
    def croc_z(x, y):
        """The lower jaw leaves the playfield flush and level at the snout, a hair above it, and climbs without a
        bend to the hinge, where it has the slope of a straight jaw of the same height, tipped back by the
        pedestal."""
        v = jaw_v(x, y)
        return jaw_shape(v) + jaw_tip * v
    surface("crocodile-jaw", kick, croc_z, 2.0, c, green)
    # the pedestal closes the head down to the board, so no ball rolls in beneath it from behind: its ribs stand
    # side by side along the jaw, their tops following its underside. Its outline in steps of 1 mm keeps the
    # ribs' tops inside the 2 mm jaw along the jaw's curve. It stands 1.5 mm inside the jaw's edge, so a ball
    # beside the head rolls with its lower half under the jaw. The jaw's back corner lies closer to the rubber at
    # post 022 than the ball is wide, and a ball held off by a pedestal flush with the edge rests there.
    dense = []
    for a, b in zip(kick, kick[1:] + kick[:1]):
        k = max(1, math.ceil(math.dist(a, b)))
        dense += [(a[0] + (b[0] - a[0]) * i / k, a[1] + (b[1] - a[1]) * i / k) for i in range(k)]
    def area(ps): return abs(sum(ps[i][0] * ps[i - 1][1] - ps[i - 1][0] * ps[i][1] for i in range(len(ps))))
    dense = min((offset_line(dense, d, True) for d in (1.5, -1.5)), key=area)
    side_n = Vector((-u.y, u.x))
    across = [(p[0] - snout[0]) * side_n.x + (p[1] - snout[1]) * side_n.y for p in dense]
    lo, hi = min(across), max(across)
    # the ribs grow longer toward the middle: the outer ones end under the hinge, the middle one runs on behind
    # the head as the crocodile's tail, its top falling to the board
    heading = math.degrees(math.atan2(u.y, u.x))
    mid = (CROC_RIBS - 1) / 2
    for k in range(CROC_RIBS):
        a0, a1 = lo + (hi - lo) * k / CROC_RIBS, lo + (hi - lo) * (k + 1) / CROC_RIBS
        band = clip_band(dense, (snout[0], snout[1]), side_n, a0, a1)
        if len(band) < 3:
            continue
        # the rib is as long as the outline is along both its faces, so no corner stands out of it
        reach = []
        for edge in (a0, a1):
            line = clip_band(dense, (snout[0], snout[1]), side_n, edge - 0.02, edge + 0.02)
            if len(line) >= 3:
                reach.append([(p[0] - snout[0]) * u.x + (p[1] - snout[1]) * u.y for p in line])
        if not reach:
            continue
        u0 = max(0.0, max(min(r) for r in reach))
        u1 = min(span, min(max(r) for r in reach))
        tail = CROC_TAIL * (1 - abs(k - mid) / mid)
        ac = (a0 + a1) / 2
        at = lambda t: (snout[0] + u.x * t + side_n.x * ac, snout[1] + u.y * t + side_n.y * ac)
        top = [(t, max(0.05, croc_z(*at(t)) - 2.1)) for t in [u0 + (u1 - u0) * i / 24 for i in range(25)]]
        z_back = top[-1][1]
        top += [(u1 + tail * i / 12, max(1.0, z_back * (1 - i / 12) ** 1.5)) for i in range(1, 13)] if tail > 0.5 else []
        profile_ = top + [(top[-1][0], 0.0), (u0, 0.0)]
        figure("crocodile-pedestal", snout[0] + side_n.x * ac, snout[1] + side_n.y * ac, heading, profile_,
               (a1 - a0) - 0.1, c, green)
    half = math.dist(kick[0], kick[1]) / 2
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
    for q, _, _ in path:
        q.z += jaw_dz(q.x, q.y)
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
        o.modifiers.new("solid", "SOLIDIFY").thickness = 0.2
        return o
    # the lower jaw, the half loop at the back of the head and the upper jaw are one kerf-bent wooden strip, F38
    # on the manual's page 51: the jaw bends up round the loop between the cheeks and runs on forward, as wide as
    # the bend, as the upper jaw; it opens toward the snout and ends round, with the nostrils
    w_strip = half - 1.5
    sweep("crocodile-loop", path[:25], green, walls=False, w=w_strip)
    upper_z = z_top
    fwd = Vector((-u.x, -u.y)); sd = Vector((-fwd.y, fwd.x))
    ahead = lambda x, y: (x - a2.x) * fwd.x + (y - a2.y) * fwd.y
    open_fn = lambda x, y: 8.0 * max(0.0, min(1.0, ahead(x, y) / span))
    holes = [t["circle"] for t in by("ramp-kicker", "hole") if "circle" in t]
    reach_jaw = max(ahead(hx, hy) + hr for hx, hy, hr in holes) + 3.0
    tip = a2 + fwd * (reach_jaw - w_strip)
    outline = [a2 + sd * w_strip, tip + sd * w_strip]
    outline += [tip + sd * (w_strip * math.cos(math.pi * k / 16)) + fwd * (w_strip * math.sin(math.pi * k / 16)) for k in range(1, 16)]
    outline += [tip - sd * w_strip, a2 - sd * w_strip]
    prism("crocodile-upper-jaw", [(q.x, q.y) for q in outline], 0.0, 2.0, c, green, zfn=lambda x, y: upper_z + open_fn(x, y) + jaw_dz(x, y))
    # each cheek is one plywood plate on the bent part's side, its outer face flush with the jaw's edge: along
    # the lower jaw to the open tip, up to the upper jaw, and round the back of the head around the half loop
    def side_plate(back, tip):
        e = Vector((tip[0] - back[0], tip[1] - back[1])); length = e.length; e.normalize()
        at = lambda d: (back[0] + e.x * d, back[1] + e.y * d)
        floor = lambda d: croc_z(*at(d))
        centre_u = loop_r
        reach, mouth = 34.0, 40.0
        prof = [(length, floor(length) - 2.0), (centre_u, floor(centre_u) - 2.0)]
        for t in (math.radians(-90 + 10 * k) for k in range(19)):
            d = centre_u - (loop_r + 0.5) * math.cos(t)
            prof.append((d, z_a + loop_r + (loop_r + 0.5) * math.sin(t) + jaw_dz(*at(d))))
        prof += [(reach, upper_z + open_fn(*at(reach)) + jaw_dz(*at(reach))), (mouth, floor(mouth) + 6.0), (length, floor(length) + 3.0)]
        n = Vector((-e.y, e.x))
        if n.dot(Vector((sum(q[0] for q in kick) / len(kick) - back[0], sum(q[1] for q in kick) / len(kick) - back[1]))) < 0:
            n = -n
        figure("crocodile-side", back[0] + n.x * 0.75, back[1] + n.y * 0.75, math.degrees(math.atan2(e.y, e.x)), prof, 1.5, c, green)
    side_plate(kick[1], kick[2])
    side_plate(kick[0], kick[-1])
    for hx, hy, hr in [t["circle"] for t in by("ramp-kicker", "hole") if "circle" in t]:
        z_n = upper_z + 2.0 + open_fn(hx, hy) + jaw_dz(hx, hy)
        cyl("crocodile-nostril", hx, hy, hr, z_n, z_n + 0.05, c, mat("pit", "#050506"))
    eye = [(2.6 * math.cos(2 * math.pi * k / 24), 2.6 + 2.6 * math.sin(2 * math.pi * k / 24)) for k in range(24)]
    pupil = [(0.9 * math.cos(2 * math.pi * k / 16), 3.0 + 0.9 * math.sin(2 * math.pi * k / 16)) for k in range(16)]
    ang = math.degrees(math.atan2(side.y, side.x))
    for d in (-4.5, 4.5):
        ex, ey = hinge[0] - u.x * 5 + side.x * d, hinge[1] - u.y * 5 + side.y * d
        z_e = upper_z + 2.0 + open_fn(ex, ey) + jaw_dz(ex, ey)
        figure("crocodile-eye", ex, ey, ang, eye, 1.5, c, mat("eye-white", "#f2efe6", rough=0.4), z0=z_e)
        figure("crocodile-pupil", ex, ey, ang, pupil, 1.7, c, mat("pupil", "#1a3a8a", rough=0.4), z0=z_e)

    # the red track: a wooden floor between two bent wooden walls, falling from the mouth onto the right
    # inlane's R, where the ball drops through the round hole at its end
    # inside the mouth the red track runs from the half loop out to where the drawing's track begins
    tie_w = math.dist(ties[0][0], ties[0][-1]) / 2
    # one smooth curve: leaving the loop toward the snout, arriving along the track's own direction
    second = ((ties[1][0][0] + ties[1][-1][0]) / 2, (ties[1][0][1] + ties[1][-1][1]) / 2)
    p0 = Vector((a2.x - u.x * CROC_DROP, a2.y - u.y * CROC_DROP, z_mouth))
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
    track_z = lambda x, y: z_mouth - TRACK_FALL * (max(y, y0) - y0)
    for s in by("wire-ramp", "rail-red"):
        ys = [p[1] for p in s["pts"]]
        if max(ys) - min(ys) < 30:
            prism("track-end-wall", s["pts"], -1.6, 4.0, c, red, zfn=track_z)
            end_y = min(ys)
    # the drawing's cross strips, each from one edge to the other, with rows between them every millimeter or
    # less, so the floor and its walls follow the profile's bends
    # the track starts in the mouth: the curve's cross sections come first, so floor, slot and walls run on in
    # one piece from there to the end
    pairs = []
    for q, tan, up in curve[:-1]:
        sd = tan.cross(up).normalized()
        pairs.append(((q.x + sd.x * tie_w, q.y + sd.y * tie_w), (q.x - sd.x * tie_w, q.y - sd.y * tie_w)))
    prev = pairs[-1]
    for t in ties:
        a, b = tuple(t[0]), tuple(t[-1])
        if prev and math.dist(a, prev[0]) + math.dist(b, prev[1]) > math.dist(a, prev[1]) + math.dist(b, prev[0]):
            a, b = b, a
        if prev:
            n = max(1, math.ceil(max(math.dist(a, prev[0]), math.dist(b, prev[1])) / 1.0))
            pairs += [(lerp2(prev[0], a, k / n), lerp2(prev[1], b, k / n)) for k in range(1, n)]
        pairs.append((a, b)); prev = (a, b)
    # the floor (F15) has a slot along its middle; the ball runs on its edges
    me = bpy.data.meshes.new("track-floor"); bm = bmesh.new()
    rows = []
    for a, b in pairs:
        f = min(0.19, 3.5 / math.dist(a, b))           # half the slot over the floor's width: 38 %, at most 7 mm
        sa = (a[0] + (b[0] - a[0]) * (0.5 - f), a[1] + (b[1] - a[1]) * (0.5 - f))
        sb = (a[0] + (b[0] - a[0]) * (0.5 + f), a[1] + (b[1] - a[1]) * (0.5 + f))
        rows.append([bm.verts.new(W(*q, track_z(*q))) for q in (a, sa, sb, b)])
    # the slot runs from the track's start in the mouth to the hole
    for r0, r1 in zip(rows, rows[1:]):
        bm.faces.new((r0[0], r0[1], r1[1], r1[0]))
        bm.faces.new((r0[2], r0[3], r1[3], r1[2]))
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    bm.normal_update()
    for f in bm.faces:
        if f.normal.z < 0: f.normal_flip()
    bm.to_mesh(me); bm.free()
    floor = link(bpy.data.objects.new("track-floor", me), c, red)
    edges = ([a for a, _ in pairs], [b for _, b in pairs])
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
    # the floor's slot runs on through the end piece into the hole
    f = min(0.19, 3.5 / math.dist(la, lb))
    sa, sb = lerp2(la, lb, 0.5 - f), lerp2(la, lb, 0.5 + f)
    dh = hy - (la[1] + lb[1]) / 2
    slot = [(sa[0], sa[1] - 0.5), (sb[0], sb[1] - 0.5), (sb[0], sb[1] + dh), (sa[0], sa[1] + dh)]
    slit = prism("track-slot-cutter", slot, -5.0, 5.0, c, None, zfn=track_z)
    slit.hide_render = True; slit.hide_viewport = True
    bo = end_floor.modifiers.new("slot", "BOOLEAN"); bo.object = slit; bo.operation = "DIFFERENCE"
    edges[0].append(ea); edges[1].append(eb)
    # both side walls are as thick as the end wall and stand outside the floor's edges, flush with its ends
    for edge, other in ((edges[0], edges[1]), (edges[1], edges[0])):
        line = []
        for p, q in zip(edge, other):
            d = Vector(p) - Vector(q); d.normalize()
            line.append((p[0] + d.x * TRACK_WALL / 2, p[1] + d.y * TRACK_WALL / 2))
        ribbon("track-wall", line, TRACK_WALL, -1.6, 4.0, c, red, zfn=track_z)
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
# Layouts measured off a photo of the left side, rectified so that u runs 0..1768 from where the wall leaves
# the tower to its front end and v runs 0..460 from the wall's top edge down to the table.
SIDE_W, SIDE_H = 1768.0, 460.0
SIDE_UNIT = 0.237     # mm a layout unit along the wall, (433.7 - 14.45) / 1768, which round pieces keep in both directions
SIDE_WINDOWS = [(37, 58, 360), (80, 100, 345), (122, 147, 320), (168, 195, 282), (212, 240, 240), (265, 295, 275),
                (320, 348, 310), (375, 402, 332), (430, 460, 343), (490, 518, 340), (548, 580, 325), (610, 642, 298),
                (673, 710, 255), (745, 780, 293), (815, 852, 318), (890, 925, 328), (970, 1010, 322), (1055, 1095, 285),
                (1140, 1185, 318)]
SIDE_TABS = [(75, 180, 425, 440), (310, 430, 425, 440), (590, 735, 418, 436), (930, 1110, 415, 432)]
# the lower edge of the curtain, from its back end to the plain red field before the drummer's niche
SWAG_EDGE = [(10, 292), (60, 270), (120, 235), (160, 195), (185, 160), (205, 150), (240, 150), (280, 200), (330, 230),
             (400, 250), (450, 255), (520, 250), (580, 225), (630, 190), (660, 172), (690, 168), (725, 168), (780, 198),
             (850, 222), (920, 232), (990, 228), (1040, 210), (1080, 200), (1110, 190)]
MEDALLIONS = [(205, 78, 40), (685, 92, 40), (1083, 110, 40)]     # all three the same size
NICHE_X0, NICHE_X1 = 1340.0, 1712.0
DRUMMER_SCALE = (1715 - 1414) / (1712 - 1340)     # the drummer shrinks as his niche does along the wall
DRUMMER_SHIFT = 100.0     # layout units the niche lies further on in the photo of the side than in the layout


def _side_height(y):
    """Height of the side wall's top edge above the table at y (mm): the edge follows the tilted playfield."""
    a = math.radians(TILT_DEG)
    return -y * math.sin(a) + wall_top(y) * math.cos(a) - TABLE_Z


# The side panel is a trapezoid: its bottom stands level on the table and its top falls with the playfield. Each
# column of the layout stands upright in the room at its place along the wall, from the table to the top edge.
SIDE_Y0, SIDE_Y1 = 14.45, 433.7     # mm: the tower's front face, where the painted wall begins, and the front end


# where the layout's marks lie along the wall, after the photo of the whole side: the gray strip at the back, the
# three medallions and the drummer's niche, as (layout u, u on the wall); between them the layout stretches evenly
SIDE_WARP = [(0, 0), (10, 35), (205, 272), (685, 819), (1083, 1211), (1340, 1414), (1712, 1715), (1768, 1768)]


def side_warp(u):
    """The layout's u moved to its place along the wall, by SIDE_WARP."""
    for (a, a2), (b, b2) in zip(SIDE_WARP, SIDE_WARP[1:]):
        if u <= b or b == SIDE_WARP[-1][0]:
            return a2 + (b2 - a2) * (u - a) / (b - a)


def _room_to_frame(y, z):
    """A point of the room, y along the wall and z up (mm), in the tipped machine's frame."""
    a = math.radians(TILT_DEG)
    return y * math.cos(a) - z * math.sin(a), y * math.sin(a) + z * math.cos(a)


def _side_room(u, v):
    """A point of the side layout in the room, (y, z) in mm: u moved to its place along the wall, v between the
    wall's top edge and the table."""
    y = SIDE_Y0 + (SIDE_Y1 - SIDE_Y0) * side_warp(u) / SIDE_W
    return y, TABLE_Z + _side_height(y) * (1 - v / SIDE_H)


def side_point(u, v):
    """A point of the side layout as (y, z) in mm in the tipped machine's frame, as the side walls are built."""
    return _room_to_frame(*_side_room(u, v))


def side_plate(name, pts_uv, d0, d1, c, material, rigid=None):
    """A flat piece laid on the outer face of both side walls, from d0 to d1 mm out from the face. With rigid, a
    point (u, v) of the layout and optionally a scale, the piece keeps its shape: it moves with that point and is
    drawn round it at one scale in both directions, SIDE_UNIT mm a layout unit times the scale."""
    if rigid is None:
        pts = [side_point(u, v) for u, v in pts_uv]
    else:
        uc, vc, k = rigid if len(rigid) == 3 else (*rigid, 1.0)
        yc, zc = _side_room(uc, vc)
        pts = [_room_to_frame(yc + (u - uc) * SIDE_UNIT * k, zc - (v - vc) * SIDE_UNIT * k) for u, v in pts_uv]
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


def clip_u(pts, u0):
    """The part of a closed outline of the side layout at u0 and beyond."""
    out = []
    for a, b in zip(pts, pts[1:] + pts[:1]):
        if a[0] >= u0: out.append(a)
        if (a[0] >= u0) != (b[0] >= u0):
            t = (u0 - a[0]) / (b[0] - a[0])
            out.append((u0, a[1] + (b[1] - a[1]) * t))
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
    # it runs on to under the front medallion, whose pennant covers its end, as at the other two
    edge = [q for q in SWAG_EDGE if q[0] <= MEDALLIONS[-1][0]]
    for name, lo, hi, material in (("side-fringe", 0, 24, gold), ("side-line-blue", 24, 30, blue), ("side-line-white", 62, 68, white)):
        band = normal_edge(edge, lo) + normal_edge(edge, hi)[::-1]
        # moved across the edge, a band reaches back past the curtain's start; it ends where the curtain does
        side_plate(name, clip_u(band, edge[0][0]), 1.2, 1.4, c, material)
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
        side_plate("side-pennant", [(cu - w, top), (cu + w, top), (cu + w, bottom - 18), (cu, bottom), (cu - w, bottom - 18)], 2.0, 3.0, c, red, rigid=(cu, cv))
        side_plate("side-pennant-fringe", [(cu - w, bottom - 45), (cu + w, bottom - 45), (cu + w, bottom - 18), (cu, bottom), (cu - w, bottom - 18)], 3.0, 3.2, c, gold, rigid=(cu, cv))
        side_plate("side-medallion", arc_uv(cu, cv, r, 0, 360, 32)[:-1], 3.4, 4.4, c, black, rigid=(cu, cv))
        side_plate("side-medallion-red", arc_uv(cu, cv, r * 0.8, 0, 360, 32)[:-1], 4.4, 4.6, c, red, rigid=(cu, cv))
        star = [(cu + (r * 0.6 if k % 2 == 0 else r * 0.25) * math.cos(math.radians(90 + 36 * k)),
                 cv - (r * 0.6 if k % 2 == 0 else r * 0.25) * math.sin(math.radians(90 + 36 * k))) for k in range(10)]
        side_plate("side-star", star, 4.6, 4.8, c, gold, rigid=(cu, cv))
    # the drummer's niche at the front: a wooden arch frame, the ochre niche, the drummer on his blue stand
    cx = (NICHE_X0 + NICHE_X1) / 2
    spring = 256.0   # where the niche's arch springs from its posts
    frame = [(NICHE_X0, 445), (NICHE_X0, spring)] + arc_uv(cx, spring, (NICHE_X1 - NICHE_X0) / 2, 180, 0, 24)[1:-1] + [(NICHE_X1, spring), (NICHE_X1, 445)]
    side_plate("side-niche-frame", frame, 0.0, 2.4, c, rail)
    inner = [(NICHE_X0 + 45, 430), (NICHE_X0 + 45, spring)] + arc_uv(cx, spring, (NICHE_X1 - NICHE_X0) / 2 - 45, 180, 0, 24)[1:-1] + [(NICHE_X1 - 45, spring), (NICHE_X1 - 45, 430)]
    side_plate("side-niche-red", inner, 2.4, 2.6, c, red)
    niche = [(NICHE_X0 + 60, 430), (NICHE_X0 + 60, spring)] + arc_uv(cx, spring, (NICHE_X1 - NICHE_X0) / 2 - 60, 180, 0, 24)[1:-1] + [(NICHE_X1 - 60, spring), (NICHE_X1 - 60, 430)]
    side_plate("side-niche", niche, 2.6, 2.8, c, ochre)
    # the drummer after the photo of the side, which shows the niche DRUMMER_SHIFT further on: the drum in front,
    # its rim over the frame's post, and behind it the slim drummer walking toward the front, one arm on the drum
    def at(pts):
        return [(u - DRUMMER_SHIFT, v) for u, v in pts]
    side_plate("side-stand", at([(1545, 400), (1730, 400), (1730, 430), (1545, 430)]), 2.8, 4.0, c, blue, rigid=((NICHE_X0 + NICHE_X1) / 2, 300.0, DRUMMER_SCALE))
    side_plate("side-drummer-legs", at([(1625, 300), (1690, 300), (1725, 390), (1705, 397), (1658, 335), (1602, 397), (1585, 390)]), 2.8, 4.0, c, white, rigid=((NICHE_X0 + NICHE_X1) / 2, 300.0, DRUMMER_SCALE))
    side_plate("side-drummer-coat", at([(1628, 205), (1685, 205), (1690, 300), (1622, 300)]), 2.8, 4.0, c, red, rigid=((NICHE_X0 + NICHE_X1) / 2, 300.0, DRUMMER_SCALE))
    side_plate("side-drummer-head", at(arc_uv(1668, 183, 19, 0, 360, 20)[:-1]), 2.8, 4.0, c, skin, rigid=((NICHE_X0 + NICHE_X1) / 2, 300.0, DRUMMER_SCALE))
    side_plate("side-drummer-hat", at([(1650, 168), (1690, 168), (1698, 138), (1655, 138)]), 4.0, 4.4, c, red, rigid=((NICHE_X0 + NICHE_X1) / 2, 300.0, DRUMMER_SCALE))
    side_plate("side-drummer-plume", at([(1688, 142), (1710, 116), (1719, 123), (1698, 148)]), 4.0, 4.4, c, gold, rigid=((NICHE_X0 + NICHE_X1) / 2, 300.0, DRUMMER_SCALE))
    side_plate("side-drum", at(arc_uv(1490, 250, 48, 0, 360, 32)[:-1]), 4.0, 5.4, c, black, rigid=((NICHE_X0 + NICHE_X1) / 2, 300.0, DRUMMER_SCALE))
    side_plate("side-drum-head", at(arc_uv(1490, 250, 40, 0, 360, 32)[:-1]), 5.4, 5.6, c, white, rigid=((NICHE_X0 + NICHE_X1) / 2, 300.0, DRUMMER_SCALE))
    side_plate("side-drummer-arm", at([(1630, 215), (1545, 246), (1549, 258), (1632, 230)]), 5.6, 5.8, c, red, rigid=((NICHE_X0 + NICHE_X1) / 2, 300.0, DRUMMER_SCALE))
    side_plate("side-ledge", [(1290, 425), (NICHE_X1, 425), (NICHE_X1, 450), (1290, 450)], 0.0, 4.5, c, mat("side-ledge", "#7a1a20", rough=0.5))


# ---------------------------------------------------------------- the cabinet's front
# Layout measured off a photo of the front panel: u runs from its left edge (80) to its right edge (1420),
# v down from the top of the gray cover strip (1005). One scale for both directions, from the panel's width.
FRONT_U0, FRONT_U1, FRONT_V0 = 80.0, 1420.0, 1005.0
SHOOTER_LANE_END = 427.33  # mm, the front wall's inner face at the shooter lane's foot
PLUNGER_IN = 10.0          # mm the plunger's rod reaches into the shooter lane at rest, an estimate
# the middle of the ball resting on the rod's tip, on the lane's axis
PLUNGER_REST = (224.4, SHOOTER_LANE_END - PLUNGER_IN - RBALL)
PLUNGER_REACH = 18.0      # mm of rod and spring between the mount and the knob, an estimate from the photos
PLUNGER_ROD_R = 1.5       # mm, the rod's radius, an estimate
PLUNGER_SPRING_R = 3.5    # mm, the spring's radius to its wire's middle, an estimate
PLUNGER_SPRING_WIRE = 0.7 # mm, the spring's wire, an estimate
PLUNGER_SPRING_TURNS = 9
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
    # a gold star sits in the star's hole, a little smaller than the hole
    gold_star = [(752 + (30 if k % 2 == 0 else 12.5) * math.cos(math.radians(90 + 36 * k)),
                  1132 - (30 if k % 2 == 0 else 12.5) * math.sin(math.radians(90 + 36 * k))) for k in range(10)]
    front_plate("front-star", gold_star, 0.6, 2.6, c, mat("side-gold", "#d9b04a", rough=0.35, metal=0.5))
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
    # the plunger: a steel rod on the axis of the ball resting on it, from the ball's back PLUNGER_IN into the
    # shooter lane through the front wall and the wooden mount centred on it, a spring round it from the mount to
    # the knob on its end
    x, y_ball = PLUNGER_REST
    z = shooter_z(x, y_ball) + RBALL
    pu = FRONT_U0 + (x + 8.38) / FRONT_SCALE
    z_top, z_table = front_point(pu, FRONT_V0)[1], front_point(pu, 1430.0)[1]
    pv = FRONT_V0 + (z_top - z) / (z_top - z_table) * (1430.0 - FRONT_V0)
    front_plate("front-plunger-mount", [(pu - 25, pv - 75), (pu + 25, pv - 75), (pu + 25, pv + 75), (pu - 25, pv + 75)], 4.1, 6.6, c, wood_m)
    mount = FRONT_FACE + 6.6
    knob_r = 8.0
    sphere("plunger-knob", x, mount + PLUNGER_REACH + knob_r, z, knob_r, c, knob)
    steel = mat("rod", "#9aa0a8", rough=0.4, metal=1.0)
    rod = bpy.data.meshes.new("plunger-rod"); bm = bmesh.new()
    tip = y_ball + RBALL
    bmesh.ops.create_cone(bm, cap_ends=True, segments=16, radius1=PLUNGER_ROD_R / 10, radius2=PLUNGER_ROD_R / 10,
                          depth=(mount + PLUNGER_REACH + knob_r - tip) / 10)
    bmesh.ops.rotate(bm, verts=bm.verts, cent=(0, 0, 0), matrix=Matrix.Rotation(math.pi / 2, 3, "X"))
    bm.to_mesh(rod); bm.free()
    link(bpy.data.objects.new("plunger-rod", rod), c, steel).location = W(x, (tip + mount + PLUNGER_REACH + knob_r) / 2, z)
    cu = bpy.data.curves.new("plunger-spring", "CURVE"); cu.dimensions = "3D"
    cu.bevel_depth = PLUNGER_SPRING_WIRE / 20; cu.bevel_resolution = 2
    n = PLUNGER_SPRING_TURNS * 24
    sp = cu.splines.new("POLY"); sp.points.add(n)
    # its origin sits at the mount, so a film stretches it along the rod by scaling it
    origin = Vector(W(x, mount, z))
    for k, pt in enumerate(sp.points):
        t = k / n; a = 2 * math.pi * PLUNGER_SPRING_TURNS * t
        p = Vector(W(x + PLUNGER_SPRING_R * math.cos(a), mount + PLUNGER_SPRING_WIRE / 2 + t * (PLUNGER_REACH - PLUNGER_SPRING_WIRE), z + PLUNGER_SPRING_R * math.sin(a)))
        pt.co = (*(p - origin), 1.0)
    link(bpy.data.objects.new("plunger-spring", cu), c, steel).location = origin


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
        # the stand's bar sits just over the screws' heads, the gold star lower than the coloured one
        bar = H["post"] + H["head"] + 0.5
        figure("deco-star-stand", x, y, ang, [(-2.0, 0), (2.0, 0), (2.0, bar - 2), (half, bar), (half, bar + 2), (-half, bar + 2), (-half, bar), (-2.0, bar - 2)], 1.2, c, wood_m)
        figure("deco-star", x, y, ang, star(5.0, 2.2, -half, bar + 6.5), 2.4, c, mat("deco-star-" + pid, colr, rough=0.4))
        figure("deco-star-hole", x, y, ang, star(2.0, 0.9, -half, bar + 6.5), 2.6, c, dark)
        figure("deco-star-gold", x, y, ang, star(5.5, 2.4, half, bar + 5.0), 2.4, c, gold)
        figure("deco-star-hole", x, y, ang, star(2.2, 1.0, half, bar + 5.0), 2.6, c, dark)
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
# the ball separator after tools/separator/separator.html at its defaults, in mm from the board's top face: two
# pins of PIN_D stand PIN_WALL from the drain wall's face, PIN_SPACING apart along the track. The seesaw below
# carries them through a clevis axle each and rocks about its pivot between them. In the start position the
# front pin stands PIN_UP above the board and holds the front ball; the back pin lies at PIN_DOWN, just under the
# board's face.
PIN_D, PIN_LEN, PIN_WALL, PIN_SPACING, PIN_UP, PIN_DOWN = 2.0, 13.0, 6.5, 9.0, 3.1, -0.4
PIN_AXLE = 10.25                    # from a pin's tip to its axle's middle
AXLE_D, HOLE_D, BOARD_T, CABINET_FLOOR = 1.5, 2.5, 3.9, -31.0
SEESAW_LEN, SEESAW_H, SEESAW_W = 13.0, 3.0, 3.0      # its width across the track is not on the page: an estimate
SEESAW_PIVOT = (PIN_UP + PIN_DOWN) / 2 - PIN_AXLE    # midway between the two axles
FOIL_ACROSS = 8.0     # mm a ball place's copper foil reaches from the drain wall into the track
SEESAW_TILT = math.degrees(math.atan((PIN_UP - PIN_DOWN) / PIN_SPACING))


def store_frame():
    """The ball store's frame: the seesaw's pivot, the unit vector along the track toward the plunger lane, the
    one across it from the drain wall into the track, and the two pins, front first (mm). The front pin stands where
    the drawing puts the front finger, PIN_WALL from the drain wall's face; the back pin PIN_SPACING behind it."""
    fingers = []
    for t in by("drain-ball-store", "seesaw"):
        (ax, ay), (bx, by_) = t["pts"][0], t["pts"][-1]
        fingers.append(((ax + bx) / 2, (ay + by_) / 2))
    fingers.sort(key=lambda q: -q[0])
    (fx, fy), (bx, by_) = fingers
    along = Vector((fx - bx, fy - by_)).normalized()
    wall = next(t for t in SHAPES if t["cls"] == "wood-wall" and "drain-wall-lower" in t["chain"])
    seg = [q for q in wall["pts"] if bx - 9.0 <= q[0] <= fx + 3.0]
    face = min((offset_line(seg, d, False) for d in (wall["sw"] / 2, -wall["sw"] / 2)), key=lambda l: sum(q.y for q in l))
    f = Vector((fx, fy))
    near = min((a + (b - a) * max(0.0, min(1.0, (f - a).dot(b - a) / (b - a).length_squared)) for a, b in zip(face, face[1:])),
               key=lambda q: (q - f).length)
    across = (f - near).normalized()
    front = near + across * PIN_WALL
    back = front - along * PIN_SPACING
    return (front + back) / 2, along, across, [tuple(front), tuple(back)]


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
    """The ball store under the apron: two balls wait in a row on the board against the drain wall, behind the
    separator, whose two pins rise through holes in the board. A DFRobot SER0049 micro servo rocks its seesaw
    through a pushrod. In the start position the front pin stands up and holds the front ball. In the end position
    the back pin holds the back ball, and the front one rolls on into the plunger lane. A copper foil lies under each ball's place, a third one on
    the drain wall behind them, and the break beam's emitter and receiver face each other across the drain. The
    sizes the drawing does not give, the servo's and the beam parts', are estimates."""
    from mathutils import Quaternion
    c = coll("store")
    pin, along, across, fingers = store_frame()
    steel = mat("kicker-steel", "#b9bec6", rough=0.25, metal=1.0)
    plastic = mat("store-plastic", "#2a2d33", rough=0.5)
    white = mat("servo-white", "#e8e8e4", rough=0.4)
    blue = mat("servo-blue", "#2f5d9e", rough=0.45)
    brass = mat("ferrule-brass", "#b08d3c", rough=0.3, metal=0.9)
    tilt = Quaternion(Vector((across.x, -across.y, 0.0)).normalized(), math.radians(SEESAW_TILT))
    # the seesaw in its start pose, turned so that its front end, toward the plunger lane, rises
    if (tilt @ Vector((along.x, -along.y, 0.0))).z < 0: tilt = tilt.inverted()
    box("store-seesaw-beam", (pin.x, pin.y, SEESAW_PIVOT), along, across, (SEESAW_LEN, SEESAW_W, SEESAW_H), c, steel, rot=tilt)
    pf = bpy.data.objects["playfield"]
    for k, (fx, fy) in enumerate(fingers):
        name = "store-finger-front" if k == 0 else "store-finger-back"
        top = PIN_UP if k == 0 else PIN_DOWN
        # the pin, its clevis axle across the track, and the wire ferrule lining its hole in the board
        cyl(name, fx, fy, PIN_D / 2, top - PIN_LEN, top, c, steel, verts=24)
        axle = top - PIN_AXLE
        box(name + "-axle", (fx, fy, axle), along, across, (AXLE_D, SEESAW_W + 2.0, AXLE_D), c, steel)
        cyl("store-ferrule", fx, fy, HOLE_D / 2, -BOARD_T, 0.02, c, brass, verts=24)
        cut = box(name + "-slot", (fx, fy, -2.0), along, across, (HOLE_D, HOLE_D, 8.0), c, None)
        cut.hide_render = True; cut.hide_viewport = True
        bo = pf.modifiers.new(name + "-slot", "BOOLEAN"); bo.object = cut; bo.operation = "DIFFERENCE"
    # the pivot's bearing: two legs from the board's underside, one on each side of the seesaw, its axle through
    # them; a foot below the pivot stands on the cabinet's floor
    for d in (-1, 1):
        leg = pin + across * d * (SEESAW_W / 2 + 1.0)
        box("store-seesaw-bracket", (leg.x, leg.y, (-BOARD_T + SEESAW_PIVOT - 1.2) / 2), along, across,
            (6.6, 1.0, -(SEESAW_PIVOT - 1.2) - BOARD_T), c, plastic)
    box("store-seesaw-pivot", (pin.x, pin.y, SEESAW_PIVOT), along, across, (AXLE_D, SEESAW_W + 4.0, AXLE_D), c, steel)
    foot = pin + across * (SEESAW_W / 2 + 2.3)
    box("store-seesaw-foot", (foot.x, foot.y, (SEESAW_PIVOT - 1.2 + CABINET_FLOOR) / 2), along, across,
        (1.6, 1.6, SEESAW_PIVOT - 1.2 - CABINET_FLOOR), c, plastic)
    # the servo lies under the board past the seesaw toward the plunger lane, its horn turned toward the beam
    servo_at = pin + along * 18.0
    box("store-servo", (servo_at.x, servo_at.y, -20.5), along, across, (23.0, 12.2, 22.5), c, blue)
    box("store-servo-tabs", (servo_at.x, servo_at.y, -12.0), along, across, (32.0, 12.2, 2.5), c, blue)
    shaft = servo_at - along * 6.0
    cyl("store-servo-shaft", shaft.x, shaft.y, 2.4, -9.5, -7.5, c, white, verts=24)
    horn_tip = shaft - along * 6.0
    mid = (shaft + horn_tip) / 2
    box("store-servo-horn", (mid.x, mid.y, -7.0), along, across, (7.5, 3.0, 1.2), c, white)
    beam_end = pin + along * (SEESAW_LEN / 2 - 0.5)
    end_z = SEESAW_PIVOT + (SEESAW_LEN / 2 - 0.5) * math.sin(math.radians(SEESAW_TILT))
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
    # each foil reaches across from the drain wall's face, where the pins stand PIN_WALL off, to beyond the ball
    to_wall = -across * PIN_WALL
    f_at = front_mid + to_wall + across * FOIL_ACROSS / 2
    box("store-foil-front", (f_at.x, f_at.y, flat / 2), along, across, (7.5, FOIL_ACROSS, flat), c, copper)
    back_mid = Vector((bx, by_)) - along * 5.0
    b_at = back_mid + to_wall + across * FOIL_ACROSS / 2
    box("store-foil-back", (b_at.x, b_at.y, flat / 2), along, across, (7.5, FOIL_ACROSS, flat), c, copper)
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
