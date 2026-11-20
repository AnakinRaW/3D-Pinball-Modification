# Simulator

A browser tool that shows the machine as the drawing in [`assets/playfield/`](../../assets/playfield/). It holds the Features view and the parts' cards, with no firmware behind them yet.

- **Features** lists the features of the root README. Selecting one lights its lamps, marks and labels its parts in the drawing, and shows its rows of [`docs/features.md`](../../docs/features.md).
- **A part's card** opens on a click into the drawing: the part's place, its driver, channel and component, and a lamp's chain and LEDs.

The app stands alone and links to no document of the repository.

## Commands

The commands run in this folder and need Node.js 22 or newer.

| | |
|---|---|
| `npm install` | Installs the dependencies at the versions `package-lock.json` pins |
| `npm run dev` | Starts the app on http://localhost:5173 and reloads it on every change, to the drawing and the texts as well |
| `npm test` | Runs the Vitest tests |
| `npm run build` | Checks the types and writes the app to `dist/`, which opens from any folder through a static server such as `npm run preview` |

## Sources

The app reads these files of the repository when it builds and holds no copy of them:

| File | Gives |
|---|---|
| `assets/playfield/playfield.svg` | The drawing, inline in the page |
| `assets/playfield/playfield.json` | The placements, lamps and strips, and the parts of each feature |
| `README.md` | The numbered feature list with its short texts |
| `docs/features.md` | What the stock machine has and what the mod adds, without the links |

[`config/config.yml`](config/config.yml) is a machine config in the format of `/config.yml` on the memory card, as [`config.md`](../../docs/firmware/components/config.md) gives it. Its lamp groups follow the drawing, and its IR calibration and pull times are invented. [`config/machine.yml`](config/machine.yml) maps the drawing's parts to their lamps, drivers and channels, and names the covers the view hides. The tests hold both files to the drawing: each strip's LEDs and hidden stretches, each lamp on the chain its lights group names and each device on a part the drawing holds.

## Layout

| Path | Holds |
|---|---|
| `src/model/` | Reading the sources, the lamps on their chains, the highlight of a feature and a part's card, with no browser needed |
| `src/ui/` | The drawing with its overlay, the list, the feature card and the part card |
| `tests/` | `model.test.ts` for the models against the repository's files, `app.test.tsx` for the view in jsdom |
