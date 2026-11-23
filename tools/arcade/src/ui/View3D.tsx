// The machine in 3D, the model assets/cgi/machine.glb holds, shown with three.js. A drag orbits, the wheel
// zooms and a right drag pans. A click on a part works as a click on the drawing does: it selects the
// feature that shows the part or opens its card, and in the studio it picks the effect's target. The marked
// parts glow. The lamps light in fixed colors, and in the studio in the colors the firmware sends.
import { useSignalEffect } from "@preact/signals";
import { useEffect, useRef, useState } from "preact/hooks";
import * as THREE from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";
import { RoomEnvironment } from "three/examples/jsm/environments/RoomEnvironment.js";
import { EffectComposer } from "three/examples/jsm/postprocessing/EffectComposer.js";
import { OutputPass } from "three/examples/jsm/postprocessing/OutputPass.js";
import { RenderPass } from "three/examples/jsm/postprocessing/RenderPass.js";
import { UnrealBloomPass } from "three/examples/jsm/postprocessing/UnrealBloomPass.js";
import { featureAt } from "../model/features";
import { resolveItem } from "../model/part-card";
import type { Model, State } from "../state";
import { loadMachine, type Machine } from "../three/model";
import { RampSwitch } from "./RampSwitch";
import { targetOf } from "./Studio";

const FRAME = "cabinet";
const DRAG_PX = 4;

// the camera's places, as its position and the point it looks at, in millimeters
const VIEWS: Record<string, { pos: [number, number, number]; at: [number, number, number] }> = {
  Player: { pos: [115, 330, 760], at: [115, 10, 180] },
  Top: { pos: [115, 720, 215], at: [115, 0, 214] },
  Side: { pos: [470, 300, 520], at: [115, 0, 200] },
  Close: { pos: [115, 190, 330], at: [115, 0, 110] },
};

export function View3D({ model, state, onClose }: { model: Model; state: State; onClose: () => void }) {
  const host = useRef<HTMLDivElement>(null);
  const api = useRef<{ go: (v: string) => void; mark: (ids: Set<string>) => void } | null>(null);
  const machine = useRef<Machine | null>(null);
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

    scene.add(new THREE.HemisphereLight("#dfe6ff", "#3a2a20", 0.55));
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

    const composer = new EffectComposer(renderer);
    composer.addPass(new RenderPass(scene, camera));
    const bloom = new UnrealBloomPass(new THREE.Vector2(256, 256), 0.5, 0.3, 1.0);
    composer.addPass(bloom);
    composer.addPass(new OutputPass());

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
      camera.aspect = w / h;
      camera.updateProjectionMatrix();
    };
    const observer = new ResizeObserver(resize);
    observer.observe(el);
    resize();

    let parts: THREE.Mesh[] = [];
    const originals = new Map<THREE.Mesh, THREE.Material | THREE.Material[]>();
    const lit = new Map<THREE.Material, THREE.Material>();
    const mark = (ids: Set<string>) => {
      for (const [mesh, m] of originals) mesh.material = m;
      originals.clear();
      if (!ids.size) return;
      for (const mesh of parts) {
        if (!(mesh.userData.ids as string[]).some((id) => ids.has(id))) continue;
        const m = mesh.material as THREE.Material;
        if (!(m instanceof THREE.MeshStandardMaterial)) continue;
        let h = lit.get(m);
        if (!h) {
          const c = m.clone() as THREE.MeshStandardMaterial;
          c.emissive = new THREE.Color("#4fa3ff");
          c.emissiveIntensity = 0.55;
          c.emissiveMap = null;
          lit.set(m, (h = c));
        }
        originals.set(mesh, m);
        mesh.material = h;
      }
    };
    api.current = { go, mark };

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

    let frame = 0;
    const loop = () => {
      frame = requestAnimationFrame(loop);
      controls.update();
      composer.render();
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
      const hit = ray.intersectObjects(parts, false)[0];
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
  useSignalEffect(() => {
    const colors = state.glow.value;
    if (ready) machine.current?.paint(colors);
  });
  // the ramps the switch bar hides
  useSignalEffect(() => {
    const hidden = state.hiddenRamps.value;
    if (!ready || !machine.current) return;
    for (const r of model.hw.map.ramps) for (const o of machine.current.byId.get(r.id) ?? []) o.visible = !hidden.includes(r.id);
  });

  // the marked parts: in the studio the picked target, elsewhere the feature's targets or the part whose card shows
  const studio = state.view.value === "studio";
  const picked = state.picked.value?.id;
  const marked = studio
    ? picked ? [picked] : []
    : [...(state.feature.value?.targets ?? []), ...(state.inspected.value ? [state.inspected.value] : [])];
  const markedKey = marked.join(" ");
  useEffect(() => {
    if (ready) api.current?.mark(new Set(marked));
  }, [ready, markedKey]);

  return (
    <div class="stage stage-3d">
      <div class="view3d-host" ref={host} />
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

