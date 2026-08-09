// Measures what the line banks would actually serve, using the REAL LineBank
// and TopicMatcher rather than a Python reimplementation — the same reason
// persona_prompt exists on the prompt side of the exchange.
//
// tools/eval_lines.py scores the REPLIES (are they in character, do they parse,
// do they leak). Nothing scored the TRIGGERS, and trigger breadth is the whole
// tuning knob for a lexical matcher: a topic with two phrasings barely hits, one
// with ten hits constantly. A bank can pass all five gates and still never fire.
// This is the missing half — it answers "would this bank serve a real player?"
//
// Probe lines live in a file so the set grows as real misses are found. They are
// deliberately NOT copied from the trigger lists; a probe that repeats a trigger
// verbatim measures nothing.
//
// Usage:
//   bank_probe <banks-dir> [--probes FILE] [--threshold F] [--json]
//
// Exit codes: 0 all probes hit, 1 one or more missed, 2 a bank failed to load.
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "LineBank.hpp"

namespace {

struct Probe {
    std::string speaker;
    std::string line;
};

// Trims ASCII whitespace from both ends; the probe file is hand-edited, so
// stray spaces around the '|' separator are expected rather than exceptional.
std::string trimEnds(const std::string& s) {
    const std::size_t first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const std::size_t last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

// Parses "Speaker Name | player line" per line. Blank lines and '#' comments
// are ignored, matching the .persona/.bank/.trait convention. A speaker name
// may contain quotes and spaces (Benny "Strings" Malone), so only the FIRST
// '|' separates.
std::vector<Probe> loadProbes(const std::string& path, std::string* error) {
    std::vector<Probe> probes;
    std::ifstream in(path);
    if (!in) {
        *error = "cannot read probe file: " + path;
        return probes;
    }
    std::string raw;
    while (std::getline(in, raw)) {
        const std::string line = trimEnds(raw);
        if (line.empty() || line[0] == '#') continue;
        const std::size_t bar = line.find('|');
        if (bar == std::string::npos) {
            *error = "probe line missing '|': " + line;
            return {};
        }
        probes.push_back({trimEnds(line.substr(0, bar)),
                          trimEnds(line.substr(bar + 1))});
    }
    return probes;
}

// JSON string escaping for the report; probe lines contain quotes and
// apostrophes, and a malformed report is worse than none.
std::string jsonEscape(const std::string& s) {
    std::string out;
    for (const char c : s) {
        if (c == '"' || c == '\\') { out.push_back('\\'); out.push_back(c); }
        else if (c == '\n') { out += "\\n"; }
        else { out.push_back(c); }
    }
    return out;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: bank_probe <banks-dir> [--probes FILE] "
                     "[--threshold F] [--json]\n";
        return 2;
    }
    const std::string dir = argv[1];
    std::string probePath = "bench/probes.txt";
    float threshold = 0.62f;  // config/llm.cfg's line_bank_threshold default
    bool asJson = false;

    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--json") asJson = true;
        else if (arg == "--probes" && i + 1 < argc) probePath = argv[++i];
        else if (arg == "--threshold" && i + 1 < argc) threshold = std::stof(argv[++i]);
        else { std::cerr << "unknown argument: " << arg << "\n"; return 2; }
    }

    std::string error;
    const std::vector<Probe> probes = loadProbes(probePath, &error);
    if (!error.empty()) {
        std::cerr << "error: " << error << "\n";
        return 2;
    }

    llm_npc::LineBank bank(dir, threshold);
    if (!bank.errors().empty()) {
        for (const std::string& e : bank.errors()) std::cerr << "error: " << e << "\n";
        return 2;
    }
    // LineBank degrades a missing directory to "never hits" on purpose, so the
    // game survives a bad install. A measurement run must not inherit that:
    // zero banks loaded would otherwise report as "every probe missed", which
    // reads like a content problem instead of a broken checkout.
    if (!bank.ok()) {
        std::cerr << "error: no banks loaded from " << dir << "\n";
        return 2;
    }

    std::size_t topics = 0, replies = 0;
    for (const llm_npc::PersonaBank& b : bank.banks()) {
        topics += b.topics.size();
        for (const llm_npc::BankedTopic& t : b.topics) replies += t.replies.size();
    }

    // Every probe is asked as a First-meeting player, the harder case: a
    // familiarity=returning topic must not answer someone it has never met.
    std::size_t hits = 0;
    std::vector<Probe> misses;
    for (const Probe& p : probes) {
        if (bank.lookup(p.speaker, llm_npc::Familiarity::First, p.line)) ++hits;
        else misses.push_back(p);
    }

    // Rotation and familiarity are contracts LineBank owns, re-checked here
    // against real content because unit tests pin them against fixtures.
    bank.resetSession();
    std::size_t rotationDistinct = 0;
    bool familiarityDiffers = false;
    if (!bank.banks().empty() && !bank.banks().front().topics.empty()) {
        const std::string who = bank.banks().front().speakerId;
        const std::string ask = bank.banks().front().topics.front().triggers.empty()
                                    ? "hello"
                                    : bank.banks().front().topics.front().triggers.front();
        std::set<std::string> seen;
        for (int i = 0; i < 3; ++i) {
            if (auto r = bank.lookup(who, llm_npc::Familiarity::First, ask)) seen.insert(*r);
        }
        rotationDistinct = seen.size();

        bank.resetSession();
        const auto first = bank.lookup(who, llm_npc::Familiarity::First, ask);
        const auto ret = bank.lookup(who, llm_npc::Familiarity::Returning, ask);
        familiarityDiffers = first && ret && *first != *ret;
    }

    if (asJson) {
        std::cout << "{\n  \"banks\": " << bank.banks().size()
                  << ",\n  \"topics\": " << topics
                  << ",\n  \"replies\": " << replies
                  << ",\n  \"probes\": " << probes.size()
                  << ",\n  \"hits\": " << hits
                  << ",\n  \"rotation_distinct\": " << rotationDistinct
                  << ",\n  \"familiarity_differs\": "
                  << (familiarityDiffers ? "true" : "false")
                  << ",\n  \"misses\": [";
        for (std::size_t i = 0; i < misses.size(); ++i) {
            std::cout << (i ? ",\n    " : "\n    ") << "{\"speaker\": \""
                      << jsonEscape(misses[i].speaker) << "\", \"line\": \""
                      << jsonEscape(misses[i].line) << "\"}";
        }
        std::cout << (misses.empty() ? "" : "\n  ") << "]\n}\n";
    } else {
        std::cout << "banks " << bank.banks().size() << ", topics " << topics
                  << ", replies " << replies << "\n";
        std::cout << "probe hit rate: " << hits << "/" << probes.size() << "\n";
        for (const Probe& m : misses) {
            std::cout << "  MISS " << m.speaker << " <- \"" << m.line << "\"\n";
        }
        std::cout << "rotation: 3 identical asks -> " << rotationDistinct
                  << " distinct replies\n";
        std::cout << "familiarity first != returning: "
                  << (familiarityDiffers ? "yes" : "NO") << "\n";
    }
    return hits == probes.size() ? 0 : 1;
}
