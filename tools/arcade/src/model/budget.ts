// What the screen's frames cost on the Teensy 4.1, estimated from the pixels the simulated screen
// sends. LVGL draws them at the rate measured for a whole screen drawn from the PSRAM, and the bus
// sends them at the rate the display's model gives a whole screen. The processor spends the drawing
// and its share of the sending. Every pixel counts once at that rate, so a page drawn over a running
// game, whose screen LVGL draws beneath it, costs more than this gives. Every figure comes from
// config/figures.json, which scripts/figures.py writes from the repository's models.
import { FIGURES, type Figures } from "./current";

// a pass of the simulated screen that sent pixels to the display
export interface Frame {
  ms: number;
  pixels: number;
}

export interface FrameCost {
  drawMs: number; // LVGL draws the pixels
  busMs: number; // the bus sends them
  totalMs: number; // the frame from the first pixel drawn to the last one sent
  cpuMs: number; // the processor's part: the drawing and its share of the sending
}

// the frames of the last WINDOW_MS against the budget
export interface Budget {
  worstMs: number; // the longest frame
  budgetMs: number; // LVGL's refresh period, which a frame has to fit in
  cpuPercent: number; // the processor's share the frames took
  over: boolean; // a frame did not fit
}

export const WINDOW_MS = 1000;

export function frameCost(pixels: number, f: Figures = FIGURES): FrameCost {
  const screen = pixels / (f.n_cols.value * f.n_rows.value);
  const drawMs = screen * f.t_render.value;
  const busMs = screen * f.t_frame_bus.value;
  return { drawMs, busMs, totalMs: drawMs + busMs, cpuMs: drawMs + (busMs * f.cpu_send.value) / 100 };
}

// Weighs the frames of the WINDOW_MS up to nowMs against LVGL's refresh period.
export function budgetOf(frames: Frame[], nowMs: number, f: Figures = FIGURES): Budget {
  let worstMs = 0;
  let cpuMs = 0;
  for (const fr of frames) {
    if (fr.ms <= nowMs - WINDOW_MS || fr.ms > nowMs) continue;
    const c = frameCost(fr.pixels, f);
    worstMs = Math.max(worstMs, c.totalMs);
    cpuMs += c.cpuMs;
  }
  const budgetMs = f.t_refr.value;
  return { worstMs, budgetMs, cpuPercent: (cpuMs / WINDOW_MS) * 100, over: worstMs > budgetMs };
}
