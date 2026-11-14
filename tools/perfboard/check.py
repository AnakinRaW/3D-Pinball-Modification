"""Checks a layout against the hard rules of perfboard building and the house style.

`check(layout)` returns (errors, warnings). An error is something that cannot be built or does
not make the schematic's netlist; a warning is a house-style rule the layout leaves where it
could have kept it.
"""
import math
from inventory import PARTS, MODULES, PITCH, INSULATED_MIN, span_range
from layout import run_holes

MARGIN = 0.3            # mm a lead keeps from a foreign body


# ---- footprints ----------------------------------------------------------------------------
def _rect_between(a, b, along, across):
    (x1, y1), (x2, y2) = a, b
    cx, cy = (x1 + x2) / 2, (y1 + y2) / 2
    horiz = abs(x2 - x1) >= abs(y2 - y1)
    w, h = (along, across) if horiz else (across, along)
    return (cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2)


def footprint(L, p):
    """The part's body seen from above, as a rectangle in board millimeters, or None."""
    B = L.board
    pts = [B.xy(h) for h in p.holes]
    xs, ys = [x for x, _ in pts], [y for _, y in pts]
    cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    k = p.kind
    s = p.spec
    if k == "axial":
        if p.opt.get("standing"):
            x, y = pts[0]
            r = s["body_d"][1] / 2
            return (x - r, y - r, x + r, y + r)
        return _rect_between(pts[0], pts[1], s["body_l"][1], s["body_d"][1])
    if k == "radial":
        return _rect_between(pts[0], pts[1], s["body_w"][1], s["body_t"][1])
    if k == "radial_round":
        r = s["body_d"][1] / 2
        return (cx - r, cy - r, cx + r, cy + r)
    if k == "to220":
        e = s["body_e"][1]
        back = s["lead_to_back"][1]
        front = s["body_a"][1] - s["lead_to_back"][0]
        tab = p.opt.get("tab", 1)
        (x1, y1), (x3, y3) = pts[0], pts[2]
        if abs(x3 - x1) >= abs(y3 - y1):          # leads along x
            d = 1 if x3 > x1 else -1
            lo, hi = (back, front) if tab * d > 0 else (front, back)   # tab left of travel = -y
            return (cx - e / 2, cy - lo, cx + e / 2, cy + hi)
        d = 1 if y3 > y1 else -1
        lo, hi = (front, back) if tab * d > 0 else (back, front)
        return (cx - lo, cy - e / 2, cx + hi, cy + e / 2)
    if k == "dip":
        socket = p.opt.get("socket", True)
        n = s["pins"]
        if socket:
            sk = PARTS["dip_socket"]
            length, width = sk["body_l"][n], sk["body_w"][1]
        else:
            length, width = s["body_l"][1], s["body_w"][1]
        horiz = (max(xs) - min(xs)) > (max(ys) - min(ys))
        w, h = (length, width) if horiz else (width, length)
        return (cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2)
    if k in ("header", "socket"):
        return (min(xs) - PITCH / 2, min(ys) - PITCH / 2, max(xs) + PITCH / 2, max(ys) + PITCH / 2)
    if k == "header_90":
        ox, oy = p.opt["out"]
        face, thick = s["row_to_face"][1], s["body_w"][1]
        if ox:
            x0 = (max(xs) if ox > 0 else min(xs)) + ox * (face - thick / 2) - thick / 2
            return (x0, min(ys) - PITCH / 2, x0 + thick, max(ys) + PITCH / 2)
        y0 = (max(ys) if oy > 0 else min(ys)) + oy * (face - thick / 2) - thick / 2
        return (min(xs) - PITCH / 2, y0, max(xs) + PITCH / 2, y0 + thick)
    if k == "terminal":
        n = len(pts)
        length, depth, back = s["pitch"] * n, s["body_depth"][1], s["pins_from_back"]
        ex, ey = p.opt["entry"]
        if max(xs) - min(xs) >= max(ys) - min(ys):
            y0 = cy - back if ey > 0 else cy - (depth - back)
            return (cx - length / 2, y0, cx + length / 2, y0 + depth)
        x0 = cx - back if ex > 0 else cx - (depth - back)
        return (x0, cy - length / 2, x0 + depth, cy + length / 2)
    if k == "side_led":
        return _rect_between(pts[0], pts[1], s["body_w"][1], s["body_t"][1])
    if k in ("reflective", "reflective_grid"):
        a, b = s["body_a"][1], s["body_b"][1]
        return (cx - b / 2, cy - a / 2, cx + b / 2, cy + a / 2)
    if k == "fuse_holder":
        return _rect_between(pts[0], pts[1], s["body_l"][1], s["body_w"][1])
    if k == "module":
        return module_rect(L, p)
    return None


def module_rect(L, p):
    m = MODULES[p.key]
    Lm, Wm = m["size"]
    fx, fy, _ = m["pins"][0]
    x0, y0 = L.board.xy(p.holes[0])
    rot = p.opt.get("rot", 0)
    # board millimeters of the module's corner (0, 0) and (Lm, Wm)
    def place(x, y):
        dx, dy = x - fx, y - fy
        if rot == 0:
            return x0 + dx, y0 + dy
        if rot == 90:
            return x0 - dy, y0 + dx
        if rot == 180:
            return x0 - dx, y0 - dy
        return x0 + dy, y0 - dx
    a, b = place(0, 0), place(Lm, Wm)
    return (min(a[0], b[0]), min(a[1], b[1]), max(a[0], b[0]), max(a[1], b[1]))


def _overlap(r1, r2):
    return min(r1[2], r2[2]) - max(r1[0], r2[0]) > 1e-6 and min(r1[3], r2[3]) - max(r1[1], r2[1]) > 1e-6


def _inside(pt, r, m=0.0):
    return r[0] - m < pt[0] < r[2] + m and r[1] - m < pt[1] < r[3] + m


def _seg_hits_rect(a, b, r, m=0.0):
    """Whether the straight segment a-b passes over the rectangle, sampled every 0.2 mm."""
    n = max(2, int(math.dist(a, b) / 0.2))
    for i in range(n + 1):
        t = i / n
        if _inside((a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t), r, m):
            return True
    return False


# ---- the checks ----------------------------------------------------------------------------
def check(L):
    B = L.board
    E, W = [], []
    nm = B.name
    fps = {p.ref: footprint(L, p) for p in L.parts}

    # holes: valid, one lead per hole
    lead_of = {}
    for p in L.parts:
        for h, pin in zip(p.holes, p.pins):
            if p.kind == "pads":
                if not B.is_pad(h):
                    E.append(f"{p.netref}.{pin}: {nm(h)} is not an edge pad")
            elif not B.on_board(h):
                E.append(f"{p.netref}.{pin}: {nm(h)} is off the board")
            if h in lead_of:
                E.append(f"{nm(h)} holds {lead_of[h]} and {p.netref}.{pin}")
            lead_of[h] = f"{p.netref}.{pin}"

    # each part's own geometry
    for p in L.parts:
        E += _geometry(L, p)

    # bodies
    refs = [p for p in L.parts if fps[p.ref]]
    for i, a in enumerate(refs):
        for b in refs[i + 1:]:
            if {a.kind, b.kind} & {"module"}:
                continue
            if _overlap(fps[a.ref], fps[b.ref]):
                E.append(f"bodies of {a.ref} and {b.ref} overlap")
    for p in L.parts:
        r = fps[p.ref]
        if not r or p.kind == "module":
            continue
        for h, who in lead_of.items():
            if who.split(".")[0] == p.netref and h in p.holes:
                continue
            if _inside(B.xy(h), r, MARGIN):
                E.append(f"{who} in {nm(h)} sits under the body of {p.ref}")
    for p in L.parts:
        if p.kind != "module":
            continue
        r = fps[p.ref]
        for q in L.parts:
            if q is p or q.kind == "module" or not fps[q.ref]:
                continue
            flat = q.kind == "axial" and not q.opt.get("standing")
            if _overlap(r, fps[q.ref]) and not flat:
                W.append(f"S14: {q.ref} stands under the daughter board {p.ref}")

    # copper: ownership and connectivity
    par = {}

    def f(x):
        par.setdefault(x, x)
        while par[x] != x:
            par[x] = par[par[x]]
            x = par[x]
        return x

    def u(a, b):
        par[f(a)] = f(b)

    copper = set(lead_of)
    for a, b, side in L.bares:
        hs = run_holes(a, b)
        if hs is None:
            E.append(f"bare wire {nm(a)}-{nm(b)} is not straight")
            continue
        for h in hs:
            if not B.valid(h):
                E.append(f"bare wire {nm(a)}-{nm(b)} leaves the board at {nm(h)}")
        for x, y in zip(hs, hs[1:]):
            u(x, y)
        copper.update(hs)
        if side == "top":
            for p in L.parts:
                if fps[p.ref] and p.kind != "module" and any(
                        _inside(B.xy(h), fps[p.ref]) for h in hs):
                    E.append(f"bare wire {nm(a)}-{nm(b)} on the top runs under {p.ref}")
    for a, b, side in L.tins:
        if abs(a[0] - b[0]) + abs(a[1] - b[1]) != 1:
            E.append(f"tin bridge {nm(a)}-{nm(b)} joins holes that are not neighbors")
        u(a, b)
        copper.update((a, b))
    for a, b, side in L.links:
        hs = run_holes(a, b)
        if hs is None:
            E.append(f"link {nm(a)}-{nm(b)} is not straight")
            continue
        u(a, b)
        copper.update((a, b))
    run_holes_of = set()
    for a, b, side in L.bares:
        run_holes_of.update(run_holes(a, b) or [])
    wire_ends = {}
    for pts, side in L.wires:
        for end in (pts[0], pts[-1]):
            # the wire's end goes through a hole of its own: no lead in it, no bare run across it,
            # no other wire's end
            name = f"insulated wire {nm(pts[0])}-{nm(pts[-1])}"
            if end in lead_of:
                E.append(f"{name} ends in {nm(end)}, which holds {lead_of[end]}")
            elif end in run_holes_of and not B.is_pad(end):
                E.append(f"{name} ends in {nm(end)}, which a bare run crosses; it needs a hole "
                         f"of its own, tinned to the run")
            if end in wire_ends:
                E.append(f"{name} ends in {nm(end)}, where {wire_ends[end]} ends too")
            wire_ends[end] = name
    for pts, side in L.wires:
        u(pts[0], pts[-1])
        copper.update((pts[0], pts[-1]))
        length = sum(math.dist(B.xy(x), B.xy(y)) for x, y in zip(pts, pts[1:]))
        if length < INSULATED_MIN - 1e-6:
            if tuple(pts) in getattr(L, "short_wires", set()):
                W.append(f"H9: insulated wire {nm(pts[0])}-{nm(pts[-1])} is {length:.1f} mm, "
                         f"under {INSULATED_MIN:.0f} mm, accepted as short")
            else:
                E.append(f"insulated wire {nm(pts[0])}-{nm(pts[-1])} is {length:.1f} mm, under "
                         f"the {INSULATED_MIN:.0f} mm it needs")
        for x, y in zip(pts, pts[1:]):
            for p in L.parts:
                r = fps[p.ref]
                if r and side == "top" and p.kind != "module" and _seg_hits_rect(B.xy(x), B.xy(y), r, -0.2):
                    E.append(f"insulated wire {nm(pts[0])}-{nm(pts[-1])} runs over {p.ref}")
                    break

    # a module joins its pins of one name on its own board, such as the Teensy's GND pins
    expected_pins = {pin for pins in L.effective_nets().values() for pin in pins}
    for p in L.parts:
        if p.kind != "module":
            continue
        same = {}
        for h, pin in zip(p.holes, p.pins):
            if f"{p.netref}.{pin}" in expected_pins:
                same.setdefault(pin.split("#")[0], []).append(h)
        for hs in same.values():
            for x in hs[1:]:
                u(hs[0], x)

    # what each copper group carries
    groups = {}
    for h, who in lead_of.items():
        groups.setdefault(f(h), set()).add(who)
    nets_made = L.effective_nets()
    expected = {pin: net for net, pins in nets_made.items() for pin in pins}
    net_of_group = {}
    for g, pins in groups.items():
        nets = {expected.get(x, "?" + x) for x in pins}
        if len(nets) > 1:
            E.append("short: one piece of copper joins " + ", ".join(sorted(nets)))
        net_of_group[g] = sorted(nets)[0]
    placed = {p.netref for p in L.parts}
    for net, pins in nets_made.items():
        gs = {f(L.pin_hole(x)) for x in pins if x.split(".", 1)[0] in placed}
        if len(gs) > 1:
            E.append(f"open: {net} falls apart into {len(gs)} pieces")
    for p in L.parts:
        for pin in p.pins:
            if (p.kind != "pads" and f"{p.netref}.{pin}" not in expected
                    and f"{p.netref}.{pin}" not in L.free):
                E.append(f"{p.netref}.{pin} is on no net of the netlist")
    E += L.against_netlist()

    # links touch nothing in between
    for a, b, side in L.links:
        hs = run_holes(a, b)
        if hs:
            na = net_of_group.get(f(a))
            for h in hs[1:-1]:
                if h in copper and net_of_group.get(f(h)) != na:
                    E.append(f"link {nm(a)}-{nm(b)} passes {nm(h)} of another net")

    # connectors at the edge, cables leaving off the board
    for p in L.parts:
        if p.kind == "terminal":
            E += _edge_connector(L, p, p.opt["entry"], "S13")
        elif p.kind == "header_90":
            E += _edge_connector(L, p, p.opt["out"], "H19")
        elif p.kind in ("header", "socket") and p.opt.get("cable", True):
            if not any(_at_edge(B, h) for h in p.holes):
                W.append(f"S12: {p.ref} sits inside the board, not at an edge")

    # limits
    for a, b, mm, fig, why in L.limits:
        d = math.dist(B.xy(L.pin_hole(a)), B.xy(L.pin_hole(b)))
        if d > mm + 1e-6:
            E.append(f"{a} is {d:.1f} mm from {b}, over the {mm} mm allowed")

    # house style
    owner = {}
    for h in copper:
        owner[h] = net_of_group.get(f(h))
    part_of = {h: who.split(".")[0] for h, who in lead_of.items()}
    for h, n in owner.items():
        for v in ((h[0] + 1, h[1]), (h[0], h[1] + 1)):
            m = owner.get(v)
            if m and n and m != n and not (part_of.get(h) and part_of.get(h) == part_of.get(v)):
                W.append(f"S1: {nm(h)} ({n}) and {nm(v)} ({m}) are neighbors")
    rails = ({n.name for n in L.declared.nets.values() if n.rail} if L.declared else set())
    for side in ("bottom", "top"):
        degree = {}
        for a, b, s in L.bares + L.tins + L.links:
            if s != side:
                continue
            hs = run_holes(a, b)
            for i, x in enumerate(hs):
                degree[x] = degree.get(x, 0) + (1 if i in (0, len(hs) - 1) else 2)
        for x, k in degree.items():
            if k >= 3 and owner.get(x) not in rails:
                W.append(f"S24: {owner.get(x)} branches at {nm(x)} on the {side}")
    # S27: straight stretches of different nets that lie along one another in a row or column
    segs = []
    for a_, b_, side in L.bares:
        segs.append((a_, b_, side, owner.get(a_), "bare"))
    for pts, side in L.wires:
        for x, y in zip(pts, pts[1:]):
            segs.append((x, y, side, owner.get(pts[0]), "wire"))
    for i, (a1, b1, s1, n1, k1) in enumerate(segs):
        for a2, b2, s2, n2, k2 in segs[i + 1:]:
            if s1 != s2 or n1 == n2 or "bare" == k1 == k2:
                continue
            for ax in (0, 1):                       # the index that varies along the stretch
                o = 1 - ax
                if a1[o] == b1[o] == a2[o] == b2[o] and a1[ax] != b1[ax] and a2[ax] != b2[ax]:
                    lo = max(min(a1[ax], b1[ax]), min(a2[ax], b2[ax]))
                    hi = min(max(a1[ax], b1[ax]), max(a2[ax], b2[ax]))
                    if hi > lo:
                        start = (lo, a1[1]) if ax == 0 else (a1[0], lo)
                        W.append(f"S27: a {k1} of {n1} and a {k2} of {n2} lie along each other "
                                 f"from {nm(start)} on")
    for kind, what in (("to220", "tab"),):
        sides = {p.opt.get(what) for p in L.parts if p.kind == kind}
        if len(sides) > 1:
            W.append(f"S5: the {kind} parts face different ways")
    for p in L.parts:
        if p.kind == "dip" and not p.opt.get("socket", True):
            W.append(f"S20: {p.ref} sits without a socket")
    corners = [(0, B.c0), (0, B.c1), (B.rows - 1, B.c0), (B.rows - 1, B.c1)]
    for h in corners:
        if h in copper:
            W.append(f"S17: corner hole {nm(h)} is used")
    return E, W


def _at_edge(B, h):
    r, c = h
    return r in (0, B.rows - 1) or c in (B.c0, B.c1)


def _edge_connector(L, p, out, rule):
    """A connector whose cable leaves off the board: its pins in a row along an edge, the
    cable direction pointing out across that edge."""
    B = L.board
    rows = {h[0] for h in p.holes}
    cols = {h[1] for h in p.holes}
    ox, oy = out
    errs = []
    if len(rows) == 1:                      # pins along a row: cable must leave up or down
        r = rows.pop()
        ok = (oy < 0 and r <= 1) or (oy > 0 and r >= B.rows - 2)
        if ox or not ok:
            errs.append(f"{rule}: {p.ref} has to sit at the top or bottom edge with its cable "
                        f"leaving across it")
    elif len(cols) == 1:
        c = cols.pop()
        ok = (ox < 0 and c <= B.c0 + 1) or (ox > 0 and c >= B.c1 - 1)
        if oy or not ok:
            errs.append(f"{rule}: {p.ref} has to sit at the left or right edge with its cable "
                        f"leaving across it")
    return errs


def _geometry(L, p):
    B = L.board
    nm = B.name
    k, s, hs = p.kind, p.spec, p.holes
    errs = []

    def span(a, b):
        line = run_holes(a, b)
        return None if line is None else len(line) - 1

    if k == "axial":
        n = span(*hs)
        lo, hi = span_range(p.key, standing=bool(p.opt.get("standing")))
        if n is None or not (lo <= n <= hi):
            how = "standing" if p.opt.get("standing") else "lying"
            errs.append(f"{p.ref}: {how}, its leads {nm(hs[0])}-{nm(hs[1])} have to be "
                        f"{lo} to {hi} holes apart in one row or column")
    elif k in ("radial", "radial_round", "side_led"):
        n = span(*hs)
        if n is None or not (1 <= n <= 2):
            errs.append(f"{p.ref}: leads {nm(hs[0])}-{nm(hs[1])} have to be 1 or 2 holes apart")
    elif k in ("to220", "header", "socket", "header_90"):
        line = run_holes(min(hs), max(hs))
        if line is None or sorted(line) != sorted(hs):
            errs.append(f"{p.ref}: its pins have to sit in neighboring holes of one row")
    elif k == "terminal":
        line = run_holes(hs[0], hs[-1])
        if line is None or len(line) != 2 * len(hs) - 1 or any(h not in line for h in hs):
            errs.append(f"{p.ref}: its pins have to sit every second hole of one row")
    elif k == "reflective":
        fan = s.get("fanout", (2, 2))
        rs = [h[0] for h in hs]
        cs = [h[1] for h in hs]
        if max(rs) - min(rs) >= max(fan) + 1 or max(cs) - min(cs) >= max(fan) + 1:
            errs.append(f"{p.ref}: its pins spread wider than the legs reach")
    return errs


def report(L, full=False):
    """The report as text: errors first, then the house-style notes, the S1 neighbors gathered
    into one line per pair of nets unless `full`."""
    E, W = check(L)
    lines = [f"  error    {e}" for e in E]
    W = sorted(set(W))
    pairs, rest = {}, []
    for w in W:
        if w.startswith("S1: ") and not full:
            words = w[4:].split(" ")          # D9 (SH3) and E9 (FOIL) are neighbors
            key = tuple(sorted((words[1].strip("()"), words[4].strip("()"))))
            pairs.setdefault(key, []).append(f"{words[0]}/{words[3]}")
        else:
            rest.append(w)
    for (a, b), where in sorted(pairs.items(), key=lambda t: -len(t[1])):
        more = f" and {len(where) - 3} more" if len(where) > 3 else ""
        lines.append(f"  style    S1: {a} beside {b} in {len(where)} places, "
                     f"{', '.join(where[:3])}{more}")
    lines += [f"  style    {w}" for w in rest]
    lines.append(f"{len(E)} errors, {len(W)} house-style notes")
    return E, W, "\n".join(lines)
