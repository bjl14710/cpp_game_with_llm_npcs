---
description: Reads review comments from a GitHub draft PR and applies the requested changes. Run after you have reviewed a PR and left comments. Respects must/should/could severity and never merges — leaves an updated draft for your final review.
argument-hint: PR number e.g. "42" (blank = detects the open PR for the current branch)
---

Read .claude/skills/github/SKILL.md before doing anything.
Read .claude/skills/code-economy/SKILL.md before making any changes.

You are applying review feedback from a GitHub PR to the code.
The human has already reviewed the PR and left comments.
Your job is to read those comments and fix what they asked for.

You do NOT merge. You push the fixes and leave the PR as a draft
for the human to do a final review pass.

---

## STEP 1 — FIND THE PR

```python
gh = get_github_client()
repo = get_repo(gh)

# If PR number given, use it directly
pr_number = "$1" if "$1" else None

if not pr_number:
    # Detect from current branch
    import subprocess
    branch = subprocess.run(
        ["git", "branch", "--show-current"],
        capture_output=True, text=True
    ).stdout.strip()

    prs = list(repo.get_pulls(state="open", head=branch))
    if not prs:
        print(f"No open PR found for branch '{branch}'")
        print("Usage: /address-review-comments 42")
        exit(1)
    pr = prs[0]
else:
    pr = repo.get_pull(int(pr_number))

print(f"PR #{pr.number}: {pr.title}")
print(f"Branch: {pr.head.ref}")
print(f"URL: {pr.html_url}")
```

---

## STEP 2 — READ ALL REVIEW COMMENTS

```python
# Get inline code review comments (on specific lines)
review_comments = list(pr.get_review_comments())

# Get general PR comments (on the conversation tab)
issue_comments = list(pr.get_issue_comments())

print(f"\nInline review comments: {len(review_comments)}")
print(f"Conversation comments:  {len(issue_comments)}")
```

For each inline comment, capture:
- The file it's on
- The line number
- The comment text
- Who wrote it (to distinguish your comments from Claude's own comments)
- Whether it's a thread with a reply (already addressed = skip)

```python
your_username = os.environ.get("GITHUB_USERNAME", "")

pending = []
for c in review_comments:
    # Skip if already resolved or replied to by Claude
    if c.user.login != your_username:
        continue  # only act on your comments, not bot comments
    pending.append({
        "file":    c.path,
        "line":    c.position,
        "comment": c.body,
        "url":     c.html_url
    })

for c in issue_comments:
    if c.user.login != your_username:
        continue
    # Skip the overnight session's own status comments
    if any(phrase in c.body.lower() for phrase in
           ["overnight session", "starting implementation",
            "draft pr", "implemented"]):
        continue
    pending.append({
        "file":    None,  # general comment, not file-specific
        "line":    None,
        "comment": c.body,
        "url":     c.html_url
    })

print(f"\nYour comments to address: {len(pending)}")
for item in pending:
    loc = f"{item['file']}:{item['line']}" if item['file'] else "general"
    print(f"  [{loc}] {item['comment'][:80]}")
```

If no pending comments from you — tell the user and stop:
```
No unresolved review comments from you found on PR #N.
If you have left comments, make sure GITHUB_USERNAME matches
your GitHub login exactly.
```

---

## STEP 3 — CLASSIFY EACH COMMENT

For each comment, classify severity:

**Must fix** (blocking — address before this PR can merge):
- Signals: "must", "needs to", "wrong", "bug", "broken", "incorrect",
  "never", "always", "this will", "fix this", "crash", explicit question
  asking why something was done a certain way

**Should fix** (strong recommendation):
- Signals: "should", "prefer", "better to", "consider", "I'd", "why not"

**Could fix** (optional suggestion):
- Signals: "could", "might", "nit:", "minor:", "optional", "if you want"

Print the classification so the human can see what will be addressed:
```
MUST FIX (3):
  src/devices/psu.py:42 — "This will crash if channel is None"
  src/devices/psu.py:88 — "Wrong error code — should be -113 not -114"
  tests/test_psu.py:15  — "Missing test for *RST on channel 2"

SHOULD FIX (2):
  src/devices/psu.py:55 — "Should use MAX_VOLTAGE constant not 30.0"
  src/gui/psu_block.py:20 — "I'd rename this to make_channel_label"

COULD FIX (1):
  src/devices/psu.py:100 — "Nit: could combine these two lines"
```

**Default: address MUST and SHOULD. Skip COULD unless simple.**
If a COULD fix is a one-liner change, apply it anyway.

---

## STEP 4 — CHECKOUT THE BRANCH

```bash
git checkout feature/issue-[N]-[slug]
git pull origin feature/issue-[N]-[slug]
```

Verify you are on the right branch:
```bash
git branch --show-current
```

---

## STEP 5 — APPLY THE FIXES

Read each file that has comments before touching it.

For each MUST and SHOULD fix:
- Read the file and understand the full context around the comment
- Make the minimal change that addresses the comment
- Apply the code-economy skill — fix what was asked, don't refactor more
- Leave a brief inline comment in the code if the fix is non-obvious:
  ```python
  # Fixed per PR review: use -113 (undefined header) not -114
  ```

For COULD fixes that are one-liners: apply them.
For COULD fixes that require significant change: skip and note it.

If a comment is ambiguous — don't guess. Leave a reply on the comment
asking for clarification, skip that fix, and note it in the summary.

---

## STEP 6 — VERIFY TESTS STILL PASS

Run the full test suite after applying fixes:

```bash
# Read CLAUDE.md for the correct test commands
cat CLAUDE.md | grep -A5 "Test Commands"
```

Run whatever the project's test command is. If tests fail:
- Fix the test failures if they are directly caused by your changes
- If a test was already failing before your changes, note it and skip

Do NOT commit if tests are failing unless the failure pre-dates this PR
and you can demonstrate that with git.

---

## STEP 7 — COMMIT AND PUSH

```bash
git add [only the files you changed]
git diff --cached --name-only  # verify — no docs/learning, no OVERNIGHT_REPORT

git commit -m "review(#[PR]): address review comments

- [what was fixed from must-fix comments]
- [what was fixed from should-fix comments]
- Skipped: [any could-fix items not addressed]"

git push origin [branch-name]
```

The push automatically updates the open PR — no need to re-create it.

---

## STEP 8 — REPLY TO COMMENTS

After pushing, reply to each addressed comment on GitHub:

```python
for comment in addressed_must_and_should:
    pr.get_review_comment(comment["id"]).reply("Fixed in latest commit.")

for comment in skipped_could:
    pr.get_review_comment(comment["id"]).reply(
        "Noted — left as-is for now to keep scope tight. "
        "Can address in a follow-up if needed."
    )

for comment in ambiguous:
    pr.get_review_comment(comment["id"]).reply(
        "Could you clarify what you had in mind here? "
        "Skipped this one until I understand the intent."
    )
```

Add a summary comment to the PR conversation:

```python
summary = f"""Review comments addressed.

**Fixed ({len(addressed)} items):**
{chr(10).join(f'- {c["comment"][:60]}' for c in addressed)}

**Skipped — optional suggestions ({len(skipped)} items):**
{chr(10).join(f'- {c["comment"][:60]}' for c in skipped) if skipped else 'None'}

**Needs clarification ({len(ambiguous)} items):**
{chr(10).join(f'- {c["comment"][:60]}' for c in ambiguous) if ambiguous else 'None'}

Ready for another look."""

pr.create_issue_comment(summary)
print(f"\nSummary comment posted to PR #{pr.number}")
```

---

## STEP 9 — FINAL OUTPUT

Print a clean summary:

```
✅ PR #[N] updated: [N] comments addressed

Fixed:
  [file:line] [comment summary]

Skipped (optional):
  [file:line] [comment summary]

Needs your input:
  [file:line] [what was unclear]

Next: review the updated PR at [PR URL]
gh pr diff [N]
```

---

## WHAT THIS COMMAND DOES NOT DO

- Does not merge the PR (you do that after final review)
- Does not address comments from other reviewers without your confirmation
- Does not make changes beyond what the comments asked for
- Does not re-open closed or resolved comments
- Does not touch docs/learning/ or OVERNIGHT_REPORT.md
