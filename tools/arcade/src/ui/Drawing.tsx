// The drawing of the machine, playfield.svg inline, with an overlay that rings the marked parts in a soft
// halo and labels them. A click on a part selects the feature that shows it, or opens the part's card
// where no feature does. In the studio a canvas over it paints the colors the firmware's lamps show, and
// a click picks the effect's target. The wheel zooms around the pointer and a drag pans.
import { useSignalEffect } from "@preact/signals";
import { useEffect, useMemo, useRef, useState } from "preact/hooks";
import { featureAt } from "../model/features";
import { resolveItem } from "../model/part-card";
import { RampSwitch } from "./RampSwitch";
import { targetOf } from "./Studio";
import type { Point } from "../model/playfield";
import { placeLabels, LABEL_FONT, type Ring } from "../model/highlight";
import type { Model, State } from "../state";

// the playfield's inner walls are 230 mm apart, so its middle lies at 115 mm
const MIDDLE = 115;
const MIN_RING = 4; // mm
const FRAME = "cabinet"; // the frame around the playfield, which a click treats as empty space
const LAMP_NUMBER_FONT = 4.2; // mm, the playfield lamp's number in the studio
const GLOW_SPREAD = 1.4; // how far a lamp's glow reaches, in its radius
const GLOW_MIN_MM = 4.5; // the reach of the smallest glow, so a strip LED stays visible
const ZOOM_MAX = 8;
const ZOOM_STEP = 1.0015; // the zoom factor for one pixel of wheel travel
const DRAG_PX = 4; // a press that moves less than this is a click, not a pan

export interface View {
  s: number;
  x: number;
  y: number;
}
const HOME: View = { s: 1, x: 0, y: 0 };

// The view held inside the stage: no zoom below 1, and no pan that pulls the drawing off an edge.
export function clampView(v: View, w: number, h: number): View {
  const s = Math.min(ZOOM_MAX, Math.max(1, v.s));
  if (s === 1) return HOME;
  return {
    s,
    x: Math.min(0, Math.max(w * (1 - s), v.x)),
    y: Math.min(0, Math.max(h * (1 - s), v.y)),
  };
}

// The view zoomed by a factor around a point of the stage, which stays where it is on screen.
export function zoomAt(
  v: View,
  factor: number,
  px: number,
  py: number,
  w: number,
  h: number,
): View {
  const s = Math.min(ZOOM_MAX, Math.max(1, v.s * factor));
  const k = s / v.s;
  return clampView({ s, x: px - (px - v.x) * k, y: py - (py - v.y) * k }, w, h);
}

// The composed SVG without its XML declaration and its fixed size, so it fills its box.
export function prepareSvg(svg: string): string {
  return svg
    .replace(/<\?xml[^>]*\?>\s*/, "")
    .replace(/<svg([^>]*?)\swidth="[^"]*"/, "<svg$1")
    .replace(/<svg([^>]*?)\sheight="[^"]*"/, "<svg$1")
    .replace(
      /<svg /,
      '<svg class="drawing" preserveAspectRatio="xMidYMid meet" ',
    );
}

function byId(root: Element, id: string): Element | null {
  return root.querySelector(`[id="${id}"]`);
}

// The circle around an element, in the drawing's millimeters. Where the browser cannot measure, as in a
// test, the circle sits on the part's anchor.
export function measure(
  root: SVGSVGElement,
  el: Element | null,
  fallback: Point,
): { cx: number; cy: number; r: number } {
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
      const [x0, x1, y0, y1] = [
        Math.min(...xs),
        Math.max(...xs),
        Math.min(...ys),
        Math.max(...ys),
      ];
      const r = Math.max(MIN_RING, Math.hypot(x1 - x0, y1 - y0) / 2 + 1.5);
      return { cx: (x0 + x1) / 2, cy: (y0 + y1) / 2, r };
    }
  } catch {
    // an element that is not drawn has no box
  }
  return { cx: fallback[0], cy: fallback[1], r: 6 };
}

const CLASSES = [
  "sim-lit",
  "sim-reveal",
  "sim-through",
  "sim-hidden",
  "sim-target",
  "sim-inspected",
];

function idsUp(target: EventTarget | null, stop: Element): string[] {
  const ids: string[] = [];
  for (let e = target as Element | null; e && e !== stop; e = e.parentElement)
    if (e.id) ids.push(e.id);
  return ids;
}

export function Drawing({ model, state }: { model: Model; state: State }) {
  const host = useRef<HTMLDivElement>(null);
  const markup = useMemo(
    () => ({ __html: prepareSvg(model.svg) }),
    [model.svg],
  );
  const [rings, setRings] = useState<Ring[]>([]);
  const [inspectRing, setInspectRing] = useState<Ring | null>(null);
  const hl = state.highlight.value;
  const studio = state.view.value === "studio";
  // the Games view shows the machine as it stands, with no feature marked
  const plain = state.view.value === "games";
  const inspected = studio
    ? (state.picked.value?.id ?? null)
    : state.inspected.value;
  const { pf } = model;
  const canvas = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const root = host.current?.querySelector(
      "svg.drawing",
    ) as SVGSVGElement | null;
    if (!root) return;
    root
      .querySelectorAll(CLASSES.map((c) => `.${c}`).join(","))
      .forEach((e) => e.classList.remove(...CLASSES));
    // the lamps dim around a feature only while one is picked
    root.classList.toggle(
      "sim-features",
      !studio && !plain && state.feature.value !== undefined,
    );
    root.classList.toggle("sim-studio", studio);
    const anchorOf = (id: string): Point => {
      const it = pf.items.get(id);
      return it && "point" in it ? it.point : [MIDDLE, 0];
    };
    // the ramps the switch bar hides, in every view
    state.hiddenRamps.value.forEach((id) =>
      byId(root, id)?.classList.add("sim-hidden"),
    );
    if (plain) {
      setRings([]);
      setInspectRing(null);
      return;
    }
    if (studio) {
      setRings([]);
      setInspectRing(
        inspected
          ? {
              id: inspected,
              label: "",
              ...measure(root, byId(root, inspected), anchorOf(inspected)),
            }
          : null,
      );
      return;
    }
    const add = (ids: string[], cls: string) =>
      ids.forEach((id) => byId(root, id)?.classList.add(cls));
    add(hl.lit, "sim-lit");
    add(hl.reveal, "sim-reveal");
    add(hl.through, "sim-through");
    add(hl.hidden, "sim-hidden");
    add(
      hl.marks.map((m) => m.id),
      "sim-target",
    );
    setRings(
      hl.marks.map((m) => ({
        id: m.id,
        label: m.label,
        ...measure(root, byId(root, m.id), anchorOf(m.id)),
      })),
    );
    if (inspected) {
      byId(root, inspected)?.classList.add("sim-inspected");
      setInspectRing({
        id: inspected,
        label: "",
        ...measure(root, byId(root, inspected), anchorOf(inspected)),
      });
    } else setInspectRing(null);
  }, [hl, inspected, pf, studio, plain, state.hiddenRamps.value]);

  const stage = useRef<HTMLDivElement>(null);
  const [zoom, setZoom] = useState<View>(HOME);
  const dragged = useRef(false);
  const size = () => {
    const r = stage.current?.getBoundingClientRect();
    return {
      w: r?.width ?? 0,
      h: r?.height ?? 0,
      left: r?.left ?? 0,
      top: r?.top ?? 0,
    };
  };
  useEffect(() => {
    const el = stage.current;
    if (!el) return;
    // the page itself must not scroll while the wheel zooms
    const wheel = (e: WheelEvent) => {
      e.preventDefault();
      const { w, h, left, top } = size();
      setZoom((v) =>
        zoomAt(
          v,
          Math.pow(ZOOM_STEP, -e.deltaY),
          e.clientX - left,
          e.clientY - top,
          w,
          h,
        ),
      );
    };
    el.addEventListener("wheel", wheel, { passive: false });
    return () => el.removeEventListener("wheel", wheel);
  }, []);
  const startPan = (e: PointerEvent) => {
    if (
      e.button !== 0 ||
      (e.target as Element).closest?.(".ramp-switch, .zoom-controls")
    )
      return;
    dragged.current = false;
    const x0 = e.clientX;
    const y0 = e.clientY;
    const from = zoom;
    const move = (m: PointerEvent) => {
      const dx = m.clientX - x0;
      const dy = m.clientY - y0;
      if (!dragged.current && Math.hypot(dx, dy) < DRAG_PX) return;
      dragged.current = true;
      const { w, h } = size();
      setZoom(clampView({ ...from, x: from.x + dx, y: from.y + dy }, w, h));
    };
    const end = () => {
      window.removeEventListener("pointermove", move);
      window.removeEventListener("pointerup", end);
    };
    window.addEventListener("pointermove", move);
    window.addEventListener("pointerup", end);
  };
  const zoomBy = (factor: number) => {
    const { w, h } = size();
    setZoom((v) => zoomAt(v, factor, w / 2, h / 2, w, h));
  };

  const onClick = (e: MouseEvent) => {
    if (!host.current) return;
    // the release at the end of a pan selects nothing
    if (dragged.current) {
      dragged.current = false;
      return;
    }
    const item = resolveItem(pf, idsUp(e.target, host.current));
    if (state.view.value === "studio") {
      // a click on anything that takes no effect, the wood or the cabinet included, clears the pick
      state.picked.value = item && targetOf(model, item) ? item : null;
      return;
    }
    if (plain) return;
    // a click on a part selects the feature that shows it, a part no feature shows opens its card, and a
    // click on the wood or the cabinet clears both
    const f = featureAt(pf, model.features.placed, item);
    state.selected.value = f?.key ?? "";
    state.inspected.value = f || !item || item.id === FRAME ? null : item.id;
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
  return (
    <div
      class={`stage${zoom.s > 1 ? " zoomed" : ""}`}
      ref={stage}
      onPointerDown={startPan}
    >
      <div
        class="zoom"
        style={{
          transform: `translate(${zoom.x}px, ${zoom.y}px) scale(${zoom.s})`,
        }}
      >
        <div
          class="drawing-host"
          ref={host}
          onClick={onClick}
          dangerouslySetInnerHTML={markup}
        />
        <canvas class="glow" ref={canvas} aria-hidden="true" />
        <svg
          class="overlay"
          viewBox={vb.join(" ")}
          preserveAspectRatio="xMidYMid meet"
          aria-hidden="true"
        >
          {rings.length > 0 && (
            <defs>
              <filter
                id="sim-halo"
                x="-50%"
                y="-50%"
                width="200%"
                height="200%"
              >
                <feGaussianBlur stdDeviation="1.6" />
              </filter>
            </defs>
          )}
          {labels.map((p) => {
            const edge = p.anchor === "end" ? p.cx - p.r : p.cx + p.r;
            const plateX = p.anchor === "end" ? p.x - p.width : p.x;
            return (
              <g key={p.id} class="mark" data-mark={p.id}>
                <circle
                  class="halo"
                  cx={p.cx}
                  cy={p.cy}
                  r={p.r}
                  filter="url(#sim-halo)"
                />
                <circle class="ring" cx={p.cx} cy={p.cy} r={p.r} />
                <line
                  class="leader"
                  x1={edge}
                  y1={p.cy}
                  x2={p.x}
                  y2={p.y - LABEL_FONT / 3}
                />
                <rect
                  class="plate"
                  x={plateX}
                  y={p.y - LABEL_FONT}
                  width={p.width}
                  height={LABEL_FONT + 2}
                  rx={1.5}
                />
                <text
                  class="label"
                  x={p.anchor === "end" ? p.x - 1.5 : p.x + 1.5}
                  y={p.y}
                  text-anchor={p.anchor}
                  font-size={LABEL_FONT}
                >
                  {p.label}
                </text>
              </g>
            );
          })}
          {inspectRing && (
            <circle
              class="inspect-ring"
              cx={inspectRing.cx}
              cy={inspectRing.cy}
              r={inspectRing.r + 1}
            />
          )}
          {studio && (
            <g class="lamp-numbers">
              {pf.lamps.map((l) => {
                const span = model.hw.spans.get(l.id);
                if (!span) return null;
                return (
                  <text
                    key={l.id}
                    data-lamp={l.id}
                    x={l.point[0]}
                    y={l.point[1] + LAMP_NUMBER_FONT / 3}
                    font-size={LAMP_NUMBER_FONT}
                  >
                    {span.index}
                  </text>
                );
              })}
            </g>
          )}
        </svg>
      </div>
      <RampSwitch model={model} state={state} />
      <div class="zoom-controls">
        <button
          data-action="zoom-in"
          title="Zoom in"
          onClick={() => zoomBy(1.5)}
        >
          +
        </button>
        <button
          data-action="zoom-out"
          title="Zoom out"
          disabled={zoom.s <= 1}
          onClick={() => zoomBy(1 / 1.5)}
        >
          −
        </button>
        <button
          data-action="zoom-reset"
          title="Show the whole machine"
          disabled={zoom.s <= 1}
          onClick={() => setZoom(HOME)}
        >
          ⤢
        </button>
        <button data-action="three-d" class="dim" title="Show the machine in 3D" onClick={() => (state.threeD.value = true)}>
          3D
        </button>
      </div>
    </div>
  );
}
