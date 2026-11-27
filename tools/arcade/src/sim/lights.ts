// The firmware's lamps component and lighting driver as a WebAssembly module, which "npm run wasm"
// builds from firmware/sim. This file loads the module, sets it up from the machine config, and
// turns the colors of its chains into the colors of the drawing's lamps and LEDs.
import { CHAINS, SECTIONS, STRIP_GROUPS, type Hardware, type MachineConfig } from "../model/config";
import type { Playfield, Strip } from "../model/playfield";
import type { ScreenModule } from "./screen";

export type Paint =
  | { r: number; g: number; b: number }
  | { none: true }
  | { palette: string; by?: "time" | "position" | "value" };

export type Target = "playfield" | "perimeterLeft" | "perimeterRight" | "ceiling" | "perimeter" | "ambient";
export type Kind = "set" | "off" | "blink" | "alternate" | "pulse" | "fade" | "run";

// what effect() of the module takes
export interface EffectSpec {
  target: Target;
  first?: number; // the playfield's first lamp
  count?: number; // how many lamps from there
  key: number; // the layer's key
  priority: number; // the layer's priority
  kind: Kind;
  a?: Paint;
  b?: Paint;
  onMs?: number; // blink, alternate
  offMs?: number;
  dark?: boolean; // a blink that shows dark while off
  periodMs?: number; // pulse
  ms?: number; // fade
  pattern?: "chase" | "asteroid" | "wipe"; // run
  down?: boolean;
  step?: number; // run: ms per lamp on the playfield, mm per s along a strip
  lastingMs?: number;
  skipGaps?: boolean; // a strip's run jumps the hidden LEDs
}

export interface Profile {
  perimeterStep: number;
  ambientMax: number;
  playfieldMax: number;
  capMa: number;
}

interface GroupSetup {
  chain: number;
  leds: number;
  lamps?: number[];
  gaps?: number[];
  heights?: number[];
}

// the module's functions, as firmware/sim/simulator.cpp binds them, with the screen's of
// firmware/sim/screen-simulator.cpp
export interface LightsModule extends ScreenModule {
  setUp(adapterA: number, groups: (GroupSetup | null)[]): boolean;
  setAdapter(adapterA: number): boolean;
  step(ms: number): number;
  nowMs(): number;
  chain(c: number): Uint8Array;
  capBrightness(): number;
  profile(): Profile;
  effect(spec: EffectSpec): boolean;
  removeLayer(key: number): void;
  clearLamps(): void;
  setBrightness(playfield: number, ambient: number): void;
  refused(): number;
  playfieldLamps(): number;
}

export const PALETTES = ["rainbow", "rainbowStripe", "party", "ocean", "lava", "forest", "cloud", "heat"] as const;

const chainIndex = (letter: string) => CHAINS.indexOf(letter as (typeof CHAINS)[number]);

// The lamp groups of a config in the order the module takes them, null for a group it leaves out.
export function groupSetups(config: MachineConfig): (GroupSetup | null)[] {
  return [...STRIP_GROUPS, ...SECTIONS].map((name) => {
    const g = config.groups[name];
    if (!g) return null;
    return { chain: chainIndex(g.chain), leds: g.leds, lamps: g.lamps.length ? g.lamps : undefined, gaps: g.gaps.flat(), heights: g.heights.length ? g.heights.flat() : undefined };
  });
}

// What the module is created with. Where wasmBinary holds the bytes of simulator.wasm, the module
// takes them in place of fetching the file, as a test in jsdom has to.
export const lightsOptions: { wasmBinary?: Uint8Array } = {};

// Loads the module, or gets null where "npm run wasm" has not built it.
export async function loadLights(): Promise<LightsModule | null> {
  const found = import.meta.glob<{ default: (options: object) => Promise<LightsModule> }>("../wasm/simulator.mjs");
  const load = found["../wasm/simulator.mjs"];
  if (!load) return null;
  const module = await load();
  const bytes = lightsOptions.wasmBinary;
  // the module hands over its imports and takes the instance back
  const instantiateWasm = (imports: WebAssembly.Imports, receive: (instance: WebAssembly.Instance) => void) => {
    void WebAssembly.instantiate(bytes as BufferSource, imports).then((r) => receive(r.instance));
    return {};
  };
  return module.default(bytes ? { instantiateWasm } : {});
}

// Copies the colors of every chain, three bytes for each LED.
export function readChains(m: LightsModule): Uint8Array[] {
  return CHAINS.map((_, c) => m.chain(c).slice());
}

export type Rgb = [number, number, number];

// The color of every lamp and strip LED of the drawing, from the chains' colors.
export function drawingColors(pf: Playfield, hw: Hardware, chains: Uint8Array[]): Map<string, Rgb> {
  const out = new Map<string, Rgb>();
  const at = (chain: Uint8Array | undefined, led: number): Rgb =>
    chain && chain.length >= 3 * (led + 1) ? [chain[3 * led], chain[3 * led + 1], chain[3 * led + 2]] : [0, 0, 0];
  for (const [stripId, group] of Object.entries(hw.map.strips)) {
    const g = hw.config.groups[group];
    const strip = pf.items.get(stripId) as Strip | undefined;
    if (!g || !strip) continue;
    const chain = chains[chainIndex(g.chain)];
    for (const led of strip.leds) out.set(led.id, at(chain, led.index));
  }
  for (const [lampId, span] of hw.spans) {
    out.set(lampId, at(chains[chainIndex(span.chain)], span.first));
  }
  return out;
}
