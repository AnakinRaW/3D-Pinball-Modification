// @vitest-environment jsdom
import { existsSync, readFileSync } from "node:fs";
import { resolve } from "node:path";
import { render } from "preact";
import { act } from "preact/test-utils";
import { afterEach, beforeEach, describe, expect, it } from "vitest";
import { App } from "../src/app";
import { sources } from "../src/sources";
import { createState, loadModel, type State } from "../src/state";
import { clampView, prepareSvg, zoomAt } from "../src/ui/Drawing";
import { studio } from "../src/ui/Studio";
import { panels, PANEL_MAX_PX, PANEL_MIN_PX } from "../src/ui/Splitter";
import { lightsOptions, loadLights } from "../src/sim/lights";
import { FIGURES as f } from "../src/model/current";
import { amps } from "../src/ui/CurrentReadout";

// the module's bytes from disk, since the test environment cannot fetch them
const wasm = resolve("src/wasm/simulator.wasm");
if (existsSync(wasm)) lightsOptions.wasmBinary = readFileSync(wasm);
const built = (await loadLights()) !== null;

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

describe("the views", () => {
  it("stand in the order Features, Games, Replays on the left, with Replays to come, and Studio on the right", () => {
    const tabs = [
      ...root.querySelectorAll(".tabs .tab"),
    ] as HTMLButtonElement[];
    expect(tabs.map((t) => t.textContent)).toEqual([
      "Features",
      "Games",
      "Replays",
      "Studio",
    ]);
    expect(tabs.map((t) => t.disabled)).toEqual([false, false, true, false]);
    const bar = [...$(".topbar")!.children].map((c) => c.className);
    expect(bar).toEqual(["brand", "tabs", "spacer", "tabs studio-tab"]);
  });

  it("put the build under the title and the theme switch outside the top bar", () => {
    expect($(".brand .build")!.textContent).toMatch(/^build /);
    expect($(".topbar .theme")).toBeNull();
    expect($(".app > .theme")).not.toBeNull();
  });
});

describe("the Games view", () => {
  const circus = {
    key: "card:circus",
    id: "circus",
    source: "card" as const,
    path: "games/circus/game.yml",
    title: "Big Top Circus",
    sections: ["name", "modes"],
    text: "",
  };
  const zoo = {
    key: "card:zoo",
    id: "zoo",
    source: "card" as const,
    path: "games/zoo/game.yml",
    title: "Zoo",
    sections: [],
    text: "",
    thumbnail: "blob:zoo",
  };

  it("filters by source, each with its count, and invites to a card while no game is found", () => {
    click($('[data-view="games"]'));
    const chips = [...root.querySelectorAll(".filter-chip")];
    expect(chips.map((c) => c.getAttribute("data-filter"))).toEqual([
      "all",
      "card",
      "firmware",
      "repo",
    ]);
    expect(chips.map((c) => c.querySelector(".count")!.textContent)).toEqual([
      "0",
      "0",
      "0",
      "0",
    ]);
    expect($(".game-hero.empty h1")!.textContent).toBe("No games yet");
    expect(root.querySelectorAll(".cover").length).toBe(0);
  });

  it("shows the picked game large, with its source, file and sections, above every game as a cover", () => {
    act(() => {
      state.cardGames.value = [circus, zoo];
    });
    click($('[data-view="games"]'));
    expect(root.querySelectorAll(".cover").length).toBe(2);
    click($('[data-game="card:circus"]'));
    const hero = $(".game-hero")!;
    expect(hero.querySelector("h1")!.textContent).toBe("Big Top Circus");
    expect(hero.querySelector(".badge")!.textContent).toBe("Memory card");
    expect(hero.textContent).toContain("games/circus/game.yml");
    expect(
      [...hero.querySelectorAll(".chip")].map((c) => c.textContent),
    ).toEqual(["name", "modes"]);
    expect($('[data-game="card:circus"]')!.classList.contains("selected")).toBe(
      true,
    );
  });

  it("shows only the games of the chosen source", () => {
    act(() => {
      state.cardGames.value = [circus];
    });
    click($('[data-view="games"]'));
    click($('[data-filter="repo"]'));
    expect(root.querySelectorAll(".cover").length).toBe(model.repoGames.length);
    click($('[data-filter="card"]'));
    expect(root.querySelectorAll(".cover").length).toBe(1);
  });

  it("shows a game's thumbnail on its cover, and its initials where it has none", () => {
    act(() => {
      state.cardGames.value = [circus, zoo];
    });
    click($('[data-view="games"]'));
    expect(
      $('[data-game="card:zoo"] img.game-thumb')!.getAttribute("src"),
    ).toBe("blob:zoo");
    expect(
      $('[data-game="card:circus"] .game-thumb.placeholder')!.textContent,
    ).toBe("BT");
  });
});

describe("the theme", () => {
  it("switches between light and dark", () => {
    const button = () => $(".theme") as HTMLButtonElement;
    const first = button().dataset.themeSwitch!;
    click(button());
    expect(document.documentElement.dataset.theme).toBe(first);
    click(button());
    expect(document.documentElement.dataset.theme).not.toBe(first);
  });
});

describe("the Features view", () => {
  it("lists the features on the machine and the ones in the firmware", () => {
    const placed = root.querySelectorAll("ol.features .feature");
    expect(placed.length).toBe(model.features.placed.length);
    expect(placed[0].textContent).toContain("Multiball");
    expect(root.querySelectorAll("ul.features .feature").length).toBe(
      model.features.unplaced.length,
    );
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
    expect(
      svg()
        .querySelector('[id="ball-scoop"]')!
        .classList.contains("sim-reveal"),
    ).toBe(true);
    expect(
      svg().querySelector('[id="lamp-scoop"]')!.classList.contains("sim-lit"),
    ).toBe(true);
    expect(
      svg()
        .querySelector('[id="ramp-orbit-left"]')!
        .classList.contains("sim-through"),
    ).toBe(true);
    const labels = [...root.querySelectorAll(".overlay .label")].map(
      (t) => t.textContent,
    );
    expect(labels).toEqual(["Ball scoop", "IR left lane", "Diffuser scoop"]);
    expect(root.querySelectorAll(".overlay .ring").length).toBe(3);
  });

  it("lights a strip's feature and lays no darkening over the drawing", () => {
    click($('[data-key="feature-10"]'));
    expect(
      svg()
        .querySelector('[id="lights-ceiling"]')!
        .classList.contains("sim-lit"),
    ).toBe(true);
    expect(root.querySelector(".overlay mask, .overlay .dim")).toBeNull();
    expect(root.querySelectorAll(".overlay .halo").length).toBe(
      root.querySelectorAll(".overlay .ring").length,
    );
  });

  it("leaves the apron out for the parts under it and puts it back afterwards", () => {
    const apron = () =>
      svg().querySelector('[id="apron"]')!.classList.contains("sim-hidden");
    click($('[data-key="feature-1"]'));
    expect(apron()).toBe(true);
    for (const ball of ["ball-store-front", "ball-store-back"])
      expect(
        svg().querySelector(`[id="${ball}"]`)!.classList.contains("sim-reveal"),
        ball,
      ).toBe(true);
    expect($(".feature-card")!.textContent).toContain(
      "seesaw separates the two balls",
    );
    click($('[data-key="feature-11"]'));
    expect(apron()).toBe(true);
    click($('[data-key="feature-2"]'));
    expect(apron()).toBe(false);
  });

  it("clears the marks of the feature selected before", () => {
    click($('[data-key="feature-2"]'));
    click($('[data-key="feature-8"]'));
    expect(
      svg().querySelector('[id="lamp-scoop"]')!.classList.contains("sim-lit"),
    ).toBe(false);
    expect(
      [...root.querySelectorAll(".overlay .label")].map((t) => t.textContent),
    ).toEqual(["Bell top", "Bell right"]);
  });

  it("picks no feature at first and leaves the drawing as it is", () => {
    expect($(".feature-card")!.textContent).toContain("Pick a feature");
    expect(svg().classList.contains("sim-features")).toBe(false);
    expect(root.querySelectorAll(".overlay .ring").length).toBe(0);
  });

  it("hides a ramp from the switch bar over the drawing and shows it again", () => {
    const pill = () =>
      $('.ramp-switch [data-ramp="ramp-blue"]') as HTMLButtonElement;
    const hidden = () =>
      svg().querySelector('[id="ramp-blue"]')!.classList.contains("sim-hidden");
    click(pill());
    expect(hidden()).toBe(true);
    expect(pill().getAttribute("aria-pressed")).toBe("false");
    click(pill());
    expect(hidden()).toBe(false);
    click($('.ramp-switch [data-ramp="all"]'));
    for (const r of model.hw.map.ramps)
      expect(
        svg().querySelector(`[id="${r.id}"]`)!.classList.contains("sim-hidden"),
        r.id,
      ).toBe(true);
    click($('.ramp-switch [data-ramp="all"]'));
    expect(state.hiddenRamps.value).toEqual([]);
  });

  it("steps to the next feature", () => {
    click($('[data-key="feature-1"]'));
    click([...root.querySelectorAll(".nav button")][1]);
    expect($(".feature-card h2")!.textContent).toBe(
      model.features.placed[1].name,
    );
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

// a part of the drawing, or its first shape where it has one, which a click lands on
const part = (id: string) =>
  svg().querySelector(`[id="${id}"]`)!.firstElementChild ??
  svg().querySelector(`[id="${id}"]`);

describe("a click into the drawing", () => {
  it("selects the feature that shows the part", () => {
    click(part("seal"));
    expect($(".feature-card h2")!.textContent).toBe("Rotating seal");
    expect($('[data-key="feature-3"]')!.classList.contains("selected")).toBe(
      true,
    );
  });

  it("selects a feature from a lamp on its part and from any LED of its strip", () => {
    click(svg().querySelector('[id="lamp-scoop"]'));
    expect($(".feature-card h2")!.textContent).toBe("Scoop");
    click(svg().querySelector('[id="led-a-5"]'));
    expect($(".feature-card h2")!.textContent).toBe("Ambient light");
  });

  it("opens the card of a part no feature shows", () => {
    click(part("seal"));
    click(part("slingshot-left"));
    expect(state.selected.value).toBe("");
    expect($(".part-card")).not.toBeNull();
    expect(state.inspected.value).toBe("slingshot-left");
  });

  it("clears the feature and the card on the cabinet", () => {
    click(part("slingshot-left"));
    click(part("cabinet"));
    expect($(".feature-card")!.textContent).toContain("Pick a feature");
    expect(state.inspected.value).toBeNull();
  });

  it("zooms with the buttons and back to the whole machine", () => {
    const zoom = () =>
      (root.querySelector(".zoom") as HTMLElement).style.transform;
    expect($('[data-action="zoom-reset"]')!.hasAttribute("disabled")).toBe(
      true,
    );
    click($('[data-action="zoom-in"]'));
    expect(zoom()).toContain("scale(1.5)");
    click($('[data-action="zoom-reset"]'));
    expect(zoom()).toContain("scale(1)");
  });
});

describe("a part's card", () => {
  it("opens from the feature card's chips, in place of the feature's card", () => {
    click($('[data-key="feature-6"]'));
    click($(".feature-card .chip"));
    const card = $(".part-card")!;
    expect($(".feature-card")).toBeNull();
    expect(card.querySelector("h2")!.textContent).toBe("Diffuser top lane 1");
    expect(card.textContent).toContain("Lamps on it");
    expect($(".overlay .inspect-ring")).not.toBeNull();
  });

  it("shows a sensor's driver and channel", () => {
    click($('[data-key="feature-5"]'));
    click(
      [...root.querySelectorAll(".feature-card .chip")].find(
        (c) => c.textContent === "IR ramp green",
      )!,
    );
    expect($(".part-card")!.textContent).toContain("IR sensing");
  });

  it("gives way to the feature's card when it closes or a part is clicked", () => {
    click($('[data-key="feature-6"]'));
    click($(".feature-card .chip"));
    click($(".part-card .close"));
    expect($(".feature-card h2")!.textContent).toBe("Top lanes");
    click($(".feature-card .chip"));
    click(part("seal"));
    expect($(".feature-card h2")!.textContent).toBe("Rotating seal");
    expect(state.inspected.value).toBeNull();
  });
});

describe.runIf(built)("the studio", () => {
  const open = async () => {
    click($('[data-view="studio"]'));
    for (let i = 0; i < 100 && studio.status.value === "loading"; i++)
      await new Promise((r) => setTimeout(r, 10));
    await act(async () => {});
    studio.running.value = false;
    click(
      [...root.querySelectorAll(".actions button")].find(
        (b) => b.textContent === "Clear",
      )!,
    );
  };
  const advance = () =>
    click(
      [...root.querySelectorAll(".pace button")].find(
        (b) => b.textContent === "+10 ms",
      )!,
    );

  it("runs the firmware's lamps and paints the playfield in the effect's color", async () => {
    await open();
    expect(studio.status.value).toBe("ready");
    expect($(".studio-controls")).not.toBeNull();
    click($('[data-action="start"]'));
    advance();
    const colors = state.glow.value!;
    expect(colors.get("lamp-scoop")!.some((v) => v > 0)).toBe(true);
    expect($(".studio-status")!.textContent).toContain("pulse");
  });

  it("picks the game it edits and opens its Lights, the other parts to come", async () => {
    act(() => {
      state.cardGames.value = [
        {
          key: "card:circus",
          id: "circus",
          source: "card",
          path: "games/circus/game.yml",
          title: "Circus",
          sections: [],
          text: "",
        },
      ];
    });
    await open();
    expect($(".studio-game")!.textContent).toContain("Pick a game");
    click($('[data-action="pick-game"]'));
    click($('.studio-controls [data-game="card:circus"]'));
    expect($(".studio-game")!.textContent).toContain("Circus");
    expect($(".studio-controls .game-list")).toBeNull();
    const tabs = [
      ...root.querySelectorAll(".sub-tabs .tab"),
    ] as HTMLButtonElement[];
    expect(tabs.map((t) => t.textContent)).toEqual([
      "Lights",
      "Slides",
      "Sounds",
      "Modes",
    ]);
    expect(tabs.map((t) => t.disabled)).toEqual([false, true, true, true]);
  });

  it("resizes a side panel by dragging its bar", async () => {
    await open();
    const before = panels.value.left;
    act(() => {
      $('[data-splitter="left"]')!.dispatchEvent(
        new MouseEvent("mousedown", { bubbles: true, clientX: 300 }),
      );
      window.dispatchEvent(new MouseEvent("mousemove", { clientX: 360 }));
      window.dispatchEvent(new MouseEvent("mouseup", {}));
    });
    expect(panels.value.left).toBe(Math.min(PANEL_MAX_PX, before + 60));
    expect(
      ($(".features-view") as HTMLElement).style.gridTemplateColumns,
    ).toContain(`${panels.value.left}px`);
    act(() => {
      $('[data-splitter="left"]')!.dispatchEvent(
        new MouseEvent("mousedown", { bubbles: true, clientX: 0 }),
      );
      window.dispatchEvent(new MouseEvent("mousemove", { clientX: -2000 }));
      window.dispatchEvent(new MouseEvent("mouseup", {}));
    });
    expect(panels.value.left).toBe(PANEL_MIN_PX);
  });

  it("numbers every playfield lamp as the firmware counts them", async () => {
    await open();
    const numbers = [...root.querySelectorAll(".lamp-numbers text")];
    expect(numbers.length).toBe(model.hw.spans.size);
    const scoop = numbers.find(
      (t) => t.getAttribute("data-lamp") === "lamp-scoop",
    )!;
    expect(scoop.textContent).toBe(
      String(model.hw.spans.get("lamp-scoop")!.index),
    );
    click($('[data-view="features"]'));
    expect(root.querySelectorAll(".lamp-numbers text").length).toBe(0);
  });

  it("clears the pick on a click into nothing", async () => {
    await open();
    click(svg().querySelector('[id="lamp-scoop"]'));
    expect($(".overlay .inspect-ring")).not.toBeNull();
    click(svg().querySelector('[id="cabinet"]')!.firstElementChild);
    expect(state.picked.value).toBeNull();
    expect($(".overlay .inspect-ring")).toBeNull();
    click(svg().querySelector('[id="lamp-scoop"]'));
    click($(".drawing-host"));
    expect(state.picked.value).toBeNull();
  });

  it("hides a ramp and shows it again", async () => {
    await open();
    const box = () => $('[data-ramp="ramp-blue"]') as HTMLInputElement;
    const hidden = () =>
      svg().querySelector('[id="ramp-blue"]')!.classList.contains("sim-hidden");
    click(box());
    expect(hidden()).toBe(true);
    click(box());
    expect(hidden()).toBe(false);
  });

  it("takes a lamp clicked in the drawing as the target", async () => {
    await open();
    click(svg().querySelector('[id="lamp-scoop"]'));
    const select = $('[data-field="target"]') as HTMLSelectElement;
    expect(select.value).toBe("range");
    expect($(".studio-status")!.textContent).toContain(
      `Lamp scoop · playfield lamp ${model.hw.spans.get("lamp-scoop")!.index}`,
    );
  });

  it("removes a layer and leaves the lamps dark", async () => {
    await open();
    click($('[data-action="start"]'));
    click($(".layers .close"));
    advance();
    expect(
      [...state.glow.value!.values()].every(([r, g, b]) => r + g + b === 0),
    ).toBe(true);
    expect(studio.layers.value).toEqual([]);
  });

  it("reads the current of the frame the driver sent", async () => {
    await open();
    advance();
    // every LED of the config's lamp groups dark, at its dark draw
    const leds = Object.values(model.hw.config.groups).reduce(
      (n, g) => n + g.leds,
      0,
    );
    expect($('[data-testid="current-led"]')!.textContent).toBe(
      amps((leds * f.pm_dark.value) / f.v_rail.value),
    );
    expect($('[data-testid="current-cap"]')!.textContent).toBe("under cap");
  });

  it("marks the whole layout at full white with three coils past the 3 A adapter and inside the 6 A one", async () => {
    await open();
    const choose = (sel: string, value: string) =>
      act(() => {
        const el = $(sel) as HTMLSelectElement | HTMLInputElement;
        el.value = value;
        el.dispatchEvent(
          new Event(el instanceof HTMLSelectElement ? "change" : "input", {
            bubbles: true,
          }),
        );
      });
    const adapter = (a: number) =>
      click(
        [...root.querySelectorAll(".studio-controls .seg button")].find(
          (b) => b.textContent === `${a} A`,
        )!,
      );
    adapter(3);
    choose(
      ".studio-controls .field:has(> select option[value=set]) select",
      "set",
    );
    choose('.studio-controls .paint input[type="color"]', "#ffffff");
    for (const target of ["playfield", "ambient"]) {
      choose('[data-field="target"]', target);
      click($('[data-action="start"]'));
    }
    advance();
    const led = () => $('[data-testid="current-led"]')!.textContent!;
    expect(led()).not.toBe(amps(0));
    expect($('[data-testid="current-cap"]')!.textContent).toBe("under cap");
    click($('[data-testid="coil-most"]'));
    expect($('[data-testid="current-coils"]')!.textContent).toBe(
      amps(f.i_coils_at_once.value),
    );
    expect($('[data-testid="current-over"]')).not.toBeNull();
    adapter(6);
    advance();
    expect($('[data-testid="current-over"]')).toBeNull();
    expect($('[data-testid="current-worst"]')!.textContent).toContain(
      amps(f.i_most_6a.value),
    );
    adapter(3);
  });

  it("goes back to the features without the firmware's colors", async () => {
    await open();
    click($('[data-view="features"]'));
    expect(state.glow.value).toBeNull();
    expect($(".feature-card")).not.toBeNull();
  });
});

describe("the playfield's zoom", () => {
  it("keeps the point under the pointer in place", () => {
    const v = zoomAt({ s: 1, x: 0, y: 0 }, 2, 300, 200, 800, 600);
    expect(v.s).toBe(2);
    // the drawing's point under the pointer maps back to the same screen point
    expect((300 - v.x) / v.s).toBe(300);
    expect((200 - v.y) / v.s).toBe(200);
  });

  it("goes no further out than the whole machine and pans no edge into view", () => {
    expect(zoomAt({ s: 2, x: -100, y: -100 }, 0.25, 0, 0, 800, 600)).toEqual({
      s: 1,
      x: 0,
      y: 0,
    });
    expect(clampView({ s: 2, x: 50, y: -900 }, 800, 600)).toEqual({
      s: 2,
      x: 0,
      y: -600,
    });
  });
});
