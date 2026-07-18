---
description: Understand a codebase or section at both macro and micro level
---

For the target "$1" (file, folder, repo, or "this codebase"):

Produce a two-level analysis. Both perspectives matter — the macro tells
me where this fits in the world, the micro tells me how it works.

## MACRO VIEW (the why)

### Purpose
What problem does this code solve? Why does it exist?

### Position in System
Where does this fit in the larger system?
- What depends on it (upstream)
- What it depends on (downstream)
- How it's invoked

### Domain Concepts
What real-world things does this represent? What terminology matters?
What would I need to understand about the problem domain to read this?

### Architecture Patterns
What patterns are in use? (MVC, event-driven, layered, hexagonal, etc.)
Why these choices?

### Constraints That Shaped It
What requirements or limitations drove the design?
(Performance, security, legacy compatibility, team size, etc.)

## MICRO VIEW (the how)

### Entry Points
Where execution begins for a typical request or use case.

### Key Files and Their Roles
The 5-10 most important files, each with a one-line description.

### Core Data Structures
What shapes the data flowing through the system?

### Critical Flows
Walk through 2-3 important code paths step by step.

### Conventions and Style
- Naming patterns
- Error handling style
- Testing style
- Documentation style

### Hot Spots
Files or functions that are central, often-modified, or risky.

### Onboarding Order
If I had one hour to understand this, what would I read first?
What would I read last? What can I safely ignore initially?

## Output
- For repos or large folders: full two-level analysis
- For single files: emphasize the micro, with brief macro context
- Save to docs/walkthroughs/zoom-out-$1.md (replace slashes with dashes)

Use Mermaid diagrams where they aid understanding.
