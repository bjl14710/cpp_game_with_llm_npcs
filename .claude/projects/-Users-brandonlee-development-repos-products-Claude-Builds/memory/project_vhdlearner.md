---
name: VHDLearner Project
description: Multi-platform VHDL learning app — web, desktop, mobile, Docker
type: project
originSessionId: 72f25cdf-8053-4a0c-8f45-12255cf9931b
---
Created vhdlearner/ folder in Claude_Builds repo with a comprehensive VHDL learning platform.

**Why:** Brandon wants to relearn VHDL from foundations (Phase 00: basic gates) through Phase 05 (synthesizing hardware neural networks on FPGAs). No board needed — everything simulates in EDA Playground or GHDL.

**What was built:**
- `web/` — React + TypeScript + Vite + Tailwind + Monaco Editor + Zustand. Full curriculum with 6 phases, 20+ lessons, quizzes, code challenges. 30/30 unit tests pass.
- `desktop/` — Electron wrapper for Windows/Mac/Linux native apps
- `mobile/` — Expo (React Native) for iPhone. Links to EDA Playground for code execution.
- `docker/` — Multi-stage Dockerfile (Node builder → Nginx), docker-compose. Tested and serving HTTP 200 at localhost:8080.
- `installers/` — Build scripts + Dockerfiles for cross-platform builds. Linux AppImage (104MB) confirmed built via Docker.
- `docs/` — UserGuide.md, DeveloperGuide.md, INSTRUCTIONS.md, generate_docs.py. Word docs (.docx) generated with python-docx.

**How to apply:** When continuing work on this project, Node.js is NOT installed on the machine. Use Docker for testing or have user run setup.sh to install via nvm first. The Docker web app is the primary tested path.

**Tech stack:** React 18 + TypeScript strict + Vite + Tailwind + Zustand + Monaco Editor + Vitest + Playwright
