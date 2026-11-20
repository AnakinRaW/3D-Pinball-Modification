// The studio for lights: the firmware's lamps component and lighting driver run in the browser on a
// simulated clock. The left panel starts effects on the lamps and strips, the drawing shows the
// colors the chains carry, and the right panel shows the clock, the profile and the running layers.
import { signal } from "@preact/signals";
import { useEffect, useState } from "preact/hooks";
import { shortName, type Item } from "../model/playfield";
import {
  drawingColors,
  groupSetups,
  loadLights,
  PALETTES,
  readChains,
  type EffectSpec,
  type Kind,
  type LightsModule,
  type Paint,
  type Target,
} from "../sim/lights";
import type { Model, State } from "../state";

const MAX_STEP_MS = 100; // a pass of the loop never advances the clock further, so a hidden tab does not jump
const LAST_KEY = 255; // a layer's key is one byte
const SPEEDS = [0.25, 1, 4];
const ADAPTERS = [3, 6]; // the two adapters the lighting design derives

type Status = "loading" | "missing" | "failed" | "ready";

// the module and what runs on it, kept across switches between the views
const lights = signal<LightsModule | null>(null);
const status = signal<Status>("loading");
const running = signal(true);
const speed = signal(1);
const nowMs = signal(0);
const cap = signal(255);
const layers = signal<{ key: number; label: string }[]>([]);
let nextKey = 1;
let loading: Promise<void> | null = null;

function ensureLoaded(model: Model) {
  loading ??= loadLights()
    .then((m) => {
      if (!m) {
        status.value = "missing";
        return;
      }
      if (!m.setUp(model.hw.config.adapterA, groupSetups(model.hw.config))) {
        status.value = "failed";
        return;
      }
      lights.value = m;
      status.value = "ready";
    })
    .catch(() => {
      status.value = "failed";
    });
  return loading;
}

const TARGETS: { value: string; label: string }[] = [
  { value: "playfield", label: "Playfield" },
  { value: "range", label: "Playfield lamps" },
  { value: "perimeterLeft", label: "Perimeter left" },
  { value: "perimeterRight", label: "Perimeter right" },
  { value: "perimeter", label: "Perimeter" },
  { value: "ceiling", label: "Ceiling" },
  { value: "ambient", label: "Ambient" },
];
const KINDS: Kind[] = ["set", "blink", "alternate", "pulse", "fade", "run", "off"];
const STRIP_TARGETS: Record<string, Target> = {
  perimeter_left: "perimeterLeft",
  perimeter_right: "perimeterRight",
  ceiling: "ceiling",
};

const rgb = (h: string) => ({ r: parseInt(h.slice(1, 3), 16), g: parseInt(h.slice(3, 5), 16), b: parseInt(h.slice(5, 7), 16) });

// a paint as the controls hold it: a color, a palette, or the effect below
interface PaintChoice {
  source: "color" | "below" | (typeof PALETTES)[number];
  color: string;
  by: "time" | "position" | "value";
}
const toPaint = (c: PaintChoice): Paint =>
  c.source === "color" ? rgb(c.color) : c.source === "below" ? { none: true } : { palette: c.source, by: c.by };

function PaintPicker({ label, value, onChange }: { label: string; value: PaintChoice; onChange: (v: PaintChoice) => void }) {
  return (
    <div class="field paint">
      <span>{label}</span>
      <select value={value.source} onChange={(e) => onChange({ ...value, source: (e.target as HTMLSelectElement).value as PaintChoice["source"] })}>
        <option value="color">Color</option>
        <option value="below">Below</option>
        {PALETTES.map((p) => (
          <option key={p} value={p}>
            {p}
          </option>
        ))}
      </select>
      {value.source === "color" && (
        <input type="color" value={value.color} onInput={(e) => onChange({ ...value, color: (e.target as HTMLInputElement).value })} />
      )}
      {value.source !== "color" && value.source !== "below" && (
        <select value={value.by} onChange={(e) => onChange({ ...value, by: (e.target as HTMLSelectElement).value as PaintChoice["by"] })}>
          <option value="value">by value</option>
          <option value="time">by time</option>
          <option value="position">by position</option>
        </select>
      )}
    </div>
  );
}

function NumberField({ label, value, onChange, min = 0, max = 65535 }: { label: string; value: number; onChange: (v: number) => void; min?: number; max?: number }) {
  return (
    <label class="field">
      <span>{label}</span>
      <input type="number" min={min} max={max} value={value} onInput={(e) => onChange(Number((e.target as HTMLInputElement).value))} />
    </label>
  );
}

// a ramp's name, from its placement's label up to the first colon, such as "Green ring ramp"
function rampName(model: Model, id: string): string {
  const item = model.pf.items.get(id);
  const label = item?.kind === "placement" ? item.label.split(":")[0].trim() : "";
  return label || shortName(model.pf, id);
}

// the playfield lamp or the strip a click in the drawing picked
export function targetOf(model: Model, item: Item | null): { target: string; first: number; count: number } | null {
  if (!item) return null;
  const lampId = item.kind === "lamp" ? item.id : item.kind === "placement" ? model.pf.lamps.find((l) => l.on === item.id)?.id : undefined;
  const span = lampId ? model.hw.spans.get(lampId) : undefined;
  if (span) return { target: "range", first: span.index, count: 1 };
  const strip = item.kind === "led" ? item.strip : item.kind === "strip" ? item.id : undefined;
  const group = strip ? model.hw.map.strips[strip] : undefined;
  if (group && STRIP_TARGETS[group]) return { target: STRIP_TARGETS[group], first: 0, count: 1 };
  return null;
}

export function StudioControls({ model, state }: { model: Model; state: State }) {
  const m = lights.value;
  const [target, setTarget] = useState("playfield");
  const [first, setFirst] = useState(0);
  const [count, setCount] = useState(1);
  const [kind, setKind] = useState<Kind>("pulse");
  const [a, setA] = useState<PaintChoice>({ source: "color", color: "#ff7800", by: "value" });
  const [b, setB] = useState<PaintChoice>({ source: "color", color: "#0000ff", by: "value" });
  const [onMs, setOnMs] = useState(300);
  const [offMs, setOffMs] = useState(300);
  const [periodMs, setPeriodMs] = useState(1500);
  const [fadeMs, setFadeMs] = useState(800);
  const [pattern, setPattern] = useState<"chase" | "asteroid" | "wipe">("chase");
  const [down, setDown] = useState(false);
  const [step, setStep] = useState(60);
  const [priority, setPriority] = useState(1);
  const [lastingMs, setLastingMs] = useState(0);
  const [adapter, setAdapter] = useState(model.hw.config.adapterA);
  const [playfield, setPlayfield] = useState(1);
  const [ambient, setAmbient] = useState(1);
  const lamps = m?.playfieldLamps() ?? 0;
  const ramps = model.hw.map.ramps;
  const hidden = state.hiddenRamps.value;

  // a click in the drawing picks the target
  const picked = state.picked.value;
  useEffect(() => {
    const t = targetOf(model, picked);
    if (!t) return;
    setTarget(t.target);
    setFirst(t.first);
    setCount(t.count);
  }, [picked]);

  if (!m) return <aside class="list studio-controls" />;

  const strip = target !== "playfield" && target !== "range";
  const start = () => {
    const key = nextKey;
    nextKey = nextKey >= LAST_KEY ? 1 : nextKey + 1;
    const spec: EffectSpec = {
      target: target === "range" ? "playfield" : (target as Target),
      key,
      priority,
      kind,
      a: toPaint(a),
      b: toPaint(b),
      onMs,
      offMs,
      dark: b.source === "color" && b.color === "#000000",
      periodMs,
      ms: fadeMs,
      pattern,
      down,
      step,
    };
    if (target === "range") {
      spec.first = first;
      spec.count = count;
    }
    if (lastingMs > 0) spec.lastingMs = lastingMs;
    if (!m.effect(spec)) return;
    const where = TARGETS.find((t) => t.value === target)!.label + (target === "range" ? ` ${first}–${first + count - 1}` : "");
    layers.value = [...layers.value, { key, label: `${kind} · ${where} · p${priority}` }];
  };

  return (
    <aside class="list studio-controls">
      <h2>Lights</h2>
      <div class="group">
        <div class="field">
          <span>Adapter</span>
          <div class="seg">
            {ADAPTERS.map((v) => (
              <button
                key={v}
                class={v === adapter ? "on" : ""}
                onClick={() => {
                  setAdapter(v);
                  m.setAdapter(v);
                }}
              >
                {v} A
              </button>
            ))}
          </div>
        </div>
        <label class="field">
          <span>Playfield</span>
          <input type="range" min={0} max={1} step={0.05} value={playfield} onInput={(e) => {
            const v = Number((e.target as HTMLInputElement).value);
            setPlayfield(v);
            m.setBrightness(v, ambient);
          }} />
          <output>{Math.round(playfield * 100)} %</output>
        </label>
        <label class="field">
          <span>Ambient</span>
          <input type="range" min={0} max={1} step={0.05} value={ambient} onInput={(e) => {
            const v = Number((e.target as HTMLInputElement).value);
            setAmbient(v);
            m.setBrightness(playfield, v);
          }} />
          <output>{Math.round(ambient * 100)} %</output>
        </label>
      </div>

      <h3>Ramps</h3>
      <div class="group ramps">
        <label class="check">
          <input
            type="checkbox"
            checked={hidden.length === ramps.length}
            onChange={(e) => (state.hiddenRamps.value = (e.target as HTMLInputElement).checked ? [...ramps] : [])}
          />
          Hide all
        </label>
        {ramps.map((id) => (
          <label key={id} class="check">
            <input
              type="checkbox"
              data-ramp={id}
              checked={hidden.includes(id)}
              onChange={(e) =>
                (state.hiddenRamps.value = (e.target as HTMLInputElement).checked ? [...hidden, id] : hidden.filter((r) => r !== id))
              }
            />
            {rampName(model, id)}
          </label>
        ))}
      </div>

      <h3>Effect</h3>
      <div class="group">
        <label class="field">
          <span>Target</span>
          <select data-field="target" value={target} onChange={(e) => setTarget((e.target as HTMLSelectElement).value)}>
            {TARGETS.map((t) => (
              <option key={t.value} value={t.value}>
                {t.label}
              </option>
            ))}
          </select>
        </label>
        {target === "range" && (
          <>
            <NumberField label="First" value={first} max={Math.max(0, lamps - 1)} onChange={setFirst} />
            <NumberField label="Count" value={count} min={1} max={lamps} onChange={setCount} />
          </>
        )}
        <label class="field">
          <span>Kind</span>
          <select value={kind} onChange={(e) => setKind((e.target as HTMLSelectElement).value as Kind)}>
            {KINDS.map((k) => (
              <option key={k} value={k}>
                {k}
              </option>
            ))}
          </select>
        </label>
        {kind !== "off" && <PaintPicker label={kind === "run" ? "Color" : "A"} value={a} onChange={setA} />}
        {(kind === "alternate" || kind === "pulse" || kind === "run" || kind === "blink") && (
          <PaintPicker label={kind === "run" ? "Back" : kind === "blink" ? "Off" : "B"} value={b} onChange={setB} />
        )}
        {(kind === "blink" || kind === "alternate") && (
          <>
            <NumberField label="On ms" value={onMs} onChange={setOnMs} />
            <NumberField label="Off ms" value={offMs} onChange={setOffMs} />
          </>
        )}
        {kind === "pulse" && <NumberField label="Period ms" value={periodMs} onChange={setPeriodMs} />}
        {kind === "fade" && <NumberField label="Fade ms" value={fadeMs} onChange={setFadeMs} />}
        {kind === "run" && (
          <>
            <label class="field">
              <span>Pattern</span>
              <select value={pattern} onChange={(e) => setPattern((e.target as HTMLSelectElement).value as typeof pattern)}>
                <option value="chase">chase</option>
                <option value="asteroid">asteroid</option>
                <option value="wipe">wipe</option>
              </select>
            </label>
            <label class="field">
              <span>Down</span>
              <input type="checkbox" checked={down} onChange={(e) => setDown((e.target as HTMLInputElement).checked)} />
            </label>
            <NumberField label={strip ? "mm/s" : "ms/lamp"} value={step} onChange={setStep} />
          </>
        )}
        <NumberField label="Priority" value={priority} max={255} onChange={setPriority} />
        <NumberField label="Lasting ms" value={lastingMs} onChange={setLastingMs} />
      </div>
      <div class="actions">
        <button class="primary" data-action="start" onClick={start}>
          Start
        </button>
        <button
          onClick={() => {
            m.clearLamps();
            layers.value = [];
          }}
        >
          Clear
        </button>
      </div>
    </aside>
  );
}

export function StudioStatus({ model, state }: { model: Model; state: State }) {
  const m = lights.value;
  const st = status.value;
  if (st !== "ready" || !m) {
    return (
      <section class="card">
        <p class="eyebrow">Studio</p>
        <h2>{st === "loading" ? "Loading the firmware's lamps" : st === "missing" ? "The firmware's lamps are not built" : "The firmware's lamps did not start"}</h2>
        {st === "missing" && (
          <p>
            <code>npm run wasm</code>
          </p>
        )}
      </section>
    );
  }
  const p = m.profile();
  const picked = state.picked.value;
  const t = nowMs.value;
  const pickedLampId = picked?.kind === "lamp" ? picked.id : picked ? model.pf.lamps.find((l) => l.on === picked.id)?.id : undefined;
  const pickedLamp = pickedLampId ? model.hw.spans.get(pickedLampId)?.index : undefined;
  return (
    <section class="card studio-status">
      <div class="pace">
        <button onClick={() => (running.value = !running.value)}>{running.value ? "Pause" : "Run"}</button>
        {!running.value && <button onClick={() => advance(m, model, state, 10)}>+10 ms</button>}
        <div class="seg">
          {SPEEDS.map((s) => (
            <button key={s} class={s === speed.value ? "on" : ""} onClick={() => (speed.value = s)}>
              {s}×
            </button>
          ))}
        </div>
      </div>
      <dl>
        <dt>Time</dt>
        <dd>{(t / 1000).toFixed(2)} s</dd>
        <dt>Cap</dt>
        <dd>
          {p.capMa} mA · {cap.value === 255 ? "not dimming" : `dims to ${cap.value}`}
        </dd>
        <dt>Perimeter</dt>
        <dd>{p.perimeterStep === 1 ? "every LED" : p.perimeterStep === 2 ? "every second LED" : `every ${p.perimeterStep}th LED`}</dd>
        <dt>Ambient max</dt>
        <dd>{p.ambientMax}</dd>
        <dt>Playfield max</dt>
        <dd>{p.playfieldMax}</dd>
        <dt>Refused</dt>
        <dd>{m.refused()}</dd>
        {picked && (
          <>
            <dt>Picked</dt>
            <dd>
              {shortName(model.pf, picked.id)}
              {pickedLamp !== undefined && ` · playfield lamp ${pickedLamp}`}
            </dd>
          </>
        )}
      </dl>
      <h4>Layers</h4>
      <ul class="layers">
        {layers.value.map((l) => (
          <li key={l.key}>
            <span>{l.label}</span>
            <button
              class="close"
              title="Remove the layer"
              onClick={() => {
                m.removeLayer(l.key);
                layers.value = layers.value.filter((x) => x.key !== l.key);
              }}
            >
              ×
            </button>
          </li>
        ))}
      </ul>
    </section>
  );
}

// Runs the main loop for ms of simulated time and hands the drawing the colors.
function advance(m: LightsModule, model: Model, state: State, ms: number) {
  m.step(ms);
  nowMs.value = m.nowMs();
  cap.value = m.capBrightness();
  state.glow.value = drawingColors(model.pf, model.hw, readChains(m));
}

// Loads the module as the app starts, so the studio opens ready, and runs its clock in real time
// while the studio shows.
export function useStudioClock(model: Model, state: State) {
  const active = state.view.value === "studio";
  useEffect(() => {
    void ensureLoaded(model);
  }, []);
  useEffect(() => {
    if (!active) {
      state.glow.value = null;
      return;
    }
    void ensureLoaded(model).then(() => {
      if (lights.value) advance(lights.value, model, state, 0);
    });
    let frame = 0;
    let last = performance.now();
    let carry = 0;
    const tick = (now: number) => {
      const m = lights.value;
      const dt = Math.min(now - last, MAX_STEP_MS);
      last = now;
      if (m && running.value) {
        carry += dt * speed.value;
        const ms = Math.floor(carry);
        carry -= ms;
        if (ms > 0) advance(m, model, state, ms);
      }
      frame = requestAnimationFrame(tick);
    };
    frame = requestAnimationFrame(tick);
    return () => cancelAnimationFrame(frame);
  }, [active]);
}

// the studio's module, its status, its layers and its pace
export const studio = { lights, status, layers, running };
