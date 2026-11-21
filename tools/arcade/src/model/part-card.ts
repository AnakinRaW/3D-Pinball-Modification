// The technical card of a part of the drawing.
import type { Hardware } from "./config";
import type { Feature } from "./features";
import { lampsOn, shortName, stripName, type Item, type Playfield, type Strip } from "./playfield";

export interface Card {
  id: string;
  title: string;
  kind: string;
  description: string;
  fields: [string, string][];
  lamps: string[]; // the ids of the lamps that sit on the part
  features: string[]; // the names of the features the part shows
}

const mm = (p: [number, number]) => `${p[0].toFixed(1)}, ${p[1].toFixed(1)} mm`;

// The item a click lands on: the element itself or the nearest element around it that the drawing names.
export function resolveItem(pf: Playfield, ids: string[]): Item | undefined {
  for (const id of ids) {
    const item = pf.items.get(id);
    if (item) return item;
  }
  return undefined;
}

function stripCard(hw: Hardware, s: Strip, fields: [string, string][]) {
  const group = hw.map.strips[s.id];
  const g = group ? hw.config.groups[group] : undefined;
  fields.push(["Firmware strip", s.firmware]);
  if (group) fields.push(["Config", `lamps.${group}`]);
  fields.push(["Chain", g?.chain ?? s.chain]);
  fields.push(["LEDs", String(g?.leds ?? s.leds.length)]);
  const gaps = g?.gaps ?? [];
  if (gaps.length) fields.push(["Hidden LEDs", gaps.map(([a, n]) => `${a} to ${a + n - 1}`).join(", ")]);
}

export function card(pf: Playfield, hw: Hardware, features: Feature[], id: string): Card | undefined {
  const item = pf.items.get(id);
  if (!item) return undefined;
  const fields: [string, string][] = [];
  let kind = "";
  let description = "";
  let lamps: string[] = [];

  switch (item.kind) {
    case "placement": {
      kind = "Part";
      // a label that only repeats the id says nothing more
      description = item.label === item.id ? "" : item.label;
      fields.push(["Anchor", mm(item.point)]);
      const dev = hw.map.devices[item.id];
      if (dev) {
        fields.push(["Driver", dev.driver]);
        if (dev.channel !== undefined) fields.push(["Channel", String(dev.channel)]);
        fields.push(["Component", dev.component]);
        if (dev.driver === "IR sensing") {
          const c = hw.config.irChannels.find((x) => x.channel === dev.channel);
          if (c) fields.push(["Calibration", `clear ${c.clear_build}, ball ${c.ball_build}, order ${c.order}`]);
        }
        if (dev.driver === "Solenoid" && dev.channel !== undefined) {
          const ms = hw.config.pullMs[dev.channel];
          if (ms !== undefined) fields.push(["Pull time", `${ms} ms`]);
        }
      }
      lamps = lampsOn(pf, item.id).map((l) => l.id);
      break;
    }
    case "lamp": {
      kind = "Lamp";
      description = `${item.groupLabel}, on ${shortName(pf, item.on)}`;
      const span = hw.spans.get(item.id);
      if (span) {
        fields.push(["Config", `lamps.${span.section}`]);
        fields.push(["Chain", span.chain]);
        fields.push(["Playfield lamp", String(span.index)]);
        fields.push(["LEDs", span.count === 1 ? `${span.first}` : `${span.first} to ${span.first + span.count - 1}`]);
      } else {
        fields.push(["Chain", "not in the config"]);
      }
      fields.push(["Place", mm(item.point)]);
      break;
    }
    case "led": {
      const s = pf.items.get(item.strip) as Strip;
      kind = "Strip LED";
      description = stripName(s);
      fields.push(["LED", String(item.index)]);
      stripCard(hw, s, fields);
      const before = s.leds.filter((l) => l.index < item.index && l.hidden).length;
      fields.push(["Lamp in the strip", item.hidden ? "hidden, stays dark" : String(item.index - before)]);
      fields.push(["Place", mm(item.point)]);
      break;
    }
    case "strip": {
      kind = "Strip";
      description = item.label;
      stripCard(hw, item, fields);
      break;
    }
  }

  const strip = item.kind === "led" ? item.strip : undefined;
  const shows = features.filter(
    (f) => f.targets.includes(id) || lamps.some((l) => f.targets.includes(l)) || (strip && f.targets.some((t) => (pf.items.get(t) as { strip?: string })?.strip === strip)),
  );
  return {
    id,
    title: shortName(pf, id),
    kind,
    description,
    fields,
    lamps,
    features: shows.map((f) => f.name),
  };
}
