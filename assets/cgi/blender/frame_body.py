"""Traces the crown's outline out of the frame's picture and paints the board in the back of the diorama.

  python frame_body.py <out dir>          (system Python, after extract.py)

The crown's outline, in drawing millimetres (x, depth below the elevation's top), goes to crown.json for
scene.py, the board to backbox-circus.png.
"""
import json, os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont

PX = 6.0                    # pixels a millimetre in the layer images
X0 = -52.0                  # the drawing's x at the images' left edge
BAR = 58.5                  # mm below the elevation's top: the frame's top bar, where the crown stands
CIRCUS_MM = (189.6, 97.8)                   # the back board's size, the casing's inside between the towers
CIRCUS_FONTS = ("georgiab.ttf", "DejaVuSerif-Bold.ttf")     # a bold serif for its letters, the first one found


def main():
    out = sys.argv[1]
    a = np.array(Image.open(os.path.join(out, "backbox-frame.png")).convert("RGBA"))
    col = lambda x: int(round((x - X0) * PX))
    row = lambda d: int(round(d * PX))
    # the crown: every opaque pixel above the bar between the towers
    crown = a[: row(BAR), col(20.0): col(210.0), 3] > 20
    outline_l, outline_r = [], []
    for r in range(row(4.0), row(BAR) + 1, 3):
        xs = np.nonzero(crown[min(r, crown.shape[0] - 1)])[0]
        if len(xs):
            outline_l.append((round(xs.min() / PX + 20.0, 2), round(r / PX, 2)))
            outline_r.append((round(xs.max() / PX + 20.0, 2), round(r / PX, 2)))
    outline = outline_l + outline_r[::-1]
    json.dump({"outline": outline, "bar": BAR}, open(os.path.join(out, "crown.json"), "w"))
    print("crown outline", len(outline), "points")
    circus_board(out)


def circus_board(out):
    """The board in the back of the diorama's casing, where the front has the diorama: dark brown wood, CIRCUS in
    light brown letters, gold ornaments in its corners and at the middle of its top and bottom edges. Written as
    backbox-circus.png, CIRCUS_MM in millimetres at PX pixels a millimetre."""
    w, h = round(CIRCUS_MM[0] * PX), round(CIRCUS_MM[1] * PX)
    img = Image.new("RGB", (w, h), "#3b2415")
    d = ImageDraw.Draw(img)
    gold, line = "#d4a640", max(2, round(0.8 * PX))
    m = round(6 * PX)                                   # the ornaments' distance from the edges
    for sx, sy in ((1, 1), (-1, 1), (1, -1), (-1, -1)):
        cx = m if sx > 0 else w - m
        cy = m if sy > 0 else h - m
        r = round(9 * PX)
        # a corner: two arms along the edges, curling inward at their ends, a dot in the corner
        d.line([(cx, cy), (cx + sx * 3 * r, cy)], fill=gold, width=line)
        d.line([(cx, cy), (cx, cy + sy * 2 * r)], fill=gold, width=line)
        # each arm ends in a small ring
        for ex, ey in ((cx + sx * 3 * r, cy), (cx, cy + sy * 2 * r)):
            bx0, by0 = (ex - r // 3, ey) if ey == cy else (ex, ey - r // 3)
            d.ellipse([bx0 - r // 3, by0 - r // 3 + (sy * r // 3 if ey == cy else 0), bx0 + r // 3, by0 + r // 3 + (sy * r // 3 if ey == cy else 0)], outline=gold, width=line)
        d.ellipse([cx - r // 4, cy - r // 4, cx + r // 4, cy + r // 4], fill=gold)
    for cy, sy in ((m, 1), (h - m, -1)):
        # the middle of the top and the bottom edge: a diamond between two scrolls
        cx, r = w // 2, round(7 * PX)
        d.polygon([(cx, cy - r // 2), (cx + r // 2, cy), (cx, cy + r // 2), (cx - r // 2, cy)], fill=gold)
        for sx in (1, -1):
            d.arc([cx + sx * r // 2 - (r if sx < 0 else 0), cy - r // 2, cx + sx * r // 2 + (r if sx > 0 else 0), cy + r // 2],
                  0 if sy > 0 else 180, 180 if sy > 0 else 360, fill=gold, width=line)
            d.line([(cx + sx * (r // 2 + r), cy), (cx + sx * (r // 2 + 3 * r), cy)], fill=gold, width=line)
    font = None
    for name in CIRCUS_FONTS:
        try:
            font = ImageFont.truetype(name, round(h * 0.42)); break
        except OSError:
            continue
    font = font or ImageFont.load_default()
    d.text((w / 2, h / 2), "CIRCUS", fill="#c99a62", font=font, anchor="mm")
    img.save(os.path.join(out, "backbox-circus.png"))


if __name__ == "__main__":
    main()
