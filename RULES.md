# Project Rules

The working agreement for this repository. This file is the authority; it is committed so it is shared with every session and every collaborator.

## 1. Language

- **Repository language is English.** All code, comments, commit messages, documentation, schematics, labels and file names are in English.
- **Conversation may be German or English.** Follow the language the question is asked in.

## 2. Who this project is for

The maintainer is a **software developer, not an electrician or electrical engineer**. Assume:

- Little experience reading or judging electrical schematics; no feel for what a good, professional schematic looks like.
- Little experience with electrical safety
- No intuition for what will destroy a component (over-voltage, reversed polarity, missing current limiting, floating inputs, inrush, ESD, thermal limits).
- Strong software-engineering intuition — analogies to software concepts land well; hardware conventions do not.

**Therefore:**

- Name the failure mode a design choice prevents.
- Never present a guess as a fact. State confidence, and separate "this is from the datasheet" from "this is my estimate."
- Cite the source for electrical values (datasheet page, standard, measurement). The maintainer cannot catch a fabricated number, so it must not be produced in the first place.
- Proactively flag anything that can destroy hardware or injure a person, even when not asked.
- Expect many questions. Answer them fully.

## 3. Quality bar

The goal is a modification that is **as professional as it reasonably can be** — explicitly *not* "ready to market," but far above a quick-and-dirty hack.

Concretely, that means:

- Component choices trace back to a requirement and a datasheet.
- Schematics follow conventional practice: sensible reference designators, net names, power symbols, decoupling, readable sheet layout — a schematic another engineer could review without explanation.
- Design decisions are written down, including the rejected alternatives and why they lost.
- Margins are calculated — ratings, tolerances and worst cases.
- Reproducible: someone else could rebuild this from the repository alone.

When a shortcut is taken deliberately, record it as a known limitation rather than leaving it silent.

## 4. Safety

- **Prefer low voltage.** Unless explicitly decided otherwise, the modification stays within SELV (USB 5 V or a low-voltage DC supply, ≤ 24 V). Anything touching mains voltage requires an explicit, deliberate decision — it is not to be introduced casually.
- Mains-side work, if ever in scope, is called out as requiring a qualified electrician.
- Safety-relevant reasoning (fusing, isolation, thermal limits, battery handling) is written into the documentation, not just discussed.

## 5. Git

- **Never push. Under no circumstances.** No `git push`, no remote creation with intent to push, no PR creation.
- **Commit only when explicitly told to** — or these rules define scenrios give allowance.
- Everything else stays in the working tree until the maintainer asks for a commit.
- No force operations, no history rewriting, no branch deletion without an explicit request.

## 6. Privacy

- Do not include any personal information about the users of this repository in commits, documentation, or anything sent to an external service.
- Do not read personal files or anything outside this repository's working directory.
- The users's identity in git history is their own configuration; nothing further is to be added.

## 7. Keeping documents in sync

- **Fundamental structural change to the repository → update [`README.md`](README.md).**
- **Something worth knowing for future sessions → add it to [`CLAUDE.md`](CLAUDE.md).**
- **A fact referenced in a document changes → update the documentation without asking** — but only once the matter is *settled*. Do not churn documentation over in-progress discussion or an idea still being weighed.
- **New rules defined by the maintainer → add them to this file** and commit it.

Each document has a scope. Detail that belongs elsewhere is moved there and linked, never duplicated:

- **`docs/parts-list.md`** — one row per part: reference designator, short name, quantity, one-line description, spec link, supplier. The spec link resolves to the part's notes in `docs/research/`, or to the manufacturer's datasheet where no notes exist yet. Every part must be findable from the list. Specifications, datasheet figures and rationale go in the linked document.
- **Sub-directory `README.md` files** — what a repository visitor finds in the folder. Instructions aimed at the assistant belong in `CLAUDE.md` or in this file.
- **`docs/research/`** — one file per component or investigation, holding the specifications and the reasoning.

## 8. Design artifacts require explicit instruction

Documentation follows reality automatically (rule 7). **Design artifacts do not.** Do not create or modify without being told:

- Schematics, PCB layouts, CAD models
- Firmware source
- The bill of materials
- Published Artifacts

Propose changes freely. Implement only when asked.

## 9. Re-evaluation protocol

When asked to re-evaluate, reconsider or double-check something, **re-derive it from the facts — never from memory or from earlier statements in the conversation.**

That means:

1. Re-read the actual sources: datasheets, standards, the current file contents. Do not trust an earlier summary of them, including one produced earlier in the same session.
2. Redo every calculation from scratch. Show the working and the units so it can be checked.
3. State the assumptions the result depends on.
4. **Actively attack the previous design choice**, including one made earlier by the assistant. The question "is this still right?" must be capable of the answer "no."
5. Report honestly when re-evaluation changes the conclusion — and say what the earlier error was.

Consistency with a previous answer is not evidence that the previous answer was correct.

## 10. Writing style

Applies to documentation, commit messages and conversation alike. Say a thing once and move on.

Banned:

- **Meta-commentary about a document.** No sentence explaining what a file is, what it is for, or what it deliberately leaves out. The structure already shows that. The sentence that prompted this rule: *"The list identifies parts; it does not describe them. Specifications and the reasoning behind a choice live in the linked documents."*
- **Antithesis used as decoration** — "X; it does not Y", "not A, but B", "not merely P, but Q", "the two are not the same thing" — wherever the negated half carries no information. Permitted only where the negation corrects a mistake the reader is likely to make, such as *"10 mA is a rating, not an operating point."*
- **A sentence that restates the heading above it.**
- **Closing paragraphs that summarise what was just written.**
- **Portentous phrasing for mundane facts** — "deliberately", "worth noting", "it is important to understand that", "by design".

Prefer the table or the link over a paragraph introducing the table or the link.

## 11. Research and sourcing

Teensy documentation is thin and scattered across the web. Treat every claim as unconfirmed until sourced.

**Always search with the exact model number** — "Teensy 4.1", never "Teensy". Generations contradict each other on points that destroy hardware. PJRC writes of the [3.2](https://www.pjrc.com/store/teensy32.html): "Teensy 3.2 pins accept 0 to 5V signals. The pins are 5V tolerant." Of the [4.1](https://www.pjrc.com/store/teensy41.html): "The pins are not 5V tolerant." A result about the wrong board looks authoritative and will wreck the part. Critical for electrical and pin-level questions, less so for software APIs.

Source ranking:

1. **pjrc.com, excluding the forum** — primary authority. Correct unless several independent sources contradict it.
2. **PJRC forum posts by Paul Stoffregen** — creator of the Teensy and operator of PJRC. Authoritative. Other regulars may be equally reliable; none are identified yet.
3. **[TeensyUser wiki](https://github.com/TeensyUser/doc/wiki)** — community-maintained, upkeep unknown. Worth checking, unconfirmed until corroborated.
4. Everything else — corroborate before use.

Cross-checking:

- Confirm a fact at more than one source wherever a second one exists.
- Compare posting dates. Newer usually wins.
- Record the source and its date next to the figure in `docs/research/`.

Arduino and ESP32 material often transfers for software questions, and never for electrical or pin-level ones.

## 12. Amending these rules

New or changed rules are added here by the assistant when the maintainer states them, in the maintainer's intent rather than verbatim, and committed. Where a rule is ambiguous, the assistant records its interpretation and flags it for confirmation rather than guessing silently.
