---
description: Turn brief ticket descriptions into JIRA-ready Markdown files grounded in the actual codebase
argument-hint: "<brief 1; brief 2; ...>  OR  @path/to/briefs.md"
allowed-tools: Read, Grep, Glob, Write, Bash(mkdir:*)
---

# /make-tickets — briefs → JIRA-ready ticket files

Your job: for each ticket brief I give you, produce one Markdown file under `tickets/`
that I can paste straight into JIRA. Each file is grounded in the real code but
describes it in **abstracted** terms — no code dumps.

## 1. Read the briefs
Briefs: $ARGUMENTS

- If `$ARGUMENTS` is empty, ask me for the briefs and stop.
- If it points to a file (starts with `@` or looks like a path), read that file.
  Each line / list item / blank-line-separated block is one ticket.
- Otherwise treat each `;`-separated or newline-separated chunk as one ticket.

## 2. Ground each brief in the code (before writing anything)
- First lean on the context you've **already built this session** — files you've read,
  changes you've made, decisions we've discussed. Don't re-derive what you already know.
- Then, per brief, locate the specific code it touches: `Glob`/`Grep` for the relevant
  modules, files, functions, classes, configs, tests, build targets.
- Read only enough to describe the work accurately.
- **Never invent paths or symbols.** If the brief implies code you can't find, note it
  under **Open questions** and keep the reference generic.

## 3. Write one file per ticket: `tickets/NNN-<slug>.md`
- `mkdir -p tickets` first.
- `NNN` = zero-padded sequence (001, 002, …), continuing from any files already in `tickets/`.
- `<slug>` = short kebab-case derived from the title.

Each file uses exactly this structure:

```
# <Imperative, specific title>

**Type:** Story | Task | Bug | Spike      <!-- inferred from the brief -->
**Complexity:** S | M | L                 <!-- rough, from scope of code touched -->

## Description
2–5 sentences: what changes and why. Implementation-aware but readable by
someone who hasn't opened the code.

## Affected areas
An abstracted map of the code involved — modules, components, seams, data flow.
Reference symbols/files inline only (e.g. `auth/session.py → refresh_token()`),
never as pasted code.

## Proposed approach
Conceptual technical steps as bullets. Name integration points and anything that
must change in lockstep. No code blocks.

## Acceptance criteria
- [ ] Concrete, checkable outcomes
- [ ] Include test / verification expectations

## Open questions & assumptions
Anything ambiguous in the brief, or code you expected but couldn't locate.
```

## 4. Rules
- **Abstract, don't transcribe.** Name components and behavior; reference symbol/file
  names inline at most. No code blocks, no copied snippets — this is what keeps the
  ticket clean to paste into JIRA.
- One file per brief. Only split if a brief clearly contains two unrelated changes —
  if so, flag it and ask before splitting.
- Titles imperative and specific ("Cache SCPI device responses", not "Caching").
- Phrasing should stand on its own in JIRA without the reader having the repo open.

## 5. Finish
After writing the files, print a short table: `file path · title · type · complexity`
so I can scan everything that was created.
