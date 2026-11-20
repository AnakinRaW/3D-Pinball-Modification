// The drawing of the machine, playfield.svg inline, with an overlay that dims it around the marked parts and
// labels them. A click on a part opens its card. In the studio a canvas over it paints the colors the
// firmware's lamps show, and a click picks the effect's target.
import { useSignalEffect } from "@preact/signals";
import { useEffect, useMemo, useRef, useState } from "preact/hooks";
import { resolveItem } from "../model/part-card";
import { targetOf } from "./Studio";
import type { Point } from "../model/playfield";
import { placeLabels, LABEL_FONT, type Ring } from "../model/highlight";
import type { Model, State } from "../state";

// the playfield's inner walls are 230 mm apart, so its middle lies at 115 mm
const MIDDLE = 115;
const MIN_RING = 4; // mm
const GLOW_HOLE = 3.5; // mm around a lit LED that the darkening leaves out
const LAMP_NUMBER_FONT = 4.2; // mm, the playfield lamp's number in the studio
const GLOW_SPREAD = 1.4; // how far a lamp's glow reaches, in its radius
const GLOW_MIN_MM = 4.5; // the reach of the smallest glow, so a strip LED stays visible

// The composed SVG without its XML declaration and its fixed size, so it fills its box.
export function prepareSvg(svg: string): string {
  return svg
    .replace(/<\?xml[^>]*\?>\s*/, "")
    .replace(/<svg([^>]*?)\swidth="[^"]*"/, "<svg$1")
    .replace(/<svg([^>]*?)\sheight="[^"]*"/, "<svg$1")
    .replace(/<svg /, '<svg class="drawing" preserveAspectRatio="xMidYMid meet" ');
}

function byId(root: Element, id: string): Element | null {
  return root.querySelector(`[id="${id}"]`);
}

// The circle around an element, in the drawing's millimeters. Where the browser cannot measure, as in a
// test, the circle sits on the part's anchor.
export function measure(root: SVGSVGElement, el: Element | null, fallback: Point): { cx: number; cy: number; r: number } {
  const g = el as SVGGraphicsElement | null;
  try {
    const bb = g?.getBBox?.();
    const rootCtm = root.getScreenCTM?.();
    const elCtm = g?.getScreenCTM?.();
    if (bb && rootCtm && elCtm && (bb.width > 0 || bb.height > 0)) {
      const m = rootCtm.inverse().multiply(elCtm);
      const xs: number[] = [];
      const ys: number[] = [];
      for (const [x, y] of [
        [bb.x, bb.y],
        [bb.x + bb.width, bb.y],
        [bb.x, bb.y + bb.height],
        [bb.x + bb.width, bb.y + bb.height],
      ]) {
        xs.push(m.a * x + m.c * y + m.e);
        ys.push(m.b * x + m.d * y + m.f);
      }
      const [x0, x1, y0, y1] = [Math.min(...xs), Math.max(...xs), Math.min(...ys), Math.max(...ys)];
      const r = Math.max(MIN_RING, Math.hypot(x1 - x0, y1 - y0) / 2 + 1.5);
      return { cx: (x0 + x1) / 2, cy: (y0 + y1) / 2, r };
    }
  } catch {
    // an element that is not drawn has no box
  }
  return { cx: fallback[0], cy: fallback[1], r: 6 };
}

const CLASSES = ["sim-lit", "sim-reveal", "sim-through", "sim-hidden", "sim-target", "sim-inspected"];

function idsUp(target: EventTarget | null, stop: Element): string[] {
  const ids: string[] = [];
  for (let e = target as Element | null; e && e !== stop; e = e.parentElement) if (e.id) ids.push(e.id);
  return ids;
}

export function Drawing({ model, state }: { model: Model; state: State }) {
  const host = useRef<HTMLDivElement>(null);
  const markup = useMemo(() => ({ __html: prepareSvg(model.svg) }), [model.svg]);
  const [rings, setRings] = useState<Ring[]>([]);
  // the revealed balls, which the darkening leaves bright without a ring or a label
  const [shown, setShown] = useState<Ring[]>([]);
  const [inspectRing, setInspectRing] = useState<Ring | null>(null);
  const hl = state.highlight.value;
  const studio = state.view.value === "studio";
  const inspected = studio ? (state.picked.value?.id ?? null) : state.inspected.value;
  const { pf } = model;
  const canvas = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const root = host.current?.querySelector("svg.drawing") as SVGSVGElement | null;
    if (!root) return;
    root.querySelectorAll(CLASSES.map((c) => `.${c}`).join(",")).forEach((e) => e.classList.remove(...CLASSES));
    root.classList.toggle("sim-features", !studio);
    root.classList.toggle("sim-studio", studio);
    const anchorOf = (id: string): Point => {
      const it = pf.items.get(id);
      return it && "point" in it ? it.point : [MIDDLE, 0];
    };
    if (studio) {
      state.hiddenRamps.value.forEach((id) => byId(root, id)?.classList.add("sim-hidden"));
      setRings([]);
      setShown([]);
      setInspectRing(inspected ? { id: inspected, label: "", ...measure(root, byId(root, inspected), anchorOf(inspected)) } : null);
      return;
    }
    const add = (ids: string[], cls: string) => ids.forEach((id) => byId(root, id)?.classList.add(cls));
    add(hl.lit, "sim-lit");
    add(hl.reveal, "sim-reveal");
    add(hl.through, "sim-through");
    add(hl.hidden, "sim-hidden");
    add(
      hl.marks.map((m) => m.id),
      "sim-target",
    );
    setRings(hl.marks.map((m) => ({ id: m.id, label: m.label, ...measure(root, byId(root, m.id), anchorOf(m.id)) })));
    const marked = new Set(hl.marks.map((m) => m.id));
    setShown(hl.reveal.filter((id) => !marked.has(id)).map((id) => ({ id, label: "", ...measure(root, byId(root, id), anchorOf(id)) })));
    if (inspected) {
      byId(root, inspected)?.classList.add("sim-inspected");
      setInspectRing({ id: inspected, label: "", ...measure(root, byId(root, inspected), anchorOf(inspected)) });
    } else setInspectRing(null);
  }, [hl, inspected, pf, studio, state.hiddenRamps.value]);

  const onClick = (e: MouseEvent) => {
    if (!host.current) return;
    const item = resolveItem(pf, idsUp(e.target, host.current));
    if (state.view.value === "studio") {
      // a click on anything that takes no effect, the wood or the cabinet included, clears the pick
      state.picked.value = item && targetOf(model, item) ? item : null;
      return;
    }
    if (item) state.inspected.value = item.id;
  };

  // paints the glow of every lamp and LED in the color the firmware gives it
  const paint = () => {
    const c = canvas.current;
    const colors = state.glow.value;
    if (!c) return;
    const dpr = globalThis.devicePixelRatio || 1;
    const w = Math.round(c.clientWidth * dpr);
    const h = Math.round(c.clientHeight * dpr);
    if (c.width !== w || c.height !== h) {
      c.width = w;
      c.height = h;
    }
    const ctx = c.getContext?.("2d");
    if (!ctx) return;
    ctx.clearRect(0, 0, w, h);
    if (!colors) return;
    // the drawing keeps its proportions in its box, centered, as preserveAspectRatio meet places it
    const [vx, vy, vw, vh] = pf.viewBox;
    const scale = Math.min(w / vw, h / vh);
    const ox = (w - vw * scale) / 2;
    const oy = (h - vh * scale) / 2;
    // overlapping glows add up like light, short of turning white
    ctx.globalCompositeOperation = "screen";
    const glowAt = (p: Point, r: number, rgb: [number, number, number]) => {
      if (rgb[0] + rgb[1] + rgb[2] === 0) return;
      const x = ox + (p[0] - vx) * scale;
      const y = oy + (p[1] - vy) * scale;
      const radius = Math.max(r * GLOW_SPREAD, GLOW_MIN_MM) * scale;
      const g = ctx.createRadialGradient(x, y, 0, x, y, radius);
      const [cr, cg, cb] = rgb;
      g.addColorStop(0, `rgba(${cr},${cg},${cb},1)`);
      g.addColorStop(0.45, `rgba(${cr},${cg},${cb},0.6)`);
      g.addColorStop(1, `rgba(${cr},${cg},${cb},0)`);
      ctx.fillStyle = g;
      ctx.fillRect(x - radius, y - radius, 2 * radius, 2 * radius);
    };
    for (const l of pf.lamps) {
      const rgb = colors.get(l.id);
      if (rgb) glowAt(l.point, l.r, rgb);
    }
    for (const s of pf.strips) {
      for (const led of s.leds) {
        const rgb = colors.get(led.id);
        if (rgb && !led.hidden) glowAt(led.point, led.r, rgb);
      }
    }
  };
  useSignalEffect(paint);
  useEffect(() => {
    const c = canvas.current;
    if (!c || typeof ResizeObserver === "undefined") return;
    const observer = new ResizeObserver(() => paint());
    observer.observe(c);
    return () => observer.disconnect();
  }, []);

  const vb = pf.viewBox;
  const labels = placeLabels(rings, vb, MIDDLE);
  // the darkening leaves every lit lamp and every visible LED of a lit strip bright
  const glows = useMemo(() => {
    const pts: Point[] = [];
    for (const id of hl.lit) {
      const it = pf.items.get(id);
      if (it?.kind === "strip") it.leds.filter((l) => !l.hidden).forEach((l) => pts.push(l.point));
      else if (it?.kind === "lamp") pts.push(it.point);
    }
    return pts;
  }, [hl, pf]);
  const big = { x: vb[0] - 50, y: vb[1] - 50, width: vb[2] + 100, height: vb[3] + 100 };
  return (
    <div class="stage">
      <div class="drawing-host" ref={host} onClick={onClick} dangerouslySetInnerHTML={markup} />
      <canvas class="glow" ref={canvas} aria-hidden="true" />
      <svg class="overlay" viewBox={vb.join(" ")} preserveAspectRatio="xMidYMid meet" aria-hidden="true">
        {rings.length > 0 && (
          <>
            <defs>
              <mask id="sim-dim" maskUnits="userSpaceOnUse" {...big}>
                <rect {...big} fill="#fff" />
                {[...rings, ...shown].map((g) => (
                  <circle key={g.id} cx={g.cx} cy={g.cy} r={g.r + 1.5} fill="#000" />
                ))}
                {glows.map(([x, y], i) => (
                  <circle key={`glow-${i}`} class="glow-hole" cx={x} cy={y} r={GLOW_HOLE} fill="#000" />
                ))}
              </mask>
            </defs>
            <rect class="dim" {...big} mask="url(#sim-dim)" />
          </>
        )}
        {labels.map((p) => {
          const edge = p.anchor === "end" ? p.cx - p.r : p.cx + p.r;
          const plateX = p.anchor === "end" ? p.x - p.width : p.x;
          return (
            <g key={p.id} class="mark" data-mark={p.id}>
              <circle class="ring" cx={p.cx} cy={p.cy} r={p.r} />
              <line class="leader" x1={edge} y1={p.cy} x2={p.x} y2={p.y - LABEL_FONT / 3} />
              <rect class="plate" x={plateX} y={p.y - LABEL_FONT} width={p.width} height={LABEL_FONT + 2} rx={1.5} />
              <text class="label" x={p.anchor === "end" ? p.x - 1.5 : p.x + 1.5} y={p.y} text-anchor={p.anchor} font-size={LABEL_FONT}>
                {p.label}
              </text>
            </g>
          );
        })}
        {inspectRing && <circle class="inspect-ring" cx={inspectRing.cx} cy={inspectRing.cy} r={inspectRing.r + 1} />}
        {studio && (
          <g class="lamp-numbers">
            {pf.lamps.map((l) => {
              const span = model.hw.spans.get(l.id);
              if (!span) return null;
              return (
                <text key={l.id} data-lamp={l.id} x={l.point[0]} y={l.point[1] + LAMP_NUMBER_FONT / 3} font-size={LAMP_NUMBER_FONT}>
                  {span.index}
                </text>
              );
            })}
          </g>
        )}
      </svg>
    </div>
  );
}
