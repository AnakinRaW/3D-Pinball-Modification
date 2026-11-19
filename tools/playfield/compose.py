"""Composes the playfield drawing from its part files.

assets/playfield/playfield.json holds the drawing's tree: layers, groups, where each part sits, the LED strips and the single
lamps. Each file in assets/playfield/parts/ is one physical part, drawn around its anchor with the styles and definitions it
needs, so it also opens alone. This script places every part, lays the LEDs of each strip along its route at the strip's
pitch, sets each lamp onto its part and writes assets/playfield/playfield.svg.

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

    # ---------------------------------------------------------------- document
    def compose(self):
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
        ET.indent(svg, space=" ")
        text = ET.tostring(svg, encoding="unicode")
        if f"xmlns:inkscape" not in text.split(">", 1)[0]:
            text = text.replace("<svg ", f'<svg xmlns:inkscape="{INK_NS}" ', 1)
        return '<?xml version="1.0" encoding="UTF-8"?>\n' + text + "\n"


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--check", action="store_true", help="compare with the drawing instead of writing it")
    ap.add_argument("--json", default=str(ASSETS / "playfield.json"))
    ap.add_argument("--out", default=str(ASSETS / "playfield.svg"))
    a = ap.parse_args()
    doc = json.loads(Path(a.json).read_text(encoding="utf-8"))
    c = Composer(doc, Path(a.json).parent / "parts")
    text = c.compose()
    for line in c.report: print(line)
    for line in c.errors: print("error:", line)
    out = Path(a.out)
    if a.check:
        same = out.exists() and out.read_text(encoding="utf-8") == text
        print("the drawing matches its parts" if same else f"{out} differs from what the parts compose")
        return 0 if same and not c.errors else 1
    out.write_text(text, encoding="utf-8", newline="\n")
    print(f"wrote {out}: {len(c.placed)} placements from {len(c.parts)} part files")
    return 1 if c.errors else 0


if __name__ == "__main__":
    sys.exit(main())
