---
name: Session 1 — Large Extension Task (2026-05-10)
description: All deliverables requested in the large refactor/extension session
type: project
originSessionId: b67514e4-0682-4237-9169-ed2a74e3ea2e
---
Deliverables from the 2026-05-10 session (Claude_Fix_Ups_and_Documentations branch):

**Completed**:
- Memory setup

**In progress / planned**:
1. refresh_tickers.py — dynamic top-N ticker fetching (default 1000) replacing static text files
2. Unit tests — Python (pytest in tests/) and C++ (tests/cpp/)
3. Doxygen comments — all .cpp files + Doxyfile
4. Docker — Dockerfile, docker-compose.yml, .dockerignore
5. FPGA/VHDL — fpga/ directory with .vhd files for each C++ algorithm + README + TCL for Vivado HLS
6. iOS app — Expo/React Native project in ios_app/
7. Web landing page — web/ with index.html, styles.css, app.js for future domain hosting
8. Documentation — docs/README_NEWS.md, docs/DEVELOPER_GUIDE.md
9. .docx files — docs/DEVELOPER_GUIDE.docx, docs/USER_GUIDE.docx (generated via python-docx)
10. TODO.md — what to do when buying a domain (hosting, password protection, etc.)

**Why:** User wants to expand from research tool to multi-platform product (iOS, web, FPGA). Also needs docs for public GitHub repo and eventual commercial release.
**How to apply:** All work goes on Claude_Fix_Ups_and_Documentations branch. Commit frequently. python-docx is available.
