---
name: project-learning-apps-ingestion
description: "Future feature for the 11 leetcode-style learning apps — pipeline to ingest curriculum content from websites, classes, and Google searches"
metadata: 
  node_type: memory
  type: project
  originSessionId: 3905548e-bb82-43d7-ab2c-f91380ce7e48
---

Brandon wants a future pipeline that pulls curriculum source material into each `<topic>_learning_app/` from external sources: web pages, class transcripts/notes, Google search results. Output would feed the existing `content/{basic,intermediate,advanced}/` structure (lessons/quizzes/problems in YAML+markdown).

**Why:** Authoring real curriculum by hand for 11 topics is the bottleneck. Auto-ingestion turns "spend a day writing PyTorch lessons" into "point the ingester at a URL list." Aligns with Brandon's longer-term goal of building a "learning app creation app."

**How to apply:**
- This is **not** built yet — only a placeholder will be dropped during initial scaffolding. Likely shape: a `content/sources.yaml` per fork listing URLs / class refs / search queries, plus a `tools/ingest/README.md` at the template level describing the planned pipeline (fetch → normalize → emit YAML/markdown).
- When Brandon returns to actually build this, expect questions about: source-of-truth precedence (curated vs. ingested), how to keep cited sources fresh, and what runner to use for verification of ingested code problems.
- Related: [[project-learning-apps-curriculum]] (the curriculum-authoring follow-up this would automate).
