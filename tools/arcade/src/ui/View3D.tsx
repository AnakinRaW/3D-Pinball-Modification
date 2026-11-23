// The machine in 3D, the model assets/cgi/machine.glb holds, shown with three.js. A drag orbits, the wheel
// zooms and a right drag pans. A click on a part works as a click on the drawing does: it selects the
// feature that shows the part or opens its card, and in the studio it picks the effect's target. The marked
// parts glow. The lamps light in fixed colors, and in the studio in the colors the firmware sends.
import "@fontsource/bungee/latin-400.css";
import { useEffect, useRef, useState } from "preact/hooks";
import * as THREE from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";
import { RoomEnvironment } from "three/examples/jsm/environments/RoomEnvironment.js";
import { EffectComposer } from "three/examples/jsm/postprocessing/EffectComposer.js";
import { OutputPass } from "three/examples/jsm/postprocessing/OutputPass.js";
import { RenderPass } from "three/examples/jsm/postprocessing/RenderPass.js";
import { ShaderPass } from "three/examples/jsm/postprocessing/ShaderPass.js";
import { UnrealBloomPass } from "three/examples/jsm/postprocessing/UnrealBloomPass.js";
import { featureAt } from "../model/features";
import { resolveItem } from "../model/part-card";
import { shortName } from "../model/playfield";
import type { Model, State } from "../state";
import { loadMachine, type Machine } from "../three/model";
import { RampSwitch } from "./RampSwitch";
import { targetOf } from "./Studio";

const FRAME = "cabinet";
const DRAG_PX = 4;
// The callout of a marked part, in screen pixels: how high its label stands over the part, how far out it
// leans and how much farther each label of a stack leans, the gap between stacked labels, the label's height,
// its width per character and its padding, and how fast a label glides to a new place.
const CALLOUT = { rise: 70, out: 34, fan: 20, gap: 10, plate: 28, char: 10.4, pad: 40, glide: 0.25 };
// the band at the view's top that the ramp pills cover, which a label keeps out of by standing below its part
const PILLS_PX = 60;
const SVG = "http://www.w3.org/2000/svg";

interface Mark {
  label: string;
  anchor: THREE.Vector3;
  g: SVGGElement;
  at: [number, number] | null; // where its label's near lower corner stands, gliding toward its place
}

// the camera's places, as its position and the point it looks at, in millimeters
const VIEWS: Record<string, { pos: [number, number, number]; at: [number, number, number] }> = {
  Player: { pos: [115, 330, 760], at: [115, 10, 180] },
  Top: { pos: [115, 720, 215], at: [115, 0, 214] },
  Side: { pos: [470, 300, 520], at: [115, 0, 200] },
  Close: { pos: [115, 190, 330], at: [115, 0, 110] },
};

export function View3D({ model, state, onClose }: { model: Model; state: State; onClose: () => void }) {
  const host = useRef<HTMLDivElement>(null);
  const api = useRef<{
    go: (v: string) => void;
    mark: (ids: Set<string>, through: Set<string>) => void;
    dim: (on: boolean) => void;
  } | null>(null);
  const machine = useRef<Machine | null>(null);
  const overlay = useRef<SVGSVGElement>(null);
  const marks = useRef<Mark[]>([]);
  const [ready, setReady] = useState(false);
  const [failed, setFailed] = useState<string | null>(model.machineGlb ? null : "assets/cgi/machine.glb is not in this checkout.");

  useEffect(() => {
    const el = host.current!;
    let disposed = false;
    const renderer = new THREE.WebGLRenderer({ antialias: true, preserveDrawingBuffer: true });
    renderer.setPixelRatio(Math.min(2, window.devicePixelRatio));
    renderer.shadowMap.enabled = true;
    renderer.shadowMap.type = THREE.PCFShadowMap;
    renderer.toneMapping = THREE.ACESFilmicToneMapping;
    renderer.toneMappingExposure = 0.9;
    el.appendChild(renderer.domElement);

    const scene = new THREE.Scene();
    // the stage's color of the theme, followed when the theme changes
    const bg = () => (scene.background = new THREE.Color(getComputedStyle(el).getPropertyValue("--stage").trim() || "#0d0f14"));
    bg();
    const themeWatch = new MutationObserver(bg);
    themeWatch.observe(document.documentElement, { attributes: true });
    const pmrem = new THREE.PMREMGenerator(renderer);
    scene.environment = pmrem.fromScene(new RoomEnvironment(), 0.04).texture;
    scene.environmentIntensity = 0.45;

    const camera = new THREE.PerspectiveCamera(35, 1, 5, 6000);
    const controls = new OrbitControls(camera, renderer.domElement);
    controls.enableDamping = true;
    controls.maxPolarAngle = Math.PI * 0.49;
    controls.minDistance = 60;
    controls.maxDistance = 2200;

    const sky = new THREE.HemisphereLight("#dfe6ff", "#3a2a20", 0.55);
    scene.add(sky);
    const sun = new THREE.DirectionalLight("#fff4e0", 1.7);
    sun.position.set(-260, 620, 520);
    sun.target.position.set(115, 0, 210);
    sun.castShadow = true;
    sun.shadow.mapSize.set(2048, 2048);
    Object.assign(sun.shadow.camera, { left: -320, right: 320, top: 360, bottom: -360, near: 100, far: 1800 });
    sun.shadow.bias = -0.0004;
    sun.shadow.normalBias = 0.6;
    scene.add(sun, sun.target);
    const fill = new THREE.DirectionalLight("#c9d8ff", 0.5);
    fill.position.set(500, 300, -200);
    scene.add(fill);

    // Only the lamps and the LEDs glow. A first pass renders them alone, every other surface black so it
    // still hides a lamp behind it, and blurs that into the glow. The second pass renders the scene and adds
    // the glow on top, so a white part in bright light never glows.
    const glowComposer = new EffectComposer(renderer);
    glowComposer.renderToScreen = false;
    glowComposer.addPass(new RenderPass(scene, camera));
    glowComposer.addPass(new UnrealBloomPass(new THREE.Vector2(256, 256), 0.9, 0.35, 0));
    const composer = new EffectComposer(renderer);
    composer.addPass(new RenderPass(scene, camera));
    const addGlow = new ShaderPass(
      new THREE.ShaderMaterial({
        uniforms: { baseTexture: { value: null }, glowTexture: { value: glowComposer.renderTarget2.texture } },
        vertexShader: "varying vec2 vUv; void main() { vUv = uv; gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }",
        fragmentShader:
          "uniform sampler2D baseTexture; uniform sampler2D glowTexture; varying vec2 vUv; void main() { gl_FragColor = texture2D(baseTexture, vUv) + vec4(texture2D(glowTexture, vUv).rgb, 0.0); }",
      }),
      "baseTexture",
    );
    composer.addPass(addGlow);
    composer.addPass(new OutputPass());
    const black = new THREE.MeshBasicMaterial({ color: "#000000" });
    const saved = new Map<THREE.Mesh, THREE.Material | THREE.Material[]>();
    const renderGlow = () => {
      const background = scene.background;
      scene.background = null;
      scene.traverse((o) => {
        if (o instanceof THREE.Mesh && !o.userData.glows) {
          saved.set(o, o.material);
          o.material = black;
        }
      });
      renderer.shadowMap.autoUpdate = false;
      glowComposer.render();
      renderer.shadowMap.autoUpdate = true;
      for (const [o, m] of saved) o.material = m;
      saved.clear();
      scene.background = background;
    };

    // the studio dims the room's light, so the lamps stand out
    const dim = (on: boolean) => {
      const k = on ? 0.3 : 1;
      sky.intensity = 0.55 * k;
      sun.intensity = 1.7 * k;
      fill.intensity = 0.5 * k;
      scene.environmentIntensity = 0.45 * k;
    };

    const go = (name: string) => {
      const v = VIEWS[name];
      camera.position.set(...v.pos);
      controls.target.set(...v.at);
      controls.update();
    };
    go("Player");

    const resize = () => {
      const { clientWidth: w, clientHeight: h } = el;
      if (!w || !h) return;
      renderer.setSize(w, h);
      composer.setSize(w, h);
      glowComposer.setSize(w, h);
      camera.aspect = w / h;
      camera.updateProjectionMatrix();
    };
    const observer = new ResizeObserver(resize);
    observer.observe(el);
    resize();

    let parts: THREE.Mesh[] = [];
    const originals = new Map<THREE.Mesh, THREE.Material | THREE.Material[]>();
    const lit = new Map<THREE.Material, THREE.Material>();
    const clear = new Map<THREE.Material, THREE.Material>();
    // a copy of a material, made once: glowing for a marked part, faint for a part the feature sees through
    const variant = (cache: Map<THREE.Material, THREE.Material>, m: THREE.MeshStandardMaterial, make: (c: THREE.MeshStandardMaterial) => void) => {
      let v = cache.get(m);
      if (!v) {
        const c = m.clone();
        make(c);
        cache.set(m, (v = c));
      }
      return v;
    };
    const mark = (ids: Set<string>, through: Set<string>) => {
      for (const [mesh, m] of originals) mesh.material = m;
      originals.clear();
      if (!ids.size && !through.size) return;
      for (const mesh of parts) {
        const own = mesh.userData.ids as string[];
        const m = mesh.material;
        if (!(m instanceof THREE.MeshStandardMaterial)) continue;
        let v: THREE.Material | null = null;
        if (own.some((id) => through.has(id)))
          v = variant(clear, m, (c) => Object.assign(c, { transparent: true, opacity: 0.3, depthWrite: false }));
        else if (own.some((id) => ids.has(id)))
          v = variant(lit, m, (c) => Object.assign(c, { emissive: new THREE.Color("#4fa3ff"), emissiveIntensity: 0.55, emissiveMap: null }));
        if (!v) continue;
        originals.set(mesh, m);
        mesh.material = v;
      }
    };
    api.current = { go, mark, dim };

    if (model.machineGlb)
      loadMachine(model.machineGlb, model.pf).then(
        (m) => {
          if (disposed) return;
          parts = m.parts;
          machine.current = m;
          scene.add(m.root);
          setReady(true);
        },
        (e) => setFailed(`The 3D model did not load: ${e}`),
      );

    // Each part gets a callout: a pulsing ring on the part, an arrow to it and a label that leans out to
    // the side of the view the part lies on, at the same size from any distance. Labels on one side stack
    // upward from the lowest part, each leaning farther out, so no arrow crosses a label. A label with no room
    // above its part, under the ramp pills, stands below it. A label glides to
    // its new place as the view turns, so it never jumps.
    const drawMarks = () => {
      const { clientWidth: w, clientHeight: h } = el;
      const shown = marks.current
        .map((m) => ({ m, v: m.anchor.clone().project(camera) }))
        .filter(({ m, v }) => {
          const on = v.z < 1 && Math.abs(v.x) < 1.2 && Math.abs(v.y) < 1.2;
          m.g.style.display = on ? "" : "none";
          if (!on) m.at = null;
          return on;
        })
        .map(({ m, v }) => ({ m, x: ((v.x + 1) / 2) * w, y: ((1 - v.y) / 2) * h }));
      for (const side of [-1, 1]) {
        const stack = shown.filter((p) => (p.x < w / 2 ? -1 : 1) === side).sort((a, b) => b.y - a.y);
        let top = Infinity;
        stack.forEach(({ m, x, y }, level) => {
          const width = m.label.length * CALLOUT.char + CALLOUT.pad;
          const above = Math.min(y - CALLOUT.rise, top - CALLOUT.gap);
          const bottom = above - CALLOUT.plate >= PILLS_PX ? above : y + CALLOUT.rise + CALLOUT.plate;
          const out = x + side * (CALLOUT.out + level * CALLOUT.fan);
          const edge = Math.min(w - 4 - (side > 0 ? width : 0), Math.max(4 + (side < 0 ? width : 0), out));
          const [lx, ly] = m.at
            ? [m.at[0] + (edge - m.at[0]) * CALLOUT.glide, m.at[1] + (bottom - m.at[1]) * CALLOUT.glide]
            : [edge, bottom];
          m.at = [lx, ly];
          if (ly < y) top = ly - CALLOUT.plate;
          // the arrow runs from the label's near edge to the ring around the part
          const [ex, ey] = [lx, ly - CALLOUT.plate / 2];
          const angle = Math.atan2(y - ey, x - ex);
          const [tx, ty] = [x - Math.cos(angle) * 12, y - Math.sin(angle) * 12];
          const [target, shaft, head, tag] = Array.from(m.g.children);
          const [plate, text] = Array.from(tag.children);
          target.setAttribute("transform", `translate(${x.toFixed(1)} ${y.toFixed(1)})`);
          shaft.setAttribute("x1", ex.toFixed(1));
          shaft.setAttribute("y1", ey.toFixed(1));
          shaft.setAttribute("x2", tx.toFixed(1));
          shaft.setAttribute("y2", ty.toFixed(1));
          head.setAttribute("transform", `translate(${tx.toFixed(1)} ${ty.toFixed(1)}) rotate(${((angle * 180) / Math.PI).toFixed(1)})`);
          const left = side > 0 ? lx : lx - width;
          plate.setAttribute("x", left.toFixed(1));
          plate.setAttribute("y", (ly - CALLOUT.plate).toFixed(1));
          plate.setAttribute("width", width.toFixed(1));
          text.setAttribute("x", (left + width / 2).toFixed(1));
          text.setAttribute("y", (ly - CALLOUT.plate / 2).toFixed(1));
        });
      }
    };

    let frame = 0;
    const loop = () => {
      frame = requestAnimationFrame(loop);
      controls.update();
      renderGlow();
      composer.render();
      drawMarks();
    };
    loop();

    // a press that moves less than a few pixels is a click
    let down: [number, number] | null = null;
    const ray = new THREE.Raycaster();
    const onDown = (e: PointerEvent) => (down = [e.clientX, e.clientY]);
    const onUp = (e: PointerEvent) => {
      if (!down || e.button !== 0 || Math.hypot(e.clientX - down[0], e.clientY - down[1]) > DRAG_PX) return;
      down = null;
      const r = renderer.domElement.getBoundingClientRect();
      ray.setFromCamera(new THREE.Vector2(((e.clientX - r.left) / r.width) * 2 - 1, -((e.clientY - r.top) / r.height) * 2 + 1), camera);
      // the ray passes through what the view hides
      const shown = (o: THREE.Object3D | null): boolean => !o || (o.visible && shown(o.parent));
      const hit = ray.intersectObjects(parts, false).find((h) => shown(h.object));
      const item = resolveItem(model.pf, hit ? (hit.object.userData.ids as string[]) : []);
      if (state.view.value === "studio") {
        // a click on anything that takes no effect, the wood or the cabinet included, clears the pick
        state.picked.value = item && targetOf(model, item) ? item : null;
        return;
      }
      const f = featureAt(model.pf, model.features.placed, item);
      state.selected.value = f?.key ?? "";
      state.inspected.value = f || !item || item.id === FRAME ? null : item.id;
    };
    renderer.domElement.addEventListener("pointerdown", onDown);
    renderer.domElement.addEventListener("pointerup", onUp);

    return () => {
      disposed = true;
      cancelAnimationFrame(frame);
      observer.disconnect();
      themeWatch.disconnect();
      controls.dispose();
      renderer.dispose();
      pmrem.dispose();
      el.removeChild(renderer.domElement);
    };
  }, [model]);

  // the lamps in the firmware's colors in the studio, in fixed colors elsewhere
  const glow = state.glow.value;
  const inStudio = state.view.value === "studio";
  useEffect(() => api.current?.dim(inStudio), [inStudio]);
  useEffect(() => {
    if (ready) machine.current?.paint(glow);
  }, [ready, glow]);

  // What the view takes apart for a selected feature, as the drawing does: the covers over its parts go,
  // the parts it shows through turn faint, and the parts it shows only in a state, such as a ball at rest,
  // appear. The studio takes nothing apart. The ramps the switch bar hides go in every view.
  const studio = state.view.value === "studio";
  const hl = state.highlight.value;
  const gone = [...state.hiddenRamps.value, ...(studio ? [] : hl.hidden)];
  const reveal = studio ? [] : hl.reveal;
  const through = studio ? [] : hl.through;
  // the marked parts: in the studio the picked target, elsewhere the feature's targets or the part whose card shows
  const picked = state.picked.value?.id;
  const marked = studio
    ? picked ? [picked] : []
    : [...(state.feature.value?.targets ?? []), ...(state.inspected.value ? [state.inspected.value] : [])];
  const key = [gone, reveal, through, marked].map((l) => l.join(" ")).join("|");
  useEffect(() => {
    const m = machine.current;
    if (!ready || !m) return;
    for (const [id, objects] of m.byId) {
      const item = model.pf.items.get(id);
      const stateOnly = item?.kind === "placement" && item.hidden;
      const visible = !gone.includes(id) && (!stateOnly || reveal.includes(id));
      objects.forEach((o) => (o.visible = visible));
    }
    api.current?.mark(new Set(marked), new Set(through));
    // an arrow with its name for each marked part
    const svg = overlay.current;
    if (!svg) return;
    svg.replaceChildren();
    marks.current = [];
    const labels = studio || !state.feature.value ? marked.map((id) => ({ id, label: shortName(model.pf, id) })) : hl.marks;
    const named = new Set<string>();
    for (const { id, label } of labels) {
      const anchor = m.anchor(id);
      if (!anchor || named.has(label)) continue;
      named.add(label);
      const g = document.createElementNS(SVG, "g");
      g.setAttribute("class", "arrow-mark");
      const el = (tag: string, attrs: Record<string, string>) => {
        const e = document.createElementNS(SVG, tag);
        for (const [k, v] of Object.entries(attrs)) e.setAttribute(k, v);
        g.appendChild(e);
        return e;
      };
      // the ring on the part and its pulse, which scales about the ring's middle
      const target = el("g", {});
      for (const cls of ["spot", "pulse"]) {
        const c = document.createElementNS(SVG, "circle");
        c.setAttribute("class", cls);
        c.setAttribute("r", "6");
        target.appendChild(c);
      }
      // the arrow: a row of chaser lights and a lit double chevron, as the playfield's inserts point
      el("line", { class: "shaft" });
      el("path", { class: "head", d: "M -16 -7 L -9 0 L -16 7 M -8 -7 L -1 0 L -8 7" });
      // the label: a sign with a neon rim, as the backglass letters its names
      const tag = el("g", { class: "tag" });
      for (const [tagName, attrs, text] of [
        ["rect", { class: "plate", height: String(CALLOUT.plate), rx: "5" }, ""],
        ["text", { class: "label", "text-anchor": "middle", "dominant-baseline": "central" }, `★ ${label} ★`],
      ] as const) {
        const e = document.createElementNS(SVG, tagName);
        for (const [k, v] of Object.entries(attrs)) e.setAttribute(k, v);
        e.textContent = text;
        tag.appendChild(e);
      }
      svg.appendChild(g);
      marks.current.push({ label, anchor, g, at: null });
    }
  }, [ready, key]);

  return (
    <div class="stage stage-3d">
      <div class="view3d-host" ref={host} />
      <svg class="view3d-marks" ref={overlay} aria-hidden="true" />
      <RampSwitch model={model} state={state} />
      <div class="view3d-bar" role="toolbar" aria-label="Camera">
        {Object.keys(VIEWS).map((v) => (
          <button key={v} class="ramp-pill" onClick={() => api.current?.go(v)}>
            {v}
          </button>
        ))}
        <button class="ramp-pill dim-switch" onClick={onClose} title="Show the drawing">
          2D
        </button>
      </div>
      {!ready && <div class="view3d-loading">{failed ?? "Loading the machine…"}</div>}
    </div>
  );
}

