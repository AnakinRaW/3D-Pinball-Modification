"""Turns the PNGs of one layer, each at most 320 x 240 pixels, into picture files for the screen component.

The PNGs are converted together as one set and share one palette of 255 entries of
color and opacity, which every file of the set holds. A file starts with 256 palette
entries, as docs/firmware/components/screen.md describes. An entry is four bytes: the
color in RGB565, high byte first, its opacity from 0 to 255, and an unused byte.
Entry 0 holds the width and the height instead, each high byte first, since index 0
stays transparent. Then come the pixels row by row from the top left, one byte each
with the pixel's palette index. A pixel below the first of the display's 32 opacity steps becomes index 0.
Each --color puts an opaque color into the palette as it is, such as the color of a
text. Floyd-Steinberg dithering spreads the error of every pixel onto its neighbors,
so a picture with more colors keeps its look. Each file lands beside its PNG.

    python tools/picture.py [--color ffffff ...] table.png lamp.png ...
"""
import pathlib
import struct
import sys
import zlib

WIDTH, HEIGHT = 320, 240
COLORS = 255                      # index 0 is transparent
CHANNELS = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}   # PNG color type -> samples per pixel


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


def rgb565(c) -> int:
    return (c[0] >> 3) << 11 | (c[1] >> 2) << 5 | c[2] >> 3


def expand(v: int):
    """The color an RGB565 value shows, back in 8 bits a channel."""
    r, g, b = v >> 11, (v >> 5) & 0x3F, v & 0x1F
    return (r << 3 | r >> 2, g << 2 | g >> 4, b << 3 | b >> 2)


def step(a: int) -> int:
    """The display's opacity step, 0 to 31, of an 8-bit opacity."""
    return a >> 3


def shown(entry):
    """What the display makes of an entry (RGB565, step), in 8 bits a channel."""
    v, s = entry
    return expand(v) + (s << 3 | s >> 2,)


def median_cut(counts: dict, n: int) -> list:
    """Up to n colors, each the weighted mean of one box the colors were cut into."""
    dims = range(len(next(iter(counts))))
    def box(entries):
        lo = [min(c[k] for c, _ in entries) for k in dims]
        hi = [max(c[k] for c, _ in entries) for k in dims]
        axis = max(dims, key=lambda k: hi[k] - lo[k])
        return hi[axis] - lo[axis], axis, entries

    boxes = [box(list(counts.items()))]
    while len(boxes) < n:
        wide = [b for b in boxes if len(b[2]) > 1]
        if not wide:
            break
        widest = max(wide, key=lambda b: b[0])
        boxes.remove(widest)
        _, axis, entries = widest
        entries.sort(key=lambda e: e[0][axis])
        half, seen = sum(w for _, w in entries) / 2, 0
        for cut, (_, w) in enumerate(entries):             # the weighted median
            seen += w
            if seen >= half:
                break
        cut = min(cut, len(entries) - 2) + 1
        boxes += [box(entries[:cut]), box(entries[cut:])]
    out = []
    for _, _, entries in boxes:
        total = sum(w for _, w in entries)
        out.append(tuple(round(sum(c[k] * w for c, w in entries) / total) for k in dims))
    return out


def palette(pictures: list, forced: list) -> list:
    """One palette for every picture of the set, as entries (RGB565, step), the forced colors among them."""
    fixed = sorted({(rgb565(c), step(255)) for c in forced})
    counts = {}
    for _, _, rows in pictures:
        for row in rows:
            for p in row:
                if step(p[3]) > 0:
                    c = shown((rgb565(p), step(p[3])))     # what the display can tell apart
                    counts[c] = counts.get(c, 0) + 1
    room = COLORS - len(fixed)
    picked = {(rgb565(c), max(1, step(c[3]))) for c in median_cut(counts, room)} if counts and room else set()
    return fixed + sorted(picked - set(fixed))


def convert(src: pathlib.Path, picture, entries: list):
    width, height, rows = picture
    looks = [shown(e) for e in entries]
    visible = [[step(p[3]) > 0 for p in row] for row in rows]

    nearest = {}
    def pick(r, g, b, a):
        key = (r >> 3, g >> 2, b >> 3, a >> 3)
        if key not in nearest:
            nearest[key] = min(range(len(looks)), key=lambda i: (looks[i][0] - r) ** 2 * 2
                               + (looks[i][1] - g) ** 2 * 4 + (looks[i][2] - b) ** 2 * 3
                               + (looks[i][3] - a) ** 2 * 3)
        return nearest[key]

    work = [[list(p) for p in row] for row in rows]
    index = bytearray(width * height)
    for y in range(height):
        for x in range(width):
            if not visible[y][x]:
                continue                                   # stays 0, transparent
            r, g, b, a = (min(255, max(0, round(v))) for v in work[y][x])
            i = pick(r, g, b, a)
            index[y * width + x] = i + 1
            err = (r - looks[i][0], g - looks[i][1], b - looks[i][2], a - looks[i][3])
            for dx, dy, part in ((1, 0, 7), (-1, 1, 3), (0, 1, 5), (1, 1, 1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < width and ny < height and visible[ny][nx]:
                    for k in range(4):
                        work[ny][nx][k] += err[k] * part / 16

    table = entries + [(0, 0)] * (COLORS - len(entries))
    out = struct.pack(">HH", width, height) + b"".join(struct.pack(">HBB", v, s << 3 | s >> 2, 0) for v, s in table)
    dst = src.with_suffix(".img")
    dst.write_bytes(out + bytes(index))
    print(f"{dst}: {width} x {height}, {sum(1 for v in index if v == 0)} transparent pixels")


def main(args: list):
    forced, sources = [], []
    while args:
        a = args.pop(0)
        if a == "--color":
            if not args or len(args[0]) != 6:
                sys.exit("--color takes a color as six hex digits, such as ffffff")
            v = int(args.pop(0), 16)
            forced.append((v >> 16, v >> 8 & 0xFF, v & 0xFF, 255))
        else:
            sources.append(pathlib.Path(a))
    if not sources:
        sys.exit(__doc__.strip().splitlines()[-1].strip())
    if len(forced) > COLORS:
        sys.exit(f"a palette holds {COLORS} colors")
    pictures = []
    for src in sources:
        width, height, rows = read_png(src)
        if not (0 < width <= WIDTH and 0 < height <= HEIGHT):
            sys.exit(f"{src} is {width} x {height}, a picture is at most {WIDTH} x {HEIGHT}")
        pictures.append((width, height, rows))
    entries = palette(pictures, forced)
    print(f"{len(entries)} palette entries for {len(sources)} pictures")
    for src, picture in zip(sources, pictures):
        convert(src, picture, entries)


if __name__ == "__main__":
    main(sys.argv[1:])
