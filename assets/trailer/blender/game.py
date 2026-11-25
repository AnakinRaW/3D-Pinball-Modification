"""The trailer's game: one game of one player, from the plunger to the last drain, simulated in segments.

A segment starts where the one before it ended, with the balls where they are and as fast as they roll. A segment
marked cut starts behind a hard cut of the film instead: one ball comes down an inlane onto a flipper, from one of
CUT_STARTS, which its parameter "from" picks, so its search does not depend on the segments before it. Most
segments start with a ball rolling down onto a flipper, and the player flips it: when the flipper is pressed sets
where the ball goes, as in a real game, where the kit's flippers hold no ball. A segment's parameters are what the
player and the machine do in it: how hard the plunger shoots, when the player flips and how long the flipper is
held, when the scoop kicks. Its goal is the gates of runs.py the balls have to pass, and it ends where the next
segment takes over: a ball rolling down onto a flipper, a ball in the scoop, or the last drain. Between the start
and the end the player flips every ball that comes down onto a flipper. search() finds a segment's parameters,
solve.py searches the segments in order and writes them to game.json, and play() runs the whole game again.
Positions are millimeters of the drawing, speeds mm/s, times seconds.
"""
import math, random
from mathutils import Vector
import physics as ph
import runs as rn
from runs import Gate, at, vel

STILL = 1.5             # BU/s, 15 mm/s: a ball held on a flipper or seated in the scoop rolls no faster
STUCK = 1.0             # BU/s, 10 mm/s: a ball slower than this anywhere else is stuck
STALL = 0.8             # s a ball may be stuck before the segment fails
TOP_LANES = [(134.3, 38.3), (159.7, 44.6), (185.3, 51.4)]
# the scoop throws the ball as the intro's does, up the board toward bumper 3's lower left side
KICK_AIM = math.atan2(109.9 - rn.SCOOP[1], 64.0 - rn.SCOOP[0])
PULL = 0.5              # s the plunger is drawn back before it shoots
RELEASE = 1.4           # s from the lock in the scoop until ball B rests at the plunger, let out of the store
APPROACH = 3.0          # BU, 30 mm: a segment ends as a ball rolling down comes this near a resting flipper
# where a segment behind a cut starts: the ball leaving an inlane toward its flipper, (x, y, z) mm and (vx, vy)
# mm/s, taken from simulated runs of the ball coming down each inlane
CUT_STARTS = [("left", (49.34, 333.29, 4.5), (156.5, 18.3)), ("right", (168.08, 333.59, 4.5), (-158.2, 16.3))]
# a segment that shows an inlane's sensor starts higher: the ball rolls down the outer inlane over its light and
# sensor, and on down the inlane rail onto the flipper
INLANE_STARTS = [("left", (28.05, 268.0, 4.5), (0.0, 140.0)), ("right", (189.05, 268.0, 4.5), (0.0, 140.0))]
INLANES = [(28.05, 290.95), (189.05, 290.95)]
# a segment that shows the U track starts with the ball on its way up the left lane, below the spinner
SPINNER_START, SPINNER_LANE = (30.5, 195.0, 4.5), 75.0


class AnyGate(Gate):
    """A gate passed at the first of several points."""

    def __init__(self, name, points, reach=3.0, zmax=None, zmin=None, ball=None):
        super().__init__(name, points[0][0], points[0][1], reach, zmax, zmin, None, ball)
        self.points = points


ORBIT_TOP = Gate("orbit", 200, 50, reach=8, zmin=20)
TOP_LANE = AnyGate("top-lane", TOP_LANES, reach=3.5, zmax=8)
INLANE = AnyGate("inlane", INLANES, reach=3.5, zmax=8)
BLUE = [Gate("blue-loop", 100, 35, reach=20, zmin=25), Gate("blue-exit", 33, 295, reach=12)]
CROC = [Gate("croc-mouth", 172, 180, reach=10), Gate("croc-loop", 188, 131, reach=12, zmin=8),
        Gate("croc-ride", 208, 250, reach=8, zmin=12), Gate("croc-hole", 189.5, 310.5, reach=15)]
ORBIT = [Gate("orbit", 200, 50, reach=8, zmin=20), Gate("over", 40, 20, reach=18, zmin=15)]
# the green track: in from the board, up the left side, across the top and down to its end above the right bell
GREEN = [Gate("green-entry", 66, 192, reach=10), Gate("green-climb", 26, 105, reach=10, zmin=8),
         Gate("green-top", 140, 16, reach=12, zmin=12), Gate("green-end", 199, 71, reach=15, zmin=8)]
SEAL = [Gate("seal", event=("hit", "seal-r"))]
TARGET = [Gate("target", event=("hit", "target-face"))]
# the U track: the lane round the top under the green track, past its sensor
U_TRACK = [Gate("u-track", 70.0, 6.0, reach=8, zmax=8)]
# the last drain: down the middle, between the flippers' tips, into the drain
TIPS = Gate("tips", rn.DRAIN[0], 402, reach=13, event=("drained", ""))


# ---------------------------------------------------------------- the state between segments
class State:
    """Where the game stands at time t: the balls, the presses of each flipper, the flipper the ball comes onto."""

    def __init__(self, t, balls, presses, held=None):
        self.t, self.balls, self.presses, self.held = t, balls, presses, held

    def copy(self):
        return State(self.t, [b.copy() for b in self.balls], {k: list(v) for k, v in self.presses.items()}, self.held)


def start():
    """Ball A rests at the foot of the plunger lane; ball B waits in the store under the apron, out of the run."""
    return State(0.0, [ph.Ball(at(*rn.PLUNGER), vel(0, 0))], {"left": [], "right": []})


def cut(state, q):
    """The state behind a cut: at the time the segment before it ended, the one ball in play comes down the inlane
    CUT_STARTS[q["from"]] onto its flipper."""
    if "speed" in q:
        # the ball comes up the left lane toward the spinner and the U track behind it
        d = (-math.cos(math.radians(SPINNER_LANE)), -math.sin(math.radians(SPINNER_LANE)))
        b = ph.Ball(at(*SPINNER_START), vel(d[0] * q["speed"], d[1] * q["speed"]))
        return State(state.t, [b], {k: list(v) for k, v in state.presses.items()}, "left")
    starts = INLANE_STARTS if q.get("inlane") else CUT_STARTS
    side, p, v = starts[min(int(q["from"]), len(starts) - 1)]
    return State(state.t, [ph.Ball(at(*p), vel(*v))], {k: list(v) for k, v in state.presses.items()}, side)


# ---------------------------------------------------------------- what happens in a segment
def plan_launch(st, q):
    """The plunger shoots ball A; from then on the player flips every ball that comes down onto a flipper."""
    st.balls[0].kicks = [(st.t + PULL, vel(0, -q["launch"]))]
    return [(st.t + PULL, q["save"])]


def plan_shot(st, q):
    """The ball rolls down onto the flipper st.held; the player presses it after delay and holds it for h. Then
    the player flips every ball that comes down onto a flipper, after save."""
    side = st.held
    hit = st.t + q["delay"]
    st.presses[side].append((hit, hit + q["h"]))
    return [(hit + q["h"] + 0.05, q["save"])]


def plan_multiball(st, q):
    """Ball A lies in the scoop. The store releases ball B into the plunger lane, which takes RELEASE, and the
    plunger shoots it; the scoop throws A out toward bumper 3. The player flips at every ball that comes onto a
    flipper. In the run B waits at the plunger from the start; the trailer shows it on its way out of the store
    meanwhile."""
    go = st.t + RELEASE
    b = ph.Ball(at(*rn.PLUNGER), vel(0, 0), kicks=[(go + PULL, vel(0, -q["launch"]))])
    st.balls.append(b)
    a = st.balls[0]
    kick = q["kick"]
    a.kicks = [(go + q["out"], vel(kick * math.cos(KICK_AIM), kick * math.sin(KICK_AIM), q["up"]))]
    return [(st.t, q["save"])]


def plan_drain(st, q):
    """The last ball comes down the inlane, and the player lets it go: no flipper moves, and it rolls off the
    flipper's tip into the drain."""
    return []


SHOT = {"delay": (0.0, 0.25), "h": (0.12, 0.35), "save": (0.0, 0.05)}
CUT_SHOT = dict(SHOT, **{"from": (0.0, len(CUT_STARTS))})

# Each feature is one short segment behind a cut: the ball comes down an inlane, the player flips it at the
# feature, and the segment ends AFTER seconds after the feature's last gate, where the film cuts on.
GAME = [
    {"name": "launch", "params": {"launch": (1100.0, 1500.0), "save": (0.0, 0.05)}, "plan": plan_launch,
     "goals": [[ORBIT_TOP, TOP_LANE]], "end": "after", "after": 1.5, "t_max": 6.0, "prefer": "bumper"},
    {"name": "tunnel", "cut": True, "optional": True, "tries": 300, "params": CUT_SHOT, "plan": plan_shot, "goals": [U_TRACK],
     "end": "after", "after": 0.5, "t_max": 5.0},
    {"name": "seal", "cut": True, "params": dict(CUT_SHOT, inlane=(1.0, 1.0), delay=(0.3, 0.9)), "plan": plan_shot, "goals": [[INLANE] + SEAL],
     "end": "after", "after": 0.7, "t_max": 5.0},
    {"name": "target", "cut": True, "params": CUT_SHOT, "plan": plan_shot, "goals": [TARGET], "end": "after",
     "after": 0.25, "t_max": 4.0},
    {"name": "scoop", "cut": True, "tries": 200, "params": CUT_SHOT, "plan": plan_shot,
     "goals": [[Gate("scoop", rn.SCOOP[0], rn.SCOOP[1], reach=3.0, zmax=0.5)]], "end": "scoop", "t_max": 5.0},
    {"name": "multiball", "params": {"launch": (1150.0, 1500.0), "out": (0.9, 2.2), "kick": (250.0, 700.0), "up": (550.0, 1200.0),
                                     "save": (0.0, 0.05)},
     "plan": plan_multiball,
     "goals": [[Gate("b-orbit", 200, 50, reach=8, zmin=20, ball=1), Gate("a-bumper", event=("bumper", "3"), ball=0),
                Gate("knock", event=("balls", ""))]],
     "end": "after", "after": 1.2, "t_max": 10.0, "prefer": "bumper"},
    # the last ball: behind a cut it comes down an inlane, the player lets it go, and it drains
    {"name": "finale", "cut": True, "params": {"from": (0.0, len(CUT_STARTS))}, "plan": plan_drain,
     "goals": [[TIPS], [Gate("drained", event=("drained", ""))]], "end": "drain", "t_max": 4.0},
]


# ---------------------------------------------------------------- running a segment
class Tracker:
    """Follows the gates of a goal as the run goes, in order, and the nearest the run came to the next one."""

    def __init__(self, goal):
        self.goal, self.i, self.marks, self.best, self.seen = goal, 0, {}, 1e9, 0

    def update(self, t, balls, events):
        while self.i < len(self.goal):
            g = self.goal[self.i]
            if g.event:
                kind, label = g.event
                hit = None
                for k in range(self.seen, len(events)):
                    e = events[k]
                    if (e[1] == kind and str(e[2]).startswith(label) and (kind == "balls" or g.ball is None or e[4] == g.ball)
                            and (g.x is None or math.hypot(e[3].x * 10.0 - g.x, -e[3].y * 10.0 - g.y) <= g.reach)):
                        hit = k; break
                if hit is None:
                    self.seen = len(events)
                    return
                self.marks[g.name] = events[hit][0]; self.i += 1; self.seen = hit + 1
                continue
            if g.ball is not None and g.ball >= len(balls):
                return
            pts = getattr(g, "points", [(g.x, g.y)])
            # how far a ball is from passing: in the board's plane, and in height below or above its bounds
            d = 1e9
            for i, b in enumerate(balls):
                if (g.ball is not None and i != g.ball) or b.out is not None:
                    continue
                x, y, z = b.p.x * 10.0, -b.p.y * 10.0, b.p.z * 10.0
                di = max(0.0, min(math.hypot(x - px, y - py) for px, py in pts) - g.reach)
                di += max(0.0, (g.zmin or 0.0) - z) + max(0.0, z - (g.zmax if g.zmax is not None else 1e9))
                d = min(d, di)
            if d > 0:
                self.best = min(self.best, d)
                return
            self.marks[g.name] = t; self.i += 1; self.best = 1e9

    def cost(self):
        left = len(self.goal) - self.i
        return 0.0 if left == 0 else 100.0 * left + min(self.best, 99.0)


def _approach_side(machine, ball, t):
    """The flipper a ball rolling down the board comes within APPROACH of while the flipper rests, or None."""
    if ball.p.z > 1.5 or ball.v.y > -0.5:
        return None
    p2 = ball.p.to_2d()
    for side, fl in machine.flippers.items():
        if fl.presses and t < fl.presses[-1][1] + 0.3:
            continue
        if (p2 - fl.pivot).length > fl.length + ph.R + APPROACH:
            continue
        q, inside = fl.nearest(p2, 0.0)
        if (p2 - q).length < ph.R + APPROACH:
            return side
    return None


def run(machine, seg, state, q, goal, record=0.002):
    """Runs one segment from state with parameters q toward goal, behind a cut from the state cut() makes. Returns
    its cost, 0 once the goal is met and the segment has reached its end, the state at its end, the samples, the
    events and the gates' times."""
    st = cut(state, q) if seg.get("cut") else state.copy()
    policy = seg["plan"](st, q)
    for side, fl in machine.flippers.items():
        fl.presses = st.presses[side]; fl.auto = None
    events, tracker = [], Tracker(goal)
    still = [None] * len(st.balls)
    stuck = [None] * len(st.balls)
    out = {"end": None, "held": None, "stall": None}
    t_end = st.t + seg["t_max"]

    def stop(t, balls):
        for when, auto in policy:
            started = any(b.out is not None for b in balls) if when == "drained" else t >= when
            if started:
                for fl in machine.flippers.values():
                    fl.auto = auto
        tracker.update(t, balls, events)
        for i, b in enumerate(balls):
            if b.out is not None:
                still[i] = stuck[i] = None; continue
            still[i] = (still[i] if still[i] is not None else t) if b.v.length < STILL else None
            stuck[i] = (stuck[i] if stuck[i] is not None else t) if b.v.length < STUCK else None
        live = [i for i, b in enumerate(balls) if b.out is None]
        done = tracker.i == len(goal)
        if not live:
            if seg["end"] == "drain" and done:
                out["end"] = t
            return True
        if done and seg["end"] == "after" and t >= max(tracker.marks.values(), default=st.t) + seg["after"]:
            out["end"] = t; return True
        if done and len(live) == 1 and seg["end"] == "approach" and t > st.t + 0.3:
            side = _approach_side(machine, balls[live[0]], t)
            if side:
                out["end"] = t; out["held"] = side; return True
        for i in live:
            b = balls[i]
            if still[i] is None or t - still[i] < 0.25:
                continue
            x, y, z = b.p.x * 10.0, -b.p.y * 10.0, b.p.z * 10.0
            in_scoop = math.hypot(x - rn.SCOOP[0], y - rn.SCOOP[1]) < 3.0 and z < 0.5
            at_plunger = math.hypot(x - rn.PLUNGER[0], y - rn.PLUNGER[1]) < 3.0 and b.kicks
            if done and len(live) == 1 and seg["end"] == "scoop" and in_scoop:
                out["end"] = t; return True
            if not (in_scoop or at_plunger) and stuck[i] is not None and t - stuck[i] > STALL:
                out["stall"] = (round(x, 1), round(y, 1)); return True
        return False

    samples, _ = ph.simulate(machine, st.balls, st.t, t_end, stop=stop, record=record, events=events)
    cost = tracker.cost()
    if any(e[1] == "drained" and not 0.0 < e[3].x * 10.0 < 232.0 for e in events):
        cost += 1000.0
    if out["stall"]:
        cost += 1000.0
    elif out["end"] is None:
        cost += 40.0
    t1 = out["end"] if out["end"] is not None else t_end
    for side, fl in machine.flippers.items():
        st.presses[side] = list(fl.presses)
    st.t, st.held = t1, out["held"]
    return cost, st, samples, events, tracker.marks, out


def prefer(seg, events):
    """Among solutions, the one with more of what the segment likes to show: bumper kicks, or flipper hits."""
    kind = seg.get("prefer")
    if kind == "bumper":
        return -sum(1 for e in events if e[1] == "bumper")
    if kind == "flips":
        return -sum(1 for e in events if e[1] == "flipper")
    return 0


def search(machine, seg, state, tries=70, steps=50, seed=1, keep=3):
    """Yields after each run; finally yields ("done", solutions): up to keep solutions with cost 0, best first, as
    (q, goal index). A goal is tried only when the ones before it find nothing."""
    rnd = random.Random(seed)
    ranges = seg["params"]
    found = []
    for gi, goal in enumerate(seg["goals"]):
        best = None
        for k in range(tries + steps):
            if k < tries or best is None:
                q = {n: rnd.uniform(lo, hi) for n, (lo, hi) in ranges.items()}
            else:
                scale = max(0.01, 0.25 * 0.95 ** (k - tries))
                q = {n: min(hi, max(lo, best[1][n] + rnd.gauss(0, scale * (hi - lo)))) for n, (lo, hi) in ranges.items()}
            c, st, smp, ev, marks, out = run(machine, seg, state, q, goal, record=0.02)
            yield ("run", seg["name"], gi, k, round(c, 1))
            if c == 0.0:
                if not any(st.held == f[2] and abs(prefer(seg, ev) - f[3]) < 1 for f in found):
                    found.append((q, gi, st.held, prefer(seg, ev)))
                if len(found) >= keep and k >= tries // 2:
                    break
            if best is None or c < best[0]:
                best = (c, q)
        if found:
            break
    found.sort(key=lambda f: f[3])
    yield ("done", [(f[0], f[1]) for f in found])


def play(machine, solution):
    """Runs the whole game from its start with the parameters found: [(segment name, q, goal index)]. Returns
    the samples of each ball over the whole game, the events with the ball's index, each flipper's presses,
    the segments [(name, t0, t1, marks, cost, the flipper it starts on, the one it ends on, q, whether a cut
    comes before it)], the kicks of the plunger and the scoop, and the time ball B enters."""
    st = start()
    by_name = {s["name"]: s for s in GAME}
    samples, events, segments = [[], []], [], []
    for name, q, gi in solution:
        seg = by_name[name]
        t0 = st.t
        c, st2, smp, ev, marks, out = run(machine, seg, st, q, seg["goals"][gi])
        for i, s in enumerate(smp):
            samples[i] += s if not samples[i] or seg.get("cut") else s[1:]
        events += ev
        start_side = cut(st, q).held if seg.get("cut") else st.held
        segments.append((name, t0, st2.t, marks, c, start_side, st2.held, q, bool(seg.get("cut"))))
        st = st2
    return {"samples": samples, "events": events, "presses": st.presses, "segments": segments, "end": st}
