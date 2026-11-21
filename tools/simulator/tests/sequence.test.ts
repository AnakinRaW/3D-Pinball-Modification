// Reads .lights files and plays them on the firmware's lamps, built by "npm run wasm".
import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import { beforeEach, describe, expect, it } from "vitest";
import { drawingColors, groupSetups, lightsOptions, loadLights, readChains, type EffectSpec, type LightsModule } from "../src/sim/lights";
import { advanceWith, lampNames, parseSequence, SequencePlayer } from "../src/sim/sequence";
import { sources } from "../src/sources";
import { loadModel } from "../src/state";

const model = loadModel(sources);
const { pf, hw } = model;
const names = lampNames(pf, hw);
const attract = readFileSync(resolve("lights/attract.lights"), "utf8");

describe("the .lights format", () => {
  it("reads the attract sequence without an error", () => {
    const { sequence, errors } = parseSequence(attract, names);
    expect(errors).toEqual([]);
    expect(sequence!.lengthMs).toBe(8000);
    expect(sequence!.tracks).toBe(3);
  });

  it("counts a relative time from the step before", () => {
    const { sequence } = parseSequence(attract, names);
    const star200 = sequence!.steps.find((s) => s.spec?.first === hw.spans.get("lamp-star-200")!.index && s.spec?.count === 1)!;
    expect(star200.atMs).toBe(620);
  });

  it("names every lights group, every lamp and the whole playfield, each as one row of lamps", () => {
    expect(names.get("playfield")!.length).toBe(hw.spans.size);
    for (const g of ["stars", "arrows", "top-lanes", "bumper-section", "inlanes", "shooter", "drain", "scoop", "targets"]) {
      const lamps = names.get(g)!;
      expect(lamps, g).toBeDefined();
      expect(lamps[lamps.length - 1] - lamps[0] + 1, g).toBe(lamps.length);
    }
  });

  it("reports what it cannot read", () => {
    const bad = `steps:
  - {at: 0, lamps: nowhere, effect: set, a: red}
  - {at: 0, lamps: stars, effect: set, a: purple}
  - {at: 0, lamps: stars, effect: run, a: red}
  - {at: 0, strip: roof, effect: set, a: red}
  - {at: soon, lamps: stars, effect: set, a: red}
  - {at: 0, lamps: stars, effect: sparkle}`;
    const { sequence, errors } = parseSequence(bad, names);
    expect(sequence).toBeNull();
    expect(errors).toEqual([
      'step 1: no lamps "nowhere"',
      "step 1: names no lamps and no strip",
      'step 2: no color "purple"',
      "step 3: a run needs ms_per_lamp",
      'step 4: no strip "roof"',
      "step 4: names no lamps and no strip",
      'step 5: at is a number of ms or "+ms"',
      'step 6: no effect "sparkle"',
    ]);
  });

  it("refuses a group whose lamps lie in no single row", () => {
    const split = new Map(names);
    split.set("corners", [0, 5]);
    const { errors } = parseSequence("steps:\n  - {at: 0, lamps: corners, effect: set, a: red}", split);
    expect(errors).toEqual(["step 1: the lamps of corners lie in no single row"]);
  });
});

const wasm = resolve("src/wasm/simulator.wasm");
let built = false;
try {
  lightsOptions.wasmBinary = readFileSync(wasm);
  built = (await loadLights()) !== null;
} catch {
  built = false;
}

describe.runIf(built)("playing a sequence on the firmware's lamps", () => {
  let m: LightsModule;
  beforeEach(async () => {
    m = (await loadLights())!;
    m.setUp(hw.config.adapterA, groupSetups(hw.config));
  });
  const color = (id: string) => drawingColors(pf, hw, readChains(m)).get(id)!;

  it("starts every step in the pass of its time, round after round", () => {
    const { sequence } = parseSequence(attract, names);
    const issued: number[] = [];
    const spy: LightsModule = { ...m, effect: (spec: EffectSpec) => (issued.push(m.nowMs()), m.effect(spec)) } as LightsModule;
    const startMs = m.nowMs();
    const player = new SequencePlayer(spy, sequence!, 200, 10);
    player.start();
    for (let i = 0; i < 170; i++) advanceWith(m, player, 100);
    const due = sequence!.steps.filter((s) => s.spec).map((s) => startMs + s.atMs);
    expect(issued.slice(0, due.length)).toEqual(due);
    expect(issued.slice(due.length, 2 * due.length)).toEqual(due.map((t) => t + sequence!.lengthMs));
  });

  it("shows the counted stars above the background and clears them for the next round", () => {
    const player = new SequencePlayer(m, parseSequence(attract, names).sequence!, 200, 10);
    player.start();
    advanceWith(m, player, 700);
    expect(color("lamp-star-200")).toEqual([255, 120, 0]);
    advanceWith(m, player, 6000 - 700 + 50);
    expect(["255,0,0", "255,120,0"]).toContain(color("lamp-scoop").join(","));
    advanceWith(m, player, 8000 - 6050 + 100);
    expect(color("lamp-star-1000")).not.toEqual([255, 255, 255]);
  });

  it("plays the whole attract sequence with no effect refused", () => {
    const player = new SequencePlayer(m, parseSequence(attract, names).sequence!, 200, 10);
    player.start();
    for (let i = 0; i < 20; i++) advanceWith(m, player, 1000);
    expect(m.refused()).toBe(0);
    player.stop();
    advanceWith(m, null, 20);
    expect([...drawingColors(pf, hw, readChains(m)).values()].every(([r, g, b]) => r + g + b === 0)).toBe(true);
  });
});
