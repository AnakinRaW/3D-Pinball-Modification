// The features the Features view lists: the README's numbered list with the places of playfield.json, the
// rows of docs/features.md that describe each, and the rows that have no place on the machine.
import { parseFeatureTables, parseReadmeFeatures, withoutLinks, type FeatureRow } from "./markdown";
import type { Item, Playfield } from "./playfield";

export interface Feature {
  key: string;
  n?: number; // the number in the README's list, for a feature with a place on the machine
  name: string;
  summary: string;
  section: string;
  rows: FeatureRow[];
  targets: string[];
}

const norm = (s: string) => s.trim().toLowerCase();

// The rows of docs/features.md a feature name stands for: the row of that name, or one row for each name
// that "and" joins, as "Screen and touch" joins Screen and Touch.
export function rowsFor(name: string, rows: FeatureRow[]): FeatureRow[] {
  const exact = rows.filter((r) => norm(r.name) === norm(name));
  if (exact.length) return exact;
  return name
    .split(/\s+and\s+/i)
    .map((part) => rows.find((r) => norm(r.name) === norm(part)))
    .filter((r): r is FeatureRow => r !== undefined);
}

export interface Features {
  placed: Feature[];
  unplaced: Feature[];
}

export function loadFeatures(pf: Playfield, featuresMd: string, readmeMd: string): Features {
  // the app stands alone, so the rows keep their text and leave the links to the repository's documents out
  const rows = parseFeatureTables(featuresMd).map((r) => ({ ...r, stock: withoutLinks(r.stock), modification: withoutLinks(r.modification) }));
  const readme = parseReadmeFeatures(readmeMd);
  const used = new Set<FeatureRow>();
  const placed = pf.features.map((f): Feature => {
    const mine = rowsFor(f.feature, rows);
    mine.forEach((r) => used.add(r));
    const item = readme.find((r) => r.n === f.n);
    return {
      key: `feature-${f.n}`,
      n: f.n,
      name: f.feature,
      summary: item?.summary ?? "",
      section: mine[0]?.section ?? "",
      rows: mine,
      targets: f.targets,
    };
  });
  const unplaced = rows
    .filter((r) => !used.has(r))
    .map(
      (r): Feature => ({
        key: `row-${norm(r.name).replace(/\s+/g, "-")}`,
        name: r.name,
        summary: "",
        section: r.section,
        rows: [r],
        targets: [],
      }),
    );
  return { placed, unplaced };
}

// The feature on the machine that shows a part of the drawing: the one that names the part, a lamp on
// it, the part a lamp sits on, or any LED of the strip an LED belongs to. Gets undefined for a part no
// feature shows.
export function featureAt(pf: Playfield, placed: Feature[], item: Item | undefined): Feature | undefined {
  if (!item) return undefined;
  const related = new Set([item.id]);
  if (item.kind === "lamp") related.add(item.on);
  for (const l of pf.lamps) if (l.on === item.id) related.add(l.id);
  const strip = item.kind === "led" ? item.strip : item.kind === "strip" ? item.id : undefined;
  return placed.find((f) =>
    f.targets.some((t) => {
      if (related.has(t)) return true;
      const target = pf.items.get(t);
      return strip !== undefined && target?.kind === "led" && target.strip === strip;
    }),
  );
}
