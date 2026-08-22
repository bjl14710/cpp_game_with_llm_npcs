#!/usr/bin/env bash
# Contract checks for `persona_prompt --state` (issue #304).
#
# WHY A SCRIPT AND NOT A doctest CASE. tests/Makefile globs src/core/*.cpp and
# nothing else, so tools/persona_prompt.cpp is not in the test binary at all --
# the same harness boundary that leaves src/app/ uncovered. The core contract
# (renderRoleBlock(nullptr, "") renders byte-identically) IS pinned in
# tests/test_role.cpp:294. What this asserts is the part only the tool can get
# wrong: that --state passes the right arguments to that already-tested code,
# and that a bad state file produces no prompt at all.
#
# Run from the repo root, after: cmake --build build --target persona_prompt
#
#   ./scripts/check-persona-prompt-state.sh
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOL="$ROOT/build/persona_prompt"
PERSONA="personas/baker.persona"
status=0
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

if [ ! -x "$TOOL" ]; then
    echo "FAIL: $TOOL not built — run: cmake --build build --target persona_prompt"
    exit 1
fi
cd "$ROOT"

pass() { echo "  ok   $1"; }
fail() { echo "FAIL: $1"; status=1; }

# 1. An uncast persona renders byte-identically to the plain mode. This is the
#    role layer's own contract (Role.hpp) restated at the tool boundary: if
#    --state drifts, every "before/with role" comparison a probe makes is
#    measuring the tool rather than the role.
printf '{"persona": "%s", "traits": []}' "$PERSONA" > "$tmp/norole.json"
"$TOOL" "$PERSONA" "" > "$tmp/plain.txt" 2>/dev/null
"$TOOL" --state "$tmp/norole.json" > "$tmp/state.txt" 2>/dev/null
if cmp -s "$tmp/plain.txt" "$tmp/state.txt"; then
    pass "an uncast persona renders byte-identically to the plain mode"
else
    fail "--state drifted from the plain mode for a persona with no role"
    diff "$tmp/plain.txt" "$tmp/state.txt" | head -10
fi

# 2. A cast persona gets the role block, and it lands in the ONE position
#    Role.hpp calls "the whole design": after "Stay in character", before
#    "ACTIONS:". Earlier placement was measured to corrupt the action protocol.
printf '{"persona": "%s", "traits": [], "role": "killer", "secret": "You were at the mill."}' \
    "$PERSONA" > "$tmp/killer.json"
"$TOOL" --state "$tmp/killer.json" > "$tmp/killer.txt" 2>/dev/null
stay=$(grep -n "Stay in character" "$tmp/killer.txt" | head -1 | cut -d: -f1)
role=$(grep -n "In this story you are" "$tmp/killer.txt" | head -1 | cut -d: -f1)
acts=$(grep -n "^ACTIONS:" "$tmp/killer.txt" | head -1 | cut -d: -f1)
if [ -n "$stay" ] && [ -n "$role" ] && [ -n "$acts" ] &&
   [ "$stay" -lt "$role" ] && [ "$role" -lt "$acts" ]; then
    pass "the role block sits after 'Stay in character' and before ACTIONS:"
else
    fail "role block misplaced (stay=$stay role=$role actions=$acts)"
fi

if grep -q "You were at the mill." "$tmp/killer.txt"; then
    pass "the secret reaches the prompt"
else
    fail "the secret is missing from a cast prompt"
fi

# 3. Every failure prints NO prompt. A partially rendered prompt would read as
#    a measurement rather than as a broken fixture, which is the one outcome a
#    probe must never be handed.
check_refuses() {
    local name="$1" json="$2"
    printf '%s' "$json" > "$tmp/bad.json"
    local out; out="$("$TOOL" --state "$tmp/bad.json" 2>/dev/null)"
    local code=$?
    if [ "$code" -ne 0 ] && [ -z "$out" ]; then
        pass "$name refuses with no prompt"
    else
        fail "$name exited $code and printed ${#out} bytes"
    fi
}
check_refuses "an unknown role id" \
    "{\"persona\": \"$PERSONA\", \"role\": \"not_a_role\"}"
check_refuses "an unknown trait id" \
    "{\"persona\": \"$PERSONA\", \"traits\": [\"not_a_trait\"]}"
check_refuses "a missing persona key" '{"traits": []}'
check_refuses "malformed JSON" 'not json at all'

out="$("$TOOL" --state "$tmp/does-not-exist.json" 2>/dev/null)"; code=$?
if [ "$code" -ne 0 ] && [ -z "$out" ]; then
    pass "a missing state file refuses with no prompt"
else
    fail "a missing state file exited $code and printed ${#out} bytes"
fi

if [ "$status" -eq 0 ]; then
    echo "persona_prompt --state: all contract checks pass"
else
    echo "persona_prompt --state: FAILED"
fi
exit $status
