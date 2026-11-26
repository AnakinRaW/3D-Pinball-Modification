"""Draws what the diorama's display shows during the trailer, one 320 x 240 PNG a frame, in the intro's look.

  python assets/trailer/blender/display.py        (system Python, after trailer.py)

trailer.py writes the screen's content for each frame to out/display.json: the score and the line above it. A
circus sunburst turns behind that line, a ring of bulbs chases round the border, and the score sits in a dark box
at the bottom, as the intro's display.py draws them. A feature's line flashes gold and red and pulses in size;
the screen is black before the game starts and after the trail has hit it. The frames go to ../display/.
"""
import json, math, os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(os.path.dirname(HERE), "display")
W, H = 320, 240
BOLD = "C:/Windows/Fonts/ariblk.ttf"
DIGITS = "C:/Windows/Fonts/consolab.ttf"
QUIET = ("BALL 1",)          # lines that stand still; every other line is a callout that flashes


def frame(f, score, line):
    img = Image.new("RGB", (W, H), (14, 6, 26))
    d = ImageDraw.Draw(img)
    # the sunburst: red and dark purple rays turning about a point below the line
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
    # the line: a callout flashes gold and red and pulses in size, fitted to the screen's width
    if line in QUIET:
        font = ImageFont.truetype(BOLD, 26)
        bw = d.textlength(line, font=font)
        d.text(((W - bw) / 2, 58), line, font=font, fill=(255, 214, 64), stroke_width=2, stroke_fill=(120, 10, 30))
    else:
        size = 38 + (3 if (f // 3) % 2 == 0 else 0)
        font = ImageFont.truetype(BOLD, size)
        while d.textlength(line, font=font) > W - 44 and size > 16:
            size -= 1
            font = ImageFont.truetype(BOLD, size)
        fill, edge = ((255, 214, 64), (200, 20, 40)) if (f // 4) % 2 == 0 else ((255, 80, 90), (255, 214, 64))
        bw = d.textlength(line, font=font)
        d.text(((W - bw) / 2, 70 - size / 2), line, font=font, fill=fill, stroke_width=4, stroke_fill=edge)
    # the score in a dark box at the bottom
    d.rounded_rectangle([40, 168, W - 40, 212], radius=8, fill=(8, 4, 14), outline=(255, 214, 64), width=2)
    font = ImageFont.truetype(DIGITS, 30)
    text = f"{score:,}"
    bw = d.textlength(text, font=font)
    d.text(((W - bw) / 2, 172), text, font=font, fill=(120, 255, 230))
    return img


def main():
    spec = json.load(open(os.path.join(HERE, "out", "display.json")))
    os.makedirs(OUT, exist_ok=True)
    black = Image.new("RGB", (W, H), (0, 0, 0))
    shown = {int(f): v for f, v in spec["frames"].items()}
    for f in range(1, spec["last"] + 1):
        img = frame(f, *shown[f]) if f in shown else black
        img.save(os.path.join(OUT, f"d{f:04d}.png"))


if __name__ == "__main__":
    main()
