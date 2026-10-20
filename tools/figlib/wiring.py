"""The wiring, the peripherals and the shared budgets, checked across every model at once."""
from __future__ import annotations

from collections import defaultdict

from .model import TEENSY_RAIL, Part, PinData
from .pins import PIN_TABLE, PinRow, pin_rows
from .quantities import Q
from .repo import load_model, pin_data, rel
from .report import Report, listing, status


def _function(peripheral: str, pin: int, data: PinData):
    """The port or timer a Peripheral entry names, its signal or channel, and whether the pin carries it."""
    for t, pins in data.timers.items():
        if peripheral.startswith(t + " channel "):
            ch = peripheral[len(t) + len(" channel "):]
            return t, ch, dict(pins).get(pin) == ch
    for f, pins in data.flexio.items():
        if peripheral.startswith(f + " pin "):
            n = peripheral[len(f) + len(" pin "):]
            return f, n, dict(pins).get(pin) == n
    for port in sorted(data.ports, key=len, reverse=True):
        if peripheral.startswith(port + " "):
            signal = peripheral[len(port) + 1:]
            return port, signal, pin in dict(data.ports[port]).get(signal, ())
    return None, None, peripheral in ("plain digital input", "plain digital output")


def teensy_part(row: PinRow, data: PinData) -> Part:
    """The Teensy pin itself, driving its net the way the function its row gives it does."""
    used, signal, _ = _function(row.peripheral, row.pin, data)
    kind = (data.kind(signal) if used in data.ports else
            "input" if row.peripheral == "plain digital input" else "push-pull")
    return Part(f"Teensy pin {row.pin}", kind, TEENSY_RAIL if kind == "push-pull" else None,
                src=f"{rel(PIN_TABLE)}:{row.line}")


def pass_wiring(models, rep: Report) -> int:
    """One driver at a time on every net, a defined level, and the Teensy's rail on its pins."""
    rows, data = {r.signal: r for r in pin_rows()}, pin_data()
    merged = defaultdict(list)
    for m in models:
        for n in m.nets:
            merged[("teensy", n.teensy) if n.teensy else ("local", m.name, n.name)].append(n)
    for key, group in merged.items():
        parts = [p for n in group for p in n.parts]
        label = f"{group[0].name} in {', '.join(sorted({n.model for n in group}))}"
        if key[0] == "teensy":
            row = rows.get(key[1])
            if row is None:
                rep.error(f"{label} reaches the Teensy signal {key[1]!r}, which "
                          f"{rel(PIN_TABLE)} does not list", kind="pins")
                continue
            label += f", pin {row.pin}"
            for n in group:
                if n.model != row.subsystem:
                    rep.error(f"{label}: {n.model} wires to a pin {rel(PIN_TABLE)}:{row.line} "
                              f"gives to {row.subsystem}", kind="pins")
            parts = [teensy_part(row, data)] + parts
            for p in parts:
                powered = (p.role in ("push-pull", "tri-state")
                           or (p.role in ("pull", "contact") and p.rail != "gnd"))
                if powered and p.rail != TEENSY_RAIL:
                    rep.error(f"{label}: {p.name} runs from {p.rail}, and only the Teensy's own "
                              f"3.3 V may reach a Teensy pin", kind="rail")
        always = [p for p in parts if p.role == "push-pull"]
        others = [p for p in parts if p.role in ("tri-state", "open-drain", "contact")]
        if len(always) > 1 or (always and others):
            rest = ", ".join(p.name for p in always[1:] + others)
            rep.error(f"{label}: {always[0].name} drives the line whenever it is powered, "
                      f"and {rest} drives it as well", kind="contention")
        selects = [p.select for p in parts if p.role == "tri-state"]
        if len(set(selects)) < len(selects):
            rep.error(f"{label}: two tri-state outputs share one select, so both answer at once",
                      kind="contention")
        if not always and not any(p.role == "pull" for p in parts):
            rep.error(f"{label}: nothing holds the line while no output drives it, so it needs "
                      f"a pull-up or a pull-down", kind="level")
    wired = set(merged)
    for r in rows.values():
        if (r.subsystem in {m.name for m in models} and not r.reserved
                and ("teensy", r.signal) not in wired):
            rep.error(f"{rel(PIN_TABLE)}:{r.line} gives pin {r.pin} ({r.signal}) to "
                      f"{r.subsystem}, whose model wires no net to it", kind="pins")
    return len(merged)


def pass_peripherals(models, rep: Report) -> int:
    """Every pin function is one the board's model gives that pin, and an owned peripheral has one user."""
    data = pin_data()
    owner = {}
    for m in models:
        for p, why in m.owned:
            if p in owner and owner[p][0] != m.name:
                rep.error(f"{p} is owned by {owner[p][0]} and by {m.name}", kind="owner")
            owner.setdefault(p, (m.name, why))
    for r in pin_rows():
        where = f"{rel(PIN_TABLE)}:{r.line}"
        used, what, ok = _function(r.peripheral, r.pin, data)
        if used is None and not ok:
            rep.error(f"{where} names {r.peripheral!r}, which is no port signal, no timer "
                      f"channel and no plain digital pin", kind="function")
            continue
        if not ok:
            rep.error(f"{where} puts {r.peripheral} on pin {r.pin}, which "
                      f"{'that channel' if used in data.timers else 'that signal'} of {used} "
                      f"does not reach", kind="function")
        if used in owner and owner[used][0] != r.subsystem:
            rep.error(f"{where} gives {used} on pin {r.pin} to {r.subsystem}, and "
                      f"{owner[used][0]} owns it: {owner[used][1]}", kind="owner")
    return len(owner)


# how the report names a pool that carries a wiring rail's name
POOL_NAMES = {TEENSY_RAIL: "the Teensy's 3V3 pin"}


def pass_budget(models, rep: Report) -> list:
    """Every pool against the one figure that supplies it, with the draws of every model added up."""
    supply, demand = {}, defaultdict(list)
    for m in models:
        for key, pool in m.supplied:
            if pool in supply:
                rep.error(f"{POOL_NAMES.get(pool, pool)} is supplied by {supply[pool][0].name} "
                          f"and by {m.name}", kind="budget")
                continue
            supply[pool] = (m, key)
        for key, pool in m.drawn:
            demand[pool].append((m, key))
    out = []
    for pool, users in demand.items():
        name = POOL_NAMES.get(pool, pool)
        if pool not in supply:
            rep.error(f"{', '.join(sorted({m.name for m, _ in users}))} draw from {name}, which "
                      f"no model supplies", kind="budget")
            continue
        holder, key = supply[pool]
        held = holder.figs[key]
        total = Q.of(0, held.unit)
        for m, k in users:
            total = total + (m.figs[k].value if k else Q.of(1, ""))
        if not total <= held.value:
            rep.error(f"the subsystems take {_amount(total, held.unit)} together from {name} in "
                      f"the machine, which holds {_amount(held.value, held.unit)}" if held.unit
                      else f"{_amount(total, '')} drivers take a slot of the {name}, which holds "
                      f"{_amount(held.value, '')}", kind="budget")
        out.append((name, total, held))
    return out


def _amount(q: Q, unit: str) -> str:
    return f"{q.to(unit):.4g} {unit}".strip()


def run_wiring(paths) -> int:
    models = [load_model(p) for p in paths]
    for m in models:
        m.evaluate()
    rep = Report()
    nets = pass_wiring(models, rep)
    owned = pass_peripherals(models, rep)
    budget = pass_budget(models, rep)
    print("Checking the wiring and what the subsystems share")
    print(f"  models     {len(models)}, {nets} nets, {owned} owned peripherals and "
          f"{len(budget)} shared budgets")
    print()
    for kind, label, what in (
            ("contention", "one driver at a time", "nets driven by two outputs at once"),
            ("level", "a defined level", "nets left floating while nothing drives them"),
            ("rail", "the Teensy's rail on its pins",
             "parts on a Teensy pin running from another rail"),
            ("pins", "the pins the table gives",
             "nets or allocated pins that disagree with the table"),
            ("function", "pin functions", "functions a pin cannot carry"),
            ("owner", "owned peripherals", "used by a subsystem that does not own them")):
        status(not rep.count(kind), label, f"{rep.count(kind)} {what}")
    for name, total, held in budget:
        status(total <= held.value, name,
               f"{_amount(total, held.unit)} of {_amount(held.value, held.unit)} in the machine"
               if held.unit else f"{_amount(total, '')} of {_amount(held.value, '')} slots taken")
    if rep.errors:
        listing("To fix", rep.errors)
        return 1
    return 0
