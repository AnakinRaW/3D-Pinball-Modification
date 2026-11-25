// The machine's display in the WebAssembly module: the firmware's screen component draws into a
// simulated display, and the firmware's touch driver reads a simulated touch controller. This file
// turns the display's RGB565 pixels into a canvas's colors and a pointer on the canvas into the
// finger the touch controller reports.

export const SCREEN_WIDTH = 320;
export const SCREEN_HEIGHT = 240;
export const SCALES = [1, 2, 3] as const;
export type Scale = (typeof SCALES)[number];

export type ScreenEventType =
  | "touchDown"
  | "touchMoved"
  | "touchHeld"
  | "touchUp"
  | "screenTapped"
  | "screenHeld"
  | "buttonClicked"
  | "edgeTouched"
  | "swipeStarted"
  | "swipeLetGo"
  | "menuOpened"
  | "menuClosed"
  | "sound"
  | "lights"
  | "machine"
  | "endGame"
  | "close";

export interface ScreenEvent {
  type: ScreenEventType;
  x: number;
  y: number;
  ms: number;
}

// the module's screen functions, as firmware/sim/screen-simulator.cpp binds them
export interface ScreenModule {
  screenBegin(): boolean;
  touch(down: boolean, x: number, y: number): void;
  screenPixels(): Uint8Array;
  screenRectangles(): number;
  screenEvents(): ScreenEvent[];
}

// Turns the display's pixels, RGB565 with the high byte first, into opaque RGBA. A channel of 5 or 6
// bits spreads over 8 bits with its upper bits repeated below, so white stays 255.
export function rgb565ToRgba(pixels: Uint8Array, out: Uint8ClampedArray): void {
  for (let i = 0, o = 0; i + 1 < pixels.length; i += 2, o += 4) {
    const v = (pixels[i] << 8) | pixels[i + 1];
    const r = v >> 11;
    const g = (v >> 5) & 0x3f;
    const b = v & 0x1f;
    out[o] = (r << 3) | (r >> 2);
    out[o + 1] = (g << 2) | (g >> 4);
    out[o + 2] = (b << 3) | (b >> 2);
    out[o + 3] = 255;
  }
}

// The screen's pixel under a point of the page, held inside the screen. box is the canvas's place
// on the page, drawn at any size.
export function screenPoint(clientX: number, clientY: number, box: { left: number; top: number; width: number; height: number }): { x: number; y: number } {
  const clamp = (v: number, max: number) => Math.min(max, Math.max(0, Math.floor(v)));
  return {
    x: clamp(((clientX - box.left) * SCREEN_WIDTH) / box.width, SCREEN_WIDTH - 1),
    y: clamp(((clientY - box.top) * SCREEN_HEIGHT) / box.height, SCREEN_HEIGHT - 1),
  };
}

// Follows one pointer on the screen, a mouse button or a finger, and reports it to the module as the
// touch controller's finger. A second pointer is ignored while the first is down, as the firmware
// reads one touch point.
export class TouchInput {
  private pointer: number | null = null;
  private last = { x: 0, y: 0 };

  constructor(private readonly module: Pick<ScreenModule, "touch">) {}

  down(pointerId: number, at: { x: number; y: number }): boolean {
    if (this.pointer !== null) return false;
    this.pointer = pointerId;
    this.last = at;
    this.module.touch(true, at.x, at.y);
    return true;
  }

  move(pointerId: number, at: { x: number; y: number }): void {
    if (pointerId !== this.pointer) return;
    this.last = at;
    this.module.touch(true, at.x, at.y);
  }

  // A pointer let go, cancelled or lost lifts the finger where it was last.
  up(pointerId: number): void {
    if (pointerId !== this.pointer) return;
    this.pointer = null;
    this.module.touch(false, this.last.x, this.last.y);
  }

  // Lifts a finger that is down, whichever pointer holds it.
  release(): void {
    if (this.pointer !== null) this.up(this.pointer);
  }

  get touching(): boolean {
    return this.pointer !== null;
  }
}
