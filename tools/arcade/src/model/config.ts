// The machine config in the format of /config.yml on the memory card, and the Arcade's map from the
// drawing's parts to the hardware. Together they place every lamp on its chain and every device on its channel.
import { parse } from "yaml";

export const CHAINS = ["A", "B", "C", "D", "E", "F"] as const;
export const SECTIONS = ["playfield_lower", "playfield_middle", "playfield_upper"] as const;
export const STRIP_GROUPS = ["perimeter_left", "perimeter_right", "ceiling"] as const;
const CHAIN_MAX_LEDS = 160; // the lighting driver's bound: one meter of the strip

export interface GroupSettings {
  chain: string;
  leds: number;
  gaps: [number, number][];
  heights: [number, number][]; // perimeters only: an LED and its height in mm, between them evenly
  lamps: number[]; // playfield sections only: each lamp's LEDs
}

export interface IrChannel {
  channel: number;
  clear_build: number;
  ball_build: number;
  order: number;
}

export interface Device {
  driver: string;
  channel?: number;
  component: string;
}

export interface MachineConfig {
  adapterA: number;
  groups: Record<string, GroupSettings>;
  irChannels: IrChannel[];
  pullMs: number[];
}

// a ramp the switch bar hides: its placement, its name on the machine and its pill's color
export interface Ramp {
  id: string;
  name: string;
  color: string;
}

export interface MachineMap {
  lamps: Record<string, string[]>; // section → lamp ids in chain order
  strips: Record<string, string>; // drawing strip id → lamp group
  devices: Record<string, Device>;
  covers: Record<string, string[]>; // cover part → the parts under it
  reveals: Record<string, string[]>; // part → the balls at rest shown with it
  ramps: Ramp[]; // the ramps the switch bar hides
}

// where a playfield lamp's LEDs sit
export interface LampSpan {
  section: string;
  chain: string;
  index: number; // the lamp's place in the playfield, lower section first
  first: number; // its first LED on the chain
  count: number;
}

export interface Hardware {
  config: MachineConfig;
  map: MachineMap;
  spans: Map<string, LampSpan>;
}

type Yaml = Record<string, unknown>;

function group(raw: unknown): GroupSettings {
  const g = (raw ?? {}) as Yaml;
  return {
    chain: String(g.chain ?? ""),
    leds: Number(g.leds ?? 0),
    gaps: ((g.gaps as [number, number][]) ?? []).map(([a, b]) => [Number(a), Number(b)]),
    heights: ((g.heights as [number, number][]) ?? []).map(([a, b]) => [Number(a), Number(b)]),
    lamps: ((g.lamps as number[]) ?? []).map(Number),
  };
}

export function loadConfig(text: string): MachineConfig {
  const y = (parse(text) ?? {}) as Yaml;
  const lamps = (y.lamps ?? {}) as Yaml;
  const groups: Record<string, GroupSettings> = {};
  for (const name of [...STRIP_GROUPS, ...SECTIONS]) if (lamps[name]) groups[name] = group(lamps[name]);
  const lighting = (y.lighting ?? {}) as Yaml;
  const ir = (y.ir ?? {}) as Yaml;
  const solenoid = (y.solenoid ?? {}) as Yaml;
  return {
    adapterA: Number(lighting.adapter_a ?? 3),
    groups,
    irChannels: ((ir.channels as IrChannel[]) ?? []).map((c) => ({ ...c })),
    pullMs: ((solenoid.pull_ms as number[]) ?? []).map(Number),
  };
}

export function loadMachineMap(text: string): MachineMap {
  const y = (parse(text) ?? {}) as Yaml;
  return {
    lamps: (y.lamps ?? {}) as Record<string, string[]>,
    strips: (y.strips ?? {}) as Record<string, string>,
    devices: (y.devices ?? {}) as Record<string, Device>,
    covers: (y.covers ?? {}) as Record<string, string[]>,
    reveals: (y.reveals ?? {}) as Record<string, string[]>,
    ramps: Object.entries((y.ramps ?? {}) as Record<string, { name?: string; color?: string }>).map(([id, r]) => ({
      id,
      name: String(r?.name ?? id),
      color: String(r?.color ?? ""),
    })),
  };
}

function hidden(g: GroupSettings, led: number): boolean {
  return g.gaps.some(([first, count]) => led >= first && led < first + count);
}

// Cuts the playfield's sections into lamps, as the lamps component walks them: LED by LED, a lamp
// starting at each LED that is not hidden. A lamp takes the section's next count of LEDs, 1 once the
// counts run out or for a count of 0, and ends early at a hidden LED or the chain's end.
export function lampSpans(config: MachineConfig): LampSpan[][] {
  const out: LampSpan[][] = [];
  let index = 0;
  for (const section of SECTIONS) {
    const g = config.groups[section];
    const spans: LampSpan[] = [];
    if (g) {
      let next = 0;
      let led = 0;
      while (led < g.leds) {
        if (hidden(g, led)) {
          led++;
          continue;
        }
        const want = next < g.lamps.length ? g.lamps[next++] || 1 : 1;
        let count = 0;
        while (count < want && led + count < g.leds && !hidden(g, led + count)) count++;
        spans.push({ section, chain: g.chain, index: index++, first: led, count });
        led += count;
      }
    }
    out.push(spans);
  }
  return out;
}

// The problems of a config, in the words of the lamps component's rules.
export function configProblems(config: MachineConfig): string[] {
  const problems: string[] = [];
  const used = new Map<string, string>();
  for (const [name, g] of Object.entries(config.groups)) {
    if (!CHAINS.includes(g.chain as (typeof CHAINS)[number])) problems.push(`lamps.${name}: chain ${g.chain} is not A to F`);
    const other = used.get(g.chain);
    if (other) problems.push(`lamps.${name}: chain ${g.chain} is already taken by lamps.${other}`);
    used.set(g.chain, name);
    if (g.leds < 0 || g.leds > CHAIN_MAX_LEDS) problems.push(`lamps.${name}: ${g.leds} LEDs lie outside 0 to ${CHAIN_MAX_LEDS}`);
    const sum = g.lamps.reduce((a, b) => a + b, 0);
    if (g.lamps.length && sum > g.leds) problems.push(`lamps.${name}: its lamps take ${sum} LEDs of ${g.leds}`);
  }
  const channels = new Set<number>();
  for (const c of config.irChannels) {
    if (c.channel < 0 || c.channel > 15 || channels.has(c.channel)) problems.push(`ir.channels: channel ${c.channel} is invalid or repeated`);
    channels.add(c.channel);
    if (!(c.clear_build >= 0 && c.clear_build < c.ball_build && c.ball_build <= 1023))
      problems.push(`ir.channels: channel ${c.channel} needs 0 <= clear_build < ball_build <= 1023`);
  }
  return problems;
}

export function loadHardware(configText: string, mapText: string): Hardware {
  const config = loadConfig(configText);
  const map = loadMachineMap(mapText);
  const spans = new Map<string, LampSpan>();
  lampSpans(config).forEach((section, k) => {
    const ids = map.lamps[SECTIONS[k]] ?? [];
    section.forEach((span, i) => {
      if (ids[i]) spans.set(ids[i], span);
    });
  });
  return { config, map, spans };
}
