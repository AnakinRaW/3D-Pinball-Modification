"""--write: every figure the model computes into the line and the drawing that state it."""
from __future__ import annotations

from collections import defaultdict

from .checks import DATA_FIG_RE, element_figures, print_as
from .document import Token, norm_map
from .model import Model
from .repo import rel
from .report import Report


def _renumber(line: str, col: int, old_num: str, new_num: str):
    """Put a new figure where the old one sat, taking any growth from the blanks in front of
    it so the hand-aligned column stays. Says whether it could."""
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


def pass_write(model: Model, tokens: list[Token], rep: Report) -> int:
    """Write every figure the model computes into the line that states it.

    A block in which a figure found no line of its own is written nowhere, since a line
    found for another figure of that block may be its own.
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
                 + " found no line of their own, so a line found for another figure of "
                   "that block may be theirs, and none is written")

    written = 0
    for path, items in edits.items():
        lines = path.read_text(encoding="utf-8").split("\n")
        for t, have, want in sorted(items, key=lambda i: (-i[0].line, -i[0].col)):
            # a column counts in the normalized line, where an entity is one character
            col = norm_map(lines[t.line - 1])[1][t.col]
            fixed, kept = _renumber(lines[t.line - 1], col, have, want)
            if fixed is None:
                rep.error(f"{path.name}:{t.line} could not find {have!r} "
                          f"at column {col}")
                continue
            lines[t.line - 1] = fixed
            written += 1
            shifted += not kept
            rep.note(f"wrote       {path.name}:{t.line} {have} -> {want} "
                     f"({t.claimed_by})" + ("" if kept else ", column moved"))
        path.write_text("\n".join(lines), encoding="utf-8", newline="\n")

    # A drawing names each figure's key. The number is replaced at its offset in the file
    # as it stands, since an entity ahead of it moves it in the normalized text.
    drawn = 0
    for path in model.drawings:
        if not path.exists():
            rep.error(f"{rel(path)} is named by the model and does not exist")
            continue
        body = path.read_text(encoding="utf-8")
        cuts = []
        for m in DATA_FIG_RE.finditer(body):
            text, where = norm_map(m.group(4))
            for key, f, t in element_figures(model, text, m.group(3)):
                if t is None:
                    continue
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

    total = written + drawn
    if shifted:
        rep.note(f"{shifted} of {total} figures grew past the blanks in front of them, "
                 f"so their column moved")
    return total
