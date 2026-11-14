"""A perfboard layout: the board, the parts on its holes, the connections between holes, and the
netlist the copper has to make.

A layout file is a Python module that builds one `Layout` and names it LAYOUT:

    L = Layout("6x28", title="Sense board")
    L.part("R41", "resistor", ["B2", "B6"], value=10000, fig="r_series")
    L.part("Q1", "to220", ["G5", "G4", "G3"], tab=+1)
    L.part("U1", "cd74hct125e", pin1="C3", along=(1, 0))
    L.part("A1", "teensy41", at="B2", rot=0)
    L.bare("A1", "A5")                      # uninsulated wire along a row, soldered in every hole
    L.tin("A5", "A6")                       # solder between two neighboring holes
    L.link("C3", "C7")                      # bare wire through two holes, lifted over the rest
    L.wire("J4", "J24", via=["I4"])         # insulated wire, soldered at its two ends only
    L.net("GND", "J-SENSE-TEENSY.5", "R31.2", "C1.2")
    L.limit("C1.1", "J-M.1", 20, fig="place_filter")
    LAYOUT = L

Holes are named by row letter and column number, as printed on the board. The edge pads are
`C<` (the pad left of row C, before column 1) and `C>` (right of the last column).
"""
from inventory import BOARDS, PARTS, MODULES, PITCH, edge_pads, unique_names


def row_name(i):
    if i < 0:
        raise ValueError(f"row index {i} lies off the board")
    s = ""
    i += 1
    while i:
        i, r = divmod(i - 1, 26)
        s = chr(65 + r) + s
    return s


def row_index(s):
    i = 0
    for ch in s:
        i = i * 26 + (ord(ch) - 64)
    return i - 1


class Board:
    def __init__(self, kind, cut=None):
        short, long_, _stock = BOARDS[kind]
        self.kind = kind
        self.rows, self.cols = short, long_
        # a single straight cut keeps a range of columns: (first, last), 1-based inclusive
        self.cut = cut
        self.c0, self.c1 = (0, long_ - 1) if not cut else (cut[0] - 1, cut[1] - 1)

    def hole(self, name):
        """(row, column) of a hole or pad name; a pad sits at column -1 or `cols`."""
        if name.endswith("<"):
            return (row_index(name[:-1]), self.c0 - 1)
        if name.endswith(">"):
            return (row_index(name[:-1]), self.c1 + 1)
        i = 0
        while name[i].isalpha():
            i += 1
        return (row_index(name[:i]), int(name[i:]) - 1)

    def name(self, h):
        r, c = h
        if c == self.c0 - 1:
            return f"{row_name(r)}<"
        if c == self.c1 + 1:
            return f"{row_name(r)}>"
        return f"{row_name(r)}{c + 1}"

    def is_pad(self, h):
        r, c = h
        has_left = not self.cut or self.cut[0] == 1
        has_right = not self.cut or self.cut[1] == self.cols
        if 1 <= r <= self.rows - 2:
            return (c == self.c0 - 1 and has_left) or (c == self.c1 + 1 and has_right)
        return False

    def on_board(self, h):
        r, c = h
        return 0 <= r < self.rows and self.c0 <= c <= self.c1

    def valid(self, h):
        return self.on_board(h) or self.is_pad(h)

    def xy(self, h):
        """Millimeters from the first hole of the kept board."""
        return ((h[1] - self.c0) * PITCH, h[0] * PITCH)

    def neighbors(self, h):
        r, c = h
        for v in ((r - 1, c), (r + 1, c), (r, c - 1), (r, c + 1)):
            if self.valid(v):
                yield v


class Part:
    def __init__(self, ref, key, holes, pins, **opt):
        self.ref, self.key, self.holes, self.pins, self.opt = ref, key, holes, pins, opt

    @property
    def netref(self):
        """The netlist's designator: the part's own, or for a piece the part it belongs to."""
        return self.opt.get("of", self.ref)

    @property
    def spec(self):
        return PARTS.get(self.key) or MODULES.get(self.key)

    @property
    def kind(self):
        if self.key in MODULES:
            return "module"
        if self.key == "pads":
            return "pads"
        return PARTS[self.key]["kind"]


DEFAULT_PINS = {
    "axial": ("1", "2"), "radial": ("1", "2"),
}


class Layout:
    def __init__(self, board, title="", subtitle="", cut=None, netlist=None):
        """netlist names the subsystem's model and the board in it, as
        ("model/parts/solenoid/figures.py", "sense"). Its nets then stand in for L.net, and the
        check holds the parts placed here to the parts it declares."""
        self.board = Board(board, cut)
        self.title, self.subtitle = title, subtitle
        self.parts = []
        self.bares, self.tins, self.links, self.wires = [], [], [], []
        self.nets = {}
        self.net_colors, self.net_labels = {}, {}
        self.limits = []
        self.notes = []
        self.declared, self.model, self.free = None, None, set()
        self.offboard_parts = {}
        self.unfitted_parts = {}
        self.gauges = {}           # runs of a wire thicker than the default, by their two ends
        self.short_wires = set()   # insulated wires accepted under the minimum length
        self.text_figs = {}
        if netlist:
            self._declared(*netlist)

    def _declared(self, model_path, board):
        """The board's netlist, as the subsystem's model declares it."""
        import pathlib
        import sys
        tools = str(pathlib.Path(__file__).resolve().parents[1])
        if tools not in sys.path:
            sys.path.insert(0, tools)
        import figcheck  # noqa: F401  the module a model imports its names from
        from figlib.repo import load_model, model_file
        self.model = load_model(model_file(model_path))
        if board not in self.model.boards:
            raise KeyError(f"{self.model.name} declares no board {board!r}, only "
                           f"{', '.join(self.model.boards) or 'none'}")
        self.declared = self.model.boards[board]
        self.free = set(self.declared.open)
        for n in self.declared.nets.values():
            self.net(n.name, *n.pins, color=n.color, label=n.label)

    # ---- parts ----------------------------------------------------------------------------
    def part(self, ref, key, holes=None, pin1=None, along=(1, 0), at=None, rot=0, **opt):
        B = self.board
        d = self.declared.parts.get(ref) if self.declared else None
        if d is not None and d.value:
            # the value the netlist sets from a figure of the model
            opt.setdefault("fig", d.value)
            if key == "resistor":
                f = self.model.figure(d.value)
                opt.setdefault("value", round(f.value.to("Ω"), 6))
        if key == "pads":
            hs = [B.hole(h) for h in holes]
            pins = tuple(str(i + 1) for i in range(len(hs)))
        elif key in MODULES:
            m = MODULES[key]
            r0, c0 = B.hole(at)                     # the hole under the module's first pin
            fx, fy, _ = m["pins"][0]
            hs, pins = [], []
            for i, (x, y, name) in enumerate(m["pins"]):
                dx, dy = round((x - fx) / PITCH), round((y - fy) / PITCH)
                if rot == 0:
                    h = (r0 + dy, c0 + dx)
                elif rot == 90:
                    h = (r0 + dx, c0 - dy)
                elif rot == 180:
                    h = (r0 - dy, c0 - dx)
                else:
                    h = (r0 - dx, c0 + dy)
                hs.append(h)
                pins.append(str(i + 1))
            opt.update(at=at, rot=rot, names=[n for _, _, n in m["pins"]])
            # a module's pins go by their names, as the netlist names them
            pins = unique_names(n for _, _, n in m["pins"])
            if opt.get("carries"):
                # an adapter: its pin n is the carried chip's pin n, which the netlist names
                if d is None:
                    raise KeyError(f"{ref} carries a chip, and the netlist declares no {ref}")
                chip = d.pins
                order = sorted(range(len(pins)), key=lambda i: int(pins[i]))
                hs = [hs[i] for i in order]
                pins = tuple(chip[int(pins[i]) - 1] for i in order)
                opt["names"] = list(pins)
                opt["carries"] = d.key
        elif PARTS[key]["kind"] in ("dip",):
            n = PARTS[key]["pins"]
            half = n // 2
            r0, c0 = B.hole(pin1)
            ax, ay = along
            rx, ry = ay, -ax                        # left of the run: pins count counterclockwise
            hs = [(r0 + ay * i, c0 + ax * i) for i in range(half)]
            hs += [(r0 + ay * (half - 1 - i) + 3 * ry, c0 + ax * (half - 1 - i) + 3 * rx)
                   for i in range(half)]
            pins = tuple(str(i + 1) for i in range(n))
            opt.update(pin1=pin1, along=along)
        else:
            hs = [B.hole(h) for h in holes]
            spec = PARTS[key]
            names = spec.get("pins")
            if isinstance(names, int):
                names = None
            if spec["kind"] in ("header", "socket", "terminal", "header_90"):
                pins = tuple(str(i + 1) for i in range(len(hs)))
            elif spec["kind"] in ("reflective", "reflective_grid"):
                pins = tuple(str(i + 1) for i in range(len(hs)))
            else:
                pins = tuple(names) if names else DEFAULT_PINS.get(spec["kind"], ("1", "2"))
        p = Part(ref, key, hs, pins, **opt)
        self.parts.append(p)
        return p

    # ---- connections ----------------------------------------------------------------------
    def bare(self, a, b, side="bottom", gauge=None):
        """A bare run; gauge names a wire thicker than the build's default, such as "1.3 mm"."""
        ha, hb = self.board.hole(a), self.board.hole(b)
        self.bares.append((ha, hb, side))
        if gauge:
            self.gauges[(ha, hb)] = gauge

    def tin(self, a, b, side="bottom"):
        self.tins.append((self.board.hole(a), self.board.hole(b), side))

    def link(self, a, b, side="top"):
        self.links.append((self.board.hole(a), self.board.hole(b), side))

    def wire(self, a, b, via=(), side="bottom", short=False):
        """An insulated wire. short accepts it under the minimum length, where the user took
        the risk to the insulation for a straighter route."""
        pts = [self.board.hole(a)] + [self.board.hole(v) for v in via] + [self.board.hole(b)]
        self.wires.append((pts, side))
        if short:
            self.short_wires.add(tuple(pts))

    # ---- what the copper has to make ------------------------------------------------------
    def net(self, name, *pins, color=None, label=None):
        """A net of the schematic and every part pin on it. color is the wire color of
        connectors.md the net is drawn and wired in; label is how the drawing names it."""
        self.nets.setdefault(name, set()).update(pins)
        if color:
            self.net_colors[name] = color
        if label:
            self.net_labels[name] = label

    def limit(self, a, b, mm, fig=None, why=""):
        self.limits.append((a, b, mm, fig, why))

    def note(self, text):
        self.notes.append(text)

    def figure(self, text, key):
        """A label the drawing prints that states a figure of a model, such as a rail's "5 V".
        Every text holding it carries key in data-fig."""
        self.text_figs[text] = key

    def unfitted(self, ref, joins=(), why=""):
        """A part of the netlist the build leaves out, such as an optional protection. joins
        names two of its pins whose nets a wire on the board joins in its place, as ("D", "S")
        for a MOSFET in the ground line; the nets on its other pins go with it."""
        self.unfitted_parts[ref] = (tuple(joins), why)

    def effective_nets(self):
        """The nets the copper has to make: the netlist's, with the nets an unfitted part's
        joins name merged into one."""
        nets = {n: set(p) for n, p in self.nets.items()}
        for ref, (joins, _why) in self.unfitted_parts.items():
            if len(joins) != 2:
                continue
            a, b = (next((n for n, p in nets.items() if f"{ref}.{pin}" in p), None)
                    for pin in joins)
            if a and b and a != b:
                rails = ({n.name for n in self.declared.nets.values() if n.rail}
                         if self.declared else set())
                if a not in rails and b in rails:
                    a, b = b, a              # the merged net keeps the rail's name
                nets[a] |= nets.pop(b)
        return nets

    def offboard(self, ref, how=""):
        """A part of the netlist that stays off this board, such as a module hanging on a cable.
        how says where it sits and how it is reached."""
        self.offboard_parts[ref] = how

    def against_netlist(self):
        """Where the layout departs from the netlist its model declares: a part placed here and
        not there or the other way round, another inventory part, another value, a net of its
        own."""
        if not self.declared:
            return []
        E, placed = [], {}
        for p in self.parts:
            if p.netref in placed and "of" in p.opt:
                q = placed[p.netref]       # the pieces of one header count as one part
                placed[p.netref] = Part(p.netref, q.key, q.holes + p.holes, q.pins + p.pins,
                                        pieces=True)
            elif "of" in p.opt:
                placed[p.netref] = Part(p.netref, p.key, p.holes, p.pins, pieces=True)
            else:
                placed[p.netref] = p
        for ref in self.offboard_parts:
            if ref not in self.declared.parts:
                E.append(f"{ref} is marked off the board and the netlist holds no {ref}")
            elif ref in placed:
                E.append(f"{ref} is marked off the board and placed on it")
        for ref in self.unfitted_parts:
            if ref not in self.declared.parts:
                E.append(f"{ref} is marked unfitted and the netlist holds no {ref}")
            elif ref in placed:
                E.append(f"{ref} is marked unfitted and placed on the board")
        for ref, d in self.declared.parts.items():
            p = placed.get(ref)
            if p is None:
                if ref not in self.offboard_parts and ref not in self.unfitted_parts:
                    E.append(f"{ref} of the netlist is not placed")
                continue
            if p.opt.get("carries", p.key) != d.key:
                E.append(f"{ref} is a {p.key} here and a {d.key} in the netlist")
            elif (sorted(p.pins) != sorted(d.pins) if p.opt.get("pieces")
                  else p.pins != d.pins):
                E.append(f"{ref} has the pins {', '.join(p.pins)} here and "
                         f"{', '.join(d.pins)} in the netlist")
            if d.value and p.opt.get("fig") != d.value:
                E.append(f"{ref} states {p.opt.get('fig')} here and {d.value} in the netlist")
            elif d.value and p.key == "resistor":
                want = self.model.figure(d.value).value.to("Ω")
                if abs(p.opt["value"] - want) > 1e-6 * want:
                    E.append(f"{ref} is {p.opt['value']:g} ohm here and {want:g} ohm in the "
                             f"netlist")
        for ref in sorted(placed.keys() - self.declared.parts.keys()):
            E.append(f"{ref} is placed here and not in the netlist")
        for name, pins in self.nets.items():
            n = self.declared.nets.get(name)
            if n is None or set(n.pins) != pins:
                E.append(f"net {name} is declared here; the netlist holds the nets")
        return E

    # ---- lookups --------------------------------------------------------------------------
    def find(self, ref):
        return next(p for p in self.parts if p.ref == ref)

    def pin_hole(self, ref_pin):
        ref, pin = ref_pin.split(".", 1)
        p = next(q for q in self.parts if q.netref == ref and pin in q.pins)
        return p.holes[p.pins.index(pin)]

    def piece(self, ref, name, key, holes, pins, **opt):
        """One piece of a header the netlist declares as one part, placed apart from its other
        pieces: pins names the netlist's pins of this piece, in the order of holes. The cable
        meets the pieces with one housing each."""
        B = self.board
        p = Part(f"{ref}/{name}", key, [B.hole(h) for h in holes], tuple(str(x) for x in pins),
                 of=ref, **opt)
        self.parts.append(p)
        return p


def run_holes(a, b):
    """Every hole a straight run from a to b passes, ends included; None if a and b are not in
    one row or one column."""
    (r1, c1), (r2, c2) = a, b
    if r1 == r2:
        return [(r1, c) for c in range(min(c1, c2), max(c1, c2) + 1)]
    if c1 == c2:
        return [(r, c1) for r in range(min(r1, r2), max(r1, r2) + 1)]
    return None
