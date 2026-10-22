"""The command line: a block for each model, then the pins and the wiring of the whole tree."""
from __future__ import annotations

import argparse
import dataclasses
import pathlib
import sys

from .checks import (pass_anchored, pass_curves, pass_datasheets, pass_direction,
                     pass_drawings, pass_invariants, pass_mutate, pass_orphans, pass_stale,
                     pass_sums, write_sums)
from .document import NEVER, parse_document
from .listings import print_blind, print_groups, print_provenance, show_graph
from .model import Model
from .pins import PIN_TABLE, pass_moved_pins, pass_pins, run_pins, write_pins
from .repo import find_models, load_model, model_file, rel, values_at
from .report import Report, listing, status
from .wiring import run_wiring
from .write import pass_write


@dataclasses.dataclass
class Run:
    """One model's run: the tokens its checks read, the report they fill, what anchoring found."""
    model: Model
    path: pathlib.Path          # the model's file, as the command line names it
    args: argparse.Namespace
    whole: bool                 # a run over the whole tree, whose own block reports the pins
    tokens: list
    rep: Report = dataclasses.field(default_factory=Report)
    own: list = dataclasses.field(default_factory=list)     # the model's files that exist
    pins: list = dataclasses.field(default_factory=list)    # the pins those files mark
    pins_good: int = 0
    hits: int = 0               # stated figures found on their line
    loose: int = 0              # prose figures found in their section


def run_model(path: pathlib.Path, args, whole: bool) -> int:
    """One model: its figures, its documents and drawings, and the pins they mark."""
    model = load_model(path)
    model.evaluate()
    if args.blind:
        return print_blind(model)
    if args.provenance:
        return print_provenance(model)
    tokens = parse_document(model.document, model.section, model.until)
    for extra in model.documents:
        tokens += parse_document(extra, None, NEVER)
    if args.groups:
        return print_groups(model, tokens)

    run = Run(model, path, args, whole, tokens)
    _header(model)
    _anchor(run)
    if args.write:
        return _write(run)
    for check in CHECKS:
        check(run)
    return _verdict(run.rep)


def _header(model: Model):
    """The model's name, the files it governs, and how many figures it declares."""
    taken = sum(1 for f in model.figs.values() if f.origin)
    inputs = sum(1 for f in model.figs.values() if f.kind != "derived" and not f.origin)
    print(f"Checking {model.name}")
    print(f"  document   {rel(model.document)}")
    for extra in model.documents:
        print(f"             {rel(extra)}")
    if model.drawings:
        print(f"  drawings   " + ", ".join(d.name for d in model.drawings))
    print(f"  model      {inputs} declared inputs, "
          f"{len(model.figs) - inputs - taken} derived figures"
          + (f", {taken} taken from other models" if taken else ""))
    print()


def _anchor(run: Run):
    """Check the marked pins, take their numbers out of the tokens, and find every stated figure."""
    model = run.model
    # A marked pin is checked against the pin table, so the figure passes leave
    # its number alone. Under --write a wrong pin is not an error but a job.
    run.own = [p for p in (model.document, *model.documents, *model.drawings) if p.exists()]
    _, run.pins, spans, run.pins_good = pass_pins(
        run.own, Report() if run.args.write or run.whole else run.rep)
    run.tokens = [t for t in run.tokens
                  if not any(a <= t.col < b
                             for a, b in spans.get((t.doc.resolve(), t.line), ()))]
    # --write also finds a figure on the line that still reads its value at --base
    base = values_at(run.args.base).get(model.name, {}) if run.args.write else None
    run.hits, run.loose = pass_anchored(model, run.tokens, run.rep, base=base)


def _write(run: Run) -> int:
    """--write: the figures into the files, then every edit and everything left unplaced."""
    rep = run.rep
    written = pass_write(run.model, run.tokens, rep)
    write_sums(run.model, rep)
    if rep.count("unplaced") and run.model.name not in values_at(run.args.base):
        rep.note(f"{run.args.base} holds no {run.model.name} model this figcheck can "
                 f"evaluate, so each figure was looked for by its new value alone")
    pins_written = 0 if run.whole else write_pins(run.own, rep)
    print(f"{written} figure{'' if written == 1 else 's'} written into the "
          f"document and the drawings"
          + (f", {pins_written} pin{'' if pins_written == 1 else 's'} from "
             f"{rel(PIN_TABLE)}" if pins_written else "")
          + "."
          + (" Everything already agreed."
             if not written and not pins_written and not rep.errors else ""))
    for line in rep.notes + rep.errors:
        print("  " + line)
    return 1 if rep.errors else 0


# Each check runs its pass and prints its own status line, in the order the report shows them.
def _figures(run: Run):
    """Every stated figure on the line that states it, and every prose figure in its section."""
    figs = run.model.figs.values()
    anchorable = sum(1 for f in figs if f.stated is True)
    loosely = sum(1 for f in figs if f.stated == "loose")
    status(run.hits == anchorable, "figures in the document",
           f"{run.hits} of {anchorable} match the line that states them")
    status(run.loose == loosely, "figures named in prose",
           f"{run.loose} of {loosely} appear in the section that mentions them")


def _numbers(run: Run):
    """Every number in a block, a table or the prose traces back to a declared quantity."""
    model, rep = run.model, run.rep
    strict = sum(1 for t in run.tokens if t.strict)
    _, loose_refs = pass_orphans(model, run.tokens, rep)
    orphans = rep.count("orphan")
    status(orphans == 0, "every number accounted for",
           f"{strict} in blocks and tables, {orphans} from nowhere")
    stray, dead, side = rep.count("prose"), rep.count("aside"), rep.count("side")
    status(stray == 0 and dead == 0 and side == 0, "every number in prose too",
           f"{loose_refs} carry a quantity, {len(model.asides) - dead} are named asides, "
           f"{stray} match nothing" + (f", {dead} asides no document states" if dead else "")
           + (f", {side} past the bound they quote" if side else ""))


def _drawings(run: Run):
    """Every figure a drawing anchors agrees with the model."""
    model = run.model
    if model.drawings:
        keyed, loose = pass_drawings(model, run.rep)
        status(loose == 0, "figures in the drawings",
               f"{keyed} checked across {len(model.drawings)} files, "
               f"{loose} left with no anchor")


def _pins(run: Run):
    """The pins the model's files mark, unless the tree's own block reports them."""
    if not run.whole:
        named = sum(1 for t in run.rep.tags if t == ("pin", "named"))
        status(run.rep.count("pin") == 0, "Teensy pins",
               f"{run.pins_good} of {len(run.pins)} marked pins match {rel(PIN_TABLE)}, "
               f"{named} named without the mark")


def _direction(run: Run):
    """Each figure moves with its inputs the way it declares."""
    figs = run.model.figs.values()
    if any(f.rises_with or f.falls_with for f in figs):
        directed = pass_direction(run.model, run.rep)
        claimed = sum(len(f.rises_with) + len(f.falls_with) for f in figs)
        status(directed == claimed, "which way a figure moves",
               f"{directed} of {claimed} inputs move the result the way it claims")


def _curves(run: Run):
    """Each plotted curve keeps its shape, and each reading sits on its curve."""
    model = run.model
    if model.curves:
        pts, checked = pass_curves(model, run.rep)
        status(not run.rep.count("curve"), "readings off a plotted curve",
               f"{len(model.curves)} curves, {pts} points, {checked} readings "
               f"bracketed")


def _invariants(run: Run):
    """The relations the design requires hold."""
    model = run.model
    if model.invariants:
        held = pass_invariants(model, run.rep)
        status(held == len(model.invariants), "what the design requires",
               f"{held} of {len(model.invariants)} requirements hold")


def _unlinted(run: Run):
    """The formulas built by exec, which the lint against bare numbers cannot read."""
    unlinted = run.model.unlinted
    if unlinted:
        run.rep.note(f"{len(unlinted)} formulas are built by exec and escape the "
                     f"no-bare-number lint: {', '.join(sorted(unlinted)[:6])}"
                     + (" and more" if len(unlinted) > 6 else ""))


def _sheets(run: Run):
    """--sheets: each datasheet reading is found in the sheet it cites."""
    if run.args.sheets:
        looked, missing = pass_datasheets(run.model, run.rep)
        status(missing == 0, "readings found in their sheet",
               f"{looked - missing} of {looked} located in the PDF they cite")


def _sums(run: Run):
    """The cited datasheets match their checksums, which --write records for a new sheet."""
    sheets = pass_sums(run.model, run.rep)
    status(not run.rep.count("sums"), "datasheet files",
           f"{sheets} named by the model, "
           + ("not every checksum matches" if run.rep.count("sums") else
              "every checksum matches"))


def _mutate(run: Run):
    """--mutate: every figure the document states is moved, and the check has to notice."""
    if run.args.mutate:
        caught, total = pass_mutate(run.model, run.tokens, run.rep)
        status(caught == total, "the check would catch a slip",
               f"{caught} of {total} figures are reported when the document "
               f"moves them")


def _stale(run: Run):
    """The numbers and the pins a diff moved, unless the tree's own block looks for them.
    --mutate has no use for them."""
    if not run.args.mutate and not run.whole:
        pass_stale(run.args.base, run.rep)
        pass_moved_pins(run.args.base, run.rep, [*run.own, run.path])


CHECKS = (_figures, _numbers, _drawings, _pins, _direction, _curves, _invariants, _unlinted,
          _sheets, _sums, _mutate, _stale)


def _verdict(rep: Report) -> int:
    """What the checks found: first what is worth knowing, then what has to be fixed."""
    if rep.notes:
        listing("Worth knowing", rep.notes)
    if rep.errors:
        listing("To fix", rep.errors)
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


def _parser() -> argparse.ArgumentParser:
    ap = argparse.ArgumentParser(
        description="check the documents against their recomputed figures, and every "
                    "marked Teensy pin against docs/pin-assignment.md")
    ap.add_argument("models", type=pathlib.Path, nargs="*",
                    help="the models to run, as paths from the repository root; every "
                         "docs/parts/*/figures.py, and the pins of the whole tree, when "
                         "none is named")
    ap.add_argument("--base", default="HEAD",
                    help="git ref the stale pass diffs against and --write takes the old "
                         "figures from")
    ap.add_argument("--mutate", action="store_true", help="run the mutation self-test")
    ap.add_argument("--provenance", action="store_true", help="list the inputs and their sources")
    ap.add_argument("--graph", metavar="KEY", help="what a quantity feeds, and what feeds it")
    ap.add_argument("--write", action="store_true",
                    help="write the model's figures into the document and the "
                         "drawings, so a figure is typed in one place only, and record "
                         "the checksum of each cited datasheet that has none yet")
    ap.add_argument("--sheets", action="store_true",
                    help="look every datasheet reading up in the sheet it cites")
    ap.add_argument("--groups", action="store_true",
                    help="dump the groups and tokens the parser found")
    ap.add_argument("--blind", action="store_true",
                    help="the brief for an independent re-derivation: every quantity, "
                         "its unit and its inputs, with no formula and no value")
    return ap


def main(argv=None):
    ap = _parser()
    args = ap.parse_args(argv)
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")

    whole = not args.models
    try:
        paths = [model_file(m) for m in args.models] or find_models()
    except ValueError as e:
        ap.error(str(e))
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
    if run_pins(args.write):
        failed.append("the pins")
    print()
    if run_wiring(paths):
        failed.append("the wiring")
    if not args.mutate and not args.write:
        rep = Report()
        pass_stale(args.base, rep)
        pass_moved_pins(args.base, rep)
        if rep.notes:
            listing("Worth knowing", rep.notes)
    print()
    if failed:
        print("Something to fix in " + ", ".join(failed) + ".")
        return 1
    print(f"Nothing to fix in {len(paths)} models, the pin table and the wiring.")
    return 0
