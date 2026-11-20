// The top bar, the feature list, the feature card and a part's card.
import { Fragment } from "preact";
import { useState } from "preact/hooks";
import { applyTheme, storedTheme, systemTheme, type Theme } from "./theme";
import type { Inline } from "../model/markdown";
import { card } from "../model/part-card";
import { shortName } from "../model/playfield";
import type { Model, State } from "../state";

export function InlineText({ tokens }: { tokens: Inline[] }) {
  return (
    <>
      {tokens.map((t, i) =>
        t.kind === "strong" ? (
          <strong key={i}>{t.text}</strong>
        ) : (
          <span key={i}>{t.text}</span>
        ),
      )}
    </>
  );
}

const VIEWS = [
  { value: "features", label: "Features" },
  { value: "studio", label: "Studio" },
] as const;
const LATER = ["Rules", "Play", "Replays"];

export function TopBar({ state }: { state: State }) {
  const view = state.view.value;
  const [theme, setTheme] = useState<Theme>(storedTheme() ?? systemTheme());
  const other: Theme = theme === "light" ? "dark" : "light";
  return (
    <header class="topbar">
      <span class="brand">Pinball Simulator</span>
      <nav class="tabs">
        {VIEWS.map((v) => (
          <button
            key={v.value}
            class={`tab${view === v.value ? " active" : ""}`}
            aria-current={view === v.value ? "page" : undefined}
            data-view={v.value}
            onClick={() => (state.view.value = v.value)}
          >
            {v.label}
          </button>
        ))}
        {LATER.map((t) => (
          <button key={t} class="tab" disabled title="Comes with a later phase">
            {t}
          </button>
        ))}
      </nav>
      <span class="spacer" />
      <button
        class="theme"
        data-theme-switch={other}
        title={`Switch to the ${other} theme`}
        onClick={() => {
          applyTheme(other);
          setTheme(other);
        }}
      >
        {other === "light" ? "Light" : "Dark"}
      </button>
      <span class="build" title="The commit the app was built from">
        build {__COMMIT__}
      </span>
    </header>
  );
}

export function FeatureList({ model, state }: { model: Model; state: State }) {
  const sel = state.selected.value;
  const { placed, unplaced } = model.features;
  const pick = (key: string) => () => {
    state.selected.value = key;
    state.inspected.value = null;
  };
  return (
    <aside class="list">
      <h2>What the mod adds</h2>
      <p class="muted">{placed.length} features on the machine</p>
      <ol class="features">
        {placed.map((f) => (
          <li key={f.key}>
            <button class={`feature${f.key === sel ? " selected" : ""}`} onClick={pick(f.key)} data-key={f.key}>
              <span class="num">{f.n}</span>
              <span class="text">
                <span class="name">{f.name}</span>
                <span class="summary">{f.summary}</span>
              </span>
            </button>
          </li>
        ))}
      </ol>
      {unplaced.length > 0 && (
        <>
          <h3>In the firmware</h3>
          <ul class="features">
            {unplaced.map((f) => (
              <li key={f.key}>
                <button class={`feature small${f.key === sel ? " selected" : ""}`} onClick={pick(f.key)} data-key={f.key}>
                  <span class="name">{f.name}</span>
                  <span class="section">{f.section}</span>
                </button>
              </li>
            ))}
          </ul>
        </>
      )}
    </aside>
  );
}

export function FeatureCard({ model, state }: { model: Model; state: State }) {
  const f = state.feature.value;
  if (!f) return null;
  const { placed } = model.features;
  const all = model.all;
  const k = all.indexOf(f);
  const go = (d: number) => () => (state.selected.value = all[(k + d + all.length) % all.length].key);
  const marks = state.highlight.value.marks;
  return (
    <section class="card feature-card">
      <p class="eyebrow">{f.n !== undefined ? `Feature ${f.n} of ${placed.length}` : f.section}</p>
      <h2>{f.name}</h2>
      {f.summary && <p class="lead">{f.summary}</p>}
      {f.rows.map((r) => (
        <div key={r.name} class="row">
          {f.rows.length > 1 && <h3>{r.name}</h3>}
          {r.stock.length > 0 && (
            <>
              <h4>On the stock machine</h4>
              <p>
                <InlineText tokens={r.stock} />
              </p>
            </>
          )}
          <h4>What the mod adds</h4>
          <p>
            <InlineText tokens={r.modification} />
          </p>
        </div>
      ))}
      {marks.length > 0 && (
        <>
          <h4>On the machine</h4>
          <div class="chips">
            {marks.map((m) => (
              <button
                key={m.id}
                class="chip"
                onClick={() => (state.inspected.value = m.id)}
              >
                {m.label}
              </button>
            ))}
          </div>
        </>
      )}
      <div class="nav">
        <button onClick={go(-1)}>‹ Previous</button>
        <button onClick={go(1)}>Next ›</button>
      </div>
    </section>
  );
}

export function PartCard({ model, state }: { model: Model; state: State }) {
  const id = state.inspected.value;
  if (!id) return null;
  const c = card(model.pf, model.hw, model.features.placed, id);
  if (!c) return null;
  const inspect = (x: string) => () => (state.inspected.value = x);
  return (
    <section class="card part-card">
      <div class="card-head">
        <p class="eyebrow">{c.kind}</p>
        <button class="close" title="Close the card" onClick={() => (state.inspected.value = null)}>
          ×
        </button>
      </div>
      <h2>{c.title}</h2>
      {c.description && <p class="lead">{c.description}</p>}
      <dl>
        <dt>Id</dt>
        <dd>
          <code>{c.id}</code>
        </dd>
        {c.fields.map(([k, v]) => (
          <Fragment key={k}>
            <dt>{k}</dt>
            <dd>{v}</dd>
          </Fragment>
        ))}
      </dl>
      {c.lamps.length > 0 && (
        <>
          <h4>Lamps on it</h4>
          <div class="chips">
            {c.lamps.map((l) => (
              <button key={l} class="chip" onClick={inspect(l)}>
                {shortName(model.pf, l)}
              </button>
            ))}
          </div>
        </>
      )}
      {c.features.length > 0 && (
        <>
          <h4>Features</h4>
          <div class="chips">
            {c.features.map((name) => {
              const f = model.features.placed.find((x) => x.name === name)!;
              return (
                <button
                  key={name}
                  class="chip"
                  onClick={() => {
                    state.selected.value = f.key;
                    state.inspected.value = null;
                  }}
                >
                  {name}
                </button>
              );
            })}
          </div>
        </>
      )}
    </section>
  );
}
