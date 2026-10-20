"""What --graph, --blind, --provenance and --groups print instead of a check."""
from __future__ import annotations

from collections import defaultdict

from .repo import load_model


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


def print_blind(model) -> int:
    """The brief for an independent derivation: every quantity, its unit and its
    inputs, with no formula and no computed value."""
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


def print_provenance(model) -> int:
    """Every input by kind, with its value and its source."""
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


def print_groups(model, tokens) -> int:
    """The block groups and the value tokens the parser found, beside the figures
    each group declares."""
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
