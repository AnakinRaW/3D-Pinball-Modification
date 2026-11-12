"""Check a perfboard layout and draw it.

    python tools/perfboard/build.py <layout.py>              check only
    python tools/perfboard/build.py <layout.py> <out.svg>    check, then draw
    --mirror      turn the board over its short edge for the bottom side, column 1 on the right

The run lists every error (cannot be built, or does not make the netlist) and every house-style
note, and exits with 1 when an error remains. It draws only a layout without errors, unless
--draw-anyway is given.
"""
import importlib.util
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from check import report          # noqa: E402
from render import render         # noqa: E402


def load(path):
    spec = importlib.util.spec_from_file_location("layout_file", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod.LAYOUT


def main(argv):
    args = [a for a in argv if not a.startswith("--")]
    if not args:
        print(__doc__)
        return 2
    L = load(args[0])
    E, W, text = report(L)
    print(text)
    if len(args) > 1 and (not E or "--draw-anyway" in argv):
        render(L, args[1], mirror="--mirror" in argv)
        print(f"drawn: {args[1]}")
    return 1 if E else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
