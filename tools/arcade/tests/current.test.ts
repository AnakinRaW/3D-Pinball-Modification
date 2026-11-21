import { execFileSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";
import {
  adapterFigures,
  baseMilliamps,
  coils,
  draw,
  FIGURES as f,
  fire,
  frameMilliwatts,
  holdPeak,
  ledDraw,
  mostAtOnce,
  moveServo,
  pulling,
  worstCases,
  type Pulls,
} from "../src/model/current";
import { loadHardware } from "../src/model/config";
import { sources } from "../src/sources";

const repo = (path: string) => fileURLToPath(new URL(`../../../${path}`, import.meta.url));
const hw = loadHardware(sources.configYml, sources.machineYml);

// one chain of n LEDs, every one at the same color
const chain = (n: number, r: number, g: number, b: number) => {
  const c = new Uint8Array(n * 3);
  for (let i = 0; i < n; i++) c.set([r, g, b], i * 3);
  return c;
};
const dark = { ma: 0, brightness: 255, dimmed: false };

describe("the figures", () => {
  it("are the ones the models compute", () => {
    let python = "python";
    try {
      execFileSync(python, ["--version"]);
    } catch {
      python = "python3";
    }
    const out = execFileSync(python, [repo("tools/arcade/scripts/figures.py"), "--check"]).toString();
    expect(out).toContain("matches the models");
  });
});

describe("the LED frame", () => {
  it("draws every LED's dark draw while dark", () => {
    expect(frameMilliwatts([chain(10, 0, 0, 0), chain(5, 0, 0, 0)], f)).toBe(15 * f.pm_dark.value);
  });

  it("sums each color as FastLED does, shifted down by eight bits", () => {
    // ten LEDs at full red: 2550 × 28 mW / 256 = 278.9, cut to 278, plus ten dark draws
    expect(frameMilliwatts([chain(10, 255, 0, 0)], f)).toBe(Math.floor((2550 * f.pm_red.value) / 256) + 10 * f.pm_dark.value);
    const white = frameMilliwatts([chain(1, 255, 255, 255)], f);
    expect(white).toBe(
      Math.floor((255 * f.pm_red.value) / 256) +
        Math.floor((255 * f.pm_green.value) / 256) +
        Math.floor((255 * f.pm_blue.value) / 256) +
        f.pm_dark.value,
    );
  });

  it("turns milliwatts into milliamps at the rail and reads the cap's dimming", () => {
    const led = ledDraw([chain(4, 255, 255, 255)], 200, f);
    expect(led.ma).toBeCloseTo(frameMilliwatts([chain(4, 255, 255, 255)], f) / f.v_rail.value, 9);
    expect(led.dimmed).toBe(true);
    expect(ledDraw([chain(4, 1, 1, 1)], 255, f).dimmed).toBe(false);
  });
});

describe("the loads", () => {
  it("that always draw are the power design's permanent consumers, the servo at rest among them", () => {
    expect(baseMilliamps(f)).toBeCloseTo(f.i_permanent.value, 6);
  });

  it("take each coil's current and pull time from the config, held to the driver's ceiling", () => {
    const all = coils(hw, f);
    expect(all.map((c) => c.channel)).toEqual([0, 1, 2, 3]);
    expect(all.map((c) => c.current)).toEqual(["i_coil", "i_coil", "i_coil", "i_coil_scoop"]);
    expect(all.every((c) => c.pullMs === Math.min(hw.config.pullMs[c.channel], f.t_on_max.value))).toBe(true);
    const long = { ...hw, config: { ...hw.config, pullMs: [500, 10, 30, 30] } };
    expect(coils(long, f).map((c) => c.pullMs)).toEqual([f.t_on_max.value, 10, 30, 30]);
  });

  it("count a coil for its pull time and no longer", () => {
    const [bumper] = coils(hw, f);
    let p: Pulls = new Map();
    p = fire(p, bumper, 1000);
    expect(pulling([bumper], p, 1000)).toHaveLength(1);
    expect(pulling([bumper], p, 1000 + bumper.pullMs - 1)).toHaveLength(1);
    expect(pulling([bumper], p, 1000 + bumper.pullMs)).toHaveLength(0);
  });

  it("leave their highest total readable for the peak's window", () => {
    const s: [number, number][] = [];
    expect(holdPeak(s, 0, 900, 1000)).toBe(900);
    expect(holdPeak(s, 10, 3000, 1000)).toBe(3000);
    expect(holdPeak(s, 1000, 950, 1000)).toBe(3000);
    expect(holdPeak(s, 1011, 950, 1000)).toBe(950);
  });

  it("hold the servo at its stall current for its longest move", () => {
    expect(moveServo(500, f)).toBe(500 + f.t_move_max.value);
  });

  it("add a coil's pull-in current while it pulls and the servo's stall while it moves", () => {
    const all = coils(hw, f);
    const d = draw(dark, [all[0], all[3]], true, 6, f);
    expect(d.coils).toBeCloseTo(f.i_coil.value + f.i_coil_scoop.value, 6);
    expect(d.servo).toBe(f.i_stall.value);
    expect(d.total).toBeCloseTo(d.base + d.coils + d.servo, 9);
    expect(d.servoWithCoil).toBe(true);
    expect(draw(dark, [], true, 6, f).servoWithCoil).toBe(false);
  });
});

describe("the worst cases", () => {
  // the lighting at its cap with the most coils that pull at once
  const most = (adapter: 3 | 6) => {
    const cap = adapterFigures(f, adapter).cap;
    return draw({ ma: cap, brightness: 255, dimmed: false }, mostAtOnce(coils(hw, f), f), false, adapter, f);
  };

  it("pull two bumpers and the scoop, the coils the solenoid model allows at once", () => {
    expect(mostAtOnce(coils(hw, f), f).map((c) => c.id)).toEqual(["bumper-1", "bumper-2", "ball-scoop"]);
    expect(most(6).coils).toBeCloseTo(f.i_coils_at_once.value, 6);
  });

  it("with the 3 A adapter pass its rating by the power design's figure, for one pull-in", () => {
    const d = most(3);
    expect(d.total).toBeCloseTo(f.i_most_3a.value, 6);
    expect(d.over).toBeCloseTo(f.i_over_3a.value, 6);
    const [w] = worstCases(f, 3);
    expect(w.over).toBeCloseTo(f.i_over_3a.value, 6);
    expect(w.forMs).toBe(f.t_on_max.value);
  });

  it("with the 6 A adapter stay inside its rating", () => {
    const d = most(6);
    expect(d.total).toBeCloseTo(f.i_most_6a.value, 6);
    expect(worstCases(f, 6)[0].over).toBe(0);
    expect(f.i_most_6a.value).toBeLessThanOrEqual(f.i_supply_6a.value);
  });

  it("at full white stay under the lighting's cap with either adapter", () => {
    for (const a of [3, 6] as const) {
      const white = worstCases(f, a).find((w) => w.key === "white")!;
      expect(white.ma).toBeLessThanOrEqual(white.limit);
      expect(white.over).toBe(0);
    }
  });

  it("mark a total past the adapter", () => {
    const over = draw({ ma: 2500, brightness: 255, dimmed: false }, [], false, 3, f);
    expect(over.over).toBeCloseTo(over.total - f.i_supply_3a.value, 9);
    expect(draw(dark, [], false, 3, f).over).toBe(0);
  });
});
