"""Draws what the diorama's display shows during the film, one 320 x 240 PNG a frame.

  python assets/intro/blender/display.py <out dir>

A circus sunburst turns behind a bold MULTIBALL that flashes once the announcer calls it, and the score counts
up, jumping at each hit of the film's ride.
"""
import math, os, sys
from PIL import Image, ImageDraw, ImageFont

W, H, FRAMES = 320, 240, 235
BOLD = "C:/Windows/Fonts/ariblk.ttf"
DIGITS = "C:/Windows/Fonts/consolab.ttf"

# the score: a start value, points a frame while the ball rolls, and jumps at the hits
START = 1_250_000
JUMPS = {38: 5_000, 71: 25_000, 97: 50_000}


def score_at(f):
    s = START + 130 * f
    for at, pts in JUMPS.items():
        if f >= at:
            s += pts * min(1.0, (f - at + 1) / 6)          # a jump counts up over six frames
    if f >= 74:
        s += 1_000 * min(f - 74, 26)                        # the lit stars score while they glow
    return int(s) // 10 * 10


def frame(f):
    img = Image.new("RGB", (W, H), (14, 6, 26))
    d = ImageDraw.Draw(img)
    # the sunburst: red and dark purple rays turning about a point below the title
    cx, cy, rays = W / 2, 112, 18
    turn = f * 1.6
    for k in range(rays):
        a0 = math.radians(turn + 360 / rays * k); a1 = math.radians(turn + 360 / rays * (k + 0.5))
        col = (120, 18, 40) if k % 2 == 0 else (44, 10, 70)
        d.polygon([(cx, cy), (cx + 400 * math.cos(a0), cy + 400 * math.sin(a0)), (cx + 400 * math.cos(a1), cy + 400 * math.sin(a1))], fill=col)
    # a ring of bulbs that chase around the border
    for k in range(32):
        t = k / 32
        x = 8 + (W - 16) * (t * 4 if t < 0.25 else 1 if t < 0.5 else (1 - (t - 0.5) * 4) if t < 0.75 else 0)
        y = 8 + (H - 16) * (0 if t < 0.25 else (t - 0.25) * 4 if t < 0.5 else 1 if t < 0.75 else 1 - (t - 0.75) * 4)
        on = (k + f // 2) % 4 == 0
        d.ellipse([x - 3, y - 3, x + 3, y + 3], fill=(255, 214, 90) if on else (90, 60, 30))
    # MULTIBALL from the announcer's call on: flashes gold and red, pulses in size
    if f >= 22:
        size = 38 + (3 if (f // 3) % 2 == 0 else 0)
        font = ImageFont.truetype(BOLD, size)
        text = "MULTIBALL"
        fill, edge = ((255, 214, 64), (200, 20, 40)) if (f // 4) % 2 == 0 else ((255, 80, 90), (255, 214, 64))
        bw = d.textlength(text, font=font)
        d.text(((W - bw) / 2, 70 - size / 2), text, font=font, fill=fill, stroke_width=4, stroke_fill=edge)
    else:
        font = ImageFont.truetype(BOLD, 26)
        text = "BALL 1"
        bw = d.textlength(text, font=font)
        d.text(((W - bw) / 2, 58), text, font=font, fill=(255, 214, 64), stroke_width=2, stroke_fill=(120, 10, 30))
    # the score in a dark box at the bottom
    d.rounded_rectangle([40, 168, W - 40, 212], radius=8, fill=(8, 4, 14), outline=(255, 214, 64), width=2)
    font = ImageFont.truetype(DIGITS, 30)
    text = f"{score_at(f):,}"
    bw = d.textlength(text, font=font)
    d.text(((W - bw) / 2, 172), text, font=font, fill=(120, 255, 230))
    return img


def main():
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    for f in range(1, FRAMES + 1):
        frame(f).save(os.path.join(out, f"d{f:04d}.png"))


if __name__ == "__main__":
    main()
