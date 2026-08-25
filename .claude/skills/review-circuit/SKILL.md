---
name: review-circuit
description: Use before building or committing any circuit, wiring plan or schematic for this modification - checks voltage limits, current ratings, missing current limiting, undefined states at boot, pin conflicts, decoupling and connector hazards against the datasheets and this repository's constraint documents. Trigger on "review this circuit", "is this safe to build", "check my wiring", "will this destroy the Teensy", or before a design moves from a document into hardware.
---

# Circuit review

Find what destroys hardware, then what stops the circuit working, then what has no margin.

## 1. Load the constraints before reviewing anything

Read these first. They hold the numbers this review is judged against. Do not review from memory of them.

| Source | Supplies |
|---|---|
| `docs/research/teensy-4.1.md` | Pin voltage limit, per-pin current, VIN range, published draw |
| `docs/pin-assignment.md` | Pins already spent, and the peripherals each one locks out |
| `docs/parts-list.md` | Parts in play and their spec links |
| `docs/research/*.md` | Component notes, measurements of the stock machine |
| `docs/parts/*/README.md` | The circuit under review and the subsystems it connects to |
| `docs/datasheets/*.pdf` | Manufacturer datasheets held locally |
| Manufacturer datasheets | Absolute maximums for every part in the circuit under review |

If a value the review needs is absent from all of them, that is a finding. Do not fill the gap with a plausible number.

## 2. Checks

Work through every category. A category with nothing to report is reported as clean, so the reader knows it was examined.

### Voltage domains

For every net that reaches a Teensy pin, establish the highest voltage it can carry **under any condition** - normal operation, during boot, with a connector unplugged, with a wire shorted to a neighbour, with an inductive load switching off.

- The Teensy 4.1 is not 5 V tolerant on any digital or analog pin.
- Sources of over-voltage that are easy to miss: a pull-up to a 5 V rail, an open-collector output referenced to a higher rail, a sensor powered from a rail above the one it signals into, inductive kickback from a solenoid or motor, a long cable acting as an antenna or accumulating ESD.
- A divider or level shifter is a fix. "The output never actually goes that high" is not, unless the ceiling is set by a rail rather than by expected behaviour.

### Current

- Every LED, coil and load: is a current limit present and computed? Show `I = (V_supply - V_drop) / R`.
- Every Teensy pin driving a load directly: PJRC recommends a **4 mA maximum** per pin, not the 10 mA in the cross-generation comparison table. Anything above that needs a transistor or driver.
- Every resistor: `P = I^2 * R` against the part's power rating.
- Every rail: sum the worst-case simultaneous draw against what the regulator or the USB supply can deliver.

### Undefined states

- Every transistor base and MOSFET gate: what holds it off while the Teensy boots and its pins are still high-impedance? Without a pull-down, a load can switch on at power-up.
- Every input: can it float? A floating CMOS input oscillates and draws current.
- Every output that drives something dangerous: what is its state before firmware runs?

### Pin allocation

- Cross-check every pin against `docs/pin-assignment.md`. A conflict across sessions is only caught here.
- ADC-capable pins on the Teensy 4.1 are 14-27 (A0-A13) and 38-41 (A14-A17). Verify any analog input sits inside that set.
- Pin 13 carries the onboard LED.
- Taking one pin of a bus or a serial port commits the whole group. Check the shared-resource tables, not just the single pin.

### Supply and return

- Decoupling: bulk capacitor plus 100 nF ceramic per regulator and per IC, placed close to the part.
- An LDO's output capacitor is part of its control loop. Confirm the type and value the datasheet requires.
- Electrolytic and tantalum capacitors are polarised. Confirm orientation against the rail.
- Every current has a return path. Trace it. Where a board has no ground wire, name what the return actually is.
- Reverse polarity protection on any externally supplied rail.

### Connectors

- Can a plug go in reversed, or into the wrong socket of the same size? Say what happens if it does.
- Where a swap is destructive rather than merely dead, require a polarised or keyed connector.

### Margin

For each operating point, state the ratio to the absolute maximum. A design with no stated margin has not been reviewed.

## 3. Package variants

Where a circuit names a through-hole and an SMD build, the two share topology, values and netlist, and share nothing else. Run these **per variant**, and label every finding with the variant it applies to:

| Figure | Why it differs |
|---|---|
| Resistor power rating | An axial part is commonly 0.25 W, a 0805 chip commonly 0.125 W |
| Capacitor voltage rating and DC-bias loss | A smaller case loses more capacitance under the same working voltage |
| Semiconductor current rating and R_DS(on) | Different die and different bond-out per package |
| Thermal resistance | Between TO-220 and SOT-223 this differs by roughly an order of magnitude |

A variant is not verified because the other one passed. An unchecked variant is reported as Unverifiable, naming the datasheet figure that would settle it.

## 4. Report

One table, most severe first.

| Severity | Meaning |
|---|---|
| **Destroys hardware** | Exceeds an absolute maximum, or can under a plausible fault |
| **Malfunctions** | Works electrically, fails to do its job |
| **No margin** | Inside ratings with too little headroom for tolerance or worst case |
| **Unverifiable** | Needs a number no source provides - name the measurement that would settle it |

Each finding carries: the net or component, the failure mode in one sentence, the arithmetic with units, and the source of every figure used.

## 5. Rules for this review

- **Re-derive from the sources.** An earlier conclusion in the same conversation is not evidence.
- **Show the arithmetic** with units, so it can be checked without electrical intuition.
- **Cite every figure** - datasheet and page, or the measurement it came from.
- **Never present an estimate as measured.** Mark estimates as estimates.
- **Stop rather than guess.** When a check cannot be completed from available sources, report it as Unverifiable and name what would resolve it.
