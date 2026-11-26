// The screen's budget: what a frame costs on the Teensy 4.1 from the pixels it sends, and the frames of
// the last second against LVGL's refresh period.
import { describe, expect, it } from "vitest";
import { budgetOf, frameCost, WINDOW_MS } from "../src/model/budget";
import { FIGURES as f } from "../src/model/current";

const SCREEN = f.n_cols.value * f.n_rows.value;

describe("a frame's cost", () => {
  it("takes the measured drawing and the bus's time for a whole screen", () => {
    const c = frameCost(SCREEN);
    expect(c.drawMs).toBeCloseTo(f.t_render.value);
    expect(c.busMs).toBeCloseTo(f.t_frame_bus.value);
    expect(c.totalMs).toBeCloseTo(f.t_render.value + f.t_frame_bus.value);
    expect(c.cpuMs).toBeCloseTo(f.t_render.value + (f.t_frame_bus.value * f.cpu_send.value) / 100);
  });

  it("scales with the pixels and costs nothing without them", () => {
    expect(frameCost(SCREEN / 4).totalMs).toBeCloseTo(frameCost(SCREEN).totalMs / 4);
    expect(frameCost(0).totalMs).toBe(0);
  });
});

describe("the budget", () => {
  it("takes the longest frame and the processor's share of the last second", () => {
    const b = budgetOf([{ ms: 100, pixels: SCREEN }, { ms: 600, pixels: SCREEN / 2 }], 1000);
    expect(b.worstMs).toBeCloseTo(frameCost(SCREEN).totalMs);
    expect(b.cpuPercent).toBeCloseTo(((frameCost(SCREEN).cpuMs + frameCost(SCREEN / 2).cpuMs) / WINDOW_MS) * 100);
    expect(b.budgetMs).toBe(f.t_refr.value);
    expect(b.over).toBe(false);
  });

  it("leaves out frames older than the window and marks one past the refresh period", () => {
    expect(budgetOf([{ ms: 0, pixels: 3 * SCREEN }], WINDOW_MS).worstMs).toBe(0);
    expect(budgetOf([{ ms: 10, pixels: 2 * SCREEN }], 500).over).toBe(true);
  });
});
