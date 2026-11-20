// @vitest-environment jsdom
import { render } from "preact";
import { act } from "preact/test-utils";
import { afterEach, beforeEach, describe, expect, it } from "vitest";
import { App } from "../src/app";
import { sources } from "../src/sources";
import { createState, loadModel, type State } from "../src/state";
import { prepareSvg } from "../src/ui/Drawing";

const model = loadModel(sources);
let root: HTMLElement;
let state: State;

beforeEach(() => {
  root = document.createElement("div");
  document.body.appendChild(root);
  state = createState(model);
  act(() => render(<App model={model} state={state} />, root));
});

afterEach(() => {
  render(null, root);
  root.remove();
});

const $ = (sel: string) => root.querySelector(sel);
const click = (el: Element | null) =>
  act(() => {
    el!.dispatchEvent(new MouseEvent("click", { bubbles: true }));
  });
const svg = () => $("svg.drawing")!;

describe("the drawing", () => {
  it("fills its box", () => {
    const head = prepareSvg(sources.playfieldSvg).slice(0, 300);
    expect(head.startsWith("<svg")).toBe(true);
    expect(head).not.toMatch(/\swidth="/);
    expect(head).toContain('class="drawing"');
  });
});

describe("the Features view", () => {
  it("lists the features on the machine and the ones in the firmware", () => {
    const placed = root.querySelectorAll("ol.features .feature");
    expect(placed.length).toBe(model.features.placed.length);
    expect(placed[0].textContent).toContain("Multiball");
    expect(root.querySelectorAll("ul.features .feature").length).toBe(model.features.unplaced.length);
  });

  it("shows the selected feature's card with the stock machine and the mod", () => {
    click($('[data-key="feature-2"]'));
    const card = $(".feature-card")!;
    expect(card.querySelector("h2")!.textContent).toBe("Scoop");
    expect(card.textContent).toContain("On the stock machine");
    expect(card.textContent).toContain("throws it out with a coil");
    expect(card.textContent!.trim()).not.toMatch(/,$/);
  });

  it("lights and labels the selected feature's parts in the drawing", () => {
    click($('[data-key="feature-2"]'));
    expect(svg().querySelector('[id="ball-scoop"]')!.classList.contains("sim-reveal")).toBe(true);
    expect(svg().querySelector('[id="lamp-scoop"]')!.classList.contains("sim-lit")).toBe(true);
    expect(svg().querySelector('[id="ramp-orbit-left"]')!.classList.contains("sim-through")).toBe(true);
    const labels = [...root.querySelectorAll(".overlay .label")].map((t) => t.textContent);
    expect(labels).toEqual(["Ball scoop", "IR left lane", "Diffuser scoop"]);
    expect(root.querySelectorAll(".overlay .ring").length).toBe(3);
  });

  it("leaves every visible LED of a lit strip bright", () => {
    click($('[data-key="feature-10"]'));
    const visible = model.pf.strips.reduce((n, s) => n + s.leds.filter((l) => !l.hidden).length, 0);
    expect(root.querySelectorAll(".overlay .glow-hole").length).toBe(visible);
    expect(svg().querySelector('[id="lights-ceiling"]')!.classList.contains("sim-lit")).toBe(true);
  });

  it("leaves the apron out for the parts under it and puts it back afterwards", () => {
    const apron = () => svg().querySelector('[id="apron"]')!.classList.contains("sim-hidden");
    click($('[data-key="feature-1"]'));
    expect(apron()).toBe(true);
    click($('[data-key="feature-11"]'));
    expect(apron()).toBe(true);
    click($('[data-key="feature-2"]'));
    expect(apron()).toBe(false);
  });

  it("clears the marks of the feature selected before", () => {
    click($('[data-key="feature-2"]'));
    click($('[data-key="feature-8"]'));
    expect(svg().querySelector('[id="lamp-scoop"]')!.classList.contains("sim-lit")).toBe(false);
    expect([...root.querySelectorAll(".overlay .label")].map((t) => t.textContent)).toEqual(["Bell top", "Bell right"]);
  });

  it("steps to the next feature", () => {
    click([...root.querySelectorAll(".nav button")][1]);
    expect($(".feature-card h2")!.textContent).toBe(model.features.placed[1].name);
  });
});

describe("standing alone", () => {
  it("links to nothing outside the app", () => {
    for (const k of model.all.map((f) => f.key)) {
      click($(`[data-key="${k}"]`));
      expect(root.querySelectorAll("a").length, k).toBe(0);
    }
    click(svg().querySelector('[id="seal"]')!.firstElementChild);
    expect(root.querySelectorAll("a").length).toBe(0);
  });
});

describe("a part's card", () => {
  it("shows on a click into the drawing, in place of the feature's card", () => {
    click(svg().querySelector('[id="ir-top-lane-2"]')!.firstElementChild ?? svg().querySelector('[id="ir-top-lane-2"]'));
    const card = $(".part-card")!;
    expect($(".feature-card")).toBeNull();
    expect(card.querySelector("h2")!.textContent).toBe("IR top lane 2");
    expect(card.textContent).toContain("IR sensing");
    expect($(".overlay .inspect-ring")).not.toBeNull();
  });

  it("opens from the feature card's chips", () => {
    click($('[data-key="feature-6"]'));
    click($(".feature-card .chip"));
    expect($(".part-card h2")!.textContent).toBe("Diffuser top lane 1");
    expect($(".part-card")!.textContent).toContain("Lamps on it");
  });

  it("gives way to the feature's card when it closes or a feature is selected", () => {
    click(svg().querySelector('[id="seal"]')!.firstElementChild);
    click($(".part-card .close"));
    expect($(".feature-card")).not.toBeNull();
    click(svg().querySelector('[id="seal"]')!.firstElementChild);
    click($('[data-key="feature-3"]'));
    expect($(".feature-card h2")!.textContent).toBe("Rotating seal");
    expect(state.inspected.value).toBeNull();
  });
});
