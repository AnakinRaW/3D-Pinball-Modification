# Project Rules

This file is the working agreement for this repository and outranks every other instruction in it.

## 1. Language

- **Repository language is English.** All code, comments, commit messages, documentation, schematics, labels and file names are in English.
- **English is spelled the American way.** Write color, center, behavior, initialize, meter, aluminum and toward. Names in code follow the same spelling.
- **Conversation may be German or English.** Follow the language the question is asked in.

## 2. Who this project is for

Expect a **software developer's** background, not an electrician's or an electrical engineer's:

- Little experience reading or judging schematics, and no feel for what a professional one looks like.
- Little experience with electrical safety.
- No intuition for what destroys a component: over-voltage, reversed polarity, missing current limiting, floating inputs, inrush, ESD, thermal limits.
- Strong software-engineering intuition. Analogies to software land well, hardware conventions do not.

**Therefore:**

- Name the failure mode a design choice prevents.
- Never present a guess as a fact. State confidence, and separate "from the datasheet" from "my estimate".
- Cite the source of an electrical value: datasheet page, standard or measurement. A fabricated number will not be caught here.
- Proactively flag anything that can destroy hardware or injure a person, even when not asked.
- Expect many questions. Answer them fully.
- **A question about a quantity is answered conceptually before it is answered numerically.** What the quantity is, what it governs, and what it is good for come first, in ordinary words. The derivation follows as a separate step, once the idea has landed. A calculation offered in place of the concept answers a question that was not asked.

## 3. Quality bar

The goal is a modification that is **as professional as it reasonably can be**: far above a quick hack, short of ready to market.

- Component choices trace back to a requirement and a datasheet.
- Schematics follow conventional practice, so another engineer could review one without explanation: sensible reference designators, net names, power symbols, decoupling, a readable sheet.
- Design decisions are written down. A **rejected alternative is recorded only where its reason is not derivable from the requirements**: an approach someone would propose again, or a trade that was judged rather than computed. A part that misses a stated figure earns no entry.
- Margins are calculated: ratings, tolerances and worst cases.
- **Every derived bound names the input extreme it was computed from.** Picking the wrong direction leaves no trace in the result. A minimum series resistor taken from V_F max instead of V_F min gives 40 Ω where the answer is 46 Ω.
- **Write the intermediate quantity on a separate line, with its extremes named.** A single division hides which values went into it, so a wrong extreme reads as correct arithmetic. The voltage across a resistor is derived first, from a named supply extreme and a named device extreme, and only then divided by the current. A bound taken at the nominal rail is not a bound.
- **A changed value invalidates every figure derived from it.** Swapping a resistor, a supply or a part re-derives every bound, margin, current and time constant that took it as an input, from the sources under rule 9. A settled fact that carries no number, such as a mode or a mechanism, re-runs every derivation that took it. The sweep across the repository follows rule 7, in one pass.
- **No pin or part is loaded past its specified figure, transients included.** A figure published for sustained operation still governs a brief peak, which is designed out. The worst case is computed from the external circuit alone, and internal current limiting that no datasheet states does not enter it.
- Reproducible: someone else could rebuild this from the repository alone.
- **An open measurement blocks only what depends on it.** An unresolved figure usually sits in a build detail. The design decision above it is settled, and the detail it defers is named. Where nothing depends on the figure, it gets no entry, since a worst case from a datasheet that the design survives settles it. A cheap part that makes the figure irrelevant beats measuring it.
- **A fact confirmed on the bench loses its caveat** in every file and is not raised again.

A shortcut taken on purpose is recorded as a known limitation.

## 4. Safety

- **Prefer low voltage.** The modification stays within SELV, at USB 5 V or a low-voltage DC supply of 24 V at most. Mains voltage needs an explicit decision, and mains-side work is called out as needing a qualified electrician.
- Safety-relevant reasoning (fusing, isolation, thermal limits, battery handling) is written into the documentation.

## 5. Git

- **Never push, under any circumstances.** No `git push`, no remote created to push to, no pull request.
- **Commit only when told to**, or where these rules allow it. Everything else stays in the working tree.
- **Commit only the named files.** `git commit` writes the whole index, including files staged from outside the session. Run `git diff --cached` first to see what is there, and name the paths with `git commit -- <path>`. Anything staged elsewhere stays uncommitted. A file staged in part is the exception, because `git commit -- <path>` commits a named file as the working tree holds it. There `git diff --cached` has to show the intended hunks and nothing staged elsewhere, and `git commit` without paths then commits the index.
- No force operations, no history rewriting, no branch deletion without an explicit request.

## 6. Privacy

- No personal information goes into commits, documentation, or anything sent to an external service.
- Do not read personal files or anything outside this repository's working directory.
- Identity in git history is whatever the local git configuration supplies; nothing further is to be added.

## 7. Keeping documents in sync

- **Fundamental structural change to the repository → update [`README.md`](README.md).**
- **Something worth knowing for future sessions → add it to [`CLAUDE.md`](CLAUDE.md).**
- **A fact referenced in a document changes → update the documentation without asking**, once the matter is *settled*. An idea still being weighed changes no document.
- **A newly stated rule → add it to this file** and commit it.
- **A design change touches every file that repeats the fact.** The subsystem document is the first file of the change. What moves together:

  | Changed | Also update |
  |---|---|
  | Any value, part, count or connector in a subsystem | **every `.svg` in that subsystem's directory** |
  | A figure a subsystem derives, or an input to one | **that subsystem's figure model**, and the check of rule 15 has to pass |
  | A Teensy pin | [`docs/pin-assignment.md`](docs/pin-assignment.md), allocation table and cost table both |
  | A part or a quantity | [`docs/parts-list.md`](docs/parts-list.md) |
  | A component's specification | its file in `docs/research/`, and its datasheet into `docs/datasheets/` |
  | A channel count or a connector width | the connector tables, and the pin rows in the figures, as geometry as well as text |

  Finish by grepping the old and the new number across the whole repository, since a value derived in one subsystem is repeated in the budgets of another. A stale value survives longest in the figures, where a wrong number still renders.
- **A document records the decision, not the choice.** No "either A or B", no "type to be selected", no "we will decide this later". A decision is made before it is written down. A rejected alternative stays, per rule 3, as rejected with the reason it lost. An unverified figure is stated as unverified with the measurement that would settle it, as a known limitation.

Each document has a scope. Detail that belongs elsewhere is moved there and linked, never duplicated:

- **`docs/parts-list.md`**: one row per part with reference designator, short name, quantity, one-line description, spec link, supplier. The spec link resolves to the part's notes in `docs/research/`, or to the manufacturer's datasheet where no notes exist. Every part is findable from the list. Specifications, datasheet figures and rationale go in the linked document.
- **Sub-directory `README.md` files**: what a repository visitor finds in the folder. A rule that binds the work goes into this file or into the `CLAUDE.md` beside what it governs, never into `docs/`.
- **`docs/research/`**: one file per component or investigation, holding the specifications and the reasoning.
- **`docs/pin-assignment.md`**: which pins are occupied, by what, and what each one locks out. The reason for an allocation belongs in the subsystem document. No reference designator appears here, so renumbering a part costs no edit in this file. A firmware document names the peripheral it drives and no pin number, so a re-allocation costs no edit there.
- **Schematic SVGs in `docs/`**: a part whose type designation depends on its package is named by its through-hole type. Passives whose designation is package-independent, such as a 220 Ω resistor or a 100 nF ceramic, carry their value only.

  Every pin of an IC carries its pin number beside its name, taken from the datasheet's pinout for the package the build uses.

  An IC of the modification is drawn with every pin of its package, in every drawing that shows it. Each pin is a separate marker and shows its connection, either a wire to its net or the gray marker of an unconnected pin. Pins on the same net are drawn one by one and joined by wires. A note that lists pins never stands in for drawing them.

  Where components must sit close to one another, as a decoupling capacitor at its supply pin, a regulator's capacitors at the regulator or a gate resistor at its gate, the diagram marks the group, the maximum distance and the reason.

## 8. Design artifacts require explicit instruction

Documentation follows reality automatically (rule 7). **Design artifacts change only on an instruction**: schematics, PCB layouts, CAD models, the firmware's source with its bench programs, the bill of materials and published Artifacts.

Changes are proposed freely and implemented only when asked. A question such as "why not X?" is answered and changes nothing. A quick demo on the bench is built from a copy outside the repository.

## 9. Re-evaluation protocol

When asked to re-evaluate, reconsider or double-check something, **re-derive it from the facts, never from memory or from earlier statements in the conversation**:

1. Re-read the actual sources: datasheets, standards, the current file contents. Do not trust an earlier summary of them, including one produced earlier in the same session.
2. Redo every calculation from scratch. Show the working and the units so it can be checked.
3. State the assumptions the result depends on.
4. **Actively attack the previous design choice**, including one made earlier by the assistant. "Is this still right?" must be able to get the answer "no".
5. Report honestly when the conclusion changes, and say what the earlier error was.

A passage that is to be rewritten is formulated from its sources again, never restored from the conversation.

Consistency with a previous answer is not evidence that the previous answer was correct.

## 10. Writing style

This rule governs documentation, code comments, commit messages and conversation alike. A fact is stated once, in the words it needs.

**A document is written for someone who reads it once and rebuilds from it.** That reader meets every term in the order the document puts them, and no passage may need a second pass across the file before it makes sense. Where brevity and comprehension collide, comprehension wins, and none of the bans below justifies a sentence that has to be decoded.

- **Full sentences.** A bold lead, a paragraph opener and the line that carries into a table have a subject and a verb. "From above, the dwell." is a label.
- **One claim to a sentence.** A sentence holding a claim, the mechanism behind it and what follows from it is split into three.
- **A term is explained where it first appears**, in ordinary words, and named after the explanation. A definition that arrives a section later has already lost the reader.
- **A name out of a datasheet or a peripheral is translated.** What the thing does is written in plain words, and the vendor's name stands beside it where the firmware needs to address it.

Banned:

- **Meta-commentary about a document.** No sentence explaining what a file is, what it is for, or what it deliberately leaves out. The structure already shows that. The sentence that prompted this rule: *"The list identifies parts; it does not describe them. Specifications and the reasoning behind a choice live in the linked documents."*
- **Antithesis used as decoration**, such as "X; it does not Y", "not A, but B", "not merely P, but Q" or "the two are not the same thing", wherever the negated half carries no information. Permitted only where the negation corrects a mistake the reader is likely to make, such as *"10 mA is a rating, not an operating point."*
- **Argument by exclusion in an explanation.** What a part is for and why a design is as it is are stated forward, from what the part does and what the design achieves. A purpose is never established by ruling out a purpose the part does not serve. The sentence that prompted this rule: *"Coil current is not among their jobs: holding 2.04 A within 0.25 V costs 8 160 µF for every millisecond of pulse."* Written forward: *"Sustained current comes from the adapter: a 0.25 V hold at 2.04 A costs 8 160 µF for every millisecond."*

  Negative reasoning belongs one level down, in the proof: a measurement that rules out one of several candidate parts, or a calculation that eliminates a value, stays.
- **A sentence that restates the heading above it.**
- **Closing paragraphs that summarize what was just written.**
- **Portentous phrasing for mundane facts**, such as "deliberately", "worth noting", "it is important to understand that" or "by design".
- **"Its own", "of its own" and "on its own".** The possessive carries nothing, and the noun alone says it: *"Each mode has a key."* Where uniqueness matters, a separate sentence states it: *"No two modes share a key."* This holds in code comments and in chat as much as in documents.
- **Teaching electronics to the reader.** A document assumes a reader who knows the fundamentals. State the fact and stop; do not add the sentence explaining why it matters or what would otherwise go wrong. The sentence that prompted this rule: *"The reference matters here, because the sensor board has no ground pin of its own."* What is banned is the added explanatory sentence. A short clause inside a sentence that names the consequence and keeps it readable stays, as in *"An open sense line has to be held at a defined level, or the bumper fires on noise."*

  A legend that defines the notation and the signal names of a document's tables is exempt. It is kept to definitions and to how the notation is read.
- **Dates.** No year, month, day or time in a document, whether for a measurement, a checked page, a decision or a source's publication. A source is identified by its document or revision number.
- **Revision narrative.** No account of what a passage said before or why it changed, and no "so far". A document reads as though its content had always stood there.
- **Personal references.** Requirements are stated impersonally: no "the maintainer", no "the user", no second person, in this file and `CLAUDE.md` as much as in documentation. The sentence that prompted this rule: *"The maintainer cannot catch a fabricated number, so it must not be produced in the first place."*
- **The dash as punctuation.** No em dash and no en dash joining clauses, glossing a value in a table cell, or setting off an aside. A comma, a colon, a full stop or a pair of parentheses carries the same structure without the visual break. The minus sign inside a formula, the hyphen inside a compound word and the dash spanning a numeric range, as in 18–20 mA, stay.
- **Inserted asides.** No clause, appositive or example set into the middle of a sentence, whether commas, parentheses or dashes set it off. The inserted fact goes into a separate sentence. The sentence that prompted this rule: *"The SD specification allows a card at most 100 ms for each block it reads and 250 ms for each block it writes, so the longest transfer, one stream chunk, takes 800 ms at most."*
- **Two clauses joined by a comma and "and".** Each clause becomes a separate sentence, or one clause gives the reason for the other. The sentence that prompted this rule: *"Playback depends on the card's read speed, and the file channel's ring buffer only bridges short delays."*

**The general statement comes first, and the detail follows it.** A paragraph, a section and a table row all open with what is being claimed and narrow from there. Leading with the mechanism and arriving at the claim at the end forces the reader to hold parts whose purpose has not been stated yet.

Prefer the table or the link over a paragraph introducing the table or the link.

**Reading flow is not meta-commentary.** A sentence that leads into a table or carries the reader to the next section stays. A paragraph of build-up before a table is one sentence too many. A document that reads as disconnected assertions has overshot the ban.

**Plain language wherever plain language works.** A point that can be made in ordinary words is made in ordinary words. Exactness is owed where something is derived, proven or recomputed, as a bound, a margin or a rating. There the wording carries the units, the extremes and the source.

The explanations rule 2 asks for, naming the failure mode, spelling out what destroys a part, belong in **conversation**, not in the repository's documents. A document's terms and the mechanism it describes are not such an explanation and are written out where they first appear. Reasoning that establishes a *finding* stays: how a value was derived, why one reading beats a contradictory one, what a measurement rules out.

## 11. Research and sourcing

Teensy documentation is thin and scattered across the web. Treat every claim as unconfirmed until sourced.

**Always search with the exact model number**, "Teensy 4.1" and never "Teensy". Generations contradict each other on points that destroy hardware. PJRC writes of the [3.2](https://www.pjrc.com/store/teensy32.html): "Teensy 3.2 pins accept 0 to 5V signals. The pins are 5V tolerant." Of the [4.1](https://www.pjrc.com/store/teensy41.html): "The pins are not 5V tolerant." A result about the wrong board looks authoritative and will wreck the part. Critical for electrical and pin-level questions, less so for software APIs.

Source ranking:

1. **pjrc.com, excluding the forum**: the primary authority. Correct unless several independent sources contradict it.
2. **PJRC forum posts by Paul Stoffregen**, the Teensy's creator and PJRC's operator: authoritative. Other regulars may be equally reliable.
3. **[TeensyUser wiki](https://github.com/TeensyUser/doc/wiki)**: community-maintained. A claim from it is corroborated before use.
4. Everything else: corroborated before use.

Cross-checking:

- Confirm a fact at more than one source wherever a second one exists.
- Compare posting dates. Newer usually wins.
- Record the source next to the figure in `docs/research/`, identified by its document or revision number where it has one.

Arduino and ESP32 material often transfers for software questions, and never for electrical or pin-level ones.

## 12. Reversibility

The stock machine's original functionality must remain restorable. An alteration is permitted where the part it touches can still do what it did before, and forbidden where it cannot.

| Alteration | Verdict |
|---|---|
| Re-soldering a stock board | Forbidden |
| Cutting or drilling a part so that its original purpose is destroyed | Forbidden |
| Drilling a new hole in the playfield or in a track | Permitted, the original function survives it |
| Cutting a cable | Permitted once a connector is soldered to each end |

The table sets the default. An alteration it forbids is permitted for a named part once that alteration is decided explicitly. The part's document records the decision. Rule 14 does not report it.

## 13. Amending these rules

New or changed rules are added here by the assistant when they are stated, in their intent rather than verbatim, and committed. Where a rule is ambiguous, the assistant records its interpretation and flags it for confirmation rather than guessing silently.

## 14. Risk levels

Every finding is reported at one of four levels. The level follows from the consequence and from the scenario the finding occurs in.

| Level | Case | Action |
|---|---|---|
| **Critical** | Normal operation, inside the system boundaries and inside the datasheet specification, destroys hardware or endangers a function of the machine. Normal operation includes power-up, boot, shutdown and any moment during a game. | Report at once and prominently. Explain the problem in plain words before the numbers, and propose an alternative in the same message. |
| **Critical** | Operation leaves the datasheet specification and no permissible, safe operation can be derived for it. | As above. |
| **Critical** | A person can be injured. This holds on the bench as much as in operation. | As above. |
| **Important** | The Critical cases, occurring while the build is on the bench, or in an abnormal or purely theoretical scenario. | As above, plus an honest assessment of how likely the case is. |
| **Important** | A part already planned changes, is added, or is dropped. Where the design provides for a choice between exactly those parts, the case is Medium instead. | Explain the change and its consequence in plain words. |
| **Medium** | A change markedly reduces the performance of a function or of another function, or markedly improves it. | Warn and name a measure. An improvement is reported the same way. |
| **Medium** | A calculation error runs through several derived figures, or moves one of them significantly. | Warn and correct. Rule 9 governs the re-derivation, rule 7 the sweep across the repository. |
| **Medium** | A different part from a documented option list becomes the preferred one. | Report prominently. |
| **Medium** | An alteration would leave a stock part unable to do what it did before (rule 12). | Warn and propose an alternative. |
| **Medium** | A statement in a main document is objectively wrong while the calculation and the schematic are right. | Warn and propose the correction. |
| **Low** | A calculation error changes the system or a function not at all or negligibly. | Correct it and report it collected. |
| **Low** | Wording in a main document is imprecise without being wrong. | Report it and propose the wording. |
| **Low** | Wording in an appendix is imprecise or wrong. | Correct it and report it collected. |

**A Critical finding is raised once the derivation is finished.** The calculation is carried to its end first, because the result can still change. Where the finding stands at the end, work stops there and waits for a decision.

**A review task is completed before anything is reported.** Findings of every level are collected and delivered with the review.

**Counterproposals are written in chat.** Rule 8 still governs the files: design artifacts change when asked. Documentation follows rule 7, and a cosmetic fix in a file already open is made in passing.

**Findings and questions are marked in chat.** The mark opens the line, and a level's mark is followed by the level's name. Findings listed together are numbered, so a reply can name them by number.

| Mark | Stands for |
|---|---|
| 🔴 | Critical |
| 🟠 | Important |
| 🟡 | Medium |
| 🔵 | Low |
| ❓ | A question the work waits on |
| 🔧 | A part or a value that changes, placed before its level or its question |

Documents and commit messages carry no marks.

**What counts as significant.** A figure that breaks a rating, an absolute maximum or a stated requirement is Critical or Important. A figure that changes a number another document repeats, or changes a part choice, is Medium. A figure that changes nothing built and nothing written down is Low.

**An unverifiable figure inherits the level of whatever depends on it**, and blocks only that (rule 3).

The severity tables of the review skills map onto these levels:

| Skill severity | Level |
|---|---|
| Destroys hardware, Malfunctions (`review-circuit`) | Critical in normal operation, Important on the bench or in a theoretical fault |
| No margin (`review-circuit`), Thin margin (`design-review`) | Important where a rating is at stake, Medium otherwise |
| Unverifiable (`review-circuit`) | The level of what depends on it |
| Below bar, Missing (`design-review`) | Medium |
| Craft (`design-review`) | Low |
| Wrong (`review-schematic-svg`) | Medium |
| Ambiguous, Convention (`review-schematic-svg`) | Low |

## 15. Figures are checked against a model

**A derived figure is computed in a model file and written into the documents from there.** Each subsystem that derives figures carries one model, and [`tools/figcheck.py`](tools/figcheck.py) checks the subsystem's document, any further document the model names and its drawings against it. The models and every other Python file the checks read live under `model/`, in folders that mirror those under `docs/`, so `docs/` holds prose and drawings only. A subsystem that starts deriving figures gets a model in its directory under `model/parts/`, and CI finds it there without being told.

**A calculation is finished when it stands in the model.** A figure worked out in conversation, or typed straight into a document, has nothing checking it. Until it is declared with its inputs, its unit and its source, it counts as unverified under rule 3, and an answer that reports it says so.

What a request touches:

| Request | Where the change goes |
|---|---|
| A value, a part, a supply, a count | the input in the model. `--write` then puts every figure that moved into the documents and the drawings, and rule 7's repo-wide sweep follows |
| A new derivation | a declaration in the model, with the directions it moves in and, where it states a bound, the side it prints on |
| Wording, structure, an explanation | the document. A figure is addressed by its value, so rewording costs nothing, and the check has to pass afterwards |
| A figure that looks wrong | a re-derivation in the model under rule 9. What the check reports is the answer |

**No figure is edited by hand in a document or in a drawing.** `--write` puts it there, so it is typed in one place, and the check reports a hand edit.

- **Every number in the document is a declared quantity.** The check reads the whole file, main body as much as appendix: a value in a fenced block or a table has to be a figure the model computes or a quantity it declares, and one that is neither fails the run as an orphan. A figure stated in prose is located by the section that holds it.
- **A formula holds no literal constant.** Only 0, 1 and 2 may stand in one, as algebra. Every other number is a declared input with a source, checked when the model loads. A factor written into a derivation, such as the ln(9) between a 10-to-90 % rise time and a time constant, appears in no provenance list and in no document.
- **Every input names its kind and its source.** `datasheet` and `graph` cite the sheet and where in it the reading sits, `measured` names the bench procedure, `assumed` and `decision` say what was assumed or what was decided. An input with no source does not enter a model.
- **A derived figure declares which way it moves.** `moves` names the direction for each input: `+key` where the figure rises with it, `-key` where it falls. The checker perturbs that input to confirm the sign. Rule 3's named extreme is recorded there, so choosing the wrong one fails a run instead of reading as correct arithmetic.
- **A reading off a plotted curve is declared with its curve.** The sheet never prints such a value, so no text search reaches it. Grouping the points of one curve gives the check instead: the shape has to hold, a reading has to sit between the points around it, and where the sheet's table covers the same condition it has to sit inside that. This is internal consistency, and the report says so.
- **A requirement is written as an invariant**, in terms of the quantities and not of the formulas: the emitter current stays under its absolute maximum, a DOUT bit is home inside the half period. An invariant holds whatever the formulas are, which is what catches a slip the arithmetic accepts.
- **A bound declares the side it is printed on.** A ceiling is rounded down and a floor is rounded up, so a reader who respects the printed figure respects the real one. Rounding a 23.571 kΩ ceiling to the nearest digit states a limit of 24 kΩ that the design does not meet, and where the digit grid is too coarse to floor without losing accuracy the figure is printed to one more digit.

**A green run means the documents agree with the model, not that the model is right.** These passes narrow that gap:

| | |
|---|---|
| `--sheets` | Looks every datasheet reading up in the PDF it cites. A reading the extractor misses is re-read by hand, and a sheet it cannot read at all is reported as unread rather than counted as passing. A found figure is weak evidence, since a subset font can map a code to the wrong glyph. |
| `--mutate` | Moves each figure's value and requires the run to report it. A figure that survives the mutation of its value is one the checker would not have caught, and fails the self-test. |
| `--blind` | Prints every quantity, its unit and its inputs, with no formula and no value. A later session derives them from the schematic and the datasheets without reading the model, and the two derivations are diffed. |

**`--blind` is for larger reviews**: a design review, a subsystem whose supply or part choice changed, a run of figures that moved together. A single changed value is re-derived under rule 9 instead.

## 16. Reuse before design

**Before a subsystem is designed, established libraries and frameworks are searched at every level of it.** The levels run from the driver through the rendering and the user interface to the assets. A level that a library covers is built on that library, or the subsystem's document records why it is not. The case that prompted this rule: the display stack was designed from the driver up as layers of pixels, and a user interface library that keeps no pixels per layer was found only afterwards.

**When workarounds pile up, the model beneath them is questioned.** A design that needs a further fix for each problem it solves is re-derived from its requirements under rule 9 before the next fix goes in. The display's palettes, its dithering and its opacity in the palette each solved a problem that the layers of pixels had created.

**An assumption that shapes a design is named as an assumption.** It constrains the design only once it is confirmed as a requirement. The display assumed a main loop that never waits for the bus, and that assumption alone ruled out drawing on demand.

## 17. Through-hole build

**The build is through-hole.** Every part is chosen, drawn, listed and computed in a through-hole package. No document, drawing or model carries an SMD variant of a part. A part that exists only in an SMD package is the exception. It sits on an adapter with through-hole pins.

## 18. Coordination between sessions

**A message from another session that coordinates shared work takes priority.** It is answered as soon as possible, so the session waiting on it is not left blocked. The step in hand is finished first and not broken off for it. The answer comes before the next step begins.
