---
name: revise-document
description: Use when a document in this repository has to be shortened, tightened or made readable - works structure first, then paragraphs, then sentences, deletes what repeats rather than rewriting it, and accounts for everything the edit removed. Trigger on "kürz das", "shorten this document", "go through the doc", "make this readable", "optimise this section", or when a review of prose rather than of figures is asked for.
---

# Revising a document

Two failures matter: the reader has to read a passage twice before it makes sense, and the same fact stands in three places. Both are found by reading the file as a stranger, which is the work, not the reporting.

The document's own rules stay in force: `RULES.md` rule 10 for style, rule 7 for what belongs in which file, rule 14 for how a finding is reported, rule 15 for figures. This skill is the order of work, not a second style guide.

## Order of work

Structure, then paragraphs, then sentences. Nothing is polished that may be deleted as a whole.

### 1. Read it as a stranger

One pass through the file, marking three things. This pass belongs here, not to the reader of the pull request.

- Every term used before it is explained: a peripheral name, a datasheet term, a symbol out of a formula block.
- Every sentence carrying more than one claim, and every bold lead that is a label rather than a sentence.
- Every pronoun or stand-in without a referent in reach: "the result", "that value", "the two".

### 2. Build the inventory

Nothing else in this skill works without knowing what already stands where. The inventory is built at the start of a run and carried through it, and it is derived rather than stored: it is rebuilt each time, so it cannot go stale.

One row per claim the document makes, keyed by the anchors that can be found again mechanically:

| Column | Content |
|---|---|
| Anchor | The symbol, figure, part or peripheral the claim is about: `t_ovh`, 17.78 µs, MCP3008, FlexPWM3.1 |
| Claim | What is asserted about it, in a handful of words |
| Where | File and section, and the line |

The anchors come out of the file itself. `grep -n` over the symbols in the formula blocks, over every figure the model declares, and over the part and peripheral names finds every place a claim can hide, including tables, legends and note boxes.

**The inventory spans the repository, not the file.** The same fact stands in the subsystem document, the pin table, the parts list and the model, and rule 7 gives each of those a different scope. Before a passage is kept, its anchor is grepped across `docs/` and `firmware/`, not only in the file being revised.

**Then, for every passage, look its anchor up.** Where the claim is already in the inventory:

| Case | What happens |
|---|---|
| The second place adds a figure, a consequence or a condition the first does not | Both stay. The second says only what is new and points at the first |
| The second place is a legend, a table cell or a note box that has to stand alone | Both stay. A table that cannot be read without scrolling back has failed |
| The two places belong to documents of different scope under rule 7 | Both stay, one carrying the fact and the other the rationale, with a link between them |
| The two say the same thing in the same scope | One goes. The one that stays is the one whose section the reader reaches first, or the one rule 7 names as the owner |
| The two are each half of the claim | They merge, and the merge decides where the whole claim lives |

A passage that survives this is worth its sentences. A passage that repeats a claim without adding to it is deleted rather than rewritten, however well it reads.

### 3. Structure

Merge two sections where one of these holds:

| Case | Example |
|---|---|
| Both describe the same mechanism at different altitudes | a section on execution and a section on the block it executes |
| One is the special case of the other | overrun handling inside the read block |
| One is two rows long and reads as a footnote of another | a reserves table under the section whose figures it qualifies |

Move a section where it answers a question that arises earlier in the file. A section that explains how a value is formed belongs to the chapter about producing values, not to the one about calibrating them.

Drop a section where its content is derivable from what already stands, and where nothing points at it.

### 4. Paragraphs

**Every paragraph, without exception, is put to one question: what is this supposed to tell a person reading it?** The answer decides what happens to it, and the default is deletion, not rewriting.

**The answers are written out before anything is edited**, one line per paragraph, numbered, in a scratch file. A question answered in the head is skipped in the head: the paragraph that prompted this rule survived a pass that claimed to apply the test. The written answer also exposes the duplication inside a paragraph, which the repository-wide inventory of step 2 cannot see, because three sentences about one anchor in one paragraph are the same fault one level down.

| Answer | What happens |
|---|---|
| Nothing | The whole paragraph goes |
| One thing, and one sentence carries it | That sentence stays, everything else in the paragraph goes |
| One thing, said in three formulations | One formulation stays |
| Every sentence carries a claim of its own | The paragraph survives whole |

The chip-select paragraph that prompted this rule said "the select is framed per conversion", then "the transfer list is split per conversion, with the select toggled between them", then "`SPI.transfer` leaves the select to the caller around each transfer". Three sentences, one instruction. What it owed the reader was the instruction, the failure it prevents and the reason the library allows it.

The answer is "nothing" where:

- It repeats the table, the list or the section above it, or anything the inventory already holds.
- It answers a question that was asked in conversation. A figure that settled a chat question stays in chat.
- It states what a rejected approach would do, where the requirements already rule that approach out.
- It copies a datasheet line that the model already carries as a source.
- It draws no conclusion: a formula with no figure and no consequence gives the reader homework.

### 5. Sentences

Rewrite for structure, not for polish:

- The claim first, the mechanism after it.
- One claim to a sentence. A semicolon or a colon joining two claims is two sentences.
- A sentence carrying "not" or "rather than" is checked against the file's own history. Where the negated half is what an earlier draft said, or what the conversation just corrected, it is revision narrative and it goes. Rule 10 allows the negation only for a mistake the reader would make unprompted, which is not the same as the mistake the writer made a minute ago.
- No cleft openings: "What a ball changes is the difference" is "A ball changes the difference".
- No dangling participle, no apposition that attaches to the wrong noun.
- A term is explained where it first appears, in ordinary words, and named after the explanation.
- A hardware constraint is stated as the shape it forces on the code, not as the mechanism behind it. "Every conversion runs as its own transfer" lands where "the chip select is toggled per conversion" does not, and the mechanism follows in the same sentence as the reason.

## After the edit

**Account for what left.** `git diff` the file and read the removed lines one by one. Each is either duplicated elsewhere, and where, or deliberately gone. A fact that silently disappeared in a restructure is the failure this step exists for.

**Chase the references.** After any heading that changed or moved, grep the repository for the old heading and for cross-references such as "below" and "above".

**Run the checks that apply.** `python tools/figcheck.py docs/parts/<subsystem>/figures.py` after any change to a document a model governs, and `python tools/svgcheck.py` after a drawing moved. Report a green run only where a figure, a formula or a drawing was touched: a green figure check says nothing about prose, and every prose defect passes it.

## Reporting

One pass, one message. Not a sentence per message, and not a question per paragraph.

| Part | Content |
|---|---|
| Findings | Anything wrong rather than merely long, with its level from rule 14, before the list of cuts |
| Structure | What was merged, moved or dropped, and why |
| Cuts | What was deleted, one line each, with the place the content already stood |

Questions to the reader go in the first line, only where the work cannot continue without an answer.

## While editing

The reader edits the same file in parallel. Read the target region immediately before replacing it, in the same step, never from what the file held two edits ago. An edit that fails to match is a signal that the file moved, not a reason to weaken the match.

When a passage is to be written again from scratch, open the sources it rests on and write from them. Text pulled back out of the conversation is not a rewrite.
