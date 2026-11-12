"""SVG drawing of perfboard parts as they look from above, at the size the inventory gives.

Every function takes hole centers in drawing units and returns SVG elements as strings. `S` is the
scale, drawing units per millimeter. Bodies use the inventory's maximum sizes, so what fits on the
drawing fits on the board.
"""
import math
from inventory import PARTS, bands, BAND_COLOR, PITCH

SILVER, SILVER_EDGE = "#c9ccd1", "#8a8f96"
TIN, TIN_EDGE = "#d5d9de", "#9aa0a8"
PAD, PAD_EDGE, HOLE = "#e9dcb4", "#b8963a", "#ffffff"


def _len(a, b):
    return math.hypot(b[0] - a[0], b[1] - a[1])


def _rot(a, b):
    return math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))


def text(x, y, s, size=10.5, weight="700", fill="#1a1a1a", anchor="middle", fig=None, rotate=None):
    df = f' data-fig="{fig}"' if fig else ""
    tr = f' transform="rotate({rotate:.1f} {x:.1f} {y:.1f})"' if rotate else ""
    s = str(s).replace("<", "&lt;")                              # edge pads on the left are named C<
    return (f'<text x="{x:.1f}" y="{y + size * 0.36:.1f}" text-anchor="{anchor}" '
            f'font-size="{size}" font-weight="{weight}" fill="{fill}"{df}{tr}>{s}</text>')


# ---- the board ------------------------------------------------------------------------------
def hole(x, y, S):
    return [f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{0.9 * S:.1f}" fill="{PAD}" stroke="{PAD_EDGE}" '
            f'stroke-width="1.2"/>',
            f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{0.35 * S:.1f}" fill="{HOLE}"/>']


def edge_pad(x, y, S, along_x=False):
    """An edge pad: an oval pad, its long side pointing to the board edge it sits at."""
    w, h = (1.5 * S, 3.0 * S) if not along_x else (3.0 * S, 1.5 * S)
    return [f'<rect x="{x - w / 2:.1f}" y="{y - h / 2:.1f}" width="{w:.1f}" height="{h:.1f}" '
            f'rx="{min(w, h) / 2:.1f}" fill="{PAD}" stroke="{PAD_EDGE}" stroke-width="1.2"/>']


def lead_dot(x, y, S, fill, edge=None):
    rim = f' stroke="{edge}" stroke-width="1.5"' if edge else ""
    return [f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{0.75 * S:.1f}" fill="{fill}"{rim}/>']


# ---- connections ----------------------------------------------------------------------------
def tin(a, b, S):
    """Solder between two neighboring pads."""
    L, ang = _len(a, b), _rot(a, b)
    cx, cy = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
    return [f'<rect x="{cx - L / 2 - 0.6 * S:.1f}" y="{cy - 0.7 * S:.1f}" width="{L + 1.2 * S:.1f}" '
            f'height="{1.4 * S:.1f}" rx="{0.7 * S:.1f}" fill="{TIN}" stroke="{TIN_EDGE}" '
            f'stroke-width="1" transform="rotate({ang:.1f} {cx:.1f} {cy:.1f})"/>']


def bare(a, b, S):
    """Uninsulated wire flat along a row, soldered in every hole on the way."""
    return [f'<line x1="{a[0]:.1f}" y1="{a[1]:.1f}" x2="{b[0]:.1f}" y2="{b[1]:.1f}" '
            f'stroke="{SILVER_EDGE}" stroke-width="{0.75 * S:.1f}" stroke-linecap="round"/>',
            f'<line x1="{a[0]:.1f}" y1="{a[1]:.1f}" x2="{b[0]:.1f}" y2="{b[1]:.1f}" '
            f'stroke="{SILVER}" stroke-width="{0.45 * S:.1f}" stroke-linecap="round"/>']


def link(a, b, S):
    """Uninsulated wire through two holes, lifted clear of what lies between."""
    out = bare(a, b, S)
    out += [f'<circle cx="{p[0]:.1f}" cy="{p[1]:.1f}" r="{0.45 * S:.1f}" fill="{SILVER}" '
            f'stroke="{SILVER_EDGE}" stroke-width="1"/>' for p in (a, b)]
    return out


def insulated(points, S, color):
    """Insulated wire along a polyline, soldered at its two ends only."""
    d = " ".join(f"{'M' if i == 0 else 'L'} {x:.1f} {y:.1f}" for i, (x, y) in enumerate(points))
    return [f'<path d="{d}" fill="none" stroke="#333333" stroke-width="{1.1 * S:.1f}" '
            'stroke-linecap="round" stroke-linejoin="round"/>',
            f'<path d="{d}" fill="none" stroke="{color}" stroke-width="{0.7 * S:.1f}" '
            'stroke-linecap="round" stroke-linejoin="round"/>']


# ---- parts ----------------------------------------------------------------------------------
def _axial_body(a, b, S, length, dia, fill, rx_frac=0.35):
    L, ang = _len(a, b), _rot(a, b)
    cx, cy = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
    out = [f'<line x1="{a[0]:.1f}" y1="{a[1]:.1f}" x2="{b[0]:.1f}" y2="{b[1]:.1f}" '
           f'stroke="{SILVER_EDGE}" stroke-width="{0.55 * S:.1f}"/>']
    bl, bd = length * S, dia * S
    tr = f' transform="rotate({ang:.1f} {cx:.1f} {cy:.1f})"'
    out.append(f'<rect x="{cx - bl / 2:.1f}" y="{cy - bd / 2:.1f}" width="{bl:.1f}" '
               f'height="{bd:.1f}" rx="{bd * rx_frac:.1f}" fill="{fill}" stroke="#4d4d4d" '
               f'stroke-width="1"{tr}/>')
    return out, (cx, cy, bl, bd, tr)


def resistor(a, b, S, ohms, standing=False):
    """Carbon film resistor: beige body with its four color bands."""
    p = PARTS["resistor"]
    if standing:
        return _standing(a, b, S, p["body_d"][1], p["color"], bands=bands(ohms))
    out, (cx, cy, bl, bd, tr) = _axial_body(a, b, S, p["body_l"][1], p["body_d"][1], p["color"])
    bw = 0.55 * S
    xs = [cx - bl / 2 + bl * f for f in (0.20, 0.33, 0.46, 0.80)]
    for x, c in zip(xs, bands(ohms)):
        out.append(f'<rect x="{x - bw / 2:.1f}" y="{cy - bd / 2:.1f}" width="{bw:.1f}" '
                   f'height="{bd:.1f}" fill="{BAND_COLOR[c]}"{tr}/>')
    return out


def _standing(a, b, S, dia, fill, bands=None):
    """A part stood upright over lead a, its other lead bent back down into b."""
    r = dia / 2 * S
    out = [f'<line x1="{a[0]:.1f}" y1="{a[1]:.1f}" x2="{b[0]:.1f}" y2="{b[1]:.1f}" '
           f'stroke="{SILVER_EDGE}" stroke-width="{0.55 * S:.1f}"/>',
           f'<circle cx="{a[0]:.1f}" cy="{a[1]:.1f}" r="{r:.1f}" fill="{fill}" stroke="#4d4d4d" '
           'stroke-width="1"/>']
    if bands:
        out.append(f'<circle cx="{a[0]:.1f}" cy="{a[1]:.1f}" r="{r * 0.62:.1f}" fill="none" '
                   f'stroke="{BAND_COLOR[bands[0]]}" stroke-width="{0.45 * S:.1f}"/>')
    return out


def diode(k, a, S):
    """DO-41: black body, gray band at the cathode end."""
    p = PARTS["diode_do41"]
    out, (cx, cy, bl, bd, tr) = _axial_body(k, a, S, p["body_l"][1], p["body_d"][1], p["color"], 0.2)
    bw = 0.9 * S
    out.append(f'<rect x="{cx - bl / 2 + 0.5 * S:.1f}" y="{cy - bd / 2:.1f}" width="{bw:.1f}" '
               f'height="{bd:.1f}" fill="{p["band"]}"{tr}/>')
    return out


def ceramic(a, b, S, key="ceramic_k15"):
    """Radial ceramic, its epoxy disc between the leads: the black Vishay K or the blue TDK FG."""
    p = PARTS[key]
    L, ang = _len(a, b), _rot(a, b)
    cx, cy = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
    w, t = p["body_w"][1] * S, p["body_t"][1] * S
    return [f'<line x1="{a[0]:.1f}" y1="{a[1]:.1f}" x2="{b[0]:.1f}" y2="{b[1]:.1f}" '
            f'stroke="{SILVER_EDGE}" stroke-width="{0.5 * S:.1f}"/>',
            f'<rect x="{cx - w / 2:.1f}" y="{cy - t / 2:.1f}" width="{w:.1f}" height="{t:.1f}" '
            f'rx="{t / 2:.1f}" fill="{p["color"]}" stroke="#4d4d4d" stroke-width="1" '
            f'transform="rotate({ang:.1f} {cx:.1f} {cy:.1f})"/>']


def electrolytic(plus, minus, S, key="electrolytic_esl_c3"):
    """Radial electrolytic standing, seen from above: its sleeve, a light stripe over the minus
    lead. key names the part in the inventory: the KEMET ESL 5 x 11 or the Rubycon ZLH 8 x 20."""
    p = PARTS[key]
    cx, cy = (plus[0] + minus[0]) / 2, (plus[1] + minus[1]) / 2
    r = p["body_d"][1] / 2 * S
    ang = _rot(plus, minus)
    out = [f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="{r:.1f}" fill="{p["color"]}" stroke="#4d4d4d" '
           'stroke-width="1"/>']
    # the stripe: a band of the can toward the minus lead
    x0 = cx + math.cos(math.radians(ang)) * r * 0.45
    out.append(f'<path d="M {x0:.1f} {cy - r * 0.89:.1f} A {r:.1f} {r:.1f} 0 0 1 {x0:.1f} '
               f'{cy + r * 0.89:.1f} Z" fill="{p["stripe"]}" '
               f'transform="rotate({ang:.1f} {cx:.1f} {cy:.1f})"/>')
    out.append(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="{r * 0.55:.1f}" fill="none" '
               f'stroke="#6b6b6b" stroke-width="1"/>')
    return out


def to220(leads, S, tab_side):
    """TO-220 standing: black body over the three leads, metal tab behind them.

    leads are pin 1, 2, 3 in a straight row. tab_side is +1 or -1: which side of the row the
    tab sits on, as the normal to the row turned counterclockwise from pin 1 toward pin 3."""
    p = PARTS["to220"]
    a, c = leads[0], leads[2]
    ang = _rot(a, c)
    cx, cy = leads[1]
    e = p["body_e"][1] * S
    back = p["lead_to_back"][1] * S
    front = (p["body_a"][1] - p["lead_to_back"][0]) * S
    tab = 1.4 * S
    tr = f' transform="rotate({ang:.1f} {cx:.1f} {cy:.1f})"'
    if tab_side > 0:
        y0, y1 = cy - back, cy + front
        ty = y0
    else:
        y0, y1 = cy - front, cy + back
        ty = y1 - tab
    return [f'<rect x="{cx - e / 2:.1f}" y="{y0:.1f}" width="{e:.1f}" height="{y1 - y0:.1f}" '
            f'fill="{p["color"]}" stroke="#4d4d4d" stroke-width="1"{tr}/>',
            f'<rect x="{cx - e / 2:.1f}" y="{ty:.1f}" width="{e:.1f}" height="{tab:.1f}" '
            f'fill="{p["tab"]}" stroke="#8a8f96" stroke-width="1"{tr}/>']


def header(pins, S, female=False):
    """Pin header or socket: black insulator, a pin or a socket opening per position."""
    key = "header_female" if female else "header_male"
    p = PARTS[key]
    xs = [x for x, _ in pins]
    ys = [y for _, y in pins]
    w = p["body_w"][1] * S
    out = [f'<rect x="{min(xs) - w / 2:.1f}" y="{min(ys) - w / 2:.1f}" '
           f'width="{max(xs) - min(xs) + w:.1f}" height="{max(ys) - min(ys) + w:.1f}" '
           f'rx="2" fill="{p["color"]}" stroke="#4d4d4d" stroke-width="1"/>']
    for x, y in pins:
        s = (0.64 if not female else 1.0) * S
        fill = "#d9b54a" if not female else "#3a3a3a"
        out.append(f'<rect x="{x - s / 2:.1f}" y="{y - s / 2:.1f}" width="{s:.1f}" '
                   f'height="{s:.1f}" fill="{fill}"/>')
    return out


def terminal(pins, S, entry):
    """Screw terminal: blue body, a screw over each pin, wire entry on the side `entry` names.

    entry is the unit vector from the pin row toward the wire openings."""
    p = PARTS["terminal_5mm"]
    xs = [x for x, _ in pins]
    ys = [y for _, y in pins]
    n = len(pins)
    horiz = max(xs) - min(xs) >= max(ys) - min(ys)
    length = p["pitch"] * n * S
    depth = p["body_depth"][1] * S
    back = p["pins_from_back"] * S
    cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    ex, ey = entry
    if horiz:
        x0, x1 = cx - length / 2, cx + length / 2
        y0 = cy - back if ey > 0 else cy - (depth - back)
        y1 = y0 + depth
    else:
        y0, y1 = cy - length / 2, cy + length / 2
        x0 = cx - back if ex > 0 else cx - (depth - back)
        x1 = x0 + depth
    out = [f'<rect x="{x0:.1f}" y="{y0:.1f}" width="{x1 - x0:.1f}" height="{y1 - y0:.1f}" rx="2" '
           f'fill="{p["color"]}" stroke="#1f4e95" stroke-width="1.2"/>']
    for x, y in pins:
        out.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{1.6 * S:.1f}" fill="{SILVER}" '
                   f'stroke="{SILVER_EDGE}" stroke-width="1"/>')
        out.append(f'<line x1="{x - 1.1 * S:.1f}" y1="{y:.1f}" x2="{x + 1.1 * S:.1f}" y2="{y:.1f}" '
                   f'stroke="#6b6b6b" stroke-width="{0.3 * S:.1f}"/>')
    return out


def dip(pins, S, body_l, body_w, color="#1a1a1a"):
    """DIP IC from above: body over two rows, notch at the pin 1 end, dot beside pin 1.

    pins are the pin centers in pin order 1..n, counterclockwise as the package numbers them."""
    n = len(pins)
    xs = [x for x, _ in pins]
    ys = [y for _, y in pins]
    cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    p1, pn = pins[0], pins[n // 2 - 1]
    horiz = abs(pn[0] - p1[0]) > abs(pn[1] - p1[1])       # rows run along x
    L, Wd = body_l * S, body_w * S
    w, h = (L, Wd) if horiz else (Wd, L)
    out = [f'<rect x="{cx - w / 2:.1f}" y="{cy - h / 2:.1f}" width="{w:.1f}" height="{h:.1f}" '
           f'rx="2" fill="{color}" stroke="#4d4d4d" stroke-width="1"/>']
    # notch: a half circle on the end where pin 1 sits
    if horiz:
        ex = cx - w / 2 if p1[0] < cx else cx + w / 2
        out.append(f'<circle cx="{ex:.1f}" cy="{cy:.1f}" r="{0.9 * S:.1f}" fill="#3a3a3a"/>')
    else:
        ey = cy - h / 2 if p1[1] < cy else cy + h / 2
        out.append(f'<circle cx="{cx:.1f}" cy="{ey:.1f}" r="{0.9 * S:.1f}" fill="#3a3a3a"/>')
    dx = (cx - p1[0]) * 0.25 if horiz else (cx - p1[0]) * 0.55
    dy = (cy - p1[1]) * 0.55 if horiz else (cy - p1[1]) * 0.25
    out.append(f'<circle cx="{p1[0] + dx:.1f}" cy="{p1[1] + dy:.1f}" r="{0.45 * S:.1f}" '
               'fill="#5a5a5a"/>')
    return out


def module(x0, y0, x1, y1, color, pins, S, edge="#1f1f1f", holes=()):
    """A breakout board seen from above as a daughter board: its PCB outline, the header pins
    it stands on and its mounting holes."""
    out = [f'<rect x="{x0:.1f}" y="{y0:.1f}" width="{x1 - x0:.1f}" height="{y1 - y0:.1f}" rx="3" '
           f'fill="{color}" stroke="{edge}" stroke-width="1.2" fill-opacity="0.92"/>']
    for x, y in pins:
        out.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{0.8 * S:.1f}" fill="#d9b54a" '
                   'stroke="#8a6d0b" stroke-width="1"/>')
    for x, y, d in holes:
        out.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{d / 2 * S:.1f}" fill="#f8f4ee" '
                   'stroke="#8a6d0b" stroke-width="1"/>')
    return out


def side_looker(a, b, S, key, facing):
    """Side-looking LED or phototransistor standing on its two leads, seen from above.

    The body is a thin block across the line a-b; `facing` (+1 or -1) is the side of that line
    its lens looks to, as the normal turned counterclockwise from a toward b."""
    p = PARTS[key]
    ang = _rot(a, b)
    cx, cy = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
    w = p["body_w"][1] * S
    t = p["body_t"][1] * S
    off = p.get("lead_offset_to_lens")
    # the leads sit on the body's center line unless the sheet places them off it
    shift = 0.0 if off is None else (t / 2 - off * S)
    y0 = cy - t / 2 - facing * shift
    tr = f' transform="rotate({ang:.1f} {cx:.1f} {cy:.1f})"'
    lens_y = y0 if facing > 0 else y0 + t
    return [f'<rect x="{cx - w / 2:.1f}" y="{y0:.1f}" width="{w:.1f}" height="{t:.1f}" rx="1.5" '
            f'fill="{p["color"]}" stroke="#4d4d4d" stroke-width="1"{tr}/>',
            f'<ellipse cx="{cx:.1f}" cy="{lens_y:.1f}" rx="{p["lens_d"] / 2 * S:.1f}" '
            f'ry="{0.35 * S:.1f}" fill="#9ad0ef" stroke="#4d4d4d" stroke-width="1"{tr}/>']


def reflective(pins, S, key):
    """Reflective sensor looking up from the board: dark body, two windows, chamfer at pin 1."""
    p = PARTS[key]
    xs = [x for x, _ in pins]
    ys = [y for _, y in pins]
    cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    a = p["body_a"][1] * S
    b = p["body_b"][1] * S
    w, h = (b, a) if (max(xs) - min(xs)) >= (max(ys) - min(ys)) else (a, b)
    out = [f'<rect x="{cx - w / 2:.1f}" y="{cy - h / 2:.1f}" width="{w:.1f}" height="{h:.1f}" '
           f'rx="1" fill="{p["color"]}" stroke="#4d4d4d" stroke-width="1"/>']
    for dx in (-0.22, 0.22):
        out.append(f'<circle cx="{cx + dx * w:.1f}" cy="{cy:.1f}" r="{0.18 * min(w, h):.1f}" '
                   'fill="#5a2a2a" stroke="#2a2a2a" stroke-width="1"/>')
    px, py = pins[0]
    out.append(f'<circle cx="{(px + cx) / 2:.1f}" cy="{(py + cy) / 2:.1f}" r="{0.3 * S:.1f}" '
               'fill="#8a8a8a"/>')
    return out


def breakout(key, ox, oy, S, rot=0, labels=True):
    """A breakout board from the inventory, its corner at (ox, oy), turned by rot (0, 90, 180,
    270 degrees, clockwise). Returns the elements and the drawing position of every pin."""
    from inventory import MODULES
    m = MODULES[key]
    L, Wd = m["size"]

    def place(x, y):
        if rot == 0:
            return ox + x * S, oy + y * S
        if rot == 90:
            return ox + (Wd - y) * S, oy + x * S
        if rot == 180:
            return ox + (L - x) * S, oy + (Wd - y) * S
        return ox + y * S, oy + (L - x) * S
    corners = [place(0, 0), place(L, Wd)]
    x0, x1 = sorted(c[0] for c in corners)
    y0, y1 = sorted(c[1] for c in corners)
    pins = [(place(x, y), n) for x, y, n in m["pins"]]
    out = module(x0, y0, x1, y1, m["color"], [p for p, _ in pins], S)
    if labels:
        mx, my = (x0 + x1) / 2, (y0 + y1) / 2
        for (x, y), n in pins:
            # the name sits beside the pin, across its row and toward the board's middle
            row_runs_x = any(abs(q[1] - y) < 0.5 and abs(q[0] - x) > 0.5 for q, _ in pins)
            if row_runs_x:
                tx, ty = x, y + (1.6 * S if y < my else -1.6 * S)
                anchor = "middle"
            else:
                tx, ty = x + (1.6 * S if x < mx else -1.6 * S), y
                anchor = "start" if x < mx else "end"
            out.append(text(tx, ty, n, size=max(6.0, 0.9 * S), weight="400", fill="#ffffff",
                            anchor=anchor))
    return out, pins


def dip_part(key, pin1, S, along=(1, 0)):
    """A DIP IC from the inventory with pin 1 at the hole `pin1`; its pins run along `along`
    and the second row lies to the left of that direction, three holes away, so the pins count
    counterclockwise seen from above as the package numbers them."""
    p = PARTS[key]
    n = p["pins"]
    half = n // 2
    ax, ay = along
    rx, ry = ay, -ax              # left of the run direction, in screen coordinates (y down)
    step = PITCH * S
    row = 3 * step                # 7.62 mm between the rows
    pins = [(pin1[0] + ax * step * i, pin1[1] + ay * step * i) for i in range(half)]
    pins += [(pin1[0] + ax * step * (half - 1 - i) + rx * row,
              pin1[1] + ay * step * (half - 1 - i) + ry * row) for i in range(half)]
    return dip(pins, S, p["body_l"][1], p["body_w"][1], p["color"]), pins


def header_90(pins, S, out_dir):
    """Right-angle pin header seen from above: the bent tails from the holes, the insulator,
    and the mating pins reaching off the board. out_dir is the unit vector the pins point to."""
    p = PARTS["header_male_90"]
    ox, oy = out_dir
    face = p["row_to_face"][1] * S            # hole row to the insulator's front face
    thick = p["body_w"][1] * S
    tip = face + p["pin_out"][1] * S
    pin = p["pin"][1] * S
    out = []
    for x, y in pins:
        out.append(f'<line x1="{x:.1f}" y1="{y:.1f}" x2="{x + ox * tip:.1f}" y2="{y + oy * tip:.1f}" '
                   f'stroke="#d9b54a" stroke-width="{pin:.1f}"/>')
    xs = [x for x, _ in pins]
    ys = [y for _, y in pins]
    half = PITCH * S / 2
    if ox != 0:      # pins point along x, the row runs along y
        xa = min(xs) + ox * (face - thick) if ox > 0 else min(xs) - face
        out.append(f'<rect x="{min(xa, xa + ox * thick):.1f}" y="{min(ys) - half:.1f}" '
                   f'width="{thick:.1f}" height="{max(ys) - min(ys) + 2 * half:.1f}" rx="2" '
                   f'fill="{p["color"]}" stroke="#4d4d4d" stroke-width="1"/>')
    else:
        ya = min(ys) + oy * (face - thick) if oy > 0 else min(ys) - face
        out.append(f'<rect x="{min(xs) - half:.1f}" y="{min(ya, ya + oy * thick):.1f}" '
                   f'width="{max(xs) - min(xs) + 2 * half:.1f}" height="{thick:.1f}" rx="2" '
                   f'fill="{p["color"]}" stroke="#4d4d4d" stroke-width="1"/>')
    return out
