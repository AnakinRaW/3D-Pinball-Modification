"""A board's circuit as a netlist: its parts, the pins of each, and the nets that join them pin
by pin. A subsystem's model declares one per board, and the perfboard layouts and the KiCad
export read it from there."""
from __future__ import annotations

import dataclasses
import importlib.util
import pathlib
import re

from .repo import ROOT

INVENTORY = ROOT / "tools" / "perfboard" / "inventory.py"

# the parts whose pins the netlist counts itself, numbered from 1
COUNTED = ("header", "socket", "terminal", "header_90", "pads")
# the pin names of a two-lead part whose inventory entry names none
TWO_LEADS = ("1", "2")


def inventory():
    """The perfboard inventory, which holds every part's pins."""
    spec = importlib.util.spec_from_file_location("perfboard_inventory", INVENTORY)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def pin_names(key: str, count: int | None, inv) -> tuple:
    """The names the layouts give the pins of an inventory part: a module's, an IC's and a
    counted part's by number, a part with named leads by those names."""
    if key == "pads":
        kind = "pads"
    elif key in inv.MODULES:
        return inv.unique_names(n for _, _, n in inv.MODULES[key]["pins"])
    elif key in inv.PARTS:
        kind = inv.PARTS[key]["kind"]
    else:
        return None
    spec = inv.PARTS.get(key, {})
    if kind in COUNTED:
        if not count:
            raise ValueError(f"a {key} names how many pins it has")
        return tuple(str(i + 1) for i in range(count))
    if kind == "dip":
        return tuple(str(i + 1) for i in range(spec["pins"]))
    if "pinout" in spec:
        return tuple(str(i + 1) for i in range(len(spec["pinout"])))
    pins = spec.get("pins")
    return tuple(pins) if isinstance(pins, tuple) else TWO_LEADS


@dataclasses.dataclass
class Part:
    ref: str
    key: str                    # the perfboard inventory's entry
    pins: tuple
    type: str | None = None     # the part list's name for it, where the value does not say it
    value: str | None = None    # the key of the model's figure that sets its value
    title: str | None = None    # how a drawing names it, where the designator does not
    roles: tuple | None = None  # its pins' roles, where its inventory entry gives none
    listed_in: str | None = None  # the subsystem whose part list holds it, where that is
                                  # another subsystem than the board's


@dataclasses.dataclass
class Net:
    name: str
    pins: list
    color: str | None = None    # the wire color of docs/connectors.md
    label: str | None = None    # how a drawing names it
    teensy: str | None = None   # the Teensy signal it carries, as the pin table names it
    rail: str | None = None     # the supply it is: "gnd", TEENSY_RAIL or a rail's name


DESIGNATOR_RE = re.compile(r"^[A-Z]+(?:-[A-Z0-9]+)*\d*$")


class Schematic:
    """A subsystem's boards, as its schematic.py declares them. The model loads it with
    MODEL.schematic("schematic.py")."""

    def __init__(self):
        self.boards: dict[str, Board] = {}
        self.path: pathlib.Path | None = None

    def board(self, name, title):
        if name in self.boards:
            raise KeyError(f"board {name!r} is declared twice")
        self.boards[name] = Board(None, name, title)
        return self.boards[name]


def load_schematic(path: pathlib.Path) -> Schematic:
    """The SCHEMATIC a schematic.py declares."""
    spec = importlib.util.spec_from_file_location(f"schematic_{abs(hash(str(path)))}", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    mod.SCHEMATIC.path = path
    return mod.SCHEMATIC


class Board:
    """One board of a subsystem: its parts and the nets between their pins."""

    def __init__(self, model: str | None, name: str, title: str):
        self.model, self.name, self.title = model, name, title
        self.parts: dict[str, Part] = {}
        self.nets: dict[str, Net] = {}
        self.open: dict[str, str] = {}          # REF.PIN of a pin on no net, with why
        self.contacts: list = []                # (REF.PIN, REF.PIN, what joins them)
        self.roles: dict = {}                   # REF.PIN: the wiring part it is on a Teensy net
        self.errors: list[str] = []             # what the declaration itself got wrong
        self._inv = None

    def _inventory(self):
        if self._inv is None:
            self._inv = inventory()
        return self._inv

    def part(self, ref, key, *, pins=None, type=None, value=None, title=None, roles=None,
             listed_in=None):
        """A part on the board: its designator and its inventory entry. A header, a terminal or
        edge pads name how many pins they have. A part the inventory does not hold names its
        pins in order, and their roles where a Teensy net reaches them."""
        if ref in self.parts:
            raise KeyError(f"{self.name}: {ref} is declared twice")
        if not DESIGNATOR_RE.match(ref):
            raise ValueError(f"{self.name}: {ref!r} is no designator")
        names = pin_names(key, pins if isinstance(pins, int) else None, self._inventory())
        if names is None:
            if not isinstance(pins, (tuple, list)):
                raise ValueError(f"{self.name}: {ref} is a {key}, which the inventory does not "
                                 f"hold, so it names its pins")
            names = self._inventory().unique_names(pins)
        if roles is not None and len(roles) != len(names):
            raise ValueError(f"{self.name}: {ref} names {len(roles)} roles for {len(names)} pins")
        self.parts[ref] = Part(ref, key, names, type, value, title,
                               tuple(roles) if roles else None, listed_in)
        return self.parts[ref]

    def net(self, name, *pins, color=None, label=None, teensy=None, rail=None):
        """A net and every part pin on it, each as REF.PIN. A supply net names its rail: "gnd",
        TEENSY_RAIL for the Teensy's own 3.3 V, or the rail's name."""
        if name in self.nets:
            raise KeyError(f"{self.name}: net {name} is declared twice")
        self.nets[name] = Net(name, list(pins), color, label, teensy, rail)
        return self.nets[name]

    def unconnected(self, *pins, why):
        """Pins on no net, and why they stay free."""
        for p in pins:
            self.open[p] = why

    def contact(self, a, b, *, what):
        """Something off the board that joins two of its pins at times, such as a ball that
        bridges two wires."""
        self.contacts.append((a, b, what))

    def role(self, pin, part):
        """What a pin does to a Teensy net where its inventory entry cannot say, as one of the
        wiring's drives, pull, reads or contact."""
        self.roles[pin] = part

    # -- reading it -------------------------------------------------------------------------
    def pin_net(self) -> dict[str, str]:
        """The net each REF.PIN sits on."""
        return {p: n.name for n in self.nets.values() for p in n.pins}

    def problems(self) -> list[str]:
        """What keeps the netlist from being a circuit: a pin of no part, a pin on two nets or on
        none, a net that joins a single pin."""
        out, seen = [], {}
        where = f"{self.model} board {self.name}"
        for n in self.nets.values():
            if len(n.pins) < 2:
                out.append(f"{where}: net {n.name} joins {len(n.pins)} pin, and a net "
                           f"joins two at least")
            for p in n.pins:
                ref, _, pin = p.partition(".")
                part = self.parts.get(ref)
                if part is None:
                    out.append(f"{where}: net {n.name} names {p}, and {ref} is no part of "
                               f"the board")
                elif pin not in part.pins:
                    out.append(f"{where}: net {n.name} names {p}, and {ref} has the pins "
                               f"{', '.join(part.pins)}")
                if p in seen and seen[p] != n.name:
                    out.append(f"{where}: {p} sits on {seen[p]} and on {n.name}")
                seen.setdefault(p, n.name)
        for p in self.open:
            if p in seen:
                out.append(f"{where}: {p} is declared free and sits on {seen[p]}")
        for part in self.parts.values():
            for pin in part.pins:
                p = f"{part.ref}.{pin}"
                if p not in seen and p not in self.open:
                    out.append(f"{where}: {p} sits on no net, and the model does not say "
                               f"why it stays free")
        for a, b, what in self.contacts:
            for p in (a, b):
                if p not in seen:
                    out.append(f"{where}: the contact {what!r} reaches {p}, which sits on no net")
        for p in self.roles:
            if p not in seen:
                out.append(f"{where}: {p} has a role and sits on no net")
        return out


# -- the Teensy's nets, as the wiring check reads them ------------------------------------------
CAPACITOR_KINDS = ("radial", "radial_round")
CONNECTOR_KINDS = COUNTED


def _kind(board, part) -> str:
    inv = board._inventory()
    return "pads" if part.key == "pads" else (
        inv.PARTS[part.key]["kind"] if part.key in inv.PARTS else
        "module" if part.key in inv.MODULES else "other")


def _spec(board, part) -> dict:
    inv = board._inventory()
    return inv.PARTS.get(part.key) or inv.MODULES.get(part.key) or {}


def _pin_label(spec, part, pin) -> str:
    """A pin as a message names it: the part and the pin's name in its datasheet."""
    i = part.pins.index(pin)
    names = spec.get("pinout") or spec.get("pins")
    if not spec:
        return f"{part.ref} {pin}"
    if isinstance(names, (list, tuple)) and len(names) == len(part.pins):
        name = names[i][2] if isinstance(names[i], tuple) else names[i]
        return f"{part.ref} {name}"
    return f"{part.ref} pin {pin}"


def _by_role(board, part, pin, through, net_of, supply, src, WiringPart):
    """What a pin does to a Teensy net, from its declared role or its inventory entry's roles. A
    string says why it cannot be told."""
    p = f"{part.ref}.{pin}"
    spec = _spec(board, part)
    roles = part.roles or spec.get("roles")
    role = roles[part.pins.index(pin)] if roles else "other"
    name = _pin_label(spec, part, pin) + through

    def rail():
        for q, r in zip(part.pins, roles):
            if r == "supply" and f"{part.ref}.{q}" in net_of:
                found, _ = supply(net_of[f"{part.ref}.{q}"])
                if found:
                    return found
        return None

    if role == "in":
        return WiringPart(name, "input", src=src)
    if role == "od":
        return WiringPart(name, "open-drain", src=src)
    if role.startswith("odpull:"):
        # an open drain with the part's own pull-up to the supply on the pin the role names
        q = role[len("odpull:"):]
        far = net_of.get(f"{part.ref}.{q}")
        found = supply(far)[0] if far is not None else None
        if found is None:
            return f"the pull-up of {p} reaches no supply through {part.ref}.{q}"
        return [WiringPart(name, "open-drain", src=src),
                WiringPart(f"the pull-up inside {part.ref} on {pin}", "pull", found, src=src)]
    if role in ("supply", "gnd", "nc"):
        return None
    if role == "out" or role.startswith("tri:"):
        r = rail()
        if r is None:
            return f"{part.ref} reaches no supply rail through its supply pins"
        if role == "out":
            return WiringPart(name, "push-pull", r, src=src)
        enable = net_of.get(f"{part.ref}.{role[4:]}")
        if enable is None:
            return f"the enable pin {part.ref}.{role[4:]} of {p} sits on no net"
        return WiringPart(name, "tri-state", r, select=enable.label or enable.name, src=src)
    if role == "config":
        return "the firmware sets its role, which the schematic does not declare with role()"
    return "its role is not declared"


# the module that is the Teensy, whose pins the pin table describes
TEENSY_KEY = "teensy41"


def teensy_nets(board, model: str, src: str, own=None):
    """The nets of the board that reach a Teensy signal, each with what its parts do to it, as
    the wiring check reads a net. A net reaches across a series resistor into the nets behind it.
    A resistor from there to a supply is a pull, a capacitor does nothing to the level, and a
    contact joins it to whatever supply stands behind the far pin. Any other pin does what its
    role says: the one the schematic declares, or the one the part's inventory entry gives."""
    from .model import Net as WiringNet, Part as WiringPart
    net_of = {p: n for n in board.nets.values() for p in n.pins}
    out, problems = [], []

    def other(part, pin):
        return next(f"{part.ref}.{p}" for p in part.pins if p != pin)

    def supply(net, seen=()):
        """The rail a net is, or reaches across one resistor, with the resistor's name."""
        if net.rail:
            return net.rail, None
        for p in net.pins:
            ref, _, pin = p.partition(".")
            part = board.parts[ref]
            if part.key == "resistor":
                far = net_of.get(other(part, pin))
                if far is not None and far.rail:
                    return far.rail, ref
        return None, None

    for start in (n for n in board.nets.values()
                  if n.teensy and (own is None or n.teensy in own)):
        region, via, frontier = [start], {start.name: []}, [start]
        while frontier:
            net = frontier.pop()
            for p in net.pins:
                ref, _, pin = p.partition(".")
                part = board.parts[ref]
                if part.key != "resistor":
                    continue
                far = net_of.get(other(part, pin))
                if far is None or far.rail or far.name in via:
                    continue
                via[far.name] = via[net.name] + [ref]
                region.append(far)
                frontier.append(far)
        parts = []
        for net in region:
            behind = " behind " + " and ".join(via[net.name]) if via[net.name] else ""
            through = " through " + " and ".join(via[net.name]) if via[net.name] else ""
            for p in net.pins:
                ref, _, pin = p.partition(".")
                part, kind = board.parts[ref], _kind(board, board.parts[ref])
                if part.key == TEENSY_KEY:
                    continue
                if p in board.roles:
                    parts.append(board.roles[p])
                elif part.key == "resistor":
                    far = net_of.get(other(part, pin))
                    if far is not None and far.rail:
                        parts.append(WiringPart(f"{ref}{behind}", "pull", far.rail, src=src))
                elif kind in CAPACITOR_KINDS:
                    continue
                elif kind in CONNECTOR_KINDS:
                    joined = [c for c in board.contacts if p in c[:2]]
                    for a, b, what in joined:
                        rail, r = supply(net_of[b if a == p else a])
                        if rail is None:
                            problems.append(f"the contact {what!r} on {p} reaches no supply")
                            continue
                        parts.append(WiringPart(f"{what}{through}"
                                                + (f" and {r}" if r else ""),
                                                "contact", rail, src=src))
                    if not joined and net is not start:
                        problems.append(f"{p} leaves the board on the {start.name} side, and "
                                        f"nothing says what is on its far end")
                else:
                    got = _by_role(board, part, pin, through, net_of, supply, src, WiringPart)
                    if isinstance(got, str):
                        problems.append(f"{p} sits on the {start.name} side, and {got}")
                    elif isinstance(got, list):
                        parts += got
                    elif got is not None:
                        parts.append(got)
        out.append(WiringNet(f"{start.label or start.name} on the {board.title.lower()}",
                             start.teensy, parts, model))
    return out, problems


# -- the check of a model's boards ------------------------------------------------------------
PART_LIST_RE = re.compile(r"(?ms)^## Part list[ \t]*$(.*?)(?=^## |\Z)")
# a designator of the part list, or a span of them written "R21 to R24" or "R1 … R16"
LISTED_RE = re.compile(r"(?<![\w-])([RCDUQLF])(\d{1,3})"
                       r"(?:\**\s+(?:to|…|\.\.\.)\s+\**\1(\d{1,3}))?(?![\w-])")
PLAIN_RE = re.compile(r"^[RCDUQLF]\d{1,3}$")


def listed(path) -> set | None:
    """The designators a design's part list holds, a span "R21 to R24" counted in full, None for
    a document without a part list."""
    try:
        text = path.read_text(encoding="utf-8")
    except OSError:
        return None
    m = PART_LIST_RE.search(text)
    if not m:
        return None
    out = set()
    for letter, first, last in LISTED_RE.findall(m.group(1)):
        for n in range(int(first), int(last or first) + 1):
            out.add(f"{letter}{n}")
    return out


def _all_models():
    from .repo import find_models, load_model
    return [load_model(p) for p in find_models()]


def teensy_pins(model) -> list[str]:
    """A net that reaches the Teensy's own pin carries the signal the pin table gives that pin."""
    from .pins import pin_rows
    rows, out = {r.signal: r for r in pin_rows()}, []
    for b in model.boards.values():
        for n in b.nets.values():
            for p in n.pins:
                ref, _, pin = p.partition(".")
                if b.parts[ref].key != TEENSY_KEY:
                    continue
                name = pin.split("#")[0]
                if not name.isdigit():
                    continue
                row = rows.get(n.teensy) if n.teensy else None
                if n.teensy is None:
                    out.append(f"board {b.name}: net {n.name} reaches Teensy pin {name} and "
                               f"names no Teensy signal")
                elif row is None:
                    out.append(f"board {b.name}: net {n.name} carries {n.teensy}, which the pin "
                               f"table does not list")
                elif name != str(row.pin):
                    out.append(f"board {b.name}: net {n.name} carries {n.teensy} to Teensy pin "
                               f"{name}, and the pin table gives it pin {row.pin}")
    return out


def pass_circuits(model, rep) -> int:
    """Each board's netlist is a circuit, sets each value from a figure of the model, holds the
    parts of the part list, and carries the Teensy signals the model's wiring declares."""
    boards = list(model.boards.values())
    if not boards:
        return 0
    models = _all_models()
    names = {m.name for m in models}
    for b in boards:
        for msg in b.problems():
            rep.error(msg, kind="circuit", key=b.name)
        for part in b.parts.values():
            if part.value is not None and part.value not in model.figs:
                rep.error(f"{model.name} board {b.name}: {part.ref} takes its value from "
                          f"{part.value}, which the model does not declare",
                          kind="circuit", key=part.ref)
            if part.listed_in and part.listed_in not in names:
                rep.error(f"{model.name} board {b.name}: {part.ref} is listed in "
                          f"{part.listed_in}, which no model is named", kind="circuit",
                          key=part.ref)
    for msg in model.schematic_problems + teensy_pins(model):
        rep.error(msg if msg.startswith(model.name) else f"{model.name}: {msg}",
                  kind="circuit", key=model.name)
    # a designator names a part on its own board; a board built twice carries it twice
    on_boards = {}
    for b in boards:
        for ref in b.parts:
            if PLAIN_RE.match(ref):
                on_boards.setdefault(ref, b.name)
    own = listed(model.document)
    guests = {ref for m in models if m is not model for b in m.boards.values()
              for ref, p in b.parts.items() if p.listed_in == model.name}
    if own is not None:
        for ref in sorted(own - on_boards.keys() - guests):
            rep.error(f"{model.name}: the part list holds {ref}, which no board's netlist "
                      f"carries", kind="circuit", key=ref)
        for ref in sorted(on_boards.keys() - own):
            rep.error(f"{model.name}: board {on_boards[ref]} carries {ref}, which the part "
                      f"list does not hold", kind="circuit", key=ref)
    from .pins import pin_rows
    mine = {r.signal for r in pin_rows() if model.name in r.subsystems}
    wired = {n.teensy for n in model.nets if n.teensy}
    carried = {n.teensy: b.name for b in boards for n in b.nets.values()
               if n.teensy and n.teensy in mine}
    for sig in sorted(wired - carried.keys()):
        rep.error(f"{model.name}: the wiring reaches the Teensy signal {sig}, which no "
                  f"board's net carries", kind="circuit", key=sig)
    for sig in sorted(carried.keys() - wired):
        rep.error(f"{model.name}: board {carried[sig]} carries the Teensy signal {sig}, which "
                  f"the model's wiring does not declare", kind="circuit", key=sig)
    for msg in part_rows(model)[1] + connector_rows(model):
        rep.error(f"{model.name}: {msg}", kind="circuit", key=model.name)
    return len(boards)


# -- the document's tables, held to the netlists ----------------------------------------------
def _norm(text: str) -> str:
    """A cell as it compares: no markup, one kind of space and of micro sign, no case."""
    text = re.sub(r"\*\*|`", "", text).replace(" ", " ").replace(" ", " ")
    return re.sub(r"\s+", " ", text.replace("μ", "µ")).strip().casefold()


def _tables(text: str):
    """Every markdown table: its first line's number, its header cells and its rows of cells."""
    lines, i = text.split("\n"), 0
    while i < len(lines):
        if lines[i].startswith("|") and i + 1 < len(lines) and re.match(r"^\|[\s:|-]+\|$",
                                                                          lines[i + 1]):
            head = [c.strip() for c in lines[i].strip().strip("|").split("|")]
            rows, j = [], i + 2
            while j < len(lines) and lines[j].startswith("|"):
                rows.append((j, [c.strip() for c in lines[j].strip().strip("|").split("|")]))
                j += 1
            yield i, head, rows
            i = j
        else:
            i += 1


def _expand(cell: str) -> list[str]:
    return [f"{letter}{n}" for letter, first, last in LISTED_RE.findall(cell)
            for n in range(int(first), int(last or first) + 1)]


def _part_index(model):
    return {ref: part for b in model.boards.values() for ref, part in b.parts.items()}


def _instances(model) -> dict:
    """How many parts each designator names: one on each of the model's boards that carries it,
    and one on each other subsystem's board that carries it for this model."""
    out = {}
    for m in _all_models():
        for b in m.boards.values():
            for ref, p in b.parts.items():
                if m is model or p.listed_in == model.name:
                    out[ref] = out.get(ref, 0) + 1
    return out


def part_rows(model):
    """The rows of the document's part list against the netlists: the count each row's Qty has
    to read, and what disagrees. A row's parts share one inventory entry, one type and one value,
    and the Part cell names the value, or the type where the part has no value."""
    path = model.document
    try:
        text = path.read_text(encoding="utf-8")
    except OSError:
        return {}, []
    m = PART_LIST_RE.search(text)
    if not m or not model.boards:
        return {}, []
    first = text.count("\n", 0, m.start(1)) + 1
    parts, counts, found = _part_index(model), {}, []
    instances = _instances(model)
    for at, head, rows in _tables(m.group(1)):
        cols = [_norm(h) for h in head]
        # the designators stand in a Where or a Use column, a type may stand in its own
        place = "where" if "where" in cols else "use" if "use" in cols else None
        if not {"qty", "part"} <= set(cols) or place is None:
            continue
        q, p, w = cols.index("qty"), cols.index("part"), cols.index(place)
        t = cols.index("type") if "type" in cols else None
        for line, cells in rows:
            refs = _expand(cells[w])
            # the row lists the parts of its first designator's kind; another kind in the cell,
            # as Q1 in "R1, with Q1, from its gate", is named in passing
            refs = [r for r in refs if r[0] == refs[0][0]] if refs else refs
            where = f"part list, line {first + line}"
            # a count such as "up to 13" covers fitting options, which the netlist does not, and
            # a row that names no designator, such as a module, has nothing to count
            if not cells[q].strip().isdigit() or not refs:
                continue
            n = sum(instances.get(r, 0) for r in refs)
            counts[first + line] = (n, q)
            if cells[q].strip() != str(n):
                found.append(f"{where} counts {cells[q].strip()}, and the boards carry {n} of "
                             f"the parts its {place.title()} cell names")
            on = [parts[r] for r in refs if r in parts]
            if not on:
                continue
            shown_of = lambda x: (None if not x.value else
                                  model.figure(x.value).value.show(model.figure(x.value).unit))
            kinds = {(x.key, x.type, shown_of(x)) for x in on}
            if len(kinds) > 1:
                found.append(f"{where} lists {', '.join(r.ref for r in on)}, which the "
                             f"netlists give different parts or values")
                continue
            key, type_, shown = kinds.pop()
            cell = _norm(cells[p] + (" " + cells[t] if t is not None else ""))
            if shown:
                if _norm(shown) not in cell:
                    found.append(f"{where} names no {shown}, the value the netlist gives "
                                 f"{', '.join(r.ref for r in on)}")
            elif type_ and _norm(type_) not in cell:
                found.append(f"{where} names no {type_}, the type the netlist gives "
                             f"{', '.join(r.ref for r in on)}")
    return counts, found


CONNECTOR_HEAD_RE = re.compile(r"^(J-[A-Z0-9-]+) pin$")


def connector_rows(model) -> list[str]:
    """Every table headed "J-XYZ pin | Signal" names, pin by pin, the net the netlist puts on
    that pin, by its label or its name, and leaves no pin of the connector out."""
    try:
        text = model.document.read_text(encoding="utf-8")
    except OSError:
        return []
    parts, found = _part_index(model), []
    pin_net = {p: n for b in model.boards.values() for n in b.nets.values() for p in n.pins}
    for at, head, rows in _tables(text):
        m = CONNECTOR_HEAD_RE.match(head[0].strip("*")) if head else None
        if not m or len(head) < 2 or _norm(head[1]) != "signal":
            continue
        ref = m.group(1)
        where = f"the {ref} table, line {at + 1}"
        if ref not in parts:
            found.append(f"{where} names {ref}, which no board's netlist carries")
            continue
        seen = set()
        for line, cells in rows:
            pin = cells[0].strip("*")
            seen.add(pin)
            net = pin_net.get(f"{ref}.{pin}")
            if net is None:
                found.append(f"{where}: {ref} has no pin {pin} on a net")
            elif _norm(cells[1]) not in (_norm(net.label or ""), _norm(net.name)):
                found.append(f"{where}: pin {pin} reads {cells[1]}, and the netlist puts "
                             f"{net.label or net.name} there")
        missing = [p for p in parts[ref].pins if p not in seen]
        if missing:
            found.append(f"{where} leaves out pin {', '.join(missing)}")
    return found


def write_part_list(model, rep) -> int:
    """--write: each Qty cell of the part list reads the count of the parts its row names."""
    counts, _ = part_rows(model)
    path = model.document
    lines = path.read_text(encoding="utf-8").split("\n")
    written = 0
    for line, (n, q) in sorted(counts.items()):
        cells = lines[line - 1].split("|")
        q += 1                      # the text before the first bar is a cell of its own
        if cells[q].strip() != str(n):
            pad = len(cells[q]) - len(cells[q].lstrip())
            cells[q] = " " * pad + str(n) + " "
            lines[line - 1] = "|".join(cells)
            written += 1
            rep.note(f"{path.name}:{line}: Qty set to {n}")
    if written:
        path.write_text("\n".join(lines), encoding="utf-8", newline="\n")
    return written


# -- the schematic drawings, held to the boards' netlists -------------------------------------
# A wire of a schematic SVG is a <line> or <polyline> carrying data-net, the net it draws. A part
# symbol's elements carry data-part, its designator or, for what sits off the boards, a name. A
# wire whose end lands on a part's pin names that pin in data-ends, "R11.2", several space
# separated.
WIRE_TAGS = ("line", "polyline")
# how far a wire's end may sit from another wire it joins, in drawing units: the offset of the
# outline stroke drawn under a light wire
JOIN = 3.0
# how far a wire's end may sit from the part it lands on: the radius of a pin marker
LAND = 7.0
# a stroke this wide is a wire or a symbol, thinner ones are table rules and outlines
WIRE_WIDTH = 2.0
DESIGNATOR_LIKE_RE = re.compile(r"^(?:[RCDUQLFA]\d{1,3}|J-[A-Z0-9-]+)$")
NUM_RE = re.compile(r"-?\d+(?:\.\d+)?")


def _local(tag):
    return tag.rsplit("}", 1)[-1]


def _pairs(text):
    nums = [float(n) for n in NUM_RE.findall(text or "")]
    return list(zip(nums[0::2], nums[1::2]))


def _points(el):
    """The outline of an element as a run of points."""
    tag, g = _local(el.tag), lambda k: float(el.get(k, 0) or 0)
    if tag == "line":
        return [(g("x1"), g("y1")), (g("x2"), g("y2"))]
    if tag == "polyline":
        return _pairs(el.get("points"))
    if tag == "polygon":
        pts = _pairs(el.get("points"))
        return pts + pts[:1]
    if tag == "rect":
        x, y, w, h = g("x"), g("y"), g("width"), g("height")
        return [(x, y), (x + w, y), (x + w, y + h), (x, y + h), (x, y)]
    if tag == "path":
        return _pairs(el.get("d"))
    if tag == "circle":
        return [(g("cx"), g("cy"))]
    return []


def _seg_dist(p, a, b):
    (px, py), (ax, ay), (bx, by) = p, a, b
    dx, dy = bx - ax, by - ay
    if dx == dy == 0:
        return ((px - ax) ** 2 + (py - ay) ** 2) ** 0.5
    t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)))
    return ((px - ax - t * dx) ** 2 + (py - ay - t * dy) ** 2) ** 0.5


def _dist(p, pts, filled=False):
    """How far a point lies from a shape's outline, 0 inside a filled shape."""
    if len(pts) == 1:
        return _seg_dist(p, pts[0], pts[0])
    if filled:
        xs, ys = [x for x, _ in pts], [y for _, y in pts]
        if min(xs) <= p[0] <= max(xs) and min(ys) <= p[1] <= max(ys):
            return 0.0
    return min(_seg_dist(p, a, b) for a, b in zip(pts, pts[1:]))


def schematic(path):
    """The wires a schematic SVG annotates, its part symbols, and the wide lines it leaves
    unannotated."""
    import xml.etree.ElementTree as ET
    wires, parts, bare, rails = [], {}, [], []
    for el in ET.parse(path).getroot().iter():
        tag, pts = _local(el.tag), _points(el)
        if not pts:
            continue
        if el.get("data-rail"):
            # a ground or supply symbol: every one of a net's symbols is the same node
            rails.append((el.get("data-rail"), pts, tag in ("rect", "polygon", "circle")))
        elif el.get("data-net") and tag in WIRE_TAGS:
            wires.append({"net": el.get("data-net"), "pts": pts,
                          "ends": el.get("data-ends", "").split()})
        elif el.get("data-part"):
            parts.setdefault(el.get("data-part"), []).append(
                (pts, tag in ("rect", "polygon", "circle")))
        elif (tag in WIRE_TAGS and float(el.get("stroke-width", 0) or 0) >= WIRE_WIDTH
              and el.get("stroke") not in (None, "none")):
            bare.append(pts)
    return wires, parts, bare, rails


def drawn_boards(model, path):
    """The boards a drawing shows: those its root's data-boards names, as "board" of the model or
    "model:board" of another, else every board of the model."""
    import xml.etree.ElementTree as ET
    from .repo import model_named
    names = (ET.parse(path).getroot().get("data-boards") or "").split()
    if not names:
        return list(model.boards.values()), []
    out, missing = [], []
    for n in names:
        owner, _, board = n.rpartition(":")
        m = model_named(owner) if owner else model
        if board in m.boards:
            out.append(m.boards[board])
        else:
            missing.append(n)
    return out, missing


def _where(pts):
    (x1, y1), (x2, y2) = pts[0], pts[-1]
    return f"{x1:g},{y1:g} to {x2:g},{y2:g}"


def check_schematic(model, path, rep) -> int:
    """A schematic drawing joins what the netlists join. Each wire draws a net of the model's
    boards, and the pins its ends land on sit on that net and touch that part's symbol. Wires
    that touch carry one net. The drawn pins of a net are joined by wires, a connector's two
    sides through its pin. A drawn part shows every pin, a connector only those it needs."""
    from .repo import rel
    name = rel(path)
    wires, parts, bare, rails = schematic(path)
    boards, missing = drawn_boards(model, path)
    nets, pins_of, free = {}, {}, set()
    for b in boards:
        nets.update(b.nets)
        pins_of.update({ref: part.pins for ref, part in b.parts.items()})
        free |= set(b.open)
    pin_net = {p: n.name for n in nets.values() for p in n.pins}
    found = [f"data-boards names {n}, which no model declares" for n in missing]

    for pts in bare:
        found.append(f"the line {_where(pts)} carries neither data-net nor data-part")
    for ref in parts:
        if DESIGNATOR_LIKE_RE.match(ref) and ref not in pins_of:
            found.append(f"draws {ref}, which no board's netlist carries")
    for w in wires:
        at = f"the wire {_where(w['pts'])}"
        if w["net"] not in nets:
            found.append(f"{at} draws net {w['net']}, which no board declares")
        for p in w["ends"]:
            ref = p.partition(".")[0]
            if p not in pin_net:
                found.append(f"{at} ends on {p}, which no netlist holds")
                continue
            if pin_net[p] != w["net"]:
                found.append(f"{at} draws {w['net']} and ends on {p}, which sits on "
                             f"{pin_net[p]}")
            shapes = parts.get(ref)
            if not shapes:
                found.append(f"{at} ends on {p}, and no element draws {ref}")
            elif min(_dist(e, s, f) for e in (w["pts"][0], w["pts"][-1])
                     for s, f in shapes) > LAND:
                found.append(f"{at} names {p}, and neither of its ends touches {ref}")

    # wires that touch carry one net, and form the pieces a net's pins have to share
    up = list(range(len(wires)))

    def root(i):
        while up[i] != i:
            up[i] = up[up[i]]
            i = up[i]
        return i

    ends = lambda w: (w["pts"][0], w["pts"][-1])
    for i, a in enumerate(wires):
        for j, b in enumerate(wires):
            if i == j or not any(_dist(e, b["pts"]) <= JOIN for e in ends(a)):
                continue
            if a["net"] == b["net"]:
                up[root(i)] = root(j)
            elif i < j or not any(_dist(e, a["pts"]) <= JOIN for e in ends(b)):
                found.append(f"the wire {_where(a['pts'])} of {a['net']} touches the wire "
                             f"{_where(b['pts'])} of {b['net']}")
    by_pin = {}
    for i, w in enumerate(wires):
        for p in w["ends"]:
            if p in by_pin:
                up[root(i)] = root(by_pin[p])
            by_pin[p] = i
    # a wire that ends on a rail symbol joins every other wire on that rail's symbols
    on_rail = {}
    for i, w in enumerate(wires):
        for rail, pts, filled in rails:
            if any(_dist(e, pts, filled) <= JOIN for e in ends(w)):
                if rail != w["net"]:
                    found.append(f"the wire {_where(w['pts'])} of {w['net']} ends on a {rail} "
                                 f"symbol")
                elif rail in on_rail:
                    up[root(i)] = root(on_rail[rail])
                else:
                    on_rail[rail] = i
    pieces = {}
    for i, w in enumerate(wires):
        if w["ends"] and w["net"] in nets:
            pieces.setdefault(w["net"], set()).add(root(i))
    for net, roots in sorted(pieces.items()):
        # a supply net is one node wherever a drawing shows it, at a symbol or at a pin
        if nets[net].rail:
            continue
        if len(roots) > 1:
            found.append(f"the pins drawn on {net} fall apart into {len(roots)} pieces")

    for ref in parts:
        # an IC is drawn with every pin; a connector or a plug-in module, such as the Teensy,
        # only with the pins the drawing needs
        if ref in pins_of and not ref.startswith("J-") and not re.match(r"A\d", ref):
            unwired = [f"{ref}.{p}" for p in pins_of[ref]
                       if f"{ref}.{p}" not in by_pin and f"{ref}.{p}" not in free]
            if unwired:
                found.append(f"draws {ref} without a wire to {', '.join(unwired)}")
    for msg in found:
        rep.error(f"{name}: {msg}", kind="schematic", key=name)
    return len(wires)


def pass_schematics(model, rep) -> int:
    """Every drawing of the model that annotates wires is held to the boards' netlists, its
    model's or those its data-boards names."""
    checked = 0
    for path in model.drawings:
        if not path.exists():
            continue
        text = path.read_text(encoding="utf-8")
        if "data-net=" in text and (model.boards or "data-boards=" in text):
            check_schematic(model, path, rep)
            checked += 1
    return checked
