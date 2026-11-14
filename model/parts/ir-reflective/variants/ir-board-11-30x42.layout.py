"""The IR board with eleven channels on a 30 x 42 perfboard: the sixteen-channel layout
with channels 7, 8 and 14 to 16 left out. The netlist is the main board of schematic.py.

    python tools/perfboard/build.py model/parts/ir-reflective/variants/ir-board-11-30x42.layout.py

The converter inputs of the channels left out are held at ground in place of their pull-downs,
U1's beside its DGND and U2's at the top of its ground rail.
"""
from layout import Layout, row_name

L = Layout("30x42", title="IR board, eleven channels",
           netlist=("model/parts/ir-reflective/figures.py", "main"))
L.figure("5 V", "v_in_machine")


def h(r, c):
    return f"{row_name(r)}{c}"


def mirror(c, right):
    return 43 - c if right else c


# ---- the two sensor bands, one along each short edge ------------------------------------
# Each band holds its channels in mirrored pairs, spread over the board's height. From the edge inward: the LED bus, ground,
# 3V3, the row of the resistor ends, the header. Each 220 ohm lies from the bus to its LED pin
# across ground and 3V3; each 4.7 kohm stands at its signal pin with its ground lead bent over
# 3V3. The left band sits one row lower than the right one, so the converters' supply pins
# come close enough for C6.
BAND = {False: (2, 12, 22), True: (1, 11, 21)}
M_ROWS = {right: [m for r in pairs for m in (r + 1, r + 4)] for right, pairs in BAND.items()}
PIN_ROW = {False: dict(zip(M_ROWS[False], range(12, 18))),        # U1, pin 1 on top
           True: {2: 12, 5: 13, 12: 14, 15: 15, 22: 16}}           # U2, pin 1 at the bottom


def channel(right, m):
    y = PIN_ROW[right].get(m)
    if y is None:
        return None
    return y - 11 if not right else 8 + 17 - y


LEFT_OUT = (7, 8, 14, 15, 16)
for n in LEFT_OUT:
    L.unfitted(f"J-S{n}", why="channel left out of the eleven-channel build")
    L.unfitted(f"R{n}", why="channel left out of the eleven-channel build")
    L.unfitted(f"R{16 + n}", joins=("1", "2"),
               why="channel left out; the converter input is tied to ground in its place")


def band(right):
    bus, gnd, v33, ends, hdr = (mirror(c, right) for c in (2, 3, 5, 6, 8))
    step = 1 if not right else -1
    for r in BAND[right]:
        for l, m, s in ((r, r + 1, r + 2), (r + 5, r + 4, r + 3)):
            n = channel(right, m)
            if n is None:
                continue
            L.part(f"J-S{n}", "header_male", [h(s, hdr), h(m, hdr), h(l, hdr)],
                   title=f"J-S{n}")
            L.part(f"R{n}", "resistor", [h(l, bus), h(l, ends)])
            L.part(f"R{16 + n}", "resistor", [h(m, ends), h(m, ends - 2 * step)],
                   standing=True)
            L.tin(h(m, ends - 2 * step), h(m, gnd))
            L.tin(h(m, ends), h(m, ends + step))
            L.tin(h(m, ends + step), h(m, hdr))
            L.bare(h(s, hdr), h(s, v33))
            L.tin(h(l, ends), h(l, ends + step))
            L.tin(h(l, ends + step), h(l, hdr))
    first, last = BAND[right][0], BAND[right][-1] + 5
    L.bare(h(0, bus), h(last, bus))                      # the LED bus, up to the top edge
    L.bare(h(1 if not right else first + 1, gnd), h(last - 1 if not right else 28, gnd))
    L.bare(h(first + 2, v33), h(29, v33))                # 3V3, down to the bottom edge


band(False)
band(True)
L.bare(h(0, 2), h(0, 41))                               # the LED bus along the top edge
L.bare(h(29, 5), h(29, 38))                             # 3V3 along the bottom edge

# ---- the converters, their control pins facing each other --------------------------------
L.part("U1", "mcp3008_p", pin1=h(12, 16), along=(0, 1))
L.part("U2", "mcp3008_p", pin1=h(16, 28), along=(0, -1))

# lanes: each signal along its header row, then along a column of its own to its pin row, and
# into the pin; the inner lanes turn first
TURN = {False: {3: 15, 6: 14, 13: 13, 16: 9, 23: 10, 26: 11},
        True: {2: 30, 5: 31, 12: 32, 15: 35, 22: 33}}
for right, pin_col in ((False, 16), (True, 28)):
    hdr = mirror(8, right)
    for m, x in TURN[right].items():
        y = PIN_ROW[right][m]
        if x != hdr:
            L.bare(h(m, hdr), h(m, x))
        if y != m:
            L.bare(h(m, x), h(y, x))
        L.bare(h(y, x), h(y, pin_col))
        n = channel(right, m)
        adc, ch = ("U1", n) if not right else ("U2", n - 8)
        L.limit(f"R{16 + n}.1", f"{adc}.{ch}", 35, fig="place_channel")


# ---- top: the LED switch, the supply module and the third housing of J-IR-ISO -------------
L.bare(h(1, 3), h(1, 27))                               # ground along row B from the left band
L.part("Q1", "to220", [h(1, 29), h(1, 28), h(1, 27)], tab=1)
L.tin(h(0, 28), h(1, 28))                               # drain onto the LED bus
L.bare(h(1, 29), h(3, 29))                              # the gate down past Q1's body
L.part("R35", "resistor", [h(3, 29), h(7, 29)])
L.tin(h(3, 29), h(3, 28))
L.part("R33", "resistor", [h(3, 24), h(3, 28)])
L.bare(h(1, 27), h(26, 27))                             # ground from Q1's source down to the regulator
L.bare(h(7, 25), h(7, 29))
L.bare(h(7, 25), h(9, 25))                              # on to U2's DGND


L.limit("R33.2", "Q1.G", 10, fig="place_local")
L.limit("R35.1", "Q1.G", 10, fig="place_local")

# ---- the converters' supply: 3V3_ADC down column 19 to U1, C6 between the two VDD pins ---
L.bare(h(9, 19), h(12, 19))
L.tin(h(12, 19), h(13, 19))
L.part("C1", "ceramic_k15", [h(10, 19), h(10, 18)])
L.tin(h(10, 18), h(10, 17))
L.bare(h(1, 17), h(19, 17))                             # ground from row B down under U1
L.bare(h(14, 17), h(14, 19))
L.tin(h(19, 19), h(19, 18))
L.tin(h(19, 18), h(19, 17))
L.part("C6", "ceramic_fg11", [h(14, 22), h(14, 21)])
L.tin(h(14, 19), h(14, 20))
L.tin(h(14, 20), h(14, 21))
L.tin(h(14, 22), h(14, 23))
L.tin(h(14, 23), h(14, 24))
L.tin(h(15, 24), h(15, 25))
L.tin(h(15, 25), h(16, 25))
L.part("C2", "ceramic_k15", [h(17, 25), h(18, 25)])
L.tin(h(17, 24), h(17, 25))
# U2's AGND and C2 tinned across onto the ground trunk under the socket; DGND takes row 7
L.tin(h(18, 25), h(18, 26))
L.tin(h(18, 26), h(18, 27))
L.tin(h(14, 25), h(14, 26))
L.tin(h(14, 26), h(14, 27))
L.limit("C1.1", "U1.16", 10, fig="place_decoupling")
L.limit("C2.1", "U2.16", 10, fig="place_decoupling")
L.limit("C6.1", "U1.16", 10, fig="place_local")
L.limit("C6.1", "U2.16", 10, fig="place_local")

# ---- U1's lines down to the first housing of J-IR-ISO, DOUT through R37 -----------------
L.bare(h(18, 19), h(18, 20))
L.bare(h(18, 20), h(28, 20))                            # CS-A
L.bare(h(17, 19), h(17, 21))
L.bare(h(17, 21), h(28, 21))                            # MOSI
L.bare(h(16, 19), h(16, 22))
L.part("R37", "resistor", [h(20, 22), h(16, 22)])
L.bare(h(20, 22), h(28, 22))
L.bare(h(15, 19), h(15, 23))
L.bare(h(15, 23), h(28, 23))                            # SCK
L.piece("J-IR-ISO", "a", "header_male", [h(28, c) for c in range(18, 24)],
        pins=[11, 10, 3, 2, 4, 1], title="J-IR-ISO", sub="U1's lines and the supply")
L.bare(h(19, 17), h(28, 17))                            # ground down to the housing
L.tin(h(28, 17), h(28, 18))
L.tin(h(28, 19), h(29, 19))                             # 3V3 from the rail along the edge

# ---- U2's lines up to the second housing, DOUT through R38 ------------------------------
L.bare(h(10, 25), h(10, 24))
L.bare(h(10, 24), h(7, 24))                             # CS-B
L.bare(h(11, 25), h(11, 23))
L.bare(h(11, 23), h(7, 23))                             # MOSI
L.bare(h(12, 25), h(12, 22))
L.part("R38", "resistor", [h(8, 22), h(12, 22)])
L.tin(h(8, 22), h(7, 22))
L.bare(h(13, 25), h(13, 21))
L.bare(h(13, 21), h(7, 21))                             # SCK
L.piece("J-IR-ISO", "b", "header_male", [h(7, c) for c in range(20, 25)], pins=[9, 5, 8, 6, 7],
        title="J-IR-ISO", sub="U2's lines and CLOCK")
L.bare(h(7, 20), h(3, 20))                              # CLOCK up and across to R33
L.bare(h(3, 20), h(3, 24))

# ---- bottom edge: the regulator on the 3V3 rail, C5 and R39 beside it ---------------------
# The power board's 5 V plugs onto the module's VIN and GND from above, so VIN's contact stays bare.
L.part("J-D24V5F3", "header_female", [h(27, 30), h(27, 29), h(27, 28), h(27, 27)],
       title="J-D24V5F3", sub="D24V5F3 socket")
L.tin(h(27, 27), h(28, 27))                             # VOUT straight onto the 3V3 rail
L.tin(h(28, 27), h(29, 27))
L.tin(h(27, 28), h(26, 28))                             # the module's GND up to the trunk
L.tin(h(26, 28), h(26, 27))                             # and on to the ground trunk
L.part("C5", "ceramic_fg11", [h(29, 25), h(28, 25)])
L.bare(h(26, 25), h(28, 25))
L.tin(h(26, 25), h(26, 26))
L.tin(h(26, 26), h(26, 27))
L.part("R39", "resistor", [h(28, 24), h(24, 24)])
L.tin(h(28, 24), h(29, 24))
L.bare(h(14, 24), h(24, 24))                            # 3V3_ADC up to C6 and U2
L.limit("C5.1", "J-D24V5F3.4", 10, fig="place_local")
L.limit("R39.1", "J-D24V5F3.4", 10, fig="place_local")

# ---- insulated wires ---------------------------------------------------------------------
L.tin(h(28, 39), h(28, 40))
L.wire(h(28, 39), h(25, 28), via=[h(28, 31), h(25, 31)])   # the right band's ground to the trunk
L.tin(h(25, 28), h(25, 27))
L.tin(h(9, 19), h(9, 20))
L.wire(h(9, 20), h(14, 23), via=[h(13, 20)])            # 3V3_ADC on to U1
# ---- the inputs of the channels left out, held at ground ---------------------------------
L.tin(h(18, 16), h(19, 16))                             # U1's inputs 7 and 8
L.tin(h(19, 16), h(19, 17))
L.tin(h(11, 28), h(10, 28))                             # U2's inputs 6 to 8
L.tin(h(10, 28), h(9, 28))
L.tin(h(9, 28), h(9, 27))
L.tin(h(9, 27), h(9, 26))
VARIANT = ("IR sensing", ["11 channels, 30x42"])
LAYOUT = L
