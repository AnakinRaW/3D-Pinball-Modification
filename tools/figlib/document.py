"""A document as figure tokens, each with its line, its column, its group and its section."""
from __future__ import annotations

import dataclasses
import pathlib
import re

from .quantities import UNITS, Q


UNIT_ALT = "|".join(re.escape(u) for u in
                    sorted((u for u in UNITS if u and u != "×"), key=len, reverse=True))
NUM = r"\d+(?:\.\d+)?"
TOKEN_RE = re.compile(rf"(?<![\w.])({NUM})\s?({UNIT_ALT})(?![\wµ°])")
RATIO_RE = re.compile(rf"(?<![\w.])({NUM})(×)")
BARE_RE = re.compile(rf"(?<![\w.])({NUM})(?![\w.]|\s?[A-Za-zµ°%×τ])")
RANGE_RE = re.compile(rf"(?<![\w.])({NUM})\s*(?:…|\.\.\.| to )\s*({NUM})\s?({UNIT_ALT})(?![\wµ°])")
RELATION_RE = re.compile(r"[=≈→≤≥]")
# a line that states a bound rather than a value: the printed figure has to be
# on the side that keeps the inequality true
INEQUALITY_RE = re.compile(r"[<>≤≥]")


NAME_END_RE = re.compile(r"\s{2,}|[=≈→≤≥]|:\s")


@dataclasses.dataclass
class Token:
    line: int
    col: int
    text: str
    value: Q
    unit: str
    decimals: int
    context: str
    group: str | None
    section: str | None
    strict: bool
    rhs: bool
    claimed_by: str | None = None
    doc: pathlib.Path | None = None
    cell: bool = False      # a table cell holding a bare number, which nothing requires to be claimed

    @property
    def precision(self) -> float:
        """The grid the document printed this figure on, in display units."""
        return 10.0 ** -self.decimals


def _decimals(num: str) -> int:
    return len(num.split(".")[1]) if "." in num else 0


def norm(text: str) -> str:
    return (text.replace("Ω", "Ω").replace("μ", "µ")
                .replace("&#937;", "Ω").replace("&#181;", "µ"))


def norm_map(raw: str) -> tuple[str, list[int]]:
    """The normalized text, and for each of its characters where it sits in raw."""
    out, where = [], []
    for m in re.finditer(r"&#937;|&#181;|.", raw, re.S):
        out.append(norm(m.group()))
        where.append(m.start())
    where.append(len(raw))
    return "".join(out), where


def group_name(line: str) -> str | None:
    """The name a code-block line in the first column gives its group, up to the first
    double space, relation sign or colon."""
    m = NAME_END_RE.search(line)
    name = (line[:m.start()] if m else line).strip()
    if not name:
        return None
    if TOKEN_RE.fullmatch(name) or re.fullmatch(NUM, name):
        # a part named by its value states a result; a summed term carries no relation
        rest = line[m.end():] if m else ""
        if not RELATION_RE.search(rest):
            return None
    return name


def scan_tokens(text: str, section: str | None, group: str | None,
                line_no: int, strict: bool, offset: int = 0) -> list[Token]:
    found, consumed = [], []
    rel = RELATION_RE.search(text)
    for m in RANGE_RE.finditer(text):
        lo, hi, unit = m.groups()
        consumed.append(m.span())
        found.append((m.start(1), lo, unit, strict))
        found.append((m.start(2), hi, unit, strict))
    for pattern in (TOKEN_RE, RATIO_RE):
        for m in pattern.finditer(text):
            if not any(a <= m.start() < b for a, b in consumed):
                found.append((m.start(), m.group(1), m.group(2), strict))
                consumed.append(m.span())
    # a number with no unit can be addressed, but an expression is full of them
    if strict and rel:
        for m in BARE_RE.finditer(text):
            if m.start() > rel.start() and not any(a <= m.start() < b
                                                   for a, b in consumed):
                found.append((m.start(), m.group(1), "", False))
    return [Token(line_no, pos + offset, f"{num} {unit}".strip(), Q.of(float(num), unit), unit,
                  _decimals(num), text, group, section, is_strict,
                  rhs=bool(rel and pos > rel.start()))
            for pos, num, unit, is_strict in sorted(found)]


NEVER = chr(0x0a)   # no line starts with a newline, so an extra document is read whole


def parse_document(path: pathlib.Path, section_head: str | None,
                   until: str) -> list[Token]:
    """Every figure token in the document, with the group and section holding it."""
    body = norm(path.read_text(encoding="utf-8"))
    lines = body.split("\n")
    start = 0 if section_head is None else next(
        i for i, l in enumerate(lines) if l.startswith(section_head))
    end = next((i for i, l in enumerate(lines) if i > start and l.startswith(until)),
               len(lines))
    tokens, section, group, infence = [], None, None, False
    for i in range(start, end):
        raw = lines[i]
        if raw.strip().startswith("```"):
            infence = not infence
            group = None
            continue
        m = re.match(r"\*\*(.+?)\.?\*\*", raw) or re.match(r"#{2,5}\s+(.+?)\s*$", raw)
        if m and not infence:
            section = m.group(1)
        if infence:
            if raw and not raw.startswith(" "):
                group = group_name(raw) or group
            tokens += scan_tokens(raw, section, group or section, i + 1, strict=True)
        elif raw.startswith("| ") and "---" not in raw:
            cells, cursor = [], 0
            for part in raw.strip().strip("|").split("|"):
                text = part.strip()
                col = raw.index(text, cursor) if text else cursor
                cursor = col + len(text)
                cells.append((text, col))
            head = re.sub(r"\*\*", "", cells[0][0])
            for text, col in cells[1:]:
                if re.fullmatch(NUM, text):
                    # a priority, a pin or a count: addressable, never required
                    tokens.append(Token(i + 1, col, text, Q.of(float(text), ""), "",
                                        _decimals(text), text,
                                        head or section, section, False, False, cell=True))
                    continue
                tokens += scan_tokens(text, section, head or section, i + 1,
                                      strict=True, offset=col)
        else:
            tokens += scan_tokens(raw, section, None, i + 1, strict=False)
    for t in tokens:
        t.doc = path
    return tokens
