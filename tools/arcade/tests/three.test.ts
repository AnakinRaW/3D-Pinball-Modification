// The machine in 3D that assets/cgi/machine.glb holds, read as the file's JSON chunk: the ids its parts
// carry are the drawing's, so a click and a lamp find their part. Skipped in a checkout without the file.
import { existsSync, readFileSync } from "node:fs";
import { resolve } from "node:path";
import { describe, expect, it } from "vitest";
import { sources } from "../src/sources";
import { loadModel } from "../src/state";

const file = resolve("../../assets/cgi/machine.glb");
const present = existsSync(file);
const { hw } = loadModel(sources);

interface Node {
  name?: string;
  extras?: { drawing_id?: unknown };
}

// the JSON chunk of a binary glTF file: a 12-byte header, then the chunk's length and type, then the chunk
function gltfJson(path: string): { nodes?: Node[]; extensionsUsed?: string[] } {
  const b = readFileSync(path);
  expect(b.toString("ascii", 0, 4)).toBe("glTF");
  expect(b.toString("ascii", 16, 20)).toBe("JSON");
  return JSON.parse(b.toString("utf8", 20, 20 + b.readUInt32LE(12)));
}

describe.skipIf(!present)("machine.glb", () => {
  const json = present ? gltfJson(file) : {};
  const nodes = json.nodes ?? [];
  const ids = new Set(nodes.map((n) => n.extras?.drawing_id).filter((id): id is string => typeof id === "string"));

  it("tags its parts with ids the drawing names", () => {
    const unknown = [...ids].filter((id) => !sources.playfieldSvg.includes(`id="${id}"`));
    expect(unknown).toEqual([]);
    expect(ids.size).toBeGreaterThan(0);
  });

  it("holds every ramp the switch bar hides", () => {
    for (const r of hw.map.ramps) expect(ids.has(r.id), r.id).toBe(true);
  });

  it("tilts the playfield in a node the view finds the playfield's frame by", () => {
    expect(nodes.some((n) => n.name === "machine-tilt")).toBe(true);
  });

  it("uses no extension the view cannot decode", () => {
    const known = ["EXT_meshopt_compression", "KHR_materials_emissive_strength", "KHR_materials_clearcoat",
      "KHR_materials_transmission", "KHR_materials_ior", "KHR_materials_specular", "KHR_texture_transform",
      "KHR_mesh_quantization"];
    expect((json.extensionsUsed ?? []).filter((e) => !known.includes(e))).toEqual([]);
  });
});
