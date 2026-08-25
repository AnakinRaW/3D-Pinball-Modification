---
name: verify-easyeda
description: Use to verify that an EasyEDA Pro board matches its documented design - exports the netlist, diffs it against a baseline after every edit, and cross-checks connectivity against the circuit document and the part list. Trigger on "check the EasyEDA project", "did that edit connect what I think", "verify the netlist", or after any schematic change in EasyEDA.
---

# Verifying an EasyEDA project

The canvas shows intent. The netlist shows connectivity. Two symbols can touch on screen without sharing a net, and a wire can silently merge two nets that were meant to stay apart. Verify against the netlist.

Adapted from [v0id-byte/easyeda-pro-claude-skill](https://github.com/v0id-byte/easyeda-pro-claude-skill) (MIT), keeping the netlist-diff discipline. The GUI-automation and third-party MCP channels of that skill are not used here.

## Files

| Extension | Container | Handling |
|---|---|---|
| `.epro2` | ZIP - `project.json` plus `SHEET/`, `PCB/`, `SYMBOL/`, `FOOTPRINT/`, entries named by UUID | `unzip -o project.epro2 -d work/`, then read `project.json` for the UUID mapping |
| `.eprj` / `.eprj2` | Single SQLite database used by offline projects | Not readable without `sqlite3`, and reportedly encrypted. Prefer `.epro2` |
| `.tel` | Allegro Telesis netlist, plain text | The ground truth for connectivity |

Structure reference: <https://github.com/easyeda/easyeda-pro-file-format-v2>

## Loop

Run this for **every** change, not once at the end. A netlist defect found three edits later costs more to locate than to prevent.

1. **Baseline.** In EasyEDA: File - Export - Netlist - Allegro. Save as `baseline.tel`.
2. **One scoped change.**
3. **Re-export** to `after.tel`.
4. **Diff.** `diff baseline.tel after.tel`
5. **Read the diff against intent.** Every changed line is either the change that was wanted, or a defect. There is no third category.
6. `after.tel` becomes the next baseline.

## Reading a .tel

Nets are listed with the pins that belong to them.

```bash
# every net a part's pin appears on
grep -nE "U1\.7( |$)" after.tel

# pins on one net
awk "/^'?GND'? ;/{p=1;next} /;$/{p=0} p" after.tel

# auto-named nets: usually an accidental connection, not a deliberate one
grep -nE "^'?NET[0-9]+'? ;" after.tel
```

## Checks

### Against the netlist

- **Silent net merge.** Nets that must stay separate appear as separate entries. Where a board has 3V3 and 5V, both appear; one missing means they merged.
- **Virtual connection.** A port placed a fraction off its wire looks connected and is not. Every pin the document says is connected appears on that net in the `.tel`.
- **Stray nets.** Auto-named `NET…` entries are unintended unless a reason is recorded.
- **Unconnected pins.** Every pin is either on a net or carries an explicit no-connect.
- **Pin remap after a part swap.** Changing a part can move pin functions while keeping the same designators. Re-check every net that touches a swapped part.

### Against the repository

- Every component in the netlist appears in `docs/parts-list.md`, and every electronics row in the part list appears in the netlist.
- Reference designators follow IEEE 315 - `A` for a separable module, `U` for an inseparable IC.
- Every Teensy pin used matches `docs/pin-assignment.md`.
- Connectivity matches the circuit document in `docs/`.

## What this does not do

Connectivity only. It confirms the board matches the intended circuit, and says nothing about whether that circuit is electrically sound. Run `review-circuit` for voltage limits, current ratings and margins.

## Commit alongside the project

The `.epro2` is one opaque blob to git. Export and commit the netlist, a BOM CSV and a schematic PDF next to it - those are the reviewable layer and the input to this check.
