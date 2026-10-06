#!/usr/bin/env python3
"""Recompute the figures a design document states, and check the document against them.

A model file declares two kinds of quantity:

  inputs    a datasheet reading, a measurement, an assumption or a decision.
            Each carries its provenance: the document it came from, its
            revision, and the table or figure inside it.
  derived   a function of other quantities. Its parameter names are its
            dependencies, so the dependency graph is the code.

Every quantity carries a unit, and the unit is a type: `mA - µs` raises rather
than returning a number. Every quantity that the document states also carries
the group and the label the document addresses it by, so a figure is compared
against the line that states it and not against the file as a whole.

Passes:

  anchored   each stated quantity is located by the value it computes, inside
             the group and section that hold it. Rewording a line therefore
             costs nothing, and a changed figure has nowhere to land: the
             report names what its group actually carries instead.
  orphans    each token inside a fenced block or an appendix table is either
             anchored by a declaration or equal to some declared quantity. One
             that is neither is a stale number or a derivation without a formula.
  expression tokens left of a relation sign are references to inputs; each has
             to equal a declared quantity of the same dimension.
  direction  each figure declares which inputs it rises and falls with, and the
             checker perturbs each one to verify the sign. A bound taken at the
             wrong extreme satisfies the arithmetic and fails here, which is
             the failure rule 3 names: picking the wrong direction of a
             parameter leaves no trace in the result.
  invariants relations that hold whatever the formula is: a least case under a
             nominal under a worst case, a current inside its supply.
  literals   a formula may not carry a bare number. Every constant is a
             declared input with a source, or it is invisible to --provenance.
  sums       every datasheet named by an input is present under docs/datasheets/
             and matches its recorded SHA-256.
  stale      numbers a `git diff` removed that still stand somewhere in the tree.
  datasheets --sheets looks every datasheet reading up in the PDF it cites. A
             sheet this reader cannot decode is reported unread, so its
             readings stay unconfirmed rather than counting as checked.

A figure that states a bound declares the side it is printed on, `prints="down"`
for a ceiling and `prints="up"` for a floor, and its tolerance is one-sided.
Nearest rounding turns a 23.571 kOhm ceiling into "<= 24 kOhm", which the design
does not meet, and a symmetric tolerance accepts it.

--write puts every computed figure into the document and the drawings, rounded
that way, so a figure is typed in the model only. It reports each edit, and
reports the figure it cannot place rather than guessing at one.

Self-test: --mutate perturbs every anchored token past its tolerance in memory
and requires the anchored pass to catch each one. A token no perturbation can
break is not being checked.
"""
from __future__ import annotations

import argparse
import dataclasses
import hashlib
import importlib.util
import inspect
import math
import pathlib
import re
import subprocess
import sys
from collections import defaultdict

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

ROOT = pathlib.Path(__file__).resolve().parents[1]
DATASHEETS = ROOT / "docs" / "datasheets"
SUMS = DATASHEETS / "SHA256SUMS"

# ---------------------------------------------------------------------------
# units
# ---------------------------------------------------------------------------
# Dimensions are exponents of (volt, ampere, second, kelvin). Everything
# electrical reduces to those three, and temperature never enters an
# expression, so it only has to be kept apart from the rest.
DIMLESS = (0, 0, 0, 0, 0)
_V, _A, _S, _K = ((1, 0, 0, 0, 0), (0, 1, 0, 0, 0), (0, 0, 1, 0, 0),
                  (0, 0, 0, 1, 0))
_M = (0, 0, 0, 0, 1)
_MPS = (0, 0, -1, 0, 1)
_OHM, _WATT, _FARAD, _HENRY, _HZ, _COUL = (
    (1, -1, 0, 0, 0), (1, 1, 0, 0, 0), (-1, 1, 1, 0, 0), (1, -1, 1, 0, 0),
    (0, 0, -1, 0, 0), (0, 1, 1, 0, 0))

UNITS: dict[str, tuple[float, tuple]] = {}


def _family(dim, entries):
    for name, scale in entries:
        UNITS[name] = (scale, dim)


_family(_V, [("V", 1.0), ("mV", 1e-3), ("µV", 1e-6), ("kV", 1e3)])
_family(_A, [("A", 1.0), ("mA", 1e-3), ("µA", 1e-6), ("nA", 1e-9)])
_family(_S, [("s", 1.0), ("ms", 1e-3), ("µs", 1e-6), ("ns", 1e-9)])
_family(_OHM, [("Ω", 1.0), ("mΩ", 1e-3), ("kΩ", 1e3), ("MΩ", 1e6)])
_family(_WATT, [("W", 1.0), ("mW", 1e-3), ("µW", 1e-6)])
_family(_FARAD, [("F", 1.0), ("µF", 1e-6), ("nF", 1e-9), ("pF", 1e-12)])
_family(_HENRY, [("H", 1.0), ("mH", 1e-3), ("µH", 1e-6)])
_family(_HZ, [("Hz", 1.0), ("kHz", 1e3), ("MHz", 1e6)])
_family(_COUL, [("C", 1.0), ("µC", 1e-6), ("nC", 1e-9)])
_family(_K, [("°C", 1.0)])
_family(_M, [("m", 1.0), ("cm", 1e-2), ("mm", 1e-3)])
_family(_MPS, [("m/s", 1.0), ("mm/s", 1e-3)])
# Counts share the dimensionless family, so the unit check cannot tell a byte
# from a sample; a model keeps them apart by name.
_family(DIMLESS, [("%", 0.01), ("×", 1.0), ("steps", 1.0), ("step", 1.0),
                  ("τ_adc", 1.0), ("τ", 1.0), ("clocks", 1.0), ("clock", 1.0),
                  ("dB", 1.0), ("bits", 1.0), ("bytes", 1.0), ("KB", 1024.0),
                  ("samples", 1.0), ("", 1.0)])

DIM_NAMES = {DIMLESS: "1", _V: "V", _A: "A", _S: "s", _K: "°C", _OHM: "Ω",
             _WATT: "W", _FARAD: "F", _HENRY: "H", _HZ: "Hz", _COUL: "C",
             _M: "m", _MPS: "m/s"}


# Distinct words below which an extracted sheet is a scan rather than a sheet.
# The lowest real datasheet in docs/datasheets yields 83, an image scan yields 8.
SHEET_WORDS = 40


def _dimstr(d):
    return DIM_NAMES.get(d) or "·".join(
        f"{n}^{e}" for n, e in zip("V A s K m".split(), d) if e)


class Q:
    """A quantity: a magnitude in base units plus a dimension."""

    __slots__ = ("v", "d")

    def __init__(self, v: float, d=DIMLESS):
        self.v, self.d = float(v), tuple(d)

    # -- construction / display ------------------------------------------
    @staticmethod
    def of(value: float, unit: str) -> "Q":
        if unit not in UNITS:
            raise KeyError(f"unknown unit {unit!r}")
        scale, dim = UNITS[unit]
        return Q(value * scale, dim)

    def to(self, unit: str) -> float:
        scale, dim = UNITS[unit]
        if dim != self.d:
            raise TypeError(f"cannot read {_dimstr(self.d)} as {unit} ({_dimstr(dim)})")
        return self.v / scale

    def __repr__(self):
        return f"Q({self.v:g} {_dimstr(self.d)})"

    def show(self, unit: str, decimals: int | None = None) -> str:
        x = self.to(unit)
        s = f"{x:.{decimals}f}" if decimals is not None else f"{x:g}"
        return f"{s} {unit}".strip()

    # -- arithmetic -------------------------------------------------------
    def _same(self, o, op):
        if self.d != o.d:
            raise TypeError(f"cannot {op} {_dimstr(self.d)} and {_dimstr(o.d)}")

    def __add__(self, o):
        o = _q(o)
        self._same(o, "add")
        return Q(self.v + o.v, self.d)

    def __sub__(self, o):
        o = _q(o)
        self._same(o, "subtract")
        return Q(self.v - o.v, self.d)

    __radd__ = __add__

    def __rsub__(self, o):
        return _q(o) - self

    def __mul__(self, o):
        o = _q(o)
        return Q(self.v * o.v, tuple(a + b for a, b in zip(self.d, o.d)))

    __rmul__ = __mul__

    def __truediv__(self, o):
        o = _q(o)
        return Q(self.v / o.v, tuple(a - b for a, b in zip(self.d, o.d)))

    def __rtruediv__(self, o):
        return _q(o) / self

    def __pow__(self, n: int):
        return Q(self.v ** n, tuple(e * n for e in self.d))

    def __neg__(self):
        return Q(-self.v, self.d)

    def __abs__(self):
        return Q(abs(self.v), self.d)

    def _cmp(self, o):
        o = _q(o)
        self._same(o, "compare")
        return self.v, o.v

    def __lt__(self, o):
        a, b = self._cmp(o)
        return a < b

    def __le__(self, o):
        a, b = self._cmp(o)
        return a <= b

    def __gt__(self, o):
        a, b = self._cmp(o)
        return a > b

    def __ge__(self, o):
        a, b = self._cmp(o)
        return a >= b

    @property
    def raw(self) -> float:
        if self.d != DIMLESS:
            raise TypeError(f"{_dimstr(self.d)} is not a plain number")
        return self.v


def _q(x) -> Q:
    return x if isinstance(x, Q) else Q(x)


def db(ratio):
    """A voltage ratio in decibels."""
    return Q.of(20 * math.log10(_q(ratio).raw), "dB")


def from_db(gain):
    """The voltage ratio a gain in decibels stands for, the inverse of db()."""
    return Q(10 ** (_q(gain).raw / 20))


def ln(x):
    return Q(math.log(_q(x).raw))


def log10(x):
    return Q(math.log10(_q(x).raw))


def exp(x):
    return Q(math.exp(_q(x).raw))


def sqrt(x):
    q = _q(x)
    if any(e % 2 for e in q.d):
        raise TypeError(f"cannot take the root of {_dimstr(q.d)}")
    return Q(math.sqrt(q.v), tuple(e // 2 for e in q.d))


def ceil_to(x, unit: str, step: float):
    """Round a quantity up to the next multiple of `step` display units."""
    return Q.of(math.ceil(_q(x).to(unit) / step) * step, unit)


def floor_to(x, unit: str, step: float):
    """Round a quantity down to the next multiple of `step` display units."""
    return Q.of(math.floor(_q(x).to(unit) / step) * step, unit)


def interp_log(points, x):
    """Log-linear interpolation through published curve points."""
    pts = sorted(points, key=lambda p: _q(p[0]).v)
    xs = _q(x)
    if xs <= _q(pts[0][0]):
        (x0, y0), (x1, y1) = pts[0], pts[1]
    elif xs >= _q(pts[-1][0]):
        (x0, y0), (x1, y1) = pts[-2], pts[-1]
    else:
        for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
            if _q(x0) <= xs <= _q(x1):
                break
    f = math.log(xs.v / _q(x0).v) / math.log(_q(x1).v / _q(x0).v)
    return _q(y0) + (_q(y1) - _q(y0)) * f


# ---------------------------------------------------------------------------
# the model registry
# ---------------------------------------------------------------------------
# `graph` is a datasheet reading taken off a plotted curve. It carries the
# same weight as a table reading and cannot be looked up as text, because
# the sheet never prints it.
KINDS = ("datasheet", "graph", "measured", "assumed", "decision", "derived")


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


def _formula_text(src: str) -> str | None:
    """The body of a formula, without its decorator or its signature."""
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


# ---------------------------------------------------------------------------
# Wiring: what sits on a net, as a model declares it
# ---------------------------------------------------------------------------
# A push-pull output drives its net whenever it is powered. A tri-state output
# drives it only while its select is active, and an open-drain output and a
# contact only pull it low. A pull is a resistor to a rail, "gnd" for a pull-down.
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
    ports: dict = dataclasses.field(default_factory=dict)    # port: [(signal, pins), ...]
    notes: dict = dataclasses.field(default_factory=dict)    # port: what its row adds
    timers: dict = dataclasses.field(default_factory=dict)   # timer: [(pin, channel), ...]
    flexio: dict = dataclasses.field(default_factory=dict)   # FlexIO module: [(pin, signal), ...]
    analog: list = dataclasses.field(default_factory=list)   # the pins of A0, A1, ...

    def kind(self, signal: str) -> str:
        """How a pin carrying `signal` drives its net, as a wiring part's role."""
        return ("input" if signal in self.inputs else
                "open-drain" if signal in self.open_drain else "push-pull")

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
        self.path: pathlib.Path | None = None
        self.pin_data: PinData | None = None

    def net(self, name, *parts, teensy=None):
        """A net of the subsystem: every part on it, and the Teensy signal it reaches.

        `teensy` is the signal as docs/pin-assignment.md names it up to the first
        comma. Nets that reach one Teensy pin merge across the models, so a second
        subsystem on a pin meets the first one's parts there.
        """
        self.nets.append(Net(name, teensy, list(parts), self.name))

    def owns(self, peripheral, why):
        """A peripheral the subsystem's driver programs itself, which no other code may use."""
        self.owned.append((peripheral, why))

    def supplies(self, key, *, pool, call=None):
        """What a pool holds: the current a rail allows, a memory's size, a list's slots.

        `call` is how a listing takes one of the pool's slots, as `driverTick.attach(`
        does, and every model's documents then take as many slots as the model draws.
        """
        self.supplied.append((key, pool, call))

    def draws(self, key=None, *, pool):
        """What the subsystem takes from a pool in the machine: the figure `key`, or one slot.

        The draws of every model add up against the one figure that supplies the
        pool, since each model alone sees only its share of it. On the bench a
        subsystem runs alone, so its own model checks that case.
        """
        self.drawn.append((key, pool))

    def uses(self, key, *, of, group=None, label=None, section=None, tol_units=1.0,
             stated=False):
        """A figure the model `of` declares, addressed the way this model's documents state it.

        The value, the unit, the kind and the source stay with that model, so the
        figure is typed once and every document that states it follows it.
        """
        return self._add(Fig(key, "", "taken", group, label, section, tol_units=tol_units,
                             stated=stated, origin=of))

    def as_written(self, key, *, of, unit):
        """The figure `of` as a listing writes it, a bare number of `unit`.

        A constant such as `kPeriodUs = 5000` then follows the period it encodes,
        and a period that moves leaves the old number with nothing to match.
        """
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

    def port_roles(self, *, inputs, open_drain, optional, src):
        """How a pin carrying a signal drives its net, and the signals a port works without."""
        d = self._pins()
        d.inputs, d.open_drain, d.optional = tuple(inputs), tuple(open_drain), tuple(optional)
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
        """A number the prose states that the model does not compute.

        A quoted datasheet row, a package pitch, a bare count. Every other
        number in the prose has to be a declared quantity, which is what keeps
        a figure from going stale where no block or table holds it: the value
        stops matching, and nothing else would have said so.
        """
        self.asides.append((text.strip(), why))

    # -- declaration -----------------------------------------------------
    def _add(self, f: Fig):
        if f.key in self.figs:
            raise KeyError(f"duplicate key {f.key!r}")
        self.figs[f.key] = f
        return f

    def input(self, key, value, unit, *, kind, src=None, sheet=None, group=None,
              label=None, section=None, tol_units=1.0, stated=False):
        if kind not in KINDS or kind == "derived":
            raise ValueError(f"input kind must be one of {KINDS[:-1]}, got {kind!r}")
        if kind in ("datasheet", "graph") and not src:
            raise ValueError(f"{key}: a datasheet input needs its src")
        f = Fig(key, unit, kind, group, label, section, src, sheet, tol_units,
                value=Q.of(value, unit), stated=stated)
        return self._add(f)

    def derived(self, key, unit, *, group=None, label=None, section=None,
                tol_units=1.0, stated=True, rises_with=(), falls_with=(),
                prints=None):
        def deco(fn):
            # a keyword default names the dependency, so a generated row can
            # bind its own input without writing the value into the formula
            sig = inspect.signature(fn)
            deps = tuple(pm.default if isinstance(pm.default, str) else name
                         for name, pm in sig.parameters.items())
            try:
                src = inspect.getsource(fn)
            except OSError:
                # a formula built by exec has no source file; the run says how
                # many escaped the lint rather than passing them silently
                self.unlinted.append(key)
                src = ""
            body = _formula_text(src)
            for found in BODY_LITERAL_RE.findall(
                    "".join(l.split("#")[0] for l in (body or "").split("\n"))):
                if found in LITERAL_ALLOWED:
                    continue
                raise ValueError(
                    f"{key}: a formula may not carry a bare number. {found!r} "
                    f"belongs in the inputs, with the source it came from")
            if prints not in (None, "up", "down"):
                raise ValueError(f"{key}: prints is 'up', 'down' or nothing")
            self._add(Fig(key, unit, "derived", group, label, section, None, None,
                          tol_units, tuple(rises_with), tuple(falls_with),
                          fn=fn, deps=deps, stated=stated, prints=prints, body=body))
            return fn
        return deco

    def invariant(self, description, fn):
        """A relation that has to hold whatever the formulas are."""
        self.invariants.append((description, fn))

    def curve(self, name, points, *, rises=True, on=(), bounded=()):
        """A plotted curve, as the points read off it.

        A curve reading cannot be looked up in the sheet's text, so it is
        checked against its own curve instead. That is internal consistency
        and not proof: it catches a transposed digit, a point read off the
        wrong axis and a reading that contradicts the sheet's own table.

        points   [(x_key, y_key), ...] in reading order
        rises    whether y grows with x along the curve
        on       [(x, y_key), ...] readings that have to sit on this curve, x
                 being any declared quantity, a derived operating point included
        bounded  [(y_key, lo_key, hi_key), ...] a point the sheet's table brackets
        """
        self.curves.append((name, list(points), rises, list(on), list(bounded)))

    # -- evaluation ------------------------------------------------------
    def order(self):
        """Dependency order, so a figure is computed after everything it needs."""
        order, mark = [], {}

        def visit(k, trail):
            if mark.get(k) == 2:
                return
            if mark.get(k) == 1:
                raise ValueError("cycle: " + " -> ".join(trail + [k]))
            if k not in self.figs:
                raise KeyError(f"{trail[-1] if trail else '?'} depends on unknown {k!r}")
            mark[k] = 1
            for d in self.figs[k].deps:
                visit(d, trail + [k])
            mark[k] = 2
            order.append(k)

        for k in list(self.figs):
            visit(k, [])
        return order

    def recompute(self, overrides: dict) -> dict:
        """Every value again, with some inputs replaced. The model is untouched."""
        vals = {k: f.value for k, f in self.figs.items()}
        vals.update(overrides)
        for k in self.order():
            f = self.figs[k]
            if f.fn is None or k in overrides:
                continue
            vals[k] = _q(f.fn(*(vals[d] for d in f.deps)))
        return vals

    def evaluate(self):
        for k in self.order():
            self.figure(k)

    def figure(self, key, _trail=()) -> Fig:
        """One figure, evaluated with everything it rests on, in its own model if it is taken.

        Two models may take figures from each other, as the firmware takes the
        solenoids' pull-in while the solenoids take its watchdog. The models then
        evaluate figure by figure, so only a figure that rests on itself is a cycle.
        """
        f = self.figs[key]
        if f.value is not None:
            return f
        here = (self.name, key)
        if here in _trail:
            raise ValueError("cycle: " + " -> ".join(f"{m}.{k}" for m, k in _trail + (here,)))
        trail = _trail + (here,)
        if f.origin is not None:
            source = model_named(f.origin)
            if key not in source.figs:
                raise KeyError(f"{self.name} takes {key!r} from {f.origin}, which does "
                               f"not declare it")
            g = source.figure(key, trail)
            f.unit, f.kind, f.src, f.sheet, f.prints = g.unit, g.kind, g.src, g.sheet, g.prints
            f.value = g.value
            return f
        got = _q(f.fn(*(self.figure(d, trail).value for d in f.deps)))
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


# ---------------------------------------------------------------------------
# the document
# ---------------------------------------------------------------------------
UNIT_ALT = "|".join(re.escape(u) for u in
                    sorted((u for u in UNITS if u and u != "×"), key=len, reverse=True))
NUM = r"\d+(?:\.\d+)?"
TOKEN_RE = re.compile(rf"(?<![\w.])({NUM})\s?({UNIT_ALT})(?![\wµ°])")
RATIO_RE = re.compile(rf"(?<![\w.])({NUM})(×)")
BARE_RE = re.compile(rf"(?<![\w.])({NUM})(?![\w.]|\s?[A-Za-zµ°%×τ])")
RANGE_RE = re.compile(rf"(?<![\w.])({NUM})\s*(?:…|\.\.\.| to )\s*({NUM})\s?({UNIT_ALT})(?![\wµ°])")
RELATION_RE = re.compile(r"[=≈→≤≥]")
# a line that states a bound rather than a value: the printed figure has to be
# on the side that keeps the inequality true
INEQUALITY_RE = re.compile(r"[<>≤≥]")
# A number in a formula body, ignoring one that is part of an identifier or
# sits inside a string. 0, 1 and 2 stay: they appear as algebra, a factor of
# two for a there-and-back path, the base of a binary code, a difference from
# unity. Anything else names a quantity and belongs in the inputs with a source.
BODY_LITERAL_RE = re.compile(r"""(?<![\w.'"])\d+(?:\.\d+)?(?![\w.'"])""")
LITERAL_ALLOWED = {"0", "1", "2"}
NAME_END_RE = re.compile(r"\s{2,}|[=≈→≤≥]|:\s")


@dataclasses.dataclass
class Token:
    line: int
    col: int
    text: str
    value: Q
    unit: str
    decimals: int
    precision: float
    context: str
    group: str | None
    section: str | None
    strict: bool
    rhs: bool
    claimed_by: str | None = None
    doc: pathlib.Path | None = None
    cell: bool = False      # a table cell holding a bare number, which nothing requires to be claimed


def _precision(num: str) -> float:
    """The grid the document printed this figure on, in display units.

    `3.150` is three decimals, so 0.001, and an integer is on the unit grid.
    Where a document rounds more coarsely than its last digit, the declaration
    widens the tolerance itself with `tol_units`: a guess at the author's
    rounding would let a wrong figure through, as `20 ns` against a computed
    15 ns did.
    """
    return 10.0 ** -len(num.split(".")[1]) if "." in num else 1.0


def _decimals(num: str) -> int:
    return len(num.split(".")[1]) if "." in num else 0


def _norm(text: str) -> str:
    return (text.replace("Ω", "Ω").replace("μ", "µ")
                .replace("&#937;", "Ω").replace("&#181;", "µ"))


def group_name(line: str) -> str | None:
    """The name a code-block line in the first column gives its group.

    Everything up to the first double space, relation sign or colon. A name
    that is itself a figure token, as in a block that sums times, names nothing.
    """
    m = NAME_END_RE.search(line)
    name = (line[:m.start()] if m else line).strip()
    if not name:
        return None
    if TOKEN_RE.fullmatch(name) or re.fullmatch(NUM, name):
        # A part can be named by its value, as the dissipation block names each
        # resistor. A block that sums times starts its lines the same way, and
        # there the leading value is a term. The relation sign tells them apart:
        # a named row states a result, a summed term does not.
        rest = line[m.end():] if m else ""
        if not RELATION_RE.search(rest):
            return None
    return name


def scan_tokens(text: str, section: str | None, group: str | None,
                line_no: int, strict: bool, offset: int = 0) -> list[Token]:
    found, consumed = [], []
    rel = RELATION_RE.search(text)
    for m in RANGE_RE.finditer(text):
        lo, hi, unit = m.groups()
        consumed.append(m.span())
        found.append((m.start(), lo, unit, strict))
        found.append((m.start() + 1, hi, unit, strict))
    for pattern in (TOKEN_RE, RATIO_RE):
        for m in pattern.finditer(text):
            if not any(a <= m.start() < b for a, b in consumed):
                found.append((m.start(), m.group(1), m.group(2), strict))
                consumed.append(m.span())
    # A number with no unit can be addressed, as the swing block's 0.061 is,
    # but it is never required to be claimed: an expression is full of them.
    if strict and rel:
        for m in BARE_RE.finditer(text):
            if m.start() > rel.start() and not any(a <= m.start() < b
                                                   for a, b in consumed):
                found.append((m.start(), m.group(1), "", False))
    return [Token(line_no, pos + offset, f"{num} {unit}".strip(), Q.of(float(num), unit), unit,
                  _decimals(num), _precision(num), text, group, section, is_strict,
                  rhs=bool(rel and pos > rel.start()))
            for pos, num, unit, is_strict in sorted(found)]


NEVER = chr(0x0a)   # no line starts with a newline, so an extra document is read whole


def parse_document(path: pathlib.Path, section_head: str | None,
                   until: str) -> list[Token]:
    """Every figure token in the document, with the group and section holding it.

    `section_head` of None starts at the first line, so the main body is scanned
    on the same terms as the appendix: a number in a table or a fenced block
    there has to trace back to a declared quantity as well.
    """
    body = _norm(path.read_text(encoding="utf-8"))
    lines = body.split("\n")
    start = 0 if section_head is None else next(
        i for i, l in enumerate(lines) if l.startswith(section_head))
    end = next((i for i, l in enumerate(lines) if i > start and l.startswith(until)),
               len(lines))
    tokens, section, group, infence = [], None, None, False
    for i in range(start, end):
        raw = lines[i]
        if raw.strip().startswith("```"):
            infence = not infence
            group = None
            continue
        m = re.match(r"\*\*(.+?)\.?\*\*", raw) or re.match(r"#{2,5}\s+(.+?)\s*$", raw)
        if m and not infence:
            section = m.group(1)
        if infence:
            if raw and not raw.startswith(" "):
                group = group_name(raw) or group
            tokens += scan_tokens(raw, section, group or section, i + 1, strict=True)
        elif raw.startswith("| ") and "---" not in raw:
            cells, cursor = [], 0
            for part in raw.strip().strip("|").split("|"):
                text = part.strip()
                col = raw.index(text, cursor) if text else cursor
                cursor = col + len(text)
                cells.append((text, col))
            head = re.sub(r"\*\*", "", cells[0][0])
            for text, col in cells[1:]:
                if re.fullmatch(NUM, text):
                    # A bare number in a cell, such as an interrupt priority, can be
                    # the line a dimensionless figure states. A pin or a count in a
                    # table is one as well, so none is required to be claimed.
                    tokens.append(Token(i + 1, col, text, Q.of(float(text), ""), "",
                                        _decimals(text), _precision(text), text,
                                        head or section, section, False, False, cell=True))
                    continue
                tokens += scan_tokens(text, section, head or section, i + 1,
                                      strict=True, offset=col)
        else:
            tokens += scan_tokens(raw, section, None, i + 1, strict=False)
    for t in tokens:
        t.doc = path
    return tokens


# ---------------------------------------------------------------------------
# passes
# ---------------------------------------------------------------------------
class Report:
    def __init__(self):
        self.errors: list[str] = []
        self.notes: list[str] = []
        # (kind, key) beside each error, so a caller counts or looks one up
        # without matching on the message text
        self.tags: list[tuple[str, str]] = []

    def error(self, msg, kind="", key=""):
        self.errors.append(msg)
        self.tags.append((kind, key))

    def count(self, kind):
        return sum(1 for k, _ in self.tags if k == kind)

    def has(self, kind, key):
        return (kind, key) in self.tags

    def note(self, msg):
        self.notes.append(msg)


def tolerance(f: Fig, tok: Token | None) -> Q:
    grid = tok.precision if tok else 0.001
    return Q.of(f.tol_units * grid, f.unit)


def within(f: Fig, tok: Token | None, v: Q) -> bool:
    """Whether a printed value may stand for this figure.

    A figure declared with `prints` is a bound, a margin or a budget that the
    document has to round one way. Nearest rounding turns a 23.571 kOhm
    ceiling into "<= 24 kOhm", which the design does not satisfy, and the
    symmetric tolerance accepts it. One-sided, the same figure has to be
    printed at 23 kOhm or the run fails.
    """
    tol = tolerance(f, tok).v
    d = (v - f.value).v
    eps = 1e-9 * max(abs(f.value.v), tol, 1e-30)
    if f.prints == "down":
        return -tol <= d <= eps
    if f.prints == "up":
        return -eps <= d <= tol
    return abs(d) <= tol


def print_as(f: Fig, decimals: int) -> str:
    """The figure as the document has to print it, rounded the declared way."""
    v, step = f.value.to(f.unit), 10.0 ** -decimals
    if f.prints == "down":
        v = math.floor(v / step + 1e-9) * step
    elif f.prints == "up":
        v = math.ceil(v / step - 1e-9) * step
    return f"{v:.{decimals}f}"


def _candidates(f: Fig, tokens, override):
    """Free tokens in the figure's group whose unit and value match it."""
    out = []
    for t in tokens:
        if (t.claimed_by is not None or t.group != f.group or t.unit != f.unit
                or (f.section is not None and f.section not in (t.section or ""))
                or (f.label is not None and f.label not in t.context)):
            continue
        v = override.get(id(t), t.value) if override else t.value
        if within(f, t, v):
            out.append((abs(v - f.value).v, t.line, t))
    return sorted(out, key=lambda p: p[:2])


def pass_anchored(model: Model, tokens: list[Token], rep: Report, override=None):
    """Address each stated figure by the value it computes, inside its group.

    Locating by value rather than by a phrase means rewording a line costs
    nothing, while a changed figure has nowhere to land and is reported against
    what its group actually carries. Where two figures of one group compute the
    same value the assignment is greedy by distance, so one is left without a
    token and says so.
    """
    pending = [f for f in model.figs.values() if f.stated is True]
    hits = 0
    while pending:
        progress = False
        # a figure with exactly one home takes it first, which constrains the rest
        for only_certain in (True, False):
            for f in list(pending):
                cands = _candidates(f, tokens, override)
                if not cands or (only_certain and len(cands) > 1):
                    continue
                cands[0][2].claimed_by = f.key
                pending.remove(f)
                hits += 1
                progress = True
            if progress:
                break
        if not progress:
            break
    for f in pending:
        carried = [t for t in tokens
                   if t.group == f.group and t.unit == f.unit
                   and (f.section is None or f.section in (t.section or ""))]
        free = [t for t in carried if t.claimed_by is None] or carried
        if len(free) == 1:
            t = free[0]
            rep.error(f"{_rel(t.doc)}:{t.line} reads {t.text} where "
                      f"{f.key} computes {f.value.show(f.unit, t.decimals)}",
                      kind="unplaced", key=f.key)
        else:
            where = ", ".join(f"{t.text} on line {t.line}" for t in free[:5]) or "nothing"
            rep.error(f"{f.key} computes {f.value.show(f.unit)} and the "
                      f"{f.group!r} block has no line for it. It holds {where}.",
                      kind="unplaced", key=f.key)

    if override is None:
        claimed = {t.claimed_by: t for t in tokens if t.claimed_by}
        # only a computed figure has a rounding side to declare; an input is a
        # reading or a decision and is printed as it stands
        undeclared = sorted(
            k for k, t in claimed.items()
            if model.figs[k].kind == "derived" and model.figs[k].prints is None
            and INEQUALITY_RE.search(t.context))
        if undeclared:
            rep.note(f"{len(undeclared)} figures sit on a line that states an "
                     f"inequality and do not declare which way they print: "
                     + ", ".join(undeclared))

    loose_hits = 0
    for f in model.figs.values():
        if f.stated != "loose":
            continue
        near = [t for t in tokens
                if f.group in (t.section or "") and t.unit == f.unit
                and within(f, t, t.value)]
        if near:
            loose_hits += 1
            for t in near:
                t.claimed_by = t.claimed_by or f.key
        else:
            rep.error(f"{f.key} computes {f.value.show(f.unit)} and the section "
                      f"{f.group!r} states no such value")
    return hits, loose_hits


def pass_orphans(model: Model, tokens: list[Token], rep: Report):
    values = [(k, f.value) for k, f in model.figs.items()]
    refs = 0
    for t in tokens:
        if not t.strict or t.claimed_by:
            continue
        same = [(k, v) for k, v in values if v.d == t.value.d]
        near = [k for k, v in same
                if abs(v - t.value) <= Q.of(t.precision, t.unit)]
        if near:
            refs += 1
            continue
        rep.error(f"{_rel(t.doc)}:{t.line} carries {t.text} and no "
                  f"declared quantity has that value: a stale number, or a figure "
                  f"the model is missing", kind="orphan")

    # The same question for prose. A figure outside a block or a table has
    # nothing anchoring it, so when the model moves under it nothing notices,
    # which is how a bench current stayed at 192 mA after it became 233 mA.
    allowed = {a for a, _ in model.asides}
    named = set()
    loose_refs = 0
    for t in tokens:
        if t.text.strip() in allowed and not t.claimed_by:
            named.add(t.text.strip())
        if t.strict or t.claimed_by or t.cell or t.text.strip() in allowed:
            continue
        same = [(k, v) for k, v in values if v.d == t.value.d]
        if any(abs(v - t.value) <= Q.of(t.precision, t.unit) for _, v in same):
            loose_refs += 1
            continue
        rep.error(f"{_rel(t.doc)}:{t.line} states {t.text} in prose and no "
                  f"declared quantity has that value: a stale number, a figure "
                  f"the model is missing, or an aside the model has to name",
                  kind="prose")
    for text, why in model.asides:
        if text not in named:
            rep.error(f"the aside {text!r} ({why}) stands in none of the documents, so the "
                      f"number it names has left them", kind="aside")
    return refs, loose_refs


DATA_FIG_RE = re.compile(r'<(\w+)\b([^>]*\bdata-fig="([^"]+)"[^>]*)>([^<]*)<')


def pass_drawings(model: Model, rep: Report):
    """Check every figure a drawing anchors, and report the ones it does not.

    A number in an SVG renders whether it is current or not, which is where a
    stale value survives longest. A `data-fig` attribute on the text element
    names the quantity, so the drawing is checked the same way the document is.
    One element may carry several figures, as a capacitor's value and the bias
    it is derated at do, and the attribute then names them space separated.
    """
    anchored = unanchored = 0
    for path in model.drawings:
        if not path.exists():
            rep.error(f"{_rel(path)} is named by the model and does not exist")
            continue
        body = _norm(path.read_text(encoding="utf-8"))
        line_of = {}
        pos = 0
        for i, line in enumerate(body.split("\n"), 1):
            line_of[pos] = i
            pos += len(line) + 1
        starts = sorted(line_of)

        def line_at(offset):
            import bisect
            return line_of[starts[bisect.bisect_right(starts, offset) - 1]]

        keyed = set()
        for m in DATA_FIG_RE.finditer(body):
            text = m.group(4)
            ln = line_at(m.start())
            keyed.add(m.start())
            toks = scan_tokens(text, None, None, ln, strict=False)
            taken = []
            for key in m.group(3).split():
                if key not in model.figs:
                    rep.error(f"{path.name}:{ln} anchors {key!r}, which the model "
                              f"does not declare")
                    continue
                f = model.figs[key]
                same = sorted((t for t in toks
                               if t.unit == f.unit and id(t) not in taken),
                              key=lambda t: abs(t.value - f.value).v)
                if not same:
                    rep.error(f"{path.name}:{ln} anchors {key} and carries no free "
                              f"figure in {f.unit or 'a bare number'}: "
                              f"{text.strip()!r}")
                    continue
                t = same[0]
                taken.append(id(t))
                if not within(f, t, t.value):
                    rep.error(f"{path.name}:{ln} draws {t.text} where {key} computes "
                              f"{f.value.show(f.unit, t.decimals)}")
                else:
                    anchored += 1
        for m in re.finditer(r"<text\b[^>]*>([^<]*)</text>", body):
            if m.start() in keyed or 'data-fig="' in m.group(0):
                continue
            if TOKEN_RE.search(_norm(m.group(1))):
                unanchored += 1
                rep.note(f"unanchored  {path.name}:{line_at(m.start())} "
                         f"{m.group(1).strip()[:44]!r}")
    return anchored, unanchored


# ---------------------------------------------------------------------------
# Teensy pins
# ---------------------------------------------------------------------------
PIN_TABLE = ROOT / "docs" / "pin-assignment.md"
# Markdown marks a pin as a link to the table, titled with the signal:
#   [39](../../pin-assignment.md "Solenoid trigger 2")
PIN_LINK_RE = re.compile(r'\[(\d+)\]\(([^)\s]*pin-assignment\.md)(?:#[^)\s]*)?\s+"([^"]+)"\)')
# A fenced line names, in its comment, the signals of the numbers right of its `=`:
#   kTrigger[kCoils] = {40, 39, 38, 37};  // pin-assignment.md: Solenoid trigger 1 to 4
PIN_NOTE_RE = re.compile(r"//\s*[\w./-]*pin-assignment\.md:\s*(.+?)\s*$")
# A drawing puts them on the text element: <text data-pin="Solenoid sense 1">pin 36</text>
DATA_PIN_RE = re.compile(r'<(\w+)\b[^>]*\bdata-pin="([^"]+)"[^>]*>([^<]*)<')
PIN_NUM_RE = re.compile(r"(?<![\w.])\d+(?![\w.])")
PIN_RANGE_RE = re.compile(r"^(.*?)(\d+) to (\d+)$")
# A sentence or a drawing that calls a number the Teensy's pin in so many words, as
# "Teensy pin 31" or "the Teensy's pin 0" do, names an allocation and carries the
# mark. A comment in a drawing has no element to carry one, so it names no number.
NAMED_PIN_RE = re.compile(rf"\bTeensy(?:'s)? pins? (\d+)(?![\w.])"
                          rf"(?!\s?(?:{UNIT_ALT})(?![\wµ°]))")
# A line speaks of the Teensy's pins when it names the board, its maker, its pads or
# one of its peripherals. A pin a diff moves is looked for in such lines alone, since
# a connector's or an IC's pin 1 is no Teensy pin.
PIN_CONTEXT_RE = re.compile(r"Teensy|PJRC|core_pins|GPIO_|FlexPWM|FlexIO|QuadTimer|I²S\d|"
                            r"Serial\d|CAN\d|S/PDIF|\bWire\d?\b|\bSPI\d?\b|\bPWM\b|[Aa]nalog")
PERIPHERAL_RE = re.compile(r"FlexPWM\d\.\d|FlexIO\d|QuadTimer\d|I²S\d|Serial\d|CAN\d|"
                           r"\bWire\d?\b|\bSPI\d?\b")
LOOSE_NUM_RE = re.compile(rf"(?<![\w.])(\d{{1,2}})(?![\w.])(?!\s?(?:{UNIT_ALT})(?![\wµ°]))")
PIN_PHRASE_RE = re.compile(r"\b[Pp]ins? \d+")
# What a drawing shows, its text and its comments, without the attributes.
SVG_TEXT_RE = re.compile(r"<!--(.*?)-->|>([^<>]+)<")
# The research notes describe the board and the stock machine rather than this
# build's allocation, and the datasheets are the manufacturers' own.
UNALLOCATED = (ROOT / "docs" / "research", ROOT / "docs" / "datasheets")


@dataclasses.dataclass
class PinRef:
    doc: pathlib.Path
    line: int
    pos: int        # where the number sits in the normalised file
    number: str
    signal: str


@dataclasses.dataclass
class PinRow:
    pin: int
    signal: str         # the Signal cell up to its first comma
    peripheral: str
    subsystem: str      # the model the Subsystem cell links to, else the cell's text
    reserved: bool
    line: int
    owner: str = ""     # the Subsystem cell as it reads, without its link


def _md_tables(text: str):
    """Every table of a markdown text: the section it stands in, its header, its rows.

    A row is its line number, its cells and the line itself.
    """
    out, section, header, rows = [], "", None, []
    for i, raw in enumerate(text.split("\n") + [""], 1):
        if raw.startswith("## "):
            section = raw[3:].strip()
        if not raw.startswith("|"):
            if header is not None:
                out.append((section, header, rows))
            header, rows = None, []
            continue
        cells = [c.strip() for c in raw.strip().strip("|").split("|")]
        if header is None:
            header = cells
        elif not all(set(c) <= set("-: ") for c in cells):
            rows.append((i, cells, raw))
    return out


def _pin_rows(text: str) -> list[PinRow]:
    """Every row of the tables headed Pin, Signal: the allocation, then the reserved pins."""
    rows = []
    for section, header, body in _md_tables(text):
        if header[:2] != ["Pin", "Signal"]:
            continue
        for line, cells, _ in body:
            if len(cells) < 3 or not cells[0].isdigit():
                continue
            cell = cells[3] if len(cells) > 3 else ""
            link = re.search(r"\(parts/([\w-]+)/", cell)
            rows.append(PinRow(int(cells[0]), re.sub(r"[*`]", "", cells[1]).split(",")[0].strip(),
                               cells[2], link.group(1) if link else cell, section == "Reserved",
                               line, re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", cell).strip()))
    return rows


def pin_rows() -> list[PinRow]:
    return _pin_rows(PIN_TABLE.read_text(encoding="utf-8"))


def pin_table(rep: Report) -> dict[str, int]:
    """Signal to pin, from every table in docs/pin-assignment.md headed Pin, Signal.

    A signal is named by its Signal cell up to the first comma, so `CS-A` names
    "CS-A, converter for channels 1 to 8". A name or a pin listed twice is a
    double booking and fails the run.
    """
    table, first, holder = {}, {}, {}
    for r in pin_rows():
        if r.signal in table:
            rep.error(f"{_rel(PIN_TABLE)}:{r.line} lists {r.signal!r} again, first on line "
                      f"{first[r.signal]}", kind="pin")
            continue
        if r.pin in holder:
            rep.error(f"{_rel(PIN_TABLE)}:{r.line} gives pin {r.pin} to {r.signal!r}, and line "
                      f"{first[holder[r.pin]]} gives it to {holder[r.pin]!r}", kind="pin")
        table[r.signal], first[r.signal], holder[r.pin] = r.pin, r.line, r.signal
    return table


def _signals(text: str, table: dict[str, int]) -> list[str]:
    """The signals a note or a data-pin names, `Solenoid trigger 1 to 4` spelled out."""
    out = []
    for item in (s.strip() for s in re.split(r"[,;]", text)):
        m = PIN_RANGE_RE.match(item)
        if item not in table and m:
            out += [f"{m.group(1)}{n}" for n in range(int(m.group(2)), int(m.group(3)) + 1)]
        elif item:
            out.append(item)
    return out


def pin_refs(path: pathlib.Path, table: dict[str, int], rep: Report):
    """Every marked pin in one file, and the spans of each line it occupies.

    The spans cover the number and, in a fenced block, the note naming it, whose
    signal names carry digits too. The figure passes skip them, so a
    pin needs no aside in the model.
    """
    body = _norm(path.read_text(encoding="utf-8"))
    lines = body.split("\n")
    starts, off = [], 0
    for line in lines:
        starts.append(off)
        off += len(line) + 1

    def line_at(pos):
        import bisect
        return bisect.bisect_right(starts, pos)

    refs, spans = [], []
    if path.suffix == ".svg":
        for m in DATA_PIN_RE.finditer(body):
            names = _signals(m.group(2), table)
            nums = list(PIN_NUM_RE.finditer(m.group(3)))
            if len(nums) != len(names):
                rep.error(f"{_rel(path)}:{line_at(m.start())} names {len(names)} "
                          f"signal(s) in data-pin and shows {len(nums)} number(s)",
                          kind="pin")
                continue
            for n, name in zip(nums, names):
                pos = m.start(3) + n.start()
                refs.append(PinRef(path, line_at(pos), pos, n.group(), name))
        return refs, spans

    infence = False
    for i, raw in enumerate(lines):
        if raw.strip().startswith("```"):
            infence = not infence
            continue
        if infence:
            note = PIN_NOTE_RE.search(raw)
            if not note:
                continue
            eq = raw.find("=")
            nums = (list(PIN_NUM_RE.finditer(raw, eq + 1, note.start()))
                    if 0 <= eq < note.start() else [])
            names = _signals(note.group(1), table)
            if len(nums) != len(names):
                rep.error(f"{_rel(path)}:{i + 1} names {len(names)} signal(s) for "
                          f"{len(nums)} number(s) right of its '='", kind="pin")
                continue
            for n, name in zip(nums, names):
                refs.append(PinRef(path, i + 1, starts[i] + n.start(), n.group(), name))
                spans.append((i + 1, n.start(), n.end()))
            spans.append((i + 1, note.start(), len(raw)))
            continue
        for m in PIN_LINK_RE.finditer(raw):
            if (path.parent / m.group(2)).resolve() != PIN_TABLE.resolve():
                rep.error(f"{_rel(path)}:{i + 1} links {m.group(2)}, which does not "
                          f"lead to {_rel(PIN_TABLE)}", kind="pin")
                continue
            refs.append(PinRef(path, i + 1, starts[i] + m.start(1), m.group(1), m.group(3)))
            spans.append((i + 1, m.start(), m.end()))
    return refs, spans


def pass_pins(files, rep: Report):
    """Check every marked pin in the files against docs/pin-assignment.md.

    A Teensy pin is typed once, in that table. Everywhere else it is marked with
    the signal it carries, so a pin that moves in the table and stays behind in a
    driver, a design document or a drawing is reported with both numbers.
    """
    table = pin_table(rep)
    refs, spans = [], defaultdict(list)
    for path in files:
        found, taken = pin_refs(path, table, rep)
        refs += found
        for ln, a, b in taken:
            spans[(path.resolve(), ln)].append((a, b))
    for path in files:
        for line, number in named_pins(path):
            rep.error(f"{_rel(path)}:{line} names Teensy pin {number} without the mark; "
                      f"mark it with the signal it carries, or leave the number out",
                      kind="pin", key="named")
    good = 0
    for r in refs:
        if r.signal not in table:
            rep.error(f"{_rel(r.doc)}:{r.line} names {r.signal!r}, which "
                      f"{_rel(PIN_TABLE)} does not list", kind="pin")
        elif int(r.number) != table[r.signal]:
            rep.error(f"{_rel(r.doc)}:{r.line} gives {r.signal} pin {r.number}, and "
                      f"{_rel(PIN_TABLE)} gives it {table[r.signal]}", kind="pin")
        else:
            good += 1
    return table, refs, spans, good


def _allocating(path: pathlib.Path) -> bool:
    """Whether a file's pin numbers speak of this build's allocation."""
    p = pathlib.Path(path).resolve()
    return p != PIN_TABLE.resolve() and not any(d.resolve() in p.parents
                                                for d in UNALLOCATED)


def _unmarked_lines(path: pathlib.Path) -> list[str]:
    """The file's lines with every marked pin cut out, so what is left is unmarked.

    A drawing keeps what it shows, its text and its comments, and drops the
    attributes, whose coordinates are numbers too.
    """
    out = []
    for raw in _norm(path.read_text(encoding="utf-8")).split("\n"):
        if PIN_NOTE_RE.search(raw):
            raw = ""
        raw = PIN_LINK_RE.sub("", raw)
        if path.suffix == ".svg":
            raw = DATA_PIN_RE.sub("<", raw)
            raw = " | ".join(a or b for a, b in SVG_TEXT_RE.findall(raw))
        out.append(raw)
    return out


def named_pins(path: pathlib.Path) -> list[tuple[int, str]]:
    """Every number the file calls a Teensy pin without the mark, with its line."""
    if not _allocating(path):
        return []
    return [(i, m.group(1)) for i, line in enumerate(_unmarked_lines(path), 1)
            for m in NAMED_PIN_RE.finditer(line)]


def allocating_files() -> list[pathlib.Path]:
    return [p for d in (ROOT / "docs", ROOT / "firmware") for p in sorted(d.rglob("*"))
            if p.suffix in {".md", ".svg", ".py"} and p.is_file() and _allocating(p)]


def pass_moved_pins(base: str, rep: Report, files=None):
    """Where a pin this diff moved in the table still stands under its old number.

    A marked pin follows the table automatically. A number written without the mark
    does not, and neither does the name of a timer or a port the table no longer
    gives any pin. Both are listed for a reading rather than failed, because only
    a reader tells a stale pin from a capability the sentence describes.
    """
    try:
        old_text = subprocess.run(["git", "show", f"{base}:{_rel(PIN_TABLE)}"],
                                  cwd=ROOT, capture_output=True, text=True,
                                  encoding="utf-8", check=True).stdout
    except (subprocess.CalledProcessError, FileNotFoundError):
        return
    old, new = _pin_rows(old_text), pin_rows()
    now = {r.signal: r.pin for r in new}
    moved = {r.pin for r in old if now.get(r.signal) != r.pin}
    named = lambda rows: {m.group() for r in rows for m in PERIPHERAL_RE.finditer(r.peripheral)}
    gone = [(g, re.compile(rf"(?<!\w){re.escape(g)}(?!\.?\d)"))
            for g in sorted(named(old) - named(new))]
    if not moved and not gone:
        return
    for path in allocating_files() if files is None else files:
        if not _allocating(path):
            continue
        pin_column, heading = False, ""
        for i, line in enumerate(_unmarked_lines(path), 1):
            # Under a heading that names the Teensy, the first column of a table
            # headed Pin holds its pins. Elsewhere a line counts when it names a pin
            # and the Teensy or one of its peripherals, and then every number in it
            # does, since "take 36 and 37" names two.
            if line.startswith("#"):
                heading = line
            cells = ([c.strip() for c in line.strip().strip("|").split("|")]
                     if line.startswith("|") else None)
            if cells is None:
                pin_column = False
            elif cells[0] == "Pin" and "Teensy" in heading:
                pin_column = True
                continue
            hits = [cells[0]] if pin_column and cells and cells[0].isdigit() else []
            if NAMED_PIN_RE.search(line) or (PIN_PHRASE_RE.search(line)
                                             and PIN_CONTEXT_RE.search(line)):
                hits += [m.group(1) for m in LOOSE_NUM_RE.finditer(line)]
            if hits:
                nums = [n for n in dict.fromkeys(hits) if int(n) in moved]
                if nums:
                    rep.note(f"pin{'s' if len(nums) > 1 else ''} {', '.join(nums)} moved in "
                             f"this diff and still stand{'' if len(nums) > 1 else 's'} "
                             f"unmarked in {_rel(path)}:{i}")
            for g, rx in gone:
                if rx.search(line):
                    rep.note(f"{g} left the pin table in this diff and still stands in "
                             f"{_rel(path)}:{i}")


def write_pins(files, rep: Report) -> int:
    """Put the table's pin into every marked place that carries another one."""
    table = pin_table(rep)
    written = 0
    for path in files:
        refs, _ = pin_refs(path, table, Report())
        wrong = [r for r in refs if r.signal in table and int(r.number) != table[r.signal]]
        if not wrong:
            continue
        raw = path.read_text(encoding="utf-8")
        _, where = _norm_map(raw)
        for r in sorted(wrong, key=lambda r: -r.pos):
            a, b = where[r.pos], where[r.pos + len(r.number)]
            want = str(table[r.signal])
            if raw[a:b] != r.number:
                rep.error(f"{_rel(path)}:{r.line} could not find pin {r.number} "
                          f"where the marker puts it", kind="pin")
                continue
            raw = raw[:a] + want + raw[b:]
            written += 1
            rep.note(f"wrote       {_rel(path)}:{r.line} pin {r.number} -> {want} "
                     f"({r.signal})")
        path.write_text(raw, encoding="utf-8", newline="\n")
    return written


def _runs(pins) -> str:
    """Pins as the tables print them, a run of three or more as its two ends."""
    out, run = [], []
    for p in sorted(pins) + [None]:
        if run and (p is None or p != run[-1] + 1):
            out += [f"{run[0]}–{run[-1]}"] if len(run) > 2 else [str(x) for x in run]
            run = []
        if p is not None:
            run.append(p)
    return ", ".join(out)


def _analog_text(pins: list[int]) -> str:
    """The analog inputs as blocks of consecutive pins, A0–A13 = 14–27 in order."""
    blocks, start = [], 0
    for i in range(1, len(pins) + 1):
        if i == len(pins) or pins[i] != pins[i - 1] + 1:
            a, b = start, i - 1
            blocks.append(f"A{a} = {pins[a]}" if a == b else
                          f"A{a}–A{b} = {pins[a]}–{pins[b]} in order")
            start = i
    return ", ".join(blocks)


def _owner(name: str, word: str, rows) -> str:
    """The subsystems whose rows use a port, a timer or a FlexIO, in the order the table gives them."""
    names = dict.fromkeys(r.owner + (", reserved" if r.reserved else "")
                          for r in rows if r.peripheral.startswith(name + word))
    return " and ".join(names)


def pin_table_rows(data: PinData, rows) -> dict:
    """The rows the board's model gives the pin table, by the header of the table holding them.

    A table the model gives every row of is written whole. The capability table also
    carries rows no model computes, and the model's rows are written among them. Who
    uses a port, a timer or a FlexIO comes from the allocation, as the subsystem whose
    rows name it.
    """
    edge = set(data.groups["edge"])
    pwm = [p for p in data.pwm() if p in edge]
    ports = [[name, ", ".join(f"{sig} {' / '.join(map(str, pins))}" for sig, pins in signals)
              + (f" ({data.notes[name]})" if name in data.notes else ""), _owner(name, " ", rows)]
             for name, signals in data.ports.items()]
    timers = [[name, ", ".join(f"{p} ({ch})" for p, ch in pins), _owner(name, " channel ", rows)]
              for name, pins in data.timers.items()]
    flexio = [[name, ", ".join(f"{p} ({n})" for p, n in pins), _owner(name, " pin ", rows)]
              for name, pins in data.flexio.items()]
    return {("Resource", "Pins", "Used by"): (ports, True),
            ("Capability", "Pins", "Count"): ([["PWM", _runs(pwm), str(len(pwm))],
                                               ["Analog in", _analog_text(data.analog),
                                                str(len(data.analog))]], False),
            ("Timer", "Pins, with the channel driving each", "Used by"): (timers, True),
            ("FlexIO", "Pins, with the signal each carries", "Used by"): (flexio, True)}


def pass_pin_tables(data: PinData, rep: Report, write: bool) -> int:
    """The pin table's capability rows against the board's model, written under --write."""
    text = PIN_TABLE.read_text(encoding="utf-8")
    lines, want_all = text.split("\n"), pin_table_rows(data, _pin_rows(text))
    edits, checked = [], 0
    for _, header, body in _md_tables(text):
        if tuple(header) not in want_all:
            continue
        want, whole = want_all[tuple(header)]
        have = {cells[0]: (line, cells) for line, cells, _ in body}
        for cells in want:
            checked += 1
            got = have.get(cells[0])
            if got is None:
                rep.error(f"{_rel(PIN_TABLE)} has no {cells[0]!r} row under "
                          f"{' | '.join(header)}, which the board's model declares", kind="table")
            elif got[1] != cells:
                rep.error(f"{_rel(PIN_TABLE)}:{got[0]} reads {' | '.join(got[1][1:])!r} where the "
                          f"board's model gives {' | '.join(cells[1:])!r}", kind="table")
        if not body:
            continue
        first, last = body[0][0], body[-1][0]
        if whole:
            for name, (line, _) in have.items():
                if name not in {c[0] for c in want}:
                    rep.error(f"{_rel(PIN_TABLE)}:{line} lists {name}, which the board's model "
                              f"does not declare", kind="table")
            edits.append((first, last, ["| " + " | ".join(c) + " |" for c in want]))
        else:
            for cells in want:
                row = ["| " + " | ".join(cells) + " |"]
                line = have.get(cells[0], (None,))[0]
                # a missing row goes in after the table's last one
                edits.append((line, line, row) if line else (last + 1, last, row))
    if write and rep.count("table"):
        for first, last, rows in sorted(edits, reverse=True):
            lines[first - 1:last] = rows
        PIN_TABLE.write_text("\n".join(lines), encoding="utf-8", newline="\n")
        rep.note(f"wrote       {_rel(PIN_TABLE)}, the rows the board's model gives it")
    return checked


def _pins_text(pins) -> str:
    """Pins as a sentence names them: pin 14, pins 14 and 15, pins 1, 26 and 27."""
    p = [str(x) for x in sorted(pins)]
    return f"pin {p[0]}" if len(p) == 1 else f"pins {', '.join(p[:-1])} and {p[-1]}"


def _numbers(text: str) -> set[int]:
    return {int(n) for n in PIN_NUM_RE.findall(text)}


def _channels(text: str) -> set[int]:
    """The analog inputs a cell names, `A6 to A9` spelled out."""
    out = set()
    for a, b in re.findall(r"\bA(\d+)(?: to A(\d+))?", text):
        out |= set(range(int(a), int(b or a) + 1))
    return out


def pass_costs(data: PinData, rep: Report) -> int:
    """What the allocation and then the reserved pins take, against the two tables that list it.

    A port is taken once a pin carrying one of the signals it needs is taken, or once
    a subsystem's row names it, and a timer once a row names one of its channels. Each
    cost table names everything its part of the table takes and nothing else, every pin
    a taken port has no alternative for, and the counts of PWM pins and analog inputs.
    """
    rows = pin_rows()
    tables = {section: body for section, header, body in _md_tables(
        PIN_TABLE.read_text(encoding="utf-8")) if header[:2] == ["Lost", "To"]}
    names = sorted([*data.ports, *data.timers, *data.flexio], key=len, reverse=True)
    edge = set(data.groups["edge"])
    pwm = [p for p in data.pwm() if p in edge]
    analog = {p: i for i, p in enumerate(data.analog)}
    taken, gone, checked = set(), set(), 0
    for section, reserved in (("Allocation", False), ("Reserved", True)):
        mine = [r for r in rows if r.reserved == reserved]
        here = {r.pin for r in mine}
        taken |= here
        body = tables.get(section)
        if body is None:
            rep.error(f"{_rel(PIN_TABLE)} has no cost table under {section}", kind="cost")
            continue
        # every resource this part of the table takes, with the pins that have to be listed
        lost = {}
        for name, signals in data.ports.items():
            users = {r.pin for r in mine if r.peripheral.startswith(name + " ")}
            needed = [pins for sig, pins in signals if sig not in data.optional]
            if name in gone or not (users or any(set(pins) <= taken for pins in needed)):
                continue
            lost[name] = users | {pins[0] for pins in needed
                                  if len(pins) == 1 and pins[0] in here}
        for name in [*data.timers, *data.flexio]:
            word = " channel " if name in data.timers else " pin "
            users = {r.pin for r in mine if r.peripheral.startswith(name + word)}
            if users and name not in gone:
                lost[name] = users
        listed = set()
        for line, cells, _ in body:
            what, to = cells[0], cells[1]
            where = f"{_rel(PIN_TABLE)}:{line}"
            count = re.match(r"(\d+) of (\d+) (PWM pins|analog inputs)$", what)
            if count:
                checked += 1
                n, of, kind = int(count.group(1)), int(count.group(2)), count.group(3)
                if kind == "PWM pins":
                    want, total, got = here & set(pwm), len(pwm), _numbers(to)
                else:
                    head, _, free = to.partition("Free:")
                    want = {analog[p] for p in here if p in analog}
                    total, got = len(analog), _channels(head)
                    if free and _channels(free) != {i for p, i in analog.items() if p not in taken}:
                        rep.error(f"{where} names A{', A'.join(map(str, sorted(_channels(free))))} "
                                  f"free, and the free analog inputs are A"
                                  + ", A".join(str(i) for p, i in sorted(analog.items(), key=lambda x: x[1])
                                               if p not in taken), kind="cost")
                if (n, of) != (len(want), total) or got != want:
                    rep.error(f"{where} states {what} with {sorted(got)}, and {section.lower()} "
                              f"takes {len(want)} of {total}: {sorted(want)}", kind="cost")
                continue
            found = [n for n in names if re.search(rf"(?<![\w.]){re.escape(n)}(?![\w.])", what)]
            pins = _numbers(to.split(". ")[0])
            for n in found:
                checked += 1
                listed.add(n)
                if n not in lost:
                    rep.error(f"{where} lists {n} as lost, which {section.lower()} leaves free",
                              kind="cost")
                elif not lost[n] <= pins:
                    rep.error(f"{where} lists {n} without {_pins_text(lost[n] - pins)}",
                              kind="cost")
            if found and not pins <= here:
                rep.error(f"{where} names {_pins_text(pins - here)}, which {section.lower()} "
                          f"does not hold", kind="cost")
        for n in sorted(set(lost) - listed):
            rep.error(f"{section} takes {n}, on {_pins_text(lost[n])}, and its cost table does "
                      f"not name it", kind="cost")
        gone |= set(lost)
    return checked


def pin_files() -> list[pathlib.Path]:
    return sorted(p for d in (ROOT / "docs", ROOT / "firmware") for p in d.rglob("*")
                  if p.suffix in {".md", ".svg"} and p.is_file())


def main_pins(write: bool) -> int:
    files = pin_files()
    rep = Report()
    data = pin_data()
    if write:
        n = write_pins(files, rep)
        pass_pin_tables(data, rep, write=True)
        wrong = [e for e, (k, _) in zip(rep.errors, rep.tags) if k != "table"]
        print(f"{n} pin{'' if n == 1 else 's'} written from {_rel(PIN_TABLE)}."
              + (" Everything already agreed." if not n and not rep.errors else ""))
        for m in rep.notes + wrong:
            print("  " + m)
        return 1 if wrong else 0
    table, refs, _, good = pass_pins(files, rep)
    rows = pass_pin_tables(data, rep, write=False)
    costs = pass_costs(data, rep)
    marked = len({r.doc for r in refs})
    print("Checking Teensy pins")
    print(f"  table      {_rel(PIN_TABLE)}, {len(table)} signals")
    print(f"  files      {len(files)} under docs/ and firmware/, {marked} of them "
          f"marking a pin")
    print()
    named = sum(1 for t in rep.tags if t == ("pin", "named"))
    _status(not rep.count("pin"), "Teensy pins",
            f"{good} of {len(refs)} marked pins match the table, "
            f"{named} named without the mark")
    _status(not rep.count("table"), "what the pins can carry",
            f"{rows - rep.count('table')} of {rows} rows match the board's model")
    _status(not rep.count("cost"), "what the allocation costs",
            f"{costs} entries match what the pins take" if not rep.count("cost") else
            f"{rep.count('cost')} entr{'y' if rep.count('cost') == 1 else 'ies'} disagree"
            f"{'s' if rep.count('cost') == 1 else ''} with what the pins take")
    if rep.errors:
        print()
        print("To fix")
        for e in rep.errors:
            print("  " + e)
        print()
        n = len(rep.errors)
        print(f"{n} problem{'' if n == 1 else 's'}. Nothing was changed: move the pin "
              f"in the table, or run --write to carry the table's pin into the files and "
              f"the board's model into the table.")
        return 1
    print()
    print("Nothing to fix. Every marked pin is the one the table gives its signal.")
    return 0


# ---------------------------------------------------------------------------
# Wiring and peripherals, checked across every model
# ---------------------------------------------------------------------------
def pin_data() -> PinData:
    """What the pins can carry, from the one model that declares it."""
    found = [m for m in (load_model(p) for p in find_models()) if m.pin_data]
    if len(found) != 1:
        raise ValueError(f"{len(found)} models declare what the pins can carry, and one has to")
    return found[0].pin_data


def _function(peripheral: str, pin: int, data: PinData):
    """The port or timer a Peripheral entry names, its signal or channel, and whether the pin carries it."""
    for t, pins in data.timers.items():
        if peripheral.startswith(t + " channel "):
            ch = peripheral[len(t) + len(" channel "):]
            return t, ch, dict(pins).get(pin) == ch
    for f, pins in data.flexio.items():
        if peripheral.startswith(f + " pin "):
            n = peripheral[len(f) + len(" pin "):]
            return f, n, dict(pins).get(pin) == n
    for port in sorted(data.ports, key=len, reverse=True):
        if peripheral.startswith(port + " "):
            signal = peripheral[len(port) + 1:]
            return port, signal, pin in dict(data.ports[port]).get(signal, ())
    return None, None, peripheral in ("plain digital input", "plain digital output")


def teensy_part(row: PinRow, data: PinData) -> Part:
    """The Teensy pin itself, driving its net the way the function its row gives it does."""
    used, signal, _ = _function(row.peripheral, row.pin, data)
    kind = (data.kind(signal) if used in data.ports else
            "input" if row.peripheral == "plain digital input" else "push-pull")
    return Part(f"Teensy pin {row.pin}", kind, TEENSY_RAIL if kind == "push-pull" else None,
                src=f"{_rel(PIN_TABLE)}:{row.line}")


def pass_wiring(models, rep: Report) -> int:
    """One driver at a time on every net, a defined level, and the Teensy's rail on its pins."""
    rows, data = {r.signal: r for r in pin_rows()}, pin_data()
    merged = defaultdict(list)
    for m in models:
        for n in m.nets:
            merged[("teensy", n.teensy) if n.teensy else ("local", m.name, n.name)].append(n)
    for key, group in merged.items():
        parts = [p for n in group for p in n.parts]
        label = f"{group[0].name} in {', '.join(sorted({n.model for n in group}))}"
        if key[0] == "teensy":
            row = rows.get(key[1])
            if row is None:
                rep.error(f"{label} reaches the Teensy signal {key[1]!r}, which "
                          f"{_rel(PIN_TABLE)} does not list", kind="pins")
                continue
            label += f", pin {row.pin}"
            for n in group:
                if n.model != row.subsystem:
                    rep.error(f"{label}: {n.model} wires to a pin {_rel(PIN_TABLE)}:{row.line} "
                              f"gives to {row.subsystem}", kind="pins")
            parts = [teensy_part(row, data)] + parts
            for p in parts:
                powered = (p.role in ("push-pull", "tri-state")
                           or (p.role in ("pull", "contact") and p.rail != "gnd"))
                if powered and p.rail != TEENSY_RAIL:
                    rep.error(f"{label}: {p.name} runs from {p.rail}, and only the Teensy's own "
                              f"3.3 V may reach a Teensy pin", kind="rail")
        always = [p for p in parts if p.role == "push-pull"]
        others = [p for p in parts if p.role in ("tri-state", "open-drain", "contact")]
        if len(always) > 1 or (always and others):
            rest = ", ".join(p.name for p in always[1:] + others)
            rep.error(f"{label}: {always[0].name} drives the line whenever it is powered, "
                      f"and {rest} drives it as well", kind="contention")
        selects = [p.select for p in parts if p.role == "tri-state"]
        if len(set(selects)) < len(selects):
            rep.error(f"{label}: two tri-state outputs share one select, so both answer at once",
                      kind="contention")
        if not always and not any(p.role == "pull" for p in parts):
            rep.error(f"{label}: nothing holds the line while no output drives it, so it needs "
                      f"a pull-up or a pull-down", kind="level")
    wired = set(merged)
    for r in rows.values():
        if (r.subsystem in {m.name for m in models} and not r.reserved
                and ("teensy", r.signal) not in wired):
            rep.error(f"{_rel(PIN_TABLE)}:{r.line} gives pin {r.pin} ({r.signal}) to "
                      f"{r.subsystem}, whose model wires no net to it", kind="pins")
    return len(merged)


def pass_peripherals(models, rep: Report) -> int:
    """Every pin function is one the board's model gives that pin, and an owned peripheral has one user."""
    data = pin_data()
    owner = {}
    for m in models:
        for p, why in m.owned:
            if p in owner and owner[p][0] != m.name:
                rep.error(f"{p} is owned by {owner[p][0]} and by {m.name}", kind="owner")
            owner.setdefault(p, (m.name, why))
    for r in pin_rows():
        where = f"{_rel(PIN_TABLE)}:{r.line}"
        used, what, ok = _function(r.peripheral, r.pin, data)
        if used is None and not ok:
            rep.error(f"{where} names {r.peripheral!r}, which is no port signal, no timer "
                      f"channel and no plain digital pin", kind="function")
            continue
        if not ok:
            rep.error(f"{where} puts {r.peripheral} on pin {r.pin}, which "
                      f"{'that channel' if used in data.timers else 'that signal'} of {used} "
                      f"does not reach", kind="function")
        if used in owner and owner[used][0] != r.subsystem:
            rep.error(f"{where} gives {used} on pin {r.pin} to {r.subsystem}, and "
                      f"{owner[used][0]} owns it: {owner[used][1]}", kind="owner")
    return len(owner)


# how the report names a pool that carries a wiring rail's name
POOL_NAMES = {TEENSY_RAIL: "the Teensy's 3V3 pin"}


def pass_budget(models, rep: Report) -> list:
    """Every pool against the one figure that supplies it, with the draws of every model added up."""
    supply, demand = {}, defaultdict(list)
    for m in models:
        for key, pool, call in m.supplied:
            if pool in supply:
                rep.error(f"{POOL_NAMES.get(pool, pool)} is supplied by {supply[pool][0].name} "
                          f"and by {m.name}", kind="budget")
                continue
            supply[pool] = (m, key, call)
        for key, pool in m.drawn:
            demand[pool].append((m, key))
    # a pool whose slots a listing takes by a call: each model draws as many as its documents call
    unlisted = set()
    for pool, (_, _, call) in supply.items():
        if call is None:
            continue
        for m in models:
            listed = sum(_fenced(d).count(call) for d in (m.document, *m.documents) if d.exists())
            drawn = sum(1 for _, p in m.drawn if p == pool)
            if listed != drawn:
                unlisted.add(pool)
                rep.error(f"{m.name}'s documents call {call}...) {listed} "
                          f"time{'' if listed == 1 else 's'}, and the model draws {drawn} "
                          f"slot{'' if drawn == 1 else 's'} of the {pool}", kind="budget")
    out = []
    for pool, users in demand.items():
        name = POOL_NAMES.get(pool, pool)
        if pool not in supply:
            rep.error(f"{', '.join(sorted({m.name for m, _ in users}))} draw from {name}, which "
                      f"no model supplies", kind="budget")
            continue
        holder, key, _ = supply[pool]
        held = holder.figs[key]
        total = Q.of(0, held.unit)
        for m, k in users:
            total = total + (m.figs[k].value if k else Q.of(1, ""))
        if not total <= held.value:
            rep.error(f"the subsystems take {_amount(total, held.unit)} together from {name} in "
                      f"the machine, which holds {_amount(held.value, held.unit)}" if held.unit
                      else f"{_amount(total, '')} drivers take a slot of the {name}, which holds "
                      f"{_amount(held.value, '')}", kind="budget")
        out.append((name, total, held, pool not in unlisted))
    return out


def _fenced(path: pathlib.Path) -> str:
    """The fenced blocks of a markdown file, the listings."""
    return "\n".join(re.findall(r"^```[^\n]*\n(.*?)^```", path.read_text(encoding="utf-8"),
                                re.S | re.M))


def _amount(q: Q, unit: str) -> str:
    return f"{q.to(unit):.4g} {unit}".strip()


def main_wiring(paths) -> int:
    models = [load_model(p) for p in paths]
    for m in models:
        m.evaluate()
    rep = Report()
    nets = pass_wiring(models, rep)
    owned = pass_peripherals(models, rep)
    budget = pass_budget(models, rep)
    print("Checking the wiring and what the subsystems share")
    print(f"  models     {len(models)}, {nets} nets, {owned} owned peripherals and "
          f"{len(budget)} shared budgets")
    print()
    _status(not rep.count("contention"), "one driver at a time",
            f"{rep.count('contention')} nets driven by two outputs at once")
    _status(not rep.count("level"), "a defined level",
            f"{rep.count('level')} nets left floating while nothing drives them")
    _status(not rep.count("rail"), "the Teensy's rail on its pins",
            f"{rep.count('rail')} parts on a Teensy pin running from another rail")
    _status(not rep.count("pins"), "the pins the table gives",
            f"{rep.count('pins')} nets or allocated pins that disagree with the table")
    _status(not rep.count("function"), "pin functions",
            f"{rep.count('function')} functions a pin cannot carry")
    _status(not rep.count("owner"), "owned peripherals",
            f"{rep.count('owner')} used by a subsystem that does not own them")
    for name, total, held, listed in budget:
        _status(total <= held.value and listed, name,
                f"{_amount(total, held.unit)} of {_amount(held.value, held.unit)} in the machine"
                if held.unit else f"{_amount(total, '')} of {_amount(held.value, '')} slots taken")
    if rep.errors:
        print()
        print("To fix")
        for e in rep.errors:
            print("  " + e)
        return 1
    return 0


def pass_direction(model: Model, rep: Report):
    """Perturb each declared input and check the figure moves the way it claims."""
    checked = flat = 0
    for f in model.figs.values():
        for key, want in [(k, +1) for k in f.rises_with] + \
                         [(k, -1) for k in f.falls_with]:
            if key not in model.figs:
                rep.error(f"{f.key} claims a direction against {key!r}, which is "
                          f"not declared")
                continue
            base = model.figs[key].value
            moved = None
            for factor in (1.10, 1.50, 3.00):
                try:
                    got = model.recompute({key: base * factor})[f.key]
                except Exception as e:
                    rep.error(f"{f.key} against {key}: recomputing raised "
                              f"{type(e).__name__}: {e}")
                    moved = "raised"
                    break
                delta = (got - f.value).v
                if delta != 0:
                    moved = delta
                    break
            if moved == "raised":
                continue
            if moved is None:
                flat += 1
                rep.error(f"{f.key} claims to move with {key} and does not move at "
                          f"all, even at three times its value")
                continue
            sign = 1 if moved > 0 else -1
            if sign != want:
                rep.error(f"{f.key} claims to {'rise' if want > 0 else 'fall'} with "
                          f"{key} and {'rises' if sign > 0 else 'falls'} instead")
            else:
                checked += 1
    return checked


def pass_invariants(model: Model, rep: Report):
    """Relations that hold whatever the formulas are."""
    values = _Namespace(model.figs)
    held = 0
    for description, fn in model.invariants:
        try:
            ok = fn(values)
        except Exception as e:
            rep.error(f"a requirement could not be evaluated, {description}: "
                      f"{type(e).__name__}: {e}")
            continue
        if ok:
            held += 1
        else:
            rep.error(f"a requirement of the design does not hold: {description}")
    return held


class _Namespace:
    """Figure values by attribute, so an invariant reads like the design does."""

    def __init__(self, figs):
        self._figs = figs

    def __getattr__(self, key):
        if key not in self._figs:
            raise AttributeError(f"no quantity {key!r}")
        return self._figs[key].value


def pass_curves(model: Model, rep: Report):
    """Check each plotted curve for shape, and each reading against its curve."""
    def val(key):
        f = model.figs.get(key)
        if f is None:
            rep.error(f"no quantity {key!r}")
            return None
        return f.value

    points_seen = checked = 0
    for name, points, rises, on, bounded in model.curves:
        xs, ys = [], []
        for xk, yk in points:
            x, y = val(xk), val(yk)
            if x is None or y is None:
                break
            xs.append((xk, x))
            ys.append((yk, y))
        if len(xs) != len(points):
            continue
        points_seen += len(points)
        for i in range(1, len(xs)):
            if not xs[i - 1][1] < xs[i][1]:
                rep.error(f"{name}: {xs[i][0]} is not past "
                          f"{xs[i - 1][0]} along the axis")
            up = ys[i - 1][1] < ys[i][1]
            if up != rises:
                rep.error(f"{name}: {ys[i - 1][0]} to {ys[i][0]} moves "
                          f"the wrong way for a curve declared "
                          f"{'rising' if rises else 'falling'}")
        for xk, yk in on:
            x, y = val(xk), val(yk)
            if x is None or y is None:
                continue
            lo = hi = None
            for i in range(1, len(xs)):
                if xs[i - 1][1] <= x <= xs[i][1]:
                    lo, hi = sorted((ys[i - 1][1], ys[i][1]))
                    break
            if lo is None:
                rep.error(f"{name}: {yk} sits at {x.show(model.figs[xk].unit)}, "
                          f"which no pair of read points brackets")
                continue
            if not lo <= y <= hi:
                u = model.figs[yk].unit
                rep.error(f"{name}: {yk} is {y.show(u)} at "
                          f"{x.show(model.figs[xk].unit)}, outside the "
                          f"{lo.show(u)} to {hi.show(u)} its neighbours read")
                continue
            checked += 1
        for yk, lok, hik in bounded:
            y, lo, hi = val(yk), val(lok), val(hik)
            if None in (y, lo, hi):
                continue
            if not lo <= y <= hi:
                u = model.figs[yk].unit
                rep.error(f"{name}: {yk} reads {y.show(u)}, outside the "
                          f"{lo.show(u)} to {hi.show(u)} the sheet's table gives")
                continue
            checked += 1
    return points_seen, checked


def pass_datasheets(model: Model, rep: Report):
    """Look every datasheet reading up in the sheet it cites.

    A reading that is found is not proof, because the merged glyph tables of a
    subset font can put a digit where none was. A reading that is not found is
    worth re-reading by hand, and a sheet whose text mostly fails to come out
    is reported as unreadable rather than blaming each of its readings.
    """
    import pdftext

    by_sheet = defaultdict(list)
    # a taken reading is looked up once, by the model that declares it
    own = [f for f in model.figs.values() if f.origin is None]
    off_plot = [f for f in own if f.kind == "graph"]
    for f in own:
        if f.sheet and f.kind == "datasheet":
            by_sheet[f.sheet].append(f)
    looked = missing = 0
    for sheet, figs in sorted(by_sheet.items()):
        path = DATASHEETS / sheet
        if not path.exists():
            rep.error(f"{sheet} is cited and not present")
            continue
        try:
            text = pdftext.extract(path)
        except Exception as e:
            rep.note(f"unreadable  {sheet}: {type(e).__name__}: {e}")
            continue
        quality = pdftext.readable(text)
        # Two ways a sheet comes back unusable. Fonts this reader cannot map
        # give noise, which the plausible-character share catches. A scan of
        # printed pages gives clean text that is not the sheet: a handful of
        # navigation labels repeated, which scores full marks on plausibility
        # and carries almost no vocabulary.
        words = len(set(text.split()))
        if quality < 0.6 or words < SHEET_WORDS:
            why = (f"this reader gets {quality:.0%} plausible text out of it"
                   if quality < 0.6 else
                   f"only {words} distinct words come out of it, so it is a scan "
                   f"whose text layer holds no sheet content")
            rep.note(f"unreadable  {sheet}: {why}, so its {len(figs)} readings "
                     f"stay unchecked and want reading by eye")
            continue
        flat = pdftext.squeeze(text)
        absent = []
        for f in figs:
            printed = f"{f.value.to(f.unit):g}"
            looked += 1
            if printed not in flat and printed.rstrip("0").rstrip(".") not in flat:
                absent.append(f)
        for f in absent:
            missing += 1
            rep.error(f"{f.key:<30} {f.value.show(f.unit):>10} is not in "
                      f"{sheet}: {f.src}")
    if off_plot:
        on_curve = {k for _n, pts, _r, _o, _b in model.curves
                    for pair in list(pts) + list(_o) for k in pair}
        loose = sorted(f.key for f in off_plot if f.key not in on_curve)
        if loose:
            rep.note(f"{len(loose)} of {len(off_plot)} curve readings sit on no "
                     f"declared curve, so nothing checks them: "
                     + ", ".join(loose))
        else:
            rep.note(f"All {len(off_plot)} curve readings are checked against their "
                     f"own curve. A sheet never prints such a value, so that is "
                     f"consistency rather than proof.")
    return looked, missing


def _renumber(line: str, col: int, old_num: str, new_num: str):
    """Put a new figure where the old one sat, keeping the column if it can.

    The appendix blocks are aligned by hand, so a figure that grows or shrinks
    takes the space back from the run of blanks in front of it. Where there is
    no such run the line moves, and the caller is told.
    """
    if line[col:col + len(old_num)] != old_num:
        return None, False
    head, tail = line[:col], line[col + len(old_num):]
    delta = len(new_num) - len(old_num)
    if delta > 0:
        blanks = len(head) - len(head.rstrip(" "))
        if blanks >= delta:
            return head[:len(head) - delta] + new_num + tail, True
        return head + new_num + tail, False
    return head + " " * -delta + new_num + tail, True


def _norm_map(raw: str) -> tuple[str, list[int]]:
    """The normalised text, and for each of its characters where it sits in raw.

    Only an entity changes length when it is normalised, so a position found in
    the normalised text is carried back to the file through this map.
    """
    out, where, i = [], [], 0
    while i < len(raw):
        for entity in ("&#937;", "&#181;"):
            if raw.startswith(entity, i):
                out.append(_norm(entity))
                where.append(i)
                i += len(entity)
                break
        else:
            out.append(_norm(raw[i]))
            where.append(i)
            i += 1
    where.append(len(raw))
    return "".join(out), where


def pass_write(model: Model, tokens: list[Token], rep: Report) -> int:
    """Write every figure the model computes into the line that states it.

    A figure is found by its value, so one that moved far no longer finds its
    own line, and another figure of its block may then take that line for its
    own new value. A block in which any figure could not be placed is therefore
    written nowhere: its lines are reported, and once the far movers stand in
    by hand a second run writes the rest.
    """
    placed = {t.claimed_by for t in tokens if t.claimed_by}
    unsure = defaultdict(list)
    for f in model.figs.values():
        if f.stated is True and f.key not in placed:
            unsure[(f.group, f.section)].append(f.key)

    edits, shifted, held = defaultdict(list), 0, set()
    for t in tokens:
        if not t.claimed_by:
            continue
        f = model.figs[t.claimed_by]
        if f.stated is not True:
            continue
        want = print_as(f, t.decimals)
        have = t.text.split(" ")[0] if " " in t.text else t.text
        if want == have:
            continue
        if (f.group, f.section) in unsure:
            held.add((f.group, f.section))
            continue
        edits[t.doc].append((t, have, want))
    for group, section in sorted(held, key=lambda g: (g[0] or "", g[1] or "")):
        rep.note(f"not written {group!r}: "
                 + ", ".join(unsure[(group, section)])
                 + " found no line, so a line found for another figure of that block "
                   "may be theirs, and none is written")

    for path, items in edits.items():
        lines = path.read_text(encoding="utf-8").split("\n")
        for t, have, want in sorted(items, key=lambda i: (-i[0].line, -i[0].col)):
            fixed, kept = _renumber(lines[t.line - 1], t.col, have, want)
            if fixed is None:
                rep.error(f"{path.name}:{t.line} could not find {have!r} "
                          f"at column {t.col}")
                continue
            lines[t.line - 1] = fixed
            shifted += not kept
            rep.note(f"wrote       {path.name}:{t.line} {have} -> {want} "
                     f"({t.claimed_by})" + ("" if kept else ", column moved"))
        path.write_text("\n".join(lines), encoding="utf-8", newline="\n")

    # A drawing names each figure's key, so its place is known whatever the
    # value did. The element is matched in the file as it stands and the number
    # replaced at its own offset: the same text may stand in several elements,
    # and an entity ahead of a number moves it in the normalised text.
    drawn = 0
    for path in model.drawings:
        body = path.read_text(encoding="utf-8")
        cuts = []
        for m in DATA_FIG_RE.finditer(body):
            text, where = _norm_map(m.group(4))
            toks = scan_tokens(text, None, None, 0, False)
            taken = []
            for key in m.group(3).split():
                if key not in model.figs:
                    continue
                f = model.figs[key]
                same = sorted((t for t in toks
                               if t.unit == f.unit and id(t) not in taken),
                              key=lambda t: abs(t.value - f.value).v)
                if not same:
                    continue
                t = same[0]
                taken.append(id(t))
                have = t.text.split(" ")[0] if " " in t.text else t.text
                want = print_as(f, t.decimals)
                if want == have:
                    continue
                start = m.start(4) + where[t.col]
                end = m.start(4) + where[t.col + len(have)]
                if body[start:end] != have:
                    rep.error(f"{path.name} anchors {key} and could not find "
                              f"{have!r} in {m.group(4).strip()!r}")
                    continue
                cuts.append((start, end, want))
                rep.note(f"wrote       {path.name} {have} -> {want} ({key})")
        for start, end, want in sorted(cuts, reverse=True):
            body = body[:start] + want + body[end:]
        if cuts:
            path.write_text(body, encoding="utf-8", newline="\n")
            drawn += len(cuts)

    total = sum(len(v) for v in edits.values()) + drawn
    if shifted:
        rep.note(f"{shifted} of {total} figures grew past the blanks in front of them, "
                 f"so their column moved")
    return total


def pass_sums(model: Model, rep: Report, write: bool):
    named = sorted({f.sheet for f in model.figs.values() if f.sheet and f.origin is None})
    if write:
        lines = []
        for name in sorted(p.name for p in DATASHEETS.glob("*") if p.is_file()
                           and p.name != SUMS.name):
            h = hashlib.sha256((DATASHEETS / name).read_bytes()).hexdigest()
            lines.append(f"{h}  {name}")
        SUMS.write_text("\n".join(lines) + "\n", encoding="utf-8")
        rep.note(f"wrote {SUMS.relative_to(ROOT).as_posix()}, {len(lines)} files")
        return
    if not SUMS.exists():
        rep.error(f"{SUMS.relative_to(ROOT).as_posix()} missing; "
                  f"run with --write-sums once")
        return
    recorded = {}
    for line in SUMS.read_text(encoding="utf-8").split("\n"):
        if line.strip():
            h, name = line.split(None, 1)
            recorded[name.strip()] = h
    for name in named:
        p = DATASHEETS / name
        if not p.exists():
            rep.error(f"{name} named by the model is not in docs/datasheets/")
            continue
        if name not in recorded:
            rep.error(f"{name} has no recorded checksum")
            continue
        got = hashlib.sha256(p.read_bytes()).hexdigest()
        if got != recorded[name]:
            rep.error(f"{name} does not match its recorded checksum")
    return len(named)


def pass_stale(base: str, rep: Report):
    try:
        diff = subprocess.run(["git", "diff", "-U0", base, "--", "docs", "firmware"],
                              cwd=ROOT, capture_output=True, text=True,
                              encoding="utf-8", check=True).stdout
    except (subprocess.CalledProcessError, FileNotFoundError) as e:
        rep.note(f"stale pass skipped: git diff {base} failed ({e})")
        return
    removed, added = set(), set()
    for line in _norm(diff).split("\n"):
        if line[:3] in ("---", "+++"):
            continue
        toks = {(m.group(1), m.group(2)) for m in TOKEN_RE.finditer(line)}
        if line.startswith("-"):
            removed |= toks
        elif line.startswith("+"):
            added |= toks
    gone = sorted(removed - added)
    files = {p: _norm(p.read_text(encoding="utf-8", errors="replace"))
             for d in (ROOT / "docs", ROOT / "firmware") for p in d.rglob("*")
             if p.suffix in {".md", ".svg", ".py"} and p.is_file()}
    for num, unit in gone:
        tok = f"{num} {unit}"
        where = sorted(p.relative_to(ROOT).as_posix() for p, t in files.items() if tok in t)
        if where:
            rep.note(f"{tok} left the model in this diff and still stands in "
                     + ", ".join(_rel(w) for w in where))


def pass_mutate(model: Model, tokens: list[Token], rep: Report):
    """Move every figure the document states, and require the run to notice.

    Perturbing a single token proves little, because a figure whose value the
    document prints twice simply lands on the other one. So every token a
    figure could land on moves together, past the tolerance, and the run has
    to report that figure. A prose figure is looked up by value inside its
    section and cannot be gated this way; those are counted instead.
    """
    stated = [f for f in model.figs.values() if f.stated is True]
    loose = sum(1 for t in tokens
                if t.claimed_by and model.figs[t.claimed_by].stated == "loose")
    saved = [(t, t.claimed_by) for t in tokens]
    survived = []
    for f in stated:
        for t, _ in saved:
            t.claimed_by = None
        moved = {id(t): t.value + tolerance(f, t) * 2
                 for _, _, t in _candidates(f, tokens, None)}
        for t, _ in saved:
            t.claimed_by = None
        probe = Report()
        pass_anchored(model, tokens, probe, override=moved)
        if not probe.has("unplaced", f.key):
            survived.append((f, moved))
    for t, claim in saved:
        t.claimed_by = claim
    for f, moved in survived:
        rep.error(f"{f.key:<34} computes {f.value.show(f.unit)}; moving "
                  f"all {len(moved)} token(s) it could land on raises nothing")
    if loose:
        rep.note(f"{loose} numbers are stated in prose and found by value inside "
                 f"their section, which cannot tell two equal values apart. The "
                 f"rest are pinned to the line that states them.")
    return len(stated) - len(survived), len(stated)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
_LOADED: dict = {}


def load_model(path: pathlib.Path) -> Model:
    """The model a file declares, loaded once, so a figure another model takes is one object."""
    path = pathlib.Path(path).resolve()
    if path in _LOADED:
        return _LOADED[path]
    # Run as a script this module is __main__; the model imports it by name, and
    # a second import would give it a second Q class that fails every isinstance.
    sys.modules.setdefault("figcheck", sys.modules[__name__])
    name = f"figmodel_{len(_LOADED)}"
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    mod.MODEL.path = path
    _LOADED[path] = mod.MODEL
    return mod.MODEL


def model_named(name: str) -> Model:
    for path in find_models():
        model = load_model(path)
        if model.name == name:
            return model
    raise KeyError(f"no model is named {name!r}")


def _rel(path) -> str:
    """A path as it reads from the repository root."""
    try:
        return pathlib.Path(path).resolve().relative_to(ROOT).as_posix()
    except ValueError:
        return str(path)


def _status(ok: bool, label: str, detail: str):
    print(f"  {'ok ' if ok else 'FAIL'}  {label:<30} {detail}")


def show_graph(paths, key: str) -> int:
    """One quantity in full, from whichever of the models declares it."""
    found, near = False, []
    for path in paths:
        model = load_model(path)
        model.evaluate()
        if key not in model.figs:
            near += [f"{k} ({model.name})" for k in model.figs if key in k]
            continue
        if found:
            print()
        found = True
        f = model.figs[key]
        print(f"{f.key} = {f.value.show(f.unit)}   [{f.kind}]   in {model.name}"
              + (f", taken from {f.origin}" if f.origin else ""))
        if f.src:
            print(f"  source     {f.src}")
        if f.deps:
            print(f"  from       {', '.join(f.deps)}")
        if f.body:
            for i, l in enumerate(f.body.split("\n")):
                print(f"  {'formula' if i == 0 else '':<10} {l}")
        for d in f.deps:
            g = model.figs.get(d)
            if g is not None:
                print(f"    {d:<26} {g.value.show(g.unit):>14}  [{g.kind}]")
        anc = model.ancestors(key)
        if anc:
            print(f"  rests on   {len(anc)}: {', '.join(anc)}")
        dep = model.dependents(key)
        print(f"  feeds      {len(dep)}: {', '.join(dep) if dep else 'nothing'}")
    if not found:
        print(f"unknown key {key!r}")
        if near:
            print("did you mean: " + ", ".join(sorted(near)[:12]))
        return 2
    return 0


def run_model(path: pathlib.Path, args, whole: bool) -> int:
    """One model: its figures, its documents and drawings, and the pins they mark.

    Inside a run over the whole tree the pins are reported once, in the tree's
    own block, and the stale pass runs once at its end.
    """
    model = load_model(path)
    model.evaluate()

    if args.blind:
        print(f"# Deriving {model.name} a second time, independently")
        print()
        print("Every quantity below is to be derived from the schematic and the")
        print("datasheets alone, without reading the model. The inputs are given")
        print("because they are readings rather than results, and no computed value")
        print("appears at all. The two derivations are diffed afterwards, which only")
        print("means something if the second one was written blind.")
        print()
        print("## Given")
        print()
        given = [x for x in model.figs.values() if x.kind != "derived" or x.origin]
        for f in sorted(given, key=lambda x: (x.kind, x.key)):
            print(f"- `{f.key}` = {f.value.show(f.unit)}  [{f.kind}"
                  + (f", from {f.origin}" if f.origin else "") + f"] {f.src or ''}")
        print()
        print("## To derive")
        print()
        for f in sorted((x for x in model.figs.values() if x.kind == "derived" and not x.origin),
                        key=lambda x: x.key):
            print(f"- `{f.key}` in {f.unit or 'a bare ratio'}, from "
                  f"{', '.join(f.deps)}")
        return 0

    if args.provenance:
        by_kind = defaultdict(list)
        taken = sorted((f for f in model.figs.values() if f.origin), key=lambda x: x.key)
        for f in model.figs.values():
            if f.kind != "derived" and not f.origin:
                by_kind[f.kind].append(f)
        headings = {"datasheet": "read from a datasheet table",
                    "graph": "read off a plotted curve",
                    "measured": "measured on the bench",
                    "assumed": "assumed, with the reason given",
                    "decision": "chosen"}
        print(f"Where every input of {model.name} came from")
        print()
        for kind in ("datasheet", "graph", "measured", "assumed", "decision"):
            group = by_kind.get(kind, [])
            print(f"{len(group)} {headings[kind]}")
            for f in sorted(group, key=lambda x: x.key):
                print(f"  {f.key:<30} {f.value.show(f.unit):>14}   {f.src or ''}"
                      + (f"  [{f.sheet}]" if f.sheet else ""))
            print()
        if model.pin_data:
            print(f"{len(model.pin_data.sources)} declarations of what the pins can carry")
            for what, src in model.pin_data.sources:
                print(f"  {what:<30} {src}")
            print()
        print(f"{len(taken)} taken from another model, which declares the source")
        for f in taken:
            print(f"  {f.key:<30} {f.value.show(f.unit):>14}   from {f.origin}, [{f.kind}]")
        print()
        derived = [f for f in model.figs.values() if f.kind == "derived" and not f.origin]
        print(f"{len(derived)} figures are computed from those and carry no source.")
        return 0

    tokens = parse_document(model.document, model.section, model.until)
    for extra in model.documents:
        tokens += parse_document(extra, None, NEVER)

    if args.groups:
        cur = object()
        declared = defaultdict(list)
        for k, f in model.figs.items():
            if f.stated is True:
                declared[f.group].append(k)
        for t in tokens:
            if not (t.strict or t.cell):
                continue
            if (t.group, t.section) != cur:
                cur = (t.group, t.section)
                print()
                print(f"[{t.group}]  in {t.section!r}")
                print(f"    declared: {', '.join(declared.get(t.group, [])) or '-'}")
            print(f"  L{t.line:<5} {t.text:<12} {t.context.strip()[:86]}")
        return 0

    rep = Report()
    taken = sum(1 for f in model.figs.values() if f.origin)
    inputs = sum(1 for f in model.figs.values() if f.kind != "derived" and not f.origin)
    print(f"Checking {model.name}")
    print(f"  document   {_rel(model.document)}")
    for extra in model.documents:
        print(f"             {_rel(extra)}")
    if model.drawings:
        print(f"  drawings   " + ", ".join(d.name for d in model.drawings))
    print(f"  model      {inputs} declared inputs, "
          f"{len(model.figs) - inputs - taken} derived figures"
          + (f", {taken} taken from other models" if taken else ""))
    print()

    # A marked pin is checked against the pin table, so the figure passes leave
    # its number alone. Under --write a wrong pin is not an error but a job.
    own = [p for p in (model.document, *model.documents, *model.drawings) if p.exists()]
    _, pins, pin_spans, pins_good = pass_pins(own, Report() if args.write or whole
                                              else rep)
    tokens = [t for t in tokens
              if not any(a <= t.col < b
                         for a, b in pin_spans.get((t.doc.resolve(), t.line), ()))]

    hits, loose = pass_anchored(model, tokens, rep)

    if args.write:
        written = pass_write(model, tokens, rep)
        pins_written = 0 if whole else write_pins(own, rep)
        print(f"{written} figure{'' if written == 1 else 's'} written into the "
              f"document and the drawings"
              + (f", {pins_written} pin{'' if pins_written == 1 else 's'} from "
                 f"{_rel(PIN_TABLE)}" if pins_written else "")
              + "."
              + (" Everything already agreed."
                 if not written and not pins_written and not rep.errors else ""))
        for n in rep.notes:
            print("  " + n)
        for e in rep.errors:
            print("  " + e)
        return 1 if rep.errors else 0

    anchorable = sum(1 for f in model.figs.values() if f.stated is True)
    loosely = sum(1 for f in model.figs.values() if f.stated == "loose")
    strict = sum(1 for t in tokens if t.strict)
    _status(hits == anchorable, "figures in the document",
            f"{hits} of {anchorable} match the line that states them")
    _status(loose == loosely, "figures named in prose",
            f"{loose} of {loosely} appear in the section that mentions them")

    refs, loose_refs = pass_orphans(model, tokens, rep)
    orphans = rep.count("orphan")
    _status(orphans == 0, "every number accounted for",
            f"{strict} in blocks and tables, {orphans} from nowhere")
    stray, dead = rep.count("prose"), rep.count("aside")
    _status(stray == 0 and dead == 0, "every number in prose too",
            f"{loose_refs} carry a quantity, {len(model.asides) - dead} are named asides, "
            f"{stray} match nothing" + (f", {dead} asides no document states" if dead else ""))

    if model.drawings:
        keyed, loose_svg = pass_drawings(model, rep)
        _status(loose_svg == 0, "figures in the drawings",
                f"{keyed} checked across {len(model.drawings)} files, "
                f"{loose_svg} left with no anchor")

    if not whole:
        named = sum(1 for t in rep.tags if t == ("pin", "named"))
        _status(rep.count("pin") == 0, "Teensy pins",
                f"{pins_good} of {len(pins)} marked pins match {_rel(PIN_TABLE)}, "
                f"{named} named without the mark")

    if any(f.rises_with or f.falls_with for f in model.figs.values()):
        directed = pass_direction(model, rep)
        claimed = sum(len(f.rises_with) + len(f.falls_with)
                      for f in model.figs.values())
        _status(directed == claimed, "which way a figure moves",
                f"{directed} of {claimed} inputs move the result the way it claims")

    if model.curves:
        pts, checked = pass_curves(model, rep)
        _status(True, "readings off a plotted curve",
                f"{len(model.curves)} curves, {pts} points, {checked} readings "
                f"bracketed")

    if model.invariants:
        held = pass_invariants(model, rep)
        _status(held == len(model.invariants), "what the design requires",
                f"{held} of {len(model.invariants)} requirements hold")

    if model.unlinted:
        rep.note(f"{len(model.unlinted)} formulas are built by exec and escape the "
                 f"no-bare-number lint: {', '.join(sorted(model.unlinted)[:6])}"
                 + (" and more" if len(model.unlinted) > 6 else ""))

    if args.sheets:
        looked, missing = pass_datasheets(model, rep)
        _status(missing == 0, "readings found in their sheet",
                f"{looked - missing} of {looked} located in the PDF they cite")

    sheets = pass_sums(model, rep, args.write_sums)
    if not args.write_sums:
        _status(True, "datasheet files",
                f"{sheets} named by the model, every checksum matches")

    if args.mutate:
        caught, total = pass_mutate(model, tokens, rep)
        _status(caught == total, "the check would catch a slip",
                f"{caught} of {total} figures are reported when the document "
                f"moves them")

    if not args.no_stale and not whole:
        pass_stale(args.base, rep)
        pass_moved_pins(args.base, rep, [*own, path])

    if rep.notes:
        print()
        print("Worth knowing")
        for n in rep.notes:
            print("  " + n)

    if rep.errors:
        print()
        print("To fix")
        for e in rep.errors:
            print("  " + e)
        print()
        n = len(rep.errors)
        print(f"{n} problem{'' if n == 1 else 's'}. Nothing was changed: correct the "
              f"model where the design moved, or the document where it did not, "
              f"then run again.")
        return 1

    print()
    print("Nothing to fix. Every figure in the documents is the one the model "
          "computes.")
    return 0


def find_models() -> list[pathlib.Path]:
    """The Teensy's model and the firmware's first, since the subsystems take figures from them."""
    return (sorted((ROOT / "docs" / "research").glob("*.py"))
            + sorted((ROOT / "docs" / "firmware").glob("figures.py"))
            + sorted((ROOT / "docs" / "parts").glob("*/figures.py")))


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="check the documents against their recomputed figures, and every "
                    "marked Teensy pin against docs/pin-assignment.md")
    ap.add_argument("models", type=pathlib.Path, nargs="*",
                    help="the models to run; every docs/parts/*/figures.py, and the "
                         "pins of the whole tree, when none is named")
    ap.add_argument("--base", default="HEAD", help="git ref the stale pass diffs against")
    ap.add_argument("--mutate", action="store_true", help="run the mutation self-test")
    ap.add_argument("--provenance", action="store_true", help="list the inputs and their sources")
    ap.add_argument("--graph", metavar="KEY", help="what a quantity feeds, and what feeds it")
    ap.add_argument("--write-sums", action="store_true", help="record the datasheet checksums")
    ap.add_argument("--no-stale", action="store_true")
    ap.add_argument("--write", action="store_true",
                    help="write the model's figures into the document and the "
                         "drawings, so a figure is typed in one place only")
    ap.add_argument("--sheets", action="store_true",
                    help="look every datasheet reading up in the sheet it cites")
    ap.add_argument("--groups", action="store_true",
                    help="dump the groups and tokens the parser found")
    ap.add_argument("--blind", action="store_true",
                    help="the brief for an independent re-derivation: every quantity, "
                         "its unit and its inputs, with no formula and no value")
    args = ap.parse_args(argv)
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")

    whole = not args.models
    paths = args.models or find_models()
    if not paths:
        print("No figure model found. Expected docs/parts/<subsystem>/figures.py.")
        return 1
    if args.graph:
        return show_graph(paths, args.graph)

    failed = []
    for i, path in enumerate(paths):
        if i:
            print()
        if run_model(path, args, whole):
            failed.append(path.parent.name)
    if not whole or args.blind or args.provenance or args.groups:
        return 1 if failed else 0

    print()
    if main_pins(args.write):
        failed.append("the pins")
    print()
    if main_wiring(paths):
        failed.append("the wiring")
    if not args.no_stale and not args.write:
        rep = Report()
        pass_stale(args.base, rep)
        pass_moved_pins(args.base, rep)
        if rep.notes:
            print()
            print("Worth knowing")
            for n in rep.notes:
                print("  " + n)
    print()
    if failed:
        print("Something to fix in " + ", ".join(failed) + ".")
        return 1
    print(f"Nothing to fix in {len(paths)} models, the pin table and the wiring.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
