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

### 2. Structure

Merge two sections where one of these holds:

| Case | Example |
|---|---|
| Both describe the same mechanism at different altitudes | a section on execution and a section on the block it executes |
| One is the special case of the other | overrun handling inside the read block |
| One is two rows long and reads as a footnote of another | a reserves table under the section whose figures it qualifies |

Move a section where it answers a question that arises earlier in the file. A section that explains how a value is formed belongs to the chapter about producing values, not to the one about calibrating them.

Drop a section where its content is derivable from what already stands, and where nothing points at it.

### 3. Paragraphs

For each paragraph, ask what it gives a reader who has to rebuild this. Delete rather than rewrite where the answer is:

- It repeats the table, the list or the section above it.
- It answers a question that was asked in conversation. A figure that settled a chat question stays in chat.
- It states what a rejected approach would do, where the requirements already rule that approach out.
- It copies a datasheet line that the model already carries as a source.
- It draws no conclusion: a formula with no figure and no consequence gives the reader homework.

### 4. Sentences

Rewrite for structure, not for polish:

- The claim first, the mechanism after it.
- One claim to a sentence. A semicolon or a colon joining two claims is two sentences.
- No cleft openings: "What a ball changes is the difference" is "A ball changes the difference".
- No dangling participle, no apposition that attaches to the wrong noun.
- A term is explained where it first appears, in ordinary words, and named after the explanation.

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
