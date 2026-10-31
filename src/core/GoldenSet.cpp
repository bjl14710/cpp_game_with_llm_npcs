#include "GoldenSet.hpp"

namespace llm_npc {

GoldenSetLoad loadGoldenSet(const std::filesystem::path& dir) {
    // TODO(dialogue-measurement step 7): implement the four-step freeze check
    // in the header, THEN parse. A scaffold that returned probes without
    // checking the manifest would let the first run establish a baseline
    // against an unfrozen set, which is the exact failure the freeze exists to
    // prevent — so this refuses instead.
    GoldenSetLoad out;
    out.version = dir.filename().string();
    out.errors.push_back("golden set loader is a scaffold — refusing to run");
    return out;
}

const GoldenProbe* findProbe(const std::vector<GoldenProbe>& probes,
                             const std::string& probeId) {
    // TODO(dialogue-measurement step 7): linear scan; the set is tens of
    // probes, not thousands.
    (void)probes;
    (void)probeId;
    return nullptr;
}

std::optional<Expectation> expectationFromKeyword(const std::string& keyword) {
    // TODO(dialogue-measurement step 7): the three keywords, nullopt
    // otherwise.
    (void)keyword;
    return std::nullopt;
}

}  // namespace llm_npc
