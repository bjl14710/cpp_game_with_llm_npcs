export const meta = {
  name: 'verify-pr-mergeability',
  description: 'Exhaustively verify the 5 open PRs merge cleanly in every order, audit content preservation, and integration-test the fully merged tree',
  phases: [
    { title: 'Verify', detail: 'permutations + content audit + integration build/test' },
  ],
}

const REPO = '/Users/brandonlee/development/repos/products/HFT_Trading_Practice_Repo'
const SCRATCH = '/private/tmp/claude-501/-Users-brandonlee-development-repos-products-HFT-Trading-Practice-Repo/cf65b35f-8909-4987-b7e3-b3900eecfd2a/scratchpad/merge-fix'

const PERM_SCHEMA = {
  type: 'object',
  required: ['totalOrders', 'cleanOrders', 'conflictedOrders', 'uniqueFinalTrees', 'verdict'],
  properties: {
    totalOrders: { type: 'number' },
    cleanOrders: { type: 'number' },
    conflictedOrders: { type: 'array', items: { type: 'string' }, description: 'each entry: "order → conflicting branch/files"' },
    uniqueFinalTrees: { type: 'number', description: 'count of distinct final tree oids across all clean orders (must be 1)' },
    verdict: { enum: ['ALL_CLEAN_ONE_TREE', 'PROBLEMS'] },
  },
}

const AUDIT_SCHEMA = {
  type: 'object',
  required: ['progressMissing', 'glossaryMissing', 'verdict', 'notes'],
  properties: {
    progressMissing: { type: 'array', items: { type: 'string' }, description: 'content lines from any original PROGRESS version absent from canonical (after whitelist)' },
    glossaryMissing: { type: 'array', items: { type: 'string' } },
    verdict: { enum: ['NO_CONTENT_LOST', 'CONTENT_LOST'] },
    notes: { type: 'string' },
  },
}

const INTEG_SCHEMA = {
  type: 'object',
  required: ['mergesClean', 'cmakeBuild', 'ctest', 'pytest', 'vhdlTests', 'verdict', 'notes'],
  properties: {
    mergesClean: { type: 'boolean' },
    cmakeBuild: { type: 'string' },
    ctest: { type: 'string', description: 'e.g. "45/45 passed, 0 failed"' },
    pytest: { type: 'string', description: 'e.g. "63 passed, 2 skipped"' },
    vhdlTests: { type: 'string', description: 'the 3 VHDL parity test results specifically' },
    verdict: { enum: ['GREEN', 'RED'] },
    notes: { type: 'string' },
  },
}

phase('Verify')
const [perms, audit, integration] = await parallel([
  () => agent(`
In the git repo at ${REPO} (read-only for the repo tree — you may create objects via plumbing but must not touch any branch ref or the working tree):

Verify that the five open-PR branches merge cleanly into origin/main in EVERY possible order.
Branches (use the origin/* refs):
  origin/feature/avellaneda-stoikov-mm-sim
  origin/feature/vhdl-flat-book
  origin/feature/hy-intraday-leadlag
  origin/docs/roadmap-plans
  origin/docs/streamlit-dashboard-lesson

Write a bash or python script in ${SCRATCH}/ that, for each of the 120 permutations:
  base = rev-parse origin/main
  for each branch B in the permutation:
    tree = git merge-tree --write-tree <base> <B>   (if it exits nonzero -> record conflict for this order and move to next permutation)
    base = git commit-tree <tree> -p <base> -p <B-sha> -m "sim"
  record the final tree oid.
This creates only unreachable dangling objects — safe, no refs are written.
Report: total orders, clean count, any conflicted orders with the branch/files that conflicted (use git merge-tree's conflict output), and the number of UNIQUE final tree oids across clean orders (order-independence means exactly 1).
`, { label: 'verify:120-permutations', schema: PERM_SCHEMA }),

  () => agent(`
Content-preservation audit in ${SCRATCH} (files already exist there; repo at ${REPO} for reference).

The canonical union files are:
  ${SCRATCH}/PROGRESS.canonical.md
  ${SCRATCH}/GLOSSARY.canonical.md
The original per-branch versions are:
  ${SCRATCH}/PROGRESS.main.md, PROGRESS.mm.md, PROGRESS.vhdl.md, PROGRESS.0008.md
  ${SCRATCH}/GLOSSARY.main.md, GLOSSARY.0008.md

Task: adversarially verify NO content was lost in canonicalization. For every original file, every normalized content line (strip whitespace, ignore blank lines) must appear in the corresponding canonical file. Two intentional exceptions (whitelist — do NOT report these):
  1. The line variant "\`/teach\` the incremental best-index optimization when it lands (perf/03), or the" was deliberately replaced by main's variant without "(perf/03)" (perf/03 is now the backtest-vs-live writeup).
  2. The five-line "**Note on numbering:**" paragraph from PROGRESS.0008.md (lines mentioning "0002-0007 are in use or reserved across other unmerged branches" / "will need reconciling") was deliberately rewritten; the canonical file has a replacement Note on numbering paragraph.
Write a small python script to do the check mechanically; report every missing line NOT covered by the whitelist. Also sanity-check canonical PROGRESS structure: exactly these headers in order: 0001, 0006, 0008 under Covered; 0003, 0004, 0005, the look-ahead pending target, the A-S/queue pending target, 0007 under Backlog.
`, { label: 'verify:content-audit', schema: AUDIT_SCHEMA }),

  () => agent(`
Integration verification for the repo at ${REPO}. Work ONLY in a fresh worktree; never modify the main working tree or any branch ref.

1. cd ${REPO} && git worktree add --detach ${SCRATCH}/wt-integration origin/main
2. cd ${SCRATCH}/wt-integration and merge all five open-PR branches for real, in this order (each must be conflict-free):
   git merge --no-ff origin/feature/avellaneda-stoikov-mm-sim -m "integ 20"
   git merge --no-ff origin/feature/vhdl-flat-book -m "integ 21"
   git merge --no-ff origin/feature/hy-intraday-leadlag -m "integ 22"
   git merge --no-ff origin/docs/roadmap-plans -m "integ 23"
   git merge --no-ff origin/docs/streamlit-dashboard-lesson -m "integ 24"
   (detached HEAD, so no branch is touched). If ANY conflicts, report mergesClean=false with details and stop.
3. Build and test the fully-integrated tree:
   - export PATH="/opt/homebrew/bin:$PATH"
   - cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j4
   - cd build && ctest --output-on-failure  → capture summary (this tree has mm-sim + replay + vhdl golden_vectors all live)
   - Copy the built extension modules into the worktree's package so the Python bridge tests exercise THIS tree's C++: find build -name '*.so' and copy signals_cpp*.so and oil_volume_cpp*.so into src/oil_alpha/ of the worktree.
   - From the worktree root: ${REPO}/.venv/bin/python -m pytest -q  → capture summary. The 3 VHDL parity tests (tests/test_vhdl_flat_book.py) MUST RUN (not skip) — ghdl is at /opt/homebrew/bin/ghdl and golden_vectors will be in build/cpp/itch/. Report their results separately.
   - Expected: zero FAILURES anywhere. A few skips are acceptable ONLY if they are environment-based; report every skip reason you see.
4. Clean up: cd ${REPO} && git worktree remove --force ${SCRATCH}/wt-integration
Report via structured output; put build/test log tails in notes if anything fails.
`, { label: 'verify:integration', schema: INTEG_SCHEMA }),
])

return { perms, audit, integration }
