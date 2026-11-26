"""Turns the rendered frames into the display's video file and a preview.

  python assets/intro/blender/convert.py <frames dir> <out dir>

Each PNG is scaled to 320 x 240 and quantized to RGB565 with a 4 x 4 ordered dither. intro.rgb holds the
frames one after another, two bytes a pixel, high byte first, as assets/media.md gives for a video.
preview.gif shows the same quantized frames at 25 frames a second, at twice the size.
"""
import os, sys
import numpy as np
from PIL import Image

BAYER = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]], np.float32) / 16 - 0.5


def quantize(img):
    a = np.asarray(img.convert("RGB").resize((320, 240), Image.LANCZOS)).astype(np.float32)
    d = np.tile(BAYER, (60, 80))[..., None]
    steps = np.array([31, 63, 31], np.float32)
    q = np.clip(np.floor(a / 255 * steps + 0.5 + d), 0, steps).astype(np.uint16)
    word = (q[..., 0] << 11) | (q[..., 1] << 5) | q[..., 2]
    back = (q.astype(np.float32) / steps * 255 + 0.5).astype(np.uint8)
    return word.astype(">u2").tobytes(), Image.fromarray(back)


def main():
    src, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    names = sorted(n for n in os.listdir(src) if n.endswith(".png"))
    previews = []
    with open(os.path.join(out, "intro.rgb"), "wb") as f:
        for n in names:
            raw, shown = quantize(Image.open(os.path.join(src, n)))
            f.write(raw); previews.append(shown.resize((640, 480), Image.NEAREST))
    previews[0].save(os.path.join(out, "preview.gif"), save_all=True, append_images=previews[1:], duration=40, loop=0)
    print(len(names), "frames,", os.path.getsize(os.path.join(out, "intro.rgb")), "bytes")
    # the sound is mixed apart from the picture; a track older than its script or the film's paths is stale
    here = os.path.dirname(os.path.abspath(__file__))
    wav = os.path.join(out, "intro.wav")
    newer = [p for p in (os.path.join(here, "soundtrack.py"), os.path.join(here, "out", "ball-paths.json"))
             if not os.path.exists(wav) or os.path.getmtime(p) > os.path.getmtime(wav)]
    if newer:
        print("intro.wav is older than", ", ".join(os.path.basename(p) for p in newer), "and needs soundtrack.py again")


if __name__ == "__main__":
    main()
