# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Rules

[`RULES.md`](RULES.md) is the working agreement for this repository and takes precedence over anything below. Read it before acting. Its load-bearing points:

- **Never push.** Commit only when explicitly told, or the conditions of `RULES.md` allow it.
- **Do not create or modify design artifacts** — schematics, PCB, CAD, firmware, BOM — without being asked. Documentation is the exception and is kept in sync automatically once a matter is settled.
- **Assume a software-engineering background, not an electrical one.** Explain the failure mode behind each choice, cite datasheet sources for electrical values, and never present an estimate as a measured fact.
- **Re-evaluation means re-deriving from sources**, not restating an earlier answer.
- **Rule 10 bans a specific writing tic**: meta-commentary about what a document is or is not, and antithesis used for rhythm. Check prose against it before writing a file.
- **Search with the model number** — "Teensy 4.1", never "Teensy". Rule 11 gives the source ranking; pjrc.com outranks the forum, where only Paul Stoffregen is authoritative.

@RULES.md

## Project

Modification of the Robotime ROKR Pinball Machine (EG01) wooden 3D-puzzle kit with custom electronics. This repository hosts everything for the mod: firmware/code, custom PCB designs (schematics + layout), 3D CAD files, the part list (BOM), research notes, documentation, and media assets.

No firmware, board design or CAD model exists yet. The first subsystem — IR ball sensing — is designed and documented in `docs/parts/ir-reflective/`.

## Toolchain

| Area | Tool | Status |
|---|---|---|
| Microcontroller | Teensy 4.1 (NXP i.MX RT1062) | Chosen |
| Schematic + PCB | EasyEDA, working from local project files | Chosen — **Standard vs. Pro not yet recorded**, and they use different file formats |
| 3D CAD | FreeCAD **or** Fusion | **Undecided** — do not assume one |
| Firmware build | Not chosen (Arduino IDE / PlatformIO / `teensy_loader_cli`) | Open |

**The Teensy 4.1 runs 3.3 V logic and its pins are not 5 V tolerant** — PJRC states plainly that no digital or analog pin may be driven above 3.3 V. Anything interfacing with 5 V needs level shifting. Flag this on every design that touches a Teensy pin; it is the most likely way to destroy the board.

**Per-pin output current is 4 mA, not 10 mA.** PJRC's [4.1 product page](https://www.pjrc.com/store/teensy41.html) gives 4 mA as the recommended maximum; the [comparison table](https://www.pjrc.com/teensy/techspecs.html) says 10 mA because that grid spans every generation. Design against 4 mA — see [`docs/research/teensy-4.1.md`](docs/research/teensy-4.1.md).

## Repository layout

- `firmware/` — embedded software for the Teensy 4.1
- `hardware/pcb/` — EasyEDA projects, one directory per board, each with a schematic PDF and a `fab/` directory of manufacturing outputs
- `hardware/cad/` — 3D models: source model plus a neutral export (STEP for mating parts, STL/3MF for printed ones)
- `docs/` — documentation of the mod
- `docs/parts/` — the design itself, one directory per subsystem, each holding its circuit document and schematic SVGs
- `docs/research/` — investigation of the stock machine and per-component notes
- `docs/datasheets/` — manufacturer datasheets, referenced from the research and design documents
- `docs/parts-list.md` — bill of materials
- `docs/pin-assignment.md` — Teensy pins spent, and what each one locks out
- `assets/` — photos, renders, and other media
- `.claude/skills/` — repository-local skills for schematic work (see below)

Sub-directory `README.md` files describe their contents **for a repository visitor**. Maintenance conventions belong here or in `RULES.md`, never in those files.

## Commands

None — no build system exists yet. Record build/flash/test commands here once the firmware toolchain is chosen.

## Conventions

- Update `docs/parts-list.md` whenever a hardware design change adds or removes a component (subject to rule 8 — the BOM is a design artifact, so ask first). Keep it to identification plus a spec link; specs and rationale go in `docs/research/`, one file per component. Every BOM row must resolve to its specification — local notes where they exist, manufacturer datasheet otherwise.
- **Check [`docs/pin-assignment.md`](docs/pin-assignment.md) before committing any Teensy pin, and add a row there once a design settles on one.** It is the single record of which pins this build has spent, and the only way a conflict gets caught across sessions. Its shared-resource tables are what a candidate pin must be checked against — taking one pin of a bus or serial port commits the whole group. Keep it to occupation: which pin, which signal, which peripheral, which subsystem. Rationale for a choice belongs in the subsystem document, and a re-allocation is discussed in chat and then written straight into the table.
- Reference designators follow IEEE 315: `A` for a separable sub-assembly such as a plug-in module, `U` for an inseparable one such as a bare IC.
- EasyEDA project files live locally and are committed directly; there is no cloud round-trip. Which format applies depends on the variant: **Standard** uses EasyEDA Source JSON, **Pro**'s desktop client creates `.eprj` for offline projects, with `.epro`/`.epro2` as its exported project archives ([Pro client FAQ](https://prodocs.easyeda.com/en/faq/client/)).
- EasyEDA Pro format generations:

  | Format | Storage | Availability | Consequence |
  |---|---|---|---|
  | `.epro2` | ZIP archive: `project.json` plus `SHEET/`, `PCB/`, `SYMBOL/`, `FOOTPRINT/`, `INSTANCE/`, `BLOB/`, `POUR/`, entries named by UUID | Shipping since V3.2.149 — File → Save As → Save Project to Local | No git diff, but unzips with standard tools and parses |
  | `.eprj` / `.eprj2` | Single SQLite3 database, used by offline projects | Current | No git diff; readable only via `sqlite3` |
  | `.eprj3` | Directory of line-based JSON records | **Documented but not shipping** — absent from the changelog through V3.2.149 | Would diff in git. Switch to it when it appears |

  Format specs: <https://github.com/easyeda/easyeda-pro-file-format-v2> (current), <https://github.com/easyeda/easyeda-pro-eprj3-format> (future). Re-check the [changelog](https://pro.easyeda.com/page/update-record) before assuming `.eprj3` is still unavailable.
- **Commit the `.epro2` as the project of record, and export a netlist, BOM CSV and schematic PDF alongside it.** The archive holds the design but shows no diff; the exports are the reviewable and machine-checkable layer.
- FreeCAD `.FCStd`, Fusion `.f3d`/`.f3z`, STL and 3MF are opaque binaries. STEP is text but very large. `.gitattributes` classifies these; Git LFS has **not** been enabled — raise it before a large volume of binaries lands in history, since removing them later means rewriting history.
- Cite the source for electrical figures. PJRC's published Teensy 4.1 numbers live at <https://www.pjrc.com/store/teensy41.html> and <https://www.pjrc.com/teensy/techspecs.html>.

## Skills

Repository-local, in `.claude/skills/`, so they version with the project.

| Skill | Fires when |
|---|---|
| `review-circuit` | A circuit is about to be built or committed. Loads constraints from `docs/research/`, `docs/parts/`, `docs/datasheets/`, `docs/pin-assignment.md` and `docs/parts-list.md` at run time rather than hardcoding them |
| `draw-schematic` | A circuit documented in markdown needs an SVG diagram. Encodes the house style — net colours, module blocks, pin markers |
| `review-schematic-svg` | A drawn SVG needs checking against its source document and for read-ambiguity |
| `verify-easyeda` | An EasyEDA board changed. Diffs the Allegro `.tel` netlist against a baseline |
| `design-review` | A design is correct and now has to be *good* — derating, thermal, schematic craft, testability, failure behaviour. Operationalises rule 3 |

`review-circuit` asks whether the circuit survives, `design-review` whether it is good, `verify-easyeda` whether the board matches the intent. None substitutes for another.
