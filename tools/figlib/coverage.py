"""What the run reads at all: every document and drawing a model's folder holds, every part of
a governed document, and every link between the documents."""
from __future__ import annotations

import re
import unicodedata

from .repo import ROOT, load_model, rel
from .report import Report, listing, status

# the folders whose documents and drawings state the design's figures
GOVERNED_TREES = (ROOT / "docs" / "parts", ROOT / "docs" / "firmware")
LINKED_FILES = (*(ROOT / "docs").rglob("*.md"), *ROOT.glob("*.md"), *(ROOT / "tools").glob("*.md"))
LINK_RE = re.compile(r"(?<!!)\[[^\]]*\]\(([^)\s]+)(?:\s+\"[^\"]*\")?\)")
HEADING_RE = re.compile(r"^#{1,6}\s+(.+?)\s*#*\s*$")


def _governed(models) -> tuple[dict, dict]:
    """Every file some model reads, and every file some model leaves out with its reason."""
    read, left = {}, {}
    for m in models:
        for p in (m.document, *m.documents, *m.drawings):
            read[p.resolve()] = m.name
        for p, why in m.ignored:
            left[p] = (m.name, why)
    return read, left


def pass_files(models, rep: Report) -> int:
    """A document or drawing in a folder that states the design is read by a model, or a model
    leaves it out and says why. A model reads the whole of its document."""
    read, left = _governed(models)
    files = sorted(p.resolve() for tree in GOVERNED_TREES for p in tree.rglob("*")
                   if p.is_file() and p.suffix in (".md", ".svg"))
    for p in files:
        if p not in read and p not in left:
            rep.error(f"{rel(p)} is read by no model and left out by none, so no number in it "
                      f"is checked", kind="file")
    for p, (name, _why) in left.items():
        if not p.exists():
            rep.error(f"{name} leaves out {rel(p)}, which does not exist", kind="file")
        elif p in read:
            rep.error(f"{name} leaves out {rel(p)}, which {read[p]} reads", kind="file")
    for m in models:
        if m.section is not None or m.until != "## Sources":
            rep.error(f"{m.name} reads only part of {rel(m.document)}, so a number outside "
                      f"that part is never checked", kind="file")
    return len(files)


def slug(heading: str) -> str:
    """The anchor GitHub gives a heading."""
    text = re.sub(r"`|\*\*|__|\[([^\]]*)\]\([^)]*\)", r"\1", heading).strip().lower()
    text = "".join(c for c in text
                   if c in " -_" or unicodedata.category(c)[0] in "LN")
    return text.replace(" ", "-")


def anchors(path) -> set:
    """Every anchor a markdown file offers, a repeated heading numbered the way GitHub does."""
    out, seen, infence = set(), {}, False
    for line in path.read_text(encoding="utf-8").split("\n"):
        if line.strip().startswith("```"):
            infence = not infence
            continue
        m = None if infence else HEADING_RE.match(line)
        if m:
            s = slug(m.group(1))
            n = seen.get(s, 0)
            out.add(s if n == 0 else f"{s}-{n}")
            seen[s] = n + 1
    return out


def pass_links(rep: Report) -> int:
    """Every relative link reaches a file, and every anchor a heading of it."""
    count = 0
    for path in sorted(set(LINKED_FILES)):
        infence = False
        for i, line in enumerate(path.read_text(encoding="utf-8").split("\n"), 1):
            if line.strip().startswith("```"):
                infence = not infence
                continue
            if infence:
                continue
            for target in LINK_RE.findall(re.sub(r"`[^`]*`", "", line)):
                if re.match(r"[a-z]+:", target):
                    continue
                count += 1
                file, _, anchor = target.partition("#")
                dest = (path.parent / file).resolve() if file else path
                if not dest.exists():
                    rep.error(f"{rel(path)}:{i} links {file}, which does not exist",
                              kind="link")
                elif anchor and dest.suffix == ".md" and anchor not in anchors(dest):
                    rep.error(f"{rel(path)}:{i} links #{anchor} in {rel(dest)}, which has no "
                              f"such heading", kind="link")
    return count


def run_coverage(paths) -> int:
    """The files no model reads, the parts of a document a model skips, and broken links."""
    print("Checking what the run reads")
    models = [load_model(p) for p in paths]
    rep = Report()
    files = pass_files(models, rep)
    links = pass_links(rep)
    status(not rep.count("file"), "every file read or left out",
           f"{files} documents and drawings, {rep.count('file')} that no model accounts for")
    status(not rep.count("link"), "every link reaches its target",
           f"{links} relative links, {rep.count('link')} broken")
    if rep.errors:
        listing("To fix", rep.errors)
        return 1
    return 0
