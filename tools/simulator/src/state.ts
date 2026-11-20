// The app's data, read once from the repository files, and its state as signals.
import { computed, signal } from "@preact/signals";
import { loadHardware, type Hardware } from "./model/config";
import { loadFeatures, type Feature, type Features } from "./model/features";
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
  };
}

export function createState(model: Model) {
  const selected = signal<string>(model.features.placed[0]?.key ?? "");
  const inspected = signal<string | null>(null); // the part whose card shows instead of the feature's
  const feature = computed(() => model.all.find((f) => f.key === selected.value));
  const highlight = computed<Highlight>(() => planHighlight(model.pf, feature.value?.targets ?? [], model.hw.map.covers, model.hw.map.reveals));
  const view = signal<"features" | "studio">("features");
  // the colors the firmware's lamps show, by the drawing's lamp and LED ids, null outside the studio
  const glow = signal<Map<string, Rgb> | null>(null);
  // the part a click in the studio picked as the effect's target
  const picked = signal<Item | null>(null);
  // the ramps the studio hides
  const hiddenRamps = signal<string[]>([]);
  return { selected, inspected, feature, highlight, view, glow, picked, hiddenRamps };
}

export type State = ReturnType<typeof createState>;
