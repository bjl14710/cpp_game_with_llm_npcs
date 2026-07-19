---
description: Understand a codebase or section at both macro and micro level, output as a readable interactive HTML page with collapsible sections and rendered diagrams.
argument-hint: A file, folder, or "this codebase"
---

For the target "$1" (file, folder, repo, or "this codebase"):

Produce a two-level analysis as a self-contained HTML page. The macro tells
me where this fits in the world; the micro tells me how it works. Both
matter.

## Step 1 — Gather
Explore the target. Use the explorer subagent for large targets to map it
without bloating context. Read the files that matter. For anything
accuracy-sensitive (library/framework behavior), web-search to confirm.

## Step 2 — Analyze at Two Levels

### MACRO VIEW (the why)
- Purpose: what problem does this solve? Why does it exist?
- Position in system: what depends on it (upstream), what it depends on
  (downstream), how it's invoked
- Domain concepts: the real-world things this represents, terminology needed
- Architecture patterns: what patterns are in use and why
- Constraints that shaped it: requirements/limits that drove the design

### MICRO VIEW (the how)
- Entry points: where execution begins for a typical use case
- Key files and roles: the 5-10 most important, one line each
- Core data structures: what shapes the data flowing through
- Critical flows: 2-3 important code paths, step by step
- Conventions: naming, error handling, testing, docs style
- Hot spots: central, often-modified, or risky files
- Onboarding order: what to read first, last, and what to skip initially

## Step 3 — Produce the HTML
Self-contained single file saved to
`docs/walkthroughs/zoom-out-<dash-case-target>.html`. Inline CSS/JS, opens
via file://, no external dependencies EXCEPT Mermaid (see below).

Requirements:

**Readable and navigational** — clean typography, comfortable line length,
good contrast, syntax-highlighted code blocks. A sticky or top table of
contents linking to each section so I can jump around.

**Collapsible sections** — each major section (macro subsections, micro
subsections) is collapsible with a clickable header, using native <details>
/<summary> elements (no JS needed, works everywhere). Default state: MACRO
sections expanded, MICRO sections collapsed — so the page opens as a
high-level overview and I expand into detail on demand. This mirrors the
"zoom" concept: start zoomed out, zoom in where you want.

**Rendered Mermaid diagrams** — where a diagram aids understanding (data
flow, call sequence, state, type hierarchy), include it as a rendered
diagram, not raw code. Load Mermaid from a CDN and init it:

  <script type="module">
    import mermaid from 'https://cdn.jsdelivr.net/npm/mermaid@11/dist/mermaid.esm.min.mjs';
    mermaid.initialize({ startOnLoad: true });
  </script>

Put each diagram in a <pre class="mermaid">...diagram source...</pre> block.
Note: this requires internet on first open to fetch Mermaid. If offline
matters, also show the diagram source in a collapsed <details> as a fallback.

**A "macro summary" banner at the top** — 2-3 sentences capturing the whole
target, readable in 10 seconds before any expanding.

**Visual macro/micro distinction** — style macro and micro sections
differently (e.g. a colored left border or subtle background) so I always
know which altitude I'm reading at.

## Step 4 — Open It
Print the open command for their platform:
- macOS:  open docs/walkthroughs/zoom-out-<name>.html
- Linux:  xdg-open docs/walkthroughs/zoom-out-<name>.html

## Step 5 — Scope to the Target
- For a repo or large folder: full two-level analysis
- For a single file: emphasize the micro, with brief macro context
- Match depth to the target's actual complexity — don't pad a simple file
  into a fake epic, don't under-serve a genuinely complex system

## Tone
Clear and direct. This is a map someone uses to navigate unfamiliar code
fast — optimize for "I can find and understand what I need," not exhaustive
documentation.
