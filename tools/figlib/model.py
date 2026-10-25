"""What a model file declares: its figures, requirements and curves, its nets, the board's pins."""
from __future__ import annotations

import ast
import builtins
import contextlib
import dataclasses
import functools
import inspect
import pathlib
import re
import sys

from .quantities import Q, as_q
from .repo import model_named


# `graph` is a reading off a plotted curve, which the sheet's text never prints
KINDS = ("datasheet", "graph", "measured", "assumed", "decision", "derived")

# a keyword a declaration leaves to its block, or to its own default outside one
_UNSET = object()


@dataclasses.dataclass
class Fig:
    key: str
    unit: str
    kind: str
    group: str | None = None          # how the document addresses it
    label: str | None = None          # disambiguator inside the group
    section: str | None = None        # where names collide across blocks
    src: str | None = None            # provenance, free text
    sheet: str | None = None          # file name under docs/datasheets/
    tol_units: float = 1.0            # tolerance, in units of the last printed digit
    rises_with: tuple = ()            # inputs this figure grows with
    falls_with: tuple = ()            # inputs this figure shrinks with
    fn: object | None = None
    deps: tuple = ()
    value: Q | None = None
    # True   the document states it on an addressable line, and it is compared there
    # "loose" the document states it in prose; the value has to appear in its section
    # False  the document does not state it
    stated: object = True
    # "down"  the document has to print it at or below the computed value
    # "up"    at or above
    # None    nearest, and either side is accepted
    prints: str | None = None
    body: str | None = None           # the formula, as it is written
    origin: str | None = None         # the model a taken figure is declared in
    free: frozenset = frozenset()     # the model file's own names an expression formula reads
    tracks: tuple = ()                # extremes a bound takes on purpose, because what it is
                                      # compared with moves with the same input


def _formula_text(src: str) -> str | None:
    """The body of a function formula, without its signature."""
    if not src:
        return None
    lines = src.split("\n")
    for i, l in enumerate(lines):
        if l.lstrip().startswith("def "):
            rest = [x for x in lines[i + 1:] if x.strip()]
            if not rest:
                return None
            pad = min(len(x) - len(x.lstrip()) for x in rest)
            return "\n".join(x[pad:] for x in rest)
    return None


# A number in a formula body outside an identifier or a string. 0, 1 and 2 appear
# as algebra; any other constant is a declared input with a source.
BODY_LITERAL_RE = re.compile(r"""(?<![\w.'"])\d+(?:\.\d+)?(?![\w.'"])""")
LITERAL_ALLOWED = {"0", "1", "2"}


def _literal_error(key, found):
    return ValueError(f"{key}: a formula may not carry a bare number. {found!r} "
                      f"belongs in the inputs, with the source it came from")


def _moves(key, moves: str):
    """The inputs a figure rises with, written +key, and falls with, written -key."""
    rises, falls = [], []
    for word in moves.split():
        if word[:1] not in ("+", "-") or not word[1:].isidentifier():
            raise ValueError(f"{key}: moves names each input as +key or -key, got {word!r}")
        (rises if word[0] == "+" else falls).append(word[1:])
    return tuple(rises), tuple(falls)


def _expression(key, text: str, scope: dict, lint=True):
    """The function an expression stands for, the keys it rests on, and the names of the
    model file it reads.

    A name the model file or Python itself defines is read from there, and every other
    name is a figure, in the order the expression first names them. `lint` holds the
    expression to the formulas' rule on bare numbers.
    """
    try:
        tree = ast.parse(text, mode="eval")
    except SyntaxError as e:
        raise ValueError(f"{key}: the formula is no Python expression: {e.msg}") from None
    names = []
    for node in ast.walk(tree):
        if isinstance(node, (ast.Lambda, ast.comprehension, ast.NamedExpr)):
            raise ValueError(f"{key}: a formula that binds names of its own is written "
                             f"as a function")
        if (lint and isinstance(node, ast.Constant) and not isinstance(node.value, bool)
                and isinstance(node.value, (int, float, complex))
                and ast.get_source_segment(text, node) not in LITERAL_ALLOWED):
            raise _literal_error(key, ast.get_source_segment(text, node))
        if isinstance(node, ast.Name):
            names.append(node)
    names.sort(key=lambda n: (n.lineno, n.col_offset))
    own = lambda name: name in scope or hasattr(builtins, name)
    deps = tuple(dict.fromkeys(n.id for n in names if not own(n.id)))
    free = frozenset(n.id for n in names if own(n.id))
    fn = eval(f"lambda {', '.join(deps)}: ({text})", scope)
    return fn, deps, free


DRIVER_KINDS = ("push-pull", "tri-state", "open-drain")
# the Teensy's own 3.3 V pin: the one rail a part on a Teensy pin may run from or pull to
TEENSY_RAIL = "teensy"


@dataclasses.dataclass
class Part:
    name: str
    role: str                   # one of DRIVER_KINDS, "contact", "pull" or "input"
    rail: str | None = None     # what powers an output, or where a pull goes
    select: str | None = None   # the signal that enables a tri-state output
    src: str = ""


@dataclasses.dataclass
class Net:
    name: str
    teensy: str | None          # the Teensy signal the net reaches, None for one off the Teensy
    parts: list
    model: str


def drives(name, kind, *, rail=None, select=None, src):
    """An output on a net: push-pull, tri-state with the select that enables it, or open-drain."""
    if kind not in DRIVER_KINDS:
        raise ValueError(f"{name}: an output is one of {DRIVER_KINDS}, got {kind!r}")
    if kind != "open-drain" and not rail:
        raise ValueError(f"{name}: a {kind} output names the rail that powers it")
    if kind == "tri-state" and not select:
        raise ValueError(f"{name}: a tri-state output names the select that enables it")
    return Part(name, kind, rail, select, src)


def contact(name, *, to="gnd", src):
    """A switch or a contact that closes the net to GND, or to the rail `to` names."""
    return Part(name, "contact", to, src=src)


def pull(name, rail, *, src):
    """A resistor from the net to a rail, "gnd" for a pull-down."""
    return Part(name, "pull", rail, src=src)


def reads(name, *, src):
    """An input that reads the net and drives nothing."""
    return Part(name, "input", src=src)


@dataclasses.dataclass
class PinData:
    """What each Teensy pin can carry, as the board's model declares it."""
    sources: list = dataclasses.field(default_factory=list)  # (what, src) of each declaration
    groups: dict = dataclasses.field(default_factory=dict)   # edge, sd, pads: their pins
    inputs: tuple = ()          # signals a pin reads
    open_drain: tuple = ()      # signals a pin drives open-drain
    optional: tuple = ()        # signals a port works without
    bidirectional: tuple = ()   # signals a pin drives and reads in turn
    ports: dict = dataclasses.field(default_factory=dict)    # port: [(signal, pins), ...]
    notes: dict = dataclasses.field(default_factory=dict)    # port: what its row adds
    timers: dict = dataclasses.field(default_factory=dict)   # timer: [(pin, channel), ...]
    flexio: dict = dataclasses.field(default_factory=dict)   # FlexIO module: [(pin, signal), ...]
    analog: list = dataclasses.field(default_factory=list)   # the pins of A0, A1, ...

    def kind(self, signal: str) -> str:
        """How a pin carrying `signal` drives its net, as a wiring part's role."""
        return ("input" if signal in self.inputs else
                "open-drain" if signal in self.open_drain else
                "tri-state" if signal in self.bidirectional else "push-pull")

    def pwm(self) -> list[int]:
        return sorted(p for pins in self.timers.values() for p, _ in pins)


class Model:
    def __init__(self, name: str, document: pathlib.Path, section: str, until: str,
                 drawings=(), documents=()):
        self.name, self.document = name, document
        self.documents = [pathlib.Path(d) for d in documents]
        self.section, self.until = section, until
        self.drawings = [pathlib.Path(d) for d in drawings]
        self.figs: dict[str, Fig] = {}
        self.invariants: list = []
        self.curves: list = []
        self.unlinted: list = []
        self.asides: list = []
        self.nets: list = []
        self.owned: list = []
        self.drawn: list = []
        self.supplied: list = []
        self.ignored: list = []       # (path, why) of files in the model's folder it leaves out
        self.skipped: list = []       # (path, heading, why) of sections of its documents it leaves out
        self.named: dict = {}         # designators of other boards and modules, with why
        self.ranges: list = []        # (nominal, low, high) of a quantity that varies
        self.path: pathlib.Path | None = None
        self.pin_data: PinData | None = None
        self._blocks: list[dict] = []

    @classmethod
    def at(cls, file, name, document="design.md", *, section=None, until="## Sources",
           drawings=(), documents=()):
        """The model a file declares, its documents and drawings named from the file's folder."""
        here = pathlib.Path(file).resolve().parent
        path = lambda p: (here / p).resolve()
        return cls(name, path(document), section, until,
                   [path(d) for d in drawings], [path(d) for d in documents])

    def declarers(self):
        """What a model file declares with: fig, then an input of each kind, in the order
        ds, gr, msr, asm, dec for datasheet, graph, measured, assumed and decision."""
        of = lambda kind: functools.partial(self.input, kind=kind)
        return (self.fig, of("datasheet"), of("graph"), of("measured"), of("assumed"),
                of("decision"))

    @contextlib.contextmanager
    def block(self, group=None, *, section=None, stated=True):
        """Where the document states the figures declared inside: their group, their
        section and how they are stated, unless a declaration names its own. A
        declaration the document does not state takes no group and no section from it."""
        self._blocks.append({"group": group, "section": section, "stated": stated})
        try:
            yield
        finally:
            self._blocks.pop()

    def _placed(self, group, section, stated, default_stated):
        """A declaration's group, section and stated: its own, else its block's."""
        block = self._blocks[-1] if self._blocks else None
        if stated is _UNSET:
            stated = block["stated"] if block else default_stated
        addressed = block is not None and stated is not False
        if group is _UNSET:
            group = block["group"] if addressed else None
        if section is _UNSET:
            section = block["section"] if addressed else None
        return group, section, stated

    def net(self, name, *parts, teensy=None):
        """A net of the subsystem: every part on it, and the Teensy signal it reaches."""
        self.nets.append(Net(name, teensy, list(parts), self.name))

    def owns(self, peripheral, why, *, serves=()):
        """A peripheral the subsystem's driver programs itself, which no other code may use. A
        bus that runs it for the drivers of other subsystems names those in `serves`."""
        self.owned.append((peripheral, why, tuple(serves)))

    def supplies(self, key, *, pool):
        """What a pool holds: the current a rail allows, a memory's size, a list's slots."""
        self.supplied.append((key, pool))

    def draws(self, key=None, *, pool):
        """What the subsystem takes from a pool in the machine: the figure `key`, or one slot."""
        self.drawn.append((key, pool))

    def uses(self, key, *, of, group=_UNSET, label=None, section=_UNSET, tol_units=1.0,
             stated=_UNSET):
        """A figure the model `of` declares, addressed the way this model's documents state it."""
        group, section, stated = self._placed(group, section, stated, False)
        return self._add(Fig(key, "", "taken", group, label, section, tol_units=tol_units,
                             stated=stated, origin=of))

    def as_written(self, key, *, of, unit):
        """The figure `of` as a listing writes it, a bare number of `unit`."""
        return self._add(Fig(key, "", "derived", fn=lambda x: Q.of(x.to(unit), ""),
                             deps=(of,), stated=False, body=f"{of} as a number of {unit}"))

    # -- the pins, which the board's model alone declares ------------------
    def _pins(self) -> PinData:
        if self.pin_data is None:
            self.pin_data = PinData()
        return self.pin_data

    def pin_groups(self, *, src, **groups):
        """The pins of each group of the board: the edge headers, the SD socket, the pads."""
        self._pins().groups.update({g: list(p) for g, p in groups.items()})
        self.pin_data.sources.append(("the pin groups", src))

    def port_roles(self, *, inputs, open_drain, optional, src, bidirectional=()):
        """How a pin carrying a signal drives its net, and the signals a port works without."""
        d = self._pins()
        d.inputs, d.open_drain, d.optional = tuple(inputs), tuple(open_drain), tuple(optional)
        d.bidirectional = tuple(bidirectional)
        d.sources.append(("the signals' directions", src))

    def port(self, name, *signals, note=None, src):
        """A port and its signals, each with the pins that can carry it."""
        d = self._pins()
        d.ports[name] = [(s[0], tuple(s[1:])) for s in signals]
        if note:
            d.notes[name] = note
        d.sources.append((name, src))

    def timer(self, name, *pins, src):
        """A timer and the pins it drives, each with the channel that drives it."""
        self._pins().timers[name] = [(p, str(ch)) for p, ch in pins]
        self.pin_data.sources.append((name, src))

    def flexio(self, name, *pins, src):
        """A FlexIO module and the pins it reaches, each with the FlexIO signal it carries."""
        self._pins().flexio[name] = [(p, str(n)) for p, n in pins]
        self.pin_data.sources.append((name, src))

    def analog(self, *pins, src):
        """The pins of A0, A1 and on, in order."""
        self._pins().analog = list(pins)
        self.pin_data.sources.append(("the analog inputs", src))

    def aside(self, text: str, why: str):
        """A number the prose states that the model does not compute."""
        self.asides.append((text.strip(), why))

    def ignores(self, path, why: str):
        """A markdown file or drawing in the model's folder that no model governs, and why."""
        here = self.path.parent if self.path else self.document.parent
        self.ignored.append(((here / path).resolve(), why))

    def skips(self, path, heading: str, why: str):
        """A section of one of the model's documents that states nothing of the design, such as
        a bench sketch, and why. It runs from its heading to the next heading of its level or
        a higher one."""
        here = self.path.parent if self.path else self.document.parent
        self.skipped.append(((here / path).resolve(), heading, why))

    def names(self, *designators, why: str):
        """Designators the document names that its own part list does not hold: a module's
        own parts, a signal written like a part."""
        for d in designators:
            self.named[d] = why

    def range(self, low: str, high: str, *, nominal: str | None = None):
        """The extremes of a quantity that varies, and its nominal value where it has one. A
        bound that rests on it takes the extreme that keeps it on its printed side."""
        self.ranges.append((nominal, low, high))

    # -- declaration -----------------------------------------------------
    def _add(self, f: Fig):
        if f.key in self.figs:
            raise KeyError(f"duplicate key {f.key!r}")
        self.figs[f.key] = f
        return f

    def input(self, key, value, unit, *, kind, src=None, sheet=None, group=_UNSET,
              label=None, section=_UNSET, tol_units=1.0, stated=_UNSET):
        group, section, stated = self._placed(group, section, stated, False)
        if kind not in KINDS or kind == "derived":
            raise ValueError(f"input kind must be one of {KINDS[:-1]}, got {kind!r}")
        if kind in ("datasheet", "graph") and not src:
            raise ValueError(f"{key}: a datasheet input needs its src")
        f = Fig(key, unit, kind, group, label, section, src, sheet, tol_units,
                value=Q.of(value, unit), stated=stated)
        return self._add(f)

    def fig(self, key, unit, formula, prints=None, *, moves="", group=_UNSET, label=None,
            section=_UNSET, tol_units=1.0, stated=_UNSET, tracks=""):
        """A derived figure.

        formula  a Python expression over the keys it rests on, or, for a formula that
                 needs statements, a function whose parameter names are those keys
        prints   "down" for a bound the document prints at or below the value, "up" for
                 one it prints at or above
        moves    the inputs it rises with as +key and falls with as -key, "+r_pd -i_led"
        tracks   extremes of a range a bound takes on purpose, "v_out_max", because the
                 quantity it is compared with moves with the same input
        """
        group, section, stated = self._placed(group, section, stated, True)
        rises, falls = _moves(key, moves)
        if prints not in (None, "up", "down"):
            raise ValueError(f"{key}: prints is 'up', 'down' or nothing")
        free = frozenset()
        if isinstance(formula, str):
            # the names of the model file that calls fig(), which the expression may read
            body = formula.strip()
            fn, deps, free = _expression(key, body, sys._getframe(1).f_globals)
        else:
            fn, deps = formula, tuple(inspect.signature(formula).parameters)
            try:
                body = _formula_text(inspect.getsource(formula))
            except OSError:
                # a formula built by exec has no source file; the run says how
                # many escaped the lint rather than passing them silently
                self.unlinted.append(key)
                body = None
            for found in BODY_LITERAL_RE.findall(
                    "".join(l.split("#")[0] for l in (body or "").split("\n"))):
                if found not in LITERAL_ALLOWED:
                    raise _literal_error(key, found)
        return self._add(Fig(key, unit, "derived", group, label, section, None, None,
                             tol_units, rises, falls, fn=fn, deps=deps, stated=stated,
                             prints=prints, body=body, free=free,
                             tracks=tuple(tracks.split())))

    def invariant(self, description, check):
        """A relation that has to hold whatever the formulas are: an expression over the
        keys, or a function that reads them as attributes of its one argument."""
        if isinstance(check, str):
            fn, keys, free = _expression(description, check.strip(),
                                         sys._getframe(1).f_globals, lint=False)

            def holds(v):
                clash = sorted(n for n in free if hasattr(v, n))
                if clash:
                    raise ValueError(f"the requirement reads {clash} from the model file, "
                                     f"where a figure has the same name")
                return fn(*(getattr(v, k) for k in keys))
            check = holds
        self.invariants.append((description, check))

    def curve(self, name, points, *, rises=True, on=(), bounded=()):
        """A plotted curve, as the points read off it, against which its readings are checked.

        points   [(x_key, y_key), ...] in reading order
        rises    whether y grows with x along the curve
        on       [(x, y_key), ...] readings that have to sit on this curve, x
                 being any declared quantity, a derived operating point included
        bounded  [(y_key, lo_key, hi_key), ...] a point the sheet's table brackets
        """
        self.curves.append((name, list(points), rises, list(on), list(bounded)))

    # -- evaluation ------------------------------------------------------
    def recompute(self, overrides: dict) -> dict:
        """Every value again, with some inputs replaced. The model is untouched."""
        vals = dict(overrides)

        def get(k):
            if k not in vals:
                f = self.figs[k]
                vals[k] = f.value if f.fn is None else as_q(f.fn(*(get(d) for d in f.deps)))
            return vals[k]
        return {k: get(k) for k in self.figs}

    def evaluate(self):
        for k in self.figs:
            self.figure(k)

    def figure(self, key, _trail=()) -> Fig:
        """One figure, evaluated with everything it rests on, in its own model if it is taken.

        Models evaluate figure by figure, so two may take figures from each other.
        """
        f = self.figs.get(key)
        if f is None:
            raise KeyError(f"{_trail[-1][1] if _trail else '?'} depends on unknown {key!r}")
        if f.value is not None:
            return f
        here = (self.name, key)
        if here in _trail:
            raise ValueError("cycle: " + " -> ".join(f"{m}.{k}" for m, k in _trail + (here,)))
        trail = _trail + (here,)
        if f.free & self.figs.keys():
            raise ValueError(f"{key}: the formula reads {sorted(f.free & self.figs.keys())} "
                             f"from the model file, where a figure has the same name")
        if f.origin is not None:
            source = model_named(f.origin)
            if key not in source.figs:
                raise KeyError(f"{self.name} takes {key!r} from {f.origin}, which does "
                               f"not declare it")
            g = source.figure(key, trail)
            f.unit, f.kind, f.src, f.sheet, f.prints = g.unit, g.kind, g.src, g.sheet, g.prints
            f.value = g.value
            return f
        got = as_q(f.fn(*(self.figure(d, trail).value for d in f.deps)))
        try:
            got.to(f.unit)
        except TypeError as e:
            raise TypeError(f"{key}: {e}") from None
        f.value = got
        return f

    # -- graph queries ---------------------------------------------------
    def dependents(self, key) -> list[str]:
        out, frontier = set(), [key]
        while frontier:
            cur = frontier.pop()
            for k, f in self.figs.items():
                if cur in f.deps and k not in out:
                    out.add(k)
                    frontier.append(k)
        return sorted(out)

    def ancestors(self, key) -> list[str]:
        out, frontier = set(), [key]
        while frontier:
            cur = frontier.pop()
            for d in self.figs[cur].deps:
                if d not in out:
                    out.add(d)
                    frontier.append(d)
        return sorted(out)
