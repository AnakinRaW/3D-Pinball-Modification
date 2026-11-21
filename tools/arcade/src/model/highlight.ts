// What the drawing shows for a selected feature: the parts it marks and labels, the lamps it lights, the
// state layers it reveals and the parts above it that turn see-through. Then where each label goes.
import { lampsOn, seeThrough, shortName, type Playfield } from "./playfield";

export interface Mark {
  id: string; // the element of the drawing the mark sits on
  label: string;
}

export interface Highlight {
  marks: Mark[];
  lit: string[]; // lamps, LEDs and strips that light
  reveal: string[]; // placements the drawing shows only in a state, such as a ball at rest
  through: string[]; // parts above the targets that turn see-through
  hidden: string[]; // covers over the targets that the drawing leaves out
}

// covers maps a cover part to the parts under it, reveals a part to the balls at rest shown with it,
// as config/machine.yml gives both
export function planHighlight(
  pf: Playfield,
  targets: string[],
  covers: Record<string, string[]> = {},
  reveals: Record<string, string[]> = {},
): Highlight {
  const marks: Mark[] = [];
  const labels = new Set<string>();
  const lit = new Set<string>();
  const reveal: string[] = [];
  for (const id of targets) {
    const item = pf.items.get(id);
    if (!item) continue;
    const label = shortName(pf, id);
    // the LEDs of one strip share a label, so it stands once
    if (!(item.kind === "led" && labels.has(label))) marks.push({ id, label });
    labels.add(label);
    if (item.kind === "led") lit.add(item.strip);
    if (item.kind === "lamp" || item.kind === "led" || item.kind === "strip") lit.add(id);
    if (item.kind === "placement") {
      lampsOn(pf, id).forEach((l) => lit.add(l.id));
      if (item.hidden) reveal.push(id);
      for (const ball of reveals[id] ?? []) if (!reveal.includes(ball)) reveal.push(ball);
    }
  }
  const hidden = Object.keys(covers).filter((c) => covers[c].some((p) => targets.includes(p)));
  return { marks, lit: [...lit], reveal, through: seeThrough(pf, targets), hidden };
}

// a mark measured in the drawing: its circle around the part
export interface Ring {
  id: string;
  label: string;
  cx: number;
  cy: number;
  r: number;
}

export interface PlacedLabel extends Ring {
  x: number; // where the text starts, or ends on the left side
  y: number; // the text's baseline
  anchor: "start" | "end";
  width: number;
}

export const LABEL_FONT = 7; // mm
const CHAR_WIDTH = 0.56 * LABEL_FONT; // an estimate for a sans-serif font
const LINE = LABEL_FONT + 3; // mm between two labels' baselines
const PAD = 3; // mm between a ring and its label

export function labelWidth(text: string): number {
  return text.length * CHAR_WIDTH + 2 * 1.5;
}

// Places the labels in columns beside the rings, so no label covers a ring. The rings on the playfield's
// left half take their labels left of the leftmost ring, those on its right half right of the rightmost
// ring. A label with no room there goes to the inner side of its half's rings instead. Labels of one
// column that would overlap move apart.
export function placeLabels(rings: Ring[], viewBox: [number, number, number, number], middle: number): PlacedLabel[] {
  const [vx, vy, vw, vh] = viewBox;
  const halves = [rings.filter((g) => g.cx < middle), rings.filter((g) => g.cx >= middle)];
  const outer = [Math.min(...halves[0].map((g) => g.cx - g.r)) - PAD, Math.max(...halves[1].map((g) => g.cx + g.r)) + PAD];
  const inner = [Math.max(...halves[0].map((g) => g.cx + g.r)) + PAD, Math.min(...halves[1].map((g) => g.cx - g.r)) - PAD];
  const placed: PlacedLabel[] = rings.map((g) => {
    const width = labelWidth(g.label);
    const y = g.cy + LABEL_FONT / 3;
    if (g.cx < middle) {
      if (outer[0] - width >= vx) return { ...g, x: outer[0], y, anchor: "end", width };
      return { ...g, x: inner[0], y, anchor: "start", width };
    }
    if (outer[1] + width <= vx + vw) return { ...g, x: outer[1], y, anchor: "start", width };
    return { ...g, x: inner[1], y, anchor: "end", width };
  });
  const span = (p: PlacedLabel): [number, number] => (p.anchor === "end" ? [p.x - p.width, p.x] : [p.x, p.x + p.width]);
  const order = [...placed].sort((a, b) => a.y - b.y);
  // a label pushed down can meet another one, so the passes repeat until none moves
  for (let pass = 0, moved = true; moved && pass < order.length; pass++) {
    moved = false;
    for (let k = 0; k < order.length; k++) {
      for (let j = 0; j < k; j++) {
        const [a0, a1] = span(order[k]);
        const [b0, b1] = span(order[j]);
        const apart = a1 < b0 || b1 < a0;
        if (!apart && Math.abs(order[k].y - order[j].y) < LINE - 1e-9) {
          order[k].y = order[j].y + LINE;
          moved = true;
        }
      }
    }
  }
  for (const p of placed) p.y = Math.min(Math.max(p.y, vy + LABEL_FONT), vy + vh - 2);
  return placed;
}
