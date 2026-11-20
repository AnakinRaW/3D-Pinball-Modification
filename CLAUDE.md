# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Safety

These come before every other consideration, in every design, document and bench run.

- **The Teensy 4.1's pins are not 5 V tolerant.** PJRC states that no digital or analog pin may be driven above 3.3 V. Anything that meets 5 V is level shifted. Every design that touches a Teensy pin flags this, since it is the most likely way to destroy the board.
- **A Teensy 4.1 pin carries 4 mA at most.** PJRC's [4.1 product page](https://www.pjrc.com/store/teensy41.html) gives 4 mA as the recommended maximum. The [comparison table](https://www.pjrc.com/teensy/techspecs.html) says 10 mA because it spans every generation. Design against 4 mA, as [`docs/research/teensy-4.1.md`](docs/research/teensy-4.1.md) sets out.
- **The build stays within SELV**, at USB 5 V or a low-voltage DC supply of 24 V at most (rule 4). Mains needs an explicit decision and a qualified electrician.
- **A bench program lights at most ten LEDs**, unless a larger number is asked for, and configures the lighting driver with only those LEDs. A whole chain at full white trips the bench supply. Before any flash or test run that lights LEDs, the chat names which LEDs light, how bright, for how long and what should show.
- **A substitute part is usable only once its datasheet is checked against everything the current part satisfies**: supply voltage, its current against the budget it draws from (`figcheck --graph` gives the other consumers), clock or timing, package and pinout. The verdict includes that check.
- **An electrical value carries its source**, a datasheet page, a standard or a measurement. An estimate is stated as one (rule 2), since a fabricated number will not be caught here.

## Rules

[`RULES.md`](RULES.md) is the working agreement for this repository and takes precedence over anything below. These are its points that sessions have broken most often:

- **Never push.** Commit only when told, and only the named files after `git diff --cached` (rule 5).
- **Design artifacts change only on an explicit instruction** (rule 8). They are schematics, PCB, CAD, firmware with the bench programs, and the BOM. A question such as "why not X?" gets an answer and no edit. A quick bench demo is built from a copy in the scratchpad. Documentation is the exception and follows a settled matter without being asked (rule 7).
- **A settled change is swept in one pass** (rules 3 and 7). Grep the old and the new number across `docs/`, `firmware/` and `model/`. A settled fact has no number to grep, so every derivation that took it as an input is re-run. Findings arrive together, never one per session.
- **A settled fact loses its caveat**, and an open figure that nothing depends on gets no measurement (rule 3). A cheap part that makes a measurement unnecessary is the answer.
- **The modification is reversible** (rule 12). No stock board is re-soldered and no part is cut or drilled past its original purpose, so a stock board is reached through its connectors or swapped out whole.
- **The build is through-hole** (rule 17). A part that exists only as SMD sits on an adapter.
- **A software developer reads this** (rule 2). Name the failure mode behind each choice, and explain a quantity's idea before its number.
- **Re-evaluation re-derives from the sources** (rule 9). Rewriting a passage means reading its sources again, never restoring sentences from the conversation.
- **Rule 14 grades every finding.** Critical and Important are reported at once, in plain words first, with an alternative. A Critical finding waits until its derivation is finished, and a review is completed before anything is reported.
- **A derived figure lives in a model** (rule 15). Each subsystem's `model/parts/<subsystem>/figures.py` computes it, `--write` puts it into the files, and `tools/figcheck.py` fails a document or a drawing that disagrees. A figure is never typed into a document by hand.
- **Rule 10 governs every sentence written**, in documents, code comments and chat. Check a draft against its bans before writing a file. "Its own" and "on its own" are never written.
- **Search with the model number**, such as "Teensy 4.1" and never "Teensy" (rule 11). pjrc.com outranks the forum, where only Paul Stoffregen is authoritative.
- **A rule that binds the work goes into the CLAUDE.md beside what it governs**, such as [`firmware/CLAUDE.md`](firmware/CLAUDE.md). `docs/` holds the design. A sub-directory's `README.md` describes its folder for a visitor.

@RULES.md

## Project

Modification of the Robotime ROKR Pinball Machine (EG01) wooden 3D-puzzle kit with custom electronics. This repository hosts everything for the mod: firmware, custom PCB designs, 3D CAD files, the part list (BOM), research notes, documentation and media.

The firmware is a skeleton in `firmware/src/` that starts FreeRTOS and builds with every library the design names. Its code stands in `firmware/src/`: the clock, the event queue and the device monitor in `core/`, the driver tick, the lighting and the servo in `drivers/`, the lamps in `components/`. No board design or CAD model exists yet. IR ball sensing, the break beam at the drain and the solenoids are designed and documented under `docs/parts/`, one directory each. The lighting design is in progress there.

## Toolchain

| Area | Tool | Status |
|---|---|---|
| Microcontroller | Teensy 4.1 (NXP i.MX RT1062) | Chosen |
| Schematic and netlist | Declared pin by pin in the subsystem's `schematic.py` under `model/`. KiCad is opt-in: `tools/netlist.py` writes a board out as a KiCad netlist and compares one KiCad exported | Chosen |
| 3D CAD | FreeCAD **or** Fusion | **Undecided**, assume neither |
| Firmware build | PlatformIO, uploading through PJRC's Teensy Loader with `upload_protocol = teensy-gui`, since `teensy-cli` fails to write on this machine. The linker script [`firmware/linker/teensy41.ld`](firmware/linker/teensy41.ld) keeps LVGL's code out of RAM1, which the Arduino IDE cannot do per project | Chosen |
| Teensy core | The newest Teensyduino release, as PlatformIO's `framework-arduinoteensy` package (1.162.0 for Teensyduino 1.62), pinned in each `platformio.ini` | Chosen |
| LED library | FastLED 3.10.6 or newer from PlatformIO's registry, for the colors, the palettes and the power model. Teensyduino 1.62 ships FastLED 3.4.0, which has no `setPowerModel`. The chains go out from FlexIO2 through `firmware/src/drivers/ws2812-flexio2.h`, since ObjectFLED's DMA writes to the pins stretched bits whenever the processor loaded the peripheral bus | Chosen |
| UI library | LVGL 9.6, its code in the flash and its drawing loops in RAM1, as [`firmware/gist/lv_conf.h`](firmware/gist/lv_conf.h) sets. The screen component in [`docs/firmware/components/screen.md`](docs/firmware/components/screen.md) runs it | Chosen |
| RTOS | FreeRTOS 11.2 from freertos-teensy 11.2.0-4 with the port's pull request 43 and four changed settings, which [`firmware/include/freertos_config_override.h`](firmware/include/freertos_config_override.h) holds. The tasks are in [`docs/firmware/general-design.md`](docs/firmware/general-design.md#execution-model) | Chosen |

## Repository layout

- `firmware/`: the firmware for the Teensy 4.1, with its code in `src/`, the shared settings headers in `include/`, the tests in `tests/`, the measurement programs in `bench/` and code notes in `gist/`. [`firmware/CLAUDE.md`](firmware/CLAUDE.md) holds the rules its code follows
- `firmware/src/`: the firmware's code, in `core/`, `drivers/`, `components/` and `game/`, which the test image, the computer tests and the bench programs build from there, as [`firmware/CLAUDE.md`](firmware/CLAUDE.md#layout) gives
- `firmware/tests/`: one program per section of [`docs/firmware/testing.md`](docs/firmware/testing.md), the computer tests in `test_<program>` and every Teensy test in the one test image `teensy/`
- `hardware/pcb/`: a printed circuit board, one directory each, its netlist taken from the subsystem's model and its manufacturing outputs in `fab/`
- `hardware/cad/`: 3D models, a source model plus a neutral export (STEP for mating parts, STL or 3MF for printed ones)
- `docs/firmware/`: the firmware's design, one document per driver and component
- `docs/parts/`: the design, one directory per subsystem with its circuit document and schematic SVGs
- `docs/research/`: the stock machine and one note per component. `docs/datasheets/` holds the manufacturer datasheets they cite
- `docs/parts-list.md` is the bill of materials, and `docs/pin-assignment.md` the Teensy pins spent and what each locks out
- `model/`: the Python files the checks read, in folders that mirror those under `docs/`. A subsystem's `figures.py`, `schematic.py` and perfboard layouts sit there, the Teensy's figures in `model/research/teensy-4.1.py` and the firmware's in `model/firmware/figures.py`
- `tools/perfboard/`: the perfboard inventory and the SVG library that draws its parts as they look. A layout takes sizes and pinouts from here, never from memory
- `tools/simulator/`: the browser simulator, a TypeScript app with Vite and Preact. It shows the drawing of `assets/playfield/` with the features and a card for every part a click selects, and its Studio runs the firmware's lamps in WebAssembly. Its machine config lies in `tools/simulator/config/`
- `firmware/sim/`: the simulator's WebAssembly build, CMake with Emscripten, of the lamps component and the lighting driver from `firmware/src`, with a simulated output stage in `fakes/` in place of the FlexIO2 one
- `assets/`: photos, renders and other media. `assets/playfield/` holds the vector drawing of the machine, one file per part plus `playfield.json`, which places them. [`assets/playfield/CLAUDE.md`](assets/playfield/CLAUDE.md) holds its rules
- `.claude/skills/`: the repository's skills, listed below

## Commands

| | |
|---|---|
| `python tools/figcheck.py --sheets` | Recomputes every figure of every model, checks the documents and the SVGs against them, and checks every marked Teensy pin against `docs/pin-assignment.md`, the wiring and the shared budgets across all models. A model path such as `model/parts/solenoid/figures.py` restricts the run to that subsystem and the pins its files mark, read from the root of the checkout whose `figcheck.py` runs. Every directory under `model/parts/` holds a model. The motherboard model derives nothing and takes the values of its parts from their subsystems. The lighting model governs only the Level shifting, Signal wiring and Supply sections of its document, plus the lamps component's document. A figure is declared in one model and taken by every other with `MODEL.uses(key, of=...)`. Run it after any change to a value. CI runs it on each push touching `docs/`, `firmware/` or `tools/`. [`tools/CLAUDE.md`](tools/CLAUDE.md) gives what a model declares and the twenty-nine checks, [`tools/README.md`](tools/README.md) the flags |
| `… --groups` | Dumps the groups, sections and tokens the parser found. A block's or a declaration's `group` and `section` are taken from here, since a guess wastes a run |
| `… --graph <key>` | One quantity in full, which is how a derivation is read back without opening the model |
| `… --write` | Writes the model's figures into the documents and the SVGs. [`tools/CLAUDE.md`](tools/CLAUDE.md#writing-the-figures) gives what it does where it cannot place a figure |
| `… --mutate` | The self-test: every figure is moved, and the run has to report it |
| `… --base <ref>` | The ref the stale-number report diffs against and `--write` takes old figures from. CI passes the pull request's base |
| `… --blind` | The brief for an independent second derivation, which rule 15 keeps for larger reviews |
| `python tools/svgcheck.py` | Reports labels in the drawings under `docs/` that leave the viewBox or their box, or land on another label, and boxes that cross an outline. **Run it after moving or adding any text or box in an SVG.** It stays out of CI, since its widths are estimated from character classes, so a report is a place to look |
| `python tools/perfboard/build.py <layout.py> [out.svg]` | Checks a perfboard layout against [`tools/perfboard/CLAUDE.md`](tools/perfboard/CLAUDE.md), the netlist and the distance limits included, and draws it once no error is left |
| `python tools/netlist.py model/parts/<subsystem>/figures.py [board]` | Prints the netlist a model declares for each board. `--kicad <out.net>` writes one board as a KiCad netlist, and `--diff <kicad.net>` compares a netlist KiCad exported with the declaration |
| `python tools/playfield/compose.py` | Writes `assets/playfield/playfield.svg` from the part files and `playfield.json`, and `features.svg`, the same drawing with the README's numbered feature bubbles. `--check` exits with 1 when a drawing differs from what they compose |
| `python tools/picture.py <png> ...` | Turns PNGs of at most 320 × 240 pixels into picture files for the screen component, in LVGL's image format and RGB565, dithered after Floyd–Steinberg. A picture with transparent pixels keeps its opacity. [`assets/media.md`](assets/media.md) gives how pictures, videos and sounds are made |
| `npm install`, then `npm run dev` in `tools/simulator/` | Installs the simulator's pinned dependencies and starts it on http://localhost:5173, where it reloads on every change to its code, the drawing or the feature texts. It needs Node.js 22 or newer |
| `npm run wasm` in `tools/simulator/` | Builds the firmware's lamps component and lighting driver from `firmware/sim` as a WebAssembly module for the simulator's Studio. It needs Emscripten at the version `firmware/sim/emsdk-version` names, in `%LOCALAPPDATA%\emsdk`, and CMake and Ninja, here from Python's `cmake` and `ninja` packages |
| `npm test` in `tools/simulator/` | Runs the simulator's Vitest tests, which hold its machine config and its part map to the drawing and run the firmware's lamps in WebAssembly, skipped without `npm run wasm` |
| `npm run build` in `tools/simulator/` | Checks the simulator's types and writes the app to `tools/simulator/dist/` |

The firmware builds with PlatformIO, installed with `python -m pip install platformio`. Its [`platformio.ini`](platformio.ini) stands in the repository's root, where the commands run. CI builds the release firmware, checks its code with clang-tidy, runs the `computer` tests and checks the tests against testing.md on each push touching `firmware/` or testing.md:

| | |
|---|---|
| `python -m platformio run` | Builds the project in the current folder and prints how much of RAM1, RAM2, the flash and the PSRAM it takes |
| `python -m platformio run -t upload` | Builds and flashes it onto the Teensy. A serial monitor holding the Teensy's port blocks the upload |
| `python firmware/tools/teensy-tests.py` | Runs every `Teensy` test of [`docs/firmware/testing.md`](docs/firmware/testing.md) on the test image in `firmware/tests/teensy`, quick, and exits with 1 on a failure, a hang or a crash. `--upload` builds and flashes the image first, a pattern such as `"servo.*"` picks tests, `--soak` takes the durations testing.md states and `--bench` adds the bench tests for a person at the bench. The image takes its tests from `src/<program>`, one folder per section, and `src/main.cpp` lists the programs. A test reports through `test-harness.h`. The Results section of testing.md holds the lines and commands between the runner and the image |
| `python firmware/tools/tidy.py` | Runs clang-tidy with the checks of `firmware/.clang-tidy` over `firmware/src`, on the release build's compile commands turned into ones clang reads, and exits with 1 on any finding in `firmware/`. A pattern such as `servo` picks files, `--fix` applies the fixes clang-tidy offers, and `--self-test` requires a clean planted file to pass and a flawed one to fail. It needs `python -m pip install clang-tidy==18.1.8` |
| `python firmware/tools/test-coverage.py` | Counts for each program of testing.md the rows the code has a test for, by place, and writes the table to the summary page of a CI run. `--missing` names each row without a test, and `--computer-results <json>` counts the computer tests from the report of `pio test -e computer --json-output-path <json>`, as CI does. It exits with 1 when the code and testing.md disagree. `--self-test` breaks a copy each way the check guards and requires each verdict. The Test programs section of testing.md gives how a test's name maps to its row and what fails the run |

The root project, the test image and every program in `firmware/bench/` keep their build folders and one `build_cache_dir` under `%LOCALAPPDATA%\pio`. The cache serves an object file only to the project that compiled it, since it finds the file by its command line, its source and its place in the build folder. Worktrees share each project's build folder, so a build after another worktree's build takes back the files it compiled before. `firmware/tools/quiet-libraries.py` gives the firmware's code `-Wall -Wextra -Werror` and every other library `-w`.

FastLED 3.10.9 does not build for PlatformIO's `native` platform. The MP3 decoder it bundles, `src/third_party/minimp3/minimp3.h`, uses `uint64_t` without including `<stdint.h>`, and the computer's gcc rejects that. The `computer` tests therefore take FastLED's colors, palettes and curves from `firmware/tests/fakes/FastLED.h` and the lighting driver from `firmware/tests/fakes/drivers/lighting.h`. `build_src_filter` of the `computer` environment leaves the real lighting driver out.

## Conventions

- **Once a subsystem's code runs on the Teensy, the code holds how it works.** Its document then keeps what changes rarely: its purpose and requirements, the decisions with their reasons, its interface, the derived figures with their sources, and its known limitations. The implementation moves into `firmware/`, one subsystem at a time as each comes up on hardware.
- **A document links the header once production code holds its interface complete.** A driver or component document under `docs/firmware/` then copies no interface into a code block. It links the `.h` file instead.
- **`docs/parts-list.md` changes with every hardware change that adds or removes a part**, after asking, since the BOM is a design artifact. A row holds identification and a spec link that resolves to the part's note in `docs/research/` or to its datasheet.
- **A Teensy pin is checked against [`docs/pin-assignment.md`](docs/pin-assignment.md) before it is committed, and gets a row there once a design settles on it.** The file is the one record of the pins this build has spent. A candidate pin is checked against its shared-resource tables, since one pin of a bus or a serial port commits the whole group. `--write` writes those tables from `model/research/teensy-4.1.py`, and figcheck fails a cost table that the allocation has left stale. The file holds occupation only: pin, signal, peripheral, subsystem. The reason for a choice goes into the subsystem document, and a re-allocation is agreed in chat and then written into the table. Every other mention of a Teensy pin is marked with the signal it carries, as [`tools/CLAUDE.md`](tools/CLAUDE.md#teensy-pins) gives, and `--write` carries a moved pin into every mark. A subsystem's pins sit together on one edge of the board, in one run where its peripherals allow.
- **Reference designators follow IEEE 315**: `A` for a separable sub-assembly such as a plug-in module, `U` for an inseparable one such as a bare IC.
- **A board's circuit is declared pin by pin in `model/parts/<subsystem>/schematic.py`**, with its parts and nets, and `figures.py` loads it with `MODEL.schematic("schematic.py")`. A part's value names the model's figure that sets it. The nets that reach a Teensy pin enter the wiring check from there, so such a subsystem declares no `MODEL.net`. The perfboard layouts take their nets from there, and figcheck holds the netlist to the part list, the connector tables and the annotated drawings. [`tools/CLAUDE.md`](tools/CLAUDE.md#boards) gives the format. Nothing in the repository needs KiCad installed.
- **CAD files are opaque binaries**: FreeCAD `.FCStd`, Fusion `.f3d` and `.f3z`, STL and 3MF. STEP is text but very large. `.gitattributes` classifies them. Git LFS is **not** enabled, and it is raised before a large volume of binaries lands in history, since removing them later rewrites history.

## Skills

The repository's skills live in `.claude/skills/`, so they version with the project.

| Skill | Fires when |
|---|---|
| `review-circuit` | A circuit is about to be built or committed. It loads its constraints from `docs/research/`, `docs/parts/`, `docs/datasheets/`, `docs/pin-assignment.md` and `docs/parts-list.md` at run time |
| `draw-schematic` | A circuit documented in markdown needs an SVG diagram in the house style: net colors, module blocks, pin markers |
| `review-schematic-svg` | A drawn SVG needs checking against its source document and for read-ambiguity |
| `revise-document` | A document has to be shortened or made readable: structure first, then paragraphs, then sentences, with the deletions accounted for against the diff |
| `design-review` | A design is correct and now has to be good: derating, thermal, schematic craft, testability, failure behavior (rule 3) |

`review-circuit` asks whether the circuit survives, `design-review` whether it is good, and each needs the other.
