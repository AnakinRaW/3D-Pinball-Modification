// The app: the Features view with the list, the drawing and the feature's or the part's card, and the
// studio with its controls, the drawing in the firmware's colors, the clock with the layers and the current.
import type { Model, State } from "./state";
import { Drawing } from "./ui/Drawing";
import { FeatureCard, FeatureList, PartCard, TopBar } from "./ui/Panels";
import { StudioControls, StudioCurrent, StudioStatus, useStudioClock } from "./ui/Studio";

export function App({ model, state }: { model: Model; state: State }) {
  useStudioClock(model, state);
  const studio = state.view.value === "studio";
  return (
    <div class="app">
      <TopBar state={state} />
      <main class="features-view">
        {studio ? <StudioControls model={model} state={state} /> : <FeatureList model={model} state={state} />}
        <Drawing model={model} state={state} />
        <div class="side">
          {studio ? (
            <>
              <StudioStatus model={model} state={state} />
              <StudioCurrent model={model} />
            </>
          ) : state.inspected.value ? (
            <PartCard model={model} state={state} />
          ) : (
            <FeatureCard model={model} state={state} />
          )}
        </div>
      </main>
    </div>
  );
}
