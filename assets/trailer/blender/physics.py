"""Simulates the steel ball on the machine scene.py builds, for the trailer's runs.

The ball is its center, which keeps the ball's radius off every surface of the machine. A BVH tree of the machine's
meshes, in the frame of machine-tilt, gives the surfaces near it. The flippers turn about their pivots and push the
ball with their speed, the bumpers and the slings kick it. Positions are Blender units in the frame of machine-tilt
(10 mm), so the board lies in z 0 and gravity has a part down the board; times are seconds.
"""
import bpy, math, os
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree

R = 0.45                        # the ball's radius
G = 981.0                       # gravity, units/s²
ROLL = 5.0 / 7.0                # a rolling ball takes 5/7 of the slope's pull
DRAG = 0.02                     # rolling resistance: the share of the speed it takes off every second
DT = 0.001
E_RUBBER, E_HARD, E_FLOOR = 0.7, 0.5, 0.15
BUMPER_KICK = 70.0
BUMPER_READY = 0.03             # s after a kick before a bumper kicks again; its coil pulls in milliseconds
BUMPER = {1: (69.634, 55.664), 2: (104.979, 74.788), 3: (71.218, 95.647)}     # mm of the drawing, as film.py
BUMPER_REACH = 1.62             # the ball's center within this of a bumper's axis touches its skirt
FLIPPER = {"left": ((74.77, 363.97), 1.0), "right": ((142.305, 363.97), -1.0)}
FLIP_UP, FLIP_DOWN, FLIP_ANGLE = 0.016, 0.08, math.radians(48.0)    # its tip swings at 1.6 m/s
E_FLIPPER = 0.5

# names of what the ball never meets: outside the cabinet, flat in the board or a ramp's floor, helpers and the
# moving parts, the spinner's flap among them, which turns away from the ball
SKIP = ("ball-", "film-", "logo-", "tr-", "preview-", "side-", "front-", "backbox", "diorama-", "coffee-table",
        "insert", "drain-diffuser", "ir-hole", "ir-sensor", "store-", "drain-beam", "bumper-plate", "flipper", "plunger-knob", "plunger-spring", "cabinet-", "room", "shooter-arrow", "chevron",
        "lane-icon", "lane-window", "deco-spinner-flap", "deco-spinner-star", "deco-spinner-top")
KEEP = ("cabinet-wall",)
# where the drawing's walls end bluntly in a lane, a guide the ball does not see leads it in, as the kit's lanes do:
# the plunger lane runs into the right orbit, whose outer wall starts 4 mm inside the lane
GUIDES = {"guide-orbit-right": [(230.0, 100.0), (226.4, 80.2)]}
E_GUIDE = 0.2
# the slot in the right orbit's floor, which drops a slow ball onto the top lanes, is drawn 8 to 9.4 mm wide, as
# wide as the ball; the ball sees it 0.5 mm wider all round, so it falls through as the kit's ball does
HOLES = {"ramp-orbit-right-floor": ("ramp-orbit-right-hole", 0.05)}


def mm(x, y, z=0.0):
    return Vector((x / 10.0, -y / 10.0, z / 10.0))


def build_scene(folder):
    """Builds the machine with the scene.py that both films share, in folder. The runs are chaotic as a real ball
    is, so solve.py and trailer.py both build the machine this way: on another machine, even the same parts in
    another order, a run comes out differently."""
    path = os.path.join(folder, "scene.py")
    exec(compile(open(path, encoding="utf-8").read(), path, "exec"), {"BUILD": folder, "__name__": "scene"})
    bpy.context.scene.frame_set(1)


def tilt():
    return bpy.data.objects["machine-tilt"]


def _under_tilt(o):
    p = o.parent
    while p:
        if p.name == "machine-tilt":
            return True
        p = p.parent
    return False


class Machine:
    """The machine's surfaces in the frame of machine-tilt, with the part each polygon belongs to. With view, all
    of the machine a camera has to keep clear of: the cabinet, the diorama and the backbox too."""

    def __init__(self, view=False):
        dg = bpy.context.evaluated_depsgraph_get()
        inv = tilt().matrix_world.inverted()
        faces = []                  # (part, corners)
        for o in bpy.data.objects:
            if o.type not in ("MESH", "CURVE") or o.hide_render:
                continue
            if view:
                if o.name.startswith(("ball-", "tr-", "logo-", "film-", "coffee-table", "room")):
                    continue
            elif not _under_tilt(o) or (o.name.startswith(SKIP) and not o.name.startswith(KEEP)):
                continue
            ev = o.evaluated_get(dg)
            me = ev.to_mesh()
            m = inv @ o.matrix_world
            co = [(m @ v.co).freeze() for v in me.vertices]
            hole = None if view else HOLES.get(o.name)
            outline = _outline(bpy.data.objects[hole[0]], inv) if hole else None
            for p in me.polygons:
                if outline and any(_near_polygon(co[i].to_2d(), outline, hole[1]) for i in p.vertices):
                    continue
                faces.append((o.name, [co[i] for i in p.vertices]))
            ev.to_mesh_clear()
        for name, line in ({} if view else GUIDES).items():
            for (x0, y0), (x1, y1) in zip(line, line[1:]):
                faces.append((name, [mm(x0, y0, 0.0), mm(x1, y1, 0.0), mm(x1, y1, 60.0), mm(x0, y0, 60.0)]))
        # Blender may hand out a modifier's faces in another order from one start to the next, and the tree breaks
        # ties by that order; the faces in a fixed order keep every run the same in every Blender
        def order(face):
            name, cs = face
            k = min(range(len(cs)), key=lambda i: tuple(round(c, 5) for c in cs[i]))
            return name, tuple(tuple(round(c, 5) for c in cs[(k + i) % len(cs)]) for i in range(len(cs)))
        faces.sort(key=order)
        verts, polys, self.owner = [], [], []
        for name, cs in faces:
            k = min(range(len(cs)), key=lambda i: tuple(round(c, 5) for c in cs[i]))
            polys.append(list(range(len(verts), len(verts) + len(cs))))
            verts += [cs[(k + i) % len(cs)] for i in range(len(cs))]
            self.owner.append(name)
        self.bvh = BVHTree.FromPolygons(verts, polys, epsilon=0.0)
        self.gravity = inv.to_3x3() @ Vector((0.0, 0.0, -G))
        self.flippers = {side: Flipper(side, inv) for side in FLIPPER}


class Flipper:
    """A flipper as the convex outline of its mesh in the board's plane, turning about its pivot."""

    def __init__(self, side, inv):
        (px, py), self.turn = FLIPPER[side]
        self.pivot = mm(px, py).to_2d()
        pts = []
        for o in bpy.data.objects:
            if o.type == "MESH" and o.name.split(".")[0] == "flipper":
                m = inv @ o.matrix_world
                vs = [(m @ v.co).to_2d() for v in o.data.vertices]
                cx = sum(v.x for v in vs) / len(vs)
                if (cx < 10.8) == (side == "left"):
                    pts = vs
        self.outline = _hull(pts)
        self.length = max((q - self.pivot).length for q in self.outline)
        self.top = 0.8
        self.presses = []           # (t_on, t_off)
        self.auto = None            # the player's delay: presses the flipper that long after a ball comes onto it

    def angle(self, t):
        """The flipper's angle at t from its rest, by its presses: up in FLIP_UP, down in FLIP_DOWN."""
        a = 0.0
        for on, off in self.presses:
            if t < on:
                break
            if t < off:
                a = max(a, min(1.0, (t - on) / FLIP_UP))
            else:
                held = min(1.0, (off - on) / FLIP_UP)
                a = max(a, held - (t - off) / FLIP_DOWN)
        return max(0.0, a) * FLIP_ANGLE * self.turn

    def rate(self, t, h=1e-4):
        return (self.angle(t + h) - self.angle(t - h)) / (2 * h)

    def nearest(self, p2, ang):
        """The point of the turned outline nearest p2, and whether p2 lies inside it."""
        rot = Matrix.Rotation(ang, 2)
        pts = [self.pivot + rot @ (q - self.pivot) for q in self.outline]
        best, inside = None, True
        for a, b in zip(pts, pts[1:] + pts[:1]):
            ab = b - a
            t = max(0.0, min(1.0, (p2 - a).dot(ab) / max(ab.length_squared, 1e-12)))
            q = a + ab * t
            if best is None or (p2 - q).length < (p2 - best).length:
                best = q
            if ab.x * (p2.y - a.y) - ab.y * (p2.x - a.x) < 0:
                inside = False
        return best, inside


def _hull(pts):
    pts = sorted({(round(p.x, 4), round(p.y, 4)) for p in pts})
    if len(pts) < 3:
        return [Vector(p) for p in pts]
    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lo, hi = [], []
    for p in pts:
        while len(lo) >= 2 and cross(lo[-2], lo[-1], p) <= 0:
            lo.pop()
        lo.append(p)
    for p in reversed(pts):
        while len(hi) >= 2 and cross(hi[-2], hi[-1], p) <= 0:
            hi.pop()
        hi.append(p)
    return [Vector(p) for p in lo[:-1] + hi[:-1]]          # counter-clockwise


def _outline(o, inv):
    """The outline of a prism in the board's plane: its cap with the most corners."""
    m = inv @ o.matrix_world
    cap = max(o.data.polygons, key=lambda p: len(p.vertices))
    return [(m @ o.data.vertices[i].co).to_2d() for i in cap.vertices]


def _near_polygon(p, poly, margin):
    """Whether p lies inside the polygon or within margin of its edges."""
    if (p.x < min(q.x for q in poly) - margin or p.x > max(q.x for q in poly) + margin or
            p.y < min(q.y for q in poly) - margin or p.y > max(q.y for q in poly) + margin):
        return False
    inside = False
    for a, b in zip(poly, poly[1:] + poly[:1]):
        if (a.y > p.y) != (b.y > p.y) and p.x < a.x + (p.y - a.y) * (b.x - a.x) / (b.y - a.y):
            inside = not inside
        ab = b - a
        t = max(0.0, min(1.0, (p - a).dot(ab) / max(ab.length_squared, 1e-12)))
        if (p - (a + ab * t)).length < margin:
            return True
    return inside


def restitution(name):
    if name.startswith("guide-"):
        return E_GUIDE
    return E_RUBBER if "rubber" in name else E_HARD


class Ball:
    def __init__(self, p, v, kicks=()):
        self.p = Vector(p); self.v = Vector(v)
        self.kicks = sorted(kicks, key=lambda k: k[0])     # [(t, velocity)]: the plunger, the scoop's coil
        self.cool = {}               # the time each kicker may kick again
        self.floor = None
        self.ground = ""             # the part it rolls on
        self.out = None              # the time it drained, past the flippers and out of play

    def copy(self):
        b = Ball(self.p.copy(), self.v.copy(), list(self.kicks))
        b.cool = dict(self.cool); b.floor = self.floor.copy() if self.floor is not None else None; b.out = self.out; b.ground = self.ground
        return b


def simulate(machine, balls, t0, t1, kicks=(), stop=None, record=0.002, events=None):
    """Runs one ball or a list of balls from t0 to t1; balls knock into each other. A ball's kicks set its velocity
    at their times; kicks= gives them for a single ball. stop(t, balls) ends the run early when it returns True;
    a list passed as events collects the events as they happen, so stop can read them. A ball that drains leaves
    play and stays where it drained. For one ball, returns its samples [(t, p, v)] every record seconds and the
    events [(t, kind, name, p)]: hits on parts, kicks, bumpers, slings, flippers and the drain. For a list, returns
    a list of samples and the events with the ball's index as a fifth item."""
    single = isinstance(balls, Ball)
    if single:
        balls = [balls]
        if kicks:
            balls[0].kicks = sorted(kicks, key=lambda k: k[0])
    samples = [[] for _ in balls]
    events = [] if events is None else events
    t, next_rec = t0, t0
    while t < t1:
        for i, ball in enumerate(balls):
            while ball.kicks and ball.kicks[0][0] <= t:
                ball.v = Vector(ball.kicks.pop(0)[1]); events.append((t, "kick", "", ball.p.copy(), i))
        if t >= next_rec:
            for i, ball in enumerate(balls):
                samples[i].append((t, ball.p.copy(), ball.v.copy()))
            next_rec += record
        if stop and stop(t, balls[0] if single else balls):
            break
        for i, ball in enumerate(balls):
            if ball.out is not None:
                continue
            _step(machine, ball, t, i, events)
            _player(machine, ball, t)
            if -ball.p.y * 10.0 > 402.0 and ball.p.x * 10.0 < 205.0:
                ball.out = t; events.append((t, "drained", "", ball.p.copy(), i))
        for i in range(len(balls)):
            for j in range(i + 1, len(balls)):
                a, b = balls[i], balls[j]
                # a ball on a ramp runs between its walls and is not knocked out of it by a ball below
                if a.out is not None or b.out is not None or a.ground.startswith("ramp") or b.ground.startswith("ramp"):
                    continue
                d = b.p - a.p
                if d.length < 2 * R:
                    n = d.normalized()
                    a.p -= n * (2 * R - d.length) / 2; b.p += n * (2 * R - d.length) / 2
                    vrel = (b.v - a.v).dot(n)
                    if vrel < 0:
                        imp = n * (1.0 + 0.9) * vrel / 2
                        a.v += imp; b.v -= imp
                        if vrel < -5.0:
                            events.append((t, "balls", "", (a.p + b.p) / 2, i))
        t += DT
    for i, ball in enumerate(balls):
        samples[i].append((t, ball.p.copy(), ball.v.copy()))
    if single:
        return samples[0], [e[:4] for e in events]
    return samples, events


def _player(machine, ball, t):
    """A flipper on auto is pressed when a ball comes down onto it anywhere along its length, as a player would."""
    if ball.p.z > 1.5 or ball.v.y > 0:
        return
    p2 = ball.p.to_2d()
    for fl in machine.flippers.values():
        if fl.auto is None or (fl.presses and t < fl.presses[-1][1] + 0.3):
            continue
        if (p2 - fl.pivot).length > fl.length + R + 0.25:
            continue
        q, inside = fl.nearest(p2, 0.0)
        if (p2 - q).length < R + 0.25:
            fl.presses.append((t + fl.auto, t + fl.auto + 0.3))


def _step(machine, ball, t, i, events):
    """Moves one ball on by DT: gravity, rolling on the floor under it, the surfaces it meets, the kickers."""
    a = machine.gravity.copy()
    if ball.floor is not None:
        an = ball.floor * a.dot(ball.floor)
        a = (a - an) * ROLL + an
    ball.v += a * DT
    ball.v *= 1.0 - DRAG * DT
    ball.p += ball.v * DT
    t += DT
    ball.floor = None
    # the fixed surfaces: the deepest first, a few times for corners
    for _ in range(4):
        loc, nor, idx, d = machine.bvh.find_nearest(ball.p, R)
        if loc is None:
            break
        n = ball.p - loc
        n = n / n.length if n.length > 1e-9 else nor
        ball.p = loc + n * R
        vn = ball.v.dot(n)
        name = machine.owner[idx]
        if n.z > 0.6:
            ball.floor = n; ball.ground = name
        if vn < 0:
            e = E_FLOOR if n.z > 0.6 else restitution(name)
            if n.z > 0.6 and vn > -3.0:
                e = 0.0
            ball.v -= n * (1.0 + e) * vn
            if vn < -5.0:
                events.append((t, "hit", name, ball.p.copy(), i))
            # the slingshots have no kicker: their rubber rings only bounce the ball
            if name.startswith("sling-rubber") and vn < -5.0 and abs(n.z) < 0.5 and t >= ball.cool.get(name, 0):
                ball.cool[name] = t + 0.1
                events.append((t, "sling", name, ball.p.copy(), i))
    # the bumpers' skirts
    p2 = ball.p.to_2d()
    for b, (bx, by) in BUMPER.items():
        d2 = p2 - mm(bx, by).to_2d()
        if d2.length < BUMPER_REACH and ball.p.z < 1.2 and t >= ball.cool.get(b, 0):
            n = Vector((d2.x, d2.y, 0.0)).normalized()
            vt = ball.v - n * ball.v.dot(n)
            ball.v = vt * 0.4 + n * BUMPER_KICK; ball.cool[b] = t + BUMPER_READY
            events.append((t, "bumper", str(b), ball.p.copy(), i))
    # the flippers
    if ball.p.z < machine.flippers["left"].top + R:
        for side, fl in machine.flippers.items():
            if (p2 - fl.pivot).length > fl.length + R + 0.5:
                continue
            q, inside = fl.nearest(p2, fl.angle(t))
            d2 = p2 - q
            if inside or d2.length < R:
                n2 = d2.normalized() if d2.length > 1e-9 else (p2 - fl.pivot).normalized()
                if inside:
                    n2 = -n2
                ball.p.xy = q + n2 * R
                w = fl.rate(t); r = q - fl.pivot
                vs = Vector((-w * r.y, w * r.x, 0.0))
                n = Vector((n2.x, n2.y, 0.0))
                vrel = (ball.v - vs).dot(n)
                if vrel < 0:
                    ball.v -= n * (1.0 + E_FLIPPER) * vrel
                    if vrel < -5.0 or abs(w) > 1.0:
                        events.append((t, "flipper", side, ball.p.copy(), i))
