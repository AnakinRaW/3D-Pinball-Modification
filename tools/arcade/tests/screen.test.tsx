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
import { cutFrames, putOnCard, rgb565ToRgba, SCREEN_HEIGHT, SCREEN_WIDTH, screenPoint, TouchInput, type ScreenEvent } from "../src/sim/screen";
import { ScreenPanel } from "../src/ui/ScreenPanel";

const wasm = resolve("src/wasm/simulator.wasm");
if (existsSync(wasm)) lightsOptions.wasmBinary = readFileSync(wasm);
const built = (await loadLights()) !== null;

const HOLD_MS = 1000; // touch.h: HOLD_MS, the rest before TouchHeld
const SLIDE_MS = 150; // games-view.h: SLIDE_MS, the ring's turn
const PREVIEW_MS = 500; // games-view.h: PREVIEW_MS, a card's stand before its preview
const CURTAIN_MS = 250 + 200 + 300; // stage-views.h and host-screens.md: the curtain closes, holds and opens
const LAUNCH_MS = 1500; // host-screens.md: t_launch_card
const READ_MS = 20; // touch.h: TICK_US, one read of the controller
const EDGE_PX = 24; // screen.md: the right edge the host's swipe starts in
const SLOP_PX = 10; // touch.h: SLOP_PX
const MENU_PX = 208; // host-screens.md: the menu's drawer
const SNAP_MS = 150; // host-screens.md: the menu snaps open or back
const HINT_STAY_MS = 1000; // host-screens.md: the hint stands once the finger leaves
const HINT_FADE_MS = 300; // host-screens.md: then fades
const REFRESH_MS = 40; // longer than LVGL's refresh period, which its animations step at
const NIGHT_565 = 0x0823; // #0e061a in RGB565
const GOLD_565 = 0xee09; // #e8c04a in RGB565

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
    screenScene: vi.fn((_scene: string) => true),
    screenScenes: () => [
      { key: "controls", label: "Controls" },
      { key: "games", label: "Games" },
      { key: "game-runs", label: "Game runs" },
      { key: "boot-film", label: "Boot film" },
    ],
    screenAnimate: vi.fn(),
    cardFile: vi.fn((_path: string, _bytes: Uint8Array) => {}),
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
    sent: [] as { ms: number; pixels: number }[],
    screenFrames() {
      const out = m.sent;
      m.sent = [];
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

  it("lists the module's scenes and shows the scene picked, the game choice at first", () => {
    const { m } = fakeModule();
    act(() => render(<ScreenPanel module={m} />, root));
    expect(m.screenScene).toHaveBeenLastCalledWith("games");
    const select = root.querySelector("select.screen-scene") as HTMLSelectElement;
    expect([...select.options].map((o) => o.textContent)).toEqual(["Controls", "Games", "Game runs", "Boot film"]);
    const pick = (key: string) =>
      act(() => {
        select.value = key;
        select.dispatchEvent(new Event("change", { bubbles: true }));
      });
    pick("controls");
    expect(m.screenScene).toHaveBeenLastCalledWith("controls");
    pick("game-runs");
    expect(m.screenScene).toHaveBeenLastCalledWith("game-runs");
  });

  it("shows the boot film once its file is fetched, unless another scene was picked meanwhile", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => new Response(new Uint8Array(4))));
    const { m } = fakeModule();
    act(() => render(<ScreenPanel module={m} />, root));
    const select = root.querySelector("select.screen-scene") as HTMLSelectElement;
    const pick = (key: string) =>
      act(() => {
        select.value = key;
        select.dispatchEvent(new Event("change", { bubbles: true }));
      });
    pick("boot-film");
    pick("controls");
    await frames();
    expect(m.screenScene).toHaveBeenLastCalledWith("controls");
    pick("boot-film");
    await frames();
    expect(m.screenScene).toHaveBeenLastCalledWith("boot-film");
    vi.unstubAllGlobals();
  });

  it("weighs the frames of the last second against the Teensy's budget, and marks a frame past it", async () => {
    const { m } = fakeModule();
    act(() => render(<ScreenPanel module={m} />, root));
    const readout = () => root.querySelector("[data-testid=screen-budget]") as HTMLElement;
    m.sent = [{ ms: 1000, pixels: SCREEN_WIDTH * SCREEN_HEIGHT }];
    await act(() => new Promise((r) => setTimeout(r, 300)));
    expect(readout().textContent).toMatch(/Frame 19\.9 \/ 33 ms/);
    expect(readout().classList.contains("over")).toBe(false);
    // one pass that sent the screen twice does not fit
    m.sent = [{ ms: 1100, pixels: 2 * SCREEN_WIDTH * SCREEN_HEIGHT }];
    await act(() => new Promise((r) => setTimeout(r, 300)));
    expect(readout().classList.contains("over")).toBe(true);
  });

  it("moves the picker to the scene a press led to", async () => {
    const { m } = fakeModule();
    act(() => render(<ScreenPanel module={m} />, root));
    m.pending = [{ type: "scene", x: 0, y: 0, ms: 5, name: "game-runs" }];
    await frames();
    expect((root.querySelector("select.screen-scene") as HTMLSelectElement).value).toBe("game-runs");
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

describe("the simulated card", () => {
  afterEach(() => vi.unstubAllGlobals());

  it("puts a file on each module's card once, fetching its URL once", async () => {
    const fetched = vi.fn(async () => new Response(new Uint8Array([1, 2, 3])));
    vi.stubGlobal("fetch", fetched);
    const { m } = fakeModule();
    const file = { path: "/video/intro.rgb", url: "/intro.rgb" };
    expect(await putOnCard(m, file)).toBe(true);
    expect(await putOnCard(m, file)).toBe(true);
    expect(fetched).toHaveBeenCalledTimes(1);
    expect(m.cardFile).toHaveBeenCalledWith("/video/intro.rgb", new Uint8Array([1, 2, 3]));
    const other = fakeModule().m;
    expect(await putOnCard(other, { ...file, url: "/intro-once.rgb" })).toBe(true);
    expect(await putOnCard(other, { path: "/video/cut.rgb", url: "/intro-once.rgb", make: (b) => b.subarray(1) })).toBe(true);
    expect(fetched).toHaveBeenCalledTimes(2);
    expect(other.cardFile).toHaveBeenLastCalledWith("/video/cut.rgb", new Uint8Array([2, 3]));
  });

  it("cuts the same rectangle out of every frame", () => {
    // two frames of 3 × 2 pixels, each pixel's two bytes holding its frame and its place
    const video = new Uint8Array(2 * 3 * 2 * 2);
    for (let i = 0; i < video.length / 2; i++) video.set([Math.floor(i / 6), i % 6], 2 * i);
    const cut = cutFrames(video, 3, 2, { x: 1, y: 0, width: 2, height: 2 });
    expect([...cut]).toEqual([0, 1, 0, 2, 0, 4, 0, 5, 1, 1, 1, 2, 1, 4, 1, 5]);
  });

  it("fetches a file again after a failed fetch, and puts nothing on the card without a URL", async () => {
    const fetched = vi.fn(async () => new Response(null, { status: 404 }));
    vi.stubGlobal("fetch", fetched);
    const { m } = fakeModule();
    expect(await putOnCard(m, { path: "/video/intro.rgb", url: "/missing.rgb" })).toBe(false);
    expect(await putOnCard(m, { path: "/video/intro.rgb", url: "/missing.rgb" })).toBe(false);
    expect(fetched).toHaveBeenCalledTimes(2);
    expect(await putOnCard(m, { path: "/video/other.rgb", url: undefined })).toBe(false);
    expect(m.cardFile).not.toHaveBeenCalled();
  });
});

describe.runIf(built)("the firmware's screen in the browser", () => {
  let m: LightsModule;
  let input: TouchInput;

  beforeEach(async () => {
    m = (await loadLights())!;
    expect(m.screenBegin()).toBe(true);
    // the module lasts across the tests, so each starts on the running game with its sunburst still
    m.screenAnimate(false);
    m.screenScene("game-runs");
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

  it("draws the stand-in game onto the display", () => {
    expect(m.screenRectangles()).toBeGreaterThan(0);
    const px = m.screenPixels();
    const at = (x: number, y: number) => (px[2 * (y * SCREEN_WIDTH + x)] << 8) | px[2 * (y * SCREEN_WIDTH + x) + 1];
    // the sunburst lights the corner above the night, and the gold lettering stands in the middle
    expect(at(0, 0)).not.toBe(NIGHT_565);
    expect(at(SCREEN_WIDTH / 2, 84) >> 11).toBeGreaterThan(20);
  });

  it("makes a short touch on the game a tap at the touched pixel", () => {
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

  it("hands a touch on the status bulb to the status and makes no tap of it", () => {
    const events = press(SCREEN_WIDTH - 12, 14, 100);
    expect(types(events).slice(0, 3)).toEqual(["touchDown", "touchUp", "control"]);
    expect(events[2].name).toBe("STATUS");
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
    m.step(2 * READ_MS + SNAP_MS + REFRESH_MS);
    return m.screenEvents();
  };
  const pixel = (x: number, y: number) => {
    const px = m.screenPixels();
    return (px[2 * (y * SCREEN_WIDTH + x)] << 8) | px[2 * (y * SCREEN_WIDTH + x) + 1];
  };

  it("opens the host's menu with a swipe from the right edge past half the menu's width", () => {
    const beside = pixel(SCREEN_WIDTH - MENU_PX - 20, 60);
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
    // a row's velvet, nine tenths over the drawer's night, has more blue than the night alone
    expect(pixel(SCREEN_WIDTH - MENU_PX / 2, 60) & 0x1f).toBeGreaterThan(NIGHT_565 & 0x1f);
    expect(pixel(SCREEN_WIDTH - MENU_PX - 20, 60)).toBe(beside);
  });

  it("lets the menu slide back when the swipe ends before half its width", () => {
    const game = pixel(SCREEN_WIDTH - 30, 60);
    const events = swipe(SCREEN_WIDTH - MENU_PX / 2 + 20);
    expect(types(events)).toContain("swipeLetGo");
    expect(types(events)).not.toContain("menuOpened");
    expect(pixel(SCREEN_WIDTH - 30, 60)).toBe(game);
  });

  it("gives a tap at the right edge to the game, and lets the hint stand 1 s and fade", () => {
    // the gold tab left of the drawer's edge still shows, then has faded once the hint's time is over
    const tab = () => pixel(SCREEN_WIDTH - 10, SCREEN_HEIGHT / 2 - 20);
    const game = tab();
    const events = press(SCREEN_WIDTH - 4, 200, 100);
    expect(types(events)).toEqual(["touchDown", "edgeTouched", "touchUp", "screenTapped"]);
    // a short tap still lets the hint reach full strength while it stands
    m.step(HINT_STAY_MS / 2);
    expect(tab()).toBe(GOLD_565);
    m.step(HINT_STAY_MS + HINT_FADE_MS + REFRESH_MS);
    expect(tab()).toBe(game);
  });

  it("names the menu's items and opens their pages, and closes it with its close and with a tap on the game", () => {
    // the rows from the header down, End game at the bottom, each closing the menu for its page
    for (const [y, item, page] of [
      [68, "sound", "sound"],
      [116, "lights", "lights"],
      [164, "status", "warning-lights"],
      [214, "endGame", "end-game"],
    ] as const) {
      swipe(150);
      const events = press(SCREEN_WIDTH - 50, y, 100);
      expect(types(events).slice(0, 4)).toEqual(["touchDown", "touchUp", item, "menuClosed"]);
      expect(events.find((e) => e.type === "scene")?.name).toBe(page);
      m.screenScene("game-runs");
      m.step(SNAP_MS + REFRESH_MS);
      m.screenEvents();
    }
    swipe(150);
    // the close at the header's right end
    let events = press(SCREEN_WIDTH - 10, 17, 100);
    expect(types(events)).toEqual(["touchDown", "touchUp", "close", "menuClosed"]);
    m.step(SNAP_MS + REFRESH_MS);
    // the velvet of the menu's rows has gone
    expect(pixel(SCREEN_WIDTH - MENU_PX / 2, 60) & 0x1f).toBeLessThan(0x0a);
    swipe(150);
    events = press(30, 200, 100);
    expect(types(events)).toEqual(["touchDown", "touchUp", "screenTapped", "menuClosed"]);
  });

  it("keeps a tap on the open menu from the game", () => {
    swipe(150);
    // the marquee bulbs under the header, where no item lies
    const events = press(SCREEN_WIDTH - MENU_PX / 2, 38, 100);
    expect(types(events)).toEqual(["touchDown", "touchUp"]);
  });

  it("shows every control on the controls page and names the one pressed", () => {
    expect(m.screenScene("controls")).toBe(true);
    m.step(2 * REFRESH_MS);
    // the header's red band across the top
    expect(pixel(SCREEN_WIDTH / 2, 4) >> 11).toBeGreaterThan(20);
    const control = (x: number, y: number) =>
      press(x, y, 100)
        .filter((e) => e.type === "control")
        .map((e) => (e.name === "slider" ? `slider ${e.x}` : e.name));
    // the buttons in the first row, the back and the gear in the header, the row at the bottom
    expect(control(54, 68)).toEqual(["PLAY"]);
    expect(control(266, 68)).toEqual(["CANCEL"]);
    expect(control(20, 17)).toEqual(["back"]);
    expect(control(300, 17)).toEqual(["gear"]);
    expect(control(160, 214)).toEqual(["row"]);
    // the disabled OFF takes no touch, and plus moves the slider from 50 % by a step
    expect(control(266, 116)).toEqual([]);
    expect(control(294, 164)).toEqual(["slider 55"]);
    // START opens a dialog, whose OK closes it
    expect(control(160, 68)).toEqual(["START"]);
    m.step(REFRESH_MS);
    // the back arrow beside the dialog takes no touch while it shows
    expect(control(20, 17)).toEqual([]);
    expect(control(227, 159)).toEqual(["OK"]);
    m.step(REFRESH_MS);
    // the dialog has gone, and the slider beneath takes the touch
    expect(control(227, 159)).not.toContain("OK");
  });

  it("lists the host's views after the controls page", () => {
    const keys = m.screenScenes().map((s) => s.key);
    expect(keys[0]).toBe("controls");
    expect(keys).not.toContain("test");
    for (const k of ["start-report", "games", "settings", "sound", "warning-lights", "ball-sensors", "record-sensor", "name", "high-scores", "calibrate-touch"]) {
      expect(keys).toContain(k);
    }
  });

  it("draws every view of the host, each its own picture", () => {
    const seen = new Map<string, string>();
    for (const { key } of m.screenScenes()) {
      expect(m.screenScene(key), key).toBe(true);
      m.step(2 * REFRESH_MS);
      const shown = m.screenEvents().filter((e) => e.type === "scene");
      expect(shown.at(-1)?.name, key).toBe(key);
      const px = m.screenPixels();
      let sum = 0;
      for (let i = 0; i < px.length; i += 7) sum = (sum * 31 + px[i]) >>> 0;
      seen.set(key, String(sum));
    }
    // the closed curtain and a toast still sliding in aside, no two views look the same
    expect(new Set(seen.values()).size).toBe(seen.size);
  });

  it("leads from a press on a view to the page it opens", () => {
    const control = (events: ScreenEvent[]) => events.filter((e) => e.type === "control").map((e) => e.name);
    const scene = (events: ScreenEvent[]) => events.filter((e) => e.type === "scene").map((e) => e.name);
    m.screenScene("games");
    m.step(2 * REFRESH_MS);
    m.screenEvents();
    // the gear at the header's right end opens Settings, whose back leads to the games
    let events = press(300, 17, 100);
    expect(control(events)).toEqual(["SETTINGS"]);
    expect(scene(events)).toEqual(["settings"]);
    m.step(2 * REFRESH_MS);
    events = press(20, 17, 100);
    expect(control(events)).toEqual(["BACK"]);
    expect(scene(events)).toEqual(["games"]);
    // the neighbor peeking in at the right turns the ring to the game that cannot start, and the
    // last game leads on to the first
    m.step(2 * REFRESH_MS);
    const shown = () => {
      m.step(SLIDE_MS + 2 * REFRESH_MS);
      return m.screenEvents().find((e) => e.type === "control" && e.name === "SHOWN")?.x;
    };
    press(300, 118, 100);
    expect(shown()).toBe(1);
    press(300, 118, 100);
    expect(shown()).toBe(2);
    press(300, 118, 100);
    expect(shown()).toBe(0);
    press(20, 118, 100);
    expect(shown()).toBe(2);
  });

  it("lets the ring of games follow a finger and turn once it is let go past the snap", () => {
    m.screenScene("games");
    m.step(2 * REFRESH_MS);
    m.screenEvents();
    const drag = (from: number, to: number) => {
      input.down(1, { x: from, y: 118 });
      m.step(2 * READ_MS);
      for (let x = from; from > to ? x >= to : x <= to; x += from > to ? -8 : 8) {
        input.move(1, { x, y: 118 });
        m.step(2 * READ_MS);
      }
      input.up(1);
      m.step(2 * READ_MS + SLIDE_MS + 2 * REFRESH_MS);
      return m.screenEvents().filter((e) => e.type === "control" && e.name === "SHOWN");
    };
    // a short drag lets the ring slide back, a long one turns it, either way
    expect(drag(200, 180)).toEqual([]);
    expect(drag(220, 120).map((e) => e.x)).toEqual([1]);
    expect(drag(120, 220).map((e) => e.x)).toEqual([0]);
  });

  it("returns with back to the page that opened the warning lights", () => {
    const scene = (events: ScreenEvent[]) => events.filter((e) => e.type === "scene").at(-1)?.name;
    m.screenScene("games");
    m.step(2 * REFRESH_MS);
    m.screenEvents();
    // the status bulb at the header's left end opens the warning lights, and back returns to the games
    expect(scene(press(20, 17, 100))).toBe("warning-lights");
    m.step(2 * REFRESH_MS);
    expect(scene(press(20, 17, 100))).toBe("games");
  });

  it("lets every level run from 0 to 100 %", () => {
    m.screenScene("sound");
    m.step(2 * REFRESH_MS);
    m.screenEvents();
    // the effects' plus, from 70 % to the end of its track
    const plus = () => press(290, 146 + 20 + 22, 100).find((e) => e.type === "control");
    let last = plus();
    for (let i = 0; i < 8; i++) last = plus();
    expect(last).toMatchObject({ name: "EFFECTS", x: 100 });
  });

  it("plays the curtain from the game choice to the launch card and on into the game", () => {
    const scenes: string[] = [];
    const take = () => scenes.push(...m.screenEvents().filter((e) => e.type === "scene").map((e) => e.name!));
    m.screenScene("games");
    m.step(2 * REFRESH_MS);
    m.screenEvents();
    scenes.push(...press(SCREEN_WIDTH / 2, 216, 100).filter((e) => e.type === "scene").map((e) => e.name!));
    m.step(CURTAIN_MS + REFRESH_MS);
    take();
    m.step(LAUNCH_MS + 2 * REFRESH_MS);
    take();
    expect(scenes).toEqual(["curtain", "launch-card", "game-runs"]);
  });

  it("asks every player for a name in turn, also after a skip", () => {
    const scene = (events: ScreenEvent[]) => events.filter((e) => e.type === "scene").at(-1)?.name;
    m.screenScene("game-over");
    m.step(2 * REFRESH_MS);
    m.screenEvents();
    expect(scene(press(SCREEN_WIDTH / 2, 120, 100))).toBe("choose-name");
    // Skip at the top right for the first player, a name for the second and the third
    m.step(2 * REFRESH_MS);
    expect(scene(press(274, 24, 100))).toBe("choose-name");
    m.step(2 * REFRESH_MS);
    expect(scene(press(80, 73, 100))).toBe("choose-name");
    m.step(2 * REFRESH_MS);
    expect(scene(press(80, 73, 100))).toBe("high-scores");
  });

  it("plays a game's preview in its card once the card has stood, and stops it when the ring turns", () => {
    // ten frames of 220 × 64 in one color whose two bytes are equal, as the boot film's test has it
    const FILM_565 = 0xf8f8;
    m.cardFile("/games/classic/video/preview.rgb", new Uint8Array(220 * 64 * 2 * 10).fill(FILM_565 & 0xff));
    const at = (x: number, y: number) => {
      const px = m.screenPixels();
      return (px[2 * (y * SCREEN_WIDTH + x)] << 8) | px[2 * (y * SCREEN_WIDTH + x) + 1];
    };
    expect(m.screenScene("games")).toBe(true);
    m.step(PREVIEW_MS - 100);
    expect(at(SCREEN_WIDTH / 2, 80)).not.toBe(FILM_565);
    m.step(300);
    const named = m.screenEvents().filter((e) => e.type === "control").map((e) => e.name);
    expect(named).toContain("PREVIEW");
    // the cover inside the middle card's rim, from corner to corner
    expect(at(50, 48)).toBe(FILM_565);
    expect(at(269, 111)).toBe(FILM_565);
    expect(at(SCREEN_WIDTH / 2, 120)).not.toBe(FILM_565);
    // a tap on the right neighbor turns the ring to CROC RIDE, which cannot start and plays none
    input.down(1, { x: 300, y: 100 });
    m.step(100);
    input.up(1);
    m.step(SLIDE_MS + PREVIEW_MS + 200);
    expect(at(SCREEN_WIDTH / 2, 80)).not.toBe(FILM_565);
    m.screenScene("game-runs");
  });

  it("plays the boot film from the card over the whole screen, over and over, and takes it away with the scene", () => {
    // a film of 30 frames in one color whose two bytes are equal, so the byte order cannot change it
    const FILM_565 = 0xf8f8;
    const frames = 30;
    m.cardFile("/video/intro.rgb", new Uint8Array(SCREEN_WIDTH * SCREEN_HEIGHT * 2 * frames).fill(FILM_565 & 0xff));
    const px = () => m.screenPixels();
    const at = (x: number, y: number) => (px()[2 * (y * SCREEN_WIDTH + x)] << 8) | px()[2 * (y * SCREEN_WIDTH + x) + 1];
    expect(m.screenScene("boot-film")).toBe(true);
    m.step(300);
    expect(at(0, 0)).toBe(FILM_565);
    expect(at(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2)).toBe(FILM_565);
    // past the film's end it starts again
    m.step(1500);
    expect(at(SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1)).toBe(FILM_565);
    m.screenScene("game-runs");
    m.step(100);
    expect(at(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2)).not.toBe(FILM_565);
  });

  it("hands over the passes that sent pixels, a whole screen for a new scene", () => {
    m.screenFrames();
    m.screenScene("controls");
    m.step(2 * REFRESH_MS);
    const sent = m.screenFrames();
    expect(sent.length).toBeGreaterThan(0);
    expect(Math.max(...sent.map((f) => f.pixels))).toBe(SCREEN_WIDTH * SCREEN_HEIGHT);
    expect(m.screenFrames()).toEqual([]);
    m.screenScene("game-runs");
  });

  it("knows no scene of another name", () => {
    expect(m.screenScene("nothing")).toBe(false);
    expect(m.screenScene("test")).toBe(false);
  });
});
