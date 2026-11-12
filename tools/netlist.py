#!/usr/bin/env python3
"""The netlist a subsystem's model declares for each of its boards, printed, written out for KiCad,
or compared with a netlist KiCad exported.

    python tools/netlist.py model/parts/solenoid/figures.py                  print every board
    python tools/netlist.py model/parts/solenoid/figures.py coil --kicad coil.net
    python tools/netlist.py model/parts/solenoid/figures.py coil --diff kicad-export.net

KiCad numbers a part's pins by its footprint's pads. A part whose inventory entry names its pins,
G, D and S of a TO-220 or K and A of a diode, takes the pads in the order the entry lists them.
"""
import argparse
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
sys.modules.setdefault("figcheck", __import__("figcheck"))

from figlib.repo import load_model, model_file  # noqa: E402

# KiCad standard library footprints for the inventory entries; names from the KiCad library,
# not checked against an installed KiCad
FOOTPRINTS = {
    "resistor": "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal",
    "diode_do41": "Diode_THT:D_DO-41_SOD81_P10.16mm_Horizontal",
    "ceramic_k15": "Capacitor_THT:C_Disc_D3.0mm_W1.6mm_P2.50mm",
    "electrolytic_esl_c3": "Capacitor_THT:CP_Radial_D5.0mm_P2.00mm",
    "to220": "Package_TO_SOT_THT:TO-220-3_Vertical",
    "header_male": "Connector_PinHeader_2.54mm:PinHeader_1x{n:02d}_P2.54mm_Vertical",
    "terminal_5mm": ("TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-{n}_1x{n:02d}_P5.00mm"
                     "_Horizontal"),
}


def pad(part, pin) -> str:
    return str(part.pins.index(pin) + 1)


def q(s) -> str:
    return '"' + str(s).replace("\\", "\\\\").replace('"', '\\"') + '"'


def kicad(model, board) -> str:
    """The board as a KiCad netlist, version E."""
    out = [f"(export (version \"E\")",
           f"  (design (source {q(model.path.name)}) (tool \"tools/netlist.py\"))",
           "  (components"]
    for part in board.parts.values():
        value = part.type or part.key
        if part.value:
            f = model.figure(part.value)
            value = f.value.show(f.unit)
        fp = FOOTPRINTS.get(part.key, "").format(n=len(part.pins))
        fields = f" (property (name \"Fig\") (value {q(part.value)}))" if part.value else ""
        out.append(f"    (comp (ref {q(part.ref)}) (value {q(value)}) (footprint {q(fp)}){fields})")
    out += ["  )", "  (nets"]
    for code, net in enumerate(board.nets.values(), 1):
        nodes = " ".join(f"(node (ref {q(r)}) (pin {q(pad(board.parts[r], p))}))"
                         for r, _, p in (x.partition(".") for x in net.pins))
        out.append(f"    (net (code {q(code)}) (name {q(net.name)}) {nodes})")
    out += ["  )", ")"]
    return "\n".join(out) + "\n"


TOKEN_RE = re.compile(r'\s*(\(|\)|"(?:\\.|[^"\\])*"|[^\s()]+)')


def parse(text):
    """A KiCad S-expression as nested lists of strings."""
    stack, cur = [], []
    for tok in TOKEN_RE.findall(text):
        if tok == "(":
            stack.append(cur)
            cur = []
        elif tok == ")":
            done, cur = cur, stack.pop()
            cur.append(done)
        else:
            cur.append(tok[1:-1].replace('\\"', '"') if tok.startswith('"') else tok)
    return cur[0]


def find(node, name):
    return [c for c in node if isinstance(c, list) and c and c[0] == name]


def read_kicad(path) -> tuple[set, set]:
    """The parts and the nets of a KiCad netlist, a net as the set of its REF.PAD."""
    root = parse(pathlib.Path(path).read_text(encoding="utf-8"))
    parts = {find(c, "ref")[0][1] for comps in find(root, "components")
             for c in find(comps, "comp")}
    nets = set()
    for nets_node in find(root, "nets"):
        for net in find(nets_node, "net"):
            pins = frozenset(f"{find(n, 'ref')[0][1]}.{find(n, 'pin')[0][1]}"
                             for n in find(net, "node"))
            if len(pins) > 1:
                nets.add(pins)
    return parts, nets


def diff(board, path) -> list[str]:
    """What a KiCad netlist joins differently from the board's declaration. Net names do not
    count, since KiCad names a net it was given no name for after one of its pins."""
    theirs_parts, theirs = read_kicad(path)
    ours = {frozenset(f"{r}.{pad(board.parts[r], p)}" for r, _, p in
                      (x.partition(".") for x in n.pins)) for n in board.nets.values()}
    names = {frozenset(f"{r}.{pad(board.parts[r], p)}" for r, _, p in
                       (x.partition(".") for x in n.pins)): n.name for n in board.nets.values()}
    out = [f"only in the model: part {r}" for r in sorted(board.parts.keys() - theirs_parts)]
    out += [f"only in KiCad: part {r}" for r in sorted(theirs_parts - board.parts.keys())]
    out += [f"only in the model: {names[n]} joins {', '.join(sorted(n))}"
            for n in sorted(ours - theirs, key=sorted)]
    out += [f"only in KiCad: a net joins {', '.join(sorted(n))}"
            for n in sorted(theirs - ours, key=sorted)]
    return out


def show(board) -> str:
    lines = [f"{board.title} ({board.name}): {len(board.parts)} parts, {len(board.nets)} nets"]
    for n in board.nets.values():
        lines.append(f"  {n.name:<10} " + " ".join(n.pins)
                     + (f"   Teensy: {n.teensy}" if n.teensy else ""))
    for p, why in board.open.items():
        lines.append(f"  free       {p}   {why}")
    return "\n".join(lines)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("model", type=pathlib.Path, help="a subsystem's figures.py")
    ap.add_argument("board", nargs="?", help="the board, every board when none is named")
    ap.add_argument("--kicad", type=pathlib.Path, metavar="OUT", help="write a KiCad netlist")
    ap.add_argument("--diff", type=pathlib.Path, metavar="NET",
                    help="compare with a netlist KiCad exported")
    args = ap.parse_args(argv)
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    model = load_model(model_file(args.model))
    if args.board and args.board not in model.boards:
        ap.error(f"{model.name} declares the boards {', '.join(model.boards) or 'none'}")
    boards = [model.boards[args.board]] if args.board else list(model.boards.values())
    if (args.kicad or args.diff) and len(boards) != 1:
        ap.error("--kicad and --diff take one board")
    if args.kicad:
        args.kicad.write_text(kicad(model, boards[0]), encoding="utf-8")
        print(f"{boards[0].name}: {len(boards[0].parts)} parts and {len(boards[0].nets)} nets "
              f"written to {args.kicad}")
        return 0
    if args.diff:
        found = diff(boards[0], args.diff)
        for line in found:
            print("  " + line)
        print(f"{len(found)} difference{'' if len(found) == 1 else 's'} between "
              f"{boards[0].name} and {args.diff}.")
        return 1 if found else 0
    print("\n\n".join(show(b) for b in boards))
    return 0


if __name__ == "__main__":
    sys.exit(main())
