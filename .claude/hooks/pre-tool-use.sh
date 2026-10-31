#!/bin/bash
# Safety hook: blocks dangerous commands before they execute.

if echo "$TOOL_ARGS" | grep -qE "rm -rf /|rm -rf ~|rm -rf \*"; then
    echo "BLOCKED: Dangerous rm -rf pattern detected"
    exit 1
fi

if echo "$TOOL_ARGS" | grep -qE "git push.*--force|git reset --hard origin"; then
    echo "BLOCKED: Destructive git operation requires manual approval"
    exit 1
fi

if echo "$TOOL_ARGS" | grep -qE "\.env|secrets/|credentials"; then
    echo "BLOCKED: Sensitive file access requires manual approval"
    exit 1
fi

exit 0
