// @vitest-environment jsdom
import { render } from "preact";
import { act } from "preact/test-utils";
import { afterEach, beforeEach, describe, expect, it } from "vitest";
import { loadHardware } from "../src/model/config";
import { FIGURES as f, type Adapter } from "../src/model/current";
import { sources } from "../src/sources";
import { amps, CurrentReadout } from "../src/ui/CurrentReadout";

const hw = loadHardware(sources.configYml, sources.machineYml);
let root: HTMLElement;

const dark = [new Uint8Array(3 * 10)];
const show = (nowMs: number, adapter: Adapter = 3, chains = dark, cap = 255) =>
  act(() => render(<CurrentReadout hw={hw} adapter={adapter} chains={chains} capBrightness={cap} nowMs={nowMs} />, root));
const text = (id: string) => root.querySelector(`[data-testid="${id}"]`)!.textContent;
const click = (id: string) => act(() => (root.querySelector(`[data-testid="${id}"]`) as HTMLElement).click());

beforeEach(() => {
  root = document.createElement("div");
  document.body.appendChild(root);
});
afterEach(() => {
  render(null, root);
  root.remove();
});

describe("the current readout", () => {
  it("shows the frame's LEDs and the loads that always draw", () => {
    show(0);
    expect(text("current-led")).toBe(amps((10 * f.pm_dark.value) / f.v_rail.value));
    expect(text("current-cap")).toBe("under cap");
    expect(text("current-total")).toBe(amps((10 * f.pm_dark.value) / f.v_rail.value + f.i_permanent.value));
    expect(root.querySelector("[data-testid=current-over]")).toBeNull();
  });

  it("marks a frame the cap dimmed", () => {
    show(0, 3, dark, 180);
    expect(text("current-cap")).toBe("cap 180/255");
  });

  it("adds a coil for its pull time on the frames' clock", () => {
    show(100);
    click("coil-3");
    show(100);
    expect(text("current-coils")).toBe(amps(f.i_coil_scoop.value));
    show(100 + 29);
    expect(text("current-coils")).toBe(amps(f.i_coil_scoop.value));
    show(100 + 30);
    expect(text("current-coils")).toBe(amps(0));
    expect(text("current-peak")).toBe(amps(f.i_coil_scoop.value + f.i_permanent.value + (10 * f.pm_dark.value) / f.v_rail.value));
  });

  it("marks the total past the 3 A adapter with the lighting at its cap and three coils", () => {
    // a frame whose FastLED sum is the 3 A cap: 2000 mA at 5 V are 10 W, which dark draws alone reach
    const leds = (f.i_budget.value * f.v_rail.value) / f.pm_dark.value;
    show(0, 3, [new Uint8Array(3 * leds)]);
    click("coil-most");
    show(0, 3, [new Uint8Array(3 * leds)]);
    expect(text("current-coils")).toBe(amps(f.i_coils_at_once.value));
    expect(text("current-over")).toBe(`+${amps(f.i_over_3a.value)} past the adapter`);
    show(0, 6, [new Uint8Array(3 * leds)]);
    expect(root.querySelector("[data-testid=current-over]")).toBeNull();
  });

  it("states the design's worst cases for the selected adapter", () => {
    show(0, 3);
    const worst = text("current-worst")!;
    expect(worst).toContain(amps(f.i_most_3a.value));
    expect(worst).toContain(`+${amps(f.i_over_3a.value)} for ≤ ${f.t_on_max.value} ms`);
    expect(worst).toContain(amps(f.i_white_3a.value));
    show(0, 6);
    expect(text("current-worst")).toContain(amps(f.i_most_6a.value));
    expect(text("current-worst")).not.toContain("for ≤");
  });

  it("lists the loads that always draw, audio and display among them, under their sum", () => {
    show(0);
    expect(text("current-always")).toBe(amps(f.i_permanent.value));
    const rows = [...root.querySelectorAll(".current-base th")].map((t) => t.textContent);
    expect(rows).toEqual(["Teensy", "Audio", "IR board", "Display", "Servo at rest"]);
  });

  it("draws the servo's stall current for its longest move and flags a coil beside it", () => {
    show(0);
    click("servo");
    click("coil-0");
    show(0);
    expect(text("current-servo")).toBe(amps(f.i_stall.value));
    expect(root.textContent).toContain("with a coil");
    show(f.t_move_max.value - 1);
    expect(text("current-servo")).toBe(amps(f.i_stall.value));
    show(f.t_move_max.value);
    expect(text("current-servo")).toBe(amps(0));
  });

  it("stops the servo on a second click during its move", () => {
    show(0);
    click("servo");
    show(10);
    expect(text("current-servo")).toBe(amps(f.i_stall.value));
    click("servo");
    show(20);
    expect(text("current-servo")).toBe(amps(0));
  });
});
