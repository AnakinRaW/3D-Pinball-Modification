// Reads the feature texts: the tables of docs/features.md and the numbered list of the root README.

export type Inline =
  | { kind: "text"; text: string }
  | { kind: "strong"; text: string }
  | { kind: "link"; text: string; path: string };

// one row of a table in docs/features.md
export interface FeatureRow {
  section: string;
  name: string;
  stock: Inline[];
  modification: Inline[];
}

// one item of the README's feature list
export interface ReadmeFeature {
  n: number;
  name: string;
  summary: string;
}

// Joins a link of a document with the document's folder, so the result is a path from the repository's root.
export function resolvePath(base: string, href: string): string {
  if (/^[a-z]+:/i.test(href)) return href;
  const [file, anchor] = href.split("#", 2);
  const parts = base.split("/").filter(Boolean);
  for (const p of file.split("/")) {
    if (p === "..") parts.pop();
    else if (p !== "." && p !== "") parts.push(p);
  }
  const path = file === "" ? base : parts.join("/");
  return anchor === undefined ? path : `${path}#${anchor}`;
}

// Splits a line of markdown into plain text, bold text and links, with link paths from the repository's root.
export function parseInline(text: string, base: string): Inline[] {
  const out: Inline[] = [];
  const re = /\*\*(.+?)\*\*|\[([^\]]+)\]\(([^)]+)\)|`([^`]+)`/g;
  let last = 0;
  for (let m = re.exec(text); m; m = re.exec(text)) {
    if (m.index > last) out.push({ kind: "text", text: text.slice(last, m.index) });
    if (m[1] !== undefined) out.push({ kind: "strong", text: m[1] });
    else if (m[2] !== undefined) out.push({ kind: "link", text: m[2], path: resolvePath(base, m[3]) });
    else out.push({ kind: "text", text: m[4] });
    last = re.lastIndex;
  }
  if (last < text.length) out.push({ kind: "text", text: text.slice(last) });
  return out;
}

// Leaves the links out, with the commas and the space that separated them from the text.
export function withoutLinks(inline: Inline[]): Inline[] {
  const out = inline.filter((t) => t.kind !== "link");
  while (out.length && out[out.length - 1].kind === "text" && /^[\s,]*$/.test(out[out.length - 1].text)) out.pop();
  const last = out[out.length - 1];
  if (last?.kind === "text") out[out.length - 1] = { ...last, text: last.text.trimEnd() };
  return out;
}

export function plain(inline: Inline[]): string {
  return inline.map((t) => t.text).join("");
}

function cells(line: string): string[] {
  return line.trim().replace(/^\|/, "").replace(/\|$/, "").split("|").map((c) => c.trim());
}

// Reads every table row of docs/features.md under its section heading.
export function parseFeatureTables(md: string): FeatureRow[] {
  const rows: FeatureRow[] = [];
  let section = "";
  let header = false;
  for (const line of md.split(/\r?\n/)) {
    const h = /^##\s+(.+)$/.exec(line);
    if (h) {
      section = h[1].trim();
      header = true;
      continue;
    }
    if (!line.trim().startsWith("|")) {
      header = true;
      continue;
    }
    const c = cells(line);
    if (header) {
      header = false; // the table's head row
      continue;
    }
    if (c.every((x) => /^:?-+:?$/.test(x))) continue;
    if (c.length < 3) continue;
    rows.push({
      section,
      name: plain(parseInline(c[0], "docs")),
      stock: parseInline(c[1], "docs"),
      modification: parseInline(c[2], "docs"),
    });
  }
  return rows;
}

// Reads the numbered list under the README's "Features" heading.
export function parseReadmeFeatures(md: string): ReadmeFeature[] {
  const out: ReadmeFeature[] = [];
  let inside = false;
  for (const line of md.split(/\r?\n/)) {
    if (/^##\s/.test(line)) inside = /^##\s+Features\s*$/.test(line);
    if (!inside) continue;
    const m = /^(\d+)\.\s+\*\*(.+?):\*\*\s*(.+)$/.exec(line);
    if (m) out.push({ n: Number(m[1]), name: m[2].trim(), summary: upperFirst(m[3].trim()) });
  }
  return out;
}

function upperFirst(s: string): string {
  return s.charAt(0).toUpperCase() + s.slice(1);
}
