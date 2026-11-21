// A bar of pills over the drawing that hides and shows the ramps, so the lamps under them show. Each
// pill carries its ramp's name and color from the machine map; a hidden ramp's pill fades out.
import type { Model, State } from "../state";

export function RampSwitch({ model, state }: { model: Model; state: State }) {
  const ramps = model.hw.map.ramps;
  const hidden = state.hiddenRamps.value;
  const allHidden = hidden.length === ramps.length;
  const toggle = (id: string) =>
    (state.hiddenRamps.value = hidden.includes(id) ? hidden.filter((r) => r !== id) : [...hidden, id]);
  return (
    <div class="ramp-switch" role="toolbar" aria-label="Ramps">
      <button
        class={`ramp-pill all${allHidden ? " off" : ""}`}
        data-ramp="all"
        aria-pressed={!allHidden}
        title={allHidden ? "Show every ramp" : "Hide every ramp"}
        onClick={() => (state.hiddenRamps.value = allHidden ? [] : ramps.map((r) => r.id))}
      >
        <span class="swatch" />
        Ramps
      </button>
      {ramps.map((r) => {
        const off = hidden.includes(r.id);
        return (
          <button
            key={r.id}
            class={`ramp-pill${off ? " off" : ""}`}
            data-ramp={r.id}
            aria-pressed={!off}
            title={`${off ? "Show" : "Hide"} the ${r.name.toLowerCase()}`}
            style={r.color ? { "--ramp": r.color } : undefined}
            onClick={() => toggle(r.id)}
          >
            <span class="swatch" />
            {r.name}
          </button>
        );
      })}
    </div>
  );
}
