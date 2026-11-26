// The machine's display in the studio: the pixels the firmware's screen component sends to the
// simulated display, at the panel's 320 × 240 pixels or scaled up, with the mouse or a finger on the
// canvas as the touch panel's finger, and the touch events and gestures the firmware made of it.
import { signal } from "@preact/signals";
import { useEffect, useRef, useState } from "preact/hooks";
import { sources } from "../sources";
import {
  putOnCard,
  rgb565ToRgba,
  SCALES,
  SCREEN_HEIGHT,
  SCREEN_WIDTH,
  screenPoint,
  TouchInput,
  type CardFile,
  type Scale,
  type Scene,
  type ScreenEvent,
  type ScreenModule,
} from "../sim/screen";

const EVENTS_SHOWN = 14;
const scale = signal<Scale>(2);
const scene = signal("games");

// the files a scene reads from the simulated card, put there when the scene is picked
const SCENE_FILES: Record<string, CardFile> = {
  "boot-film": { path: "/video/intro.rgb", url: sources.introRgb },
};

// Shows a scene once the files it reads lie on the card, unless another scene was picked meanwhile.
async function showScene(module: ScreenModule, key: string): Promise<void> {
  const file = SCENE_FILES[key];
  if (file) await putOnCard(module, file);
  if (scene.value === key) module.screenScene(key);
}

const LABELS: Record<ScreenEvent["type"], string> = {
  touchDown: "Touch down",
  touchMoved: "Touch moved",
  touchHeld: "Touch held",
  touchUp: "Touch up",
  screenTapped: "Screen tapped",
  screenHeld: "Screen held",
  buttonClicked: "Button clicked",
  edgeTouched: "Edge touched",
  swipeStarted: "Swipe started",
  swipeLetGo: "Swipe let go",
  menuOpened: "Menu opened",
  menuClosed: "Menu closed",
  sound: "Menu: Sound",
  lights: "Menu: Lights",
  machine: "Menu: Machine",
  endGame: "Menu: End game",
  close: "Menu: Close",
  control: "Control",
  scene: "Scene",
};
// the events that carry no point, and those that carry the finger's x alone
const POINTLESS = new Set<ScreenEvent["type"]>([
  "buttonClicked",
  "menuOpened",
  "menuClosed",
  "sound",
  "lights",
  "machine",
  "endGame",
  "close",
  "control",
  "scene",
]);
const X_ONLY = new Set<ScreenEvent["type"]>(["edgeTouched", "swipeStarted", "swipeLetGo"]);

// Copies the display's pixels into the canvas.
export function drawScreen(ctx: CanvasRenderingContext2D, m: ScreenModule): void {
  const image = ctx.createImageData(SCREEN_WIDTH, SCREEN_HEIGHT);
  rgb565ToRgba(m.screenPixels(), image.data);
  ctx.putImageData(image, 0, 0);
}

export function ScreenPanel({ module }: { module: ScreenModule | null }) {
  const canvas = useRef<HTMLCanvasElement>(null);
  const input = useRef<TouchInput | null>(null);
  const [begun, setBegun] = useState<boolean | null>(null);
  const [events, setEvents] = useState<ScreenEvent[]>([]);
  const [scenes, setScenes] = useState<Scene[]>([]);

  useEffect(() => {
    if (!module) return;
    setBegun(module.screenBegin());
    setScenes(module.screenScenes());
    void showScene(module, scene.value);
    input.current = new TouchInput(module);
    const ctx = canvas.current?.getContext("2d") ?? null;
    let shown = -1;
    let frame = 0;
    const tick = () => {
      const rectangles = module.screenRectangles();
      if (rectangles !== shown && ctx) {
        shown = rectangles;
        drawScreen(ctx, module);
      }
      const fresh = module.screenEvents();
      // a press that led to another view moves the picker along
      for (const e of fresh) if (e.type === "scene" && e.name) scene.value = e.name;
      if (fresh.length) setEvents((old) => [...fresh.reverse(), ...old].slice(0, EVENTS_SHOWN));
      frame = requestAnimationFrame(tick);
    };
    frame = requestAnimationFrame(tick);
    return () => {
      cancelAnimationFrame(frame);
      // a finger still down when the studio leaves the screen lifts
      input.current?.release();
    };
  }, [module]);

  const at = (e: PointerEvent) => screenPoint(e.clientX, e.clientY, (e.currentTarget as HTMLCanvasElement).getBoundingClientRect());
  const s = scale.value;

  return (
    <div class="stage screen-stage">
      <div class="screen-bar">
        <div class="seg">
          {SCALES.map((k) => (
            <button key={k} class={k === s ? "on" : ""} onClick={() => (scale.value = k)}>
              {k}×
            </button>
          ))}
        </div>
        <select
          class="screen-scene"
          aria-label="Scene"
          value={scene.value}
          onChange={(e) => {
            const key = (e.currentTarget as HTMLSelectElement).value;
            scene.value = key;
            if (module) void showScene(module, key);
          }}
        >
          {scenes.map((k) => (
            <option key={k.key} value={k.key}>
              {k.label}
            </option>
          ))}
        </select>
        <span class="screen-size">
          {SCREEN_WIDTH} × {SCREEN_HEIGHT}
        </span>
      </div>
      <div class="screen-frame">
        <canvas
          ref={canvas}
          class="screen-canvas"
          width={SCREEN_WIDTH}
          height={SCREEN_HEIGHT}
          style={{ width: `${SCREEN_WIDTH * s}px`, height: `${SCREEN_HEIGHT * s}px` }}
          onPointerDown={(e) => {
            if (!input.current?.down(e.pointerId, at(e))) return;
            (e.currentTarget as HTMLCanvasElement).setPointerCapture?.(e.pointerId);
            e.preventDefault();
          }}
          onPointerMove={(e) => input.current?.move(e.pointerId, at(e))}
          onPointerUp={(e) => input.current?.up(e.pointerId)}
          onPointerCancel={(e) => input.current?.up(e.pointerId)}
          onLostPointerCapture={(e) => input.current?.up(e.pointerId)}
        />
      </div>
      {begun === false && <p class="screen-failed">The touch driver did not begin</p>}
      <ol class="screen-events">
        {events.map((e, i) => (
          <li key={`${e.ms}-${i}-${e.type}`} class={e.type}>
            <span class="time">{(e.ms / 1000).toFixed(2)} s</span>
            <span>
              {LABELS[e.type] ?? e.type}
              {e.name ? `: ${e.name}` : ""}
              {e.name === "slider" ? ` ${e.x} %` : ""}
            </span>
            {!POINTLESS.has(e.type) && (
              <span class="where">
                {X_ONLY.has(e.type) ? `x ${e.x}` : `${e.x}, ${e.y}`}
              </span>
            )}
          </li>
        ))}
      </ol>
    </div>
  );
}
