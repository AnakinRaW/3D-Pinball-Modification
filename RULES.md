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

- Explain the *why*, not just the *what*. Name the failure mode a design choice prevents.
- Never present a guess as a fact. State confidence, and separate "this is from the datasheet" from "this is my estimate."
- Cite the source for electrical values (datasheet page, standard, measurement). The maintainer cannot catch a fabricated number, so it must not be produced in the first place.
- Proactively flag anything that can destroy hardware or injure a person, even when not asked.
- Expect many questions. Questions are not a sign the explanation failed; answer them fully rather than tersely.

## 3. Quality bar

The goal is a modification that is **as professional as it reasonably can be** — explicitly *not* "ready to market," but far above a quick-and-dirty hack.

Concretely, that means:

- Designs are justified, not guessed. Component choices trace back to a requirement and a datasheet.
- Schematics follow conventional practice: sensible reference designators, net names, power symbols, decoupling, readable sheet layout — a schematic another engineer could review without explanation.
- Design decisions are written down, including the rejected alternatives and why they lost.
- Margins are deliberate. Ratings, tolerances and worst cases are calculated, not hoped for.
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

Each document has a scope and stays inside it. Detail that belongs elsewhere gets moved, not duplicated:

- **`docs/parts-list.md` identifies parts and points to their specifications; it does not reproduce them.** A row is a reference designator, a short name, a quantity, a one-line description, a **spec link** and a supplier. The spec link resolves to the part's notes in `docs/research/`, or to the manufacturer's datasheet where no such notes exist. The list is not a buyer's guide: a part must be findable from it, not merely orderable. Specifications, datasheet figures and the rationale for a choice go in the linked document.
- **Sub-directory `README.md` files tell a repository visitor what is in the folder.** Instructions aimed at the assistant belong in `CLAUDE.md` or in this file.

## 8. Design artifacts require explicit instruction

Documentation follows reality automatically (rule 7). **Design artifacts do not.** Do not create or modify without being told:

- Schematics, PCB layouts, CAD models
- Firmware source
- The bill of materials
- Published Artifacts

Proposing a change is always welcome. Making it is not, until asked.

## 9. Re-evaluation protocol

When asked to re-evaluate, reconsider or double-check something, **re-derive it from the facts — never from memory or from earlier statements in the conversation.**

That means:

1. Re-read the actual sources: datasheets, standards, the current file contents. Do not trust an earlier summary of them, including one produced earlier in the same session.
2. Redo every calculation from scratch. Show the working and the units so it can be checked.
3. State the assumptions the result depends on.
4. **Actively attack the previous design choice**, including one made earlier by the assistant. The question "is this still right?" must be capable of the answer "no."
5. Report honestly when re-evaluation changes the conclusion — and say what the earlier error was.

Consistency with a previous answer is not evidence that the previous answer was correct.

## 10. Amending these rules

New or changed rules are added here by the assistant when the maintainer states them, in the maintainer's intent rather than verbatim, and committed. Where a rule is ambiguous, the assistant records its interpretation and flags it for confirmation rather than guessing silently.
