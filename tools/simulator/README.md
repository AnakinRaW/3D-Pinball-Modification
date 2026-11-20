# Simulator

A browser tool that shows the machine as the drawing in [`assets/playfield/`](../../assets/playfield/) and runs the firmware's lamps behind it.

- **Features** lists the features of the root README. Selecting one lights its lamps, marks and labels its parts in the drawing, and shows its rows of [`docs/features.md`](../../docs/features.md).
- **A part's card** opens on a click into the drawing: the part's place, its driver, channel and component, and a lamp's chain and LEDs.
- **Studio** runs the firmware's lamps component and lighting driver, compiled to WebAssembly, on a simulated clock. It starts effects on the playfield's lamps and the strips, switches between the 3 A and the 6 A adapter, and paints the colors the chains carry onto the drawing. A click on a lamp or a strip picks it as the effect's target. Each playfield lamp shows its number in the firmware's count, and the ramps can be hidden so the lamps under them show.

The app stands alone and links to no document of the repository. It follows the system's light or dark setting, and a switch in the top bar picks the other theme for this browser.

The app loads the WebAssembly module in the background as it starts, so the Studio opens ready.

## Commands

The commands run in this folder and need Node.js 22 or newer.

| | |
|---|---|
| `npm install` | Installs the dependencies at the versions `package-lock.json` pins |
| `npm run wasm` | Builds the WebAssembly module from [`firmware/sim`](../../firmware/sim/) into `src/wasm/`, which the Studio and its tests need |
| `npm run dev` | Starts the app on http://localhost:5173 and reloads it on every change, to the drawing and the texts as well |
| `npm test` | Runs the Vitest tests. Without `npm run wasm` the tests of the firmware's lamps are skipped |
| `npm run build` | Checks the types and writes the app to `dist/`, which opens from any folder through a static server such as `npm run preview` |

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
| `assets/playfield/playfield.json` | The placements, lamps and strips, and the parts of each feature |
| `README.md` | The numbered feature list with its short texts |
| `docs/features.md` | What the stock machine has and what the mod adds, without the links |
| `firmware/src` | The lamps component and the lighting driver, through `npm run wasm` |

[`config/config.yml`](config/config.yml) is a machine config in the format of `/config.yml` on the memory card, as [`config.md`](../../docs/firmware/components/config.md) gives it. Its lamp groups follow the drawing, and its IR calibration and pull times are invented. The Studio sets the firmware's lamps up from it, as the main task does at the start. [`config/machine.yml`](config/machine.yml) maps the drawing's parts to their lamps, drivers and channels, and names the covers the view hides. The tests hold both files to the drawing and to the firmware: each strip's LEDs and hidden stretches, each lamp on the chain and LED the firmware lights for it, and each device on a part the drawing holds.

## Layout

| Path | Holds |
|---|---|
| `src/model/` | Reading the sources, the lamps on their chains, the highlight of a feature and a part's card, with no browser needed |
| `src/sim/` | Loading the WebAssembly module, setting it up from the config, and the colors of the drawing's lamps and LEDs |
| `src/ui/` | The drawing with its overlay and its glow, the list, the cards and the Studio |
| `scripts/build-wasm.mjs` | What `npm run wasm` runs |
| `tests/` | `model.test.ts` for the models against the repository's files, `lights.test.ts` for the firmware's lamps in WebAssembly, `app.test.tsx` for the views in jsdom |
