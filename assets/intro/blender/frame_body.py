"""Cuts the parts the diorama builds in 3D out of the frame's picture, and traces the crown's outline.

  python frame_body.py <out dir>          (system Python, after extract.py)

The towers' tops above their red boxes and the crown above the frame's top bar leave the picture; the rest is
written as backbox-frame-body.png. The crown's outline, in drawing millimetres (x, depth below the elevation's
top), goes to crown.json for scene.py.
"""
import json, os, sys
import numpy as np
from PIL import Image

PX = 6.0                    # pixels a millimetre in the layer images
X0 = -52.0                  # the drawing's x at the images' left edge
BAR = 58.5                  # mm below the elevation's top: the frame's top bar, where the crown stands
TOWER_TOP = 79.0            # mm below the elevation's top: the top of the towers' red boxes
TOWER_CENTRES = (-15.6, 245.6)              # the towers' middles in the drawing


def main():
    out = sys.argv[1]
    img = Image.open(os.path.join(out, "backbox-frame.png")).convert("RGBA")
    a = np.array(img)
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
    a[: row(BAR), col(20.0): col(210.0), 3] = 0
    for xc in TOWER_CENTRES:
        # the tower's top above the bar, then its cover beside the frame's star band
        a[: row(BAR), col(xc - 25.0): col(xc + 25.0), 3] = 0
        inner = 21.1 if xc < 115 else -21.1
        lo, hi = sorted((xc - 29.0 * (1 if xc < 115 else -1), xc + inner))
        a[row(BAR): row(TOWER_TOP), col(lo): col(hi), 3] = 0
    Image.fromarray(a).save(os.path.join(out, "backbox-frame-body.png"))
    json.dump({"outline": outline, "bar": BAR}, open(os.path.join(out, "crown.json"), "w"))
    print("crown outline", len(outline), "points")


if __name__ == "__main__":
    main()
