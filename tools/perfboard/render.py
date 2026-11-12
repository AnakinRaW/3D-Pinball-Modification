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
MARGIN_L, MARGIN_R = 250, 250


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


def plate(x, y, lines, figs=None, size=10.5):
    """A label on a light plate, so it reads over whatever lies beneath."""
    w = max(len(t.replace("&#937;", "O").replace("&#181;", "u")) for t in lines) * size * 0.58 + 8
    h = len(lines) * size * 1.25 + 4
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
    a(d.text(30, V.y0 - 0.9 * P - 92, title, size=16, anchor="start"))
    if subtitle:
        a(d.text(30, V.y0 - 0.9 * P - 72, subtitle, size=13, weight="400", fill="#666",
                 anchor="start"))
    x0 = V.x0 - (V.pad_cols + 0.8) * P
    x1 = V.x0 + (V.ncol - 1 + V.pad_cols + 0.8) * P
    y0, y1 = V.y0 - 0.8 * P, V.y0 + (B.rows - 1 + 0.8) * P
    a(f'<rect x="{x0:.1f}" y="{y0:.1f}" width="{x1 - x0:.1f}" height="{y1 - y0:.1f}" rx="6" '
      'fill="#f8f4ee" stroke="#8a6d0b" stroke-width="2"/>')
    for r in range(B.rows):
        y = V.px((r, B.c0))[1]
        a(d.text(x0 - 12, y, row_name(r), size=12.5, fill="#777", anchor="end"))
        a(d.text(x1 + 12, y, row_name(r), size=12.5, fill="#777", anchor="start"))
    for c in range(B.c0, B.c1 + 1):
        x, _ = V.px((0, c))
        a(d.text(x, y0 - 10, str(c + 1), size=10.5, weight="400", fill="#777"))
        a(d.text(x, y1 + 12, str(c + 1), size=10.5, weight="400", fill="#777"))
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
        for (x, y), name in zip(pts, p.opt["names"]):
            o.append(d.text(x, y + 1.7 * S, name, size=8.5, weight="400", fill="#ffffff"))


def label_part(o, V, p):
    if p.kind in ("pads", "module", "header", "socket", "header_90", "terminal"):
        return
    L = V.L
    r = footprint(L, p)
    if not r:
        return
    cx, cy = V.mm_to_px((r[0] + r[2]) / 2, (r[1] + r[3]) / 2)
    lines, figs = [p.ref], {}
    if p.opt.get("label"):
        lines = [f"{p.ref} {p.opt['label']}"]
        if p.opt.get("fig"):
            figs[0] = p.opt["fig"]
    elif p.kind == "axial" and p.key == "resistor":
        lines = [f"{p.ref} {ohms_text(p.opt['value'])}"]
        if p.opt.get("fig"):
            figs[0] = p.opt["fig"]
    o += plate(cx, cy, lines, figs, size=9.5)


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
        shape = [s for s in shape if not s.lstrip().startswith("<text")]
        o.append('<g opacity="0.22" data-svgcheck="a part on the other side, under the copper">')
        o += shape
        o.append("</g>")
        ax, ay = V.mm_to_px(r[0], r[1])
        bx, by = V.mm_to_px(r[2], r[3])
        o.append(f'<rect x="{min(ax, bx):.1f}" y="{min(ay, by):.1f}" width="{abs(bx - ax):.1f}" '
                 f'height="{abs(by - ay):.1f}" rx="3" fill="#8a6d0b" fill-opacity="0.10" '
                 'stroke="#8a6d0b" stroke-opacity="0.55" stroke-width="1.2" stroke-dasharray="4 3" '
                 'data-svgcheck="the outline of a part on the other side, under the copper"/>')


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


def copper(o, V, side_filter):
    L = V.L
    nets = net_of(L)
    group_net = _hole_nets(L)
    for a, b, side in L.bares:
        if side != side_filter:
            continue
        col = color(L, group_net.get(a))
        (xa, ya), (xb, yb) = V.px(a), V.px(b)
        o.append(f'<line x1="{xa:.1f}" y1="{ya:.1f}" x2="{xb:.1f}" y2="{yb:.1f}" stroke="{col[0]}" '
                 f'stroke-opacity="0.45" stroke-width="{1.5 * S:.1f}" stroke-linecap="round"/>')
        o += d.bare((xa, ya), (xb, yb), S)
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
            n = nets.get(f"{p.ref}.{pin}")
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
    """Solder joints: every lead hole and every hole a wire or a link ends in, ringed in the
    color of its net. The bottom shows them all; the top shows those of its own wires."""
    L = V.L
    hn = _hole_nets(L)
    ends = [(pts[0], s) for pts, s in L.wires] + [(pts[-1], s) for pts, s in L.wires]
    ends += [(a, s) for a, b, s in L.links] + [(b, s) for a, b, s in L.links]
    holes = [h for p in L.parts for h in p.holes] if V.bottom else []
    holes += [h for h, s in ends if V.bottom or s == "top"]
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
        for h, pin in zip(p.holes, p.pins):
            r, c = h
            for er, ec in order:
                nb = (r + er, c + ec)
                if nb in leads or (er == 0 or ec == 0) and frozenset((h, nb)) in used:
                    continue
                if er and ec and (frozenset(((r + er, c), (r, c + ec))) in used
                                  or frozenset((h, (r + er, c))) in used
                                  or frozenset((h, (r, c + ec))) in used):
                    continue
                (xa, ya), (xb, yb) = V.px(h), V.px(nb)
                x, y = (xa + xb) / 2, (ya + yb) / 2
                if (round(x), round(y)) in taken:
                    continue
                taken.add((round(x), round(y)))
                o.append(f'<text x="{x:.1f}" y="{y + 3.3:.1f}" text-anchor="middle" '
                         f'font-size="9.5" font-weight="700" fill="#1a1a1a" stroke="#ffffff" '
                         f'stroke-width="2.6" paint-order="stroke">{PIN_TEXT.get(pin, pin)}</text>')
                break


def connector_labels(o, V, frame):
    L = V.L
    x0, y0, x1, y1 = frame
    nets = net_of(L)
    for p in L.parts:
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
            _connector_label(o, V, frame, p, [V.px(h) for h, _ in g], [pin for _, pin in g], nets)


def _connector_label(o, V, frame, p, pts, pins, nets):
    L = V.L
    x0, y0, x1, y1 = frame
    cx = sum(x for x, _ in pts) / len(pts)
    cy = sum(y for _, y in pts) / len(pts)
    dist = {"left": cx - x0, "right": x1 - cx, "top": cy - y0, "bottom": y1 - cy}
    if len({round(y) for _, y in pts}) < len(pts):
        # pins side by side in a row: their names stand in one line above or below
        dist = {k: v for k, v in dist.items() if k in ("top", "bottom")}
    side = min(dist, key=dist.get)
    title = p.opt.get("title", p.ref)
    sub = p.opt.get("sub", "")
    if side in ("left", "right"):
        tx = x0 - 34 if side == "left" else x1 + 34
        anc = "end" if side == "left" else "start"
        ys = [y for _, y in pts]
        o.append(d.text(tx, min(ys) - 34, title, size=12.5, anchor=anc, fill="#333333"))
        if sub:
            o.append(d.text(tx, min(ys) - 19, sub, size=10.5, weight="400", fill="#777",
                            anchor=anc))
        for (x, y), pin in zip(pts, pins):
            net = nets.get(f"{p.ref}.{pin}")
            name = L.net_labels.get(net, net or "")
            t = f"{name}, pin {pin}" if side == "left" else f"pin {pin}, {name}"
            o.append(d.text(tx, y, t, size=11, anchor=anc, fill=color(L, net)[2]))
    else:
        ty = y0 - 30 if side == "top" else y1 + 34
        o.append(d.text(cx, ty, f"{title}  " + "  ".join(
            f"{pin} {L.net_labels.get(nets.get(f'{p.ref}.{pin}'), nets.get(f'{p.ref}.{pin}') or '')}"
            for pin in pins), size=11, fill="#333333"))


def tables(o, L, y):
    a = o.append
    nm = L.board.name
    a(d.text(40, y, "Parts", size=13, anchor="start"))
    rows = [("Ref", "Part", "Holes", None)]
    for p in L.parts:
        what = p.opt.get("part") or NAMES.get(p.key, p.key)
        if p.key == "resistor":
            what = f"{ohms_text(p.opt['value'])} resistor" + (", standing" if p.opt.get("standing") else "")
        rows.append((p.ref, what, ", ".join(f"{nm(h)} ({pin})" if len(p.holes) > 2 else nm(h)
                                            for h, pin in zip(p.holes, p.pins))[:70],
                     p.opt.get("fig")))
    for ref, how in getattr(L, "offboard_parts", {}).items():
        rows.append((ref, "off the board", how[:70], None))
    yy = y + 22
    for i, r in enumerate(rows):
        st = dict(size=11.5, weight="700" if i == 0 else "400", fill="#1a1a1a" if i == 0 else "#444",
                  anchor="start")
        a(d.text(40, yy, r[0], **st))
        a(d.text(170, yy, r[1], fig=r[3], **st))
        a(d.text(380, yy, r[2], **st))
        yy += 19
    end1 = yy
    a(d.text(900, y, "Copper", size=13, anchor="start"))
    yy = y + 22
    hn = _hole_nets(L)
    by_net = {}
    for a_, b_, side in L.bares:
        by_net.setdefault(hn.get(a_), []).append(f"bare {nm(a_)}-{nm(b_)}" + (" top" if side == "top" else ""))
    for a_, b_, side in L.tins:
        by_net.setdefault(hn.get(a_), []).append(f"tin {nm(a_)}-{nm(b_)}")
    for a_, b_, side in L.links:
        by_net.setdefault(hn.get(a_), []).append(f"link {nm(a_)}-{nm(b_)}")
    for pts, side in L.wires:
        by_net.setdefault(hn.get(pts[0]), []).append(
            f"insulated {nm(pts[0])}-{nm(pts[-1])}" + (f" via {', '.join(nm(v) for v in pts[1:-1])}"
                                                       if len(pts) > 2 else ""))
    for net in sorted(by_net, key=lambda n: (n is None, n or "")):
        items = by_net[net]
        label = L.net_labels.get(net, net or "?")
        line, first = "", True
        for it in items:
            if len(line) + len(it) > 62:
                a(d.text(900, yy, label if first else "", size=11.5, weight="700", anchor="start",
                         fill=color(L, net)[2]))
                a(d.text(1000, yy, line.rstrip(", "), size=11.5, weight="400", anchor="start",
                         fill="#444"))
                yy += 19
                first, line = False, ""
            line += it + ", "
        a(d.text(900, yy, label if first else "", size=11.5, weight="700", anchor="start",
                 fill=color(L, net)[2]))
        a(d.text(1000, yy, line.rstrip(", "), size=11.5, weight="400", anchor="start", fill="#444"))
        yy += 19
    if L.limits:
        yy += 12
        a(d.text(900, yy, "Placement limits", size=13, anchor="start"))
        yy += 20
        for a_, b_, mm, fig, why in L.limits:
            t = f"{a_} to {b_}: under {mm:g} mm" + (f", {why}" if why else "")
            a(d.text(900, yy, t, size=11.5, weight="400", anchor="start", fill="#0a8a6a", fig=fig))
            yy += 19
    return max(end1, yy)


def render(L, path, width=1700, mirror=False):
    B = L.board
    o = []
    board_h = (B.rows - 1) * P
    top = View(L, 200, False)
    o_top = []
    frame = board_frame(o_top, top, "Top side", "")
    ghost_copper(o_top, top)
    copper(o_top, top, "top")
    for p in L.parts:
        draw_part(o_top, top, p, None)
    joints(o_top, top)
    for p in L.parts:
        label_part(o_top, top, p)
    pin_labels(o_top, top)
    connector_labels(o_top, top, frame)
    bottom_y = frame[3] + 260
    bot = View(L, bottom_y, True, flip="cols" if mirror else "rows")
    o_bot = []
    frame_b = board_frame(o_bot, bot, "Bottom side, turned over", "")
    ghost_parts(o_bot, bot)
    copper(o_bot, bot, "bottom")
    joints(o_bot, bot)
    pin_labels(o_bot, bot)
    connector_labels(o_bot, bot, frame_b)
    end = tables(o_bot, L, frame_b[3] + 80)
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
