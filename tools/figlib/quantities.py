"""A quantity carries its dimension, so adding a current to a time raises instead of computing."""
from __future__ import annotations

import functools
import math
import operator


# a dimension is the exponents of (volt, ampere, second, kelvin, meter)
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
_family(_S, [("minutes", 60.0), ("s", 1.0), ("ms", 1e-3), ("µs", 1e-6), ("ns", 1e-9)])
_family(_OHM, [("Ω", 1.0), ("mΩ", 1e-3), ("kΩ", 1e3), ("MΩ", 1e6)])
_family(_WATT, [("W", 1.0), ("mW", 1e-3), ("µW", 1e-6)])
# a source's radiant intensity, power per steradian; the steradian is a plain number
_family(_WATT, [("mW/sr", 1e-3)])
# an irradiance, the power falling on an area
_family((1, 1, 0, 0, -2), [("W/m²", 1.0), ("mW/cm²", 10.0)])
_family(_FARAD, [("F", 1.0), ("µF", 1e-6), ("nF", 1e-9), ("pF", 1e-12)])
_family(_HENRY, [("H", 1.0), ("mH", 1e-3), ("µH", 1e-6)])
_family(_HZ, [("Hz", 1.0), ("kHz", 1e3), ("MHz", 1e6)])
_family(_COUL, [("C", 1.0), ("µC", 1e-6), ("nC", 1e-9)])
_family(_K, [("°C", 1.0)])
_family(_M, [("m", 1.0), ("cm", 1e-2), ("mm", 1e-3), ("nm", 1e-9)])
_family(_MPS, [("m/s", 1.0), ("mm/s", 1e-3)])
# an input's longest edge, as a logic datasheet gives it: time per volt of swing
_family((-1, 0, 1, 0, 0), [("ns/V", 1e-9)])
# a wire's resistance per length, as a gauge table gives it
_family((1, -1, 0, 0, -1), [("Ω/m", 1.0), ("mΩ/m", 1e-3)])
# a cable's capacitance per length, as a cable datasheet gives it
_family((-1, 1, 1, 0, -1), [("pF/m", 1e-12)])
# a cable's loop inductance per length
_family((1, -1, 1, 0, -1), [("µH/m", 1e-6)])
# a package's thermal resistance, junction to ambient
_family((-1, -1, 0, 1, 0), [("°C/W", 1.0)])
# counts are dimensionless, so a model keeps a byte and a sample apart by name
_family(DIMLESS, [("%", 0.01), ("×", 1.0), ("steps", 1.0), ("step", 1.0),
                  ("τ_adc", 1.0), ("τ", 1.0), ("clocks", 1.0), ("clock", 1.0),
                  ("dB", 1.0), ("bits", 1.0), ("bytes", 1.0), ("KB", 1024.0), ("MB", 1048576.0),
                  ("samples", 1.0), ("sectors", 1.0), ("sector", 1.0), ("", 1.0)])

DIM_NAMES = {DIMLESS: "1", _V: "V", _A: "A", _S: "s", _K: "°C", _OHM: "Ω",
             _WATT: "W", _FARAD: "F", _HENRY: "H", _HZ: "Hz", _COUL: "C",
             _M: "m", _MPS: "m/s"}


def _dimstr(d):
    return DIM_NAMES.get(d) or "·".join(
        f"{n}^{e}" for n, e in zip("V A s K m".split(), d) if e)


class Q:
    """A quantity: a magnitude in base units plus a dimension."""

    __slots__ = ("v", "d")

    def __init__(self, v: float, d=DIMLESS):
        self.v, self.d = float(v), tuple(d)

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

    def _same(self, o, op):
        if self.d != o.d:
            raise TypeError(f"cannot {op} {_dimstr(self.d)} and {_dimstr(o.d)}")

    def __add__(self, o):
        o = as_q(o)
        self._same(o, "add")
        return Q(self.v + o.v, self.d)

    def __sub__(self, o):
        o = as_q(o)
        self._same(o, "subtract")
        return Q(self.v - o.v, self.d)

    __radd__ = __add__

    def __rsub__(self, o):
        return as_q(o) - self

    def __mul__(self, o):
        o = as_q(o)
        return Q(self.v * o.v, tuple(a + b for a, b in zip(self.d, o.d)))

    __rmul__ = __mul__

    def __truediv__(self, o):
        o = as_q(o)
        return Q(self.v / o.v, tuple(a - b for a, b in zip(self.d, o.d)))

    def __rtruediv__(self, o):
        return as_q(o) / self

    def __pow__(self, n: int):
        return Q(self.v ** n, tuple(e * n for e in self.d))

    def __neg__(self):
        return Q(-self.v, self.d)

    def __abs__(self):
        return Q(abs(self.v), self.d)

    def _cmp(self, o, op):
        o = as_q(o)
        self._same(o, "compare")
        return op(self.v, o.v)

    __lt__ = functools.partialmethod(_cmp, op=operator.lt)
    __le__ = functools.partialmethod(_cmp, op=operator.le)
    __gt__ = functools.partialmethod(_cmp, op=operator.gt)
    __ge__ = functools.partialmethod(_cmp, op=operator.ge)

    @property
    def raw(self) -> float:
        if self.d != DIMLESS:
            raise TypeError(f"{_dimstr(self.d)} is not a plain number")
        return self.v


def as_q(x) -> Q:
    return x if isinstance(x, Q) else Q(x)


def db(ratio):
    """A voltage ratio in decibels."""
    return Q.of(20 * math.log10(as_q(ratio).raw), "dB")


def from_db(gain):
    """The voltage ratio a gain in decibels stands for, the inverse of db()."""
    return Q(10 ** (as_q(gain).raw / 20))


def ln(x):
    return Q(math.log(as_q(x).raw))


def log10(x):
    return Q(math.log10(as_q(x).raw))


def exp(x):
    return Q(math.exp(as_q(x).raw))


def sqrt(x):
    q = as_q(x)
    if any(e % 2 for e in q.d):
        raise TypeError(f"cannot take the root of {_dimstr(q.d)}")
    return Q(math.sqrt(q.v), tuple(e // 2 for e in q.d))


def ceil_to(x, unit: str, step: float):
    """Round a quantity up to the next multiple of `step` display units."""
    return Q.of(math.ceil(as_q(x).to(unit) / step) * step, unit)


def floor_to(x, unit: str, step: float):
    """Round a quantity down to the next multiple of `step` display units."""
    return Q.of(math.floor(as_q(x).to(unit) / step) * step, unit)


def interp_log(points, x):
    """Log-linear interpolation through published curve points."""
    pts = sorted(points, key=lambda p: as_q(p[0]).v)
    xs = as_q(x)
    if xs <= as_q(pts[0][0]):
        (x0, y0), (x1, y1) = pts[0], pts[1]
    elif xs >= as_q(pts[-1][0]):
        (x0, y0), (x1, y1) = pts[-2], pts[-1]
    else:
        for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
            if as_q(x0) <= xs <= as_q(x1):
                break
    f = math.log(xs.v / as_q(x0).v) / math.log(as_q(x1).v / as_q(x0).v)
    return as_q(y0) + (as_q(y1) - as_q(y0)) * f
