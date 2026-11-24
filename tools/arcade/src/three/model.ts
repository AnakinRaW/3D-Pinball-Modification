// The machine in 3D as assets/cgi/machine.glb holds it, the model the Blender scripts of assets/cgi/ build
// from the playfield drawing. Every object of the file carries the drawing's id of the part it belongs to,
// so a click resolves to the part the drawing resolves it to. A lamp lights the insert it sits on, the
// strip LEDs light at the drawing's places: in the model's colors, or in the colors the studio's firmware
// sends.
import * as THREE from "three";
import { MeshoptDecoder } from "three/examples/jsm/libs/meshopt_decoder.module.js";
import { GLTFLoader } from "three/examples/jsm/loaders/GLTFLoader.js";
import type { Playfield } from "../model/playfield";
import type { Rgb } from "../sim/lights";

// the color of a lamp whose insert the model does not hold, and of the strip LEDs, outside the studio
const LAMP_COLOR = "#fff1d6";
const STRIP_COLOR = "#ffcf8a";
const LAMP_GAIN = 1.5; // the brightness of a lamp's light
const INSERT_GAIN = 1.6; // the brightness a lit insert glows with
// A material at least this metallic is drawn with a highlight; its matte part keeps this share of its
// color, and a smooth one shines this sharply.
const METAL = 0.5;
const METAL_MATTE = 0.35;
const METAL_SHINE = 120;
const LED_GAIN = 2;
// how bright the lamps and the LEDs shine outside the studio, where they only show the machine at rest
const AT_REST = 0.45;
// Two faces closer than this (mm) along their normal lie in one plane for the depth buffer, and their
// normals agree when they differ by less than this (radians, about one degree).
const PLANE_GAP = 0.05;
const PLANE_TURN = 0.02;
const CELL = 0.5; // mm across a cell of the grid on which the faces of one plane are compared
// how far (mm) a part lying on another moves off it, for each layer it lies above the bottom one
const LAYER_MM = 0.02;
const SHEET_MM = 0.005; // a part thinner than this (mm) is a sheet with no inside, such as a picture
const LED_DEPTH = 1.5; // mm an LED lies under the top of the clear layer it shines through
const LED_HALO = 18; // mm across the light an LED spreads in the clear layer
// The diorama's ceiling LEDs light the scene through a few spotlights, one for each run of LEDs: how many
// runs, how wide a cone in radians, how soft its edge, and how bright a fully lit run shines.
const CEILING = { runs: 4, angle: 0.9, soft: 0.8, brightness: 9 };
// The drawing shows the backbox as a front view above the playfield's back edge, which lies at this y (mm).
const FRONT_VIEW_BELOW = -8.4;

// The view builds its scene once, so a live update of this code would leave the running scene on the old
// code. The page loads again instead.
if (import.meta.hot) import.meta.hot.accept(() => location.reload());

export interface Machine {
  root: THREE.Object3D;
  parts: THREE.Mesh[]; // every mesh a click can land on
  byId: Map<string, THREE.Object3D[]>; // the model's meshes of each part, by its drawing id
  paint: (colors: Map<string, Rgb> | null) => void; // lights the lamps, in fixed colors for null
  anchor: (id: string) => THREE.Vector3 | null; // the point a mark on the part points at: the top of its middle
}

interface Light {
  meshes: THREE.Mesh[]; // the lights the view adds, shown only while the lamp is on
  materials: THREE.MeshBasicMaterial[];
  gains: number[];
  inserts: (THREE.MeshLambertMaterial | THREE.MeshPhongMaterial)[]; // the model's insert, lit
  base: THREE.Color;
  now?: THREE.Color; // the color it shows, black while it is off
  instance?: number; // the LED's instance among the drawn LEDs
  at?: THREE.Vector3; // where the LED lies
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
  const meshes: THREE.Mesh[] = [];
  const byId = new Map<string, THREE.Object3D[]>();
  // The view lights the model the simple way, matte with no reflections, which draws fast and crisp. Each
  // material keeps its colors, its pictures and how it glows and shows through.
  // A metal keeps a highlight, so a foil, gold paint or a chrome ball still reads as metal: its color goes
  // into the shine, and the matte part of it darkens.
  const simple = new Map<THREE.Material, THREE.MeshLambertMaterial | THREE.MeshPhongMaterial>();
  const matte = (m: THREE.Material): THREE.MeshLambertMaterial | THREE.MeshPhongMaterial => {
    let l = simple.get(m);
    if (!l) {
      const s = m as THREE.MeshStandardMaterial;
      const shared = {
        name: s.name,
        map: s.map,
        emissive: s.emissive,
        emissiveMap: s.emissiveMap,
        emissiveIntensity: s.emissiveIntensity,
        transparent: s.transparent,
        opacity: s.opacity,
        alphaTest: s.alphaTest,
        side: s.side,
        vertexColors: s.vertexColors,
      };
      if ((s.metalness ?? 0) >= METAL) {
        l = new THREE.MeshPhongMaterial({
          ...shared,
          color: s.color.clone().multiplyScalar(METAL_MATTE),
          specular: s.color,
          shininess: METAL_SHINE * (1 - (s.roughness ?? 0.5)),
        });
        simple.set(m, l);
        return l;
      }
      l = new THREE.MeshLambertMaterial({
        name: s.name,
        color: s.color,
        map: s.map,
        emissive: s.emissive,
        emissiveMap: s.emissiveMap,
        emissiveIntensity: s.emissiveIntensity,
        transparent: s.transparent,
        opacity: s.opacity,
        alphaTest: s.alphaTest,
        side: s.side,
        vertexColors: s.vertexColors,
      });
      simple.set(m, l);
    }
    return l;
  };
  model.traverse((o) => {
    if (!(o instanceof THREE.Mesh)) return;
    o.material = matte(o.material as THREE.Material);
    meshes.push(o);
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

  layer(meshes);
  // the lamps' and the LEDs' lights lie on the board and on the inserts, a few of the depth buffer's steps nearer
  const OVER = { polygonOffset: true, polygonOffsetFactor: 0, polygonOffsetUnits: -8 };
  const ray = new THREE.Raycaster();
  // the surface under a drawing point: the part the lamp sits on where the ray meets it, else the lowest one
  const surface = (x: number, y: number, on?: string): THREE.Vector3 => {
    ray.set(at(x, y, 400), up.clone().negate());
    const hits = ray.intersectObjects(parts, false);
    const own = on ? hits.find((h) => (h.object.userData.ids as string[]).includes(on)) : undefined;
    return (own ?? hits[hits.length - 1])?.point.clone() ?? at(x, y, 0);
  };

  // A point of the front view lies on the backbox. The backdrop's picture, the drawing's part "sky", spans the
  // front view's width and height, so it gives a point's place across and up. The point stands at the front
  // of the diorama's frame, "dio-frame", in front of everything the backbox shows.
  const box = (id: string) => (byId.get(id) ?? []).reduce((b, m) => b.expandByObject(m), new THREE.Box3());
  const back = box("sky");
  const backdrop = back.min.z;
  if (!back.isEmpty()) back.max.z = Math.max(back.max.z, box("dio-frame").max.z);
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
  const ceiling: string[] = [];
  for (const l of pf.lamps) {
    const normal = facing(l.point[1]);
    const p = place(...l.point, l.on).addScaledVector(normal, 0.15);
    // The model's insert under the lamp glows in the lamp's color, in its own shape. A click on it reaches the
    // lamp, as on the drawing. Outside the studio the lamp takes the color the model gives its insert.
    const inserts: Light["inserts"] = [];
    let base = new THREE.Color(LAMP_COLOR);
    for (const o of byId.get(l.on) ?? []) {
      if (!(o instanceof THREE.Mesh) || !(o.material instanceof THREE.MeshLambertMaterial || o.material instanceof THREE.MeshPhongMaterial)) continue;
      if (o.material.emissiveIntensity > 0 && inserts.length === 0) base = o.material.emissive.clone();
      const lit = o.material.clone();
      o.material = lit;
      o.userData.ids = [l.id, l.on];
      inserts.push(lit);
    }
    const meshes: THREE.Mesh[] = [];
    const materials: THREE.MeshBasicMaterial[] = [];
    const gains: number[] = [];
    if (!inserts.length) {
      // a lamp with no insert in the model lights a soft round spot, which only adds to what lies under it
      const discMat = new THREE.MeshBasicMaterial({ map: core, transparent: true, blending: THREE.AdditiveBlending, depthWrite: false, toneMapped: false, ...OVER });
      const disc = new THREE.Mesh(flatAt(new THREE.PlaneGeometry(l.r * 1.3, l.r * 1.3), p, normal), discMat);
      disc.name = l.id;
      disc.userData.ids = [l.id, l.on];
      parts.push(disc);
      lamps.add(disc);
      meshes.push(disc);
      materials.push(discMat);
      gains.push(LAMP_GAIN);
    }
    // the light a lamp throws on the board around its insert
    const haloMat = new THREE.MeshBasicMaterial({
      map: glow,
      transparent: true,
      blending: THREE.AdditiveBlending,
      depthWrite: false,
      toneMapped: false,
      opacity: l.r > 6 ? 0.45 : 0.7,
      ...OVER,
    });
    const halo: THREE.Mesh = new THREE.Mesh(flatAt(new THREE.PlaneGeometry(l.r * 3, l.r * 3), p.clone().addScaledVector(normal, 0.05), normal), haloMat);
    lamps.add(halo);
    meshes.push(halo);
    materials.push(haloMat);
    gains.push(1);
    lights.set(l.id, { meshes, materials, gains, inserts, base });
  }
  const strip: { id: string; strip: string; at: THREE.Vector3; top: THREE.Vector3 }[] = [];
  for (const s of pf.strips) {
    for (const led of s.leds) {
      if (led.hidden) continue;
      // An LED of the diorama hides under the frame's top board, as on the machine, so what shows is the
      // light its run throws on the scene
      if (inFrontView(led.point[1])) {
        ceiling.push(led.id);
        lights.set(led.id, { meshes: [], materials: [], gains: [], inserts: [], base: new THREE.Color(STRIP_COLOR) });
        continue;
      }
      // An LED of the playfield lies in the clear layer of the side stack and shines out through it. The
      // clear layer is the topmost see-through surface above the LED's place; without one, the LED sits on
      // the lowest surface. The clear layer's own glow, which the films give it, goes, since the LEDs light it.
      ray.set(at(...led.point, 400), up.clone().negate());
      const hits = ray.intersectObjects(parts, false);
      const clear = hits.find((h) => ((h.object as THREE.Mesh).material as THREE.Material).transparent);
      if (clear) {
        const m = (clear.object as THREE.Mesh).material as THREE.MeshLambertMaterial;
        if (m.emissive) m.emissiveIntensity = 0;
      }
      const p = clear ? clear.point.clone().addScaledVector(up, -LED_DEPTH) : place(...led.point);
      // the light the LED spreads in the clear layer, seen on its top, as a lamp spreads its light on the board
      const top = clear ? clear.point.clone().addScaledVector(up, 0.2) : p.clone().addScaledVector(up, 0.2);
      strip.push({ id: led.id, strip: s.id, at: p, top });
    }
  }

  // All the playfield's LEDs are drawn in one go each, their dots and their halos, since a draw for every
  // LED costs more time than drawing them all. Each LED is one instance with its color, and a dark LED
  // vanishes, since its light only adds to what lies around it.
  const additive = { transparent: true, blending: THREE.AdditiveBlending, depthWrite: false, toneMapped: false };
  const dots = new THREE.InstancedMesh(new THREE.SphereGeometry(1.6, 12, 8), new THREE.MeshBasicMaterial(additive), strip.length);
  const halos = new THREE.InstancedMesh(
    new THREE.PlaneGeometry(LED_HALO, LED_HALO).lookAt(up),
    new THREE.MeshBasicMaterial({ ...additive, map: glow, ...OVER }),
    strip.length,
  );
  const place1 = new THREE.Matrix4();
  strip.forEach((l, i) => {
    dots.setMatrixAt(i, place1.makeTranslation(l.at));
    halos.setMatrixAt(i, place1.makeTranslation(l.top));
    lights.set(l.id, { meshes: [], materials: [], gains: [], inserts: [], base: new THREE.Color(STRIP_COLOR), instance: i, at: l.at });
  });
  for (const m of [dots, halos]) {
    m.frustumCulled = false;
    m.setColorAt(0, new THREE.Color());
    lamps.add(m);
  }
  dots.userData.ids = [];
  dots.userData.instanceIds = strip.map((l) => [l.id, l.strip]);
  parts.push(dots);

  // the ceiling's runs of LEDs, each a spotlight under the front of the frame's top board, aimed down at the
  // middle of the scene
  const runs = Array.from({ length: ceiling.length ? CEILING.runs : 0 }, (_, k) => {
    const ids = ceiling.filter((_, i) => Math.floor((i * CEILING.runs) / ceiling.length) === k);
    const pts = ids.map((id) => onBackbox(...(pf.items.get(id) as { point: [number, number] }).point));
    const at = pts.reduce((a, b) => a.add(b), new THREE.Vector3()).divideScalar(pts.length);
    at.z = back.max.z;
    const spot = new THREE.SpotLight("#ffffff", 0, 0, CEILING.angle, CEILING.soft, 0);
    spot.position.copy(at);
    spot.target.position.set(at.x, (back.min.y + at.y) / 2, backdrop);
    lamps.add(spot, spot.target);
    return { ids, spot };
  });

  const shade = new THREE.Color();
  const paint = (colors: Map<string, Rgb> | null) => {
    for (const [id, light] of lights) {
      const rgb = colors?.get(id);
      const on = !colors || (rgb !== undefined && rgb[0] + rgb[1] + rgb[2] > 0);
      light.meshes.forEach((m) => (m.visible = on));
      const c = !on
        ? new THREE.Color(0, 0, 0)
        : rgb
          ? new THREE.Color().setRGB(rgb[0] / 255, rgb[1] / 255, rgb[2] / 255, THREE.SRGBColorSpace)
          : light.base.clone().multiplyScalar(AT_REST);
      for (const lit of light.inserts) {
        lit.emissive.copy(c);
        lit.emissiveIntensity = INSERT_GAIN;
      }
      if (on) light.materials.forEach((m, k) => m.color.copy(c).multiplyScalar(light.gains[k]));
      if (light.instance !== undefined) {
        dots.setColorAt(light.instance, shade.copy(c).multiplyScalar(LED_GAIN));
        halos.setColorAt(light.instance, c);
      }
      light.now = c;
    }
    for (const m of [dots, halos]) if (m.instanceColor) m.instanceColor.needsUpdate = true;
    // a run's spotlight shines in the mean color of its LEDs
    for (const { ids, spot } of runs) {
      const mean = ids.reduce((sum, id) => sum.add(lights.get(id)!.now ?? new THREE.Color()), new THREE.Color()).multiplyScalar(1 / ids.length);
      spot.color.copy(mean);
      spot.intensity = CEILING.brightness;
    }
  };
  paint(null);

  // A part of the model is marked at the top of its middle. A lamp or an LED is marked on its light, and a
  // part the model does not hold, such as a sensor under the board, at its place in the drawing.
  const anchor = (id: string): THREE.Vector3 | null => {
    const led = lights.get(id)?.at;
    if (led) return led.clone();
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
    // a part of the drawing's upper layer, such as a sensor on a ramp, sits on the topmost surface shown
    // there, a part of the playfield on the lowest
    if (item.kind === "placement" && item.groups[0] === "layer-1" && !inFrontView(item.point[1])) {
      ray.set(at(...item.point, 400), up.clone().negate());
      const shown = (o: THREE.Object3D | null): boolean => !o || (o.visible && shown(o.parent));
      const hit = ray.intersectObjects(parts, false).find((h) => shown(h.object));
      if (hit) return hit.point.clone();
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

// Where two parts of the file lie in one plane, the depth buffer cannot tell which is in front, and the two
// flicker through each other as the view turns: a foil on the board, a paint on a wall, a picture on the
// diorama's back. This finds every such place and moves the part with less area in that plane a little off
// the part it lies on, toward the side its face shows. A part's layer is one above the highest layer it
// lies on, so a sticker on a foil on the board shows over both. The move is a fixed distance, which the depth
// buffer resolves from every view; a shift in the buffer's own terms grows with the size of a pixel and at a
// slant reaches millimeters, which pulls a part through the one in front of it.
function layer(meshes: THREE.Mesh[]) {
  // every mesh's corners where the scene places them, and its faces as triples of corners
  const corners: Float32Array[] = [];
  const faces: Uint32Array[] = [];
  const p = new THREE.Vector3();
  for (const o of meshes) {
    o.updateWorldMatrix(true, false);
    const pos = o.geometry.attributes.position;
    const world = new Float32Array(pos.count * 3);
    for (let i = 0; i < pos.count; i++) p.fromBufferAttribute(pos, i).applyMatrix4(o.matrixWorld).toArray(world, i * 3);
    corners.push(world);
    const index = o.geometry.index;
    faces.push(index ? Uint32Array.from(index.array) : Uint32Array.from({ length: pos.count }, (_, i) => i));
  }
  // A part is a sheet when it is this thin: twice its volume over its surface area, which is the thickness
  // of a plate. A solid part's face shows away from its inside, a sheet's either way.
  const sheet = meshes.map((_, mi) => {
    const [world, list] = [corners[mi], faces[mi]];
    const [a, b, c] = [new THREE.Vector3(), new THREE.Vector3(), new THREE.Vector3()];
    let [volume, surface] = [0, 0];
    for (let f = 0; f + 2 < list.length; f += 3) {
      a.fromArray(world, list[f] * 3);
      b.fromArray(world, list[f + 1] * 3);
      c.fromArray(world, list[f + 2] * 3);
      volume += a.dot(b.clone().cross(c)) / 6;
      surface += b.clone().sub(a).cross(c.clone().sub(a)).length() / 2;
    }
    return surface > 0 && (2 * Math.abs(volume)) / surface < SHEET_MM;
  });

  // The faces of every plane, by the plane's normal and its distance from the origin. Each face goes into
  // two grids of planes, shifted by half a step, so two faces closer than half a step share one of them.
  // A plane's key packs the normal's three steps and the distance's step into one number.
  type Faces = { area: number; faces: number[]; facing: THREE.Vector3 }; // facing: the way its faces show, by area
  const planes = new Map<number, Map<number, Faces>>();
  const turns = Math.round(1 / PLANE_TURN);
  const span = 2 * turns + 1;
  const [a, b, c, e, n] = [new THREE.Vector3(), new THREE.Vector3(), new THREE.Vector3(), new THREE.Vector3(), new THREE.Vector3()];
  meshes.forEach((_, mi) => {
    const [world, list] = [corners[mi], faces[mi]];
    for (let f = 0; f + 2 < list.length; f += 3) {
      a.fromArray(world, list[f] * 3);
      b.fromArray(world, list[f + 1] * 3);
      c.fromArray(world, list[f + 2] * 3);
      n.subVectors(b, a).cross(e.subVectors(c, a));
      const area = n.length() / 2;
      if (!(area > 0)) continue;
      n.divideScalar(2 * area);
      const shows = n.clone().multiplyScalar(area);
      // Every material draws both sides of a face, so faces facing apart in one plane fight as well: a
      // normal and its opposite count as one, the one whose largest part points along its axis.
      const lead = Math.abs(n.x) >= Math.abs(n.y) && Math.abs(n.x) >= Math.abs(n.z) ? n.x : Math.abs(n.y) >= Math.abs(n.z) ? n.y : n.z;
      if (lead < 0) n.negate();
      const facing = ((Math.round(n.x * turns) + turns) * span + Math.round(n.y * turns) + turns) * span + Math.round(n.z * turns) + turns;
      const d = n.dot(a) / PLANE_GAP;
      for (const shift of [0, 0.5]) {
        const key = (facing * 2 + shift * 2) * 2 ** 24 + Math.round(d + shift) + 2 ** 23;
        let plane = planes.get(key);
        if (!plane) planes.set(key, (plane = new Map()));
        let faced = plane.get(mi);
        if (!faced) plane.set(mi, (faced = { area: 0, faces: [], facing: new THREE.Vector3() }));
        faced.area += area;
        faced.facing.add(shows);
        faced.faces.push(f);
      }
    }
  });

  // In a plane that several parts share, each part's faces are laid on a grid of cells, and parts that
  // cover the same cell lie on one another. A part's faces are laid out only inside the extent of another
  // part of the plane, since only there can a cell be shared.
  const under = new Map<number, Set<number>>(); // the parts each part lies on
  const away = new Map<number, THREE.Vector3>(); // the way each part lying on another moves off it
  const [u, v] = [new THREE.Vector3(), new THREE.Vector3()];
  // which side of the line from (ax, ay) to (bx, by) the point (x, y) lies on
  const side = (ax: number, ay: number, bx: number, by: number, x: number, y: number) => (bx - ax) * (y - ay) - (by - ay) * (x - ax);
  const reach = new THREE.Box2();
  const q = new THREE.Vector2();
  for (const plane of planes.values()) {
    if (plane.size < 2) continue;
    // the plane's normal, from the faces' own, and two directions across it
    const [mi0, first] = plane.entries().next().value!;
    const w = corners[mi0];
    const l0 = faces[mi0];
    const f0 = first.faces[0];
    a.fromArray(w, l0[f0] * 3);
    n.subVectors(b.fromArray(w, l0[f0 + 1] * 3), a).cross(e.subVectors(c.fromArray(w, l0[f0 + 2] * 3), a)).normalize();
    u.set(Math.abs(n.x) > 0.9 ? 0 : 1, Math.abs(n.x) > 0.9 ? 1 : 0, 0);
    u.addScaledVector(n, -u.dot(n)).normalize();
    v.crossVectors(n, u);
    // each face as a triangle on the grid, and each part's extent there
    const flat = new Map<number, { tris: Float64Array; box: THREE.Box2 }>();
    for (const [mi, faced] of plane) {
      const [world, list] = [corners[mi], faces[mi]];
      const tris = new Float64Array(faced.faces.length * 6);
      const box = new THREE.Box2();
      faced.faces.forEach((f, k) => {
        for (let j = 0; j < 3; j++) {
          a.fromArray(world, list[f + j] * 3);
          q.set(a.dot(u) / CELL, a.dot(v) / CELL);
          tris[k * 6 + j * 2] = q.x;
          tris[k * 6 + j * 2 + 1] = q.y;
          box.expandByPoint(q);
        }
      });
      flat.set(mi, { tris, box });
    }
    const ids = [...flat.keys()];
    for (let i = 0; i < ids.length; i++) {
      for (let j = i + 1; j < ids.length; j++) {
        const [pi, pj] = [flat.get(ids[i])!, flat.get(ids[j])!];
        const shared = pi.box.clone().intersect(pj.box);
        if (shared.isEmpty()) continue;
        // the cells part i covers inside the shared extent, then whether part j covers one of them
        const [x0, y0] = [Math.floor(shared.min.x), Math.floor(shared.min.y)];
        const [cols, rows] = [Math.ceil(shared.max.x) - x0, Math.ceil(shared.max.y) - y0];
        if (cols <= 0 || rows <= 0) continue;
        const grid = new Uint8Array(cols * rows);
        const cover = (tris: Float64Array, hit: (cell: number) => boolean): boolean => {
          for (let t = 0; t < tris.length; t += 6) {
            const [ax, ay, bx, by, cx, cy] = [tris[t], tris[t + 1], tris[t + 2], tris[t + 3], tris[t + 4], tris[t + 5]];
            reach.min.set(Math.min(ax, bx, cx), Math.min(ay, by, cy));
            reach.max.set(Math.max(ax, bx, cx), Math.max(ay, by, cy));
            if (!reach.intersectsBox(shared)) continue;
            // A cell is covered when its center lies strictly inside the triangle, so faces that only meet at
            // an edge cover no cell together.
            const [xa, xb] = [Math.max(Math.floor(reach.min.x), x0), Math.min(Math.ceil(reach.max.x), x0 + cols)];
            const [ya, yb] = [Math.max(Math.floor(reach.min.y), y0), Math.min(Math.ceil(reach.max.y), y0 + rows)];
            for (let x = xa; x < xb; x++) {
              for (let y = ya; y < yb; y++) {
                const s0 = side(ax, ay, bx, by, x + 0.5, y + 0.5);
                const s1 = side(bx, by, cx, cy, x + 0.5, y + 0.5);
                const s2 = side(cx, cy, ax, ay, x + 0.5, y + 0.5);
                if (((s0 > 0 && s1 > 0 && s2 > 0) || (s0 < 0 && s1 < 0 && s2 < 0)) && hit((x - x0) * rows + y - y0)) return true;
              }
            }
          }
          return false;
        };
        // Two solid parts whose faces show apart only touch there, and neither face can be seen.
        const [fi, fj] = [plane.get(ids[i])!, plane.get(ids[j])!];
        if (fi.facing.dot(fj.facing) < 0 && !sheet[ids[i]] && !sheet[ids[j]]) continue;
        cover(pi.tris, (cell) => ((grid[cell] = 1), false));
        if (!cover(pj.tris, (cell) => grid[cell] === 1)) continue;
        // The part with less area in the plane lies on the other one. It moves off a solid part the way that
        // part's face shows. Off a sheet, a solid part moves back from the face it touches the sheet with.
        const [over, base] = fi.area < fj.area ? [ids[i], ids[j]] : [ids[j], ids[i]];
        if (!under.has(over)) under.set(over, new Set());
        under.get(over)!.add(base);
        const off = !sheet[base] ? plane.get(base)!.facing.clone() : !sheet[over] ? plane.get(over)!.facing.clone().negate() : plane.get(base)!.facing.clone();
        if (!away.has(over)) away.set(over, new THREE.Vector3());
        away.get(over)!.add(off.normalize());
      }
    }
  }

  // Each part's layer lies one above the highest part it lies on. A ring of parts lying on one another is cut.
  const layers = new Map<number, number>();
  const busy = new Set<number>();
  const level = (mi: number): number => {
    const known = layers.get(mi);
    if (known !== undefined) return known;
    if (busy.has(mi)) return 0;
    busy.add(mi);
    let l = 0;
    for (const base of under.get(mi) ?? []) l = Math.max(l, level(base) + 1);
    busy.delete(mi);
    layers.set(mi, l);
    return l;
  };
  for (const mi of under.keys()) {
    const o = meshes[mi];
    const to = away.get(mi)!;
    if (to.lengthSq() === 0 || !o.parent) continue;
    const at = o.getWorldPosition(new THREE.Vector3()).addScaledVector(to.normalize(), LAYER_MM * level(mi));
    o.position.copy(o.parent.worldToLocal(at));
    o.updateMatrixWorld(true);
  }
}
