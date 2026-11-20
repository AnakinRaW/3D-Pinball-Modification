// The current readout: what the machine draws from its adapter in the frame shown, the coils' and the
// servo's buttons, and the design's worst cases for the selected adapter. The clock is the one the frames
// run on, so a pull lasts its pull time at any pace and holds still while the clock stands.
import { useMemo, useRef, useState } from "preact/hooks";
import type { Hardware } from "../model/config";
import {
  coils as coilsOf,
  draw,
  FIGURES,
  fire,
  holdPeak,
  moveServo,
  ledDraw,
  mostAtOnce,
  pulling,
  worstCases,
  type Adapter,
  type BaseKey,
  type Coil,
  type Figures,
  type Pulls,
} from "../model/current";
import "./current.css";

const PEAK_MS = 1000;

const BASE_LABELS: Record<BaseKey, string> = {
  i_teensy_vin: "Teensy",
  i_amp: "Amplifier",
  i_ir_in: "IR board",
  i_disp: "Display",
  i_rest: "Servo at rest",
};

export function amps(ma: number): string {
  return Math.abs(ma) < 1000 ? `${Math.round(ma)} mA` : `${(ma / 1000).toFixed(2)} A`;
}

function coilLabel(c: Coil): string {
  return c.id.replace(/^ball-/, "").replace(/-(\d+)$/, " $1").replace(/^\w/, (s) => s.toUpperCase());
}

export interface ReadoutProps {
  hw: Hardware;
  adapter: Adapter;
  chains: Uint8Array[]; // the colors of the frame shown, three bytes for each LED
  capBrightness: number;
  nowMs: number;
  figures?: Figures;
}

export function CurrentReadout({ hw, adapter, chains, capBrightness, nowMs, figures = FIGURES }: ReadoutProps) {
  const f = figures;
  const all = useMemo(() => coilsOf(hw, f), [hw, f]);
  const [pulls, setPulls] = useState<Pulls>(new Map());
  const [servoEnds, setServoEnds] = useState(-Infinity);
  const samples = useRef<[number, number][]>([]);

  const active = pulling(all, pulls, nowMs);
  const servo = servoEnds > nowMs;
  const led = ledDraw(chains, capBrightness, f);
  const d = draw(led, active, servo, adapter, f);
  const peak = holdPeak(samples.current, nowMs, d.total, PEAK_MS);
  const share = Math.min(d.total / d.supply, 1.5);
  const pullAll = (list: Coil[]) => setPulls(list.reduce((p, c) => fire(p, c, nowMs), pulls));
  const most = mostAtOnce(all, f);

  return (
    <section class={`current ${d.over > 0 ? "over" : ""}`} data-testid="current">
      <h2>Current</h2>
      <div class="current-total">
        <output data-testid="current-total">{amps(d.total)}</output>
        <span class="muted"> of {amps(d.supply)}</span>
        {d.over > 0 && <span class="current-over" data-testid="current-over">+{amps(d.over)} past the adapter</span>}
      </div>
      <div class="current-bar" role="meter" aria-valuemin={0} aria-valuemax={d.supply} aria-valuenow={Math.round(d.total)}>
        <div style={{ width: `${(share / 1.5) * 100}%` }} />
        <i style={{ left: `${100 / 1.5}%` }} />
      </div>
      <div class="current-peak muted">
        Peak, last second <output data-testid="current-peak">{amps(peak)}</output>
      </div>

      <table class="current-rows">
        <tbody>
          <tr>
            <th>LEDs</th>
            <td data-testid="current-led">{amps(d.led)}</td>
            <td class={led.dimmed ? "current-dimmed" : "muted"} data-testid="current-cap">
              {led.dimmed ? `cap ${led.brightness}/255` : "under cap"}
            </td>
          </tr>
          {(Object.keys(BASE_LABELS) as BaseKey[]).map((k) => (
            <tr key={k} class="muted">
              <th>{BASE_LABELS[k]}</th>
              <td>{amps(f[k].value)}</td>
              <td />
            </tr>
          ))}
          <tr>
            <th>Coils</th>
            <td data-testid="current-coils">{amps(d.coils)}</td>
            <td class="muted">{active.length ? `${active.length} pulling` : ""}</td>
          </tr>
          <tr>
            <th>Servo</th>
            <td data-testid="current-servo">{amps(d.servo)}</td>
            <td class={d.servoWithCoil ? "current-over" : "muted"}>{d.servoWithCoil ? "with a coil" : servo ? "moving" : ""}</td>
          </tr>
        </tbody>
      </table>

      <div class="current-buttons">
        {all.map((c) => (
          <button
            key={c.channel}
            class={active.includes(c) ? "on" : ""}
            data-testid={`coil-${c.channel}`}
            title={`${amps(f[c.current].value)} for ${c.pullMs} ms`}
            onClick={() => pullAll([c])}
          >
            {coilLabel(c)}
          </button>
        ))}
        <button data-testid="coil-most" title={`${amps(f.i_coils_at_once.value)} for ${f.t_on_max.value} ms`} onClick={() => pullAll(most)}>
          {most.length} at once
        </button>
        <button
          class={servo ? "on" : ""}
          data-testid="servo"
          title={`${amps(f.i_stall.value)} for ${f.t_move_max.value} ms`}
          onClick={() => setServoEnds(moveServo(nowMs, f))}
        >
          Servo
        </button>
      </div>

      <h3>Worst cases, {adapter} A</h3>
      <table class="current-rows" data-testid="current-worst">
        <tbody>
          {worstCases(f, adapter).map((w) => (
            <tr key={w.key} class={w.over > 0 ? "current-over" : ""}>
              <th>{w.key === "most" ? `Cap + ${most.length} coils` : "Full white"}</th>
              <td>{amps(w.ma)}</td>
              <td>
                {w.over > 0
                  ? `+${amps(w.over)} for ≤ ${w.forMs} ms`
                  : `of ${amps(w.limit)}${w.key === "white" ? " cap" : ""}`}
              </td>
            </tr>
          ))}
        </tbody>
      </table>
    </section>
  );
}
