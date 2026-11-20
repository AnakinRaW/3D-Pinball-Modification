import { describe, expect, it } from "vitest";
import { configProblems, lampSpans, SECTIONS } from "../src/model/config";
import { LABEL_FONT, placeLabels, planHighlight, type Ring } from "../src/model/highlight";
import { card, resolveItem } from "../src/model/part-card";
import { parseFeatureTables, parseInline, parseReadmeFeatures, plain, resolvePath } from "../src/model/markdown";
import { hiddenStretches, type Led, type Placement } from "../src/model/playfield";
import { sources } from "../src/sources";
import { loadModel } from "../src/state";

const model = loadModel(sources);
const { pf, hw } = model;

// every attribute set of an element of playfield.svg by id
function svgElement(id: string): string | undefined {
  return new RegExp(`<[a-z]+ [^>]*id="${id}"[^>]*>`).exec(sources.playfieldSvg)?.[0];
}
function attr(el: string, name: string): number {
  return Number(new RegExp(`\\s${name}="([^"]+)"`).exec(el)![1]);
}

describe("markdown", () => {
  it("resolves links from the docs folder", () => {
    expect(resolvePath("docs", "firmware/components/drain.md")).toBe("docs/firmware/components/drain.md");
    expect(resolvePath("docs", "../README.md#x")).toBe("README.md#x");
  });

  it("splits bold text and links", () => {
    const t = parseInline("**Two balls** may be in play. [Drain](firmware/components/drain.md)", "docs");
    expect(t).toEqual([
      { kind: "strong", text: "Two balls" },
      { kind: "text", text: " may be in play. " },
      { kind: "link", text: "Drain", path: "docs/firmware/components/drain.md" },
    ]);
  });

  it("reads every row of features.md under its section", () => {
    const rows = parseFeatureTables(sources.featuresMd);
    expect(rows.map((r) => r.name)).toContain("Multiball");
    expect(rows.find((r) => r.name === "Scoop")?.section).toBe("Playfield");
    expect(rows.some((r) => r.name === "Feature")).toBe(false);
  });

  it("reads the README's numbered feature list", () => {
    const list = parseReadmeFeatures(sources.readmeMd);
    expect(list.length).toBeGreaterThan(0);
    expect(list[0]).toMatchObject({ n: 1, name: "Multiball" });
    expect(list[0].summary).toMatch(/^Two balls/);
  });
});

describe("features", () => {
  it("lists the README's features with their places on the machine", () => {
    const readme = parseReadmeFeatures(sources.readmeMd);
    expect(model.features.placed.map((f) => f.name)).toEqual(readme.map((r) => r.name));
  });

  it("finds a features.md row for every feature on the machine", () => {
    for (const f of model.features.placed) expect(f.rows.length, f.name).toBeGreaterThan(0);
    const screen = model.features.placed.find((f) => f.name === "Screen and touch")!;
    expect(screen.rows.map((r) => r.name)).toEqual(["Screen", "Touch"]);
  });

  it("leaves the links to the repository's documents out of the texts", () => {
    for (const f of model.all)
      for (const r of f.rows) {
        expect([...r.stock, ...r.modification].some((t) => t.kind === "link"), f.name).toBe(false);
        expect(plain(r.modification), f.name).not.toMatch(/[\s,]$/);
      }
  });

  it("uses every row of features.md exactly once", () => {
    const rows = parseFeatureTables(sources.featuresMd).map((r) => r.name);
    const used = model.all.flatMap((f) => f.rows.map((r) => r.name));
    expect([...used].sort()).toEqual([...rows].sort());
  });

  it("points every feature at parts the drawing holds", () => {
    for (const f of model.features.placed) for (const t of f.targets) expect(svgElement(t), t).toBeDefined();
  });
});

describe("playfield", () => {
  it("places every strip LED where the composed drawing has it", () => {
    for (const s of pf.strips) {
      for (const led of s.leds) {
        const el = svgElement(led.id)!;
        expect(attr(el, "cx")).toBeCloseTo(led.point[0], 2);
        expect(attr(el, "cy")).toBeCloseTo(led.point[1], 2);
        expect(el.includes("display:none"), led.id).toBe(led.hidden);
      }
    }
  });

  it("places every lamp where the composed drawing has it", () => {
    expect(pf.lamps.length).toBeGreaterThan(0);
    for (const l of pf.lamps) {
      const el = svgElement(l.id)!;
      expect(attr(el, "cx")).toBeCloseTo(l.point[0], 2);
      expect(attr(el, "cy")).toBeCloseTo(l.point[1], 2);
    }
  });

  it("anchors every placement where its transform puts it", () => {
    for (const p of pf.placements) {
      const el = svgElement(p.id)!;
      const t = /transform="([^"]*)"/.exec(el)![1];
      // the frame comes first where a placement has one, then the anchor
      const translates = [...t.matchAll(/translate\(([-\d.]+) ([-\d.]+)\)/g)];
      if (translates.length > 1) continue;
      expect(Number(translates[0][1])).toBeCloseTo(p.point[0], 2);
      expect(Number(translates[0][2])).toBeCloseTo(p.point[1], 2);
    }
  });
});

describe("config", () => {
  it("is valid under the lamps component's rules", () => {
    expect(configProblems(hw.config)).toEqual([]);
  });

  it("gives each strip the LEDs and the hidden stretches the drawing has", () => {
    for (const [strip, group] of Object.entries(hw.map.strips)) {
      const s = pf.strips.find((x) => x.id === strip)!;
      const g = hw.config.groups[group];
      expect(g.leds, strip).toBe(s.leds.length);
      expect(g.chain, strip).toBe(s.chain);
      expect(g.gaps, strip).toEqual(hiddenStretches(s));
    }
  });

  it("names every lamp of the drawing once, in a section on the chain its lights group names", () => {
    const named = Object.values(hw.map.lamps).flat();
    expect([...named].sort()).toEqual(pf.lamps.map((l) => l.id).sort());
    for (const section of SECTIONS) {
      const ids = hw.map.lamps[section];
      expect(ids.length, section).toBe(hw.config.groups[section].lamps.length);
      for (const id of ids) {
        const chain = /chain ([A-F])\b/.exec(pf.lamps.find((l) => l.id === id)!.groupLabel)?.[1];
        if (chain) expect(hw.spans.get(id)!.chain, id).toBe(chain);
      }
    }
  });

  it("cuts the sections into lamps that skip hidden LEDs", () => {
    const spans = lampSpans({
      adapterA: 3,
      groups: { playfield_lower: { chain: "D", leds: 6, gaps: [[1, 2]], lamps: [1, 2, 1] } },
      irChannels: [],
      pullMs: [],
    });
    expect(spans[0]).toEqual([
      { section: "playfield_lower", chain: "D", index: 0, first: 0, count: 1 },
      { section: "playfield_lower", chain: "D", index: 1, first: 3, count: 2 },
      { section: "playfield_lower", chain: "D", index: 2, first: 5, count: 1 },
    ]);
  });

  it("maps devices to parts of the drawing, and IR channels to channels the config calibrates", () => {
    const channels = hw.config.irChannels.map((c) => c.channel);
    for (const [id, dev] of Object.entries(hw.map.devices)) {
      expect(pf.items.has(id), id).toBe(true);
      if (dev.driver === "IR sensing") expect(channels, id).toContain(dev.channel);
    }
  });

});

describe("highlight", () => {
  it("lights the lamps on a feature's parts, reveals its ball and looks through the ramps above", () => {
    const h = planHighlight(pf, ["ball-scoop", "ir-left-lane", "diffuser-scoop"]);
    expect(h.marks.map((m) => m.label)).toEqual(["Ball scoop", "IR left lane", "Diffuser scoop"]);
    expect(h.lit).toContain("lamp-scoop");
    expect(h.reveal).toEqual(["ball-scoop"]);
    expect(h.through).toEqual(expect.arrayContaining(["ramp-orbit-left", "cover-left"]));
  });

  it("names ramps the drawing holds", () => {
    expect(hw.map.ramps.length).toBeGreaterThan(0);
    for (const r of hw.map.ramps) expect(pf.items.get(r)?.kind, r).toBe("placement");
  });

  it("shows the two balls in the drain's store with the store", () => {
    const h = planHighlight(pf, ["drain-ball-store"], hw.map.covers, hw.map.reveals);
    expect(h.reveal).toEqual(["ball-store-front", "ball-store-back"]);
    for (const balls of Object.values(hw.map.reveals)) for (const b of balls) expect((pf.items.get(b) as { hidden?: boolean })?.hidden, b).toBe(true);
  });

  it("leaves out the cover over a feature's parts, and only then", () => {
    const covers = hw.map.covers;
    expect(planHighlight(pf, ["drain-ball-store"], covers).hidden).toEqual(["apron"]);
    expect(planHighlight(pf, ["speaker"], covers).hidden).toEqual(["apron"]);
    expect(planHighlight(pf, ["seal"], covers).hidden).toEqual([]);
    for (const [cover, under] of Object.entries(covers)) {
      expect(pf.items.has(cover), cover).toBe(true);
      for (const p of under) expect(pf.items.has(p), p).toBe(true);
    }
  });

  it("lights a whole strip for one of its LEDs and labels the strip once", () => {
    const h = planHighlight(pf, ["led-a-21", "led-b-21", "led-c-1", "led-c-24"]);
    expect(h.marks.map((m) => m.label)).toEqual(["Perimeter left", "Perimeter right", "Diorama ceiling"]);
    expect(h.lit).toEqual(expect.arrayContaining(["lights-perimeter-left", "lights-perimeter-right", "lights-ceiling"]));
  });

  it("keeps labels apart and inside the drawing", () => {
    const vb = pf.viewBox;
    for (const f of model.features.placed) {
      const rings: Ring[] = planHighlight(pf, f.targets).marks.map((m) => {
        const it = pf.items.get(m.id) as Placement | Led;
        return { id: m.id, label: m.label, cx: it.point[0], cy: it.point[1], r: 6 };
      });
      const labels = placeLabels(rings, vb, 115);
      for (const a of labels) {
        const [a0, a1] = a.anchor === "end" ? [a.x - a.width, a.x] : [a.x, a.x + a.width];
        expect(a0, `${f.name}: ${a.label}`).toBeGreaterThanOrEqual(vb[0]);
        expect(a1, `${f.name}: ${a.label}`).toBeLessThanOrEqual(vb[0] + vb[2]);
        // the label's plate covers no other part's ring
        for (const g of rings) {
          if (g.id === a.id) continue;
          const coversX = a0 < g.cx + g.r && a1 > g.cx - g.r;
          const coversY = a.y - LABEL_FONT < g.cy + g.r && a.y + 2 > g.cy - g.r;
          expect(coversX && coversY, `${f.name}: ${a.label} covers ${g.label}`).toBe(false);
        }
        for (const b of labels) {
          if (a === b) continue;
          const [b0, b1] = b.anchor === "end" ? [b.x - b.width, b.x] : [b.x, b.x + b.width];
          const overlapX = !(a1 < b0 || b1 < a0);
          if (overlapX) expect(Math.abs(a.y - b.y), `${f.name}: ${a.label} / ${b.label}`).toBeGreaterThan(5);
        }
      }
    }
  });
});

describe("part card", () => {
  it("resolves a click to the nearest element the drawing names", () => {
    expect(resolveItem(pf, ["seal-crossbar", "seal", "layer-0"])?.id).toBe("seal");
  });

  it("shows a lamp's chain and LED from the config", () => {
    const c = card(pf, hw, model.features.placed, "lamp-scoop")!;
    const f = Object.fromEntries(c.fields);
    expect(f.Chain).toBe("E");
    expect(f.Config).toBe("lamps.playfield_middle");
    expect(f.LEDs).toBe(String(hw.spans.get("lamp-scoop")!.first));
  });

  it("shows an IR sensor's channel and calibration", () => {
    const c = card(pf, hw, model.features.placed, "ir-top-lane-2")!;
    const f = Object.fromEntries(c.fields);
    expect(f.Driver).toBe("IR sensing");
    expect(f.Channel).toBe(String(hw.map.devices["ir-top-lane-2"].channel));
    expect(f.Calibration).toMatch(/^clear \d+, ball \d+/);
  });

  it("shows a hidden strip LED as dark and counts the strip's lamps past a hidden stretch", () => {
    const [first, count] = hw.config.groups.perimeter_left.gaps[0];
    const dark = Object.fromEntries(card(pf, hw, [], `led-a-${first}`)!.fields);
    expect(dark["Lamp in the strip"]).toMatch(/hidden/);
    const after = Object.fromEntries(card(pf, hw, [], `led-a-${first + count}`)!.fields);
    expect(after["Lamp in the strip"]).toBe(String(first));
  });

  it("names the features a part shows and the lamps on it", () => {
    const c = card(pf, hw, model.features.placed, "diffuser-scoop")!;
    expect(c.features).toEqual(["Scoop"]);
    expect(c.lamps).toEqual(["lamp-scoop"]);
  });
});
