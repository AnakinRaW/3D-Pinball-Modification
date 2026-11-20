// Runs the firmware's lamps component and lighting driver, built by "npm run wasm", against the
// machine config and the drawing.
import { existsSync, readFileSync } from "node:fs";
import { resolve } from "node:path";
import { beforeEach, describe, expect, it } from "vitest";
import { CHAINS, lampSpans } from "../src/model/config";
import { hiddenStretches, type Strip } from "../src/model/playfield";
import { drawingColors, groupSetups, lightsOptions, loadLights, readChains, type LightsModule } from "../src/sim/lights";
import { sources } from "../src/sources";
import { loadModel } from "../src/state";

const model = loadModel(sources);
const { pf, hw } = model;
// the module's bytes from disk, since the test environment cannot fetch them
const wasm = resolve("src/wasm/simulator.wasm");
if (existsSync(wasm)) lightsOptions.wasmBinary = readFileSync(wasm);
const built = (await loadLights()) !== null;
const RED = { r: 255, g: 0, b: 0 };
const WHITE = { r: 255, g: 255, b: 255 };
const FRAME_MS = 10; // the lamps component hands the driver a frame every 10 ms

let m: LightsModule;

beforeEach(async () => {
  if (!built) return;
  m = (await loadLights())!;
  expect(m.setUp(hw.config.adapterA, groupSetups(hw.config))).toBe(true);
});

const lit = (chains: Uint8Array[]) => {
  const out: [number, number][] = [];
  chains.forEach((chain, c) => {
    for (let led = 0; 3 * led < chain.length; led++) {
      if (chain[3 * led] || chain[3 * led + 1] || chain[3 * led + 2]) out.push([c, led]);
    }
  });
  return out;
};

describe.runIf(built)("the firmware's lamps in the browser", () => {
  it("places each chain's LEDs as the config gives them", () => {
    m.step(FRAME_MS);
    const chains = readChains(m);
    for (const g of Object.values(hw.config.groups)) expect(chains[CHAINS.indexOf(g.chain as never)].length).toBe(3 * g.leds);
  });

  it("cuts the playfield into the lamps the simulator's walk gives, each on its chain and LED", () => {
    const spans = lampSpans(hw.config).flat();
    expect(m.playfieldLamps()).toBe(spans.length);
    for (const span of spans) {
      m.clearLamps();
      m.effect({ target: "playfield", first: span.index, count: 1, key: 1, priority: 1, kind: "set", a: RED });
      m.step(FRAME_MS);
      const on = lit(readChains(m));
      const want = Array.from({ length: span.count }, (_, i) => [CHAINS.indexOf(span.chain as never), span.first + i]);
      expect(on, `lamp ${span.index}`).toEqual(want);
    }
  });

  it("lights every second perimeter LED at the 3 A profile's ambient brightness, every one at 6 A", () => {
    const strip = pf.strips.find((s) => s.id === "lights-perimeter-left") as Strip;
    const hidden = new Set(hiddenStretches(strip).flatMap(([a, n]) => Array.from({ length: n }, (_, i) => a + i)));
    const visible = strip.leds.map((l) => l.index).filter((i) => !hidden.has(i));
    m.effect({ target: "perimeterLeft", key: 1, priority: 1, kind: "set", a: WHITE });
    m.step(FRAME_MS);
    const p3 = m.profile();
    expect(p3.perimeterStep).toBe(2);
    let chain = readChains(m)[0];
    const lit3 = visible.filter((i) => chain[3 * i] > 0);
    expect(lit3).toEqual(visible.filter((_, k) => k % 2 === 0));
    expect(chain[3 * lit3[0]]).toBe(p3.ambientMax);
    for (const i of hidden) expect(chain[3 * i]).toBe(0);

    expect(m.setAdapter(6)).toBe(true);
    m.step(FRAME_MS);
    const p6 = m.profile();
    chain = readChains(m)[0];
    expect(visible.every((i) => chain[3 * i] === p6.ambientMax)).toBe(true);
  });

  it.each([3, 6])("keeps the whole layout at full white under the %i A profile's cap", (adapter) => {
    m.setAdapter(adapter);
    m.effect({ target: "playfield", key: 1, priority: 1, kind: "set", a: WHITE });
    m.effect({ target: "ambient", key: 1, priority: 1, kind: "set", a: WHITE });
    m.step(FRAME_MS);
    expect(m.capBrightness()).toBe(255);
  });

  it("blinks on the simulated clock", () => {
    m.effect({ target: "playfield", first: 0, count: 1, key: 1, priority: 1, kind: "blink", a: RED, onMs: 100, offMs: 100, dark: true });
    const red = () => readChains(m)[CHAINS.indexOf(lampSpans(hw.config)[0][0].chain as never)][0];
    m.step(50);
    expect(red()).toBe(255);
    m.step(100);
    expect(red()).toBe(0);
    m.step(100);
    expect(red()).toBe(255);
  });

  it("shows a higher layer over a lower one and the lower one again once the higher is removed", () => {
    m.effect({ target: "playfield", key: 1, priority: 1, kind: "set", a: RED });
    m.effect({ target: "playfield", key: 2, priority: 5, kind: "set", a: { r: 0, g: 0, b: 255 } });
    m.step(FRAME_MS);
    let colors = drawingColors(pf, hw, readChains(m));
    expect(colors.get("lamp-scoop")).toEqual([0, 0, 255]);
    m.removeLayer(2);
    m.step(FRAME_MS);
    colors = drawingColors(pf, hw, readChains(m));
    expect(colors.get("lamp-scoop")).toEqual([255, 0, 0]);
  });

  it("gives every lamp and strip LED of the drawing a color", () => {
    m.step(FRAME_MS);
    const colors = drawingColors(pf, hw, readChains(m));
    for (const l of pf.lamps) expect(colors.has(l.id), l.id).toBe(true);
    for (const s of pf.strips) for (const led of s.leds) expect(colors.has(led.id), led.id).toBe(true);
  });
});

if (!built) it.skip("the firmware's lamps need npm run wasm", () => {});
