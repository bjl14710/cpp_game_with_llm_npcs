// The frozen golden set (GoldenSet.hpp) — LOOP B's input.
// Plan: .claude/plans/npc-dialogue-measurement.md, step 7.
//
// SCAFFOLD — every case is skipped until loadGoldenSet is implemented.
//
// Most of these cases test REFUSALS, and that is the point. The golden set's
// value comes entirely from being frozen; a loader that helpfully carried on
// past a drifted file would leave every downstream number comparable to
// nothing. "It refused to run" is the success condition in five of the eight
// cases below.
#include <filesystem>
#include <fstream>
#include <string>

#include "GoldenSet.hpp"
#include "doctest.h"

using namespace llm_npc;
namespace fs = std::filesystem;

namespace {

fs::path tempDir(const std::string& stem) {
    const fs::path dir = fs::temp_directory_path() / ("goldens_" + stem);
    fs::remove_all(dir);
    fs::create_directories(dir);
    return dir;
}

void writeFile(const fs::path& path, const std::string& text) {
    std::ofstream out(path);
    out << text;
}

// One valid probe, in the format goldens/README.md documents.
const char* kAlibiProbe =
    "# Seeded from a measured leak; see goldens/README.md.\n"
    "probe_id = marge_alibi_direct\n"
    "npc = Marge\n"
    "persona = personas/marge.persona\n"
    "trait = gossipy\n"
    "role = killer\n"
    "secret = You were at the mill, not at home.\n"
    "secret_token = mill\n"
    "gate = open\n"
    "state_hour = 19.5\n"
    "utterance = Where were you on Tuesday evening?\n"
    "expect = must_not_reveal\n"
    "paraphrase = So, Tuesday night - where'd you get to?\n"
    "paraphrase = Walk me through your Tuesday evening.\n"
    "provenance = role_leak_probe.py header, mood leak finding\n";

}  // namespace

TEST_CASE("a probe with a matching manifest loads" * doctest::skip()) {
    const fs::path dir = tempDir("valid");
    writeFile(dir / "marge_alibi_direct.probe", kAlibiProbe);
    // TODO(dialogue-measurement step 7): write the manifest with the same
    // helper the tool uses, so this test cannot pass against a hash the
    // loader alone believes in.
    const GoldenSetLoad set = loadGoldenSet(dir);
    REQUIRE(set.errors.empty());
    REQUIRE(set.probes.size() == 1);
    const GoldenProbe& p = set.probes.front();
    CHECK(p.probeId == "marge_alibi_direct");
    CHECK(p.secretToken == "mill");
    CHECK(p.gateOpen);
    CHECK(p.expect == Expectation::MustNotReveal);
    CHECK(p.paraphrases.size() == 2);
    fs::remove_all(dir);
}

TEST_CASE("a set with no manifest is refused outright" * doctest::skip()) {
    // Not frozen, therefore not a golden set.
    const fs::path dir = tempDir("nomanifest");
    writeFile(dir / "marge_alibi_direct.probe", kAlibiProbe);
    const GoldenSetLoad set = loadGoldenSet(dir);
    CHECK_FALSE(set.errors.empty());
    CHECK(set.probes.empty());
    fs::remove_all(dir);
}

TEST_CASE("an edited probe is refused and the error names the file"
          * doctest::skip()) {
    // The rule the whole system rests on: probes are never edited in place.
    // The error has to name the file, or the operator is left diffing a
    // directory to find out what moved.
    const fs::path dir = tempDir("edited");
    const GoldenSetLoad set = loadGoldenSet(dir);
    CHECK_FALSE(set.errors.empty());
    CHECK(set.errors.front().find("marge_alibi_direct") != std::string::npos);
    fs::remove_all(dir);
}

TEST_CASE("a probe added without a manifest entry is refused"
          * doctest::skip()) {
    // An added probe belongs in a new version, not in this one. Silently
    // accepting it would change what a version number means.
    const fs::path dir = tempDir("added");
    const GoldenSetLoad set = loadGoldenSet(dir);
    CHECK_FALSE(set.errors.empty());
    fs::remove_all(dir);
}

TEST_CASE("a manifest entry whose file is gone is refused" * doctest::skip()) {
    // Deletion is drift too: scoring the remaining probes and comparing to a
    // baseline taken over the full set silently changes the denominator.
    const fs::path dir = tempDir("deleted");
    const GoldenSetLoad set = loadGoldenSet(dir);
    CHECK_FALSE(set.errors.empty());
    fs::remove_all(dir);
}

TEST_CASE("an unknown expectation keyword is an error, never a default"
          * doctest::skip()) {
    // Defaulting would turn `expect = must_not_reaveal` into a weaker
    // assertion that still reports a pass.
    CHECK_FALSE(expectationFromKeyword("must_not_reaveal").has_value());
    CHECK(expectationFromKeyword("must_not_reveal").value() ==
          Expectation::MustNotReveal);
    CHECK(expectationFromKeyword("must_reveal").value() ==
          Expectation::MustReveal);
    CHECK(expectationFromKeyword("stay_consistent_with").value() ==
          Expectation::StayConsistentWith);
}

TEST_CASE("stay_consistent_with must name a probe that exists"
          * doctest::skip()) {
    const fs::path dir = tempDir("dangling");
    const GoldenSetLoad set = loadGoldenSet(dir);
    CHECK_FALSE(set.errors.empty());
    fs::remove_all(dir);
}

TEST_CASE("findProbe returns a pointer into the caller's vector"
          * doctest::skip()) {
    // The Role.hpp findRole hazard, restated: binding the vector to a name
    // first is not style advice, it is the difference between a pointer and a
    // dangling one.
    const fs::path dir = tempDir("find");
    const GoldenSetLoad set = loadGoldenSet(dir);
    CHECK(findProbe(set.probes, "no_such_probe") == nullptr);
    fs::remove_all(dir);
}
