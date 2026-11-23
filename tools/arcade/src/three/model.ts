// The machine in 3D as assets/cgi/machine.glb holds it, the model the Blender scripts of assets/cgi/ build
// from the playfield drawing. Every object of the file carries the drawing's id of the part it belongs to,
// so a click resolves to the part the drawing resolves it to. The lamps and the strip LEDs light on top of
// the model at the drawing's places: in a fixed color per lights group, or in the colors the studio's
// firmware sends.
import * as THREE from "three";
import { MeshoptDecoder } from "three/examples/jsm/libs/meshopt_decoder.module.js";
import { GLTFLoader } from "three/examples/jsm/loaders/GLTFLoader.js";
import type { Playfield } from "../model/playfield";
import type { Rgb } from "../sim/lights";

// the lamp colors outside the studio, by the drawing's lights group
const LAMP_COLORS: Record<string, string> = {
  "lights-stars": "#ffc23a",
  "lights-drain": "#fff1d6",
  "lights-shooter": "#ff7a2e",
  "lights-arrows": "#3fe07a",
  "lights-top-lanes": "#40d4ff",
  "lights-bumper-section": "#ff4fd0",
  "lights-inlanes": "#ff4040",
  "lights-scoop": "#ffa020",
  "lights-targets": "#ffe040",
};
const STRIP_COLOR = "#ffcf8a";
const LAMP_GAIN = 1.5; // above 1, so the bloom picks a lit lamp up
const LED_GAIN = 1.4;
// The drawing shows the backbox as a front view above the playfield's back edge, which lies at this y (mm).
const FRONT_VIEW_BELOW = -8.4;

export interface Machine {
  root: THREE.Object3D;
  parts: THREE.Mesh[]; // every mesh a click can land on
  byId: Map<string, THREE.Object3D[]>; // the model's meshes of each part, by its drawing id
  paint: (colors: Map<string, Rgb> | null) => void; // lights the lamps, in fixed colors for null
  anchor: (id: string) => THREE.Vector3 | null; // the point a mark on the part points at: the top of its middle
}

interface Light {
  meshes: THREE.Mesh[];
  materials: THREE.MeshBasicMaterial[];
  gains: number[];
  base: THREE.Color;
}

// The id of the drawing an object of the file belongs to, from its own extras or its parents'.
function drawingId(o: THREE.Object3D | null): string | undefined {
  for (; o; o = o.parent) if (typeof o.userData.drawing_id === "string") return o.userData.drawing_id;
  return undefined;
}

export async function loadMachine(url: string, pf: Playfield): Promise<Machine> {
  // the file may compress its meshes with EXT_meshopt_compression
  const gltf = await new GLTFLoader().setMeshoptDecoder(MeshoptDecoder).loadAsync(url);
  const model = gltf.scene;
  // the file's unit is the meter, the view's the millimeter
  model.scale.setScalar(1000);
  model.updateMatrixWorld(true);
  const parts: THREE.Mesh[] = [];
  const byId = new Map<string, THREE.Object3D[]>();
  model.traverse((o) => {
    if (!(o instanceof THREE.Mesh)) return;
    o.castShadow = true;
    o.receiveShadow = true;
    const id = drawingId(o);
    o.userData.ids = id ? [id] : [];
    if (!id) return;
    parts.push(o);
    byId.set(id, [...(byId.get(id) ?? []), o]);
  });

  // the playfield's frame, which the file tilts: a drawing point (x, y) at height h lies at (x, h, y) in it
  const tilt = model.getObjectByName("machine-tilt") ?? model;
  const turn = tilt.getWorldQuaternion(new THREE.Quaternion());
  const origin = tilt.getWorldPosition(new THREE.Vector3());
  const at = (x: number, y: number, h: number) => new THREE.Vector3(x, h, y).applyQuaternion(turn).add(origin);
  const up = new THREE.Vector3(0, 1, 0).applyQuaternion(turn);
  const ray = new THREE.Raycaster();
  // the surface under a drawing point: the part the lamp sits on where the ray meets it, else the lowest one
  const surface = (x: number, y: number, on?: string): THREE.Vector3 => {
    ray.set(at(x, y, 400), up.clone().negate());
    const hits = ray.intersectObjects(parts, false);
    const own = on ? hits.find((h) => (h.object.userData.ids as string[]).includes(on)) : undefined;
    return (own ?? hits[hits.length - 1])?.point.clone() ?? at(x, y, 0);
  };

  // A point of the front view lies on the front of the model's backbox, across its width and up its height.
  const backIds = ["backbox", ...pf.placements.filter((p) => p.groups[0] === "backbox").map((p) => p.id)];
  const back = new THREE.Box3();
  for (const id of backIds) byId.get(id)?.forEach((m) => back.expandByObject(m));
  const [vx, vy, vw] = pf.viewBox;
  const inFrontView = (y: number) => y < FRONT_VIEW_BELOW && !back.isEmpty();
  const onBackbox = (x: number, y: number) =>
    new THREE.Vector3(
      THREE.MathUtils.lerp(back.min.x, back.max.x, (x - vx) / vw),
      THREE.MathUtils.lerp(back.min.y, back.max.y, (FRONT_VIEW_BELOW - y) / (FRONT_VIEW_BELOW - vy)),
      back.max.z + 0.5,
    );
  // where a light or a mark of a drawing point goes, and which way it faces
  const place = (x: number, y: number, on?: string) => (inFrontView(y) ? onBackbox(x, y) : surface(x, y, on));
  const facing = (y: number) => (inFrontView(y) ? new THREE.Vector3(0, 0, 1) : up);
  const flatAt = (g: THREE.BufferGeometry, p: THREE.Vector3, normal: THREE.Vector3) => {
    g.lookAt(normal);
    g.translate(p.x, p.y, p.z);
    return g;
  };

  const lamps = new THREE.Group();
  const lights = new Map<string, Light>();
  // a lamp's light only adds to what lies under it, so a dark or unlit lamp leaves the insert as it is
  const core = glowTexture([[0, 1], [0.45, 0.85], [1, 0]]);
  const glow = glowTexture([[0, 0.9], [0.35, 0.35], [1, 0]]);
  for (const l of pf.lamps) {
    const normal = facing(l.point[1]);
    const p = place(...l.point, l.on).addScaledVector(normal, 0.15);
    const discMat = new THREE.MeshBasicMaterial({
      map: core,
      transparent: true,
      blending: THREE.AdditiveBlending,
      depthWrite: false,
      toneMapped: false,
    });
    const disc = new THREE.Mesh(flatAt(new THREE.PlaneGeometry(l.r * 1.3, l.r * 1.3), p, normal), discMat);
    disc.name = l.id;
    disc.userData.ids = [l.id, l.on];
    disc.userData.glows = true;
    parts.push(disc);
    const haloMat = new THREE.MeshBasicMaterial({
      map: glow,
      transparent: true,
      blending: THREE.AdditiveBlending,
      depthWrite: false,
      toneMapped: false,
      opacity: l.r > 6 ? 0.35 : 0.6,
    });
    const halo: THREE.Mesh = new THREE.Mesh(flatAt(new THREE.PlaneGeometry(l.r * 2.2, l.r * 2.2), p.clone().addScaledVector(normal, 0.05), normal), haloMat);
    halo.userData.glows = true;
    lamps.add(disc, halo);
    lights.set(l.id, { meshes: [disc, halo], materials: [discMat, haloMat], gains: [LAMP_GAIN, 1], base: new THREE.Color(LAMP_COLORS[l.group] ?? "#fff1d6") });
  }
  for (const s of pf.strips) {
    for (const led of s.leds) {
      if (led.hidden) continue;
      // an LED of the playfield shines up through the clear layer of the side stack, so it lights the stack's
      // top; one of the diorama lights its front
      const g = new THREE.SphereGeometry(1.1, 10, 6);
      const p = place(...led.point);
      g.translate(p.x, p.y, p.z);
      const mat = new THREE.MeshBasicMaterial({ toneMapped: false });
      const m = new THREE.Mesh(g, mat);
      m.name = led.id;
      m.userData.ids = [led.id, s.id];
      m.userData.glows = true;
      parts.push(m);
      lamps.add(m);
      lights.set(led.id, { meshes: [m], materials: [mat], gains: [LED_GAIN], base: new THREE.Color(STRIP_COLOR) });
    }
  }

  const paint = (colors: Map<string, Rgb> | null) => {
    for (const [id, light] of lights) {
      const rgb = colors?.get(id);
      const on = !colors || (rgb !== undefined && rgb[0] + rgb[1] + rgb[2] > 0);
      light.meshes.forEach((m) => (m.visible = on));
      if (!on) continue;
      const c = rgb ? new THREE.Color().setRGB(rgb[0] / 255, rgb[1] / 255, rgb[2] / 255, THREE.SRGBColorSpace) : light.base;
      light.materials.forEach((m, k) => m.color.copy(c).multiplyScalar(light.gains[k]));
    }
  };
  paint(null);

  // A part of the model is marked at the top of its middle. A lamp or an LED is marked on its light, and a
  // part the model does not hold, such as a sensor under the board, at its place in the drawing.
  const anchor = (id: string): THREE.Vector3 | null => {
    const meshes = byId.get(id) ?? lights.get(id)?.meshes.slice(0, 1);
    if (meshes?.length) {
      const box = new THREE.Box3();
      meshes.forEach((m) => box.expandByObject(m));
      const c = box.getCenter(new THREE.Vector3());
      return c.setY(box.max.y);
    }
    const item = pf.items.get(id);
    if (!item) return null;
    if (item.kind === "strip") {
      const points = item.leds.filter((l) => !l.hidden).map((l) => anchor(l.id)).filter((p): p is THREE.Vector3 => !!p);
      return points.length ? points.reduce((a, b) => a.add(b), new THREE.Vector3()).divideScalar(points.length) : null;
    }
    return place(...item.point);
  };

  const root = new THREE.Group();
  root.add(model, lamps);
  return { root, parts, byId, paint, anchor };
}

// a round white light that fades out to its rim, as stops of its radius and their brightness
function glowTexture(stops: [number, number][]): THREE.Texture {
  const c = document.createElement("canvas");
  c.width = c.height = 128;
  const g = c.getContext("2d")!;
  const r = g.createRadialGradient(64, 64, 0, 64, 64, 64);
  for (const [at, a] of stops) r.addColorStop(at, `rgba(255,255,255,${a})`);
  g.fillStyle = r;
  g.fillRect(0, 0, 128, 128);
  const t = new THREE.CanvasTexture(c);
  t.colorSpace = THREE.SRGBColorSpace;
  return t;
}
