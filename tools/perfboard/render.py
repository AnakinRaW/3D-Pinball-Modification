"""Draws a layout as one SVG: the top side, the bottom side turned over sideways, and the tables
a builder solders from."""
import math
import re
import draw as d
from check import footprint, module_rect
from inventory import PITCH, PARTS, MODULES, BAND_COLOR
from layout import run_holes, row_name

COLOR = {  # wire color of connectors.md: wire, edge, label
    "black": ("#1a1a1a", None, "#1a1a1a"), "red": ("#c0392b", None, "#c0392b"),
    "yellow": ("#f1c40f", "#9a7d0a", "#8a6d0b"), "blue": ("#1f4fd1", None, "#1f4fd1"),
    "green": ("#3cbf4e", "#23862f", "#1f7a33"), "white": ("#ffffff", "#808080", "#4d4d4d"),
    None: ("#9a9a9a", None, "#6b6b6b"),
}
NAMES = {
    "resistor": "resistor", "diode_do41": "1N5819, DO-41", "ceramic_k15": "100 nF ceramic, Vishay K",
    "ceramic_fg11": "22 &#181;F ceramic, TDK FG11", "electrolytic_esl_c3": "100 &#181;F, KEMET ESL",
    "electrolytic_zlh_8x20": "1000 &#181;F, Rubycon ZLH", "to220": "IRL540N, TO-220",
    "stp160n3ll": "STP160N3LL, TO-220", "header_male": "pin header", "header_female": "socket header",
    "header_male_90": "pin header, 90&#176;", "terminal_5mm": "screw terminal",
    "cd74hct125e": "CD74HCT125E in a socket", "mcp3008_p": "MCP3008 in a socket",
    "mcp23008_p": "MCP23008 in a socket", "am4457f3c": "IR LED AM4457F3C",
    "tekt5400s": "phototransistor TEKT5400S", "qre1113": "QRE1113", "opb606a": "OPB606A",
    "fuse_holder_5x20": "fuse holder 5 x 20", "pads": "edge pads", "teensy41": "Teensy 4.1",
    "d24v5f3": "Pololu D24V5F3", "adafruit757": "Adafruit 757", "adafruit1207": "Adafruit 1207",
    "dfr0954": "DFRobot DFR0954",
}
S = 13.0                 # drawing units per millimeter
P = PITCH * S
MARGIN_L, MARGIN_R = 320, 250


def ohms_text(v):
    if v >= 1e6:
        t, u = v / 1e6, "M"
    elif v >= 1e3:
        t, u = v / 1e3, "k"
    else:
        t, u = v, ""
    s = f"{t:.1f}".rstrip("0").rstrip(".")
    return f"{s} {u}&#937;"


class View:
    def __init__(self, L, y_top, bottom, flip=None):
        """bottom: the view shows the bottom side's copper. flip: "rows" draws the board turned
        over its long edge, column 1 still on the left and row A at the bottom; "cols" turns it
        over its short edge, column 1 on the right. mirror is true for either, since a turned
        board shows every part the other way round."""
        self.L, self.bottom = L, bottom
        self.fx, self.fy = flip == "cols", flip == "rows"
        self.mirror = self.fx or self.fy
        B = L.board
        self.ncol = B.c1 - B.c0 + 1
        pads = 1 if any(B.is_pad((r, B.c0 - 1)) or B.is_pad((r, B.c1 + 1))
                        for r in range(B.rows)) else 0
        self.pad_cols = pads
        self.x0 = MARGIN_L + (pads + 0.9) * P
        self.y0 = y_top + 0.9 * P

    def px(self, h):
        B = self.L.board
        c, r = h[1] - B.c0, h[0]
        if self.fx:
            c = (self.ncol - 1) - c
        if self.fy:
            r = (B.rows - 1) - r
        return self.x0 + c * P, self.y0 + r * P

    def mm_to_px(self, x, y):
        """A point in board millimeters (from the first kept hole) in this view."""
        if self.fx:
            x = (self.ncol - 1) * PITCH - x
        if self.fy:
            y = (self.L.board.rows - 1) * PITCH - y
        return self.x0 + x * S, self.y0 + y * S


def net_of(L):
    m = {}
    for net, pins in L.nets.items():
        for pin in pins:
            m[pin] = net
    return m


def color(L, net):
    return COLOR[L.net_colors.get(net)]


def plate_box(x, y, lines, size=10.5):
    """The box a plate centered on (x, y) takes: x0, y0, x1, y1."""
    w = max(len(t.replace("&#937;", "O").replace("&#181;", "u")) for t in lines) * size * 0.58 + 8
    h = len(lines) * size * 1.25 + 4
    return (x - w / 2, y - h / 2, x + w / 2, y + h / 2)


def plate(x, y, lines, figs=None, size=10.5):
    """A label on a light plate, so it reads over whatever lies beneath."""
    x0, y0, x1, y1 = plate_box(x, y, lines, size)
    w, h = x1 - x0, y1 - y0
    out = [f'<rect x="{x - w / 2:.1f}" y="{y - h / 2:.1f}" width="{w:.1f}" height="{h:.1f}" '
           'rx="3" fill="#ffffff" fill-opacity="0.92" stroke="#6b6b6b" stroke-width="0.8"/>']
    for i, t in enumerate(lines):
        ty = y - h / 2 + 2 + size * 1.25 * (i + 0.5)
        fig = (figs or {}).get(i)
        out.append(d.text(x, ty, t, size=size, fig=fig))
    return out


def board_frame(o, V, title, subtitle):
    L, B = V.L, V.L.board
    a = o.append
    lift = getattr(V, "lift", 0)            # room for the pin names standing above the board
    a(d.text(30, V.y0 - 0.9 * P - 92 - lift, title, size=16, anchor="start"))
    if subtitle:
        a(d.text(30, V.y0 - 0.9 * P - 72 - lift, subtitle, size=13, weight="400", fill="#666",
                 anchor="start"))
    x0 = V.x0 - (V.pad_cols + 0.8) * P
    x1 = V.x0 + (V.ncol - 1 + V.pad_cols + 0.8) * P
    y0, y1 = V.y0 - 0.8 * P, V.y0 + (B.rows - 1 + 0.8) * P
    a(f'<rect x="{x0:.1f}" y="{y0:.1f}" width="{x1 - x0:.1f}" height="{y1 - y0:.1f}" rx="6" '
      'fill="#f8f4ee" stroke="#8a6d0b" stroke-width="2"/>')
    for r in range(B.rows):
        y = V.px((r, B.c0))[1]
        a(d.text(x0 - 12, y, B.row_label(r), size=12.5, fill="#777", anchor="end"))
        a(d.text(x1 + 12, y, B.row_label(r), size=12.5, fill="#777", anchor="start"))
    # the column numbers stand clear of whatever overhangs the board, such as a breakout board
    ty0, ty1 = y0, y1
    for p in L.parts:
        r = module_rect(L, p) if p.kind == "module" else footprint(L, p)
        if r:
            ya, yb = V.mm_to_px(r[0], r[1])[1], V.mm_to_px(r[2], r[3])[1]
            ty0, ty1 = min(ty0, ya, yb), max(ty1, ya, yb)
    V.ty0, V.ty1 = ty0, ty1
    for c in range(B.c0, B.c1 + 1):
        x, _ = V.px((0, c))
        a(d.text(x, ty0 - 10, B.col_label(c), size=10.5, weight="400", fill="#777"))
        a(d.text(x, ty1 + 12, B.col_label(c), size=10.5, weight="400", fill="#777"))
    for r in range(B.rows):
        for c in range(B.c0, B.c1 + 1):
            o += d.hole(*V.px((r, c)), S)
        for c in (B.c0 - 1, B.c1 + 1):
            if B.is_pad((r, c)):
                o += d.edge_pad(*V.px((r, c)), S, along_x=True)
    return x0, y0, x1, y1


def draw_part(o, V, p, nets):
    L = V.L
    pts = [V.px(h) for h in p.holes]
    k, s = p.kind, p.spec
    if k == "axial":
        if p.key == "diode_do41":
            o += d.diode(pts[0], pts[1], S)
        else:
            o += d.resistor(pts[0], pts[1], S, p.opt["value"], standing=p.opt.get("standing"))
    elif k == "radial":
        o += d.ceramic(pts[0], pts[1], S, p.key)
    elif k == "radial_round":
        o += d.electrolytic(pts[0], pts[1], S, p.key)
    elif k == "fuse_holder":
        o += d.fuse_holder(pts[0], pts[1], S, p.key)
    elif k == "to220":
        tab = p.opt.get("tab", 1) * (-1 if V.mirror else 1)
        o += d.to220(pts, S, tab)
    elif k == "dip":
        if p.opt.get("socket", True):
            r = footprint(L, p)
            ax, ay = V.mm_to_px(r[0], r[1])
            bx, by = V.mm_to_px(r[2], r[3])
            o.append(f'<rect x="{min(ax, bx):.1f}" y="{min(ay, by):.1f}" width="{abs(bx - ax):.1f}" '
                     f'height="{abs(by - ay):.1f}" rx="2" fill="#2a2a2a" stroke="#4d4d4d"/>')
        o += d.dip(pts, S, s["body_l"][1], s["body_w"][1], s["color"])
    elif k in ("header", "socket"):
        o += d.header(pts, S, female=(k == "socket"))
    elif k == "header_90":
        ox, oy = p.opt["out"]
        o += d.header_90(pts, S, (-ox if V.fx else ox, -oy if V.fy else oy))
    elif k == "terminal":
        ex, ey = p.opt["entry"]
        o += d.terminal(pts, S, (-ex if V.fx else ex, -ey if V.fy else ey))
    elif k == "side_led":
        o += d.side_looker(pts[0], pts[1], S, p.key,
                           p.opt.get("facing", 1) * (-1 if V.mirror else 1))
    elif k in ("reflective", "reflective_grid"):
        o += d.reflective(pts, S, p.key)
    elif k == "module":
        r = module_rect(L, p)
        ax, ay = V.mm_to_px(r[0], r[1])
        bx, by = V.mm_to_px(r[2], r[3])
        m = MODULES[p.key]
        o += d.module(min(ax, bx), min(ay, by), max(ax, bx), max(ay, by), m["color"], pts, S)
        mx, my = (ax + bx) / 2, (ay + by) / 2
        for (x, y), name in zip(pts, p.opt["names"]):
            # the name sits beside its pin, across the pin's row and toward the module's middle
            along_x = any(abs(q[1] - y) < 0.5 and 0.5 < abs(q[0] - x) < 1.5 * P for q in pts)
            if along_x:
                dy = 1.7 * S if y < my else -1.7 * S
                def under_part(px_, py_):
                    # within reach of a part soldered onto the module, such as D2 on the Teensy
                    for q in getattr(L, "on_module", []):
                        (ax_, ay_), (bx_, by_) = V.px(q.holes[0]), V.px(q.holes[-1])
                        vx, vy = bx_ - ax_, by_ - ay_
                        t = max(0.0, min(1.0, ((px_ - ax_) * vx + (py_ - ay_) * vy)
                                         / (vx * vx + vy * vy or 1)))
                        if math.hypot(px_ - ax_ - t * vx, py_ - ay_ - t * vy) < 0.6 * P:
                            return True
                    return False
                if under_part(x, y + dy) or any(
                        abs(q[0] - x) < 0.5 and abs(q[1] - (y + dy)) < 0.6 * P for q in pts):
                    # a part soldered onto the pin, or a pin inside the row, takes the inner
                    # side: the name moves outside the module
                    o.append(f'<text x="{x:.1f}" y="{y - dy + 3:.1f}" text-anchor="middle" '
                             f'font-size="8.5" font-weight="700" fill="#1a1a1a" stroke="#ffffff" '
                             f'stroke-width="2.6" paint-order="stroke" data-svgcheck="a module '
                             f'pin\'s name outside the module, over the copper beneath, where a '
                             f'pin inside the row takes the inner side">{name}</text>')
                    continue
                o.append(d.text(x, y + dy, name, size=8.5, weight="400", fill="#ffffff"))
            else:
                o.append(d.text(x + (1.4 * S if x < mx else -1.4 * S), y, name, size=8.5,
                                weight="400", fill="#ffffff", anchor="start" if x < mx else "end"))


def label_part(o, V, p):
    if p.kind in ("pads", "module", "header", "socket", "header_90", "terminal"):
        return
    L = V.L
    r = footprint(L, p)
    if not r:
        return
    cx, cy = V.mm_to_px((r[0] + r[2]) / 2, (r[1] + r[3]) / 2)
    lines, figs = [p.ref], {}
    standing = p.kind == "axial" and p.opt.get("standing")
    if p.opt.get("label"):
        lines = [f"{p.ref} {p.opt['label']}"]
        if p.opt.get("fig"):
            figs[0] = p.opt["fig"]
    elif p.kind == "axial" and p.key == "resistor":
        lines = [f"{p.ref} {ohms_text(p.opt['value'])}"]
        if p.opt.get("fig"):
            figs[0] = p.opt["fig"]
    if standing:
        cx, cy = _beside_body(V, p, lines)
    o += plate(cx, cy, lines, figs, size=9.5)
    # a connector's plate inside the board steers around the parts' plates
    V.part_plates = getattr(V, "part_plates", []) + [plate_box(cx, cy, lines, size=9.5)]


def _beside_body(V, p, lines):
    """Where the plate of a standing resistor goes: beside its body, so the body and its bands
    stay in sight. First on the side away from the bent lead, then across it, then past the
    lead's end, wherever no other body and no plate lies."""
    L = V.L
    (ax, ay), (bx, by) = V.px(p.holes[0]), V.px(p.holes[1])
    n = math.hypot(bx - ax, by - ay) or 1
    ux, uy = (bx - ax) / n, (by - ay) / n
    rad = p.spec["body_d"][1] / 2 * S
    x0, y0, x1, y1 = plate_box(0, 0, lines, size=9.5)
    hw, hh = (x1 - x0) / 2, (y1 - y0) / 2

    def reach(dx, dy):
        # how far the plate's center has to sit from a point for its edge to clear it by 3
        return (hw * abs(dx) + hh * abs(dy)) + 3

    spots = []
    for dx, dy, ox, oy in ((-ux, -uy, ax, ay), (-uy, ux, ax, ay), (uy, -ux, ax, ay),
                           (ux, uy, bx, by)):
        k = reach(dx, dy) + rad
        spots.append((ox + dx * k, oy + dy * k))
    boxes = list(getattr(V, "part_plates", []))
    for q in L.parts:
        if q is p:
            continue
        f = footprint(L, q)
        if f:
            (qx0, qy0), (qx1, qy1) = V.mm_to_px(f[0], f[1]), V.mm_to_px(f[2], f[3])
            boxes.append((min(qx0, qx1), min(qy0, qy1), max(qx0, qx1), max(qy0, qy1)))

    def free(x, y):
        bx0, by0, bx1, by1 = plate_box(x, y, lines, size=9.5)
        return not any(bx0 < q[2] and q[0] < bx1 and by0 < q[3] and q[1] < by1 for q in boxes)

    return next((xy for xy in spots if free(*xy)), spots[0])


def ghost_parts(o, V):
    """The parts on the other side, drawn faintly and without their labels inside a dashed
    outline, so the kind of part shows through to the bottom."""
    L = V.L
    nets = _hole_nets(L)
    for p in L.parts:
        r = footprint(L, p)
        if not r:
            continue
        shape = []
        draw_part(shape, V, p, nets)
        why = ' data-svgcheck="a part on the other side, faint under the copper"'
        shape = [re.sub(r"^(\s*<\w+)", lambda m: m.group(1) + why, s, count=1)
                 for s in shape if not s.lstrip().startswith("<text")]
        o.append('<g opacity="0.22">')
        o += shape
        o.append("</g>")
        ax, ay = V.mm_to_px(r[0], r[1])
        bx, by = V.mm_to_px(r[2], r[3])
        o.append(f'<rect x="{min(ax, bx):.1f}" y="{min(ay, by):.1f}" width="{abs(bx - ax):.1f}" '
                 f'height="{abs(by - ay):.1f}" rx="3" fill="#8a6d0b" fill-opacity="0.10" '
                 'stroke="#8a6d0b" stroke-opacity="0.55" stroke-width="1.2" stroke-dasharray="4 3" '
                 'data-svgcheck="the outline of a part on the other side, under the copper"/>')


def adapter_names(o, V):
    """On the bottom, the pin names of a chip on its adapter, beside each pin toward the
    adapter's middle, so the joints under it can be told apart."""
    L = V.L
    for p in L.parts:
        if p.kind != "module" or not p.opt.get("carries"):
            continue
        r = module_rect(L, p)
        (ax, ay), (bx, by) = V.mm_to_px(r[0], r[1]), V.mm_to_px(r[2], r[3])
        mx = (ax + bx) / 2
        for h, name in zip(p.holes, p.opt["names"]):
            x, y = V.px(h)
            left = x < mx
            o.append(f'<text x="{x + (1.4 * S if left else -1.4 * S):.1f}" y="{y + 3:.1f}" '
                     f'text-anchor="{"start" if left else "end"}" font-size="8.5" '
                     f'font-weight="700" fill="#1a1a1a" stroke="#ffffff" stroke-width="2.6" '
                     f'paint-order="stroke" data-svgcheck="the pin name of a chip on the bottom, over '
                     f'the copper beneath the adapter">{name}</text>')


def ghost_copper(o, V):
    """The bottom side's runs, faint, under the parts of the top view."""
    L = V.L
    hn = _hole_nets(L)
    for a, b, side in L.bares:
        if side != "bottom":
            continue
        col = color(L, hn.get(a))
        (xa, ya), (xb, yb) = V.px(a), V.px(b)
        o.append(f'<line x1="{xa:.1f}" y1="{ya:.1f}" x2="{xb:.1f}" y2="{yb:.1f}" stroke="{col[0]}" '
                 f'stroke-opacity="0.35" stroke-width="{0.6 * S:.1f}" stroke-linecap="round"/>')
    for a, b, side in L.tins:
        if side == "bottom":
            col = color(L, hn.get(a))
            (xa, ya), (xb, yb) = V.px(a), V.px(b)
            o.append(f'<line x1="{xa:.1f}" y1="{ya:.1f}" x2="{xb:.1f}" y2="{yb:.1f}" '
                     f'stroke="{col[0]}" stroke-opacity="0.35" stroke-width="{0.6 * S:.1f}" '
                     'stroke-linecap="round"/>')
    for pts, side in L.wires:
        # the insulated wires, faint and wider than the bare runs
        if side == "bottom":
            col = color(L, hn.get(pts[0]))
            path = " ".join(f"{x:.1f},{y:.1f}" for x, y in (V.px(h) for h in pts))
            o.append(f'<polyline points="{path}" fill="none" stroke="{col[0]}" '
                     f'stroke-opacity="0.3" stroke-width="{1.1 * S:.1f}" stroke-linecap="round" '
                     'stroke-linejoin="round"/>')


def copper(o, V, side_filter):
    L = V.L
    nets = net_of(L)
    group_net = _hole_nets(L)
    for a, b, side in L.bares:
        if side != side_filter:
            continue
        col = color(L, group_net.get(a))
        (xa, ya), (xb, yb) = V.px(a), V.px(b)
        thick = (a, b) in getattr(L, "gauges", {})
        o.append(f'<line x1="{xa:.1f}" y1="{ya:.1f}" x2="{xb:.1f}" y2="{yb:.1f}" stroke="{col[0]}" '
                 f'stroke-opacity="0.45" stroke-width="{(2.2 if thick else 1.5) * S:.1f}" '
                 'stroke-linecap="round"/>')
        o += d.bare((xa, ya), (xb, yb), S, thick=thick)
    for a, b, side in L.tins:
        if side == side_filter:
            o += d.tin(V.px(a), V.px(b), S)
    for a, b, side in L.links:
        if side == side_filter:
            o += d.link(V.px(a), V.px(b), S)
    for pts, side in L.wires:
        if side == side_filter:
            col = color(L, group_net.get(pts[0]))
            o += d.insulated([V.px(h) for h in pts], S, col[0])


def _hole_nets(L):
    """The net each copper hole carries, found from the part pins it is joined to."""
    from check import check  # noqa: F401  (same union as the checker, rebuilt here)
    par = {}

    def f(x):
        par.setdefault(x, x)
        while par[x] != x:
            par[x] = par[par[x]]
            x = par[x]
        return x
    for a, b, _ in L.bares:
        hs = run_holes(a, b) or [a, b]
        for x, y in zip(hs, hs[1:]):
            par[f(x)] = f(y)
    for a, b, _ in L.tins + L.links:
        par[f(a)] = f(b)
    for pts, _ in L.wires:
        par[f(pts[0])] = f(pts[-1])
    nets = net_of(L)
    gnet = {}
    for p in L.parts:
        for h, pin in zip(p.holes, p.pins):
            n = nets.get(f"{p.netref}.{pin}")
            if n:
                gnet[f(h)] = n
    out = {}
    for x in list(par):
        out[x] = gnet.get(f(x))
    for p in L.parts:
        for h in p.holes:
            out.setdefault(h, gnet.get(f(h)))
    return out


def joints(o, V):
    """Solder joints: every lead hole, every hole a wire or a link ends in, and every hole where
    one bare run crosses another, ringed in the color of its net. A run that ends on another
    plainly joins it and needs no ring. The bottom shows them all; the top shows those of its
    own copper."""
    L = V.L
    hn = _hole_nets(L)
    ends = [(pts[0], s) for pts, s in L.wires] + [(pts[-1], s) for pts, s in L.wires]
    ends += [(a, s) for a, b, s in L.links] + [(b, s) for a, b, s in L.links]
    holes = [h for p in L.parts for h in p.holes] if V.bottom else []
    holes += [h for h, s in ends if V.bottom or s == "top"]
    runs = [run_holes(a, b) for a, b, s in L.bares if s == ("bottom" if V.bottom else "top")]
    for i, ra in enumerate(runs):
        for rb in runs[i + 1:]:
            for h in set(ra) & set(rb):
                if h not in (ra[0], ra[-1]) and h not in (rb[0], rb[-1]):
                    holes.append(h)
    for h in dict.fromkeys(holes):
        col = color(L, hn.get(h))
        x, y = V.px(h)
        o.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{0.8 * S:.1f}" fill="#d5d9de" '
                 f'stroke="{col[0] if col[0] != "#ffffff" else "#808080"}" stroke-width="2.2"/>')


PIN_TEXT = {"-": "&#8722;"}


def pin_labels(o, V):
    """The pin names of every part that goes in one way round: G, D and S of a MOSFET, an IC's
    numbers, K and A of a diode, plus and minus of an electrolytic, a sensor's pins. Each name
    sits half a pitch off its hole, on a side where no copper of this view runs and no other lead
    sits, across the part's row of pins where it can."""
    L = V.L
    side = "bottom" if V.bottom else "top"
    leads = {h for p in L.parts for h in p.holes}
    used = set()
    for a, b, s in L.bares + L.tins + L.links:
        if s == side:
            hs = run_holes(a, b)
            used |= {frozenset(x) for x in zip(hs, hs[1:])}
    taken = set()
    for p in L.parts:
        if not (p.kind in ("to220", "dip", "radial_round", "side_led", "reflective",
                           "reflective_grid") or p.key == "diode_do41"):
            continue
        dr, dc = 0, 1
        if len(p.holes) > 1:
            (r0, c0), (r1, c1) = p.holes[0], p.holes[1]
            dr, dc = (r1 > r0) - (r1 < r0), (c1 > c0) - (c1 < c0)
        across = [(dc, dr), (-dc, -dr)]
        if p.kind == "to220":
            # the front first, away from the body that stands behind the pins on the tab's side
            t = p.opt.get("tab", 1)
            across = [(t * dc, -t * dr), (-t * dc, t * dr)]
        order = across + [(1, 1), (1, -1), (-1, 1), (-1, -1), (dr, dc), (-dr, -dc)]
        mr = sum(r for r, _ in p.holes) / len(p.holes)
        mc = sum(c for _, c in p.holes) / len(p.holes)
        for h, pin in zip(p.holes, p.pins):
            r, c = h
            own = order
            if p.kind == "dip":
                # an IC's numbers stand just inside each pin: on the socket on the top, and on
                # the bottom inside or else outside, wherever no copper runs
                own = sorted(order[:2], key=lambda e: e[0] * (r - mr) + e[1] * (c - mc))
                own = own if V.bottom else own[:1]
            on_socket = p.kind == "dip" and not V.bottom
            pinout = PARTS.get(p.key, {}).get("pinout")
            if on_socket and pinout:
                # on the IC itself: number and function, from the pin toward the middle; along a
                # pin row that runs across the view the text stands upright, so it stays in its
                # pin's own column
                er, ec = own[0]
                (xa, ya), (xb, yb) = V.px(h), V.px((r + er, c + ec))
                name = pinout[int(pin) - 1]
                style = ('font-weight="700" fill="#ffffff" stroke="#2a2a2a" '
                         'stroke-width="2.2" paint-order="stroke"')
                # the number next to the pin, the function after it, as two texts, so the check
                # of the figures reads a name such as 1A on its own
                gap = len(str(pin)) * 8 * 0.62 + 3
                if abs(xb - xa) > abs(yb - ya):
                    sx = 1 if xb > xa else -1
                    x, y = xa + 0.42 * (xb - xa), ya
                    anc = "start" if sx > 0 else "end"
                    for tx_, t, fs in ((x, pin, 8), (x + sx * gap, name, 7)):
                        o.append(f'<text x="{tx_:.1f}" y="{y + 2.8:.1f}" text-anchor="{anc}" '
                                 f'font-size="{fs}" {style}>{t}</text>')
                else:
                    sy = 1 if yb > ya else -1
                    x, y = xa + 2.8, ya + 0.42 * (yb - ya)
                    anc = "end" if sy > 0 else "start"
                    for ty_, t, fs in ((y, pin, 8), (y + sy * gap, name, 7)):
                        o.append(f'<text x="{x:.1f}" y="{ty_:.1f}" text-anchor="{anc}" '
                                 f'transform="rotate(-90 {x:.1f} {ty_:.1f})" font-size="{fs}" '
                                 f'{style}>{t}</text>')
                continue
            spot, sideways = None, False
            for er, ec in own:
                nb = (r + er, c + ec)
                if not on_socket and (nb in leads or (er == 0 or ec == 0)
                                      and frozenset((h, nb)) in used):
                    continue
                if er and ec and (frozenset(((r + er, c), (r, c + ec))) in used
                                  or frozenset((h, (r + er, c))) in used
                                  or frozenset((h, (r, c + ec))) in used):
                    continue
                k = 0.42 if on_socket else 0.5
                (xa, ya), (xb, yb) = V.px(h), V.px(nb)
                xy = (xa + k * (xb - xa), ya + k * (yb - ya))
                if (round(xy[0]), round(xy[1])) not in taken:
                    spot, sideways = xy, ec != 0
                    break
            if spot is None and p.kind == "dip":
                # every side carries copper: the name stands inside, over the copper
                (xa, ya), (xb, yb) = V.px(h), V.px((r + own[0][0], c + own[0][1]))
                spot = ((xa + xb) / 2, (ya + yb) / 2, ' data-svgcheck="a pin name on copper, '
                        'which runs on every side of the pin"')
            if spot is None:
                continue
            x, y, waiver = (*spot, "") if len(spot) == 2 else spot
            if not waiver and p.kind == "radial_round":
                waiver = ' data-svgcheck="a pin name at its capacitor, over the ring drawn on the can or the copper beneath it"'
            taken.add((round(x), round(y)))
            ink, halo = ("#ffffff", "#2a2a2a") if on_socket else ("#1a1a1a", "#ffffff")
            text = PIN_TEXT.get(pin, pin)
            # a name of two or more characters between two holes of one row stands smaller, so
            # it clears both rings
            size = 7.5 if sideways and len(text) > 1 else 9.5
            o.append(f'<text x="{x:.1f}" y="{y + 0.35 * size:.1f}" text-anchor="middle" '
                     f'font-size="{size}" font-weight="700" fill="{ink}" stroke="{halo}" '
                     f'stroke-width="2.6" paint-order="stroke"{waiver}>{text}</text>')


def connector_labels(o, V, frame):
    L = V.L
    x0, y0, x1, y1 = frame
    nets = net_of(L)
    placed = {}          # the label lines already set, by side and row
    rows = L.board.rows
    # a connector at the edge names its pins first; one further in takes what is left
    spans = placed.setdefault("side_spans", [])
    for q in L.parts:
        if q.kind in ("header", "socket", "header_90", "terminal", "pads"):
            pts_ = [V.px(h) for h in q.holes]
            sd = _side(frame, pts_)
            if sd in ("left", "right"):
                ys_ = sorted(y for _, y in pts_)
                spans.append((q, sd, ys_[0], ys_[-1]))

    def edge_rank(q):
        if len({h[0] for h in q.holes}) != 1 or len(q.holes) < 1:
            return -1                    # a connector named at the side keeps its order
        return min(q.holes[0][0], rows - 1 - q.holes[0][0])
    for p in sorted(L.parts, key=edge_rank):
        if p.kind not in ("header", "socket", "header_90", "terminal", "pads", "module"):
            continue
        if p.kind == "module":
            continue
        groups = [list(zip(p.holes, p.pins))]
        if p.kind == "pads":
            # edge pads on both ends: each end is named on its own side
            mid = (x0 + x1) / 2
            groups = [g for g in ([hp for hp in groups[0] if V.px(hp[0])[0] < mid],
                                  [hp for hp in groups[0] if V.px(hp[0])[0] >= mid]) if g]
        for g in groups:
            _connector_label(o, V, frame, p, [V.px(h) for h, _ in g], [pin for _, pin in g], nets,
                             placed)
    # the wide connectors' names first, the narrow ones into what is left, the names that carry
    # their pins last
    for V_, side, cx, xs, title, ty, inline in sorted(placed.get("titles", []),
                                                       key=lambda t: (t[6], -len(t[3]), t[2])):
        up = side == "top"
        half = len(title) * 12 * 0.3 + 8
        boxes = placed.setdefault("boxes", [])

        w = 2 * half
        # the name stands centered under its pins, or flush with their left or right end, and
        # moves outward only where none of the three is free
        spots = [(cx - half, "middle", cx), (min(xs) - 6, "start", min(xs) - 6),
                 (max(xs) + 6 - w, "end", max(xs) + 6)]

        def own(y, left):
            r = y + 9 if up else y - 12
            return [(left, y - 10, left + w, y + 5), (min(xs) - 6, r - 2, max(xs) + 6, r + 2)]

        def free(y, left):
            return not any(a0 < x1_ and x0_ < a1 and b0 < y1_ and y0_ < b1
                           for a0, b0, a1, b1 in own(y, left) for x0_, y0_, x1_, y1_ in boxes)
        while True:
            spot = next((sp for sp in spots if free(ty, sp[0])), None)
            if spot:
                break
            ty += -16 if up else 16
        left, anchor, tx = spot
        boxes += own(ty, left)
        V_.label_y1 = max(getattr(V_, "label_y1", ty), ty)
        rule = ty + 9 if up else ty - 12
        o.append(f'<line x1="{min(xs) - 6:.1f}" y1="{rule:.1f}" x2="{max(xs) + 6:.1f}" '
                 f'y2="{rule:.1f}" stroke="#999999" stroke-width="1"/>')
        o.append(d.text(tx, ty, title, size=12, fill="#333333", anchor=anchor))


def _side(frame, pts):
    """The side of the drawing a connector's names stand on: the nearest edge, above or below for
    pins side by side in a row."""
    x0, y0, x1, y1 = frame
    cx = sum(x for x, _ in pts) / len(pts)
    cy = sum(y for _, y in pts) / len(pts)
    dist = {"left": cx - x0, "right": x1 - cx, "top": cy - y0, "bottom": y1 - cy}
    if len({round(y) for _, y in pts}) < len(pts):
        # pins side by side in a row: their names stand in one line above or below
        dist = {k: v for k, v in dist.items() if k in ("top", "bottom")}
    elif len({round(x) for x, _ in pts}) < len(pts):
        # pins one above the other in a column: each name in its pin's line, left or right
        dist = {k: v for k, v in dist.items() if k in ("left", "right")}
    return min(dist, key=dist.get)


def _connector_label(o, V, frame, p, pts, pins, nets, placed):
    L = V.L
    x0, y0, x1, y1 = frame
    cx = sum(x for x, _ in pts) / len(pts)
    side = _side(frame, pts)
    title = p.opt.get("title", p.ref)
    sub = p.opt.get("sub", "")
    B = L.board
    inner = p.kind != "pads" and all(B.c0 < h[1] < B.c1 for h in p.holes)
    # a header further in than the edge's own two rows stands inside the board
    inner_tb = p.kind != "pads" and all(1 < h[0] < B.rows - 2 for h in p.holes)
    if (side in ("left", "right") and inner) or (side in ("top", "bottom") and inner_tb):
        # a connector inside the board carries its name and its pins on a plate beside it,
        # toward the board's middle, so its lines never share the edge's rows with another's
        lines_ = [title] + [f"{pin} {L.net_labels.get(nets.get(f'{p.netref}.{pin}'), '')}"
                            for pin in pins]
        w = max(len(t) for t in lines_) * 10.5 * 0.58 + 8
        xs_, ys_ = [x for x, _ in pts], [y for _, y in pts]
        py_ = sum(ys_) / len(ys_)
        right_ = max(xs_) + 0.8 * P + w / 2
        left_ = min(xs_) - 0.8 * P - w / 2
        _, h0, _, h1 = plate_box(0, 0, lines_)
        above_ = (sum(xs_) / len(xs_), min(ys_) - 0.8 * P + h0)
        below_ = (sum(xs_) / len(xs_), max(ys_) + 0.8 * P + h1)
        # toward the board's middle first, then the other side, then across the pins
        if side == "left":
            spots = [(right_, py_), (left_, py_), above_, below_]
        elif side == "right":
            spots = [(left_, py_), (right_, py_), above_, below_]
        elif side == "top":
            spots = [below_, above_, (right_, py_), (left_, py_)]
        else:
            spots = [above_, below_, (right_, py_), (left_, py_)]

        if not hasattr(V, "header_boxes"):
            # the connectors' bodies, which a plate never hides
            V.header_boxes = []
            for q in L.parts:
                f = footprint(L, q) if q.kind in ("header", "socket", "header_90",
                                                  "terminal") else None
                if f:
                    (qx0, qy0), (qx1, qy1) = V.mm_to_px(f[0], f[1]), V.mm_to_px(f[2], f[3])
                    V.header_boxes.append((min(qx0, qx1), min(qy0, qy1), max(qx0, qx1),
                                           max(qy0, qy1)))

        def free(spot):
            b = plate_box(spot[0], spot[1], lines_)
            return not any(min(b[2], q[2]) > max(b[0], q[0]) and min(b[3], q[3]) > max(b[1], q[1])
                           for q in getattr(V, "part_plates", []) + V.header_boxes)

        if not hasattr(V, "busy_px"):
            # every hole that carries copper or a lead, in this view's coordinates
            busy = set(_hole_nets(L))
            for a, b, _ in L.bares + L.tins + L.links:
                busy.update(run_holes(a, b) or [a, b])
            for pts_w, _ in L.wires:
                for a, b in zip(pts_w, pts_w[1:]):
                    busy.update(run_holes(a, b) or [a, b])
            busy.update(h for q in L.parts for h in q.holes)
            V.busy_px = [V.px(h) for h in busy]

        def covered(spot):
            # the holes with copper the plate would hide
            b = plate_box(spot[0], spot[1], lines_)
            return sum(b[0] - 0.4 * P < x < b[2] + 0.4 * P and b[1] - 0.4 * P < y < b[3] + 0.4 * P
                       for x, y in V.busy_px)

        def inside(spot):
            b = plate_box(spot[0], spot[1], lines_)
            return x0 <= b[0] and b[2] <= x1 and y0 <= b[1] and b[3] <= y1
        # of the free spots on the board the one that hides the least copper, the order breaking
        # a tie
        ok = ([s for s in spots if free(s) and inside(s)] or [s for s in spots if free(s)]
              or spots[:1])
        px_, py_ = min(ok, key=lambda s: (covered(s), ok.index(s)))
        # the next connector's plate keeps clear of this one too
        V.part_plates = getattr(V, "part_plates", []) + [plate_box(px_, py_, lines_)]
        o.extend(plate(px_, py_, lines_))
        return
    if side in ("left", "right"):
        tx = x0 - 34 if side == "left" else x1 + 34
        anc = "end" if side == "left" else "start"
        ys = sorted(y for _, y in pts)
        together = len(ys) == 1 or all(abs(b - a - P) < 1 for a, b in zip(ys, ys[1:]))
        lines = placed.setdefault("lines", {})
        spans = placed.setdefault("spans", [])
        if together and any(sd == side and lo - 1.2 * P < ys[-1] and ys[0] < hi + 1.2 * P
                            for sd, lo, hi in spans):
            together = False             # another run's name already stands in these rows
        if together and any(sd == side and who is not p and ys[0] - 2 * P < hi and lo < ys[0]
                            for who, sd, lo, hi in placed["side_spans"]):
            together = False             # another run's pins stand where this name would
        spans.append((side, ys[0], ys[-1]))
        if together:
            # pins in one run: the name and its line above the run, the nets at the pins
            o.append(d.text(tx, ys[0] - 34, title, size=12.5, anchor=anc, fill="#333333"))
            if sub:
                o.append(d.text(tx, ys[0] - 19, sub, size=10.5, weight="400", fill="#777",
                                anchor=anc))
        for (x, y), pin in zip(pts, pins):
            net = nets.get(f"{p.netref}.{pin}")
            name = L.net_labels.get(net, net or "")
            t = f"{name}, pin {pin}" if side == "left" else f"pin {pin}, {name}"
            if not together:
                # pins apart: each line names its connector, so none stands without it
                t = f"{title} {t}" if side == "right" else f"{title}: {t}"
            key = (side, round(y))
            if key in lines:
                # another connector's pin on the same row: one line carries both
                k, old = lines[key]
                o[k] = o[k].replace("</text>", f"  &#183;  {t}</text>")
            else:
                lines[key] = (len(o), t)
                o.append(d.text(tx, y, t, size=11, anchor=anc, fill=color(L, net)[2]))
    else:
        # each pin's net stands upright in the pin's own column, pin number first; the
        # connector's name follows right beyond its own pins' names, placed once every
        # connector's pins are drawn
        up = side == "top"
        edge = getattr(V, "ty0", y0) - 22 if up else getattr(V, "ty1", y1) + 24
        taken = placed.setdefault("pin_cols", set())
        boxes = placed.setdefault("boxes", [])
        reach = 0
        if any((side, round(x)) in taken for x, _ in pts):
            # another connector's names already stand in these columns: this one's pins go into
            # its name's line
            title = f"{title}  " + "  ".join(
                f"{pin} {L.net_labels.get(nets.get(f'{p.netref}.{pin}'), '')}" for pin in pins)
        else:
            for (x, _), pin in zip(pts, pins):
                taken.add((side, round(x)))
                net = nets.get(f"{p.netref}.{pin}")
                t = f"{pin}  {L.net_labels.get(net, net or '')}"
                o.append(d.text(x, edge, t, size=11, anchor="start" if up else "end",
                                fill=color(L, net)[2], rotate=-90))
                n = len(t) * 11 * 0.58
                reach = max(reach, n)
                boxes.append((x - 7, edge - n, x + 7, edge) if up else (x - 7, edge, x + 7, edge + n))
        start = edge - reach - 16 if up else edge + reach + 18
        placed.setdefault("titles", []).append((V, side, cx, [x for x, _ in pts], title, start,
                                                not reach))


def tables(o, L, y):
    """Under the drawing only what it cannot show: a run of thicker wire, a part that sits off
    the board, a part left out."""
    a = o.append
    nm = L.board.printed
    hn = _hole_nets(L)
    notes = []
    by_gauge = {}
    for (a_, b_), g in getattr(L, "gauges", {}).items():
        by_gauge.setdefault((g, hn.get(a_)), []).append(f"{nm(a_)} to {nm(b_)}")
    for (g, net), runs in sorted(by_gauge.items(), key=lambda t: (t[0][0], t[0][1] or "")):
        notes.append(f"{g} wire for {L.net_labels.get(net, net or '?')}: {', '.join(runs)}")
    if by_gauge:
        notes.append("Every other wire is 22 AWG solid.")
    for ref, how in getattr(L, "offboard_parts", {}).items():
        notes.append(f"{ref} sits off the board: {how}")
    for ref, (_joins, why) in getattr(L, "unfitted_parts", {}).items():
        notes.append(f"{ref} is not fitted: {why}")
    if not notes:
        return y - 40
    a(d.text(40, y, "Notes", size=13, anchor="start"))
    yy = y + 22
    for t in notes:
        a(d.text(40, yy, t, size=11.5, weight="400", anchor="start", fill="#444"))
        yy += 19
    return yy


def edge_label_room(L):
    """How far the pin names of the connectors along the top and bottom edges reach out."""
    if not any(p.kind in ("header", "socket", "header_90", "terminal") for p in L.parts):
        return 0
    nets = net_of(L)
    names = [L.net_labels.get(n, n) for n in nets.values() if n]
    return int((max(map(len, names), default=4) + 4) * 11 * 0.58) + 40


def render(L, path, width=1700, mirror=False):
    B = L.board
    o = []
    board_h = (B.rows - 1) * P
    lift = edge_label_room(L)
    top = View(L, 200 + lift, False)
    top.lift = lift
    o_top = []
    frame = board_frame(o_top, top, "Top side", "")
    ghost_copper(o_top, top)
    copper(o_top, top, "top")
    for p in L.parts:
        draw_part(o_top, top, p, None)
    for p in getattr(L, "on_module", []):
        # soldered onto a module, above it, and seen through so the pins beneath it still show
        o_top.append('<g opacity="0.7">')
        draw_part(o_top, top, p, None)
        o_top.append('</g>')
        label_part(o_top, top, p)             # its designator on its body, as on the board
    joints(o_top, top)
    # a standing resistor's plate goes beside its body, so it is placed once the others stand
    for p in sorted(L.parts, key=lambda p: bool(p.kind == "axial" and p.opt.get("standing"))):
        label_part(o_top, top, p)
    pin_labels(o_top, top)
    connector_labels(o_top, top, frame)
    bottom_y = max(frame[3], getattr(top, "label_y1", 0)) + 260 + lift
    bot = View(L, bottom_y, True, flip="cols" if mirror else "rows")
    bot.lift = lift
    o_bot = []
    frame_b = board_frame(o_bot, bot, "Bottom side, turned over", "")
    ghost_parts(o_bot, bot)
    copper(o_bot, bot, "bottom")
    joints(o_bot, bot)
    pin_labels(o_bot, bot)
    adapter_names(o_bot, bot)
    connector_labels(o_bot, bot, frame_b)
    end = tables(o_bot, L, max(frame_b[3] + 80, getattr(bot, "label_y1", 0) + 40))
    W = max(width, int(frame[2] + MARGIN_R))
    H = int(end + 30)
    head = [f'<svg viewBox="0 0 {W} {H}" xmlns="http://www.w3.org/2000/svg" '
            'font-family="Segoe UI, Arial, sans-serif">',
            f'<rect x="0" y="0" width="{W}" height="{H}" fill="#ffffff"/>',
            d.text(30, 34, L.title, size=20, anchor="start"),
            d.text(30, 56, L.subtitle or f"Perfboard {B.kind}, holes named as printed on it",
                   size=13, weight="400", fill="#666", anchor="start")]
    body = "\n".join(head + o_top + o_bot + ["</svg>"]) + "\n"
    svg = label_figs(L, body)
    import xml.etree.ElementTree as ET
    ET.fromstring(svg)                   # a drawing no viewer can open is never written
    open(path, "w", encoding="utf-8").write(svg)


def label_figs(L, svg):
    """Gives every text that prints a label of L.figure the key of its figure, so figcheck reads
    it, such as a rail's "5 V" wherever a net or a pin is named."""
    figs = getattr(L, "text_figs", {})

    def one(m):
        attrs, content = m.group(1), m.group(2)
        keys = [k for t, k in figs.items() if t in content]
        if not keys:
            return m.group(0)
        old = re.search(r' data-fig="([^"]*)"', attrs)
        merged = " ".join(dict.fromkeys((old.group(1).split() if old else []) + keys))
        attrs = (attrs.replace(old.group(0), f' data-fig="{merged}"') if old
                 else attrs + f' data-fig="{merged}"')
        return f"<text{attrs}>{content}</text>"
    return re.sub(r"<text([^>]*)>([^<]*)</text>", one, svg) if figs else svg
