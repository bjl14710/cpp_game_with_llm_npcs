---
description: Reads review comments from a GitLab Merge Request and applies the requested changes. Run after you have reviewed an MR and left comments. Never merges — leaves an updated draft for your final review.
argument-hint: MR number e.g. "42" (blank = detects open MR for current branch)
---

Read .claude/skills/gitlab/SKILL-gitlab.md before doing anything.
Read .claude/skills/code-economy/SKILL.md before making changes.

You are applying review feedback from a GitLab Merge Request.
The human has reviewed the MR and left discussion comments.
Your job: read those comments, fix what was asked, push, reply.

You do NOT merge. You push fixes and leave the MR as a draft
for the human to do a final review pass.

---

## STEP 1 — FIND THE MR

```bash
# If MR number given, use it
MR_NUMBER="$1"

# Otherwise detect from current branch
if [[ -z "$MR_NUMBER" ]]; then
    BRANCH=$(git branch --show-current)
    MR_NUMBER=$(glab mr list --source-branch "$BRANCH" --output json \
        | python3 -c "import sys,json; mrs=json.load(sys.stdin); \
          print(mrs[0]['iid'] if mrs else '')" 2>/dev/null)

    if [[ -z "$MR_NUMBER" ]]; then
        echo "No open MR found for branch '$BRANCH'"
        echo "Usage: /address-review-comments-gitlab 42"
        exit 1
    fi
fi

echo "MR !$MR_NUMBER"
glab mr view "$MR_NUMBER" --output json | \
    python3 -c "import sys,json; m=json.load(sys.stdin); \
    print(m['title'], '\n', m['web_url'])"
```

---

## STEP 2 — READ ALL DISCUSSION THREADS

```bash
# Get all discussion threads (inline review comments)
DISCUSSIONS=$(glab api "projects/:fullpath/merge_requests/$MR_NUMBER/discussions" \
    --paginate 2>/dev/null)

# Get general MR notes (conversation tab comments)
NOTES=$(glab api "projects/:fullpath/merge_requests/$MR_NUMBER/notes" \
    --paginate 2>/dev/null)
```

Parse discussions to find unresolved threads from you:

```python
import json, os, subprocess

username = os.environ.get("GITLAB_USERNAME", "")
discussions = json.loads(DISCUSSIONS)
notes = json.loads(NOTES)

pending = []

# Inline discussion threads (code review comments)
for disc in discussions:
    if disc.get("resolved"):
        continue  # already resolved — skip
    for note in disc.get("notes", []):
        if note["author"]["username"] != username:
            continue
        if note.get("system"):
            continue  # system messages, not human comments
        pending.append({
            "type":          "discussion",
            "discussion_id": disc["id"],
            "note_id":       note["id"],
            "file":          note.get("position", {}).get("new_path"),
            "line":          note.get("position", {}).get("new_line"),
            "comment":       note["body"],
        })

# General MR comments
for note in notes:
    if note["author"]["username"] != username:
        continue
    if note.get("system"):
        continue
    # Skip Claude's own status updates
    skip_phrases = ["starting implementation", "overnight session",
                    "implemented", "draft mr", "blocked overnight"]
    if any(p in note["body"].lower() for p in skip_phrases):
        continue
    pending.append({
        "type":    "note",
        "note_id": note["id"],
        "file":    None,
        "line":    None,
        "comment": note["body"],
    })

print(f"Your unresolved comments: {len(pending)}")
for item in pending:
    loc = f"{item['file']}:{item['line']}" if item['file'] else "general"
    print(f"  [{loc}] {item['comment'][:80]}")
```

If no pending comments — tell the user and stop.

---

## STEP 3 — CLASSIFY EACH COMMENT

Must fix (blocking):
- Keywords: "must", "needs to", "wrong", "bug", "broken",
  "incorrect", "this will crash", "fix this", explicit question

Should fix (strong recommendation):
- Keywords: "should", "prefer", "better to", "consider",
  "I'd", "why not"

Could fix (optional):
- Keywords: "could", "might", "nit:", "minor:", "optional"

Print classification before touching any code:
```
MUST FIX (N):
  src/devices/psu.py:42 — "This will crash if channel is None"

SHOULD FIX (N):
  src/devices/psu.py:55 — "Should use MAX_VOLTAGE constant"

COULD FIX (N) — one-liners only:
  src/devices/psu.py:100 — "Nit: could combine these lines"
```

Address MUST and SHOULD. COULD only if it's a one-liner.

---

## STEP 4 — CHECKOUT THE BRANCH

```bash
MR_BRANCH=$(glab mr view "$MR_NUMBER" --output json | \
    python3 -c "import sys,json; print(json.load(sys.stdin)['source_branch'])")

git checkout "$MR_BRANCH"
git pull origin "$MR_BRANCH"
echo "On branch: $MR_BRANCH"
```

---

## STEP 5 — APPLY THE FIXES

Read each file with comments before touching it.
For each MUST and SHOULD fix:
- Understand full context around the comment
- Make the minimal change that addresses it
- Apply code-economy skill — fix what was asked, nothing extra
- Add a brief inline comment if the fix is non-obvious:
  ```python
  # Fixed per MR review: use -113 (undefined header) not -114
  ```

For ambiguous comments: reply asking for clarification, skip the
fix, and note it in the summary.

---

## STEP 6 — VERIFY TESTS PASS

```bash
cat CLAUDE.md | grep -A5 "Test Commands"
# Run whatever the project's test command specifies
```

Do not commit if tests fail unless the failure pre-dates this MR.

---

## STEP 7 — COMMIT AND PUSH

```bash
# Stage only changed source files — no docs/learning, no reports
git add src/ tests/ lib/ app/ 2>/dev/null || true
git diff --cached --name-only  # verify before committing

git commit -m "review(!$MR_NUMBER): address review comments

- [what was fixed from must-fix]
- [what was fixed from should-fix]
- Skipped: [any could-fix items not addressed]"

git push origin "$MR_BRANCH"
# Push automatically updates the open MR — no recreating needed
```

---

## STEP 8 — REPLY TO COMMENTS AND RESOLVE THREADS

For each addressed discussion thread:
```bash
# Reply
glab api "projects/:fullpath/merge_requests/$MR_NUMBER/discussions/${DISC_ID}/notes" \
    --method POST \
    --field "body=Fixed in latest commit."

# Resolve the thread
glab api "projects/:fullpath/merge_requests/$MR_NUMBER/discussions/${DISC_ID}" \
    --method PUT \
    --field "resolved=true"
```

For skipped COULD items — reply but don't resolve:
```bash
glab api "projects/:fullpath/merge_requests/$MR_NUMBER/discussions/${DISC_ID}/notes" \
    --method POST \
    --field "body=Noted — left as-is to keep scope tight. Can address in a follow-up."
```

For ambiguous comments — reply asking for clarification:
```bash
glab api "projects/:fullpath/merge_requests/$MR_NUMBER/discussions/${DISC_ID}/notes" \
    --method POST \
    --field "body=Could you clarify what you had in mind here? Skipped until clear."
```

Add a summary comment to the MR:
```bash
glab mr comment "$MR_NUMBER" --body "Review comments addressed.

Fixed (N items):
- [comment summary]

Skipped — optional (N items):
- [comment summary]

Needs your clarification (N items):
- [comment summary]

Ready for another look."
```

---

## STEP 9 — FINAL OUTPUT

```
✅ MR !N updated: N comments addressed

Fixed:
  src/file.py:42 — [summary]

Skipped (optional):
  src/file.py:100 — [summary]

Needs your input:
  src/file.py:77 — [what was unclear]

Next: review the updated MR
  glab mr view !N --web
  or: glab mr diff N
```

---

## WHAT THIS COMMAND DOES NOT DO

- Does not merge the MR (you do that after final review)
- Does not address comments from other reviewers without your confirmation
- Does not make changes beyond what the comments asked for
- Does not touch docs/learning/ or OVERNIGHT_REPORT.md
- Does not resolve threads it didn't address
