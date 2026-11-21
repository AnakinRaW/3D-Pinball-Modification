// The app: the Features view with the list, the drawing and the feature's or the part's card; the
// Games view with every source's games as covers below the picked one; and the studio with
// the game it edits, its Lights controls, the drawing in the firmware's colors, the clock with the
// layers and the current.
import type { Model, State } from "./state";
import { Drawing } from "./ui/Drawing";
import { GamesView } from "./ui/Games";
import { FeatureCard, FeatureList, PartCard, ThemeSwitch, TopBar } from "./ui/Panels";
import { panels, Splitter } from "./ui/Splitter";
import { StudioControls, StudioCurrent, StudioStatus, useStudioClock } from "./ui/Studio";

export function App({ model, state }: { model: Model; state: State }) {
  useStudioClock(model, state);
  const view = state.view.value;
  const left =
    view === "studio" ? (
      <StudioControls model={model} state={state} />
    ) : (
      <FeatureList model={model} state={state} />
    );
  const side =
    view === "studio" ? (
      <>
        <StudioStatus model={model} state={state} />
        <StudioCurrent model={model} />
      </>
    ) : state.inspected.value ? (
      <PartCard model={model} state={state} />
    ) : (
      <FeatureCard model={model} state={state} />
    );
  return (
    <div class="app">
      <TopBar state={state} />
      {view === "games" ? (
        <GamesView state={state} />
      ) : view === "studio" ? (
        <main
          class="features-view resizable"
          style={{ gridTemplateColumns: `${panels.value.left}px 6px minmax(0, 1fr) 6px ${panels.value.right}px` }}
        >
          {left}
          <Splitter side="left" />
          <Drawing model={model} state={state} />
          <Splitter side="right" />
          <div class="side">{side}</div>
        </main>
      ) : (
        <main class="features-view">
          {left}
          <Drawing model={model} state={state} />
          <div class="side">{side}</div>
        </main>
      )}
      <ThemeSwitch />
    </div>
  );
}
