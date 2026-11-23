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

export interface Machine {
  root: THREE.Object3D;
  parts: THREE.Mesh[]; // every mesh a click can land on
  byId: Map<string, THREE.Object3D[]>; // the model's meshes of each part, by its drawing id
  paint: (colors: Map<string, Rgb> | null) => void; // lights the lamps, in fixed colors for null
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
  const flatAt = (g: THREE.BufferGeometry, p: THREE.Vector3) => {
    g.lookAt(up);
    g.translate(p.x, p.y, p.z);
    return g;
  };

  const lamps = new THREE.Group();
  const lights = new Map<string, Light>();
  const glow = glowTexture();
  for (const l of pf.lamps) {
    const p = surface(...l.point, l.on).addScaledVector(up, 0.15);
    const discMat = new THREE.MeshBasicMaterial({ toneMapped: false, transparent: true, opacity: 0.85 });
    const disc = new THREE.Mesh(flatAt(new THREE.CircleGeometry(l.r * 0.55, 32), p), discMat);
    disc.name = l.id;
    disc.userData.ids = [l.id, l.on];
    parts.push(disc);
    const haloMat = new THREE.MeshBasicMaterial({
      map: glow,
      transparent: true,
      blending: THREE.AdditiveBlending,
      depthWrite: false,
      toneMapped: false,
      opacity: l.r > 6 ? 0.35 : 0.6,
    });
    const halo = new THREE.Mesh(flatAt(new THREE.PlaneGeometry(l.r * 2.2, l.r * 2.2), p.clone().addScaledVector(up, 0.05)), haloMat);
    lamps.add(disc, halo);
    lights.set(l.id, { meshes: [disc, halo], materials: [discMat, haloMat], gains: [LAMP_GAIN, 1], base: new THREE.Color(LAMP_COLORS[l.group] ?? "#fff1d6") });
  }
  for (const s of pf.strips) {
    for (const led of s.leds) {
      if (led.hidden) continue;
      // an LED shines up through the clear layer of the side stack, so it lights the stack's top
      const g = new THREE.SphereGeometry(1.1, 10, 6);
      const p = surface(...led.point);
      g.translate(p.x, p.y, p.z);
      const mat = new THREE.MeshBasicMaterial({ toneMapped: false });
      const m = new THREE.Mesh(g, mat);
      m.name = led.id;
      m.userData.ids = [led.id, s.id];
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

  const root = new THREE.Group();
  root.add(model, lamps);
  return { root, parts, byId, paint };
}

function glowTexture(): THREE.Texture {
  const c = document.createElement("canvas");
  c.width = c.height = 128;
  const g = c.getContext("2d")!;
  const r = g.createRadialGradient(64, 64, 0, 64, 64, 64);
  r.addColorStop(0, "rgba(255,255,255,0.9)");
  r.addColorStop(0.35, "rgba(255,255,255,0.35)");
  r.addColorStop(1, "rgba(255,255,255,0)");
  g.fillStyle = r;
  g.fillRect(0, 0, 128, 128);
  const t = new THREE.CanvasTexture(c);
  t.colorSpace = THREE.SRGBColorSpace;
  return t;
}
