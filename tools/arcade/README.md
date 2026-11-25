# Arcade

A browser tool that shows the machine as the drawing in [`assets/playfield/`](../../assets/playfield/) and runs the firmware's lamps behind it.

- **Features** lists the features of the root README. Selecting one lights its lamps, marks and labels its parts in the drawing, and shows its rows of [`docs/features.md`](../../docs/features.md). A click on a part in the drawing selects the feature that shows it. A part that no feature shows opens its card, and a click on the cabinet or into empty space clears both. The wheel or the buttons at the bottom right zoom the drawing, and a drag pans it, in this view and in the Studio.
- **3D** is how the Features view and the Studio show the machine at first. The button 2D among the camera buttons switches to the drawing, and 3D among its zoom buttons switches back. The model is `assets/cgi/machine.glb`, which the Blender scripts of [`assets/cgi/`](../../assets/cgi/) build from the drawing and export, so the films and the Arcade show the same machine. Every part of the file carries its id from the drawing. A drag turns the machine, the wheel zooms and a right drag moves it. A click on a part works as in the drawing. The marked parts glow. An arrow with the part's name points at each of them and keeps its size from any distance. A selected feature takes the machine apart as the drawing does: the covers over its parts go, the parts it shows through turn faint, and its balls at rest appear. The ramp pills hide the ramps. A lamp lights the insert it sits on in the model, in the insert's shape. The perimeter LEDs light at their places in the drawing. The ceiling's LEDs hide under the diorama's top board, as on the machine, and light its scene. Both take the colors the firmware sends in the Studio and the model's colors elsewhere.
- **A part's card** opens from the feature card's list of parts: the part's place, its driver, channel and component, and a lamp's chain and LEDs.
- **Games** is the launcher of every game of the three sources: the memory card, opened in the browser through Chrome's or Edge's folder picker, the firmware, and the repository's `games/<id>/game.yml`. The picked game stands large on top, every game of the chosen source as a cover below it. A `thumbnail.png`, `.jpg` or `.webp` beside a game.yml is the game's picture. Its rules and playing it come with the game loader.
- **Studio** edits a game picked from the same list, one part per tab. Its Lights tab runs the firmware's lamps component and lighting driver, compiled to WebAssembly, on a simulated clock. It starts effects on the playfield's lamps and the strips, switches between the 3 A and the 6 A adapter, and paints the colors the chains carry onto the drawing. A click on a lamp or a strip picks it as the effect's target. Each playfield lamp shows its number in the firmware's count. It plays the light sequences of [`lights/`](lights/), `.lights` files of timed effect calls on tracks, which [`src/sim/sequence.ts`](src/sim/sequence.ts) reads; the format there is a prototype.
- **The current readout** in the Studio adds up what the machine draws from the selected adapter in each frame: the LEDs by FastLED's power model over the colors the driver sent, with the cap's dimming, the loads that always draw, and each coil for its pull time and the servo for its longest move while their buttons have fired them. A total past the adapter's rating turns red. Below it stand the design's worst cases for that adapter: the lighting at its cap with two bumpers and the scoop pulling, and the whole layout at full white.

In every view a bar of pills over the drawing hides and shows the ramps, so the lamps under them show; `config/machine.yml` names each ramp as on the machine and gives its color.

The app stands alone and links to no document of the repository. It follows the system's light or dark setting, and a switch in the top bar picks the other theme for this browser.

The app loads the WebAssembly module in the background as it starts, so the Studio opens ready. The 3D view, three.js and the model load after the page, so the drawing and the lists stand at once. A checkout without `machine.glb`, or a browser without WebGL 2, starts on the drawing. Its 3D view says the file is missing.

## Commands

The commands run in this folder and need Node.js 22 or newer.

| | |
|---|---|
| `npm install` | Installs the dependencies at the versions `package-lock.json` pins |
| `npm run wasm` | Builds the WebAssembly module from [`firmware/sim`](../../firmware/sim/) into `src/wasm/`, which the Studio and its tests need |
| `npm run dev` | Starts the app on http://localhost:5173 and reloads it on every change, to the drawing and the texts as well |
| `npm test` | Runs the Vitest tests. Without `npm run wasm` the tests of the firmware's lamps are skipped |
| `npm run build` | Checks the types and writes the app to `dist/`, which opens from any folder through a static server such as `npm run preview` |
| `python scripts/figures.py` | Writes the figures the current readout takes from the models into `config/figures.json`. `--check` exits with 1 where the file differs from the models, and `npm test` runs that check |

`npm run wasm` takes Emscripten from an emsdk in `EMSDK`, `%LOCALAPPDATA%\emsdk` or `~/emsdk`, at the version [`firmware/sim/emsdk-version`](../../firmware/sim/emsdk-version) names, and CMake and Ninja from the PATH or from Python's `cmake` and `ninja` packages:

```
git clone https://github.com/emscripten-core/emsdk %LOCALAPPDATA%\emsdk
%LOCALAPPDATA%\emsdk\emsdk install 7.0.0
%LOCALAPPDATA%\emsdk\emsdk activate 7.0.0
python -m pip install cmake ninja
```

## Sources

The app reads these files of the repository when it builds and holds no copy of them:

| File | Gives |
|---|---|
| `assets/playfield/playfield.svg` | The drawing, inline in the page |
| `assets/cgi/machine.glb` | The machine in 3D, as the Blender scripts of `assets/cgi/` export it |
| `assets/playfield/playfield.json` | The placements, lamps and strips, and the parts of each feature |
| `README.md` | The numbered feature list with its short texts |
| `docs/features.md` | What the stock machine has and what the mod adds, without the links |
| `firmware/src` | The lamps component and the lighting driver, through `npm run wasm` |
| `games/*/game.yml` | The repository's games |

[`config/figures.json`](config/figures.json) holds every current the readout shows or adds, each with the model that computes it. `scripts/figures.py` evaluates the models under `model/` and writes it, so the app types no figure.

[`config/config.yml`](config/config.yml) is a machine config in the format of `/config.yml` on the memory card, as [`config.md`](../../docs/firmware/components/config.md) gives it. Its lamp groups follow the drawing, the perimeters' heights read off it included, so a run on both perimeters stays level. Its IR calibration and pull times are invented. The Studio sets the firmware's lamps up from it, as the main task does at the start. [`config/machine.yml`](config/machine.yml) maps the drawing's parts to their lamps, drivers and channels, and names the covers the view hides. The tests hold both files to the drawing and to the firmware: each strip's LEDs and hidden stretches, each lamp on the chain and LED the firmware lights for it, and each device on a part the drawing holds.

## Layout

| Path | Holds |
|---|---|
| `src/model/` | Reading the sources, the lamps on their chains, the highlight of a feature, a part's card and the current the machine draws, with no browser needed |
| `src/sim/` | Loading the WebAssembly module, setting it up from the config, the colors of the drawing's lamps and LEDs, and reading and playing `.lights` files |
| `lights/` | The light sequences the Studio plays |
| `src/three/` | Loading the machine in 3D, with its lamps and strip LEDs at the drawing's places |
| `src/ui/` | The drawing with its overlay and its glow, the 3D view, the list, the cards, the Studio and its current readout |
| `scripts/build-wasm.mjs` | What `npm run wasm` runs |
| `scripts/figures.py` | Writes `config/figures.json` from the models |
| `tests/` | `model.test.ts` for the models against the repository's files, `current.test.ts` for the current's arithmetic and the design's worst cases, `lights.test.ts` for the firmware's lamps in WebAssembly, `sequence.test.ts` for the `.lights` format and its player, `three.test.ts` for the ids and the frame of `machine.glb`, `app.test.tsx` and `readout.test.tsx` for the views in jsdom |
