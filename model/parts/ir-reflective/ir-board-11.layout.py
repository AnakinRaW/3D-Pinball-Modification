"""The IR board with eleven channels on a 26 x 31 perfboard. The netlist is the main
board of schematic.py, five of its sixteen channel positions left out.

    python tools/perfboard/build.py model/parts/ir-reflective/ir-board-11.layout.py

U1 reads channels 1 to 6 from the band along the left edge, the stock sensors on channels 1 to 3.
U2 reads channels 9 to 13 from the band along the right edge. Channels 7, 8 and 14 to 16 are not
fitted, and their converter inputs are held at ground in place of their pull-downs.
"""
from layout import Layout, row_name

L = Layout("26x31", title="IR board, eleven channels",
           netlist=("model/parts/ir-reflective/figures.py", "main"))
L.figure("5 V", "v_in_machine")


def h(r, c):
    return f"{row_name(r)}{c}"


def mirror(c, right):
    return 32 - c if right else c


# ---- the channels left out ----------------------------------------------------------------
for n in (7, 8, 14, 15, 16):
    L.unfitted(f"J-S{n}", why="channel left out of the eleven-channel build")
    L.unfitted(f"R{n}", why="channel left out of the eleven-channel build")
    L.unfitted(f"R{16 + n}", joins=("1", "2"),
               why="channel left out; a link holds the converter input at ground")

# ---- the two sensor bands -----------------------------------------------------------------
# each channel: (number, right band, LED row, signal row, 3V3 row)
CHANNELS = [(1, False, 4, 5, 6), (2, False, 9, 8, 7), (3, False, 11, 12, 13),
            (4, False, 16, 15, 14), (5, False, 18, 19, 20), (6, False, 23, 22, 21),
            (13, True, 2, 3, 4), (12, True, 5, 6, 7), (11, True, 10, 9, 8),
            (10, True, 12, 13, 14), (9, True, 17, 16, 15)]
for n, right, l, m, s in CHANNELS:
    bus, gnd, v33, ends, hdr = (mirror(c, right) for c in (2, 3, 5, 6, 8))
    step = -1 if right else 1
    L.part(f"J-S{n}", "header_male", [h(s, hdr), h(m, hdr), h(l, hdr)], title=f"J-S{n}")
    L.part(f"R{n}", "resistor", [h(l, bus), h(l, ends)])
    L.part(f"R{16 + n}", "resistor", [h(m, ends), h(m, ends - 2 * step)], standing=True)
    L.tin(h(m, ends - 2 * step), h(m, gnd))
    L.tin(h(m, ends), h(m, ends + step))
    L.tin(h(m, ends + step), h(m, hdr))
    L.bare(h(s, hdr), h(s, v33))
    L.tin(h(l, ends), h(l, ends + step))
    L.tin(h(l, ends + step), h(l, hdr))
L.bare(h(0, 2), h(0, 30))                               # the LED bus along the top edge
L.bare(h(0, 2), h(23, 2))
L.bare(h(0, 30), h(17, 30))
L.bare(h(1, 3), h(1, 29))                               # ground along row B
L.bare(h(1, 3), h(22, 3))
L.bare(h(1, 29), h(16, 29))
L.bare(h(6, 5), h(25, 5))                               # 3V3 along the bottom edge
L.bare(h(4, 27), h(25, 27))
L.bare(h(25, 5), h(25, 27))

# ---- the converters, their supply pins facing each other across rows M to O ---------------
L.part("U1", "mcp3008_p", pin1=h(15, 13), along=(0, 1))
L.part("U2", "mcp3008_p", pin1=h(11, 20), along=(0, -1))

# lanes: (channel, signal row, turn column, pin row, pin column)
LANES = [(1, 5, 12, 15, 13), (2, 8, 11, 16, 13), (3, 12, 10, 17, 13), (4, 15, 9, 18, 13),
         (5, 19, None, 19, 13), (6, 22, 11, 20, 13),
         (13, 3, 21, 7, 20), (12, 6, 22, 8, 20), (11, 9, None, 9, 20), (10, 13, 22, 10, 20),
         (9, 16, 21, 11, 20)]
for n, m, x, y, pc in LANES:
    hdr = 24 if pc == 20 else 8
    adc, ch = ("U1", n) if n <= 8 else ("U2", n - 8)
    L.limit(f"R{16 + n}.1", f"{adc}.{ch}", 35, fig="place_channel")
    if x is None:
        L.bare(h(m, hdr), h(m, pc))
        continue
    (L.tin if abs(x - hdr) == 1 else L.bare)(h(m, hdr), h(m, x))
    L.bare(h(m, x), h(y, x))
    (L.tin if abs(x - pc) == 1 else L.bare)(h(y, x), h(y, pc))

# ---- the converters' grounds: column 14 under U1, column 18 under U2 and up to row B -------
L.bare(h(9, 14), h(22, 14))                             # U1's ground under its socket
L.tin(h(17, 16), h(17, 15))
L.tin(h(17, 15), h(17, 14))                             # U1's AGND
L.tin(h(22, 16), h(22, 15))
L.tin(h(22, 15), h(22, 14))                             # U1's DGND
L.tin(h(21, 13), h(22, 13))                             # inputs 7 and 8 held at ground
L.tin(h(22, 13), h(22, 14))
L.bare(h(9, 14), h(9, 17))                              # on to U2's AGND
L.tin(h(9, 17), h(9, 18))
L.bare(h(1, 18), h(13, 18))                             # U2's ground from row B down under its socket
L.bare(h(4, 17), h(4, 20))                              # U2's DGND and input 8, across it
L.tin(h(4, 20), h(5, 20))
L.tin(h(5, 20), h(6, 20))                               # inputs 6 and 7

# ---- 3V3_ADC down column 16 between the converters, the three ceramics beside it -----------
L.tin(h(10, 17), h(11, 17))
L.tin(h(11, 17), h(11, 16))
L.bare(h(11, 16), h(16, 16))
L.part("C2", "ceramic_k15", [h(12, 15), h(12, 14)])
L.tin(h(12, 15), h(12, 16))
L.part("C6", "ceramic_fg11", [h(13, 16), h(13, 17)])
L.tin(h(13, 17), h(13, 18))
L.part("C1", "ceramic_k15", [h(14, 19), h(14, 18)])
L.tin(h(13, 18), h(14, 18))
L.bare(h(15, 16), h(15, 19))
L.tin(h(14, 19), h(15, 19))
L.part("R39", "resistor", [h(15, 23), h(15, 19)])       # from J-S9's 3V3 pin
L.tin(h(15, 23), h(15, 24))
L.limit("C1.1", "U1.16", 10, fig="place_decoupling")
L.limit("C2.1", "U2.16", 10, fig="place_decoupling")
L.limit("C6.1", "U1.16", 10, fig="place_local")
L.limit("C6.1", "U2.16", 10, fig="place_local")

# ---- U2's lines left to the second housing of J-IR-ISO, DOUT through R38 ------------------
L.bare(h(5, 17), h(5, 13))                              # CS-B
L.bare(h(6, 17), h(6, 13))                              # MOSI
L.tin(h(7, 17), h(7, 16))
L.part("R38", "resistor", [h(7, 15), h(7, 16)], standing=True)
L.tin(h(7, 15), h(7, 14))
L.tin(h(7, 14), h(7, 13))
L.bare(h(8, 17), h(8, 13))                              # SCK
L.piece("J-IR-ISO", "b", "header_male", [h(r, 13) for r in range(5, 9)], pins=[7, 6, 8, 5],
        title="J-IR-ISO", sub="U2's lines")

# ---- U1's lines right and down to the first housing of J-IR-ISO, DOUT through R37 ---------
# The outer lines turn last, so none crosses another: CS-A down column Q, MOSI down R, DOUT
# through R37 down S, SCK down T.
L.bare(h(21, 16), h(21, 17))
L.bare(h(21, 17), h(24, 17))                            # CS-A
L.bare(h(20, 16), h(20, 18))
L.bare(h(20, 18), h(24, 18))                            # MOSI
L.bare(h(19, 16), h(19, 18))
L.part("R37", "resistor", [h(19, 19), h(19, 18)], standing=True)
L.bare(h(19, 19), h(24, 19))                            # DOUT
L.bare(h(18, 16), h(18, 20))
L.bare(h(18, 20), h(24, 20))                            # SCK
L.tin(h(22, 16), h(23, 16))                             # U1's DGND on to the housing's GND
L.tin(h(23, 16), h(24, 16))
L.piece("J-IR-ISO", "a", "header_male", [h(24, c) for c in range(16, 23)],
        pins=[11, 3, 2, 4, 1, 10, 9], title="J-IR-ISO", sub="U1's lines, the supply and CLOCK")
L.tin(h(24, 21), h(25, 21))                             # 3V3 from the bottom rail

# ---- the regulator on row S, the module lying over the free corner below it ---------------
L.part("J-D24V5F3", "header_female", [h(18, 22), h(18, 23), h(18, 24), h(18, 25)],
       title="J-D24V5F3", sub="D24V5F3 socket")
L.tin(h(18, 25), h(18, 26))                             # VOUT onto the right band's 3V3
L.tin(h(18, 26), h(18, 27))
L.bare(h(17, 16), h(17, 21))                            # ground from U1's AGND to the module
L.bare(h(17, 21), h(19, 21))
L.bare(h(19, 21), h(19, 25))
L.tin(h(18, 24), h(19, 24))                             # the module's GND
L.part("C5", "ceramic_fg11", [h(21, 25), h(20, 25)])
L.tin(h(19, 25), h(20, 25))
L.bare(h(21, 25), h(21, 27))
L.limit("C5.1", "J-D24V5F3.4", 10, fig="place_local")
L.limit("R39.1", "J-D24V5F3.4", 10, fig="place_local")

# ---- the LED switch in the corner below the right band ------------------------------------
L.part("Q1", "to220", [h(19, 30), h(19, 29), h(19, 28)], tab=1)
L.tin(h(19, 29), h(18, 29))                             # drain up to R9's bus lead
L.tin(h(18, 29), h(17, 29))
L.tin(h(17, 29), h(17, 30))
L.bare(h(16, 28), h(21, 28))                            # source up to R25's ground lead
L.bare(h(19, 30), h(22, 30))                            # the gate
L.part("R35", "resistor", [h(21, 30), h(21, 29)], standing=True)
L.tin(h(21, 29), h(21, 28))
L.part("R33", "resistor", [h(22, 26), h(22, 30)])       # across the 3V3 column
L.bare(h(24, 22), h(22, 22))                            # CLOCK up from the housing
L.bare(h(22, 22), h(22, 26))
L.limit("R33.2", "Q1.G", 10, fig="place_local")
L.limit("R35.1", "Q1.G", 10, fig="place_local")

VARIANT = ("IR sensing", ["11 channels, 26x31"])
LAYOUT = L
