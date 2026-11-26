# Arcade

A browser tool that shows the machine as the drawing in [`assets/playfield/`](../../assets/playfield/) or in 3D, and runs the firmware's lamps and screen behind it.

| View | Shows |
|---|---|
| Features | The features of the root README on the machine. A click on a part selects its feature or opens the part's card |
| Games | Every game of the memory card, the firmware and `games/`, as covers. A `thumbnail.png`, `.jpg` or `.webp` beside a `game.yml` is its picture |
| Studio, Lights | The firmware's lamps on the machine, with effects, the light sequences of [`lights/`](lights/) and the current the machine draws. The `.lights` format is a prototype |
| Studio, Screen | The machine's display, worked with the mouse or a finger as on the touch panel. Each screen of the host shows with made-up data, and its presses lead on as the host's would, with Back returning to the page before. A stand-in game runs behind the screens of a game, where a swipe from the right edge pulls in the host's menu. The Controls page shows every control of the host's screens. The boot film plays through the firmware's video player from a simulated card, without its sound |

The machine in 3D is `assets/cgi/machine.glb`. Without it, or in a browser without WebGL 2, the views show the drawing.

## Commands

The commands run in this folder and need Node.js 22 or newer.

| | |
|---|---|
| `npm install` | Installs the dependencies at the versions `package-lock.json` pins |
| `npm run wasm` | Builds the firmware from [`firmware/sim`](../../firmware/sim/) into `src/wasm/`, which the Studio and its tests need |
| `npm run dev` | Starts the app on http://localhost:5173 and reloads it on every change |
| `npm test` | Runs the Vitest tests. Without `npm run wasm` the firmware's tests are skipped |
| `npm run build` | Checks the types and writes the app to `dist/` |
| `python scripts/figures.py` | Writes the readout's currents from the models into `config/figures.json`. `--check` exits with 1 where they differ |

`npm run wasm` needs Emscripten at the version [`firmware/sim/emsdk-version`](../../firmware/sim/emsdk-version) names, and CMake and Ninja:

```
git clone https://github.com/emscripten-core/emsdk %LOCALAPPDATA%\emsdk
%LOCALAPPDATA%\emsdk\emsdk install 7.0.0
%LOCALAPPDATA%\emsdk\emsdk activate 7.0.0
python -m pip install cmake ninja
```

## Sources

The app reads these files when it builds and holds no copy of them:

| File | Gives |
|---|---|
| `assets/playfield/playfield.svg`, `playfield.json` | The drawing, its lamps and strips, and the parts of each feature |
| `assets/cgi/machine.glb` | The machine in 3D |
| `README.md`, `docs/features.md` | The features and their texts |
| `firmware/src` | The lamps, the lighting driver, the screen and the touch driver, through `npm run wasm` |
| `games/*/game.yml` | The repository's games |
| [`config/config.yml`](config/config.yml) | A machine config in the memory card's format. Its IR calibration and pull times are invented |
| [`config/machine.yml`](config/machine.yml) | The drawing's parts with their lamps, drivers and channels, and the ramps |
| [`config/figures.json`](config/figures.json) | The currents the readout adds up, from the models under `model/` |

## Layout

| Path | Holds |
|---|---|
| `src/model/` | The sources read, the lamps on their chains, a feature's highlight, a part's card and the current |
| `src/sim/` | The WebAssembly module, the `.lights` player, and the display's colors and touches |
| `src/three/` | The machine in 3D |
| `src/ui/` | The views |
| `lights/` | The light sequences |
| `scripts/` | `build-wasm.mjs` for `npm run wasm`, `figures.py` for `config/figures.json` |
| `tests/` | The Vitest tests, one file per part of the app |
