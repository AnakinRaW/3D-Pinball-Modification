// The widths of the studio's side panels, which a drag on the bar between a panel and the drawing
// changes. This browser remembers them.
import { signal } from "@preact/signals";

export const PANEL_MIN_PX = 220;
export const PANEL_MAX_PX = 560;
const KEY = "pinball-arcade-studio-panels";

function stored(): { left: number; right: number } {
  try {
    const v = JSON.parse(localStorage.getItem(KEY) ?? "null");
    if (v && typeof v.left === "number" && typeof v.right === "number") return v;
  } catch {
    // no storage, or nothing stored
  }
  return { left: 300, right: 380 };
}

export const panels = signal(stored());

const fit = (px: number) => Math.round(Math.min(PANEL_MAX_PX, Math.max(PANEL_MIN_PX, px)));

// Sets a panel's width, held between PANEL_MIN_PX and PANEL_MAX_PX, and remembers both widths.
export function setPanel(side: "left" | "right", px: number) {
  panels.value = { ...panels.value, [side]: fit(px) };
  try {
    localStorage.setItem(KEY, JSON.stringify(panels.value));
  } catch {
    // a browser without storage keeps the widths until the page reloads
  }
}

// The bar between a side panel and the drawing. A drag moves it, a double click restores the panel's
// first width.
export function Splitter({ side }: { side: "left" | "right" }) {
  const start = (e: MouseEvent) => {
    e.preventDefault();
    const x0 = e.clientX;
    const w0 = panels.value[side];
    const move = (m: MouseEvent) => setPanel(side, side === "left" ? w0 + m.clientX - x0 : w0 - (m.clientX - x0));
    const end = () => {
      window.removeEventListener("mousemove", move);
      window.removeEventListener("mouseup", end);
      document.body.classList.remove("dragging");
    };
    document.body.classList.add("dragging");
    window.addEventListener("mousemove", move);
    window.addEventListener("mouseup", end);
  };
  return (
    <div
      class="splitter"
      data-splitter={side}
      role="separator"
      aria-orientation="vertical"
      onMouseDown={start}
      onDblClick={() => setPanel(side, side === "left" ? 300 : 380)}
    />
  );
}
