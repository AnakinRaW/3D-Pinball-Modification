---
name: design-review
description: Use to judge whether a design meets the professional quality bar of rule 3, not merely whether it works - covers derating targets, thermal calculation, schematic craft, inductive loads, testability, serviceability and failure behaviour. Trigger on "is this good design", "would an engineer approve this", "review the quality", "is this professional", or before a board is sent for fabrication.
---

# Design review

`review-circuit` answers whether the circuit survives. This answers whether it is *good* - the judgement a reviewing engineer makes on top of correctness. Rule 3 sets the bar; this operationalises it.

Run after `review-circuit`, never instead of it.

## 1. Derating

Working inside the absolute maximum is correctness. Professional practice leaves margin for tolerance, ageing and worst case.

| Part | Target | Reason |
|---|---|---|
| Resistor power | ≤ 50 % of rating | Rating assumes free air at 25 °C; a board is neither |
| Ceramic capacitor voltage | ≤ 50 % of rating | See DC bias below |
| Electrolytic voltage | ≤ 80 % of rating | Ripple and ageing |
| Semiconductor continuous current | ≤ 80 % | Datasheet figures assume ideal cooling |
| Connector current per contact | ≤ 80 % | Contact resistance rises with wear |
| LED forward current | ≤ 80 % of maximum | Lifetime falls steeply near the limit |

**DC bias on ceramics.** X5R and X7R capacitors lose a large fraction of their capacitance under applied voltage - a 10 µF 6.3 V part at 5 V can retain well under half its nominal value, and the loss is not on the front page of the datasheet. Where a capacitance value matters (regulator output, filter corner), verify the bias curve or choose a part rated at least twice the working voltage. Flag any ceramic whose rating is under 2× its working voltage.

## 2. Thermal

Every part that dissipates gets a calculation, not an assumption.

```
P            = worst-case dissipation, shown with its terms
delta_T      = P * Rth(j-a)              from the datasheet
T_junction   = T_ambient + delta_T
```

Report `T_junction` against the datasheet limit. Ambient inside a closed wooden machine is not room temperature - state the figure used.

The usual offender is a linear regulator: `P = (V_in - V_out) * I_out`, all of it as heat, independent of package.

## 3. Schematic craft

What makes a schematic reviewable by someone who was not there.

- **Reference designators** sequential, no gaps, IEEE 315 - `A` separable module, `U` inseparable IC, `R`/`C`/`D`/`Q`/`J` conventional.
- **Net names** functional and readable: `3V3`, `GND`, `LED_BUS`, `S1_SIG`. Any auto-generated `NET…` name is a defect.
- **Power symbols** rather than wires running off to nowhere.
- **Signal flow** left to right, supply at the top, ground at the bottom.
- **Decoupling drawn beside its IC**, not collected in a corner of the sheet.
- **Every pin accounted for** - connected, or carrying an explicit no-connect marker.
- **Values and ratings on every passive** where the rating is part of the design.
- **Title block**: board name, revision, date.
- **Functional blocks grouped** and labelled, one concern per region of the sheet.

## 4. Inductive and switching loads

- A flyback diode across every relay coil, solenoid and motor. Without it the switching device sees a voltage spike far above the supply.
- Gate resistors on MOSFETs, and a defined gate potential at all times.
- Switching current loops kept physically small, and away from analog signal runs.
- Star ground: a single return point, so switching return current does not flow through an analog reference.

## 5. Testability and serviceability

The difference between a board that can be debugged and one that must be replaced.

- Test points on every supply rail and every signal worth probing.
- Connectors keyed or polarised, and labelled on the silkscreen.
- Pin 1 marked. Polarity marked on every polarised part.
- Designators readable on the assembled board.
- Modules replaceable without desoldering the parts around them.

## 6. Failure behaviour

For each of these, state what happens and whether the result is dangerous, dead, or degraded:

- A connector unplugged while powered.
- A cable to an external board shorted, conductor to conductor and to ground.
- Firmware hung with outputs in their last state.
- Supply applied reversed.
- One shared part failing - name what else it takes with it.

Fusing or current limiting on any externally routed rail, so a fault stays local.

## 7. Reproducibility

Rule 3 requires that someone else could rebuild this from the repository alone.

- The decision is recorded, including the rejected alternatives and why they lost.
- Every part resolves from `docs/parts-list.md` to a specification.
- A bring-up procedure exists: inspect, check for shorts between supply and ground, power through a current-limited supply, then verify rail by rail.
- Known limitations written down rather than left silent.

## Report

| Severity | Meaning |
|---|---|
| **Below bar** | Violates rule 3 - would not pass a reviewing engineer |
| **Thin margin** | Works, but with less headroom than practice expects |
| **Craft** | Schematic or board conventions |
| **Missing** | Something rule 3 requires that does not exist yet |

Each finding names the target it misses and the calculation or convention behind it. Where a judgement depends on a number no source provides, report it as unverifiable and name the measurement that settles it.
