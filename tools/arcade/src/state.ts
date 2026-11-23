// The app's data, read once from the repository files, and its state as signals.
import { computed, signal } from "@preact/signals";
import { loadHardware, type Hardware } from "./model/config";
import { loadFeatures, type Feature, type Features } from "./model/features";
import { FIRMWARE_GAMES, gamesFromFiles, type GameEntry } from "./model/games";
import { planHighlight, type Highlight } from "./model/highlight";
import { loadPlayfield, type Item, type Playfield } from "./model/playfield";
import type { Rgb } from "./sim/lights";
import type { Sources } from "./sources";

export interface Model {
  pf: Playfield;
  hw: Hardware;
  features: Features;
  all: Feature[];
  svg: string;
  machineGlb: string | undefined; // the URL of the machine in 3D
  repoGames: GameEntry[];
}

export function loadModel(s: Sources): Model {
  const pf = loadPlayfield(s.playfieldJson);
  const features = loadFeatures(pf, s.featuresMd, s.readmeMd);
  return {
    pf,
    hw: loadHardware(s.configYml, s.machineYml),
    features,
    all: [...features.placed, ...features.unplaced],
    svg: s.playfieldSvg,
    machineGlb: s.machineGlb,
    repoGames: gamesFromFiles("repo", s.repoGames, s.repoThumbnails),
  };
}

export function createState(model: Model) {
  // the feature the view shows, none at first
  const selected = signal<string>("");
  const inspected = signal<string | null>(null); // the part whose card shows instead of the feature's
  const feature = computed(() => model.all.find((f) => f.key === selected.value));
  const highlight = computed<Highlight>(() => planHighlight(model.pf, feature.value?.targets ?? [], model.hw.map.covers, model.hw.map.reveals));
  const view = signal<"features" | "games" | "studio">("features");
  // the colors the firmware's lamps show, by the drawing's lamp and LED ids, null outside the studio
  const glow = signal<Map<string, Rgb> | null>(null);
  // the part a click in the studio picked as the effect's target
  const picked = signal<Item | null>(null);
  // the ramps the studio hides
  const hiddenRamps = signal<string[]>([]);
  // whether the Features view and the studio show the machine in 3D instead of the drawing: at first
  // wherever the model is in the checkout and the browser draws WebGL 2, as three.js needs
  const threeD = signal(!!model.machineGlb && typeof WebGL2RenderingContext !== "undefined");
  // the games of a memory card the browser opened, and every game of the three sources
  const cardGames = signal<GameEntry[]>([]);
  const games = computed(() => [...model.repoGames, ...FIRMWARE_GAMES, ...cardGames.value]);
  // the game the explorer and the studio show, by its key
  const selectedGame = signal<string | null>(null);
  const game = computed(() => games.value.find((g) => g.key === selectedGame.value) ?? null);
  return { selected, inspected, feature, highlight, view, glow, picked, hiddenRamps, threeD, cardGames, games, selectedGame, game };
}

export type State = ReturnType<typeof createState>;
