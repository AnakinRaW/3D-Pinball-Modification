# Simulator

A browser tool that shows the machine as the drawing in [`assets/playfield/`](../../assets/playfield/) and runs the firmware's game code behind it. Everything the machine shows comes from the firmware's code: the lights, the screen with its videos, and the sounds.

| View | For | What it shows |
|---|---|---|
| Features | A player | The features of the mod, one at a time, each on the machine and in a short live demonstration |
| Rules | A player | A game's rules: its modes, what starts each, and how far a running game has come |
| Play | Testing a game | A game reacting to sensors clicked in the drawing, with its state, its screen and its sounds |
| Studio | Building a game | One mode of a game with its lights, its slide and its sounds, the same as on the device |
| Replays | Watching a game | A game recorded on the machine, from its file or live over USB, with its score and its modes |

## How it works

The firmware's code above the drivers is compiled to WebAssembly, a binary format that browsers and Node.js run at close to native speed. [Emscripten](https://emscripten.org) is the compiler that turns C++ into WebAssembly. The core, the components, the game host, the games and LVGL run unchanged. Simulated drivers stand in for the hardware, with the interfaces of the real drivers, the same way the fakes of the `computer` tests do. The browser draws what the simulated drivers receive and feeds them what the user clicks.

The tool is therefore one web page. The firmware's C++ runs inside it as a WebAssembly module, and the page around it is written in TypeScript. It starts with one command on `localhost` and needs no installation beyond the build tools.

```
 browser (TypeScript)                      WebAssembly module (C++)
 ┌──────────────────────────┐   step(dt)   ┌───────────────────────────────────────────┐
 │ drawing, panels, canvas  │ ───────────► │ main loop pass: host, components, games,  │
 │ LED glow, LVGL frame,    │              │ lamps, screen (LVGL), video player, sounds│
 │ Web Audio                │ ◄─────────── │                                           │
 └──────────────────────────┘  LED colors, │ simulated drivers: lighting output,       │
              │                screen      │ display, audio, storage, IR, contacts,    │
              └── inputs ────► pixels,     │ rotary, touch, solenoids, servo, controls │
                  (events)     sounds      └───────────────────────────────────────────┘
```

Time is simulated. The clock of [`core/clock.h`](../../firmware/src/core/clock.h) reads `GPT2_CNT`, which the fake `Arduino.h` of the computer tests already turns into a variable. The simulator sets it before every pass of the main loop. A step advances the time, publishes the events that fall due into the real event queue, and runs one pass of the main loop in the order [general-design](../firmware/general-design.md#main-program) gives. The same events at the same times therefore give the same frames, slides and sounds.

The step runs in three paces. In real time, the browser runs passes of 1 ms simulated time between two animation frames. Fast runs passes without waiting, for tests and for seeking in a replay. Paused runs single passes on request.

## What runs real and what is simulated

| Code | In the simulator |
|---|---|
| Clock, event queue, priority list | Real. The clock reads the simulated time |
| Game host, games, modes, components | Real |
| Lamps component | Real |
| Lighting driver | Real down to its chains, with the adapter profile and every limit on brightness. Only the output stage, ws2812-flexio2, is replaced: the browser reads the six chains after every frame |
| Screen component, LVGL 9.6, video player | Real, with the firmware's [`lv_conf.h`](../../firmware/gist/lv_conf.h). The simulated display driver receives the rectangles LVGL sends and copies them into a 320 × 240 frame. Videos are the same `.rgb` files the card holds |
| Sounds component | Real. The simulated audio driver plays the files through Web Audio and publishes `SOUND_ENDED` when one ends |
| Config and file system components | Real. The simulated storage driver reads and writes an in-memory file system that holds the card's contents |
| IR, break beam, contacts, controls, rotary sensor, touch | Simulated: each publishes the driver events the browser hands it, stamped with their simulated time |
| Solenoids, servo | Simulated: each records its pulls and moves, which the drawing animates |
| FreeRTOS and the storage task | Absent. The simulated storage driver finishes every transfer at once |

The firmware's code stays free of simulator switches. The simulated drivers replace the real headers the way the `computer` environment's fakes do, by include order.

The card's contents come from the card in the computer's reader, or over USB from the Teensy, which sends the files the simulator asks for.

## Views

### Features

The view lists the features of the root README. Selecting one lights its parts on the machine, labels them, and shows what the mod adds over the stock machine, how it works and what each game on the card does with it. "Show me" runs the feature in the simulator with the firmware's code, such as a multiball start with its lights, its slide and its sound. A feature without a game behind it runs a short demonstration of its component, such as the seal turning or the ambient strips running a palette.

The texts come from [`features.md`](../features.md) and from `playfield.json`, which names the parts of every feature. The demonstrations are scenarios of the Play view.

### Rules

The view shows a game's rules as a graph of its modes. Each mode states what starts it, as a list of conditions, and what it scores. A condition shows how far the game has come toward it, such as two of three top lanes lit. Selecting a condition lights the parts where it happens. The progress comes from the game in the Play view, from a replay at the cursor, or stays empty to show the rules alone.

The rules come from the game's code, so they cannot drift apart from what the game does. This needs an addition to the [game abstraction](../firmware/game-abstraction.md#modes): a mode names itself, its conditions and its scores, and counts each condition while it runs. The host's menu on the machine's screen can show the same page.

### Play

Every input of the machine is clickable in the drawing: a lane's IR sensor fires a ball passing, a bell or a target a hit, the seal turns by dragging, the drain and the ball store take a ball, the scoop takes and holds one, and the toggle switch flips. A click publishes the driver events the real driver would, with the timing its document gives. A panel shows the game's phase, its ball, its score, its running modes and the coils it pulled. The machine's screen and its sound channels run beside it. A log lists every driver event and the pinball event a component made of it.

A scenario is a file of timed inputs and the expected outcome, such as a score, a mode that runs, a lamp's color, a slide on top or a track that plays. The view records the inputs clicked into a scenario and plays a scenario back with its checks.

The ball itself is not simulated. A test or a person decides where the ball goes.

### Studio

The view runs one mode of a game alone: it starts the mode, shows what the mode holds on each component, and plays its lights, its slide with its video, and its sounds on a timeline. A game's lights and animations are worked out here, faster than at the bench. The loop runs on the game's code: a change in the mode's C++ rebuilds the module in seconds, and the view starts the mode again to the same moment.

What the studio shows has to be what the device shows. Four things carry that:

| | How |
|---|---|
| The same code | The module is built from the same commit as the firmware. Its build id stands in the top bar and in every replay's header |
| The same settings | The simulator reads the machine's config from the card, chains, LEDs and gaps included |
| The same driver | The lighting driver runs real down to its chains, so the 3 A and the 6 A adapter profiles, the perimeters' every second LED and the brightness limits act as on the device. The view switches the profile |
| A check on the bench | A debug build of the firmware sends a checksum of every lamp frame and of every picture sent to the display over USB, while it runs a scenario. The simulator computes the same checksums for the same scenario, and the studio marks every frame where the two differ |

The colors on the computer's screen approximate the LEDs, which look different in the room. Everything else matches frame for frame.

### Replays

A replay file holds the driver events of one game on the machine, each with the time the driver stamped, and the records the game adds, such as its score and its modes. The view reads the file from the card or receives it over USB, and starts a fresh simulation with the firmware build and the config the header names. It publishes every recorded event at its recorded time. The lamps, the screen and the sounds follow from the firmware's code.

The view shows the score, the ball, the phase and the running modes at the cursor, with the progress toward the modes that have not started. A timeline plays, pauses, changes the speed and seeks, and draws the events, the modes and the score over the whole game. The simulation keeps a copy of the module's memory every 5 s of simulated time, so a seek restores the nearest copy before the target and runs fast from there. The simulation compares the score and the modes it computes with the game records of the file and marks the first difference.

Live over USB, the Teensy sends every event and every game record while the game runs on the machine, and the view follows the game as it happens. The stream carries the same records as a replay file.

The sensor that saw the ball last lights in the drawing. Where the ball is between two sensors stays unknown.

### Part card

A click on a part in any view opens its technical card: its id, its firmware component, its sensor channel or its lamp's chain and LEDs, its coil, and its events and calls. The tool stands alone and links to no document of the repository. The channels and LED ranges come from the config through the real config component, so the simulator holds no second copy of them.

## Tests

The scenarios and the replays run headless in Node.js with the same module, in CI on each push that touches `firmware/`. A replay recorded with an earlier firmware is played with the current one and has to reach the recorded score and modes. A change to a game's rules changes its replays on purpose. The run then names the replays that changed, and a recording made with the new rules replaces them.

## Sketches

A bar at the top of every view switches between the views and holds the simulated time with its pace, the card's source and the firmware build the module was compiled from.

![Features](simulator/features.svg)

![Rules](simulator/rules.svg)

![Play](simulator/game.svg)

![Studio](simulator/studio.svg)

![Replays](simulator/replay.svg)

## Tech stack

| Part | Choice | Reason |
|---|---|---|
| Compiler | Emscripten, the newest emsdk release at the start, pinned in `firmware/sim/emsdk-version` | Compiles the firmware's C++17 to WebAssembly that runs in the browser and in Node.js |
| C++ build | CMake with Ninja through `emcmake` | PlatformIO builds for the Teensy and has no Emscripten platform, so the WebAssembly module takes a second build. CMake lists the same source folders the `computer` environment does |
| C++ to JavaScript | embind, with the module built as an ES module (`MODULARIZE`, `EXPORT_ES6`) | Exposes `step()`, the inputs, the chains, the frame and the game's state to TypeScript with types |
| LVGL in the browser | LVGL 9.6 compiled into the module, display flush into a buffer | [lv_web_emscripten](https://github.com/lvgl/lv_web_emscripten) and LVGL's [browser guide](https://docs.lvgl.io/9.5/integration/pc/browser.html) show the build. SDL stays out, since the screen draws into a canvas of the page |
| Frontend | TypeScript 5, Vite, Preact with `@preact/signals` | Small runtime, JSX, and reactive state for the panels |
| Drawing | `playfield.svg` inline in the page for the parts, a canvas over it for the LED glow, a canvas for the screen | The DOM gives hit testing and ids. A canvas paints the 244 LEDs of the [light layout](../parts/lighting/design.md#light-layout) every frame faster than SVG attributes |
| Sound | Web Audio API, one gain node per channel | Plays the same files the card holds |
| Card and USB | The File System Access API for a card in the reader, the Web Serial API for the Teensy over USB | Both run in Chrome and Edge, which the tool takes as its browsers |
| Tests | Node.js 22 with Vitest | Runs the same module as the browser |
| Place in the repository | `firmware/sim/` for the simulated drivers, the entry point and the CMake build; `tools/simulator/` for the web app; `firmware/tests/scenarios/` for the scenarios | The C++ sits beside the firmware's other builds, the app beside the other tools |

Two existing projects shaped the plan without being reused. [MPF Monitor](https://pypi.org/project/mpf-monitor/) of the Mission Pinball Framework shows lights and switches live over a picture of the playfield. It is bound to MPF's Python runtime, so the idea transfers and the code does not. FastLED's [WebAssembly compiler](https://pypi.org/project/fastled/) runs a FastLED sketch in the browser. The lamps component writes into the lighting driver's chains rather than into FastLED's output, so the real driver above its output stage already holds every color the browser needs.

## What the firmware has to provide

| For | The firmware provides |
|---|---|
| Every view | Fixed numbers for the event types, so a recording and a build agree on them |
| Rules | Modes that name their conditions and scores and count the conditions |
| Studio's check | A debug build that sends a checksum of every lamp frame and every display picture over USB |
| Replays | The replay recorder with its file format |
| Live | The same records sent over USB while the game runs, and the card's files sent on request |

## Phases

| Phase | Delivers | Needs from the firmware |
|---|---|---|
| 1 | Web app with the drawing, the Features view without its demonstrations, and the part card | Nothing |
| 2 | WebAssembly build with the lamps component and the lighting driver; the Studio for lights, with the adapter profiles | The lamps component and the lighting driver, as they stand |
| 3 | Play with clickable inputs, the event log and scenarios, in the browser and in Node.js; the Features view's demonstrations | The game host, the components, a game, fixed event numbers |
| 4 | Screen, touch and videos through LVGL and the video player; sounds through Web Audio; the Studio complete | The screen, video player and sounds components and the display, touch and audio interfaces |
| 5 | Replays from files with the timeline and the comparison; replays as tests in CI | The replay recorder |
| 6 | Live over USB; the Rules view; the Studio's check against the device | The USB stream, the rules of the modes, the checksum build |

Each phase ends in a working tool. Phases 1 and 2 can start now.

## Known limitations

- The ball's motion is not simulated. A replay shows what the sensors saw, and a test states where the ball goes.
- Interrupt latencies, the display bus and the SD card's delays are not modeled. An event reaches the queue at its stamped time, and every transfer ends at once.
- Web Audio resamples and mixes differently from the Teensy's audio library, so a sound's level comes close to the machine's without matching it sample for sample.
- The colors of the LEDs on a computer screen approximate the LEDs in the room.
