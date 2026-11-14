"""Draw every part of the inventory once, each in a cell with its name: python catalog.py out.svg"""
import sys
import draw as d
from inventory import PITCH, PARTS, MODULES

S = 13.0
P = PITCH * S


def grid(rows, cols, x0, y0):
    out = []
    for r in range(rows):
        for c in range(cols):
            out += d.hole(x0 + c * P, y0 + r * P, S)
    return out


def cell(title, sub, rows, cols, draw_fn):
    """One catalog cell: a patch of board, the part drawn on it, its name beneath."""
    return dict(title=title, sub=sub, rows=rows, cols=cols, fn=draw_fn)


def h(x0, y0, r, c):
    return (x0 + c * P, y0 + r * P)


CELLS = [
    cell("Resistor, lying", "10 kΩ, 4 holes", 3, 6,
         lambda x, y: d.resistor(h(x, y, 1, 1), h(x, y, 1, 5), S, 10000)),
    cell("Resistor, lying", "1.2 kΩ, 5 holes", 3, 7,
         lambda x, y: d.resistor(h(x, y, 1, 1), h(x, y, 1, 6), S, 1200)),
    cell("Resistor, standing", "330 Ω, 2 holes", 4, 3,
         lambda x, y: d.resistor(h(x, y, 1, 1), h(x, y, 3, 1), S, 330, standing=True)),
    cell("Schottky 1N5819", "DO-41, band = cathode", 3, 6,
         lambda x, y: d.diode(h(x, y, 1, 1), h(x, y, 1, 5), S)),
    cell("Ceramic Vishay K", "100 nF, 1 hole", 3, 3,
         lambda x, y: d.ceramic(h(x, y, 1, 1), h(x, y, 2, 1), S)),
    cell("Ceramic TDK FG11", "22 µF, 1 hole", 3, 4,
         lambda x, y: d.ceramic(h(x, y, 1, 1), h(x, y, 1, 2), S, "ceramic_fg11")),
    cell("Electrolytic KEMET ESL", "100 µF, 5 x 11", 4, 4,
         lambda x, y: d.electrolytic(h(x, y, 2, 1), h(x, y, 2, 2), S)),
    cell("Electrolytic Rubycon ZLH", "1000 µF, 8 x 20", 5, 6,
         lambda x, y: d.electrolytic(h(x, y, 2, 2), h(x, y, 2, 4), S, "electrolytic_zlh_8x20")),
    cell("IRL540N / STP160N3LL", "TO-220, G D S, tab up", 4, 7,
         lambda x, y: d.to220([h(x, y, 2, 2), h(x, y, 2, 3), h(x, y, 2, 4)], S, +1)),
    cell("CD74HCT125E", "DIP-14", 5, 9,
         lambda x, y: d.dip_part("cd74hct125e", h(x, y, 3, 1), S)[0]),
    cell("MCP3008", "DIP-16", 5, 10,
         lambda x, y: d.dip_part("mcp3008_p", h(x, y, 3, 1), S)[0]),
    cell("MCP23008", "DIP-18", 5, 11,
         lambda x, y: d.dip_part("mcp23008_p", h(x, y, 3, 1), S)[0]),
    cell("IR LED AM4457F3C", "side looking, lens up", 3, 6,
         lambda x, y: d.side_looker(h(x, y, 1, 1), h(x, y, 1, 2), S, "am4457f3c", +1)),
    cell("Phototransistor TEKT5400S", "side looking, lens up", 3, 7,
         lambda x, y: d.side_looker(h(x, y, 1, 1), h(x, y, 1, 2), S, "tekt5400s", +1)),
    cell("QRE1113", "reflective, looks up", 3, 4,
         lambda x, y: d.reflective([h(x, y, 1, 1), h(x, y, 1, 2), h(x, y, 2, 2), h(x, y, 2, 1)],
                                   S, "qre1113")),
    cell("OPB606A", "reflective, looks up", 3, 4,
         lambda x, y: d.reflective([h(x, y, 1, 1), h(x, y, 1, 2), h(x, y, 2, 2), h(x, y, 2, 1)],
                                   S, "opb606a")),
    cell("Pin header", "1 x 5", 3, 7,
         lambda x, y: d.header([h(x, y, 1, c) for c in range(1, 6)], S)),
    cell("Socket header", "1 x 5", 3, 7,
         lambda x, y: d.header([h(x, y, 1, c) for c in range(1, 6)], S, female=True)),
    cell("Pin header, 90°", "1 x 5, pins off the right edge", 7, 6,
         lambda x, y: d.header_90([h(x, y, r, 1) for r in range(1, 6)], S, (1, 0))),
    cell("Screw terminal", "2 poles, entry up", 6, 5,
         lambda x, y: d.terminal([h(x, y, 3, 1), h(x, y, 3, 3)], S, (0, -1))),
    cell("Screw terminal", "3 poles, entry up", 6, 7,
         lambda x, y: d.terminal([h(x, y, 3, 1), h(x, y, 3, 3), h(x, y, 3, 5)], S, (0, -1))),
    cell("Pololu D24V5F3", "daughter board", 7, 6,
         lambda x, y: d.breakout("d24v5f3", h(x, y, 1, 1)[0] - 1.27 * S,
                                 h(x, y, 1, 1)[1] - 1.27 * S, S)[0]),
    cell("Adafruit 757", "daughter board", 8, 10,
         lambda x, y: d.breakout("adafruit757", h(x, y, 1, 1)[0] - 3.81 * S,
                                 h(x, y, 1, 1)[1] - 2.54 * S, S)[0]),
    cell("DFRobot DFR0954", "daughter board", 9, 9,
         lambda x, y: d.breakout("dfr0954", h(x, y, 1, 1)[0] - 2.65 * S,
                                 h(x, y, 1, 1)[1] - 1.38 * S, S)[0]),
    cell("Adafruit 1207", "SOIC-16 adapter", 9, 10,
         lambda x, y: d.breakout("adafruit1207", h(x, y, 1, 1)[0] - 1.27 * S,
                                 h(x, y, 1, 1)[1] - 1.27 * S, S)[0]),
    cell("Teensy 4.1", "daughter board, USB left", 9, 26,
         lambda x, y: d.breakout("teensy41", h(x, y, 1, 1)[0] - 1.27 * S,
                                 h(x, y, 1, 1)[1] - 1.27 * S, S)[0]),
    cell("Connections", "tin, bare wire, link, insulated wire", 6, 10,
         lambda x, y: d.tin(h(x, y, 1, 1), h(x, y, 1, 2), S) + d.bare(h(x, y, 2, 1), h(x, y, 2, 7), S)
         + d.link(h(x, y, 3, 1), h(x, y, 3, 6), S)
         + d.insulated([h(x, y, 5, 1), h(x, y, 4, 2), h(x, y, 4, 7), h(x, y, 5, 8)], S, "#1f4fd1")),
]


def build(path, width=1500):
    out, x, y, row_h = [], 20, 20, 0
    for cdef in CELLS:
        w = cdef["cols"] * P + 10
        hgt = cdef["rows"] * P + 44
        if x + w > width:
            x, y, row_h = 20, y + row_h + 16, 0
        gx, gy = x + P / 2, y + P / 2
        out.append(f'<rect x="{x}" y="{y}" width="{w:.0f}" height="{cdef["rows"] * P:.0f}" rx="4" '
                   'fill="#f8f4ee" stroke="#d4c7a0" stroke-width="1"/>')
        out += grid(cdef["rows"], cdef["cols"], gx, gy)
        out += cdef["fn"](gx, gy)
        ty = y + cdef["rows"] * P + 16
        out.append(d.text(x + 4, ty, cdef["title"], size=12, anchor="start"))
        out.append(d.text(x + 4, ty + 16, cdef["sub"], size=10.5, weight="400", fill="#666",
                          anchor="start"))
        x += w + 16
        row_h = max(row_h, hgt)
    H = y + row_h + 20
    svg = (f'<svg viewBox="0 0 {width} {H:.0f}" xmlns="http://www.w3.org/2000/svg" '
           'font-family="Segoe UI, Arial, sans-serif">'
           f'<rect width="{width}" height="{H:.0f}" fill="#ffffff"/>' + "\n".join(out) + "</svg>\n")
    open(path, "w", encoding="utf-8").write(svg)
    return H


if __name__ == "__main__":
    print(build(sys.argv[1]))
