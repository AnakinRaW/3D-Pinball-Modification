# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Rules

[`RULES.md`](RULES.md) is the working agreement for this repository and takes precedence over anything below. Read it before acting. Its load-bearing points:

- **Never push.** Commit only when explicitly told, or when updating `RULES.md` itself.
- **Do not create or modify design artifacts** — schematics, PCB, CAD, firmware, BOM — without being asked. Documentation is the exception and is kept in sync automatically once a matter is settled.
- **The maintainer is a software developer, not an electrician.** Explain the failure mode behind each choice, cite datasheet sources for electrical values, and never present an estimate as a measured fact.
- **Re-evaluation means re-deriving from sources**, not restating an earlier answer.

@RULES.md

## Project

Modification of the Robotime ROKR Pinball Machine (EG01) wooden 3D-puzzle kit with custom electronics. This repository hosts everything for the mod: firmware/code, custom PCB designs (schematics + layout), 3D CAD files, the part list (BOM), research notes, documentation, and media assets.

The project is in its initial phase: no firmware or board designs are committed yet and no toolchains have been chosen.

## Repository layout

- `firmware/` — embedded software for the mod (toolchain TBD)
- `hardware/pcb/` — EDA projects: schematics and board layouts; exported schematic PDFs and fabrication outputs live next to the sources
- `hardware/cad/` — 3D CAD files: PCB 3D models, mounts, 3D-printable parts; keep a neutral export (STEP/STL) alongside native sources
- `docs/` — documentation of the mod
- `docs/research/` — research notes, datasheets, measurements of the stock machine
- `docs/parts-list.md` — bill of materials for the mod
- `assets/` — photos, renders, and other media

## Commands

None yet — there is no build system, firmware project, or EDA toolchain in the repo. When one is added (e.g. PlatformIO / Arduino / ESP-IDF for firmware, KiCad for the PCB), record the build/flash/test commands here.

## Conventions

- Update `docs/parts-list.md` whenever a hardware design change adds or removes components.
- Prefer text-based file formats when the tool offers them (e.g. KiCad's s-expression formats); for large opaque binaries (CAD, renders), consider Git LFS before they land in history.
