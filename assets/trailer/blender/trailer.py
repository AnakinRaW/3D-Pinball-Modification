"""Builds the trailer on the machine that both films share: one game of one player, its cameras and its lights,
without rendering.

  blender -b --factory-startup --python trailer.py        writes ../trailer.blend

scene.py in ../../cgi/blender builds the machine with its lights, and logo.py there the logo. game.py plays the game
again from the parameters solve.py found in game.json. A clock lays the game's time onto the frames: half speed,
slower around what the trailer shows, and never a jump. The shots follow the game, each from one of its moments to
another. The flippers, the bumpers' coils, the slings, the seal, the plunger, the scoop's kicker and the ball
store's seesaw move with it, and the lights keep the cues of cues.md. Twice the apron comes off, so the ball store and
the drain beneath it show. The game ends in the drain; a trail of light rises from it and flies into the screen,
and the picture goes black and opens on the logo.

Positions are millimeters of the drawing on the board, as in scene.py, unless a name says world; the logo's are
Blender units in the world, as in logo.py. 25 frames a second.
"""
import bpy, bisect, importlib, json, math, os, sys
from mathutils import Euler, Matrix, Quaternion, Vector

HERE = BUILD if "BUILD" in globals() else os.path.dirname(os.path.abspath(__file__))
CGI = os.path.join(os.path.dirname(os.path.dirname(HERE)), "cgi", "blender")
INTRO = os.path.join(os.path.dirname(os.path.dirname(HERE)), "intro")
OUT = os.path.join(os.path.dirname(HERE), "trailer.blend")
FPS = 25
SEESAW_TILT = math.degrees(math.atan(3.5 / 9.0))    # the separator's seesaw, as scene.py builds it
PIN_STROKE = 0.35                                    # BU, the pins' stroke of 3.5 mm
PLUNGER_PULL = 6.0                                   # mm the player draws the plunger back before it shoots
POWER = 100             # the power-up's last frame; the game follows
G0 = POWER + 1          # the frame where the game's time starts, ball A at rest at the plunger
BASE, EASE = 0.5, 0.3  # the game's speed against real time, and the game time a change of speed takes
BEAT = 12               # frames from beat to beat of the march, 125 beats a minute
# the follow cameras turn slowly enough to watch: about 90 degrees a second makes a pan uncomfortable, and a
# roll about the view sooner
AIM_PASS = 0.8                              # BU, 8 mm: how near the ball comes to a ball cam's aim before it looks ahead
LOOK_RATE = math.radians(3.0)               # the ball cam's largest turn a frame, 75 degrees a second
HEAD_RATE = math.radians(2.0)               # the chase camera's largest turn a frame, 50 degrees a second
HEAD_TIME = 1.5                             # s the chase camera averages the ball's travel over
LEAN = math.radians(3.0)                    # the chase camera's largest lean into a curve
CLEARANCE = 0.5                             # BU, 5 mm: the least distance from a camera to the machine
BALL = 0.5                                  # BU: the line of sight ends this short of a ball's centre
LIFT_STEP, LIFT_MAX = 0.1, 15.0             # BU: the search for a camera's lift, 1 mm steps up to 150 mm
SIGHT_MAX = 3.0                             # BU, 30 mm: the most a camera rises further to see its ball
SETTLE = 12                                 # frames either side over which a camera's lift is smoothed
TRAIL, LAG_MIN = 6.0, 2.0                   # BU, 60 mm: the farthest a chase camera's target trails its ball; the
                                            # spring's least lag in frames
SHORTEST = 45                               # frames: the shortest shot of the game
STORE_FRONT, STORE_BACK = (192.32, 397.74), (183.35, 400.99)
# under the apron, the drained ball rolls along the drain wall to the store; let out, the front ball rolls past the
# exit sensor through the opening in the plunger lane's wall down to the plunger
TO_STORE = [(109.0, 406.0), (117.0, 412.5), (131.0, 415.4), (145.0, 415.4), (153.0, 414.0), (161.0, 411.5),
            (169.0, 408.0), (177.0, 404.2)]
TO_LANE = [STORE_FRONT, (199.0, 395.4), (206.5, 392.9), (213.5, 393.6), (219.5, 398.0), (223.5, 405.0),
           (224.5, 410.0)]           # and on to the plunger's rod, runs.PLUNGER
ROLL = 1.3                                  # s a drained ball takes to the store
MULTI = ["#ffc21f", "#ff3048", "#e040ff"]   # the multiball's palette: gold, red and magenta

for folder in (CGI, HERE):
    if folder not in sys.path:
        sys.path.insert(0, folder)
import common, lights, logo
importlib.reload(common); importlib.reload(lights); importlib.reload(logo)
import physics as ph
importlib.reload(ph)
import runs as rn
importlib.reload(rn)
import game
importlib.reload(game)
from common import coll, fcurves_of

SCREEN = lights.display_centre()            # world: the middle of the diorama's display


def smooth(u):
    u = max(0.0, min(1.0, u))
    return u * u * (3 - 2 * u)


def lerp(a, b, u):
    return a + (b - a) * u


# ---------------------------------------------------------------- the clock and the game
class Clock:
    """Lays the game's time onto the frames, window by window. A window (t0, t1, base) of game time plays at base
    times real speed, slower around each moment of slow, (time, before, after, speed), each change taking EASE of
    game time. Between two windows that do not touch, the film cuts and the game's time jumps on, and the next
    window starts on a whole frame. frame(t) is None for a time no window shows; near(t) gives the frame of the
    window that shows t or of the next one; t(f) converts back."""

    def __init__(self, windows, slow):
        self.slow, self.wins = slow, []
        f, prev = float(G0), None
        for t0, t1, base in windows:
            if prev is None or abs(t0 - prev) > 1e-6:
                f = float(math.ceil(f - 1e-6))
            ts, fs, t, dt = [], [], t0, 0.002
            while t < t1:
                ts.append(t); fs.append(f)
                f += dt * FPS / self.speed(t, base); t += dt
            ts.append(t1); fs.append(f)
            self.wins.append((ts, fs, base))
            prev = t1
        self.end = f

    def speed(self, t, base=BASE):
        s = base
        for tc, before, after, sp in self.slow:
            a, b = tc - before, tc + after
            w = min(smooth((t - (a - EASE)) / EASE), smooth(((b + EASE) - t) / EASE))
            if w > 0:
                s = min(s, lerp(base, sp, w))
        return s

    @staticmethod
    def _at(xs, ys, x):
        k = max(0, min(len(xs) - 2, bisect.bisect_right(xs, x) - 1))
        u = (x - xs[k]) / max(xs[k + 1] - xs[k], 1e-12)
        return ys[k] + (ys[k + 1] - ys[k]) * u

    def frame(self, t):
        for ts, fs, _ in self.wins:
            if ts[0] - 1e-9 <= t <= ts[-1] + 1e-9:
                return self._at(ts, fs, t)
        return None

    def near(self, t):
        for ts, fs, _ in self.wins:
            if t <= ts[-1] + 1e-9:
                return self._at(ts, fs, max(t, ts[0]))
        ts, fs, base = self.wins[-1]
        return fs[-1] + (t - ts[-1]) * FPS / base

    def shown(self, t):
        return self.frame(t) is not None

    def t(self, f):
        prev = None
        for ts, fs, base in self.wins:
            if f < fs[0]:
                return ts[0] + (f - fs[0]) * base / FPS if prev is None else prev
            if f <= fs[-1]:
                return self._at(fs, ts, f)
            prev = ts[-1]
        ts, fs, base = self.wins[-1]
        return ts[-1] + (f - fs[-1]) * base / FPS


class Play:
    """The game as game.play() runs it from game.json: each ball's samples, the events, the presses, and the
    segments with their start, end, gates, the flipper holding the ball at each end and their parameters."""

    def __init__(self, machine, solution):
        r = game.play(machine, [(s["name"], s["params"], s["goal"]) for s in solution])
        self.samples, self.events, self.presses = r["samples"], r["events"], r["presses"]
        self.segments = [dict(name=s[0], t0=s[1], t1=s[2], marks=s[3], cost=s[4], start=s[5], end=s[6], q=s[7], cut=s[8])
                         for s in r["segments"]]
        self.drains = sorted((e[0], e[4]) for e in self.events if e[1] == "drained")
        # a cut puts ball A back in play, and ball B leaves the game when the multiball's segment ends
        self.restarts = [s["t0"] for s in self.segments if s["cut"]]
        mb = self.seg("multiball")
        self.release = mb["t0"] if mb else None        # ball B leaves the store, and rests at the plunger RELEASE later
        self.b_gone = mb["t1"] if mb else None
        self.out = {}
        for t, i in self.drains:
            self.out.setdefault(i, t)

    def drained_at(self, i, t):
        """When ball i last drained before t, unless a cut has put it back in play since; else None."""
        d = max((td for td, j in self.drains if j == i and td <= t), default=None)
        if d is None or (i == 0 and any(d < r <= t for r in self.restarts)):
            return None
        return d

    def seg(self, name):
        return next((s for s in self.segments if s["name"] == name), None)

    def live(self, t):
        """The ball in play at t while one ball plays: A, or after the multiball the one that stayed."""
        alive = [i for i in (0, 1) if self.samples[i] and (i not in self.out or self.out[i] > t)
                 and (i == 0 or (self.release is not None and t >= self.release + game.RELEASE))]
        return alive[0] if len(alive) == 1 else 0

    def state(self, i, t):
        s = self.samples[i]
        if not s:
            return None
        if t <= s[0][0]:
            return s[0][1].copy(), s[0][2].copy()
        if t >= s[-1][0]:
            return s[-1][1].copy(), s[-1][2].copy()
        lo, hi = 0, len(s) - 1
        while hi - lo > 1:
            mid = (lo + hi) // 2
            if s[mid][0] <= t:
                lo = mid
            else:
                hi = mid
        u = (t - s[lo][0]) / max(s[hi][0] - s[lo][0], 1e-9)
        return s[lo][1].lerp(s[hi][1], u), s[lo][2].lerp(s[hi][2], u)

    def catch(self, seg):
        """A moment before a shot segment starts, the ball rolling down toward the flipper that will hit it. Behind a
        cut the ball first appears where the segment starts, so the moment is the cut."""
        return seg["t0"] + 0.25 if seg["cut"] else seg["t0"] - 0.6

    def flip(self, seg):
        """The moment the player presses the flipper that starts a shot segment."""
        return seg["t0"] + seg["q"]["delay"]


def path_at(points, u):
    """The point a fraction u along a polyline (mm), by length."""
    lens = [math.dist(a, b) for a, b in zip(points, points[1:])]
    total = sum(lens)
    left = max(0.0, min(1.0, u)) * total
    for (a, b), d in zip(zip(points, points[1:]), lens):
        if left <= d:
            w = left / max(d, 1e-9)
            return (a[0] + (b[0] - a[0]) * w, a[1] + (b[1] - a[1]) * w)
        left -= d
    return points[-1]


def lane_z(x, y):
    """The ball's middle over the board where it rolls: on the board, or on the plunger lane's floor."""
    if x < 220.0:
        return 4.5
    t = max(0.0, min(1.0, (420.0 - y) / 320.0))
    return 4.5 + 24.0 * (1 - (1 - t) ** 1.8) * 0.4


# ---------------------------------------------------------------- the scene
def coll_link(o, name="trailer"):
    coll(name).objects.link(o)
    return o


def empty(name, parent, loc):
    o = coll_link(bpy.data.objects.new(name, None))
    o.parent = parent; o.location = loc
    return o


def adopt(o, parent):
    """Hangs o from parent where it stands now."""
    o.parent = parent
    o.matrix_parent_inverse = Matrix.Translation(-parent.location)


def key_curve(o, path, index, frames_values, interp="LINEAR"):
    o.animation_data or o.animation_data_create()
    for f, v in frames_values:
        if index is None:
            setattr(o, path, v); o.keyframe_insert(path, frame=f)
        else:
            getattr(o, path)[index] = v; o.keyframe_insert(path, index=index, frame=f)
    for fc in fcurves_of(o):
        if fc.data_path == path:
            for kp in fc.keyframe_points:
                kp.interpolation = interp


def middle(o):
    """The middle of a mesh's vertices in its parent's frame (Blender units)."""
    vs = o.data.vertices
    return Vector([sum(v.co[i] for v in vs) / len(vs) for i in range(3)])


def build_parts(tilt):
    """Hangs the moving parts from empties that turn or push them: the flippers, the bumpers, the seal, the
    slings' rubbers, the plunger, the scoop's kicker and the store's seesaw with its fingers."""
    parts = {"flipper": {}, "coil": {}}
    for side, ((x, y), turn) in ph.FLIPPER.items():
        pv = empty("tr-flipper-" + side, tilt, ph.mm(x, y))
        for o in bpy.data.objects:
            if o.type != "MESH" or not o.name.startswith("flipper") or o.parent != tilt:
                continue
            if (middle(o).x < 10.8) == (side == "left"):
                adopt(o, pv)
        parts["flipper"][side] = pv
    for k in ph.BUMPER:
        c = empty("tr-bumper-%d-coil" % k, tilt, Vector((0, 0, 0)))
        for o in list(bpy.data.objects):
            if o.name.startswith("bumper-%d-" % k) and not any(w in o.name for w in ("foil", "cutter", "well")):
                o.parent = c
        parts["coil"][k] = c
    parts["seal"] = empty("tr-seal", tilt, ph.mm(120.43, 169.65))
    for o in list(bpy.data.objects):
        if o.name.startswith("seal") and o.parent == tilt:
            adopt(o, parts["seal"])
    # the slings' rubbers: each is pushed toward the playfield's middle, square to its kicking face
    parts["sling"] = {}
    for name in ("sling-rubber", "sling-rubber.001"):
        o = bpy.data.objects[name]
        e = empty("tr-" + name, tilt, Vector((0, 0, 0)))
        o.parent = e
        parts["sling"][name] = (e, ph.sling_face(o))
    parts["plunger"] = empty("tr-plunger", tilt, Vector((0, 0, 0)))
    for o in list(bpy.data.objects):
        if o.name.startswith("plunger") and o.parent == tilt and o.name != "plunger-spring":
            o.parent = parts["plunger"]
    parts["spring"] = bpy.data.objects["plunger-spring"]        # stays at the mount and stretches
    parts["kicker"] = empty("tr-scoop-kicker", tilt, Vector((0, 0, 0)))
    for n in ("scoop-kicker", "scoop-kicker-rod"):
        bpy.data.objects[n].parent = parts["kicker"]
    # the seesaw rocks about its pin; the fingers ride up and down with its ends
    front, back = bpy.data.objects["store-finger-front"], bpy.data.objects["store-finger-back"]
    pf, pb = middle(front), middle(back)
    pin = Vector(((pf.x + pb.x) / 2, (pf.y + pb.y) / 2, -0.9))
    along = Vector((pf.x - pb.x, pf.y - pb.y, 0.0)).normalized()
    rock = empty("tr-seesaw", tilt, pin)
    for n in ("store-seesaw-beam", "store-pushrod"):
        adopt(bpy.data.objects[n], rock)
    parts["seesaw"] = (rock, Vector((-along.y, along.x, 0.0)))
    parts["fingers"] = {}
    for name, o in (("front", front), ("back", back)):
        e = empty("tr-finger-" + name, tilt, Vector((0, 0, 0)))
        o.parent = e
        parts["fingers"][name] = e
    # the spinner's flap turns about its rod, which runs across the left lane at SPINNER's height
    rod = math.radians(SPINNER_ROD)
    axis = Vector((math.cos(rod), -math.sin(rod), 0.0))
    sp = empty("tr-spinner", tilt, ph.mm(SPINNER[0], SPINNER[1], SPINNER_AXLE))
    sp.rotation_mode = "AXIS_ANGLE"
    for o in [o for o in bpy.data.objects if o.name.startswith(("deco-spinner-flap", "deco-spinner-star", "deco-spinner-top"))]:
        adopt(o, sp)
    parts["spinner"] = (sp, axis)
    horn = bpy.data.objects["store-servo-horn"]
    h = empty("tr-servo-horn", tilt, middle(bpy.data.objects["store-servo-shaft"]))
    adopt(horn, h)
    parts["horn"] = h
    return parts


def make_ball(name, tilt):
    src = bpy.data.objects["ball-other"]
    o = coll_link(bpy.data.objects.new(name, src.data))
    o.parent = tilt; o.rotation_mode = "QUATERNION"
    return o


# ---------------------------------------------------------------- the apron coming off
def build_apron(tilt):
    """Hangs the apron's parts from an empty at the apron's middle, which lifts it off."""
    parts = [o for o in bpy.data.objects if o.name.startswith("apron") and o.parent == tilt]
    plate = bpy.data.objects["apron"]
    e = empty("tr-apron", tilt, middle(plate))
    for o in parts:
        adopt(o, e)
    return e, parts


def key_apron(apron, parts, shots):
    """Over each shot that looks under the apron, the apron comes off: its clips let go with a jolt, it lifts and
    tips, and it flies up to the left and out of the picture, turning. At the cut after the shot it is back in
    place."""
    e = apron
    loc, rot, hide = [(1, (0.0, 0.0, 0.0))], [(1, (0.0, 0.0, 0.0))], [(1, False)]
    for name, f0, f1, poses, world, extra in shots:
        if "open" not in extra:
            continue
        a, b = extra["open"]
        loc += [(a, (0.0, 0.0, 0.0)), (a + 2, (0.0, 0.0, 0.15)), (a + 4, (0.0, 0.0, 0.1)), (a + 14, (-0.6, 0.2, 2.5)),
                (a + 30, (-17.0, 2.0, 11.0)), (b + 1, (0.0, 0.0, 0.0))]
        rot += [(a, (0.0, 0.0, 0.0)), (a + 2, (math.radians(1.5), 0.0, 0.0)), (a + 4, (0.0, 0.0, 0.0)),
                (a + 14, (0.0, math.radians(-12.0), math.radians(3.0))), (a + 30, (math.radians(-10.0), math.radians(-70.0), math.radians(20.0))),
                (b + 1, (0.0, 0.0, 0.0))]
        hide += [(a + 31, True), (b + 1, False)]
    rest = e.location.copy()
    for f, v in sorted(loc):
        e.location = rest + Vector(v); e.keyframe_insert("location", frame=f)
    for f, v in sorted(rot):
        e.rotation_euler = v; e.keyframe_insert("rotation_euler", frame=f)
    for o in parts:
        for f, v in sorted(hide):
            o.hide_render = v; o.keyframe_insert("hide_render", frame=f)
            o.hide_viewport = v; o.keyframe_insert("hide_viewport", frame=f)
    for fc in fcurves_of(e):
        for kp in fc.keyframe_points:
            kp.interpolation = "BEZIER"
    # it jumps back at the cut, so the frame before the cut holds it off
    for fc in fcurves_of(e):
        for kp in fc.keyframe_points:
            if any(abs(kp.co.x - (x["open"][1] + 1)) < 0.5 for *_, x in shots if "open" in x):
                kp.interpolation = "CONSTANT"
        pts = list(fc.keyframe_points)
        for k in range(1, len(pts)):
            if any(abs(pts[k].co.x - (x["open"][1] + 1)) < 0.5 for *_, x in shots if "open" in x):
                pts[k - 1].interpolation = "CONSTANT"


# ---------------------------------------------------------------- cameras
def look(cam, target, up, roll=0.0):
    f = (target - cam).normalized()
    r = f.cross(up)
    if r.length < 1e-6:
        r = f.cross(Vector((0.0, 1.0, 0.0)))
    r.normalize(); u = r.cross(f)
    if roll:
        c, s = math.cos(roll), math.sin(roll)
        r, u = r * c + u * s, u * c - r * s
    return Matrix((r, u, -f)).transposed().to_quaternion()


class Shots:
    """The camera moves; each returns {frame: (location, rotation, lens)} in the frame of machine-tilt, or in the
    world where a shot says so."""

    def __init__(self, tilt, play, clock, view, balls_at):
        self.tilt, self.play, self.clock, self.view, self.balls_at = tilt, play, clock, view, balls_at
        self.Mt = tilt.matrix_world.copy()
        self.Q = self.Mt.to_quaternion()
        self.up = Vector((0.0, 0.0, 1.0))

    def board(self, x, y, z):
        return ph.mm(x, y, z)

    def ball(self, f, i=0):
        """Where ball i is at frame f, as the trailer shows it, and its velocity in units a second of game time."""
        return self.balls_at(f, i)

    def ballcam(self, f0, f1, i=0, ahead=4.0, pull=None, rate=LOOK_RATE, blend=0.12, aim=None):
        """Looks where the ball travels, smoothed by blend a frame and turning at most rate a frame, so a bounce
        or a loop turns the view and does not whip it. With aim, (x, y, z) mm, it looks at that point instead while
        the ball comes up the board toward it, until the ball is AIM_PASS from it or past it. A slight handheld shake
        rides on top."""
        out, d = {}, None
        target = self.board(*aim) if aim else None
        for f in range(f0 - 12, f1 + 1):
            p, v = self.ball(f, i)
            v2 = Vector((v.x, v.y, v.z * 0.3))
            # the view holds on the aim until the ball has passed it, up the board, whatever the ball bounces off
            aiming = target is not None and (target - p).length > AIM_PASS and -p.y * 10.0 > aim[1] - 2.0
            if aiming:
                v2 = (target - p) * 10.0
            if v2.length > 2.0:
                if d is None:
                    d = v2.normalized()
                r = rate * 1.8 if aiming else rate
                want = d.lerp(v2.normalized(), max(blend, 0.5) if aiming else blend).normalized()
                a = d.angle(want, 0.0)
                d = (d.slerp(want, r / a) if a > r else want).normalized()
            if d is None:
                d = Vector((0.0, 1.0, 0.0))
            if f < f0:
                continue
            cam = p.copy()
            if pull:
                a, b = pull
                cam.y -= 0.2 * smooth((f - a) / max(1, b - a)) if f <= b + 1 else 0.0   # 2 mm, clear of the cabinet
            out[f] = (cam, shake(look(cam, cam + d * ahead, self.up), f), 16.0)
        return out

    def chase(self, f0, f1, i=0, D=6.0, H=3.5, lag=6.0, lens=24.0, both=False, rise=None):
        """The helicopter: a target that trails the ball by a critically damped spring of lag frames, the camera D
        behind it along its heading and H above, climbing with speed, leaning a little in curves, dipping at hits,
        and kept clear of the machine. The heading follows the ball's travel averaged over HEAD_TIME, so a bounce
        between bumpers barely moves it, and turns at most HEAD_RATE a frame; with both balls, seen from high above,
        it holds the playfield's length."""
        out, frames = {}, []
        tgt = vel = last = None
        head = None
        travel = None
        lean = 0.0
        height = None
        pa = pb = None
        hits = self.hit_frames(f0 - 30, f1)
        for f in range(f0 - 30, f1 + 1):
            if both:
                (pa, va), (pb, vb) = self.ball(f, 0), self.ball(f, 1)
                p, v = (pa + pb) / 2, (va + vb) / 2
                sep = (pa - pb).length
            else:
                p, v = self.ball(f, i)
                sep = 0.0
            if tgt is None:
                tgt, vel = p.copy(), Vector((0, 0, 0))
            step = (p - last).length if last is not None else 0.0
            last = p.copy()
            a = 2.0 / max(LAG_MIN, min(lag, TRAIL / max(step, 1e-6)))
            acc = (p - tgt) * a * a - vel * 2 * a
            vel += acc; tgt += vel
            v2 = v.to_2d()
            travel = v2.copy() if travel is None else travel.lerp(v2, 1.0 / (HEAD_TIME * FPS))
            if both:
                want = Vector((0.0, 1.0))
            elif travel.length > 1.0:
                want = travel.normalized()
            else:
                want = head if head is not None else Vector((0.0, 1.0))
            if head is None:
                head = want.copy()
            ang = math.atan2(head.y, head.x)
            turn = (math.atan2(want.y, want.x) - ang + math.pi) % (2 * math.pi) - math.pi
            turn = max(-HEAD_RATE, min(HEAD_RATE, turn))
            head = Vector((math.cos(ang + turn), math.sin(ang + turn)))
            lean += (max(-LEAN, min(LEAN, -turn * 3.0)) - lean) * 0.1
            climb = max(H, min(25.0, H + sep * 1.2)) if both else H + v.length * 10.0 / 500.0
            height = climb if height is None else height + (climb - height) * 0.08
            if f < f0:
                continue
            h = height
            if rise:
                g0, g1, hh = rise
                h = lerp(h, hh, smooth((f - g0) / max(1, g1 - g0)))
            for fh in hits:
                if fh <= f < fh + 18:
                    k = f - fh
                    h -= 0.5 * smooth(k / 6.0 if k < 6 else 1.0 - (k - 6) / 12.0)
            h3 = Vector((head.x, head.y, 0.0))
            balls = [pa, pb] if both else [p]
            frames.append((f, tgt - h3 * D + Vector((0.0, 0.0, h)), tgt + h3 * 1.5, [b.copy() for b in balls], lean))
        for (f, cam, aim, balls, roll), lift in zip(frames, self.lifts([(c, b) for _, c, _, b, _ in frames])):
            cam = cam + Vector((0.0, 0.0, lift))
            out[f] = (cam, look(cam, aim, self.up, roll), lens)
        return out

    def lifts(self, poses):
        """How far each camera of a shot rises over its pose, smoothly: a frame's least lift keeps CLEARANCE to
        every part of the machine and a free line of sight to every ball; the largest least lift within SETTLE
        frames either side is averaged over the same span, so the camera rises in time before an obstacle."""
        need = [self.need_lift(cam, balls, 0.0) for cam, balls in poses]
        out = need
        for _ in range(3):
            n = len(need)
            top = [max(need[max(0, i - SETTLE):i + SETTLE + 1]) for i in range(n)]
            out = [sum(top[max(0, i - SETTLE):i + SETTLE + 1]) / len(top[max(0, i - SETTLE):i + SETTLE + 1]) for i in range(n)]
            again = [self.need_lift(cam, balls, l) for (cam, balls), l in zip(poses, out)]
            if all(a <= l + 1e-6 for a, l in zip(again, out)):
                return out
            need = [max(a, b) for a, b in zip(need, again)]
        return out

    def need_lift(self, cam, balls, start):
        """The least lift from start at which the camera keeps CLEARANCE to the machine, and from there the least
        further lift, up to SIGHT_MAX, at which it sees every ball."""
        bvh = self.view.bvh
        k = start
        while k < LIFT_MAX and bvh.find_nearest(cam + Vector((0.0, 0.0, k)), CLEARANCE)[0] is not None:
            k += LIFT_STEP
        clear = k
        while k <= clear + SIGHT_MAX:
            c = cam + Vector((0.0, 0.0, k))
            if bvh.find_nearest(c, CLEARANCE)[0] is None and all(
                    bvh.ray_cast(c, (b - c).normalized(), max((b - c).length - BALL, 0.0))[0] is None for b in balls):
                return k
            k += LIFT_STEP
        return clear

    def hit_frames(self, f0, f1):
        frames = []
        for e in self.play.events:
            if (e[1] in ("bumper", "sling", "flipper") or (e[1] == "hit" and str(e[2]).startswith(("bell", "target", "seal-r"))))                     and self.clock.shown(e[0]):
                fe = round(self.clock.frame(e[0]))
                if f0 <= fe <= f1 and (not frames or fe - frames[-1] > 6):
                    frames.append(fe)
        return frames

    def dolly(self, f0, f1, c0, c1, t0, t1, lens0, lens1=None, up=None, ease=True):
        out = {}
        up = up or self.up
        for f in range(f0, f1 + 1):
            u = (f - f0) / max(1, f1 - f0)
            u = smooth(u) if ease else u
            cam, tgt = c0.lerp(c1, u), t0.lerp(t1, u)
            out[f] = (cam, look(cam, tgt, up), lerp(lens0, lens1 if lens1 else lens0, u))
        return out

    def orbit(self, f0, f1, centre, r, h, a0, a1, lens):
        out = {}
        for f in range(f0, f1 + 1):
            a = math.radians(lerp(a0, a1, smooth((f - f0) / max(1, f1 - f0)) * 0.6 + 0.4 * (f - f0) / max(1, f1 - f0)))
            x, y = centre[0] + r * math.cos(a), centre[1] - r * math.sin(a)
            cam = self.board(x, y, h)
            out[f] = (cam, look(cam, self.board(*centre), self.up), lens)
        return out

    def track(self, f0, f1, cam_path, aim_path, lens):
        """A camera along a path of (frame, (x, y, z)) in mm, looking along a path of the same kind; both are
        Catmull-Rom splines through their points, and a lift keeps the camera clear of the machine."""
        poses = []
        for f in range(f0, f1 + 1):
            cam = self.board(*catmull(cam_path, f)); aim = self.board(*catmull(aim_path, f))
            poses.append((f, cam, aim))
        out = {}
        for (f, cam, aim), lift in zip(poses, self.lifts([(c, []) for _, c, _ in poses])):
            cam = cam + Vector((0.0, 0.0, lift))
            out[f] = (cam, look(cam, aim, self.up), lens(f) if callable(lens) else lens)
        return out

    def follow(self, f0, f1, pos, focus, i=0, lens=35.0, keep=0.35, lag=4.0):
        """A camera standing at pos (mm) that pans after ball i: it looks at the ball drawn keep of the way toward
        focus (mm), so the part the shot is about stays in the picture, through a critically damped spring of lag
        frames that turns at most LOOK_RATE a frame."""
        cam = self.board(*pos)
        cam = cam + Vector((0.0, 0.0, self.lifts([(cam, [])])[0]))
        fc = self.board(*focus)
        out, aim, vel, d = {}, None, Vector(), None
        for f in range(f0 - 10, f1 + 1):
            p, v = self.ball(f, i)
            want = p.lerp(fc, keep)
            if aim is None:
                aim = want.copy()
            a = 2.0 / lag
            acc = (want - aim) * a * a - vel * 2 * a
            vel += acc; aim += vel
            nd = (aim - cam).normalized()
            if d is None:
                d = nd
            ang = d.angle(nd, 0.0)
            d = (d.slerp(nd, LOOK_RATE / ang) if ang > LOOK_RATE else nd).normalized()
            if f >= f0:
                out[f] = (cam, look(cam, cam + d, self.up), lens)
        return out

    def vantage(self, f0, f1, focus, i=0):
        """Where a camera stands that sees both the part at focus (mm) and ball i over the frames: of the points
        round the part at three distances and four heights, the one with a free line of sight to the part and to
        the most of the ball's places, the nearer and the more from the front the better."""
        bvh = self.view.bvh
        fc = self.board(*focus)
        balls = [self.ball(f, i)[0] for f in range(f0, f1 + 1, 3)]

        def free(cam, p):
            d = p - cam
            return bvh.ray_cast(cam, d.normalized(), max(d.length - BALL, 0.0))[0] is None

        best = None
        for r in (70.0, 100.0, 140.0):
            for h in (30.0, 60.0, 100.0, 150.0):
                for k in range(18):
                    a = math.radians(20 * k)
                    pos = (focus[0] + r * math.cos(a), focus[1] + r * math.sin(a), h)
                    cam = self.board(*pos)
                    if bvh.find_nearest(cam, CLEARANCE)[0] is not None or not free(cam, fc):
                        continue
                    seen = sum(1 for p in balls if free(cam, p)) / max(1, len(balls))
                    score = seen + 0.1 * math.sin(a) - 0.0005 * r
                    if best is None or score > best[0]:
                        best = (score, pos)
        return best[1] if best else (focus[0], focus[1] + 120.0, 120.0)

    def flipcam(self, f0, f1, side, i=0):
        """The player's view: above the apron's front, behind the flipper side, looking up the board over it; it
        pans a little after the ball, so a ball coming down from anywhere rolls into the picture."""
        (px, py), turn = ph.FLIPPER[side]
        toward = 1.0 if side == "left" else -1.0
        return self.follow(f0, f1, (px + 14.0 * toward, 452.0, 80.0), (px + 16.0 * toward, 330.0, 0.0), i=i, lens=30.0, keep=0.65)

    def screen(self, f0, f1, d0, d1, lens, yaw0=0.0, yaw1=None, look_at=None):
        """World: square on to the diorama's display from d0 to d1 mm, turned yaw degrees about it."""
        out = {}
        for f in range(f0, f1 + 1):
            u = smooth((f - f0) / max(1, f1 - f0))
            yaw = math.radians(lerp(yaw0, yaw1 if yaw1 is not None else yaw0, u))
            d = lerp(d0, d1, u) / 10.0
            cam = SCREEN + Vector((math.sin(yaw) * d, -math.cos(yaw) * d, 0.0))
            tgt = SCREEN if look_at is None else SCREEN.lerp(look_at, u)
            out[f] = (cam, look(cam, tgt, Vector((0, 0, 1))), lens)
        return out


SHAKE = math.radians(0.6)   # the ball cam's handheld shake at its largest, about each axis


def shake(q, f):
    """A view turned by a slight, smooth shake: three slow sines a little out of step about each axis."""
    w = lambda a, b, c: SHAKE * (0.5 * math.sin(f * a) + 0.3 * math.sin(f * b + 1.3) + 0.2 * math.sin(f * c + 2.1))
    return q @ Euler((w(0.21, 0.53, 1.17), w(0.17, 0.61, 1.31), 0.5 * w(0.13, 0.47, 0.97))).to_quaternion()


def catmull(points, f):
    fs = [p[0] for p in points]
    if f <= fs[0]:
        return Vector(points[0][1])
    if f >= fs[-1]:
        return Vector(points[-1][1])
    k = max(i for i in range(len(fs) - 1) if fs[i] <= f)
    p0 = Vector(points[max(k - 1, 0)][1]); p1 = Vector(points[k][1])
    p2 = Vector(points[k + 1][1]); p3 = Vector(points[min(k + 2, len(points) - 1)][1])
    u = (f - fs[k]) / (fs[k + 1] - fs[k])
    # tangents scaled by the spans, so the camera's speed runs on smoothly through the points
    d1 = fs[k + 1] - fs[k]
    m1 = (p2 - p0) / max(fs[min(k + 1, len(fs) - 1)] - fs[max(k - 1, 0)], 1) * d1
    m2 = (p3 - p1) / max(fs[min(k + 2, len(fs) - 1)] - fs[k], 1) * d1
    if k + 1 == len(fs) - 1:
        m2 = Vector((0, 0, 0))
    if k == 0:
        m1 = Vector((0, 0, 0))
    u2, u3 = u * u, u * u * u
    return (2 * u3 - 3 * u2 + 1) * p1 + (u3 - 2 * u2 + u) * m1 + (-2 * u3 + 3 * u2) * p2 + (u3 - u2) * m2


# what each feature's shot calls out: the sign's name and the part the arrow points at, (x, y, z) mm
# only what the modification adds to the machine, as docs/features.md lists it, gets a callout
CALLOUT = {"lanes": ("TOP LANES", (159.7, 44.6, 3.0)), "tunnel": ("U TRACK SEES THE BALL", (70.0, 6.0, 5.0)), "seal": ("SEAL SPINS SCORE", (120.43, 169.65, 12.0)),
           "scoop": ("SCOOP", (rn.SCOOP[0], rn.SCOOP[1], 3.0)), "store": ("MULTIBALL", (STORE_FRONT[0], STORE_FRONT[1], 5.0)),
           "inlane-left": ("INLANES SEE THE BALL", (28.05, 290.95, 1.0)), "inlane-right": ("INLANES SEE THE BALL", (189.05, 290.95, 1.0))}
MULTIBALL_SPEED = 0.5  # the game's speed against real time while two balls play
RIDE = 0.85            # the game's speed against real time on the rides along the ramp, the tracks and the crocodile
SPINNER = (24.7, 166.4, 9.0)   # mm: the middle of the spinner's flap across the left lane, as scene.py stands it
SPINNER_ROD, SPINNER_AXLE = 165.0, 15.0     # the rod's direction in degrees and its height in mm, as scene.py has them
TUNNEL_SPEED = 0.2             # the game's speed against real time on the ride to the spinner, which is fast
SPIN_HOLD = 0.2                # s of game time the film watches the spinner turn after the ball has passed it
SPINNER_TURNS = 3.0            # turns the flap makes after a ball rolls through, running down
LEAD = 0.9             # s of game time a window keeps before a feature's first gate, so the ball rolls into the picture
LOGO_SKIP = 8                  # frames at the logo's start that stay black, the intro's frames 126 to 130
SCOOP_SNAP = 0.12             # s of game time the ball takes to slide into the scoop's seat
SCOOP_HOLD = 0.4       # s the film stays on the scoop after the ball enters it
FLIP_AFTER = 0.35      # s of game time the player's view stays on after the flip


def spinner_pass(play, seg):
    """When the ball of a segment passes up through the spinner."""
    return next((t for t, p, v in play.samples[0] if t > play.flip(seg) and -p.y * 10.0 < SPINNER[1] - 4.0), seg["t1"])


def windows(play):
    """The windows of game time the film shows: the launch whole; for a feature, the ball coming down onto the
    flipper and the flip, then, after a cut where the ride is long, the ball rolling into the feature; the
    multiball whole; and the last drain with the ball's roll into the store."""
    out = []
    for seg in play.segments:
        t0, t1, name = seg["t0"], seg["t1"], seg["name"]
        if name == "finale":
            out.append((t0, t1 + ROLL, BASE))
            continue
        if not seg["cut"]:
            out.append((t0, t1, MULTIBALL_SPEED if name == "multiball" else BASE))
            continue
        flip = play.flip(seg)
        if name == "tunnel":
            # the ride ends as the ball passes through the spinner, before the tunnel goes dark; the flap's turning
            # follows from outside
            out.append((t0, min(t1, spinner_pass(play, seg) + SPIN_HOLD), TUNNEL_SPEED))
            continue
        first = min(seg["marks"].values(), default=t1)
        base = RIDE if name in ("ramp", "track", "croc") else BASE
        if name in ("target", "scoop"):
            # no flip: the film cuts straight to the ball rolling into the part, and away once it is in
            out.append((first - LEAD, min(t1, first + SCOOP_HOLD) if name == "scoop" else t1, base))
            continue
        if first - LEAD > flip + FLIP_AFTER + 0.3:
            out.append((t0, flip + FLIP_AFTER, BASE))
            out.append((first - LEAD, t1, base))
        else:
            out.append((t0, flip + FLIP_AFTER, BASE))
            out.append((flip + FLIP_AFTER, t1, base))
    return out


def plan_shots(S, play, clock):
    """The shots of the game, in order: [(name, f0, f1, poses, world, extra)]. Each starts where the one before it
    ends; a window's start is a cut."""
    F = lambda t: int(round(clock.near(t)))
    b = S.board
    plan = []

    def add(name, f0, f1, make, extra=None):
        if f1 >= f0:
            plan.append([name, f0, f1, make, extra or {}])

    inside = {"clip": 0.02}
    for seg in play.segments:
        name, marks = seg["name"], seg["marks"]
        f_seg0, f_seg1 = F(seg["t0"]), F(seg["t1"])
        if name == "launch":
            kick = seg["t0"] + game.PULL
            f_kick = F(kick)
            lane = marks.get("top-lane", kick + 1.0)

            def ride(a, z, f_kick=f_kick):
                return S.ballcam(a, z, pull=(a + 6, f_kick - 1))

            # the ride till just before the top lane, the lane seen from the front with its callout, the bumpers
            f_l0, f_lane = F(lane - 0.45), F(lane + 0.5)
            f_out = min(f_kick + 12, f_l0 - 20)
            add("launch", f_seg0, f_out, ride, dict(inside, inside_ball=(0, f_seg0, f_out)))
            add("orbit", f_out + 1, f_l0 - 1, lambda a, z: S.follow(a, z, (150, 260, 190), (215, 110, 20), lens=26.0, keep=0.5),
                {"ball": 0})
            add("lanes", f_l0, f_lane, lambda a, z: S.follow(a, z, (190, 155, 93), (160, 45, 3), lens=30.0, keep=0.6),
                {"ball": 0, "callout": CALLOUT["lanes"]})
            add("bumpers", f_lane + 1, f_seg1, lambda a, z: S.follow(a, z, (110, 210, 170), (82, 75, 10), lens=28.0, keep=0.5),
                {"ball": 0})
            continue
        if name == "multiball":
            plan_multiball(S, play, clock, seg, add, F)
            continue
        if name == "finale":
            add("finale-drain", f_seg0, F(seg["t1"] + ROLL * 0.7),
                lambda a, z: S.dolly(a, z, b(108.5, 455, 95), b(108.5, 450, 80), b(108.5, 350, 0), b(108.5, 375, 0), 20),
                {"ball": 0, "cut": True})
            continue
        if name == "target":
            t_hit = marks.get("target", seg["t1"])
            p, v = S.ball(F(t_hit), 0)
            tx = 29.5 if p.x * 10.0 < 108 else 187.6
            add("target", f_seg0, f_seg1, lambda a, z, tx=tx: S.follow(a, z, S.vantage(a, z, (tx, 232.4, 7)), (tx, 232.4, 7),
                                                                       lens=28.0, keep=0.8),
                {"ball": 0, "callout": ("STAND-UP TARGET", (tx, 232.4, 9.0))})
            continue
        if name == "scoop":
            f_in = F(marks.get("scoop", seg["t1"]) + SCOOP_HOLD)
            add("scoop", f_seg0, f_in, lambda a, z: S.follow(a, z, S.vantage(a, z, (31.9, 123.7, 2)), (31.9, 123.7, 2), lens=28.0, keep=0.6),
                {"ball": 0, "callout": CALLOUT["scoop"]})
            continue
        # a feature: the player's view as the ball comes down and the flip, then the feature
        f_flip_end = F(play.flip(seg) + FLIP_AFTER)
        extra = {"ball": 0, "cut": True}
        if name == "seal":
            extra["callout"] = CALLOUT["inlane-" + seg["start"]]
        add(name + "-flip", f_seg0, f_flip_end, lambda a, z, side=seg["start"]: S.flipcam(a, z, side, i=0), extra)
        f = f_flip_end + 1
        if name == "tunnel":
            # the ride as the ball, up the lane through the spinner and into the tunnel past its sensor
            # it turns quicker than the launch's ball cam, so the view swings round toward the spinner with the ball
            f_in = F(spinner_pass(play, seg))
            add("tunnel", f, f_in, lambda a, z: S.ballcam(a, z, rate=math.radians(7.0), blend=0.35, aim=SPINNER),
                dict(inside, inside_ball=(0, f, f_in), callout=CALLOUT["tunnel"]))
            # from outside, down the lane: the flap turning as the ball disappears behind it into the tunnel
            sx, sy, sz = SPINNER
            front = (sx + 45.0 * math.cos(math.radians(75.0)), sy + 45.0 * math.sin(math.radians(75.0)), 25.0)
            add("spinner", f_in + 1, f_seg1, lambda a, z, front=front: S.dolly(a, z, b(*front), b(front[0] - 2.0, front[1] - 6.0, front[2] - 2.0),
                                                                           b(sx, sy, sz), b(sx, sy, sz), 35.0), {"ball": 0})
        elif name == "ramp":
            add("ramp", f, f_seg1, lambda a, z: S.follow(a, z, (150, 290, 210), (70, 120, 20), lens=30.0, keep=0.3),
                {"ball": 0, "callout": CALLOUT["ramp"]})
        elif name == "seal":
            add("seal", f, f_seg1, lambda a, z: S.follow(a, z, (190, 270, 75), (120.4, 169.7, 8), lens=28.0, keep=0.55),
                {"ball": 0, "callout": CALLOUT["seal"]})
        elif name == "croc":
            f_in = F(marks.get("croc-mouth", seg["t1"]) - 0.2)
            add("croc", f, f_in - 1, lambda a, z: S.follow(a, z, (130, 270, 130), (178, 150, 10), lens=28.0, keep=0.4),
                {"ball": 0, "callout": CALLOUT["croc"]})
            add("croc-ride", f_in, f_seg1, lambda a, z: S.follow(a, z, (225, 205, 95), (192, 150, 15), lens=30.0, keep=0.5), {"ball": 0})
        elif name == "track":
            add("track", f, f_seg1, lambda a, z: S.follow(a, z, (115, 330, 290), (90, 100, 15), lens=26.0, keep=0.35),
                {"ball": 0, "callout": CALLOUT["track"]})
        elif name == "scoop":
            add("scoop", f, f_seg1, lambda a, z: S.follow(a, z, (95, 160, 28), (34, 128, 2), lens=28.0, keep=0.6),
                {"ball": 0, "callout": CALLOUT["scoop"]})
    for a, z in zip(plan, plan[1:]):
        if z[1] <= a[2]:
            a[2] = z[1] - 1
    plan = [s for s in plan if s[2] >= s[1]]
    return [(name, f0, f1, make(f0, f1), False, extra) for name, f0, f1, make, extra in plan]


def plan_multiball(S, play, clock, seg, add, F):
    """The multiball: through the apron the store lets ball B out, the plunger shoots it, the scoop throws ball A
    against bumper 3, both balls play and knock together."""
    b = S.board
    t0 = seg["t0"]
    f0 = F(t0)
    f_go = F(t0 + game.RELEASE)
    add("store", f0, f_go + 6, lambda a, z: S.dolly(a, z, b(160, 470, 150), b(168, 455, 180), b(170, 396, 0), b(184, 400, 0), 28),
        {"open": (f0, f_go + 6), "ball": 1, "callout": CALLOUT["store"]})
    kick = next((e[0] for e in play.events if e[1] == "kick" and e[4] == 0 and e[0] > t0), t0 + game.RELEASE + 1.0)
    f_k = F(kick - 0.35)
    add("b-launch", f_go + 7, f_k - 1, lambda a, z: S.follow(a, z, (175, 330, 150), (222, 180, 10), i=1, lens=30.0, keep=0.35),
        {"ball": 1})
    bump = next((e[0] for e in play.events if e[1] == "bumper" and e[4] == 0 and e[0] > kick), kick + 0.4)
    f_k1 = F(bump + 0.4)
    add("kick", f_k, f_k1, lambda a, z: S.follow(a, z, S.vantage(a, z, (40, 115, 4)), (40, 115, 4), i=0, lens=26.0, keep=0.4), {"ball": 0})
    add("both", f_k1 + 1, F(seg["t1"]),
        lambda a, z: S.dolly(a, z, b(110, 330, 430), b(110, 320, 440), b(110, 190, 0), b(110, 180, 0), 22), {"ball": 0})


# ---------------------------------------------------------------- the power-up
# the power-up's flight, keyed over 175 frames and laid onto POWER
POWER_CAM = [(max(1, round(f * POWER / 175)), p) for f, p in [(1, (108.0, 452.0, 14.0)), (35, (108.0, 372.0, 11.0)),
             (75, (118.0, 232.0, 26.0)), (110, (92.0, 128.0, 48.0)), (145, (115.0, 60.0, 110.0)), (175, (115.0, 25.0, 165.0))]]
POWER_AIM = [(max(1, round(f * POWER / 175)), p) for f, p in [(1, (108.0, 300.0, 5.0)), (35, (95.0, 110.0, 8.0)),
             (75, (85.0, 80.0, 10.0)), (110, (115.0, 10.0, 40.0)), (145, (115.0, 300.0, 0.0)), (175, (115.0, 270.0, 0.0))]]


# ---------------------------------------------------------------- keys
def key_cameras(S, shots, last):
    sc = bpy.context.scene
    sc.timeline_markers.clear()
    c = coll("trailer-cameras")
    first = None
    cams = {}
    for name, f0, f1, poses, world, extra in shots:
        cd = bpy.data.cameras.new("tr-cam-" + name)
        cd.clip_start = extra.get("clip", 0.05); cd.clip_end = 600.0; cd.sensor_width = 36.0
        cam = bpy.data.objects.new("tr-cam-" + name, cd); c.objects.link(cam)
        cam.rotation_mode = "QUATERNION"
        prev = None
        for f in sorted(poses):
            loc, q, lens = poses[f]
            if not world:
                loc, q = S.Mt @ loc, S.Q @ q
            if prev is not None and q.dot(prev) < 0:
                q = -q
            prev = q
            cam.location = loc; cam.keyframe_insert("location", frame=f)
            cam.rotation_quaternion = q; cam.keyframe_insert("rotation_quaternion", frame=f)
            cd.lens = lens; cd.keyframe_insert("lens", frame=f)
        for blk in (cam, cd):
            for fc in fcurves_of(blk):
                for kp in fc.keyframe_points:
                    kp.interpolation = "LINEAR"
        m = sc.timeline_markers.new(name, frame=f0); m.camera = cam
        first = first or cam
        cams[name] = cam
    sc.camera = first
    return cams


def balls_at_factory(play, clock):
    """Where each ball is at a frame, as the trailer shows it: ball A at the plunger before the game, ball B in the
    store till the multiball lets it out, a drained ball rolling under the apron into the store."""
    slots = {}
    order = sorted(play.out, key=lambda i: play.out[i])
    for k, i in enumerate(order):
        slots[i] = STORE_FRONT if k == 0 else STORE_BACK

    # a ball resting on the plunger follows its rod back as the player draws it
    plunger = [(e[0], e[4]) for e in play.events
               if e[1] == "kick" and math.hypot(e[3].x * 10.0 - rn.PLUNGER[0], -e[3].y * 10.0 - rn.PLUNGER[1]) < 4]

    def drawn(t, i):
        for tk, j in plunger:
            if j == i and tk - game.PULL <= t < tk:
                return Vector((0.0, -PLUNGER_PULL / 10.0 * (t - (tk - game.PULL)) / game.PULL, 0.0))
        return Vector()

    # the scoop holds the ball as a magnet would: from its entry it slides into the seat within SCOOP_SNAP and stays
    # there until the kicker throws it
    sc = play.seg("scoop")
    hold = None
    if sc and "scoop" in sc["marks"]:
        t_in = sc["marks"]["scoop"]
        t_kick = next((e[0] for e in play.events if e[1] == "kick" and e[4] == 0 and e[0] > sc["t1"]), None)
        seat, _ = play.state(0, sc["t1"])
        entry, _ = play.state(0, t_in)
        hold = (t_in, t_kick, entry, seat)

    def at(f, i):
        t = clock.t(f)
        if i == 0 and hold and hold[0] <= t and (hold[1] is None or t < hold[1]):
            u = smooth((t - hold[0]) / SCOOP_SNAP)
            return hold[2].lerp(hold[3], u), Vector()
        p, v = place(f, i)
        return p + drawn(t, i), v

    def place(f, i):
        t = clock.t(f)
        if i == 1 and (play.release is None or t < play.release):
            return ph.mm(*STORE_FRONT, 4.5), Vector()
        if i == 1 and t < play.release + game.RELEASE:
            u = smooth((t - play.release - 0.75) / (game.RELEASE - 0.8))
            x, y = path_at(TO_LANE + [rn.PLUNGER[:2]], u)
            return ph.mm(x, y, lane_z(x, y)), Vector()
        d = play.drained_at(i, t)
        if d is not None:
            p_out, _ = play.state(i, d)
            u = (t - d) / ROLL
            pts = [(p_out.x * 10.0, -p_out.y * 10.0)] + TO_STORE + [slots[i]]
            x, y = path_at(pts, 1 - (1 - min(1.0, u)) ** 2)
            return ph.mm(x, y, 4.5), Vector()
        if t < 0:
            return ph.mm(*rn.PLUNGER), Vector()
        st = play.state(i, t)
        return st if st else (ph.mm(*rn.PLUNGER), Vector())

    return at


def key_balls(S, balls, last, inside):
    for i, ball in enumerate(balls):
        rot = Quaternion()
        prev = None
        for f in range(1, last + 1):
            p, _ = S.ball(f, i)
            if prev is not None:
                d = p - prev
                if 1e-6 < d.length < 2.0:
                    axis = Vector((0, 0, 1)).cross(d)
                    if axis.length > 1e-9:
                        rot = Quaternion(axis.normalized(), d.length / ph.R) @ rot
            prev = p
            ball.location = p; ball.keyframe_insert("location", frame=f)
            ball.rotation_quaternion = rot; ball.keyframe_insert("rotation_quaternion", frame=f)
            hidden = any(j == i and a <= f <= b for j, a, b in inside)
            gone = S.play.b_gone
            if i == 1 and gone is not None and S.clock.t(f) > gone:
                hidden = True
            ball.hide_render = hidden; ball.keyframe_insert("hide_render", frame=f)
            ball.hide_viewport = hidden; ball.keyframe_insert("hide_viewport", frame=f)
        for fc in fcurves_of(ball):
            for kp in fc.keyframe_points:
                kp.interpolation = "LINEAR" if fc.data_path in ("location", "rotation_quaternion") else "CONSTANT"


def pulse(frames, rise, hold, fall, peak, rest=0.0):
    """Keys of a value that jumps to peak at each frame and falls back; a pulse during another's fall starts again."""
    keys = {1: rest}
    for fe in sorted(set(frames)):
        for f in [f for f in keys if f >= fe]:
            del keys[f]
        keys.setdefault(fe - rise, rest)
        keys[fe] = peak; keys[fe + hold] = peak; keys[fe + hold + fall] = rest
    return sorted(keys.items())


def key_parts(S, parts, play, clock, last, f_last_drain):
    """The flippers by the game's presses, the bumpers' coils at their kicks, the slings, the seal, the plunger,
    the scoop's kicker and the store's seesaw; the power-up pulls every coil once, in turn."""
    F = lambda t: int(round(clock.near(t)))
    shown = clock.shown
    # the flippers: the game's presses, a flick of each in the power-up, and the player's late presses at the end
    for side, pv in parts["flipper"].items():
        f_obj = game.ph.Flipper.__new__(game.ph.Flipper)
        f_obj.turn = ph.FLIPPER[side][1]
        f_obj.presses = sorted(play.presses[side])
        late = []
        if f_last_drain is not None:
            t_late = clock.t(f_last_drain - 10)
            late = [(t_late + (0.0 if side == "left" else 0.03), t_late + 0.25)]
        f_obj.presses = sorted(f_obj.presses + late)
        keys = [(1, 0.0)]
        flick = P(52) if side == "left" else P(58)
        keys += [(flick - 1, 0.0), (flick, ph.FLIP_ANGLE * f_obj.turn), (flick + 3, ph.FLIP_ANGLE * f_obj.turn), (flick + 6, 0.0)]
        prev = None
        for f in range(G0, last + 1):
            a = f_obj.angle(clock.t(f))
            keys.append((f, a))
        slim = [keys[0]] + [keys[i] for i in range(1, len(keys) - 1) if not (keys[i - 1][1] == keys[i][1] == keys[i + 1][1])] + [keys[-1]]
        key_curve(pv, "rotation_euler", 2, sorted(dict(slim).items()))
    # the bumpers' coils at their kicks, and in turn in the power-up
    test = {1: [P(22)], 2: [P(30)], 3: [P(38)]}
    for k, c in parts["coil"].items():
        frames = [F(e[0]) for e in play.events if e[1] == "bumper" and e[2] == str(k) and shown(e[0])] + test[k]
        key_curve(c, "location", 2, pulse(frames, 1, 2, 4, -0.4))
        for part, peak in (("base", 10.0), ("ring", 20.0)):
            lights.key_glow(f"bumper-{k}-{part}-lit", pulse(frames, 1, 0, 9, peak))
    # the slings' rubbers at their kicks
    for name, (e, n) in parts["sling"].items():
        frames = [F(ev[0]) for ev in play.events if ev[1] == "sling" and ev[2] == name and shown(ev[0])]
        for i in range(2):
            key_curve(e, "location", i, [(f, v * n[i]) for f, v in pulse(frames, 1, 1, 3, 0.15)])
    # the spinner's flap turns as a ball rolls through it and runs down
    sp, axis = parts["spinner"]
    sp.rotation_axis_angle = (0.0, axis.x, axis.y, axis.z)
    keys, angle, last_pass, prev_y = [(1, 0.0)], 0.0, -99, None
    for f in range(G0, last + 1):
        p, _ = S.ball(f, 0)
        x, y, z = p.x * 10.0, -p.y * 10.0, p.z * 10.0
        # the ball crosses the spinner's line between two frames, a frame's step being wider than the flap
        crossed = prev_y is not None and (prev_y - SPINNER[1]) * (y - SPINNER[1]) <= 0 and abs(prev_y - y) < 80.0
        prev_y = y
        if crossed and 12.0 < x < 37.0 and z < 10.0 and f - last_pass > 20:
            last_pass = f
            spin = 2 * math.pi * SPINNER_TURNS
            span = 30
            keys.append((f, angle))
            for k in range(1, span + 1):
                keys.append((f + k, angle + spin * (1 - (1 - k / span) ** 2)))
            angle += spin
    for f, v in keys:
        sp.rotation_axis_angle[0] = v
        sp.keyframe_insert("rotation_axis_angle", index=0, frame=f)
    # the seal turns at its hits
    turn, keys = 0.0, [(1, 0.0)]
    for e in play.events:
        if e[1] == "hit" and str(e[2]).startswith("seal-r") and shown(e[0]):
            way = rn.seal_turn([play.samples[e[4]]], [], e[0])
            fe = F(e[0]); fend = F(e[0] + 0.8)
            keys.append((fe, turn))
            amount = math.radians(120.0) * way
            for k in range(1, 9):
                keys.append((round(lerp(fe, fend, k / 8.0)), turn - amount * (1 - (1 - k / 8.0) ** 2)))
            turn -= amount
    key_curve(parts["seal"], "rotation_euler", 2, sorted(dict(keys).items()), interp="BEZIER")
    # the plunger draws back while the player pulls it and snaps forward as it shoots
    pk = [(1, 0.0)]
    for e in play.events:
        if e[1] == "kick" and math.hypot(e[3].x * 10.0 - rn.PLUNGER[0], -e[3].y * 10.0 - rn.PLUNGER[1]) < 4 and shown(e[0]):
            fs, fe = F(e[0] - game.PULL), F(e[0])
            pk += [(fs, 0.0), (fe - 1, -PLUNGER_PULL / 10.0), (fe, 0.0)]
    key_curve(parts["plunger"], "location", 1, sorted(dict(pk).items()))
    length = parts["spring"].dimensions.y                  # BU, the spring at rest, from the mount to the knob
    stretch = lambda d: 1.0 - d / length
    key_curve(parts["spring"], "scale", 1, [(f, stretch(d)) for f, d in sorted(dict(pk).items())])
    # the scoop's kicker rises at its kick, and in the power-up
    kk = [F(e[0]) for e in play.events if e[1] == "kick" and math.hypot(e[3].x * 10 - rn.SCOOP[0], -e[3].y * 10 - rn.SCOOP[1]) < 4 and shown(e[0])]
    key_curve(parts["kicker"], "location", 2, pulse(kk + [P(46)], 1, 2, 6, 0.75))
    # the seesaw tips to let ball B out of the store, and back
    rock, axis = parts["seesaw"]
    rock.rotation_mode = "AXIS_ANGLE"
    keys = [(1, 0.0)]
    finger = {"front": [(1, 0.0)], "back": [(1, 0.0)]}
    if play.release is not None:
        a, b = F(play.release + 0.55), F(play.release + 0.75)
        c, d = F(play.release + game.RELEASE - 0.25), F(play.release + game.RELEASE - 0.05)
        # the separator of tools/separator/separator.html: its seesaw rocks 21.25 degrees each way, and the pins
        # trade places over their 3.5 mm stroke
        along = Vector((axis.y, -axis.x, 0.0))
        start = along * math.cos(math.radians(SEESAW_TILT)) + Vector((0, 0, math.sin(math.radians(SEESAW_TILT))))
        tilt = -math.radians(2 * SEESAW_TILT)
        if (Quaternion(axis, tilt) @ start).z > 0:
            tilt = -tilt
        keys += [(a, 0.0), (b, tilt), (c, tilt), (d, 0.0)]
        finger["front"] += [(a, 0.0), (b, -PIN_STROKE), (c, -PIN_STROKE), (d, 0.0)]
        finger["back"] += [(a, 0.0), (b, PIN_STROKE), (c, PIN_STROKE), (d, 0.0)]
        key_curve(parts["horn"], "rotation_euler", 2, [(1, 0.0), (a, 0.0), (b, math.radians(60)), (c, math.radians(60)), (d, 0.0)], interp="BEZIER")
    rock.rotation_axis_angle = (0.0, axis.x, axis.y, axis.z)
    for f, v in keys:
        rock.rotation_axis_angle[0] = v
        rock.keyframe_insert("rotation_axis_angle", index=0, frame=f)
    for name, ks in finger.items():
        key_curve(parts["fingers"][name], "location", 2, ks, interp="BEZIER")


def P(f):
    """A frame of the power-up as cues.md times it over 175 frames, laid onto POWER frames."""
    return max(1, int(round(f * POWER / 175.0)))


# what the game scores and what the display calls out: a gate's or an event's points and its message
SCORE_GATES = {"top-lane": (5000, "SKILL SHOT"), "inlane": (500, "INLANE"), "u-track": (5000, "U TRACK"), "blue-exit": (2500, "RAMP"), "croc-hole": (7500, "CROCODILE"),
               "green-end": (3000, "TRACK"), "scoop": (2500, "BALL LOCKED")}
SCORE_EVENTS = {"bumper": 100, "sling": 10}
SCORE_HITS = {"seal-r": (25000, "SEAL SPIN"), "target": (1000, "TARGET")}
MULTIBALL_POINTS = 10000


def game_score(play):
    """The score's steps [(game time, points)] and the display's messages [(game time, text)]."""
    steps, msgs = [], []
    for s in play.segments:
        for g, t in s["marks"].items():
            if g in SCORE_GATES:
                pts, text = SCORE_GATES[g]
                steps.append((t, pts)); msgs.append((t, text))
        if s["name"] == "multiball":
            steps.append((s["t0"] + game.RELEASE, MULTIBALL_POINTS)); msgs.append((s["t0"] + game.RELEASE, "MULTIBALL"))
        if s["name"] == "finale":
            msgs.append((s["t1"], "GAME OVER"))
    seen = set()
    for e in play.events:
        if e[1] in SCORE_EVENTS:
            steps.append((e[0], SCORE_EVENTS[e[1]]))
        elif e[1] == "hit":
            for key, (pts, text) in SCORE_HITS.items():
                # one score and one message a hit of a part, not one for each contact of the same hit
                if str(e[2]).startswith(key) and not any(abs(e[0] - t) < 0.5 and x == text for t, x in msgs):
                    steps.append((e[0], pts)); msgs.append((e[0], text))
    return sorted(steps), sorted(msgs)


def insert_table():
    """Every lit insert: its material, its point light, its middle (x, y) mm and its strength at rest."""
    out = []
    for material, (axis, shift, strength) in lights.INSERTS.items():
        for m in [m for m in bpy.data.materials if m.name.startswith(material + "-c")]:
            users = [o for o in bpy.data.objects if o.type == "MESH" and o.data.materials and o.data.materials[0] == m]
            if not users:
                continue
            light = bpy.data.lights.get(f"lamp-{material}-{m.name.rsplit('-c', 1)[1]}")
            out.append((m, light, lights.centre(users[0]), strength))
    return out


def key_lights(S, play, clock, f_game0, f_game1, f_mb0, f_mb1, f_drain, f_hit):
    """The lights: dark at first, the power-up's wave, then lights that answer the game. An insert flashes as a
    ball rolls over it and as a ball hits a part near it; the strips flash at each feature the display calls out
    and run through the multiball's palette while two balls play; the drain flashes red as a ball drains. At the
    trail's hit every light goes dark."""
    F = lambda t: int(round(clock.frame(t)))
    shown = lambda t: clock.shown(t)
    _, msgs = game_score(play)
    feats = [F(t) for t, _ in msgs if shown(t)]
    off = f_hit - 8
    # the strips and the room: dark, then the power-up; a flash at each feature
    per = {1: 0.0, P(100): 0.0, P(130): lights.PERIMETER}
    ceil = {1: 0.0, P(115): 0.0, P(145): 1.0}
    for fe in feats:
        per.setdefault(fe - 1, lights.PERIMETER); per[fe + 2] = lights.PERIMETER * 3.0; per[fe + 14] = lights.PERIMETER
        ceil.setdefault(fe - 1, 1.0); ceil[fe + 2] = 2.5; ceil[fe + 14] = 1.0
    per[off] = 0.0; ceil[off] = 0.0
    lights.key_glow("perimeter", sorted(per.items()))
    lights.key_ceiling(sorted(ceil.items()))
    # the inserts: the power-up's wave down the board, then flashes where the balls roll and hit
    hits = [(F(e[0]), e[3]) for e in play.events if e[1] in ("bumper", "sling", "hit", "kick") and shown(e[0])]
    table = insert_table()
    for m, light, (cx, cy), strength in table:
        on = int(P(120) + P(50) * max(0.0, min(1.0, cy / 400.0)))
        keys = {1: 0.0, on - 1: 0.0, on + 3: strength * 2.0, on + 10: strength}
        flashes = []
        for f in range(f_game0, f_game1 + 1):
            for i in (0, 1):
                p, _ = S.ball(f, i)
                x, y, z = p.x * 10.0, -p.y * 10.0, p.z * 10.0
                if z < 7.0 and math.hypot(x - cx, y - cy) < 5.0 and (not flashes or f - flashes[-1] > 15):
                    flashes.append(f)
        for fe, at in hits:
            if math.hypot(at.x * 10.0 - cx, -at.y * 10.0 - cy) < 40.0:
                flashes.append(fe)
        for fe in sorted(set(flashes)):
            if fe <= on + 10 or fe >= off - 12:
                continue
            keys.setdefault(fe - 1, strength); keys[fe + 1] = strength * 4.0; keys[fe + 12] = strength
        keys[off] = 0.0
        sock = m.node_tree.nodes["Principled BSDF"].inputs["Emission Strength"]
        for f, v in sorted(keys.items()):
            sock.default_value = v; sock.keyframe_insert("default_value", frame=f)
            if light:
                light.energy = lights.INSERT_LIGHT * v; light.keyframe_insert("energy", frame=f)
    # the drain: dark at rest, a red flash as a ball drains
    drains = [F(t) for t, _ in play.drains if shown(t)]
    lights.key_glow("diffuser-drain", [(1, 0.0)] + [kv for fe in drains for kv in ((fe - 1, 0.0), (fe + 2, 30.0), (fe + 18, 0.0))])
    lights.key_color("diffuser-drain", [(1, "#ff3a2a")])
    # the strips' colors: warm white in the power-up, the march's palette a beat a color, the multiball's while two play
    lights.key_color("perimeter", [(1, None), (POWER, None)])
    lights.key_color("ceiling-led", [(1, None), (POWER, None)])
    strip = bpy.data.lights["lamp-ceiling-strip"]
    for f in (1, POWER):
        strip.keyframe_insert("color", frame=f)
    lights.key_chase(range(POWER + 1, f_mb0 if f_mb0 else f_hit, BEAT))
    if f_mb0:
        lights.key_chase(range(f_mb0, f_mb1, BEAT // 2), MULTI)
        lights.key_chase(range(f_mb1, f_hit, BEAT))
    for name, full in (("room-key", 8000.0), ("room-fill", 250.0), ("room-backfill", 6000.0)):
        d = bpy.data.lights[name]
        for f, v in ((1, 0.08), (P(110), 0.08), (POWER, 1.0), (f_hit - 4, 0.0)):
            d.energy = full * v; d.keyframe_insert("energy", frame=f)
    for name in ("lamp-perimeter-left", "lamp-perimeter-right"):
        d = bpy.data.lights[name]
        for f, v in ((1, 0.0), (P(100), 0.0), (P(135), 2500.0), (off, 0.0)):
            d.energy = v; d.keyframe_insert("energy", frame=f)


def font(paths):
    path = next((p for p in paths if os.path.exists(p)), None)
    return bpy.data.fonts.load(path, check_existing=True) if path else None


def emit_material(name, color, strength):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (0.0, 0.0, 0.0, 1.0)
    b.inputs["Emission Color"].default_value = common.hexcol(color)
    b.inputs["Emission Strength"].default_value = strength
    return m


COUNT = 6              # frames a jump of the score counts up over, as in the intro
CALL = 45              # frames the display shows a feature's line


def build_display(play, clock, f_game0, f_over, f_hit, last):
    """The display in the intro's look, drawn by display.py from out/display.json: dark through the power-up; from
    the game's start BALL 1 over the score, each feature's line for CALL frames, GAME OVER from the last drain;
    a white flash as the trail hits it, then black."""
    F = lambda t: int(round(clock.near(t)))
    steps, msgs = game_score(play)
    total = lambda t: sum(p for ts, p in steps if ts <= t)
    calls = sorted((F(t), body) for t, body in msgs if clock.shown(t))
    mb = play.seg("multiball")
    multi = (F(mb["t0"] + game.RELEASE), F(mb["t1"])) if mb else None
    frames, shown, start, target = {}, 0, 0, 0
    for f in range(f_game0, f_hit):
        v = total(clock.t(f))
        if v != target:
            start, target, f_jump = shown, v, f
        shown = int(target if target == start else start + (target - start) * min(1.0, (f - f_jump + 1) / COUNT)) // 10 * 10
        line = "MULTIBALL" if multi and multi[0] <= f <= multi[1] else "BALL 1"
        for fc, body in calls:
            if fc <= f and (body == "GAME OVER" or f < fc + CALL):
                line = body
        frames[f] = [shown, line]
    os.makedirs(os.path.join(HERE, "out"), exist_ok=True)
    json.dump({"last": last, "frames": frames}, open(os.path.join(HERE, "out", "display.json"), "w"))
    # the glass shows the frames as its light: dark before the game, the flash, then black
    folder = os.path.join(os.path.dirname(HERE), "display")
    m = bpy.data.materials.new("tr-display"); m.use_nodes = True; nt = m.node_tree
    tex = nt.nodes.new("ShaderNodeTexImage")
    img = bpy.data.images.new("tr-display-frames", 320, 240)
    img.source = "SEQUENCE"; img.filepath = os.path.join(folder, "d0001.png")
    tex.image = img
    tex.image_user.frame_start = 1; tex.image_user.frame_offset = 0; tex.image_user.frame_duration = last
    tex.image_user.use_auto_refresh = True
    flash = nt.nodes.new("ShaderNodeMix"); flash.data_type = "RGBA"
    nt.links.new(tex.outputs["Color"], flash.inputs[6])
    flash.inputs[7].default_value = (1.0, 1.0, 1.0, 1.0)
    b = nt.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (0.01, 0.012, 0.02, 1.0); b.inputs["Roughness"].default_value = 0.1
    nt.links.new(flash.outputs[2], b.inputs["Emission Color"])
    sock, fac = b.inputs["Emission Strength"], flash.inputs[0]
    for f, v, w in ((1, 0.0, 0.0), (f_game0 - 1, 0.0, 0.0), (f_game0, 2.5, 0.0), (f_hit - 1, 2.5, 0.0), (f_hit, 40.0, 1.0),
                    (f_hit + 3, 0.0, 1.0)):
        sock.default_value = v; sock.keyframe_insert("default_value", frame=f)
        fac.default_value = w; fac.keyframe_insert("default_value", frame=f)
    bpy.data.objects["display-screen"].data.materials[0] = m


NEON, BULB, PLATE = "#ff3fb4", "#ffd23f", "#1a0a26"
LABEL_FONT = ("C:/Windows/Fonts/seguibl.ttf", "C:/Windows/Fonts/ariblk.ttf")
DEPTH = 0.25           # BU, 2.5 mm: how far in front of its camera a callout floats, nearer than any part


def flat(name, polys, material, c, parent):
    """A flat mesh of polygons [(x, y)] in the callout's plane, parented to the callout."""
    import bmesh
    me = bpy.data.meshes.new(name); bm = bmesh.new()
    for poly in polys:
        bm.faces.new([bm.verts.new((x, y, 0.0)) for x, y in poly])
    bm.to_mesh(me); bm.free(); me.materials.append(material)
    o = bpy.data.objects.new(name, me); c.objects.link(o); o.parent = parent
    o.visible_shadow = False
    return o


def ring_polys(r_in, r_out, n=32):
    return [[(r_out * math.cos(2 * math.pi * k / n), r_out * math.sin(2 * math.pi * k / n)),
             (r_out * math.cos(2 * math.pi * (k + 1) / n), r_out * math.sin(2 * math.pi * (k + 1) / n)),
             (r_in * math.cos(2 * math.pi * (k + 1) / n), r_in * math.sin(2 * math.pi * (k + 1) / n)),
             (r_in * math.cos(2 * math.pi * k / n), r_in * math.sin(2 * math.pi * k / n))] for k in range(n)]


def stroke(p, q, w):
    d = (Vector(q) - Vector(p)); d.normalize(); nrm = Vector((-d.y, d.x)) * w / 2
    return [(p[0] + nrm.x, p[1] + nrm.y), (q[0] + nrm.x, q[1] + nrm.y), (q[0] - nrm.x, q[1] - nrm.y), (p[0] - nrm.x, p[1] - nrm.y)]


def star_poly(r):
    return [[(r * (1.0 if k % 2 == 0 else 0.45) * math.sin(math.pi * k / 5), r * (1.0 if k % 2 == 0 else 0.45) * math.cos(math.pi * k / 5))
             for k in range(10)]]


def build_callouts(S, shots, cams):
    """The Arcade's callouts on each feature's shot: a sign with a neon rim and the feature's name between two
    stars, a row of chaser lights to the part with a blinking double chevron, and a lit ring with a pulse on the
    part. The sign leans out to the side of the picture the part lies on and pops up as the shot opens; the ring
    and the arrow follow the part as the camera moves."""
    c = coll("trailer-callouts")
    fnt = font(LABEL_FONT)
    N = 9
    for name, f0, f1, poses, world, extra in shots:
        if "callout" not in extra:
            continue
        label, at = extra["callout"]
        cam = cams[name]
        part = at if isinstance(at, Vector) else S.Mt @ S.board(*at)
        g = bpy.data.objects.new("tr-callout-" + name, None); c.objects.link(g); g.parent = cam
        g.location = (0.0, 0.0, -DEPTH)
        # the sign
        w, h = 0.05 * 0.66 * len(label) + 0.16, 0.085
        plate = flat("tr-co-plate-" + name, [[(-w / 2, -h / 2), (w / 2, -h / 2), (w / 2, h / 2), (-w / 2, h / 2)]],
                     emit_material("tr-co-plate-" + name, PLATE, 1.0), c, g)
        rim = flat("tr-co-rim-" + name, [[(-w / 2 - 0.006, -h / 2 - 0.006), (w / 2 + 0.006, -h / 2 - 0.006),
                                         (w / 2 + 0.006, h / 2 + 0.006), (-w / 2 - 0.006, h / 2 + 0.006)]],
                   emit_material("tr-co-rim-" + name, NEON, 10.0), c, plate)
        rim.location = (0.0, 0.0, -0.002)
        cu = bpy.data.curves.new("tr-co-text-" + name, "FONT"); cu.body = label; cu.size = 0.05
        cu.align_x = "CENTER"; cu.align_y = "CENTER"
        if fnt:
            cu.font = fnt
        cu.materials.append(emit_material("tr-co-text-" + name, BULB, 8.0))
        txt = bpy.data.objects.new("tr-co-text-" + name, cu); c.objects.link(txt); txt.parent = plate
        txt.location = (0.0, 0.0, 0.002); txt.visible_shadow = False
        for sx in (-1, 1):
            st = flat("tr-co-star-" + name, star_poly(0.016), emit_material("tr-co-star-%s-%d" % (name, sx), BULB, 8.0), c, plate)
            st.location = (sx * (w / 2 - 0.04), 0.0, 0.002)
        # the ring on the part and its pulse
        ring = flat("tr-co-ring-" + name, ring_polys(0.024, 0.031), emit_material("tr-co-ring-" + name, BULB, 10.0), c, g)
        pm = emit_material("tr-co-pulse-" + name, NEON, 8.0)
        pulse_o = flat("tr-co-pulse-" + name, ring_polys(0.028, 0.033), pm, c, ring)
        # the chaser lights and the chevron
        dots = []
        for k in range(N):
            dm = emit_material("tr-co-dot-%s-%d" % (name, k), BULB, 2.0)
            dots.append((flat("tr-co-dot-%s-%d" % (name, k), [[(0.007 * math.cos(2 * math.pi * j / 12), 0.007 * math.sin(2 * math.pi * j / 12))
                                                                for j in range(12)]], dm, c, g), dm))
        s = 0.0026
        chev = [stroke((-16 * s, -7 * s), (-9 * s, 0), 3.5 * s), stroke((-9 * s, 0), (-16 * s, 7 * s), 3.5 * s),
                stroke((-8 * s, -7 * s), (-1 * s, 0), 3.5 * s), stroke((-1 * s, 0), (-8 * s, 7 * s), 3.5 * s)]
        hm = emit_material("tr-co-head-" + name, NEON, 10.0)
        head = flat("tr-co-head-" + name, chev, hm, c, g)
        # per frame: the part's place in the picture, in units of half the picture's width
        side = None
        for f in range(f0, f1 + 1):
            loc, q, lens = poses[f]
            if not world:
                loc, q = S.Mt @ loc, S.Q @ q
            hw = DEPTH * 18.0 / lens
            pc = q.inverted() @ (part - loc)
            seen = pc.z < -0.05
            rx, ry = ((pc.x / -pc.z) / (18.0 / lens), (pc.y / -pc.z) / (18.0 / lens)) if seen else (0.0, 0.0)
            seen = seen and abs(rx) < 1.0 and abs(ry) < 0.56
            if side is None:
                side = -1.0 if rx < 0 else 1.0
            k = f - f0
            pop = 1.0 if k >= 8 else (0.4 + 0.6 * smooth(k / 6.0)) * (1.0 + 0.12 * math.sin(math.pi * min(1.0, k / 8.0)))
            show = 0.0 if f > f1 - 2 else 1.0
            g.scale = (hw * show, hw * show, hw * show); g.keyframe_insert("scale", frame=f)
            lx = max(-0.95 + w / 2, min(0.95 - w / 2, rx + side * 0.32))
            ly = max(-0.5, min(0.47, ry + 0.24))
            plate.location = (lx, ly, 0.0); plate.keyframe_insert("location", frame=f)
            plate.scale = (pop, pop, pop); plate.keyframe_insert("scale", frame=f)
            a = 1.0 if seen else 0.0
            ring.location = (rx, ry, 0.0); ring.keyframe_insert("location", frame=f)
            ring.scale = (a, a, a); ring.keyframe_insert("scale", frame=f)
            ph_ = (k % 30) / 30.0
            pulse_o.scale = (1.0 + 1.2 * ph_,) * 3; pulse_o.keyframe_insert("scale", frame=f)
            pm.node_tree.nodes["Principled BSDF"].inputs["Emission Strength"].default_value = 8.0 * (1.0 - ph_)
            pm.node_tree.nodes["Principled BSDF"].inputs["Emission Strength"].keyframe_insert("default_value", frame=f)
            ex, ey = lx, ly - h / 2 - 0.01
            dx, dy = rx - ex, ry - ey
            dist = math.hypot(dx, dy)
            ang = math.atan2(dy, dx)
            tx, ty = rx - math.cos(ang) * 0.045, ry - math.sin(ang) * 0.045
            for j, (dot, dm) in enumerate(dots):
                u = (j + 0.5) / N
                dot.location = (ex + (tx - ex) * u, ey + (ty - ey) * u, 0.0); dot.keyframe_insert("location", frame=f)
                v = a if dist > 0.12 else 0.0
                dot.scale = (v, v, v); dot.keyframe_insert("scale", frame=f)
                lit = ((f // 2) - j) % 3 == 0
                sk = dm.node_tree.nodes["Principled BSDF"].inputs["Emission Strength"]
                sk.default_value = 12.0 if lit else 1.5; sk.keyframe_insert("default_value", frame=f)
            head.location = (tx, ty, 0.0); head.keyframe_insert("location", frame=f)
            head.rotation_euler = (0.0, 0.0, ang); head.keyframe_insert("rotation_euler", frame=f)
            hv = a if dist > 0.12 else 0.0
            head.scale = (hv, hv, hv); head.keyframe_insert("scale", frame=f)
            hs = hm.node_tree.nodes["Principled BSDF"].inputs["Emission Strength"]
            hs.default_value = 12.0 if (f // 7) % 2 == 0 else 2.0; hs.keyframe_insert("default_value", frame=f)
        for f, v in ((f0 - 1, 0.0), (f1 + 1, 0.0)):
            g.scale = (v, v, v); g.keyframe_insert("scale", frame=f)


def build_logo(f0):
    """The logo, its keys moved from the intro's frames 126 to 175 onto the trailer's from f0, stretched as the
    intro stretches them. Its camera stays, as the last shot's."""
    logo.build()
    marks = [(126, f0), (131, f0 + 8), (150, f0 + 32), (175, f0 + 68)]

    def remap(f):
        for (a0, b0), (a1, b1) in zip(marks, marks[1:]):
            if f <= a1:
                return b0 + (f - a0) * (b1 - b0) / (a1 - a0)
        return marks[-1][1] + f - marks[-1][0]

    blocks = list(bpy.data.collections["logo"].objects)
    blocks += [o.data for o in blocks if o.data is not None]
    blocks += [m for m in bpy.data.materials if m.name.startswith("logo-")]
    blocks += [m.node_tree for m in bpy.data.materials if m.name.startswith("logo-") and m.node_tree]
    done = set()
    for b in blocks:
        ad = getattr(b, "animation_data", None)
        if not ad or not ad.action or ad.action.name in done:
            continue
        done.add(ad.action.name)
        for fc in fcurves_of(b):
            for kp in fc.keyframe_points:
                kp.co.x = remap(kp.co.x)
                kp.handle_left.x = remap(kp.handle_left.x); kp.handle_right.x = remap(kp.handle_right.x)
            fc.update()
    return bpy.data.objects["logo-cam"], f0 + 68


# ---------------------------------------------------------------- checks
def check_shots(S, shots):
    """Every fourth frame of each shot: whether its ball is in the picture and seen, how much of the picture
    the machine fills, and how near the nearest thing in the middle of the picture is. Returns a report."""
    bvh = S.view.bvh
    report = {}
    for name, f0, f1, poses, world, extra in shots:
        if world:
            continue
        out_of, hidden, fill, near, n = 0, 0, [], [], 0
        for f in range(f0, f1 + 1, 4):
            cam, q, lens = poses[f]
            tw = 18.0 / lens; th = tw * 9.0 / 16.0
            hits, dists = 0, []
            for gx in range(7):
                for gy in range(4):
                    d = q @ Vector(((gx / 6 * 2 - 1) * tw * 0.9, (gy / 3 * 2 - 1) * th * 0.9, -1.0))
                    loc, nor, idx, dist = bvh.ray_cast(cam, d.normalized(), 60.0)
                    if loc is not None:
                        hits += 1; dists.append(dist)
            fill.append(hits / 28.0)
            mid = bvh.ray_cast(cam, (q @ Vector((0, 0, -1))).normalized(), 60.0)
            near.append(mid[3] * 10.0 if mid[0] is not None else 999.0)
            i = extra.get("ball")
            if i is not None:
                p, _ = S.ball(f, i)
                v = q.inverted() @ (p - cam)
                if v.z >= 0 or abs(v.x / -v.z) > tw * 0.95 or abs(v.y / -v.z) > th * 0.95:
                    out_of += 1
                elif bvh.ray_cast(cam, (p - cam).normalized(), max((p - cam).length - BALL, 0.0))[0] is not None:
                    hidden += 1
            n += 1
        report[name] = {"frames": [f0, f1], "out": out_of, "hidden": hidden, "n": n,
                        "fill": round(min(fill), 2) if fill else None, "nearest_mm": round(min(near), 1) if near else None}
    return report


# ---------------------------------------------------------------- all of it
def build():
    ph.build_scene(CGI)
    sc = bpy.context.scene
    sc.frame_set(1)
    machine = ph.Machine()
    solution = json.load(open(os.path.join(HERE, "game.json")))
    play = Play(machine, solution)
    tilt = bpy.data.objects["machine-tilt"]
    for n in ("ball-other", "ball-camera"):
        o = bpy.data.objects[n]; o.hide_render = True; o.hide_viewport = True
    # the clock: the windows the film shows, slow around what it shows
    slow = []
    for s in play.segments:
        for g, t in s["marks"].items():
            if g in ("top-lane", "seal", "target", "a-bumper", "knock", "croc-loop", "blue-loop", "green-top"):
                slow.append((t, 0.15, 0.35, 0.2))
    for e in play.events:
        if e[1] == "kick" and math.hypot(e[3].x * 10 - rn.SCOOP[0], -e[3].y * 10 - rn.SCOOP[1]) < 4:
            slow.append((e[0], 0.1, 0.5, 0.2))
    clock = Clock(windows(play), slow)
    F = lambda t: int(round(clock.near(t)))
    view = ph.Machine(view=True)
    parts = build_parts(tilt)
    balls = [make_ball("tr-ball-a", tilt), make_ball("tr-ball-b", tilt)]
    S = Shots(tilt, play, clock, view, balls_at_factory(play, clock))
    # the shots: the power-up's flight, the game, the trail's flight and the logo
    shots = [("power", 1, POWER, S.track(1, POWER, POWER_CAM, POWER_AIM, 24.0), False, {})]
    game_shots = plan_shots(S, play, clock)
    shots += game_shots
    final = play.segments[-1]
    t_end = final["t1"]
    # after the last drain the film cuts to the display; 70 frames later it flashes and goes black
    f_drain = game_shots[-1][2] + 1
    f_hit = f_drain + 70
    f_logo = f_hit + 14
    logo_cam, f_logo_end = build_logo(f_logo)
    last = f_logo_end + 60
    # the camera moves in on the display, which shows GAME OVER and the final score, until it goes black
    shots.append(("screen", f_drain, f_hit, S.screen(f_drain, f_hit, 300.0, 120.0, 35.0, 14.0, 0.0), True,
                  {"callout": ("TOUCH SCREEN", SCREEN + Vector((-3.0, 0.0, 0.0)))}))
    apron, apron_parts = build_apron(tilt)
    key_apron(apron, apron_parts, shots)
    inside = [(e["inside_ball"][0], e["inside_ball"][1], e["inside_ball"][2]) for *_, e in shots if "inside_ball" in e]
    key_balls(S, balls, last, inside)
    key_parts(S, parts, play, clock, last, F(t_end))
    mb = play.seg("multiball")
    f_mb0 = F(mb["t0"]) if mb else None
    f_mb1 = F(mb["t1"]) if mb else None
    key_lights(S, play, clock, G0, game_shots[-1][2], f_mb0, f_mb1, f_drain, f_hit)
    cams = key_cameras(S, shots, last)
    build_display(play, clock, G0, F(t_end), f_hit, last)
    build_callouts(S, shots, cams)
    m = sc.timeline_markers.new("logo", frame=f_logo); m.camera = logo_cam
    # black from the trail's hit until the logo, which fades out at the end
    vs = sc.view_settings
    # the logo's first frames, from the intro's frames before its cut, put the trail's head in the wrong place;
    # the picture stays black through them
    for f, v in ((1, 0.0), (f_hit, 0.0), (f_hit + 4, -20.0), (f_logo + LOGO_SKIP - 1, -20.0), (f_logo + LOGO_SKIP, 0.0),
                 (last - 15, 0.0), (last, -10.0)):
        vs.exposure = v; vs.keyframe_insert("exposure", frame=f)
    sc.frame_start, sc.frame_end = 1, last
    sc.render.fps = FPS
    sc.render.resolution_x, sc.render.resolution_y, sc.render.resolution_percentage = 1920, 1080, 100
    # the ball stays sharp in the slow motion; the intro's motion blur would otherwise carry over from a build of
    # the intro in the same Blender
    sc.render.use_motion_blur = False
    lights.look()
    sc.frame_set(1)
    bpy.ops.wm.save_as_mainfile(filepath=OUT)
    report = check_shots(S, game_shots)
    return {"last": last, "shots": [(s[0], s[1], s[2]) for s in shots],
            "segments": [(s["name"], round(s["t0"], 2), round(s["t1"], 2), s["cost"]) for s in play.segments],
            "check": report, "file": OUT}


result = build()
print(json.dumps(result, indent=1, default=str))
