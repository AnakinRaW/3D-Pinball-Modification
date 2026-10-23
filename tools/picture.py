"""Turns PNGs of at most 320 x 240 pixels into picture files for the screen component.

A picture file is in LVGL's image format, as docs/firmware/components/screen.md describes:
a header of 12 bytes, then the pixels row by row in RGB565. An opaque picture holds each
pixel high byte first, LVGL's RGB565_SWAPPED, which the display takes as it is, so LVGL
copies it without converting a pixel. A picture with transparent pixels, of any size,
is RGB565A8: its colors low byte first, then an alpha byte for each pixel.
Floyd-Steinberg dithering spreads the error of every pixel onto its neighbors, so a
gradient keeps its look in RGB565. Each file lands beside its PNG.

    python tools/picture.py background.png lamp.png ...
"""
import pathlib
import struct
import sys
import zlib

WIDTH, HEIGHT = 320, 240
CHANNELS = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}   # PNG color type -> samples per pixel
MAGIC = 0x19                       # LV_IMAGE_HEADER_MAGIC in LVGL's lv_image_dsc.h
RGB565A8, RGB565_SWAPPED = 0x14, 0x1B   # lv_color_format_t in LVGL's lv_color.h


def read_png(path: pathlib.Path):
    """The pixels of an 8-bit, non-interlaced PNG as rows of (r, g, b, a)."""
    raw = path.read_bytes()
    if raw[:8] != b"\x89PNG\r\n\x1a\n":
        sys.exit(f"{path} is no PNG")
    pos, idat, plte, trns = 8, b"", b"", b""
    while pos < len(raw):
        size, kind = struct.unpack(">I4s", raw[pos:pos + 8])
        body = raw[pos + 8:pos + 8 + size]
        pos += size + 12
        if kind == b"IHDR":
            width, height, depth, ctype, _, _, interlace = struct.unpack(">IIBBBBB", body)
        elif kind == b"PLTE":
            plte = body
        elif kind == b"tRNS":
            trns = body
        elif kind == b"IDAT":
            idat += body
        elif kind == b"IEND":
            break
    if depth != 8 or interlace or ctype not in CHANNELS:
        sys.exit(f"{path}: only 8-bit, non-interlaced PNGs are read")
    n = CHANNELS[ctype]
    data, stride = zlib.decompress(idat), width * n
    rows, prev = [], bytearray(stride)
    for y in range(height):
        start = y * (stride + 1)
        ftype, line = data[start], bytearray(data[start + 1:start + 1 + stride])
        for i in range(stride):                            # undo the row's filter
            a = line[i - n] if i >= n else 0
            b = prev[i]
            c = prev[i - n] if i >= n else 0
            if ftype == 1:
                line[i] = (line[i] + a) & 0xFF
            elif ftype == 2:
                line[i] = (line[i] + b) & 0xFF
            elif ftype == 3:
                line[i] = (line[i] + (a + b) // 2) & 0xFF
            elif ftype == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 0xFF
        prev = line
        row = []
        for x in range(width):
            s = line[x * n:(x + 1) * n]
            if ctype == 0:
                row.append((s[0], s[0], s[0], 255))
            elif ctype == 2:
                row.append((s[0], s[1], s[2], 255))
            elif ctype == 3:
                i = s[0]
                row.append((plte[3 * i], plte[3 * i + 1], plte[3 * i + 2], trns[i] if i < len(trns) else 255))
            elif ctype == 4:
                row.append((s[0], s[0], s[0], s[1]))
            else:
                row.append((s[0], s[1], s[2], s[3]))
        rows.append(row)
    return width, height, rows


def expand(v: int):
    """The color an RGB565 value shows, back in 8 bits a channel."""
    r, g, b = v >> 11, (v >> 5) & 0x3F, v & 0x1F
    return (r << 3 | r >> 2, g << 2 | g >> 4, b << 3 | b >> 2)


def dither(width: int, height: int, rows: list):
    """The RGB565 value and the opacity of every pixel, dithered after Floyd-Steinberg."""
    work = [[[float(v) for v in p] for p in row] for row in rows]
    colors, alphas = [], []
    for y in range(height):
        for x in range(width):
            r, g, b, a = (min(255, max(0, round(v))) for v in work[y][x])
            v = (r >> 3) << 11 | (g >> 2) << 5 | b >> 3
            colors.append(v)
            alphas.append(a)
            if a == 0:
                continue                               # a hidden pixel passes no error on
            shown = expand(v)
            err = (r - shown[0], g - shown[1], b - shown[2])
            for dx, dy, part in ((1, 0, 7), (-1, 1, 3), (0, 1, 5), (1, 1, 1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < width and ny < height and work[ny][nx][3] > 0:
                    for k in range(3):
                        work[ny][nx][k] += err[k] * part / 16
    return colors, alphas


def convert(src: pathlib.Path):
    width, height, rows = read_png(src)
    if not (0 < width <= WIDTH and 0 < height <= HEIGHT):
        sys.exit(f"{src} is {width} x {height}, a picture is at most {WIDTH} x {HEIGHT}")
    colors, alphas = dither(width, height, rows)
    alpha = any(a < 255 for a in alphas)
    stride = width * 2
    header = struct.pack("<BBHHHHH", MAGIC, RGB565A8 if alpha else RGB565_SWAPPED, 0, width, height, stride, 0)
    if alpha:
        body = struct.pack(f"<{len(colors)}H", *colors) + bytes(alphas)
    else:
        body = struct.pack(f">{len(colors)}H", *colors)
    dst = src.with_suffix(".img")
    dst.write_bytes(header + body)
    print(f"{dst}: {width} x {height}, {len(header + body)} bytes" + (", with opacity" if alpha else ""))


def main(args: list):
    if not args:
        sys.exit(__doc__.strip().splitlines()[-1].strip())
    for a in args:
        convert(pathlib.Path(a))


if __name__ == "__main__":
    main(sys.argv[1:])
