"""What every layout of the build takes from stock: perfboards and screw terminals, held
against the counts in inventory.py.

    python tools/perfboard/stock.py

A layout that is one of several ways to build the same thing names its family and the variants
it belongs to, as VARIANT = ("IR sensing", ["16 channels"]). Only one variant of a family is
built, so a variant takes no stock from the others: every combination of one variant per family,
with every layout that names no variant, has to fit the stock on its own. The counts shown are
the largest any combination takes.
"""
import itertools
import pathlib
import runpy
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from inventory import BOARDS, TERMINALS  # noqa: E402


def takes(L):
    """The perfboard and the screw terminals of one layout, as counts by kind."""
    need = {("board", L.board.kind): 1}
    for p in L.parts:
        if p.kind == "terminal":
            need[("terminal", len(p.holes))] = need.get(("terminal", len(p.holes)), 0) + 1
    return need


def main():
    root = HERE.parents[1]
    always, families, rows = [], {}, []
    for path in sorted(root.glob("model/**/*.layout.py")):
        mod = runpy.run_path(str(path))
        L = mod["LAYOUT"]
        B = L.board
        cut = f", cut to columns {B.cut[0]} to {B.cut[1]}" if B.cut else ""
        variant = mod.get("VARIANT")
        tag = ""
        if variant:
            family, names = variant
            for name in names:
                families.setdefault(family, {}).setdefault(name, []).append(takes(L))
            tag = f"  {family}: {', '.join(names)}"
        else:
            always.append(takes(L))
        rows.append(f"  {path.relative_to(root).as_posix():55} {B.kind}{cut}{tag}")

    def total(needs):
        out = {}
        for need in needs:
            for k, v in need.items():
                out[k] = out.get(k, 0) + v
        return out

    combos = [dict(zip(families, pick)) for pick in
              itertools.product(*(sorted(v) for v in families.values()))] or [{}]
    worst, failing = {}, []
    for combo in combos:
        t = total(always + [n for f, name in combo.items() for n in families[f][name]])
        for k, v in t.items():
            worst[k] = max(worst.get(k, 0), v)
        over = [k for k, v in t.items()
                if v > (BOARDS[k[1]][2] if k[0] == "board" and k[1] in BOARDS
                        else TERMINALS.get(k[1], 0) if k[0] == "terminal" else 0)]
        if over:
            failing.append((combo, over))
    boards = {k[1]: v for k, v in worst.items() if k[0] == "board"}
    terminals = {k[1]: v for k, v in worst.items() if k[0] == "terminal"}
    print("Layouts")
    print("\n".join(rows))
    bad = 0
    print("\nPerfboards        used  in stock")
    for kind, (_s, _l, have) in BOARDS.items():
        used = boards.get(kind, 0)
        flag = "  too few" if used > have else ""
        bad += used > have
        print(f"  {kind:14} {used:5} {have:9}{flag}")
    print("\nScrew terminals   used  in stock")
    for poles, have in sorted(TERMINALS.items()):
        used = terminals.get(poles, 0)
        flag = "  too few" if used > have else ""
        bad += used > have
        print(f"  {poles}-pole         {used:5} {have:9}{flag}")
    for poles in sorted(set(terminals) - set(TERMINALS)):
        bad += 1
        print(f"  {poles}-pole         {terminals[poles]:5}         0  none in stock")
    for combo, over in failing:
        which = "; ".join(f"{f}: {name}" for f, name in combo.items()) or "the build"
        print(f"  {which} exceeds the stock")
    print("\n" + ("Everything comes from stock." if not bad else f"{bad} item(s) exceed the stock."))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
