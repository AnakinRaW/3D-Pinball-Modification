// The drawing's structure, read from assets/playfield/playfield.json: every placement, lamp and strip LED
// with its place in millimeters, computed as tools/playfield/compose.py computes it.

export type Point = [number, number];

interface Frame {
  scale: number;
  about: Point;
}

interface JsonNode {
  id?: string;
  group?: string;
  strip?: string;
  lamp?: string;
  part?: string;
  label?: string;
  at?: Point;
  rotate?: number;
  scale?: number;
  mirror?: boolean;
  frame?: string;
  on?: string;
  children?: JsonNode[];
  firmware?: string;
  chain?: string;
  prefix?: string;
  count?: number;
  pitch?: number;
  route?: { points: Point[]; hidden?: boolean }[];
}

export interface FeaturePlace {
  n: number;
  feature: string;
  targets: string[];
}

export interface FeatureWindow {
  around: string[];
  through: string[];
  margin: number;
}

interface PlayfieldJson {
  viewBox: [number, number, number, number];
  frames?: Record<string, Frame>;
  layers: JsonNode[];
  features?: FeaturePlace[];
  feature_windows?: FeatureWindow[];
}

export interface Placement {
  kind: "placement";
  id: string;
  part: string;
  label: string;
  groups: string[]; // the ids of the groups around it, outermost first
  point: Point;
  hidden: boolean; // shown only in a state, such as a ball at rest
}

export interface Lamp {
  kind: "lamp";
  id: string;
  on: string;
  group: string; // the lights group, such as lights-stars
  groupLabel: string;
  point: Point;
}

export interface Led {
  kind: "led";
  id: string;
  strip: string;
  index: number;
  point: Point;
  hidden: boolean; // on a stretch of the route the drawing hides, or past its end
}

export interface Strip {
  kind: "strip";
  id: string;
  label: string;
  firmware: string;
  chain: string;
  leds: Led[];
}

export type Item = Placement | Lamp | Strip | Led;

export interface Playfield {
  viewBox: [number, number, number, number];
  items: Map<string, Item>;
  placements: Placement[];
  lamps: Lamp[];
  strips: Strip[];
  features: FeaturePlace[];
  windows: FeatureWindow[];
}

function framed(doc: PlayfieldJson, node: JsonNode, p: Point): Point {
  const f = node.frame ? doc.frames?.[node.frame] : undefined;
  if (!f) return p;
  const [cx, cy] = f.about;
  return [cx + (p[0] - cx) * f.scale, cy + (p[1] - cy) * f.scale];
}

// a point in a part's coordinates, carried into the drawing's
function toWorld(doc: PlayfieldJson, node: JsonNode, p: Point): Point {
  let [x, y] = p;
  if (node.mirror) x = -x;
  const s = node.scale ?? 1;
  x *= s;
  y *= s;
  const a = ((node.rotate ?? 0) * Math.PI) / 180;
  [x, y] = [x * Math.cos(a) - y * Math.sin(a), x * Math.sin(a) + y * Math.cos(a)];
  const at = node.at ?? [0, 0];
  return framed(doc, node, [x + at[0], y + at[1]]);
}

function stripLeds(doc: PlayfieldJson, node: JsonNode): Led[] {
  const runs: { start: number; length: number; a: Point; b: Point; hidden: boolean }[] = [];
  let total = 0;
  for (const seg of node.route ?? []) {
    const pts = seg.points.map((p) => framed(doc, node, p));
    for (let k = 0; k + 1 < pts.length; k++) {
      const [a, b] = [pts[k], pts[k + 1]];
      const n = Math.hypot(b[0] - a[0], b[1] - a[1]);
      if (n > 0) {
        runs.push({ start: total, length: n, a, b, hidden: seg.hidden ?? false });
        total += n;
      }
    }
  }
  const leds: Led[] = [];
  for (let i = 0; i < (node.count ?? 0); i++) {
    const s = (i + 0.5) * (node.pitch ?? 0);
    const found = runs.find((r) => s <= r.start + r.length);
    const run = found ?? runs[runs.length - 1];
    const t = (s - run.start) / run.length;
    leds.push({
      kind: "led",
      id: `${node.prefix}${i}`,
      strip: node.strip!,
      index: i,
      point: [run.a[0] + (run.b[0] - run.a[0]) * t, run.a[1] + (run.b[1] - run.a[1]) * t],
      hidden: run.hidden || found === undefined,
    });
  }
  return leds;
}

export function loadPlayfield(text: string): Playfield {
  const doc = JSON.parse(text) as PlayfieldJson;
  const placed = new Map<string, JsonNode>();
  const collect = (n: JsonNode) => {
    if (n.part && n.id) placed.set(n.id, n);
    n.children?.forEach(collect);
  };
  doc.layers.forEach(collect);

  const pf: Playfield = {
    viewBox: doc.viewBox,
    items: new Map(),
    placements: [],
    lamps: [],
    strips: [],
    features: doc.features ?? [],
    windows: doc.feature_windows ?? [],
  };

  const walk = (n: JsonNode, groups: string[], groupLabel: string) => {
    if (n.part && n.id) {
      const p: Placement = {
        kind: "placement",
        id: n.id,
        part: n.part,
        label: n.label ?? "",
        groups,
        point: toWorld(doc, n, [0, 0]),
        hidden: groups.includes("balls"),
      };
      pf.placements.push(p);
      pf.items.set(p.id, p);
      return;
    }
    if (n.strip) {
      const s: Strip = {
        kind: "strip",
        id: n.strip,
        label: n.label ?? n.strip,
        firmware: n.firmware ?? "",
        chain: n.chain ?? "",
        leds: stripLeds(doc, n),
      };
      pf.strips.push(s);
      pf.items.set(s.id, s);
      for (const led of s.leds) pf.items.set(led.id, led);
      return;
    }
    if (n.lamp && n.on) {
      const owner = placed.get(n.on);
      if (!owner) throw new Error(`${n.lamp} sits on ${n.on}, which no placement names`);
      const l: Lamp = {
        kind: "lamp",
        id: n.lamp,
        on: n.on,
        group: groups[groups.length - 1] ?? "",
        groupLabel,
        point: toWorld(doc, owner, n.at ?? [0, 0]),
      };
      pf.lamps.push(l);
      pf.items.set(l.id, l);
      return;
    }
    const name = n.group ?? n.id;
    const inner = name ? [...groups, name] : groups;
    for (const c of n.children ?? []) walk(c, inner, n.label ?? groupLabel);
  };
  doc.layers.forEach((n) => walk(n, [], ""));
  return pf;
}

const ACRONYMS: Record<string, string> = { ir: "IR", led: "LED" };

// A short name for a part of the drawing: a strip's name for a strip and its LEDs, otherwise its id in words.
export function shortName(pf: Playfield, id: string): string {
  const item = pf.items.get(id);
  if (item?.kind === "led") return stripName(pf.items.get(item.strip) as Strip);
  if (item?.kind === "strip") return stripName(item);
  const s = id
    .split("-")
    .map((w) => ACRONYMS[w] ?? w)
    .join(" ");
  return s.charAt(0).toUpperCase() + s.slice(1);
}

// a strip's name without the chain and the notes its label carries, such as "Perimeter left"
export function stripName(s: Strip): string {
  return s.label.split(/[(:]/)[0].trim();
}

// The lamps that sit on a placement.
export function lampsOn(pf: Playfield, id: string): Lamp[] {
  return pf.lamps.filter((l) => l.on === id);
}

// The parts above the given targets that the drawing shows through, after the feature windows.
export function seeThrough(pf: Playfield, targets: string[]): string[] {
  const out = new Set<string>();
  for (const w of pf.windows) {
    if (w.around.some((t) => targets.includes(t))) w.through.forEach((p) => out.add(p));
  }
  return [...out];
}

// The hidden stretches of a strip as pairs of first LED and count, the form of the config's gaps.
export function hiddenStretches(s: Strip): [number, number][] {
  const gaps: [number, number][] = [];
  for (const led of s.leds) {
    if (!led.hidden) continue;
    const last = gaps[gaps.length - 1];
    if (last && last[0] + last[1] === led.index) last[1]++;
    else gaps.push([led.index, 1]);
  }
  return gaps;
}
