#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace llm_npc {

// SCAFFOLD — format, freeze rule and interface are settled here; bodies TODO.
// Plan: .claude/plans/npc-dialogue-measurement.md, step 7. This is LOOP B's
// input.
//
// THE GOLDEN SET IS A CALIBRATION STANDARD, NOT A TEST FIXTURE.
//
// That distinction is the single load-bearing rule of the whole measurement
// system, so it is written here rather than only in the plan:
//
//   * A probe is FROZEN the moment it enters the set. Not "should not be
//     changed" — cannot be, because loadGoldenSet refuses to run when a file's
//     hash does not match the committed MANIFEST.sha256.
//   * Probes are NEVER training or few-shot data. Nothing in goldens/ may be
//     copied into personas/*.persona, traits/*.trait or banks/*.bank. The
//     enforcement lives in tools/curate_candidates.py, which refuses any
//     candidate overlapping a probe.
//   * Adding probes creates a NEW VERSION (goldens/golden-v2/) by copying the
//     directory. It does not edit v1.
//
// The reason is arithmetic, not hygiene. A measurement taken with an
// instrument that was itself built from the thing being measured is not a
// measurement of anything. If a probe's utterance also appears as a few-shot
// example in the prompt, a perfect score means the model can copy, and every
// historical comparison against that number is void — including the ones
// already made before anyone noticed.
//
// FORMAT. goldens/golden-vN/<probe_id>.probe, `key = value` with `#`
// comments, the same shape as personas/*.persona, traits/*.trait,
// banks/*.bank, storylines/*.storyline and cutscenes/*.cutscene — for the
// reason banks/README.md gives: generated or hand-authored content has to
// review as a normal text diff in a PR. Repeated keys (`trait`, `gossip_fact`,
// `paraphrase`) accumulate. See goldens/README.md.

// What a probe asserts about the reply.
enum class Expectation {
    // The secret must NOT appear. The default and the important one: this is
    // the assertion the gated-secret architecture lets us make deterministically,
    // with no judge model anywhere near it.
    MustNotReveal,
    // The secret MUST appear. The symmetric check, and not optional — a model
    // that never reveals anything under any pressure is not passing the
    // secrecy test, it is broken in a way MustNotReveal alone cannot see.
    MustReveal,
    // The answer must not contradict this NPC's answer to another probe.
    // `consistentWith` names that probe.
    StayConsistentWith,
};

// One frozen probe.
struct GoldenProbe {
    std::string probeId;      // matches the filename stem
    std::string npcId;        // persona name
    std::string personaFile;  // personas/*.persona
    std::vector<std::string> traitIds;

    // --- the scripted game state -----------------------------------------
    // Everything needed to reproduce the exact system prompt. Rendered by
    // `persona_prompt --state`, never by re-implementing prompt assembly in
    // Python — see tools/probe_lib.py.
    std::string roleId;       // roles/*.role id; empty for an uncast resident
    std::string secret;       // the per-match specific handed to renderRoleBlock
    // The discriminating substring the assertion tests for. NOT the whole
    // secret: "You were at the mill, not at home" leaks the moment "mill"
    // appears, and matching the full sentence would miss every paraphrase of
    // it. Chosen by the probe author to be a word the reply cannot innocently
    // contain.
    std::string secretToken;
    bool gateOpen = false;    // does the secret reach the prompt at all?
    double stateHour = 12.0;  // world clock, for prompts that mention time
    std::string memory;       // persisted first-person summary, may be empty
    // "subject|content" pairs seeded into the gossip block.
    std::vector<std::string> gossipFacts;

    // --- the question and the assertion -----------------------------------
    std::string utterance;
    Expectation expect = Expectation::MustNotReveal;
    std::string consistentWith;  // probe id; only for StayConsistentWith

    // Alternative phrasings of the same question for the paraphrase-invariance
    // metric. Five or more, or the metric reports its own n as too small.
    std::vector<std::string> paraphrases;

    // Where this probe came from — a rated turn, a known failure, a measured
    // leak. Authoring aid; never sent to a model.
    std::string provenance;
};

// A loaded, verified version of the set.
struct GoldenSetLoad {
    std::string version;  // "golden-v1", from the directory name
    std::vector<GoldenProbe> probes;
    // Named, human-readable failures. NON-EMPTY MEANS DO NOT RUN. Unlike the
    // other loaders in this codebase, which skip a bad file and carry on, a
    // golden set that partly loaded is worse than one that did not: the run
    // would silently score against a smaller set and compare the number to a
    // baseline taken over the full one.
    std::vector<std::string> errors;
};

// Loads and FREEZE-CHECKS goldens/golden-vN/.
//
// Verification order matters and is part of the contract:
//   1. MANIFEST.sha256 must exist. A set without one is not frozen and is
//      refused outright.
//   2. Every *.probe file's hash must match its manifest entry.
//   3. Every manifest entry must have a file (a deleted probe is drift too).
//   4. No *.probe file may be absent from the manifest (an added probe
//      belongs in a new version, not this one).
// Only then is anything parsed.
//
// TODO(dialogue-measurement step 7): implement. SHA-256 is NOT vendored — the
// only crypto-shaped code here is Gossip's factIdFor. Do NOT add a dependency
// and do NOT hand-roll SHA-256 into this repo. Shell out to `shasum -a 256`
// (macOS) / `sha256sum` (Linux) from tools/probe_lib.py and have THAT write
// and verify the manifest, keeping this loader's job to parsing plus checking
// a hash string the tool already computed. If that split proves awkward, the
// defensible fallback is to name the file MANIFEST.fnv and use
// promptFingerprint() — weaker, but honest, dependency-free and still
// detects every accidental edit. Decide it in the issue, record which in
// goldens/README.md, and do not leave both.
GoldenSetLoad loadGoldenSet(const std::filesystem::path& dir);

// Probe by id within a loaded set; nullptr when unknown.
//
// RETURNS A POINTER INTO `probes`, so that vector must outlive the result —
// the same hazard Role.hpp's findRole documents, and for the same reason it
// failed a test the first time it ran.
//
// TODO(dialogue-measurement step 7): implement.
const GoldenProbe* findProbe(const std::vector<GoldenProbe>& probes,
                             const std::string& probeId);

// Parses one expectation keyword: "must_not_reveal", "must_reveal",
// "stay_consistent_with". Nullopt for anything else — an unknown expectation
// is an error, never a default, because defaulting would turn a typo into a
// silently weaker assertion.
//
// TODO(dialogue-measurement step 7): implement.
std::optional<Expectation> expectationFromKeyword(const std::string& keyword);

}  // namespace llm_npc
