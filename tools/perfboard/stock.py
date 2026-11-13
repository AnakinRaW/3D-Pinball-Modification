"""What every layout of the build takes from stock: perfboards and screw terminals, held
against the counts in inventory.py.

    python tools/perfboard/stock.py
"""
import pathlib
import runpy
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from inventory import BOARDS, TERMINALS  # noqa: E402


def main():
    root = HERE.parents[1]
    boards, terminals, rows = {}, {}, []
    for path in sorted(root.glob("model/**/*.layout.py")):
        L = runpy.run_path(str(path))["LAYOUT"]
        B = L.board
        boards[B.kind] = boards.get(B.kind, 0) + 1
        cut = f", cut to columns {B.cut[0]} to {B.cut[1]}" if B.cut else ""
        rows.append(f"  {path.relative_to(root).as_posix():55} {B.kind}{cut}")
        for p in L.parts:
            if p.kind == "terminal":
                terminals[len(p.holes)] = terminals.get(len(p.holes), 0) + 1
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
    print("\n" + ("Everything comes from stock." if not bad else f"{bad} item(s) exceed the stock."))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
