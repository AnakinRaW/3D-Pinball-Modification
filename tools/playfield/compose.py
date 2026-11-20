"""Composes the playfield drawing from its part files.

assets/playfield/playfield.json holds the drawing's tree: layers, groups, where each part sits, the LED strips and the single
lamps. Each file in assets/playfield/parts/ is one physical part, drawn around its anchor with the styles and definitions it
needs, so it also opens alone. This script places every part, lays the LEDs of each strip along its route at the strip's
pitch, sets each lamp onto its part and writes assets/playfield/playfield.svg. It also writes assets/playfield/features.svg,
the same drawing with a numbered bubble for each feature the JSON lists, joined by lines to the parts that show it.

    python tools/playfield/compose.py            writes the drawing
    python tools/playfield/compose.py --check    exits with 1 when the drawing differs from what the parts compose

Only the standard library is used.
"""
import argparse, copy, json, math, re, sys
import xml.etree.ElementTree as ET
from pathlib import Path

SVG_NS = "http://www.w3.org/2000/svg"
INK_NS = "http://www.inkscape.org/namespaces/inkscape"
ET.register_namespace("", SVG_NS)
ET.register_namespace("inkscape", INK_NS)
LABEL, MODE = f"{{{INK_NS}}}label", f"{{{INK_NS}}}groupmode"

ROOT = Path(__file__).resolve().parents[2]
ASSETS = ROOT / "assets" / "playfield"


def tag(name):
    return f"{{{SVG_NS}}}{name}"


def fmt(v):
    s = f"{v:.3f}".rstrip("0").rstrip(".")
    return "0" if s in ("-0", "") else s


def rules(css):
    """The rules of a style sheet as (selector, declarations), whitespace normalized."""
    return [(s.strip(), " ".join(b.split())) for s, b in re.findall(r"([^{}]+)\{([^{}]*)\}", css or "")]


class Composer:
    def __init__(self, doc, parts_dir):
        self.doc = doc
        self.parts_dir = parts_dir
        self.parts = {}            # file name -> (rules, defs, content)
        self.style = {}            # selector -> (declarations, first part that brought it)
        self.defs = {}             # id -> (markup, element, first part that brought it)
        self.placed = {}           # placement id -> placement node
        self.errors = []
        self.report = []
        self.points = {}           # placement or LED id -> its point in the drawing

    # ---------------------------------------------------------------- part files
    def part(self, name):
        if name not in self.parts:
            path = self.parts_dir / f"{name}.svg"
            root = ET.parse(path).getroot()
            css, defs, content = [], [], []
            for e in root:
                if e.tag == tag("style"): css += rules(e.text)
                elif e.tag == tag("defs"): defs += list(e)
                elif e.tag != tag("title"): content.append(e)
            self.parts[name] = (css, defs, content)
            for sel, body in css: self.add_rule(sel, body, name)
            for d in defs: self.add_def(d, name)
        return self.parts[name]

    def add_rule(self, sel, body, source):
        if sel in self.style and self.style[sel][0] != body:
            self.errors.append(f"{sel} differs between {self.style[sel][1]} and {source}")
        self.style.setdefault(sel, (body, source))

    def add_def(self, d, source):
        i = d.get("id"); markup = ET.tostring(d, encoding="unicode")
        if i in self.defs and self.defs[i][0] != markup:
            self.errors.append(f"definition #{i} differs between {self.defs[i][2]} and {source}")
        self.defs.setdefault(i, (markup, d, source))

    # ---------------------------------------------------------------- transforms
    def frame(self, node):
        f = node.get("frame")
        if not f: return None
        fr = self.doc["frames"][f]
        return fr["scale"], fr["about"]

    def transform(self, node):
        """The SVG transform of a placement: the frame, then the anchor, the rotation, the scale and the mirror."""
        t = []
        fr = self.frame(node)
        if fr and fr[0] != 1.0:
            s, (cx, cy) = fr
            t.append(f"translate({fmt(cx)} {fmt(cy)}) scale({fmt(s)}) translate({fmt(-cx)} {fmt(-cy)})")
        x, y = node["at"]
        t.append(f"translate({fmt(x)} {fmt(y)})")
        if node.get("rotate"): t.append(f"rotate({fmt(node['rotate'])})")
        if node.get("scale", 1.0) != 1.0: t.append(f"scale({fmt(node['scale'])})")
        if node.get("mirror"): t.append("scale(-1 1)")
        return " ".join(t)

    def to_world(self, node, p):
        """A point in a part's coordinates, carried to the drawing's."""
        x, y = p
        if node.get("mirror"): x = -x
        s = node.get("scale", 1.0); x, y = x * s, y * s
        a = math.radians(node.get("rotate", 0.0))
        x, y = x * math.cos(a) - y * math.sin(a), x * math.sin(a) + y * math.cos(a)
        x, y = x + node["at"][0], y + node["at"][1]
        return self.framed(node, (x, y))

    def framed(self, node, p):
        fr = self.frame(node)
        if not fr: return p
        s, (cx, cy) = fr
        return cx + (p[0] - cx) * s, cy + (p[1] - cy) * s

    # ---------------------------------------------------------------- tree
    def collect(self, node):
        if "part" in node:
            self.placed[node["id"]] = node
        for k in node.get("children", []): self.collect(k)

    def group(self, node, parent):
        g = ET.SubElement(parent, tag("g"))
        name = node.get("group") or node.get("strip") or node.get("id")
        g.set("id", name)
        if node.get("label"): g.set(LABEL, node["label"])
        if node.get("layer"): g.set(MODE, "layer")
        for k, v in node.get("attrs", {}).items(): g.set(k, v)
        return g

    def build(self, node, parent):
        if "part" in node:
            g = self.group(node, parent)
            g.set("transform", self.transform(node))
            texts = node.get("text", {})
            for e in self.part(node["part"])[2]:
                e = copy.deepcopy(e)
                for x in e.iter():
                    name = x.attrib.pop("data-name", None)
                    if name is None: continue
                    # an element a part names gets the placement's id in front, so every copy of the part stays addressable
                    x.set("id", f"{node['id']}-{name}")
                    if name in texts: x.text = texts[name]
                g.append(e)
            return
        if "strip" in node:
            self.strip(node, self.group(node, parent)); return
        if "lamp" in node:
            self.lamp(node, parent); return
        g = self.group(node, parent)
        for k in node.get("children", []): self.build(k, g)

    def strip(self, node, g):
        g.set("data-firmware", node["firmware"]); g.set("data-chain", node["chain"])
        runs, total = [], 0.0
        for seg in node["route"]:
            pts = [self.framed(node, p) for p in seg["points"]]
            for a, b in zip(pts, pts[1:]):
                n = math.dist(a, b)
                if n > 0: runs.append((total, n, a, b, seg.get("hidden", False))); total += n
        hidden, gaps = [], []
        for i in range(node["count"]):
            s = (i + 0.5) * node["pitch"]
            run = next((r for r in runs if s <= r[0] + r[1]), None)
            beyond = run is None
            if beyond: run = runs[-1]
            t = (s - run[0]) / run[1]
            x, y = run[2][0] + (run[3][0] - run[2][0]) * t, run[2][1] + (run[3][1] - run[2][1]) * t
            c = ET.SubElement(g, tag("circle"))
            c.set("id", f"{node['prefix']}{i}"); c.set("class", node["class"])
            c.set("cx", fmt(x)); c.set("cy", fmt(y)); c.set("r", fmt(node["r"]))
            self.points[c.get("id")] = (x, y)
            if run[4] or beyond:
                c.set("style", "display:none"); hidden.append(i)
        for i in hidden:
            if gaps and gaps[-1][0] + gaps[-1][1] == i: gaps[-1][1] += 1
            else: gaps.append([i, 1])
        length = node["count"] * node["pitch"]
        self.report.append(f"{node['strip']}: {node['count']} LEDs, {fmt(length)} mm of strip on a {fmt(total)} mm route; "
                           f"hidden stretches as first LED and count: {gaps}")
        if length > total: self.errors.append(f"{node['strip']}: the strip runs {fmt(length - total)} mm past its route")

    def lamp(self, node, parent):
        owner = self.placed.get(node["on"])
        if owner is None:
            self.errors.append(f"{node['lamp']} sits on {node['on']}, which no placement names"); return
        x, y = self.to_world(owner, node["at"])
        c = ET.SubElement(parent, tag("circle"))
        c.set("id", node["lamp"]); c.set("class", node["class"])
        c.set("cx", fmt(x)); c.set("cy", fmt(y)); c.set("r", fmt(node["r"]))

    # ---------------------------------------------------------------- feature bubbles
    BUBBLE_R = 5.5             # mm
    BUBBLE_GAP = 13.0          # mm between two bubbles of one column
    CLUSTER = 60.0             # mm: places of one feature further apart than this get bubbles of their own

    def features(self, svg):
        """A layer of numbered bubbles in the margins, each with lines to the parts that show its feature."""
        vb = self.doc["viewBox"]
        left_x = vb[0] + 12.0
        right_x = vb[0] + vb[2] - 12.0
        middle = vb[0] + vb[2] / 2
        cols = {"left": [], "right": []}
        for f in self.doc.get("features", []):
            pts = []
            for t in f["targets"]:
                if t not in self.points: self.errors.append(f"feature {f['n']} points at {t}, which the drawing does not hold")
                else: pts.append(self.points[t])
            # A feature gets a bubble for each side it has places on, so no line crosses the playfield, and a further
            # bubble wherever its places on one side lie far apart, so every line stays short.
            for side in ("left", "right"):
                near = sorted((p for p in pts if (p[0] < middle) == (side == "left")), key=lambda p: p[1])
                groups = []
                for p in near:
                    if groups and p[1] - groups[-1][0][1] <= self.CLUSTER: groups[-1].append(p)
                    else: groups.append([p])
                for grp in groups:
                    cols[side].append(dict(f=f, pts=grp, y=sum(p[1] for p in grp) / len(grp)))
        layer = ET.SubElement(svg, tag("g"))
        layer.set("id", "features"); layer.set(LABEL, "Feature bubbles"); layer.set(MODE, "layer")
        for side, items in cols.items():
            items.sort(key=lambda it: it["y"])
            # bubbles keep their gap: pushed down where they crowd, then back up where they ran past the bottom
            for k in range(1, len(items)):
                items[k]["y"] = max(items[k]["y"], items[k - 1]["y"] + self.BUBBLE_GAP)
            bottom = vb[1] + vb[3] - self.BUBBLE_R - 2
            for k in range(len(items) - 1, -1, -1):
                limit = bottom if k == len(items) - 1 else items[k + 1]["y"] - self.BUBBLE_GAP
                items[k]["y"] = min(items[k]["y"], limit)
            x = left_x if side == "left" else right_x
            seen = {}
            for it in items:
                n = it["f"]["n"]; seen[n] = seen.get(n, 0) + 1
                g = ET.SubElement(layer, tag("g"))
                g.set("id", f"feature-{n}-{side}-{seen[n]}"); g.set(LABEL, it["f"]["feature"])
                for px, py in it["pts"]:
                    d = math.dist((x, it["y"]), (px, py))
                    sx = x + (px - x) * self.BUBBLE_R / d; sy = it["y"] + (py - it["y"]) * self.BUBBLE_R / d
                    for cls in ("feature-leader-halo", "feature-leader"):
                        ln = ET.SubElement(g, tag("line")); ln.set("class", cls)
                        ln.set("x1", fmt(sx)); ln.set("y1", fmt(sy)); ln.set("x2", fmt(px)); ln.set("y2", fmt(py))
                    dot = ET.SubElement(g, tag("circle")); dot.set("class", "feature-dot")
                    dot.set("cx", fmt(px)); dot.set("cy", fmt(py)); dot.set("r", "1.2")
                c = ET.SubElement(g, tag("circle")); c.set("class", "feature-bubble")
                c.set("cx", fmt(x)); c.set("cy", fmt(it["y"])); c.set("r", fmt(self.BUBBLE_R))
                t = ET.SubElement(g, tag("text")); t.set("class", "feature-number")
                t.set("x", fmt(x)); t.set("y", fmt(it["y"] + 2.3)); t.text = str(it["f"]["n"])
        style = svg.find(tag("style"))
        style.text += "".join(f"  {r}\n" for r in self.doc.get("feature_style", []))
        self.windows(svg)

    WINDOW_SHADE = "#4d4d4d"   # what is left of a part inside a window: 30 % of its opacity

    def windows(self, svg):
        """Round windows that look through the parts above a feature: one mask per part, holding every window it lies under."""
        holes, rings = {}, []
        for w in self.doc.get("feature_windows", []):
            pts = [self.points[t] for t in w["around"] if t in self.points]
            missing = [t for t in w["around"] if t not in self.points]
            if missing: self.errors.append(f"a window looks at {missing}, which the drawing does not hold")
            if not pts: continue
            c = (sum(p[0] for p in pts) / len(pts), sum(p[1] for p in pts) / len(pts))
            r = max(math.dist(c, p) for p in pts) + w["margin"]
            rings.append((c, r))
            for pid in w["through"]:
                if pid not in self.placed: self.errors.append(f"a window cuts through {pid}, which no placement names"); continue
                holes.setdefault(pid, []).append((self.to_local(self.placed[pid], c), r / self.placed[pid].get("scale", 1.0)))
        defs, style = svg.find(tag("defs")), svg.find(tag("style"))
        for pid, circles in holes.items():
            m = ET.SubElement(defs, tag("mask")); m.set("id", f"window-{pid}"); m.set("maskUnits", "userSpaceOnUse")
            for k, v in (("x", "-2000"), ("y", "-2000"), ("width", "4000"), ("height", "4000")): m.set(k, v)
            full = ET.SubElement(m, tag("rect"))
            for k, v in (("x", "-2000"), ("y", "-2000"), ("width", "4000"), ("height", "4000"), ("fill", "#ffffff")): full.set(k, v)
            for (x, y), r in circles:
                c = ET.SubElement(m, tag("circle"))
                c.set("cx", fmt(x)); c.set("cy", fmt(y)); c.set("r", fmt(r)); c.set("fill", self.WINDOW_SHADE)
            style.text += f"  #{pid} {{ mask: url(#window-{pid}); }}\n"
        # a thin ring marks each window as a cutout, over everything the window looks through
        rims = ET.Element(tag("g")); rims.set("id", "feature-windows"); rims.set(LABEL, "Feature windows")
        for c, r in rings:
            ring = ET.SubElement(rims, tag("circle")); ring.set("class", "feature-window-rim")
            ring.set("cx", fmt(c[0])); ring.set("cy", fmt(c[1])); ring.set("r", fmt(r))
        if rings:
            layer = svg.find(f"{tag('g')}[@id='features']")
            layer.insert(0, rims)

    def to_local(self, node, p):
        """A point of the drawing, carried back into a placement's coordinates."""
        x, y = p
        fr = self.frame(node)
        if fr:
            s, (cx, cy) = fr; x, y = cx + (x - cx) / s, cy + (y - cy) / s
        x, y = x - node["at"][0], y - node["at"][1]
        a = -math.radians(node.get("rotate", 0.0))
        x, y = x * math.cos(a) - y * math.sin(a), x * math.sin(a) + y * math.cos(a)
        s = node.get("scale", 1.0); x, y = x / s, y / s
        if node.get("mirror"): x = -x
        return x, y

    # ---------------------------------------------------------------- document
    def compose(self):
        """The drawing, and the drawing with the feature bubbles over it."""
        doc = self.doc
        for n in doc["layers"]: self.collect(n)
        body = ET.Element("body")
        for n in doc["layers"]: self.build(n, body)
        for r in doc.get("style", []):
            for sel, decl in rules(r): self.add_rule(sel, decl, "playfield.json")
        wrap = f'<svg xmlns="{SVG_NS}">' + "".join(doc.get("defs", [])) + "</svg>"
        for d in ET.fromstring(wrap): self.add_def(d, "playfield.json")

        vb = doc["viewBox"]
        svg = ET.Element(tag("svg"))
        svg.set("viewBox", " ".join(fmt(v) for v in vb))
        svg.set("width", f"{fmt(vb[2])}mm"); svg.set("height", f"{fmt(vb[3])}mm")
        ET.SubElement(svg, tag("title")).text = doc["title"]
        style = ET.SubElement(svg, tag("style"))
        style.text = "\n" + "".join(f"  {s} {{ {b} }}\n" for s, (b, _) in self.style.items())
        defs = ET.SubElement(svg, tag("defs"))
        for _, d, _ in self.defs.values(): defs.append(d)
        for e in body: svg.append(e)
        for node in self.placed.values():
            self.points[node["id"]] = self.to_world(node, (0.0, 0.0))
        bubbles = copy.deepcopy(svg)
        self.features(bubbles)
        return self.text(svg), self.text(bubbles)

    @staticmethod
    def text(svg):
        ET.indent(svg, space=" ")
        text = ET.tostring(svg, encoding="unicode")
        if "xmlns:inkscape" not in text.split(">", 1)[0]:
            text = text.replace("<svg ", f'<svg xmlns:inkscape="{INK_NS}" ', 1)
        return '<?xml version="1.0" encoding="UTF-8"?>\n' + text + "\n"


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--check", action="store_true", help="compare with the drawing instead of writing it")
    ap.add_argument("--json", default=str(ASSETS / "playfield.json"))
    ap.add_argument("--out", default=str(ASSETS / "playfield.svg"))
    ap.add_argument("--features", default=str(ASSETS / "features.svg"), help="the drawing with the feature bubbles")
    a = ap.parse_args()
    doc = json.loads(Path(a.json).read_text(encoding="utf-8"))
    c = Composer(doc, Path(a.json).parent / "parts")
    outputs = dict(zip((Path(a.out), Path(a.features)), c.compose()))
    for line in c.report: print(line)
    for line in c.errors: print("error:", line)
    if a.check:
        stale = [p for p, text in outputs.items() if not p.exists() or p.read_text(encoding="utf-8") != text]
        for p in stale: print(f"{p} differs from what the parts compose")
        if not stale: print("the drawings match their parts")
        return 0 if not stale and not c.errors else 1
    for p, text in outputs.items():
        p.write_text(text, encoding="utf-8", newline="\n")
        print(f"wrote {p}")
    print(f"{len(c.placed)} placements from {len(c.parts)} part files, {len(doc.get('features', []))} feature bubbles")
    return 1 if c.errors else 0


if __name__ == "__main__":
    sys.exit(main())
