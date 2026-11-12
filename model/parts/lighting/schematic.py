"""The two lighting boards, pin by pin. figures.py loads this file, and the checks of
docs/parts/lighting/ read it from there.

Both boards carry the same parts and the same wiring. The ambient board drives chains A to C, the
playfield board chains D to F.

Connector pin orders: J-AMBIENT-TEENSY and J-PLAYFIELD-TEENSY carry the board's first to third
chain on pins 1 to 3. J-AMBIENT-PWR and J-PLAYFIELD-PWR carry 5 V on pin 1 and GND on pin 2. Each
of J-LA to J-LF carries 5 V on pin 1, the chain's data on pin 2 and GND on pin 3.

U1 is a CD74HCT125E (TI SCHS415A, page 3 pin table). Channels 1 to 3 carry the chains, channel 4
stays unused with its input 4A at GND.
"""
from figcheck import Schematic

SCHEMATIC = Schematic()


def lighting_board(name, title, chains):
    """One lighting board: U1, R1 to R6, C1, C2, its power input, its data input from the Teensy
    and the three chain outputs."""
    upper = name.upper()
    b = SCHEMATIC.board(name, title)
    b.part(f"J-{upper}-TEENSY", "header_male", pins=3)
    b.part(f"J-{upper}-PWR", "pads", pins=2, title="the pair from J-LIGHT on the edge pads")
    b.part("U1", "cd74hct125e", type="CD74HCT125E")
    b.part("C1", "ceramic_k15", type="K104K15X7RF5TL2", value="c_bypass")
    b.part("C2", "electrolytic_zlh_8x20", type="Rubycon 16ZLH1000MEFC8X20", value="c_one")
    # U1's channel n: its input A, its output Y, each on its pin of the DIP-14
    inputs, outputs = ("U1.2", "U1.5", "U1.9"), ("U1.3", "U1.6", "U1.8")
    for n, chain in enumerate(chains, start=1):
        out = f"J-L{chain}"
        b.part(out, "terminal_5mm", pins=3)
        b.part(f"R{n}", "resistor", value="r_in")
        b.part(f"R{n + 3}", "resistor", value="r_out")
        b.net(f"CHAIN{n}", f"J-{upper}-TEENSY.{n}", f"R{n}.1", color="blue",
              label=f"CHAIN {chain}", teensy=f"LED chain {chain}")
        b.net(f"IN{n}", f"R{n}.2", inputs[n - 1], color="blue", label=f"{n}A")
        b.net(f"OUT{n}", outputs[n - 1], f"R{n + 3}.1", color="white", label=f"{n}Y")
        b.net(f"DATA{n}", f"R{n + 3}.2", f"{out}.2", color="white", label="DATA")
    outs = [f"J-L{chain}" for chain in chains]
    b.net("5V", f"J-{upper}-PWR.1", "U1.14", "C1.1", "C2.+", *(f"{o}.1" for o in outs),
          color="red", label="5 V", rail="5V")
    # 1OE, 2OE, 3OE and 4OE on pins 1, 4, 10 and 13, the unused input 4A on pin 12
    b.net("GND", f"J-{upper}-PWR.2", "U1.7", "U1.1", "U1.4", "U1.10", "U1.13", "U1.12",
          "C1.2", "C2.-", *(f"{o}.3" for o in outs), color="black", label="GND", rail="gnd")
    b.unconnected("U1.11", why="4Y, the output of the unused channel 4")
    return b


AMBIENT = lighting_board("ambient", "Ambient board", "ABC")
PLAYFIELD = lighting_board("playfield", "Playfield board", "DEF")
