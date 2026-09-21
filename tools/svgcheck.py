#!/usr/bin/env python3
"""Geometry check for the hand-written schematic SVGs.

A drawing that renders is not a drawing that reads. Text that runs past the
viewBox is cut off, and two labels on the same spot are unreadable however
correct their content is. Neither shows up in a diff and neither shows up in
figcheck, which only compares values.

What it reports, per file:

  clipped     a label whose box leaves the viewBox on any side
  overruns    a label that starts inside a box and leaves it, which is what
              a board outline or a module block does to a caption
  overlap     two labels whose boxes intersect
  clips       two boxes that cross, where one of them is an outline. A zone
              drawn into another zone is the case; a filled box over a filled
              box is left alone, a sketch stacks parts on purpose

Widths are estimated from the character classes of a sans-serif face, so the
boxes are approximate. The tolerance below keeps that estimate from crying
wolf on labels that merely touch.

    python tools/svgcheck.py                       every SVG under docs/
    python tools/svgcheck.py path/to/one.svg ...   only those
    python tools/svgcheck.py --slack 4             a wider tolerance
    python tools/svgcheck.py --margin 8            clearance a label is owed from a shape
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Advance widths as a fraction of the font size, for a Helvetica-like face.
# Three classes are enough: the estimate only has to be good to a few per cent.
WIDE = set("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789@#%&ΩµΔ")
THIN = set(" .,:;'`!|iljtfr()[]-")


def text_width(s, size):
    w = 0.0
    for ch in s:
        w += 0.30 if ch in THIN else 0.68 if ch in WIDE else 0.52
    return w * size


ENTITY = {"&#937;": "Ω", "&#8230;": "…", "&#181;": "µ", "&#8722;": "-",
          "&amp;": "&", "&lt;": "<", "&gt;": ">", "&#176;": "°", "&#8804;": "≤",
          "&#8805;": "≥", "&#215;": "×", "&#8594;": "→", "&#8730;": "√"}


def unescape(s):
    for k, v in ENTITY.items():
        s = s.replace(k, v)
    return re.sub(r"&#\d+;", "?", s)


def attr(tag, name, default=None):
    # The boundary has to be whitespace or the tag start. \b would let "width"
    # match inside "stroke-width", which gives every box its outline's width.
    m = re.search(r'(?:^|\s)' + name + r'="([^"]*)"', tag)
    return m.group(1) if m else default


def boxes(path):
    """Every text label in the file, as (line, text, x0, y0, x1, y1, rotated)."""
    src = path.read_text(encoding="utf-8")
    out = []
    for m in re.finditer(r"<text\b([^>]*)>(.*?)</text>", src, re.S):
        tag, body = m.group(1), m.group(2)
        line = src.count("\n", 0, m.start()) + 1
        # a tspan-bearing label is measured on its flattened text
        txt = unescape(re.sub(r"<[^>]+>", "", body)).strip()
        if not txt:
            continue
        try:
            x = float(attr(tag, "x", "0"))
            y = float(attr(tag, "y", "0"))
        except ValueError:
            continue
        size = float(attr(tag, "font-size", "12"))
        w = text_width(txt, size)
        anchor = attr(tag, "text-anchor", "start")
        x0 = x - w / 2 if anchor == "middle" else x - w if anchor == "end" else x
        rotated = "rotate" in (attr(tag, "transform") or "")
        out.append(dict(line=line, txt=txt, x0=x0, y0=y - 0.78 * size,
                        x1=x0 + w, y1=y + 0.22 * size, rot=rotated, size=size))
    return src, out


def rects(src):
    """Every rectangle, smallest last, so the tightest enclosing one wins."""
    out = []
    for m in re.finditer(r"<rect\s([^>]*)>", src):
        tag = m.group(1)
        try:
            x = float(attr(tag, "x", "0")); y = float(attr(tag, "y", "0"))
            w = float(attr(tag, "width")); h = float(attr(tag, "height"))
        except (TypeError, ValueError):
            continue
        hollow = (attr(tag, "fill", "") or "").strip() in ("none", "")
        out.append((w * h, x, y, x + w, y + h,
                    src.count(chr(10), 0, m.start()) + 1, hollow))
    return sorted(out, reverse=True)


NUM = re.compile(r"-?\d+(?:\.\d+)?")

# How many numbers each path command takes, and which of them are a point.
# An arc carries five numbers that are not coordinates at all, and reading them
# as one would put a box from the origin to the endpoint.
PATH_CMD = {"M": (2, [0]), "L": (2, [0]), "T": (2, [0]),
            "C": (6, [0, 2, 4]), "S": (4, [0, 2]), "Q": (4, [0, 2]),
            "A": (7, [5]), "Z": (0, [])}


def path_points(d):
    """The coordinates a path visits, arcs taken at their endpoint only.

    A relative command would need the running position, so a path that has one
    is given up on rather than guessed at.
    """
    if re.search(r"[mlcsqtahv]", d):
        return []
    out, i = [], 0
    toks = re.findall(r"[A-Z]|-?\d+(?:\.\d+)?", d)
    cmd = None
    while i < len(toks):
        if toks[i].isalpha():
            cmd = toks[i].upper()
            i += 1
            continue
        if cmd in ("H", "V") or cmd not in PATH_CMD:
            return []                       # one axis only, or something unhandled
        n, pick = PATH_CMD[cmd]
        if i + n > len(toks):
            break
        nums = [float(v) for v in toks[i:i + n]]
        for k in pick:
            out.append((nums[k], nums[k + 1]))
        i += n
    return out


def shapes(src):
    """Every drawn element as (kind, x0, y0, x1, y1, line).

    Curves are taken at their control points, which is wider than the curve
    itself, so a path reports a box no smaller than what it covers.
    """
    out = []
    for m in re.finditer(r"<(rect|line|circle|ellipse|path|polygon|polyline)\s([^>]*)>", src):
        kind, tag = m.group(1), m.group(2)
        line = src.count(chr(10), 0, m.start()) + 1
        try:
            if kind == "rect":
                x, y = float(attr(tag, "x", "0")), float(attr(tag, "y", "0"))
                box = (x, y, x + float(attr(tag, "width")), y + float(attr(tag, "height")))
            elif kind == "line":
                xs = [float(attr(tag, "x1")), float(attr(tag, "x2"))]
                ys = [float(attr(tag, "y1")), float(attr(tag, "y2"))]
                box = (min(xs), min(ys), max(xs), max(ys))
            elif kind in ("circle", "ellipse"):
                cx, cy = float(attr(tag, "cx", "0")), float(attr(tag, "cy", "0"))
                rx = float(attr(tag, "r") or attr(tag, "rx"))
                ry = float(attr(tag, "r") or attr(tag, "ry"))
                box = (cx - rx, cy - ry, cx + rx, cy + ry)
            elif kind == "path":
                pts = path_points(attr(tag, "d") or "")
                if len(pts) < 2:
                    continue
                box = (min(p[0] for p in pts), min(p[1] for p in pts),
                       max(p[0] for p in pts), max(p[1] for p in pts))
            else:
                pts = [float(v) for v in NUM.findall(attr(tag, "points") or "")]
                if len(pts) < 4:
                    continue
                box = (min(pts[0::2]), min(pts[1::2]), max(pts[0::2]), max(pts[1::2]))
        except (TypeError, ValueError):
            continue
        # A stroke has width, so a hairline still covers a band.
        w = float(attr(tag, "stroke-width", "1") or 1) / 2
        hollow = (attr(tag, "fill", "") or "").strip() in ("none", "")
        out.append((kind, box[0] - w, box[1] - w, box[2] + w, box[3] + w, line, hollow))
    return out


def viewbox(src):
    m = re.search(r'viewBox="([-\d.]+)\s+([-\d.]+)\s+([-\d.]+)\s+([-\d.]+)"', src)
    if not m:
        return None
    a, b, c, d = (float(g) for g in m.groups())
    return a, b, a + c, b + d


def check(path, slack, margin=0.0):
    src, labels = boxes(path)
    vb = viewbox(src)
    bad = []
    if vb:
        vx0, vy0, vx1, vy1 = vb
        for t in labels:
            if t["rot"]:
                continue                       # a rotated box is not this rectangle
            over = []
            if t["x1"] > vx1 + slack:
                over.append(f"{t['x1'] - vx1:.0f} past the right edge")
            if t["x0"] < vx0 - slack:
                over.append(f"{vx0 - t['x0']:.0f} past the left edge")
            if t["y1"] > vy1 + slack:
                over.append(f"{t['y1'] - vy1:.0f} below the bottom")
            if t["y0"] < vy0 - slack:
                over.append(f"{vy0 - t['y0']:.0f} above the top")
            if over:
                bad.append((t["line"], "clipped", t["txt"], ", ".join(over)))

    # A caption that starts inside a rectangle and ends outside it reads as
    # running off the board, whatever the viewBox allows.
    boxes_ = rects(src)
    for t in labels:
        if t["rot"]:
            continue
        cx = (t["x0"] + t["x1"]) / 2, (t["y0"] + t["y1"]) / 2
        for _, rx0, ry0, rx1, ry1, rline, _hollow in reversed(boxes_):
            if not (rx0 <= cx[0] <= rx1 and ry0 <= cx[1] <= ry1):
                continue
            if rx1 - rx0 < t["x1"] - t["x0"] + 2 * slack:
                continue                  # the label was never meant to fit
            if t["x1"] > rx1 + slack or t["x0"] < rx0 - slack:
                side = "right" if t["x1"] > rx1 else "left"
                by = t["x1"] - rx1 if side == "right" else rx0 - t["x0"]
                bad.append((t["line"], "overruns", t["txt"],
                            f"{by:.0f} past the {side} edge of the box on line {rline}"))
            break

    # Anything drawn, not only rectangles. A label over a wire, a resistor body
    # or a pad is as unreadable as one over another label. A shape that holds
    # the whole label is the symbol it was put inside, and a shape that spans
    # most of the drawing is a frame, so neither counts.
    page = 0.0
    if vb:
        page = (vb[2] - vb[0]) * (vb[3] - vb[1])
    drawn = shapes(src)
    for t in labels:
        if t["rot"]:
            continue
        # A filled shape painted after a wire hides it, so a label inside that
        # shape reads cleanly however the wire runs beneath. A resistor value
        # inside its own body, with its net passing through, is the case.
        covers = [ln for _k, x0, y0, x1, y1, ln, hollow in drawn
                  if not hollow and x0 <= t["x0"] and t["x1"] <= x1
                  and y0 <= t["y0"] and t["y1"] <= y1]
        for kind, sx0, sy0, sx1, sy1, sline, hollow in drawn:
            if any(c > sline for c in covers):
                continue
            if (sx1 - sx0) * (sy1 - sy0) > 0.25 * page:
                continue                          # a frame, not a symbol
            if sx0 - slack <= t["x0"] and t["x1"] <= sx1 + slack \
               and sy0 - slack <= t["y0"] and t["y1"] <= sy1 + slack:
                continue                          # the label sits inside it
            # The margin is clearance the label is owed, so the box is grown
            # by it before the overlap is measured. A label that merely comes
            # close to a wire or a symbol is then reported the same way one
            # sitting on it is.
            ox = min(t["x1"] + margin, sx1) - max(t["x0"] - margin, sx0)
            oy = min(t["y1"] + margin, sy1) - max(t["y0"] - margin, sy0)
            # A label placed beside a wire touches it by a pixel or two as a
            # matter of course, so without a margin this wants more room than
            # the edges do. With one, any intersection of the grown box counts:
            # a wire is thinner than the margin, so the overlap with it can
            # never reach that floor however much clearance the label is owed.
            # A label beside a wire touches it by a pixel or two as a matter of
            # course, and the tolerance covers that. It may not be raised above
            # the tolerance: a wire is thinner than that, so the overlap with
            # one can never reach a higher floor however squarely the label
            # sits on it, which is how a label centred on a wire went unseen.
            if ox <= slack or oy <= slack:
                continue
            # Nothing is painted inside an outline, so only its border band can
            # be crossed. A label that merely reaches into an open box is fine.
            if hollow and kind in ("rect", "circle", "ellipse", "polygon"):
                pad = 2.0
                inner = (sx0 + pad, sy0 + pad, sx1 - pad, sy1 - pad)
                if inner[0] <= t["x0"] and t["x1"] <= inner[2] \
                   and inner[1] <= t["y0"] and t["y1"] <= inner[3]:
                    continue
            gap = min(ox, oy) - margin
            how = (f"{ox:.0f} x {oy:.0f} over" if gap > 0
                   else f"{-gap:.0f} short of the {margin:.0f} it is owed from")
            bad.append((t["line"], "on a " + kind, t["txt"],
                        f"{how} the {kind} on line {sline}"))

    for i, a in enumerate(labels):
        if a["rot"]:
            continue
        for b in labels[i + 1:]:
            if b["rot"]:
                continue
            ox = min(a["x1"], b["x1"]) - max(a["x0"], b["x0"])
            oy = min(a["y1"], b["y1"]) - max(a["y0"], b["y0"])
            if ox > slack and oy > slack:
                bad.append((a["line"], "overlap", a["txt"],
                            f"{ox:.0f} x {oy:.0f} with line {b['line']}, {b['txt']!r}"))

    # Two boxes that cross each other. A frame holding its members is what a
    # group outline and a board outline do, so containment passes; a partial
    # overlap is two zones drawn into one another. One of the two has to be
    # hollow: a filled box over a filled box is a part drawn over a part, which
    # is what a mechanical sketch does when a plunger enters a coil.
    def holds(o, i_):
        return (o[1] <= i_[1] + slack and o[2] <= i_[2] + slack
                and o[3] >= i_[3] - slack and o[4] >= i_[4] - slack)

    for i, a in enumerate(boxes_):
        for b in boxes_[i + 1:]:
            ox = min(a[3], b[3]) - max(a[1], b[1])
            oy = min(a[4], b[4]) - max(a[2], b[2])
            if ox <= slack or oy <= slack:
                continue
            if holds(a, b) or holds(b, a):
                continue
            if not (a[6] or b[6]):
                continue
            size = f"{a[3] - a[1]:.0f}x{a[4] - a[2]:.0f}"
            bad.append((a[5], "clips", f"box {size}",
                        f"{ox:.0f} x {oy:.0f} into the box on line {b[5]}"))
    return sorted(bad)


def main():
    # A flag's value is not a file name, so the two are separated here rather
    # than by dropping everything that starts with a dash.
    argv, args = sys.argv[1:], []
    slack, margin, i = 2.0, 0.0, 0
    while i < len(argv):
        a = argv[i]
        if a in ("--slack", "--margin"):
            if i + 1 >= len(argv):
                print(f"{a} needs a number")
                return 2
            if a == "--slack":
                slack = float(argv[i + 1])
            else:
                margin = float(argv[i + 1])
            i += 2
            continue
        if not a.startswith("--"):
            args.append(a)
        i += 1
    files = [Path(a) for a in args] or sorted((ROOT / "docs").rglob("*.svg"))

    total = 0
    for f in files:
        found = check(f, slack, margin)
        total += len(found)
        try:
            rel = f.resolve().relative_to(ROOT)
        except ValueError:
            rel = f
        if found:
            print(f"\n{rel}")
            for line, kind, txt, detail in found:
                print(f"  {line:>5}  {kind:<8} {txt[:44]!r:<48} {detail}")
        else:
            print(f"ok    {rel}")

    print()
    if total:
        print(f"{total} to look at. Widths are estimated, so check the drawing before moving anything.")
    else:
        print("No label leaves its viewBox and none sits on another.")
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main())
