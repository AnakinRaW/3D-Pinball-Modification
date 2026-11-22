"""Turns the playfield drawing into the inputs of the intro scene.

Writes, into the folder given as the first argument:
  shapes.json  every shape of the composed drawing as a world polyline in millimeters, tagged with its
               placement, its class and its data-name
  floor.png    the playfield board as the floor texture: layer 0 without the parts the scene raises in 3D
  backbox.png  the backbox picture, which the scene stands up behind the playfield

Run with the system Python from the repository root: python assets/intro/blender/extract.py <out>
"""
import copy, ctypes, json, math, os, re, sys
import xml.etree.ElementTree as ET

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
SVG = os.path.join(ROOT, "assets", "playfield", "playfield.svg")
NS = {"svg": "http://www.w3.org/2000/svg", "ink": "http://www.inkscape.org/namespaces/inkscape"}
ET.register_namespace("", NS["svg"]); ET.register_namespace("inkscape", NS["ink"])
TEX_SCALE = 6.0  # texture pixels per millimeter

# Groups the scene builds in 3D, or that the film does not show, so the floor texture leaves them out.
RAISED = {"bumpers", "bells", "side-guides", "flippers", "inlane-separators", "inlane-rails",
          "top-lane-guide-1", "top-lane-guide-2", "top-lane-guide-3", "lane-guides-left", "orbit-wall",
          "target-left", "target-right", "ir-sensors", "drain-ball-store", "speaker", "seal",
          "balls", "lights", "layer-1", "backbox", "bumper-plate", "drain"}


def matmul(a, b):
    return (a[0] * b[0] + a[2] * b[1], a[1] * b[0] + a[3] * b[1], a[0] * b[2] + a[2] * b[3],
            a[1] * b[2] + a[3] * b[3], a[0] * b[4] + a[2] * b[5] + a[4], a[1] * b[4] + a[3] * b[5] + a[5])


def parse_transform(s):
    m = (1, 0, 0, 1, 0, 0)
    for name, args in re.findall(r"(\w+)\s*\(([^)]*)\)", s or ""):
        v = [float(x) for x in re.split(r"[\s,]+", args.strip()) if x]
        if name == "translate":
            t = (1, 0, 0, 1, v[0], v[1] if len(v) > 1 else 0)
        elif name == "scale":
            t = (v[0], 0, 0, v[1] if len(v) > 1 else v[0], 0, 0)
        elif name == "rotate":
            a = math.radians(v[0]); c, s_ = math.cos(a), math.sin(a)
            t = (c, s_, -s_, c, 0, 0)
            if len(v) == 3:
                t = matmul(matmul((1, 0, 0, 1, v[1], v[2]), t), (1, 0, 0, 1, -v[1], -v[2]))
        elif name == "matrix":
            t = tuple(v)
        else:
            continue
        m = matmul(m, t)
    return m


def apply(m, p):
    return (m[0] * p[0] + m[2] * p[1] + m[4], m[1] * p[0] + m[3] * p[1] + m[5])


def arc_points(p0, rx, ry, phi, large, sweep, p1, n=12):
    """Samples an SVG elliptical arc by converting it to its center form."""
    if rx == 0 or ry == 0:
        return [p1]
    phi = math.radians(phi); c, s = math.cos(phi), math.sin(phi)
    dx, dy = (p0[0] - p1[0]) / 2, (p0[1] - p1[1]) / 2
    x1, y1 = c * dx + s * dy, -s * dx + c * dy
    rx, ry = abs(rx), abs(ry)
    lam = x1 * x1 / (rx * rx) + y1 * y1 / (ry * ry)
    if lam > 1:
        rx, ry = rx * math.sqrt(lam), ry * math.sqrt(lam)
    num = rx * rx * ry * ry - rx * rx * y1 * y1 - ry * ry * x1 * x1
    den = rx * rx * y1 * y1 + ry * ry * x1 * x1
    k = math.sqrt(max(0.0, num / den)) * (-1 if large == sweep else 1)
    cx1, cy1 = k * rx * y1 / ry, -k * ry * x1 / rx
    cx, cy = c * cx1 - s * cy1 + (p0[0] + p1[0]) / 2, s * cx1 + c * cy1 + (p0[1] + p1[1]) / 2
    def ang(u, v):
        a = math.atan2(u[0] * v[1] - u[1] * v[0], u[0] * v[0] + u[1] * v[1]); return a
    t1 = ang((1, 0), ((x1 - cx1) / rx, (y1 - cy1) / ry))
    dt = ang(((x1 - cx1) / rx, (y1 - cy1) / ry), ((-x1 - cx1) / rx, (-y1 - cy1) / ry))
    if not sweep and dt > 0: dt -= 2 * math.pi
    if sweep and dt < 0: dt += 2 * math.pi
    out = []
    for i in range(1, n + 1):
        t = t1 + dt * i / n
        out.append((cx + rx * math.cos(t) * c - ry * math.sin(t) * s, cy + rx * math.cos(t) * s + ry * math.sin(t) * c))
    return out


def path_polylines(d, n=8):
    """Flattens path data into a list of (points, closed) subpaths."""
    toks = re.findall(r"[A-Za-z]|-?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?", d)
    i, cmd, cur, start = 0, None, (0.0, 0.0), (0.0, 0.0)
    subs, pts, last_c = [], [], None
    def num():
        nonlocal i; v = float(toks[i]); i += 1; return v
    while i < len(toks):
        if re.match(r"[A-Za-z]", toks[i]):
            cmd = toks[i]; i += 1
            if cmd in "Zz":
                if pts: subs.append((pts, True))
                pts, cur, last_c = [], start, None
                continue
        rel = cmd.islower(); C = cmd.upper()
        ox, oy = cur if rel else (0.0, 0.0)
        if C == "M":
            if pts: subs.append((pts, False))
            cur = (ox + num(), oy + num()); start = cur; pts = [cur]; cmd = "l" if rel else "L"; last_c = None
        elif C == "L":
            cur = (ox + num(), oy + num()); pts.append(cur); last_c = None
        elif C == "H":
            cur = ((cur[0] if rel else 0) + num(), cur[1]); pts.append(cur); last_c = None
        elif C == "V":
            cur = (cur[0], (cur[1] if rel else 0) + num()); pts.append(cur); last_c = None
        elif C in "CS":
            if C == "C":
                c1 = (ox + num(), oy + num())
            else:
                c1 = (2 * cur[0] - last_c[0], 2 * cur[1] - last_c[1]) if last_c else cur
            c2 = (ox + num(), oy + num()); p = (ox + num(), oy + num())
            for k in range(1, n + 1):
                t = k / n; u = 1 - t
                pts.append((u**3 * cur[0] + 3 * u * u * t * c1[0] + 3 * u * t * t * c2[0] + t**3 * p[0],
                            u**3 * cur[1] + 3 * u * u * t * c1[1] + 3 * u * t * t * c2[1] + t**3 * p[1]))
            cur, last_c = p, c2
        elif C in "QT":
            if C == "Q":
                c1 = (ox + num(), oy + num())
            else:
                c1 = (2 * cur[0] - last_c[0], 2 * cur[1] - last_c[1]) if last_c else cur
            p = (ox + num(), oy + num())
            for k in range(1, n + 1):
                t = k / n; u = 1 - t
                pts.append((u * u * cur[0] + 2 * u * t * c1[0] + t * t * p[0], u * u * cur[1] + 2 * u * t * c1[1] + t * t * p[1]))
            cur, last_c = p, c1
        elif C == "A":
            rx, ry, phi, la, sw = num(), num(), num(), num(), num()
            p = (ox + num(), oy + num())
            pts += arc_points(cur, rx, ry, phi, int(la), int(sw), p); cur = p; last_c = None
        else:
            i += 1
    if pts: subs.append((pts, False))
    return subs


def shape_polylines(e):
    tag = e.tag.split("}")[1]
    f = lambda k, dflt=0.0: float(e.get(k, dflt))
    if tag == "path":
        return path_polylines(e.get("d", ""))
    if tag == "circle":
        cx, cy, r = f("cx"), f("cy"), f("r")
        return [([(cx + r * math.cos(2 * math.pi * k / 48), cy + r * math.sin(2 * math.pi * k / 48)) for k in range(48)], True)]
    if tag == "ellipse":
        cx, cy, rx, ry = f("cx"), f("cy"), f("rx"), f("ry")
        return [([(cx + rx * math.cos(2 * math.pi * k / 48), cy + ry * math.sin(2 * math.pi * k / 48)) for k in range(48)], True)]
    if tag == "rect":
        x, y, w, h = f("x"), f("y"), f("width"), f("height")
        rx = min(f("rx", e.get("ry", 0)), w / 2, h / 2)
        if rx <= 0:
            return [([(x, y), (x + w, y), (x + w, y + h), (x, y + h)], True)]
        pts = []
        for cx, cy, a0 in ((x + w - rx, y + rx, -90), (x + w - rx, y + h - rx, 0), (x + rx, y + h - rx, 90), (x + rx, y + rx, 180)):
            for k in range(7):
                a = math.radians(a0 + 15 * k); pts.append((cx + rx * math.cos(a), cy + rx * math.sin(a)))
        return [(pts, True)]
    if tag in ("polygon", "polyline"):
        v = [float(x) for x in re.split(r"[\s,]+", e.get("points", "").strip()) if x]
        return [(list(zip(v[0::2], v[1::2])), tag == "polygon")]
    if tag == "line":
        return [([(f("x1"), f("y1")), (f("x2"), f("y2"))], False)]
    return []


def css_rules(root):
    rules = {}
    for st in root.iter("{%s}style" % NS["svg"]):
        for sel, body in re.findall(r"([^{}]+)\{([^}]*)\}", st.text or ""):
            props = dict((k.strip(), v.strip()) for k, v in (p.split(":", 1) for p in body.split(";") if ":" in p))
            for s in sel.split(","):
                s = s.strip()
                if re.fullmatch(r"\.[\w-]+", s):
                    rules.setdefault(s[1:], {}).update(props)
    return rules


def extract(root):
    rules = css_rules(root)
    out = []
    def walk(e, m, chain):
        tag = e.tag.split("}")[1]
        if tag in ("defs", "style", "title", "clipPath", "use"):
            return
        m = matmul(m, parse_transform(e.get("transform")))
        if tag == "g":
            ch = chain + [e.get("id")] if e.get("id") else chain
            for c in e:
                walk(c, m, ch)
            return
        cls = e.get("class", "")
        style = {}
        for c in cls.split():
            style.update(rules.get(c, {}))
        scale = math.sqrt(abs(m[0] * m[3] - m[1] * m[2]))
        for pts, closed in shape_polylines(e):
            rec = {"chain": chain, "cls": cls, "name": e.get("data-name") or e.get("id"), "closed": closed,
                   "fill": style.get("fill"), "stroke": style.get("stroke"),
                   "sw": float(re.sub("[^0-9.]", "", style.get("stroke-width", "0")) or 0) * scale,
                   "pts": [tuple(round(c, 3) for c in apply(m, p)) for p in pts]}
            if e.tag.endswith("circle"):
                c = apply(m, (float(e.get("cx", 0)), float(e.get("cy", 0))))
                rec["circle"] = [round(c[0], 3), round(c[1], 3), round(float(e.get("r")) * scale, 3)]
            out.append(rec)
    for c in root:
        walk(c, (1, 0, 0, 1, 0, 0), [])
    return out


def render(svg_bytes, out_png, width_px):
    """Renders SVG markup to PNG with librsvg from the GTK3 runtime."""
    os.add_dll_directory(r"C:\Program Files\GTK3-Runtime Win64\bin")
    rsvg = ctypes.CDLL("librsvg-2-2.dll"); cairo = ctypes.CDLL("libcairo-2.dll")
    rsvg.rsvg_handle_new_from_data.restype = ctypes.c_void_p
    cairo.cairo_image_surface_create.restype = ctypes.c_void_p
    cairo.cairo_create.restype = ctypes.c_void_p
    err = ctypes.c_void_p()
    h = rsvg.rsvg_handle_new_from_data(svg_bytes, ctypes.c_size_t(len(svg_bytes)), ctypes.byref(err))
    class Dim(ctypes.Structure):
        _fields_ = [("w", ctypes.c_int), ("h", ctypes.c_int), ("em", ctypes.c_double), ("ex", ctypes.c_double)]
    d = Dim(); rsvg.rsvg_handle_get_dimensions(ctypes.c_void_p(h), ctypes.byref(d))
    scale = width_px / d.w
    W, H = int(d.w * scale), int(d.h * scale)
    s = cairo.cairo_image_surface_create(0, W, H); c = cairo.cairo_create(ctypes.c_void_p(s))
    cairo.cairo_scale(ctypes.c_void_p(c), ctypes.c_double(scale), ctypes.c_double(scale))
    rsvg.rsvg_handle_render_cairo(ctypes.c_void_p(h), ctypes.c_void_p(c))
    cairo.cairo_surface_write_to_png(ctypes.c_void_p(s), out_png.encode())
    return W, H


def strip(root, keep_only=None, drop=()):
    """Copies the drawing and removes groups by id: all but keep_only at the top, and every id in drop."""
    r = copy.deepcopy(root)
    def prune(e):
        for c in list(e):
            if c.get("id") in drop or (keep_only and e is r and c.tag.endswith("}g") and c.get("id") not in keep_only):
                e.remove(c)
            else:
                prune(c)
    prune(r)
    return r


def main():
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    root = ET.parse(SVG).getroot()
    vb = [float(v) for v in root.get("viewBox").split()]
    width_px = round(TEX_SCALE * vb[2])
    shapes = extract(root)
    json.dump({"viewBox": vb, "shapes": shapes}, open(os.path.join(out, "shapes.json"), "w"))
    floor = strip(root, drop=RAISED)
    W, H = render(ET.tostring(floor), os.path.join(out, "floor.png"), width_px)
    back = strip(root, keep_only={"backbox"})
    render(ET.tostring(back), os.path.join(out, "backbox.png"), width_px)
    # the diorama's layers, back to front, each alone, so the scene can stand them up at their depths
    layers = {"backbox-back": {"sky", "display"}, "backbox-b20": {"dio-b20"}, "backbox-scene": {"dio-scene"},
              "backbox-frame": {"dio-frame"}}
    every = {"sky", "display", "dio-b20", "dio-scene", "dio-frame", "lights-ceiling"}
    for name, keep in layers.items():
        layer = strip(root, keep_only={"backbox"}, drop=every - keep)
        render(ET.tostring(layer), os.path.join(out, name + ".png"), width_px)
    json.dump({"viewBox": vb, "size": [W, H]}, open(os.path.join(out, "textures.json"), "w"))
    print(len(shapes), "shapes;", "textures", W, "x", H)


if __name__ == "__main__":
    main()
