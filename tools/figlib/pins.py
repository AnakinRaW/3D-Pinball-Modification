"""Teensy pins: the pin table, the pins each file marks, what the allocation costs, and the pins
a diff moved."""
from __future__ import annotations

import dataclasses
import pathlib
import re
import subprocess
from collections import defaultdict

from .document import UNIT_ALT, norm, norm_map
from .model import PinData
from .repo import ROOT, pin_data, rel
from .report import Report, listing, status


PIN_TABLE = ROOT / "docs" / "pin-assignment.md"
# markdown: [39](../../pin-assignment.md "Solenoid trigger 2")
PIN_LINK_RE = re.compile(r'\[(\d+)\]\(([^)\s]*pin-assignment\.md)(?:#[^)\s]*)?\s+"([^"]+)"\)')
# a fenced line: kTrigger[kCoils] = {40, 39, 38, 37};  // pin-assignment.md: Solenoid trigger 1 to 4
PIN_NOTE_RE = re.compile(r"//\s*[\w./-]*pin-assignment\.md:\s*(.+?)\s*$")
# a drawing: <text data-pin="Solenoid sense 1">pin 36</text>
DATA_PIN_RE = re.compile(r'<(\w+)\b[^>]*\bdata-pin="([^"]+)"[^>]*>([^<]*)<')
# a number, also one that ends a sentence, though not the integer part of a decimal
PIN_NUM_RE = re.compile(r"(?<![\w.])\d+(?!\w|\.\d)")
PIN_RANGE_RE = re.compile(r"^(.*?)(\d+) to (\d+)$")
# "Teensy pin 31" or "the Teensy's pin 0" names an allocation, which carries the mark
NAMED_PIN_RE = re.compile(rf"\bTeensy(?:'s)? pins? (\d+)(?!\w|\.\d)"
                          rf"(?!\s?(?:{UNIT_ALT})(?![\wµ°]))")
# a line that names the board, its maker, its pads or a peripheral speaks of Teensy pins
PIN_CONTEXT_RE = re.compile(r"Teensy|PJRC|core_pins|GPIO_|FlexPWM|FlexIO|QuadTimer|I²S\d|"
                            r"Serial\d|CAN\d|S/PDIF|\bWire\d?\b|\bSPI\d?\b|\bPWM\b|[Aa]nalog")
PERIPHERAL_RE = re.compile(r"FlexPWM\d\.\d|FlexIO\d|QuadTimer\d|I²S\d|Serial\d|CAN\d|"
                           r"\bWire\d?\b|\bSPI\d?\b")
LOOSE_NUM_RE = re.compile(rf"(?<![\w.])(\d{{1,2}})(?!\w|\.\d)(?!\s?(?:{UNIT_ALT})(?![\wµ°]))")
PIN_PHRASE_RE = re.compile(r"\b[Pp]ins? \d+")
# "pin 13", "pins 2, 3 and 4", "pins 11 to 16": the numbers a pin phrase lists
PIN_LIST_RE = re.compile(r"\b[Pp]ins?\s+\[?\d{1,2}\]?(?:(?:\s*,\s*|\s+(?:and|or|to)\s+)\[?\d{1,2}\]?)*")
# a connector named right before the phrase owns the pin: "J-T pin 7"
CONNECTOR_BEFORE_RE = re.compile(r"\bJ-[\w-]+\s*$")
SVG_TEXT_RE = re.compile(r"<!--(.*?)-->|>([^<>]+)<")
# files that describe the board and the stock machine rather than this build's allocation
UNALLOCATED = (ROOT / "docs" / "research", ROOT / "docs" / "datasheets")


@dataclasses.dataclass
class PinRef:
    doc: pathlib.Path
    line: int
    pos: int        # where the number sits in the normalized file
    number: str
    signal: str


@dataclasses.dataclass
class PinRow:
    pin: int
    signal: str         # the Signal cell up to its first comma
    peripheral: str
    subsystems: tuple   # the models the Subsystem cell links to, else the cell's text
    reserved: bool
    line: int
    owner: str = ""     # the Subsystem cell as it reads, without its links


def _md_tables(text: str):
    """Every table of a markdown text: its section, its header, its rows as (line, cells, raw)."""
    out, section, header, rows = [], "", None, []
    for i, raw in enumerate(text.split("\n") + [""], 1):
        if raw.startswith("## "):
            section = raw[3:].strip()
        if not raw.startswith("|"):
            if header is not None:
                out.append((section, header, rows))
            header, rows = None, []
            continue
        cells = [c.strip() for c in raw.strip().strip("|").split("|")]
        if header is None:
            header = cells
        elif not all(set(c) <= set("-: ") for c in cells):
            rows.append((i, cells, raw))
    return out


def _pin_rows(text: str) -> list[PinRow]:
    """Every row of the tables headed Pin, Signal: the allocation, then the reserved pins."""
    rows = []
    for section, header, body in _md_tables(text):
        if header[:2] != ["Pin", "Signal"]:
            continue
        for line, cells, _ in body:
            if len(cells) < 3 or not cells[0].isdigit():
                continue
            cell = cells[3] if len(cells) > 3 else ""
            links = tuple(dict.fromkeys(re.findall(r"\(parts/([\w-]+)/", cell)))
            rows.append(PinRow(int(cells[0]), re.sub(r"[*`]", "", cells[1]).split(",")[0].strip(),
                               cells[2], links or (cell,), section == "Reserved",
                               line, re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", cell).strip()))
    return rows


def pin_rows() -> list[PinRow]:
    return _pin_rows(PIN_TABLE.read_text(encoding="utf-8"))


def pin_table(rep: Report) -> dict[str, int]:
    """Signal to pin, from the tables headed Pin, Signal. A signal or a pin listed twice fails."""
    table, first, holder = {}, {}, {}
    for r in pin_rows():
        if r.signal in table:
            rep.error(f"{rel(PIN_TABLE)}:{r.line} lists {r.signal!r} again, first on line "
                      f"{first[r.signal]}", kind="pin")
            continue
        if r.pin in holder:
            rep.error(f"{rel(PIN_TABLE)}:{r.line} gives pin {r.pin} to {r.signal!r}, and line "
                      f"{first[holder[r.pin]]} gives it to {holder[r.pin]!r}", kind="pin")
        table[r.signal], first[r.signal], holder[r.pin] = r.pin, r.line, r.signal
    return table


def _signals(text: str, table: dict[str, int]) -> list[str]:
    """The signals a note or a data-pin names, `Solenoid trigger 1 to 4` spelled out."""
    out = []
    for item in (s.strip() for s in re.split(r"[,;]", text)):
        m = PIN_RANGE_RE.match(item)
        if item not in table and m:
            out += [f"{m.group(1)}{n}" for n in range(int(m.group(2)), int(m.group(3)) + 1)]
        elif item:
            out.append(item)
    return out


def pin_refs(path: pathlib.Path, table: dict[str, int], rep: Report):
    """Every marked pin in one file, and the spans the figure passes skip: each number and,
    in a fenced block, the note naming it."""
    body = norm(path.read_text(encoding="utf-8"))
    line_at = lambda pos: body.count("\n", 0, pos) + 1
    refs, spans = [], []
    if path.suffix == ".svg":
        for m in DATA_PIN_RE.finditer(body):
            names = _signals(m.group(2), table)
            nums = list(PIN_NUM_RE.finditer(m.group(3)))
            if len(nums) != len(names):
                rep.error(f"{rel(path)}:{line_at(m.start())} names {len(names)} "
                          f"signal(s) in data-pin and shows {len(nums)} number(s)",
                          kind="pin")
                continue
            for n, name in zip(nums, names):
                pos = m.start(3) + n.start()
                refs.append(PinRef(path, line_at(pos), pos, n.group(), name))
        return refs, spans

    infence, off = False, 0
    for i, raw in enumerate(body.split("\n")):
        start, off = off, off + len(raw) + 1
        if raw.strip().startswith("```"):
            infence = not infence
            continue
        if infence:
            note = PIN_NOTE_RE.search(raw)
            if not note:
                continue
            eq = raw.find("=")
            nums = (list(PIN_NUM_RE.finditer(raw, eq + 1, note.start()))
                    if 0 <= eq < note.start() else [])
            names = _signals(note.group(1), table)
            if len(nums) != len(names):
                rep.error(f"{rel(path)}:{i + 1} names {len(names)} signal(s) for "
                          f"{len(nums)} number(s) right of its '='", kind="pin")
                continue
            for n, name in zip(nums, names):
                refs.append(PinRef(path, i + 1, start + n.start(), n.group(), name))
                spans.append((i + 1, n.start(), n.end()))
            spans.append((i + 1, note.start(), len(raw)))
            continue
        for m in PIN_LINK_RE.finditer(raw):
            if (path.parent / m.group(2)).resolve() != PIN_TABLE.resolve():
                rep.error(f"{rel(path)}:{i + 1} links {m.group(2)}, which does not "
                          f"lead to {rel(PIN_TABLE)}", kind="pin")
                continue
            refs.append(PinRef(path, i + 1, start + m.start(1), m.group(1), m.group(3)))
            spans.append((i + 1, m.start(), m.end()))
    return refs, spans


def pass_pins(files, rep: Report):
    """Check every marked pin in the files against docs/pin-assignment.md."""
    table = pin_table(rep)
    refs, spans = [], defaultdict(list)
    for path in files:
        found, taken = pin_refs(path, table, rep)
        refs += found
        for ln, a, b in taken:
            spans[(path.resolve(), ln)].append((a, b))
    for path in files:
        for line, number in named_pins(path):
            rep.error(f"{rel(path)}:{line} names Teensy pin {number} without the mark; "
                      f"mark it with the signal it carries, or leave the number out",
                      kind="pin", key="named")
    good = 0
    for r in refs:
        if r.signal not in table:
            rep.error(f"{rel(r.doc)}:{r.line} names {r.signal!r}, which "
                      f"{rel(PIN_TABLE)} does not list", kind="pin")
        elif int(r.number) != table[r.signal]:
            rep.error(f"{rel(r.doc)}:{r.line} gives {r.signal} pin {r.number}, and "
                      f"{rel(PIN_TABLE)} gives it {table[r.signal]}", kind="pin")
        else:
            good += 1
    return table, refs, spans, good


def _allocating(path: pathlib.Path) -> bool:
    """Whether a file's pin numbers speak of this build's allocation."""
    p = pathlib.Path(path).resolve()
    return p != PIN_TABLE.resolve() and not any(d.resolve() in p.parents
                                                for d in UNALLOCATED)


def _unmarked_lines(path: pathlib.Path) -> list[str]:
    """The file's lines with every marked pin cut out, a drawing's down to its text and comments."""
    out = []
    for raw in norm(path.read_text(encoding="utf-8")).split("\n"):
        if PIN_NOTE_RE.search(raw):
            raw = ""
        raw = PIN_LINK_RE.sub("", raw)
        if path.suffix == ".svg":
            raw = DATA_PIN_RE.sub("<", raw)
            raw = " | ".join(a or b for a, b in SVG_TEXT_RE.findall(raw))
        out.append(raw)
    return out


def named_pins(path: pathlib.Path) -> list[tuple[int, str]]:
    """Every number the file calls a Teensy pin without the mark, with its line."""
    if not _allocating(path):
        return []
    return [(i, m.group(1)) for i, line in enumerate(_unmarked_lines(path), 1)
            for m in NAMED_PIN_RE.finditer(line)]


def pass_moved_pins(base: str, rep: Report, files=None):
    """Where a pin this diff moved, or a timer or port the table dropped, still stands unmarked.
    Listed rather than failed, since only a reader tells a stale pin from a capability."""
    try:
        old_text = subprocess.run(["git", "show", f"{base}:{rel(PIN_TABLE)}"],
                                  cwd=ROOT, capture_output=True, text=True,
                                  encoding="utf-8", check=True).stdout
    except (subprocess.CalledProcessError, FileNotFoundError):
        return
    old, new = _pin_rows(old_text), pin_rows()
    now = {r.signal: r.pin for r in new}
    moved = {r.pin for r in old if now.get(r.signal) != r.pin}
    named = lambda rows: {m.group() for r in rows for m in PERIPHERAL_RE.finditer(r.peripheral)}
    gone = [(g, re.compile(rf"(?<!\w){re.escape(g)}(?!\.?\d)"))
            for g in sorted(named(old) - named(new))]
    if not moved and not gone:
        return
    for path in files or [p for p in sorted((ROOT / "docs").rglob("*"))
                          if p.suffix in {".md", ".svg", ".py"} and p.is_file()]:
        if not _allocating(path):
            continue
        pin_column, heading = False, ""
        for i, line in enumerate(_unmarked_lines(path), 1):
            # the Pin column of a table under a Teensy heading, or every number of a line
            # that names a pin together with the Teensy or a peripheral
            if line.startswith("#"):
                heading = line
            cells = ([c.strip() for c in line.strip().strip("|").split("|")]
                     if line.startswith("|") else None)
            if cells is None:
                pin_column = False
            elif cells[0] == "Pin" and "Teensy" in heading:
                pin_column = True
                continue
            hits = [cells[0]] if pin_column and cells and cells[0].isdigit() else []
            if NAMED_PIN_RE.search(line) or (PIN_PHRASE_RE.search(line)
                                             and PIN_CONTEXT_RE.search(line)):
                # the numbers the pin phrase itself gives, not every small number on the line,
                # and not a connector's own pin
                for m in PIN_LIST_RE.finditer(line):
                    if not CONNECTOR_BEFORE_RE.search(line[:m.start()]):
                        hits += LOOSE_NUM_RE.findall(m.group(0))
            if hits:
                nums = [n for n in dict.fromkeys(hits) if int(n) in moved]
                if nums:
                    rep.note(f"pin{'s' if len(nums) > 1 else ''} {', '.join(nums)} moved in "
                             f"this diff and still stand{'' if len(nums) > 1 else 's'} "
                             f"unmarked in {rel(path)}:{i}")
            for g, rx in gone:
                if rx.search(line):
                    rep.note(f"{g} left the pin table in this diff and still stands in "
                             f"{rel(path)}:{i}")


def write_pins(files, rep: Report) -> int:
    """Put the table's pin into every marked place that carries another one."""
    table = pin_table(rep)
    written = 0
    for path in files:
        refs, _ = pin_refs(path, table, Report())
        wrong = [r for r in refs if r.signal in table and int(r.number) != table[r.signal]]
        if not wrong:
            continue
        raw = path.read_text(encoding="utf-8")
        _, where = norm_map(raw)
        for r in sorted(wrong, key=lambda r: -r.pos):
            a, b = where[r.pos], where[r.pos + len(r.number)]
            want = str(table[r.signal])
            if raw[a:b] != r.number:
                rep.error(f"{rel(path)}:{r.line} could not find pin {r.number} "
                          f"where the marker puts it", kind="pin")
                continue
            raw = raw[:a] + want + raw[b:]
            written += 1
            rep.note(f"wrote       {rel(path)}:{r.line} pin {r.number} -> {want} "
                     f"({r.signal})")
        path.write_text(raw, encoding="utf-8", newline="\n")
    return written


def _runs(pins) -> str:
    """Pins as the tables print them, a run of three or more as its two ends."""
    out, run = [], []
    for p in sorted(pins) + [None]:
        if run and (p is None or p != run[-1] + 1):
            out += [f"{run[0]}–{run[-1]}"] if len(run) > 2 else [str(x) for x in run]
            run = []
        if p is not None:
            run.append(p)
    return ", ".join(out)


def _analog_text(pins: list[int]) -> str:
    """The analog inputs as blocks of consecutive pins, A0–A13 = 14–27 in order."""
    blocks, start = [], 0
    for i in range(1, len(pins) + 1):
        if i == len(pins) or pins[i] != pins[i - 1] + 1:
            a, b = start, i - 1
            blocks.append(f"A{a} = {pins[a]}" if a == b else
                          f"A{a}–A{b} = {pins[a]}–{pins[b]} in order")
            start = i
    return ", ".join(blocks)


def _owner(name: str, word: str, rows) -> str:
    """The subsystems whose rows use a port, a timer or a FlexIO, in the order the table gives them."""
    names = dict.fromkeys(r.owner + (", reserved" if r.reserved else "")
                          for r in rows if r.peripheral.startswith(name + word))
    return " and ".join(names)


def pin_table_rows(data: PinData, rows) -> dict:
    """The rows the board's model gives the pin table, by table header, and whether the model
    gives that table whole. Who uses a resource comes from the allocation rows naming it."""
    edge = set(data.groups["edge"])
    pwm = [p for p in data.pwm() if p in edge]
    ports = [[name, ", ".join(f"{sig} {' / '.join(map(str, pins))}" for sig, pins in signals)
              + (f" ({data.notes[name]})" if name in data.notes else ""), _owner(name, " ", rows)]
             for name, signals in data.ports.items()]
    timers = [[name, ", ".join(f"{p} ({ch})" for p, ch in pins), _owner(name, " channel ", rows)]
              for name, pins in data.timers.items()]
    flexio = [[name, ", ".join(f"{p} ({n})" for p, n in pins), _owner(name, " pin ", rows)]
              for name, pins in data.flexio.items()]
    return {("Resource", "Pins", "Used by"): (ports, True),
            ("Capability", "Pins", "Count"): ([["PWM", _runs(pwm), str(len(pwm))],
                                               ["Analog in", _analog_text(data.analog),
                                                str(len(data.analog))]], False),
            ("Timer", "Pins, with the channel driving each", "Used by"): (timers, True),
            ("FlexIO", "Pins, with the signal each carries", "Used by"): (flexio, True)}


def pass_pin_tables(data: PinData, rep: Report, write: bool) -> int:
    """The pin table's capability rows against the board's model, written under --write."""
    text = PIN_TABLE.read_text(encoding="utf-8")
    lines, want_all = text.split("\n"), pin_table_rows(data, _pin_rows(text))
    edits, checked = [], 0
    for _, header, body in _md_tables(text):
        if tuple(header) not in want_all:
            continue
        want, whole = want_all[tuple(header)]
        have = {cells[0]: (line, cells) for line, cells, _ in body}
        for cells in want:
            checked += 1
            got = have.get(cells[0])
            if got is None:
                rep.error(f"{rel(PIN_TABLE)} has no {cells[0]!r} row under "
                          f"{' | '.join(header)}, which the board's model declares", kind="table")
            elif got[1] != cells:
                rep.error(f"{rel(PIN_TABLE)}:{got[0]} reads {' | '.join(got[1][1:])!r} where the "
                          f"board's model gives {' | '.join(cells[1:])!r}", kind="table")
        if not body:
            continue
        first, last = body[0][0], body[-1][0]
        if whole:
            for name, (line, _) in have.items():
                if name not in {c[0] for c in want}:
                    rep.error(f"{rel(PIN_TABLE)}:{line} lists {name}, which the board's model "
                              f"does not declare", kind="table")
            edits.append((first, last, ["| " + " | ".join(c) + " |" for c in want]))
        else:
            for cells in want:
                row = ["| " + " | ".join(cells) + " |"]
                line = have.get(cells[0], (None,))[0]
                # a missing row goes in after the table's last one
                edits.append((line, line, row) if line else (last + 1, last, row))
    if write and rep.count("table"):
        for first, last, rows in sorted(edits, reverse=True):
            lines[first - 1:last] = rows
        PIN_TABLE.write_text("\n".join(lines), encoding="utf-8", newline="\n")
        rep.note(f"wrote       {rel(PIN_TABLE)}, the rows the board's model gives it")
    return checked


def _pins_text(pins) -> str:
    """Pins as a sentence names them: pin 14, pins 14 and 15, pins 1, 26 and 27."""
    p = [str(x) for x in sorted(pins)]
    return f"pin {p[0]}" if len(p) == 1 else f"pins {', '.join(p[:-1])} and {p[-1]}"


def _numbers(text: str) -> set[int]:
    return {int(n) for n in PIN_NUM_RE.findall(text)}


def _inputs(channels) -> str:
    """Analog inputs as a cost table names them: A6, A7, or none."""
    return ", ".join(f"A{i}" for i in sorted(channels)) or "none"


def _channels(text: str) -> set[int]:
    """The analog inputs a cell names, `A6 to A9` spelled out."""
    out = set()
    for a, b in re.findall(r"\bA(\d+)(?: to A(\d+))?", text):
        out |= set(range(int(a), int(b or a) + 1))
    return out


def pass_costs(data: PinData, rep: Report) -> int:
    """What the allocation and then the reserved pins take, against the two cost tables."""
    rows = pin_rows()
    tables = {section: body for section, header, body in _md_tables(
        PIN_TABLE.read_text(encoding="utf-8")) if header[:2] == ["Lost", "To"]}
    names = sorted([*data.ports, *data.timers, *data.flexio], key=len, reverse=True)
    edge = set(data.groups["edge"])
    pwm = [p for p in data.pwm() if p in edge]
    analog = {p: i for i, p in enumerate(data.analog)}
    taken, gone, checked = set(), set(), 0
    for section, reserved in (("Allocation", False), ("Reserved", True)):
        mine = [r for r in rows if r.reserved == reserved]
        here = {r.pin for r in mine}
        taken |= here
        body = tables.get(section)
        if body is None:
            rep.error(f"{rel(PIN_TABLE)} has no cost table under {section}", kind="cost")
            continue
        # every resource this part of the table takes, with the pins that have to be listed
        lost = {}
        for name, signals in data.ports.items():
            users = {r.pin for r in mine if r.peripheral.startswith(name + " ")}
            needed = [pins for sig, pins in signals if sig not in data.optional]
            if name in gone or not (users or any(set(pins) <= taken for pins in needed)):
                continue
            lost[name] = users | {pins[0] for pins in needed
                                  if len(pins) == 1 and pins[0] in here}
        for name in [*data.timers, *data.flexio]:
            word = " channel " if name in data.timers else " pin "
            users = {r.pin for r in mine if r.peripheral.startswith(name + word)}
            if users and name not in gone:
                lost[name] = users
        listed = set()
        for line, cells, _ in body:
            what, to = cells[0], cells[1]
            where = f"{rel(PIN_TABLE)}:{line}"
            count = re.match(r"(\d+) of (\d+) (PWM pins|analog inputs)$", what)
            if count:
                checked += 1
                n, of, kind = int(count.group(1)), int(count.group(2)), count.group(3)
                if kind == "PWM pins":
                    want, total, got = here & set(pwm), len(pwm), _numbers(to)
                else:
                    head, _, free = to.partition("Free:")
                    want = {analog[p] for p in here if p in analog}
                    total, got = len(analog), _channels(head)
                    unused = {i for p, i in analog.items() if p not in taken}
                    if free and _channels(free) != unused:
                        rep.error(f"{where} names {_inputs(_channels(free))} free, and the free "
                                  f"analog inputs are {_inputs(unused)}", kind="cost")
                if (n, of) != (len(want), total) or got != want:
                    rep.error(f"{where} states {what} with {sorted(got)}, and {section.lower()} "
                              f"takes {len(want)} of {total}: {sorted(want)}", kind="cost")
                continue
            found = [n for n in names if re.search(rf"(?<![\w.]){re.escape(n)}(?![\w.])", what)]
            pins = _numbers(to.split(". ")[0])
            for n in found:
                checked += 1
                listed.add(n)
                if n not in lost:
                    rep.error(f"{where} lists {n} as lost, which {section.lower()} leaves free",
                              kind="cost")
                elif not lost[n] <= pins:
                    rep.error(f"{where} lists {n} without {_pins_text(lost[n] - pins)}",
                              kind="cost")
            if found and not pins <= here:
                rep.error(f"{where} names {_pins_text(pins - here)}, which {section.lower()} "
                          f"does not hold", kind="cost")
        for n in sorted(set(lost) - listed):
            rep.error(f"{section} takes {n}, on {_pins_text(lost[n])}, and its cost table does "
                      f"not name it", kind="cost")
        gone |= set(lost)
    return checked


def run_pins(write: bool) -> int:
    files = sorted(p for p in (ROOT / "docs").rglob("*")
                   if p.suffix in {".md", ".svg"} and p.is_file())
    rep = Report()
    data = pin_data()
    if write:
        n = write_pins(files, rep)
        pass_pin_tables(data, rep, write=True)
        pass_costs(data, rep)
        wrong = [e for e, (k, _) in zip(rep.errors, rep.tags) if k != "table"]
        print(f"{n} pin{'' if n == 1 else 's'} written from {rel(PIN_TABLE)}."
              + (" Everything already agreed." if not n and not rep.errors else ""))
        for m in rep.notes + wrong:
            print("  " + m)
        return 1 if wrong else 0
    table, refs, _, good = pass_pins(files, rep)
    rows = pass_pin_tables(data, rep, write=False)
    costs = pass_costs(data, rep)
    marked = len({r.doc for r in refs})
    print("Checking Teensy pins")
    print(f"  table      {rel(PIN_TABLE)}, {len(table)} signals")
    print(f"  files      {len(files)} under docs/, {marked} of them "
          f"marking a pin")
    print()
    named = sum(1 for t in rep.tags if t == ("pin", "named"))
    status(not rep.count("pin"), "Teensy pins",
           f"{good} of {len(refs)} marked pins match the table, "
           f"{named} named without the mark")
    status(not rep.count("table"), "what the pins can carry",
           f"{rows - rep.count('table')} of {rows} rows match the board's model")
    status(not rep.count("cost"), "what the allocation costs",
           f"{costs} entries match what the pins take" if not rep.count("cost") else
           f"{rep.count('cost')} entr{'y' if rep.count('cost') == 1 else 'ies'} disagree"
           f"{'s' if rep.count('cost') == 1 else ''} with what the pins take")
    if rep.errors:
        listing("To fix", rep.errors)
        print()
        n = len(rep.errors)
        print(f"{n} problem{'' if n == 1 else 's'}. Nothing was changed: move the pin "
              f"in the table, or run --write to carry the table's pin into the files and "
              f"the board's model into the table.")
        return 1
    print()
    print("Nothing to fix. Every marked pin is the one the table gives its signal.")
    return 0
