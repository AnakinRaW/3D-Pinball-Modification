// What the machine draws from its adapter: the LED frame by FastLED's power model, the loads that always
// draw, and the coils and the servo while they are active. Every figure comes from config/figures.json,
// which scripts/figures.py writes from the repository's models.
import type { Hardware } from "./config";
import figuresJson from "../../config/figures.json";

export type FigureKey = keyof typeof figuresJson;
export interface Figure {
  value: number;
  unit: string;
  model: string;
  file: string;
}
export type Figures = Record<FigureKey, Figure>;

export const FIGURES: Figures = figuresJson;

export const ADAPTERS = [3, 6] as const;
export type Adapter = (typeof ADAPTERS)[number];

// the adapter's rating, the lighting's cap and the design's worst case for each adapter
export function adapterFigures(f: Figures, a: Adapter) {
  return a === 3
    ? { supply: f.i_supply_3a.value, cap: f.i_budget.value, most: f.i_most_3a.value, white: f.i_white_3a.value }
    : { supply: f.i_supply_6a.value, cap: f.i_budget_6a.value, most: f.i_most_6a.value, white: f.i_white_6a.value };
}

// FastLED's calculate_unscaled_power_mW over the colors as sent: each color's sum times its milliwatts at
// full value, shifted down by eight bits, plus every LED's dark draw.
export function frameMilliwatts(chains: Uint8Array[], f: Figures): number {
  let r = 0;
  let g = 0;
  let b = 0;
  let n = 0;
  for (const c of chains) {
    for (let i = 0; i + 2 < c.length; i += 3) {
      r += c[i];
      g += c[i + 1];
      b += c[i + 2];
      n++;
    }
  }
  const shift = (sum: number, mw: number) => Math.floor((sum * mw) / 256);
  return shift(r, f.pm_red.value) + shift(g, f.pm_green.value) + shift(b, f.pm_blue.value) + n * f.pm_dark.value;
}

export interface LedDraw {
  ma: number;
  brightness: number; // the cap's brightness of the frame, 255 where it dimmed nothing
  dimmed: boolean;
}

export function ledDraw(chains: Uint8Array[], capBrightness: number, f: Figures): LedDraw {
  return { ma: frameMilliwatts(chains, f) / f.v_rail.value, brightness: capBrightness, dimmed: capBrightness < 255 };
}

// the loads that draw all the time, which the power design sums to its permanent consumers
export const BASE_KEYS = ["i_teensy_vin", "i_amp", "i_ir_in", "i_disp", "i_rest"] as const satisfies readonly FigureKey[];
export type BaseKey = (typeof BASE_KEYS)[number];

export function baseMilliamps(f: Figures): number {
  return BASE_KEYS.reduce((sum, k) => sum + f[k].value, 0);
}

export interface Coil {
  id: string; // the part in the drawing
  channel: number;
  current: "i_coil" | "i_coil_scoop";
  pullMs: number;
}

// The solenoid driver's channels, each with its pull-in current and the pull time of the config's
// solenoid.pull_ms, which the driver holds to its ceiling t_on_max.
export function coils(hw: Hardware, f: Figures): Coil[] {
  const ceiling = f.t_on_max.value;
  return Object.entries(hw.map.devices)
    .filter(([, d]) => d.driver === "Solenoid" && d.channel !== undefined)
    .map(([id, d]) => {
      const channel = d.channel as number;
      const wanted = hw.config.pullMs[channel] ?? ceiling;
      return {
        id,
        channel,
        current: d.component === "Scoop" ? ("i_coil_scoop" as const) : ("i_coil" as const),
        pullMs: Math.min(Math.max(wanted, 0), ceiling),
      };
    })
    .sort((a, b) => a.channel - b.channel);
}

// The most coils that pull at once in the solenoid model: as many bumpers as one ball triggers within one
// pull-in, and the scoop.
export function mostAtOnce(all: Coil[], f: Figures): Coil[] {
  const bumpers = all.filter((c) => c.current === "i_coil").slice(0, f.n_bumpers_at_once.value);
  return [...bumpers, ...all.filter((c) => c.current === "i_coil_scoop")];
}

// When each channel's running pull ends, in milliseconds of the clock that fired it.
export type Pulls = ReadonlyMap<number, number>;

export function fire(pulls: Pulls, coil: Coil, nowMs: number): Pulls {
  const next = new Map(pulls);
  next.set(coil.channel, nowMs + coil.pullMs);
  return next;
}

export function pulling(all: Coil[], pulls: Pulls, nowMs: number): Coil[] {
  return all.filter((c) => (pulls.get(c.channel) ?? -Infinity) > nowMs);
}

// When a servo move started at nowMs ends: the servo draws its stall current for the longest move.
export function moveServo(nowMs: number, f: Figures): number {
  return nowMs + f.t_move_max.value;
}

export interface Draw {
  led: number;
  base: number;
  coils: number;
  servo: number;
  total: number;
  supply: number;
  over: number; // how far the total passes the adapter's rating, 0 inside it
  servoWithCoil: boolean; // the servo moves while a coil pulls, which the game rules out
}

export function draw(led: LedDraw, active: Coil[], servoMoving: boolean, adapter: Adapter, f: Figures): Draw {
  const base = baseMilliamps(f);
  const coilMa = active.reduce((sum, c) => sum + f[c.current].value, 0);
  const servo = servoMoving ? f.i_stall.value : 0;
  const total = led.ma + base + coilMa + servo;
  const supply = adapterFigures(f, adapter).supply;
  return {
    led: led.ma,
    base,
    coils: coilMa,
    servo,
    total,
    supply,
    over: Math.max(0, total - supply),
    servoWithCoil: servoMoving && active.length > 0,
  };
}

// The highest total of the last windowMs, so a pull of a few frames stays readable. Takes the samples
// as [time, total] in time order and drops the ones the window has left.
export function holdPeak(samples: [number, number][], nowMs: number, total: number, windowMs: number): number {
  samples.push([nowMs, total]);
  while (samples.length && samples[0][0] < nowMs - windowMs) samples.shift();
  return samples.reduce((peak, [, v]) => Math.max(peak, v), 0);
}

// The design's worst cases for an adapter: the lighting at its cap with the permanent consumers and the
// most coils that pull at once, held against the adapter's rating, and the whole layout at full white
// under the adapter's profile, held against the lighting's cap.
export interface WorstCase {
  key: "most" | "white";
  ma: number;
  limit: number;
  over: number;
  forMs: number | null; // how long the case lasts at most, null where it lasts
}

export function worstCases(f: Figures, adapter: Adapter): WorstCase[] {
  const a = adapterFigures(f, adapter);
  return [
    { key: "most", ma: a.most, limit: a.supply, over: Math.max(0, a.most - a.supply), forMs: f.t_on_max.value },
    { key: "white", ma: a.white, limit: a.cap, over: Math.max(0, a.white - a.cap), forMs: null },
  ];
}
