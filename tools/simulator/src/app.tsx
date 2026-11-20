// The Features view: the list on the left, the drawing in the middle, the feature's or the part's card on
// the right. A click on a part in the drawing shows the part's card.
import type { Model, State } from "./state";
import { Drawing } from "./ui/Drawing";
import { FeatureCard, FeatureList, PartCard, TopBar } from "./ui/Panels";

export function App({ model, state }: { model: Model; state: State }) {
  return (
    <div class="app">
      <TopBar />
      <main class="features-view">
        <FeatureList model={model} state={state} />
        <Drawing model={model} state={state} />
        <div class="side">{state.inspected.value ? <PartCard model={model} state={state} /> : <FeatureCard model={model} state={state} />}</div>
      </main>
    </div>
  );
}
