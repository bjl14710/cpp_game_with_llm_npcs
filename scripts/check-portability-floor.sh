#!/usr/bin/env bash
#
# Assert that the macOS deployment floor actually bites.
#
# Plan: .claude/plans/cpp20-upgrade.md.
#
# WHY THIS EXISTS. The project moved to C++20 but deliberately does NOT adopt
# std::format, std::format_to, or floating-point std::to_chars. Those three are
# gated behind Apple availability annotations and are a compile error below
# macOS 13.3 — so using them would silently drop support for macOS 11 and 12,
# and nobody would find out on a machine running 14.x.
#
# CMakeLists.txt sets CMAKE_OSX_DEPLOYMENT_TARGET to make that a build error
# instead of a silent regression. But a deployment target that is set and not
# honoured is WORSE than none, because it reads as protection. This script is
# the thing that proves it is real.
#
# It is also the caller for a decision that would otherwise live only in a
# comment. Run it with:
#
#     make -C tests portability
#
# THE TRAP THIS GUARDS AGAINST. __cpp_lib_format is 0 on Apple's libc++ even
# when std::format works perfectly, because the availability annotations
# suppress the feature-test macro. Anyone reaching for the conventional
#
#     #if defined(__cpp_lib_format)
#
# guard would disable the feature permanently on every macOS and never notice.
# Guard on the deployment target, which is what this script verifies.
#
# Exits 0 if the floor holds, 1 if it does not. Skips with 0 off Darwin, where
# none of these gates exist.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CXX_BIN="${CXX:-c++}"

if [ "$(uname -s)" != "Darwin" ]; then
    echo "portability: not Darwin — Apple availability gates do not apply, skipping"
    exit 0
fi

# Single source of truth: read the floor out of CMakeLists.txt rather than
# repeating it, so the two cannot drift apart without this failing loudly.
FLOOR="$(sed -n 's/.*set(CMAKE_OSX_DEPLOYMENT_TARGET "\([0-9.]*\)".*/\1/p' \
    "$REPO_ROOT/CMakeLists.txt" | head -1)"

if [ -z "$FLOOR" ]; then
    echo "FAIL: no CMAKE_OSX_DEPLOYMENT_TARGET found in CMakeLists.txt."
    echo "      The portability decision is unenforced — see .claude/plans/cpp20-upgrade.md"
    exit 1
fi

echo "portability: floor is macOS $FLOOR (read from CMakeLists.txt)"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

fails_to_compile() {
    # $1 = human label, $2 = source text. Passes when compilation FAILS.
    printf '%s\n' "$2" > "$WORK/probe.cpp"
    if "$CXX_BIN" -std=c++20 -mmacosx-version-min="$FLOOR" \
        -c "$WORK/probe.cpp" -o "$WORK/probe.o" 2>"$WORK/err.txt"; then
        echo "FAIL: $1 compiles at macOS $FLOOR but must not."
        echo "      Either the floor was raised past 13.3, or the SDK changed."
        echo "      If raising the floor was deliberate, update this script and the"
        echo "      rationale in CMakeLists.txt together."
        return 1
    fi
    echo "  ok   $1 is correctly unavailable"
    return 0
}

compiles() {
    printf '%s\n' "$2" > "$WORK/probe.cpp"
    if ! "$CXX_BIN" -std=c++20 -mmacosx-version-min="$FLOOR" \
        -c "$WORK/probe.cpp" -o "$WORK/probe.o" 2>"$WORK/err.txt"; then
        echo "FAIL: $1 does NOT compile at macOS $FLOOR, but the project relies on it."
        sed -n '1,5p' "$WORK/err.txt"
        return 1
    fi
    echo "  ok   $1 is available"
    return 0
}

status=0

# --- 1. the gated features must stay out of reach -------------------------
fails_to_compile "std::format" \
'#include <format>
#include <string>
int main() { std::string s = std::format("{}", 1); return (int)s.size(); }' || status=1

fails_to_compile "float std::to_chars" \
'#include <charconv>
int main() { char b[32]; auto r = std::to_chars(b, b + 32, 1.5); return r.ec == std::errc{}; }' || status=1

# --- 2. everything the project actually uses must stay reachable ----------
# If the floor is ever raised or lowered carelessly, this half catches the
# damage in the other direction.
compiles "the adopted C++20 surface" \
'#include <algorithm>
#include <charconv>
#include <map>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
struct P { int a; float b; };
int main() {
    std::string s = "npc.storyline.beat";
    std::string_view v = s;
    std::map<std::string, int, std::less<>> hm{{"k", 1}};
    std::unordered_map<std::string, int> um{{"a", 1}};
    std::vector<int> vec{3, 1, 2};
    std::span<int> sp{vec};
    std::ranges::sort(vec);
    char buf[32];
    auto r = std::to_chars(buf, buf + sizeof buf, 42);   // INTEGER to_chars is fine
    P p{.a = 1, .b = 2.f};
    return s.starts_with("npc.") + s.ends_with("beat") + v.starts_with("npc.")
         + hm.contains("k") + um.contains("a") + (int)sp.size() + (int)std::ssize(vec)
         + p.a + (r.ec == std::errc{});
}' || status=1

# --- 3. nothing in the tree may have started using the gated features -----
# The compile checks above only prove the gate exists. This proves nobody has
# routed around it by raising the floor locally or including <format> anyway.
if grep -rn 'std::format\|std::format_to' "$REPO_ROOT/src" "$REPO_ROOT/tools" 2>/dev/null; then
    echo "FAIL: std::format is in use above. It is 1.7-2.3x slower than the"
    echo "      to_string concatenation it replaces on Apple's libc++, and it"
    echo "      raises the macOS floor to 13.3. See .claude/plans/cpp20-upgrade.md."
    status=1
else
    echo "  ok   no std::format usage in src/ or tools/"
fi

if [ "$status" -eq 0 ]; then
    echo "portability: floor holds at macOS $FLOOR"
else
    echo "portability: FAILED"
fi
exit "$status"
