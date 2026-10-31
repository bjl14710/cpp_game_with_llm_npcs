#pragma once

#include <cstdint>
#include <string>

namespace llm_npc {

// SCAFFOLD — the constant is real, the fingerprint body is TODO.
// Plan: .claude/plans/npc-dialogue-measurement.md, step 3.
//
// Provenance for a non-deterministic output. Two fields, because "which prompt
// produced this reply?" is two questions:
//
//   kPromptTemplateVersion  WHICH COMPOSITION SHAPE. Hand-bumped when the
//                           assembly in Persona::renderSystemPrompt changes —
//                           a new section, a reordering, different fixed
//                           wording. Coarse and stable, so a telemetry query
//                           can say "every turn under v3 regressed".
//
//   promptFingerprint()     WAS THIS LITERALLY THE SAME PROMPT. A hash of the
//                           fully rendered string, so it also moves when a
//                           traits/*.trait or personas/*.persona file is
//                           edited, or when memory and gossip grow. Fine and
//                           unstable, and free.
//
// One without the other is a trap. The constant alone cannot tell you someone
// reworded a trait file; the hash alone changes on every turn (memory grows)
// and so can never group turns.
//
// BUMPING THE CONSTANT IS PART OF THE CHANGE, not follow-up work. A prompt
// edit that leaves this alone silently merges two populations in every
// baseline comparison afterwards.
constexpr const char* kPromptTemplateVersion = "v1";

// FNV-1a 64-bit over the rendered system prompt, lowercase hex, 16 chars.
//
// FNV-1a and not std::hash: std::hash's value is implementation-defined and
// may differ between libstdc++ and libc++, which would make a Linux
// container's telemetry incomparable with a macOS run's. It is also allowed to
// vary between runs. A fingerprint that is not stable across builds is not a
// fingerprint. No new dependency (CLAUDE.md) — this is twelve lines.
//
// Not cryptographic and does not need to be: this detects accidental drift,
// not tampering.
//
// TODO(dialogue-measurement step 3): implement as the standard 64-bit FNV-1a
// (offset basis 14695981039346656037u, prime 1099511628211u) over the bytes of
// `renderedPrompt`, formatted with std::to_chars in hex, zero-padded to 16.
// Integer to_chars is available at the macOS 11.0 floor; the FLOATING-POINT
// overload is not, and neither is the <format> header's formatting facility —
// both are Apple-gated at 13.3. See CLAUDE.md.
//
// (That last sentence is worded around the literal API name on purpose:
// scripts/check-portability-floor.sh greps src/ and tools/ for it and cannot
// tell a comment from a call. Blunt is the right design for that check — a
// warning that something is forbidden should not be able to look like a use of
// it — so the comment moves, not the script.)
//
// Header-only (inline) like NpcAction.hpp and AsciiText.hpp: it is a pure
// function over a string with no state, and keeping it out of the .cpp list
// means it cannot go missing from CMakeLists.txt while the globbing test
// build stays green — the dual-build trap CLAUDE.md documents.
inline std::string promptFingerprint(const std::string& renderedPrompt) {
    (void)renderedPrompt;
    return "";  // TODO(dialogue-measurement step 3): the FNV-1a above.
}

}  // namespace llm_npc
