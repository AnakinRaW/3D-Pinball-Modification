---
name: review-schematic-svg
description: Use to check that a schematic SVG in this repository matches the circuit it documents and is unambiguous to read - verifies every pin and value against the source markdown, junction dots, net colour consistency, English labels and decimal notation. Trigger on "check this diagram", "does the SVG match the text", or before committing a drawn schematic.
---

# Reviewing a schematic SVG

Two failures matter: the diagram disagrees with the document, and the diagram can be read two ways. Both put wrong wiring on a bench.

## Inputs

The SVG, and the markdown describing the circuit it depicts. Reviewing an SVG without its source document is not possible - say so rather than guessing what was intended.

## Checks

### Agreement with the source

- Every component in the document appears in the diagram, and nothing appears that the document does not name.
- Every component **value** matches: resistors, capacitors, voltages, currents.
- Every pin number and pin name matches, including the connector pinouts.
- Every net in the document is drawn, with the same endpoints.

Any mismatch is reported as a finding against the diagram. The document is the authority.

### Readability

- **Junction dots.** Every place wires join carries `<circle r="4"/>` in the net colour. Every crossing without a dot is genuinely not connected. This is the single most common way such a diagram misleads.
- **Orphan pins.** Every pin circle drawn on a module either has a wire or is labelled as unused.
- **Net labels.** Every net is named somewhere along its run.
- **Colour consistency.** One net class, one colour, throughout. The same colour never appears on two unrelated nets.
- **Package variants.** Every part whose type designation depends on its package appears in the variants table with both types named. Parts whose designation is package-independent are absent from it. Designators in the table match those on the drawn symbols.
- **Overlap.** No text sits on a wire or another label.
- **Wire endpoints.** Lines actually meet the pin circles - a gap of a few units reads as connected but is a drawing error worth fixing before it is copied.

### Language and notation

Required by rule 1 and by the notation used across the repository.

- All labels English. Flag any German text.
- Decimal point, not comma: `3.3 V`.
- Space between value and unit.
- File name kebab-case, English, correctly spelled.

### Constraint sanity

Not a substitute for `review-circuit`, but cheap to check while reading:

- A voltage annotated on a net that reaches a Teensy pin must be at or below 3.3 V.
- Any pin drawn driving a load directly should already have been through `review-circuit`.

## Report

| Severity | Meaning |
|---|---|
| **Wrong** | Diagram contradicts the document |
| **Ambiguous** | Can be read two ways - missing junction dot, unlabelled net, overlapping text |
| **Convention** | Language, notation, naming, colour reuse |

State the SVG line number for each finding.
