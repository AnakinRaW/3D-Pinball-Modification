// Light sequences: a .lights file lists timed calls of the firmware's lamp effects. This file reads
// one, resolves its lamp names against the config, and plays it on the WebAssembly module on the
// simulated clock. A prototype of the format, played from the browser.
//
//   name: attract
//   length: 8000          # ms; the sequence repeats after it while loop is true
//   loop: true
//   steps:
//     - at: 0             # ms from the start, or "+200" after the previous step
//       track: 0          # 0 lies lowest; a step replaces what its track showed on its lamps
//       lamps: stars      # a lamp group, a lamp, "playfield", or a list of them
//       effect: blink     # set, off, blink, alternate, pulse, fade, run, clear
//       a: red            # a named color, "#rrggbb", "below", or {palette: rainbow, by: position}
//       b: black
//       on: 80            # blink, alternate: ms on and off
//       off: 80
//       for: 800          # optional: the effect ends after this many ms
//     - at: 400
//       track: 1
//       strip: ambient    # perimeter-left, perimeter-right, ceiling, perimeter, ambient
//       effect: run
//       pattern: wipe     # chase, asteroid, wipe
//       mm_per_s: 300     # a strip run's speed; a playfield run takes ms_per_lamp
//       a: {palette: rainbow, by: position}
import { parse } from "yaml";
import type { Hardware } from "../model/config";
import type { Playfield } from "../model/playfield";
import type { EffectSpec, Kind, LightsModule, Paint, Target } from "./lights";

export interface SequenceStep {
  atMs: number;
  track: number;
  spec: Omit<EffectSpec, "key" | "priority"> | null; // null clears the track
  line: string; // what the step names, for messages
}

export interface Sequence {
  name: string;
  lengthMs: number;
  loop: boolean;
  tracks: number;
  steps: SequenceStep[];
}

export interface Parsed {
  sequence: Sequence | null;
  errors: string[];
}

const COLORS: Record<string, { r: number; g: number; b: number }> = {
  black: { r: 0, g: 0, b: 0 },
  white: { r: 255, g: 255, b: 255 },
  red: { r: 255, g: 0, b: 0 },
  green: { r: 0, g: 255, b: 0 },
  blue: { r: 0, g: 0, b: 255 },
  amber: { r: 255, g: 120, b: 0 },
};
const STRIPS: Record<string, Target> = {
  "perimeter-left": "perimeterLeft",
  "perimeter-right": "perimeterRight",
  ceiling: "ceiling",
  perimeter: "perimeter",
  ambient: "ambient",
};
const KINDS: (Kind | "clear")[] = ["set", "off", "blink", "alternate", "pulse", "fade", "run", "clear"];
const PALETTES = ["rainbow", "rainbowStripe", "party", "ocean", "lava", "forest", "cloud", "heat"];

type Yaml = Record<string, unknown>;

// The playfield lamps of each name a sequence may use: every lights group of the drawing by its
// short name, such as "stars" for lights-stars, every lamp by its id, and "playfield" for all.
export function lampNames(pf: Playfield, hw: Hardware): Map<string, number[]> {
  const out = new Map<string, number[]>();
  const add = (name: string, index: number) => out.set(name, [...(out.get(name) ?? []), index]);
  for (const lamp of pf.lamps) {
    const span = hw.spans.get(lamp.id);
    if (!span) continue;
    add(lamp.id, span.index);
    add(lamp.group.replace(/^lights-/, ""), span.index);
    add("playfield", span.index);
  }
  for (const [k, v] of out) out.set(k, [...v].sort((a, b) => a - b));
  return out;
}

function paintOf(v: unknown, where: string, errors: string[]): Paint | undefined {
  if (v === undefined) return undefined;
  if (v === "below") return { none: true };
  if (typeof v === "string") {
    if (COLORS[v]) return COLORS[v];
    const m = /^#([0-9a-f]{2})([0-9a-f]{2})([0-9a-f]{2})$/i.exec(v);
    if (m) return { r: parseInt(m[1], 16), g: parseInt(m[2], 16), b: parseInt(m[3], 16) };
  }
  if (v && typeof v === "object" && "palette" in (v as Yaml)) {
    const p = v as Yaml;
    const by = (p.by ?? "value") as string;
    if (PALETTES.includes(String(p.palette)) && ["time", "position", "value"].includes(by)) {
      return { palette: String(p.palette), by: by as "time" | "position" | "value" };
    }
  }
  errors.push(`${where}: no color ${JSON.stringify(v)}`);
  return undefined;
}

const num = (s: Yaml, key: string) => (typeof s[key] === "number" ? (s[key] as number) : undefined);

// Reads a .lights file. The lamp names resolve to ranges of the playfield; a name whose lamps do not
// lie in one row of the playfield's count is an error, since a lamp range is one row.
export function parseSequence(text: string, names: Map<string, number[]>): Parsed {
  const errors: string[] = [];
  let doc: Yaml;
  try {
    doc = (parse(text) ?? {}) as Yaml;
  } catch (e) {
    return { sequence: null, errors: [`not YAML: ${(e as Error).message}`] };
  }
  const rawSteps = Array.isArray(doc.steps) ? (doc.steps as Yaml[]) : [];
  if (!rawSteps.length) errors.push("no steps");
  const steps: SequenceStep[] = [];
  let last = 0;
  rawSteps.forEach((s, i) => {
    const where = `step ${i + 1}`;
    let atMs = 0;
    if (typeof s.at === "number") atMs = s.at;
    else if (typeof s.at === "string" && /^\+\d+$/.test(s.at)) atMs = last + Number(s.at.slice(1));
    else errors.push(`${where}: at is a number of ms or "+ms"`);
    last = atMs;
    const track = typeof s.track === "number" ? s.track : 0;
    const effect = String(s.effect ?? "");
    if (!KINDS.includes(effect as Kind)) {
      errors.push(`${where}: no effect ${JSON.stringify(s.effect)}`);
      return;
    }
    if (effect === "clear") {
      steps.push({ atMs, track, spec: null, line: `${where}: clear track ${track}` });
      return;
    }
    // the targets: lamps of the playfield, or a strip
    const targets: Pick<EffectSpec, "target" | "first" | "count">[] = [];
    if (s.strip !== undefined) {
      const t = STRIPS[String(s.strip)];
      if (!t) errors.push(`${where}: no strip ${JSON.stringify(s.strip)}`);
      else targets.push({ target: t });
    }
    if (s.lamps !== undefined) {
      for (const name of Array.isArray(s.lamps) ? s.lamps : [s.lamps]) {
        const lamps = names.get(String(name));
        if (!lamps) {
          errors.push(`${where}: no lamps ${JSON.stringify(name)}`);
          continue;
        }
        const first = lamps[0];
        const count = lamps[lamps.length - 1] - first + 1;
        if (count !== lamps.length) errors.push(`${where}: the lamps of ${name} lie in no single row`);
        targets.push({ target: "playfield", first, count });
      }
    }
    if (!targets.length) errors.push(`${where}: names no lamps and no strip`);
    const strip = s.strip !== undefined;
    const spec: Omit<EffectSpec, "key" | "priority" | "target"> = {
      kind: effect as Kind,
      a: paintOf(s.a, where, errors),
      b: paintOf(s.b, where, errors),
      onMs: num(s, "on"),
      offMs: num(s, "off"),
      dark: s.b === "black",
      periodMs: num(s, "period"),
      ms: num(s, "ms"),
      pattern: s.pattern as EffectSpec["pattern"],
      down: s.direction === "down",
      step: strip ? num(s, "mm_per_s") : num(s, "ms_per_lamp"),
      lastingMs: num(s, "for"),
    };
    if (effect === "run" && spec.step === undefined) {
      errors.push(`${where}: a run needs ${strip ? "mm_per_s" : "ms_per_lamp"}`);
    }
    for (const t of targets) steps.push({ atMs, track, spec: { ...spec, ...t }, line: `${where}: ${effect}` });
  });
  steps.sort((a, b) => a.atMs - b.atMs);
  const lengthMs = typeof doc.length === "number" ? doc.length : (steps[steps.length - 1]?.atMs ?? 0);
  const tracks = steps.reduce((n, s) => Math.max(n, s.track + 1), 0);
  if (errors.length) return { sequence: null, errors };
  return {
    sequence: { name: String(doc.name ?? "sequence"), lengthMs, loop: doc.loop !== false, tracks, steps },
    errors,
  };
}

// Plays a sequence on the module. Each track takes a layer: key firstKey + track, priority
// basePriority + track, so a higher track lies above a lower one.
export class SequencePlayer {
  private next = 0; // the next step to issue
  private startMs = 0; // the module's time the current round began
  playing = false;

  constructor(
    private m: LightsModule,
    readonly sequence: Sequence,
    private firstKey: number,
    private basePriority: number,
  ) {}

  private layer(track: number) {
    return { key: this.firstKey + track, priority: this.basePriority + track };
  }

  start() {
    this.stop();
    this.playing = true;
    this.startMs = this.m.nowMs();
    this.next = 0;
    this.issueDue();
  }

  stop() {
    for (let t = 0; t < this.sequence.tracks; t++) this.m.removeLayer(this.firstKey + t);
    this.playing = false;
  }

  // Issues every step due at the module's time, and starts the next round where the sequence loops.
  issueDue() {
    if (!this.playing) return;
    const now = this.m.nowMs();
    for (;;) {
      const steps = this.sequence.steps;
      if (this.next >= steps.length) {
        if (!this.sequence.loop) {
          this.playing = this.sequence.lengthMs > now - this.startMs;
          return;
        }
        if (now - this.startMs < this.sequence.lengthMs) return;
        // a new round begins exactly one length after the last
        this.startMs += this.sequence.lengthMs;
        for (let t = 0; t < this.sequence.tracks; t++) this.m.removeLayer(this.firstKey + t);
        this.next = 0;
        continue;
      }
      const step = steps[this.next];
      if (now - this.startMs < step.atMs) return;
      const { key, priority } = this.layer(step.track);
      if (step.spec === null) this.m.removeLayer(key);
      else this.m.effect({ ...step.spec, key, priority } as EffectSpec);
      this.next++;
    }
  }

  // Gets the module's time of the next step, so the clock stops there and the step starts in the
  // pass it is due in.
  nextDueMs(): number | null {
    if (!this.playing) return null;
    const steps = this.sequence.steps;
    if (this.next < steps.length) return this.startMs + steps[this.next].atMs;
    return this.sequence.loop ? this.startMs + this.sequence.lengthMs : null;
  }
}

// Runs the module's clock for ms, stopping at each step the player has due, so every step starts in
// the pass of its time.
export function advanceWith(m: LightsModule, player: SequencePlayer | null, ms: number) {
  const end = m.nowMs() + ms;
  for (;;) {
    player?.issueDue();
    const due = player?.nextDueMs() ?? null;
    const now = m.nowMs();
    const to = due !== null && due < end ? due : end;
    if (to > now) m.step(to - now);
    if (to >= end) {
      player?.issueDue();
      return;
    }
  }
}
