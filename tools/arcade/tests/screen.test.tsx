// @vitest-environment jsdom
// The machine's display in the studio: the colors the canvas takes from the display's RGB565, the
// pointer's way to the touch controller's finger, the panel, and the firmware's screen component
// and touch driver, built by "npm run wasm", taking taps, holds, button presses and the swipe that
// pulls in the host's menu.
import { existsSync, readFileSync } from "node:fs";
import { resolve } from "node:path";
import { render } from "preact";
import { act } from "preact/test-utils";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { lightsOptions, loadLights, type LightsModule } from "../src/sim/lights";
import { rgb565ToRgba, SCREEN_HEIGHT, SCREEN_WIDTH, screenPoint, TouchInput, type ScreenEvent } from "../src/sim/screen";
import { ScreenPanel } from "../src/ui/ScreenPanel";

const wasm = resolve("src/wasm/simulator.wasm");
if (existsSync(wasm)) lightsOptions.wasmBinary = readFileSync(wasm);
const built = (await loadLights()) !== null;

const HOLD_MS = 1000; // touch.h: HOLD_MS, the rest before TouchHeld
const READ_MS = 20; // touch.h: TICK_US, one read of the controller
const EDGE_PX = 24; // screen.md: the right edge the host's swipe starts in
const SLOP_PX = 10; // touch.h: SLOP_PX
const MENU_PX = 208; // host-screens.md: the menu's drawer
const SNAP_MS = 150; // host-screens.md: the menu snaps open or back
const NIGHT_565 = 0x0823; // #0e061a in RGB565
const VELVET_565 = 0x2848; // #2c0a46 in RGB565

// the colors of one pixel, RGB565 high byte first
const rgbOf = (hi: number, lo: number) => {
  const out = new Uint8ClampedArray(4);
  rgb565ToRgba(new Uint8Array([hi, lo]), out);
  return [...out];
};

describe("the display's colors", () => {
  it("spreads RGB565 over the canvas's eight bits, opaque", () => {
    expect(rgbOf(0x00, 0x00)).toEqual([0, 0, 0, 255]);
    expect(rgbOf(0xff, 0xff)).toEqual([255, 255, 255, 255]);
    expect(rgbOf(0xf8, 0x00)).toEqual([255, 0, 0, 255]);
    expect(rgbOf(0x07, 0xe0)).toEqual([0, 255, 0, 255]);
    expect(rgbOf(0x00, 0x1f)).toEqual([0, 0, 255, 255]);
  });

  it("reads the high byte first, as the display driver sends it", () => {
    // 0x0823: red 1, green 1, blue 3, the screen's night background #0e061a in RGB565
    expect(rgbOf(0x08, 0x23)).toEqual([8, 4, 24, 255]);
  });
});

describe("the pointer on the screen", () => {
  it("lands on the display's pixel at every scale", () => {
    for (const s of [1, 2, 3]) {
      const box = { left: 100, top: 50, width: SCREEN_WIDTH * s, height: SCREEN_HEIGHT * s };
      expect(screenPoint(100, 50, box)).toEqual({ x: 0, y: 0 });
      expect(screenPoint(100 + 123 * s + s / 2, 50 + 45 * s, box)).toEqual({ x: 123, y: 45 });
      expect(screenPoint(100 + SCREEN_WIDTH * s - 0.5, 50 + SCREEN_HEIGHT * s - 0.5, box)).toEqual({ x: 319, y: 239 });
    }
  });

  it("stays inside the screen when the pointer leaves the canvas", () => {
    const box = { left: 0, top: 0, width: 640, height: 480 };
    expect(screenPoint(-30, -5, box)).toEqual({ x: 0, y: 0 });
    expect(screenPoint(900, 700, box)).toEqual({ x: 319, y: 239 });
  });

  it("follows one pointer down, along and up, and leaves a second one out", () => {
    const touch = vi.fn();
    const input = new TouchInput({ touch });
    expect(input.down(1, { x: 10, y: 20 })).toBe(true);
    expect(input.down(2, { x: 200, y: 200 })).toBe(false);
    input.move(2, { x: 210, y: 200 });
    input.move(1, { x: 30, y: 20 });
    input.up(2);
    expect(input.touching).toBe(true);
    input.up(1);
    expect(touch.mock.calls).toEqual([
      [true, 10, 20],
      [true, 30, 20],
      [false, 30, 20],
    ]);
    expect(input.touching).toBe(false);
  });

  it("lifts a finger still down when released", () => {
    const touch = vi.fn();
    const input = new TouchInput({ touch });
    input.down(7, { x: 5, y: 6 });
    input.release();
    input.release();
    expect(touch.mock.calls).toEqual([
      [true, 5, 6],
      [false, 5, 6],
    ]);
  });
});

// A module that records the touches and hands over the events and pixels a test sets.
function fakeModule() {
  const pixels = new Uint8Array(SCREEN_WIDTH * SCREEN_HEIGHT * 2);
  const m = {
    touches: [] as [boolean, number, number][],
    pending: [] as ScreenEvent[],
    rectangles: 0,
    screenBegin: vi.fn(() => true),
    touch(down: boolean, x: number, y: number) {
      m.touches.push([down, x, y]);
    },
    screenPixels: () => pixels,
    screenRectangles: () => m.rectangles,
    screenEvents() {
      const out = m.pending;
      m.pending = [];
      return out;
    },
  };
  return { m, pixels };
}

describe("the screen panel", () => {
  let root: HTMLElement;
  let painted: ImageData[];

  beforeEach(() => {
    root = document.createElement("div");
    document.body.appendChild(root);
    painted = [];
    // jsdom draws no canvas, so a 2D context that keeps what is put stands in
    vi.spyOn(HTMLCanvasElement.prototype, "getContext").mockImplementation(
      () =>
        ({
          createImageData: (w: number, h: number) => ({ width: w, height: h, data: new Uint8ClampedArray(w * h * 4) }),
          putImageData: (image: ImageData) => painted.push(image),
        }) as unknown as CanvasRenderingContext2D,
    );
  });
  afterEach(() => {
    render(null, root);
    root.remove();
    vi.restoreAllMocks();
  });

  const frames = () => act(() => new Promise((r) => setTimeout(r, 60)));
  const canvas = () => root.querySelector("canvas.screen-canvas") as HTMLCanvasElement;
  const pointer = (type: string, x: number, y: number, pointerId = 1) =>
    act(() => {
      canvas().dispatchEvent(new PointerEvent(type, { clientX: x, clientY: y, pointerId, bubbles: true }));
    });

  it("draws the display's 320 × 240 pixels at the scale picked", async () => {
    const { m } = fakeModule();
    act(() => render(<ScreenPanel module={m} />, root));
    expect(canvas().width).toBe(SCREEN_WIDTH);
    expect(canvas().height).toBe(SCREEN_HEIGHT);
    for (const [label, width] of [
      ["1×", "320px"],
      ["3×", "960px"],
      ["2×", "640px"],
    ]) {
      act(() => {
        [...root.querySelectorAll(".screen-bar button")].find((b) => b.textContent === label)!.dispatchEvent(new MouseEvent("click", { bubbles: true }));
      });
      expect(canvas().style.width).toBe(width);
    }
  });

  it("begins the screen and repaints once the display took a rectangle", async () => {
    const { m, pixels } = fakeModule();
    pixels.set([0xf8, 0x00], 0);
    act(() => render(<ScreenPanel module={m} />, root));
    expect(m.screenBegin).toHaveBeenCalledTimes(1);
    await frames();
    const first = painted.length;
    expect(first).toBe(1);
    expect([...painted[0].data.slice(0, 4)]).toEqual([255, 0, 0, 255]);
    await frames();
    expect(painted.length).toBe(first);
    m.rectangles = 1;
    await frames();
    expect(painted.length).toBe(first + 1);
  });

  it("sends a press, a drag and a lift as the finger at the pixels under the pointer", async () => {
    const { m } = fakeModule();
    act(() => render(<ScreenPanel module={m} />, root));
    vi.spyOn(canvas(), "getBoundingClientRect").mockReturnValue({ left: 10, top: 20, width: 640, height: 480 } as DOMRect);
    pointer("pointerdown", 10 + 2 * 300, 20 + 2 * 100);
    pointer("pointermove", 10 + 2 * 250, 20 + 2 * 100);
    pointer("pointermove", 10 + 2 * 250, 20 + 2 * 100, 2);
    pointer("pointerup", 10 + 2 * 200, 20 + 2 * 100);
    expect(m.touches).toEqual([
      [true, 300, 100],
      [true, 250, 100],
      [false, 250, 100],
    ]);
  });

  it("lists the firmware's events, the latest first", async () => {
    const { m } = fakeModule();
    act(() => render(<ScreenPanel module={m} />, root));
    m.pending = [
      { type: "touchDown", x: 12, y: 34, ms: 1000 },
      { type: "touchUp", x: 12, y: 34, ms: 1100 },
      { type: "screenTapped", x: 12, y: 34, ms: 1100 },
    ];
    await frames();
    const rows = [...root.querySelectorAll(".screen-events li")].map((li) => li.className);
    expect(rows).toEqual(["screenTapped", "touchUp", "touchDown"]);
    expect(root.querySelector(".screen-events li")!.textContent).toContain("12, 34");
  });

  it("lifts a finger still down when it leaves the studio", async () => {
    const { m } = fakeModule();
    act(() => render(<ScreenPanel module={m} />, root));
    vi.spyOn(canvas(), "getBoundingClientRect").mockReturnValue({ left: 0, top: 0, width: 320, height: 240 } as DOMRect);
    pointer("pointerdown", 40, 50);
    act(() => render(null, root));
    expect(m.touches).toEqual([
      [true, 40, 50],
      [false, 40, 50],
    ]);
  });
});

describe.runIf(built)("the firmware's screen in the browser", () => {
  let m: LightsModule;
  let input: TouchInput;

  beforeEach(async () => {
    m = (await loadLights())!;
    expect(m.screenBegin()).toBe(true);
    m.step(100);
    m.screenEvents();
    input = new TouchInput(m);
  });

  const press = (x: number, y: number, ms: number) => {
    input.down(1, { x, y });
    m.step(ms);
    input.up(1);
    m.step(2 * READ_MS);
    return m.screenEvents();
  };
  const types = (events: ScreenEvent[]) => events.map((e) => e.type);

  it("draws the test scene onto the display", () => {
    expect(m.screenRectangles()).toBeGreaterThan(0);
    const px = m.screenPixels();
    const at = (x: number, y: number) => (px[2 * (y * SCREEN_WIDTH + x)] << 8) | px[2 * (y * SCREEN_WIDTH + x) + 1];
    // the night background in the corner, and the button's red in the middle
    expect(at(0, 0)).toBe(NIGHT_565);
    expect(at(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 18) >> 11).toBeGreaterThan(16);
  });

  it("makes a short touch beside the button a tap at the touched pixel", () => {
    const events = press(20, 200, 100);
    expect(types(events)).toEqual(["touchDown", "touchUp", "screenTapped"]);
    expect(events[2]).toMatchObject({ x: 20, y: 200 });
  });

  it("reports the touch at the pixel touched, across the screen", () => {
    for (let x = 0; x < SCREEN_WIDTH; x += 29) {
      for (const y of [0, 57, 239]) {
        const down = press(x, y, 40).find((e) => e.type === "touchDown")!;
        expect([down.x, down.y], `${x}, ${y}`).toEqual([x, y]);
      }
    }
    for (const [x, y] of [[319, 0], [0, 239], [319, 239]]) {
      const down = press(x, y, 40).find((e) => e.type === "touchDown")!;
      expect([down.x, down.y]).toEqual([x, y]);
    }
  });

  it("makes a finger resting for 1 s a hold", () => {
    const events = press(40, 60, HOLD_MS + 4 * READ_MS);
    expect(types(events)).toEqual(["touchDown", "touchHeld", "screenHeld", "touchUp"]);
    expect(events.find((e) => e.type === "screenHeld")).toMatchObject({ x: 40, y: 60 });
  });

  it("hands a touch on the button to the button and makes no tap of it", () => {
    const events = press(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 100);
    expect(types(events)).toEqual(["touchDown", "touchUp", "buttonClicked"]);
  });

  // Swipes from the right edge to x and lets go, and gets the events.
  const swipe = (to: number, y = 100) => {
    const from = SCREEN_WIDTH - EDGE_PX / 2;
    input.down(1, { x: from, y });
    m.step(2 * READ_MS);
    for (let x = from - SLOP_PX - 5; x >= to; x -= SLOP_PX + 5) {
      input.move(1, { x, y });
      m.step(2 * READ_MS);
    }
    input.up(1);
    m.step(2 * READ_MS + SNAP_MS);
    return m.screenEvents();
  };
  const pixel = (x: number, y: number) => {
    const px = m.screenPixels();
    return (px[2 * (y * SCREEN_WIDTH + x)] << 8) | px[2 * (y * SCREEN_WIDTH + x) + 1];
  };

  it("opens the host's menu with a swipe from the right edge past half the menu's width", () => {
    const events = swipe(150);
    expect(events[0]).toMatchObject({ type: "touchDown", x: SCREEN_WIDTH - EDGE_PX / 2 });
    const moved = events.filter((e) => e.type === "touchMoved").map((e) => e.x);
    expect(moved.length).toBeGreaterThan(4);
    expect(moved).toEqual([...moved].sort((a, b) => b - a));
    const kinds = types(events);
    expect(kinds).toContain("edgeTouched");
    expect(kinds.indexOf("swipeStarted")).toBeGreaterThan(kinds.indexOf("edgeTouched"));
    expect(kinds.slice(-2)).toEqual(["swipeLetGo", "menuOpened"]);
    expect(kinds).not.toContain("screenTapped");
    expect(kinds).not.toContain("screenHeld");
    // the menu covers the screen from 320 - 208 = 112 px on
    expect(pixel(SCREEN_WIDTH - MENU_PX / 2, 60)).toBe(VELVET_565);
    expect(pixel(SCREEN_WIDTH - MENU_PX - 20, 60)).toBe(NIGHT_565);
  });

  it("lets the menu slide back when the swipe ends before half its width", () => {
    const events = swipe(SCREEN_WIDTH - MENU_PX / 2 + 20);
    expect(types(events)).toContain("swipeLetGo");
    expect(types(events)).not.toContain("menuOpened");
    expect(pixel(SCREEN_WIDTH - 30, 60)).toBe(NIGHT_565);
  });

  it("gives a tap at the right edge to the game", () => {
    const events = press(SCREEN_WIDTH - 5, 200, 100);
    expect(types(events)).toEqual(["touchDown", "edgeTouched", "touchUp", "screenTapped"]);
    expect(pixel(SCREEN_WIDTH - 5, 60)).toBe(NIGHT_565);
  });

  it("closes the menu with its Close button and with a tap on the game", () => {
    swipe(150);
    // Close lies at the menu's bottom left
    let events = press(SCREEN_WIDTH - MENU_PX + 40, SCREEN_HEIGHT - 30, 100);
    expect(types(events)).toEqual(["touchDown", "touchUp", "menuClosed"]);
    m.step(SNAP_MS);
    expect(pixel(SCREEN_WIDTH - MENU_PX / 2, 60)).toBe(NIGHT_565);
    swipe(150);
    events = press(30, 200, 100);
    expect(types(events)).toEqual(["touchDown", "touchUp", "screenTapped", "menuClosed"]);
  });

  it("keeps a tap on the open menu from the game", () => {
    swipe(150);
    const events = press(SCREEN_WIDTH - MENU_PX / 2, 120, 100);
    expect(types(events)).toEqual(["touchDown", "touchUp"]);
  });
});
