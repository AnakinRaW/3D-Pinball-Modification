"""The checks run on one model: its figures in the documents and the drawings, its derivations,
its sources, the numbers a diff left behind, and the mutation self-test."""
from __future__ import annotations

import dataclasses
import hashlib
import math
import re
import subprocess
from collections import defaultdict

from .document import INEQUALITY_RE, TOKEN_RE, Token, norm, scan_tokens
from .model import KINDS, Fig, Model
from .quantities import Q
from .repo import ROOT, model_named, rel
from .report import Report


# Distinct words below which an extracted sheet is a scan rather than a sheet.
# The lowest real datasheet in docs/datasheets yields 83, an image scan yields 8.
SHEET_WORDS = 40

DATASHEETS = ROOT / "docs" / "datasheets"
SUMS = DATASHEETS / "SHA256SUMS"


def tolerance(f: Fig, tok: Token | None) -> Q:
    grid = tok.precision if tok else 0.001
    # an input is printed as it is typed, so its digits agree exactly
    if f.kind in KINDS[:-1] and f.tol_units == 1.0:
        return Q.of(0, f.unit)
    return Q.of(f.tol_units * grid, f.unit)


def within(f: Fig, tok: Token | None, v: Q) -> bool:
    """Whether a printed value may stand for this figure, on its declared side for a bound."""
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


def printed(f: Fig, decimals: int) -> str:
    """The figure with its unit, as the document has to print it."""
    return f"{print_as(f, decimals)} {f.unit}".strip()


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


def pass_anchored(model: Model, tokens: list[Token], rep: Report, override=None, base=None):
    """Address each stated figure by the value it computes, inside its group.

    --write passes `base`, each figure's value at the base revision, so a figure that
    moved far also finds the line that still reads its old value. A line that a figure
    printing another value could take as well is not given away by distance.
    """
    pending = [f for f in model.figs.values() if f.stated is True]
    old = {f.key: dataclasses.replace(f, value=base[f.key]) for f in pending
           if base and base.get(f.key) is not None and base[f.key].d == f.value.d}

    def homes(f):
        """The free tokens the figure can stand on, by its value first and then by its old one."""
        found = _candidates(f, tokens, override)
        if f.key in old:
            seen = {id(c[2]) for c in found}
            found += [c for c in _candidates(old[f.key], tokens, override)
                      if id(c[2]) not in seen]
        return found

    def contested(t, f):
        """Whether another figure that could take the token prints it differently."""
        return any(print_as(g, t.decimals) != print_as(f, t.decimals)
                   for g in pending if any(c[2] is t for c in homes(g)))

    hits = 0
    while pending:
        progress = False
        # a figure with exactly one home takes it first, which constrains the rest
        for only_certain in (True, False):
            for f in list(pending):
                cands = homes(f)
                if not cands or (only_certain and len(cands) > 1):
                    continue
                if base is not None and not only_certain and contested(cands[0][2], f):
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
        if base is not None and homes(f):
            rep.error(f"{f.key} could stand on lines "
                      f"{', '.join(str(n) for n in sorted({c[1] for c in homes(f)}))} of the "
                      f"{f.group!r} block, as could a figure that prints another value there. "
                      f"A label in the model tells them apart.",
                      kind="unplaced", key=f.key)
            continue
        carried = [t for t in tokens
                   if t.group == f.group and t.unit == f.unit
                   and (f.section is None or f.section in (t.section or ""))]
        free = [t for t in carried if t.claimed_by is None] or carried
        if len(free) == 1:
            t = free[0]
            rep.error(f"{rel(t.doc)}:{t.line} reads {t.text} where "
                      f"{f.key} computes {printed(f, t.decimals)}",
                      kind="unplaced", key=f.key)
        else:
            where = ", ".join(f"{t.text} on line {t.line}" for t in free[:5]) or "nothing"
            rep.error(f"{f.key} computes {f.value.show(f.unit)} and the "
                      f"{f.group!r} block has no line for it. It holds {where}.",
                      kind="unplaced", key=f.key)

    if override is None:
        claimed = {t.claimed_by: t for t in tokens if t.claimed_by}
        # only a computed figure has a rounding side to declare
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


def past_bound(model: Model, t: Token) -> list[Fig]:
    """The bounds a prose figure stands for, when it sits past every one of them.

    A ceiling is printed down and a floor up. A prose figure one digit past the bound
    passes the two-sided match and still claims more margin than the design has.
    """
    near = [f for f in model.figs.values() if f.value.d == t.value.d
            and abs(f.value - t.value) <= Q.of(t.precision, t.unit)]

    def past(f):
        d = (t.value - f.value).v
        eps = 1e-9 * max(abs(f.value.v), 1e-30)
        return (f.prints == "down" and d > eps) or (f.prints == "up" and d < -eps)
    return near if near and all(past(f) for f in near) else []


def pass_orphans(model: Model, tokens: list[Token], rep: Report):
    values = [f.value for f in model.figs.values()]

    def declared(t):
        return any(v.d == t.value.d and abs(v - t.value) <= Q.of(t.precision, t.unit)
                   for v in values)

    refs = 0
    for t in tokens:
        if not t.strict or t.claimed_by:
            continue
        if declared(t):
            refs += 1
            continue
        rep.error(f"{rel(t.doc)}:{t.line} carries {t.text} and no "
                  f"declared quantity has that value: a stale number, or a figure "
                  f"the model is missing", kind="orphan")

    # the same question for prose, where nothing else anchors a figure
    allowed = {a for a, _ in model.asides}
    named = set()
    loose_refs = 0
    for t in tokens:
        if t.text.strip() in allowed and not t.claimed_by:
            named.add(t.text.strip())
        if t.strict or t.claimed_by or t.cell or t.text.strip() in allowed:
            continue
        if declared(t):
            loose_refs += 1
            for f in past_bound(model, t)[:1]:
                rep.error(f"{rel(t.doc)}:{t.line} states {t.text} in prose, where {f.key} "
                          f"computes {f.value.show(t.unit)} and is printed {f.prints}: "
                          f"the prose overstates the bound", kind="side")
            continue
        rep.error(f"{rel(t.doc)}:{t.line} states {t.text} in prose and no "
                  f"declared quantity has that value: a stale number, a figure "
                  f"the model is missing, or an aside the model has to name",
                  kind="prose")
    for text, why in model.asides:
        if text not in named:
            rep.error(f"the aside {text!r} ({why}) stands in none of the documents, so the "
                      f"number it names has left them", kind="aside")
    return refs, loose_refs


DATA_FIG_RE = re.compile(r'<(\w+)\b([^>]*\bdata-fig="([^"]+)"[^>]*)>([^<]*)<')


def anchor_fig(model: "Model", key: str):
    """The figure a drawing anchors: a key of the drawing's own model, or `model:key` of any
    other. A drawing that carries the parts of several subsystems then needs no `uses`."""
    if ":" not in key:
        return model.figs.get(key)
    name, k = key.split(":", 1)
    try:
        other = model_named(name)
    except KeyError:
        return None
    return other.figure(k) if k in other.figs else None


def element_figures(model: Model, text: str, keys: str):
    """Each key a drawing's element names, its figure, and the nearest free token of its unit
    in the element's text, None for either one the element does not give."""
    toks, taken = scan_tokens(text, None, None, 0, strict=False), []
    for key in keys.split():
        f = anchor_fig(model, key)
        same = [] if f is None else sorted(
            (t for t in toks if t.unit == f.unit and id(t) not in taken),
            key=lambda t: abs(t.value - f.value).v)
        taken += [id(t) for t in same[:1]]
        yield key, f, (same[0] if same else None)


def pass_drawings(model: Model, rep: Report):
    """Check every figure a drawing anchors, and report the ones it does not."""
    anchored = unanchored = 0
    for path in model.drawings:
        if not path.exists():
            rep.error(f"{rel(path)} is named by the model and does not exist")
            continue
        body = norm(path.read_text(encoding="utf-8"))
        keyed = set()
        for m in DATA_FIG_RE.finditer(body):
            ln = body.count("\n", 0, m.start()) + 1
            keyed.add(m.start())
            for key, f, t in element_figures(model, m.group(4), m.group(3)):
                if f is None:
                    rep.error(f"{path.name}:{ln} anchors {key!r}, which "
                              + (f"{key.split(':')[0]} does not declare" if ":" in key
                                 else "the model does not declare"))
                elif t is None:
                    rep.error(f"{path.name}:{ln} anchors {key} and carries no free "
                              f"figure in {f.unit or 'a bare number'}: "
                              f"{m.group(4).strip()!r}")
                elif not within(f, t, t.value):
                    rep.error(f"{path.name}:{ln} draws {t.text} where {key} computes "
                              f"{printed(f, t.decimals)}")
                else:
                    anchored += 1
        for m in re.finditer(r"<text\b[^>]*>([^<]*)</text>", body):
            if m.start() in keyed or 'data-fig="' in m.group(0):
                continue
            if TOKEN_RE.search(norm(m.group(1))):
                unanchored += 1
                ln = body.count("\n", 0, m.start()) + 1
                rep.error(f"{path.name}:{ln} shows {m.group(1).strip()[:44]!r}, and no "
                          f"data-fig names its figure")
    return anchored, unanchored


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
            rep.error(f"no quantity {key!r}", kind="curve")
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
                          f"{xs[i - 1][0]} along the axis", kind="curve")
            up = ys[i - 1][1] < ys[i][1]
            if up != rises:
                rep.error(f"{name}: {ys[i - 1][0]} to {ys[i][0]} moves "
                          f"the wrong way for a curve declared "
                          f"{'rising' if rises else 'falling'}", kind="curve")
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
                          f"which no pair of read points brackets", kind="curve")
                continue
            if not lo <= y <= hi:
                u = model.figs[yk].unit
                rep.error(f"{name}: {yk} is {y.show(u)} at "
                          f"{x.show(model.figs[xk].unit)}, outside the "
                          f"{lo.show(u)} to {hi.show(u)} its neighbors read", kind="curve")
                continue
            checked += 1
        for yk, lok, hik in bounded:
            y, lo, hi = val(yk), val(lok), val(hik)
            if None in (y, lo, hi):
                continue
            if not lo <= y <= hi:
                u = model.figs[yk].unit
                rep.error(f"{name}: {yk} reads {y.show(u)}, outside the "
                          f"{lo.show(u)} to {hi.show(u)} the sheet's table gives", kind="curve")
                continue
            checked += 1
    return points_seen, checked


def pass_datasheets(model: Model, rep: Report):
    """Look every datasheet reading up in the sheet it cites, or report the sheet unreadable."""
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
        # unmapped fonts give noise; a scan gives a few labels and almost no vocabulary
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
        return len(named)
    if not SUMS.exists():
        rep.error(f"{SUMS.relative_to(ROOT).as_posix()} missing; "
                  f"run with --write-sums once", kind="sums")
        return len(named)
    recorded = {}
    for line in SUMS.read_text(encoding="utf-8").split("\n"):
        if line.strip():
            h, name = line.split(None, 1)
            recorded[name.strip()] = h
    for name in named:
        p = DATASHEETS / name
        if not p.exists():
            rep.error(f"{name} named by the model is not in docs/datasheets/", kind="sums")
            continue
        if name not in recorded:
            rep.error(f"{name} has no recorded checksum", kind="sums")
            continue
        got = hashlib.sha256(p.read_bytes()).hexdigest()
        if got != recorded[name]:
            rep.error(f"{name} does not match its recorded checksum", kind="sums")
    return len(named)


def pass_stale(base: str, rep: Report):
    try:
        diff = subprocess.run(["git", "diff", "-U0", base, "--", "docs"],
                              cwd=ROOT, capture_output=True, text=True,
                              encoding="utf-8", check=True).stdout
    except (subprocess.CalledProcessError, FileNotFoundError) as e:
        rep.note(f"stale pass skipped: git diff {base} failed ({e})")
        return
    removed, added = set(), set()
    for line in norm(diff).split("\n"):
        if line[:3] in ("---", "+++"):
            continue
        toks = {(m.group(1), m.group(2)) for m in TOKEN_RE.finditer(line)}
        if line.startswith("-"):
            removed |= toks
        elif line.startswith("+"):
            added |= toks
    gone = sorted(removed - added)
    files = {p: norm(p.read_text(encoding="utf-8", errors="replace"))
             for p in (ROOT / "docs").rglob("*")
             if p.suffix in {".md", ".svg", ".py"} and p.is_file()}
    for num, unit in gone:
        tok = f"{num} {unit}"
        where = sorted(p.relative_to(ROOT).as_posix() for p, t in files.items() if tok in t)
        if where:
            rep.note(f"{tok} left the model in this diff and still stands in "
                     + ", ".join(rel(w) for w in where))


def pass_mutate(model: Model, tokens: list[Token], rep: Report):
    """Move every token each stated figure could land on, and require the run to notice."""
    stated = [f for f in model.figs.values() if f.stated is True]
    loose = sum(1 for t in tokens
                if t.claimed_by and model.figs[t.claimed_by].stated == "loose")
    saved = [(t, t.claimed_by) for t in tokens]
    survived = []
    for f in stated:
        for t, _ in saved:
            t.claimed_by = None
        moved = {id(t): t.value + (tolerance(f, t) * 2 if tolerance(f, t).v
                                   else Q.of(t.precision, f.unit))
                 for _, _, t in _candidates(f, tokens, None)}
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
