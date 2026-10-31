export const meta = {
  name: 'fix-pr-merge-conflicts',
  description: 'Merge origin/main into #20 and #21 with canonical PROGRESS resolution, then build+test each branch',
  phases: [
    { title: 'Resolve', detail: 'merge main + canonical PROGRESS per branch' },
  ],
}

const REPO = '/Users/brandonlee/development/repos/products/HFT_Trading_Practice_Repo'
const SCRATCH = '/private/tmp/claude-501/-Users-brandonlee-development-repos-products-HFT-Trading-Practice-Repo/cf65b35f-8909-4987-b7e3-b3900eecfd2a/scratchpad/merge-fix'
const CANON = SCRATCH + '/PROGRESS.canonical.md'

const RESULT_SCHEMA = {
  type: 'object',
  required: ['branch', 'status', 'headSha', 'progressBlobSha', 'conflictFiles', 'ctest', 'pytest', 'notes'],
  properties: {
    branch: { type: 'string' },
    status: { enum: ['resolved', 'aborted', 'test-failure'] },
    headSha: { type: 'string', description: 'new HEAD sha after merge commit, or original sha if aborted' },
    progressBlobSha: { type: 'string', description: 'git hash-object of docs/learning/PROGRESS.md at HEAD' },
    conflictFiles: { type: 'array', items: { type: 'string' }, description: 'files git reported as conflicted during the merge' },
    ctest: { type: 'string', description: 'ctest summary line, e.g. "45/45 passed" or "SKIPPED: reason"' },
    pytest: { type: 'string', description: 'pytest summary line' },
    notes: { type: 'string' },
  },
}

const BRANCHES = [
  { name: 'feature/avellaneda-stoikov-mm-sim', wt: 'wt-mm', pr: 20 },
  { name: 'feature/vhdl-flat-book', wt: 'wt-vhdl', pr: 21 },
]

phase('Resolve')
const results = await parallel(BRANCHES.map(b => () => agent(`
You are fixing merge conflicts for PR #${b.pr} (branch ${b.name}) in the git repo at ${REPO}.
Work ONLY inside a dedicated worktree — NEVER touch the main working tree at ${REPO} itself, and NEVER run git checkout/reset/clean in ${REPO}.

Steps (do them exactly; abort loudly on any surprise):

1. Create the worktree (retry once after sleeping 5s if it fails on a lock):
   cd ${REPO} && git worktree add ${SCRATCH}/${b.wt} ${b.name}
2. cd ${SCRATCH}/${b.wt} and confirm: git rev-parse HEAD equals git rev-parse origin/${b.name}. If not, ABORT (remove worktree, report status=aborted).
3. Run: git merge origin/main --no-ff --no-commit
   It is EXPECTED to fail with conflicts. Collect the conflicted file list via: git diff --name-only --diff-filter=U
   The list must be EXACTLY [docs/learning/PROGRESS.md]. If any other file is conflicted, run git merge --abort, remove the worktree, and report status=aborted with the actual list.
4. Resolve: cp ${CANON} docs/learning/PROGRESS.md && git add docs/learning/PROGRESS.md
5. Sanity-check the staged merge: git diff --cached --stat should show changes consistent with a merge of main (backtest/bench/replay files arriving) — no unexpected deletions of ${b.name}'s own files. Then commit with EXACTLY this message (heredoc):

Merge origin/main into ${b.name}

Resolves docs/learning/PROGRESS.md by adopting the canonical learning-docs
union shared across the open PR stack (#20, #21, #24), so the remaining PRs
merge cleanly into main in any order. No changes beyond the merge itself and
the canonical PROGRESS resolution.

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>

6. Record: headSha = git rev-parse HEAD; progressBlobSha = git hash-object docs/learning/PROGRESS.md
7. Build and test the merged tree inside the worktree:
   - export PATH="/opt/homebrew/bin:$PATH"  (ghdl must be findable)
   - cmake -S . -B build -DCMAKE_BUILD_TYPE=Release  && cmake --build build -j4  (log to files, show tail on failure)
   - cd build && ctest --output-on-failure  → capture the "N tests passed" summary
   - Build the Python extension so pytest's C++-bridge tests run against THIS tree: from the worktree root run bash scripts/build_cpp.sh if it exists and works in-place (read it first to understand what it does); if it is not suitable, fall back to: the compiled signals_cpp/oil_volume_cpp modules may be produced by the cmake build already — find them (find build -name '*.so') and copy them into src/oil_alpha/ of the WORKTREE (never the main repo).
   - Run the full Python suite with the main repo's venv interpreter but this worktree's code: cd <worktree> && ${REPO}/.venv/bin/python -m pytest -q  → capture the summary line.
   - Some skips are expected (tests for sibling PRs' features not on this branch). FAILURES are not — if pytest or ctest FAILS, report status=test-failure with details in notes; do NOT amend or retry-fix code.
8. Clean up the worktree REGISTRATION but keep the branch commit: cd ${REPO} && git worktree remove --force ${SCRATCH}/${b.wt}
   (the merge commit lives on the branch ref, so removing the worktree is safe)
9. DO NOT push anything. Report via structured output.
`, { label: `resolve:${b.wt}`, schema: RESULT_SCHEMA })))

return { results: results.filter(Boolean) }