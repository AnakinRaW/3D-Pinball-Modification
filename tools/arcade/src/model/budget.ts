// What the screen's frames cost on the Teensy 4.1, estimated from the pixels the simulated screen
// sends. LVGL draws them at the rate measured for a whole screen of the costliest page, the
// sunburst turning, and the bus sends them at the rate measured for a whole screen. LVGL draws the
// next frame while the bus sends one, so a frame takes the longer of the two. The processor spends
// the drawing and its share of the sending, and beside them the lamps of the whole layout and the
// four sound channels. The IR sensing's interrupts, the driver tick, videos and the card's writes
// stay out, since no bench run has measured them beside the screen. Every figure comes from
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
  totalMs: number; // the frame, the drawing and the sending side by side
  cpuMs: number; // the processor's part: the drawing and its share of the sending
}

// the frames of the last WINDOW_MS against the budget
export interface Budget {
  worstMs: number; // the longest frame
  budgetMs: number; // LVGL's refresh period, which a frame has to fit in
  cpuPercent: number; // the processor's share the frames, the lamps and the sounds took
  over: boolean; // a frame did not fit
}

export const WINDOW_MS = 1000;

export function frameCost(pixels: number, f: Figures = FIGURES): FrameCost {
  const screen = pixels / (f.n_cols.value * f.n_rows.value);
  const drawMs = screen * f.t_render_rays.value;
  const busMs = screen * f.t_frame_bus_bench.value;
  return { drawMs, busMs, totalMs: Math.max(drawMs, busMs), cpuMs: drawMs + (busMs * f.cpu_send_bench.value) / 100 };
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
  const cpuPercent = (cpuMs / WINDOW_MS) * 100 + f.lamps_cpu.value + f.audio_cpu_bench.value;
  return { worstMs, budgetMs, cpuPercent, over: worstMs > budgetMs };
}
