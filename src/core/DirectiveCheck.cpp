#include "DirectiveCheck.hpp"

#include "json.hpp"

namespace llm_npc {

DirectiveVerdict checkReply(const std::string& raw) {
    // TODO(dialogue-measurement step 2): implement per the header's five
    // steps. Returning an invalid verdict with a named reason is the correct
    // scaffold behaviour — a stub that reported `valid = true` would make the
    // schema-validity rate read 100% before the check exists, which is worse
    // than no measurement at all.
    DirectiveVerdict out;
    out.spokenText = raw;
    out.reasons.push_back("not_implemented");
    return out;
}

std::string verdictToJson(const DirectiveVerdict& verdict) {
    // TODO(dialogue-measurement step 2): emit the documented object once the
    // enum name tables move here from tools/persona_prompt.cpp.
    nlohmann::json out;
    out["valid"] = verdict.valid;
    out["reasons"] = verdict.reasons;
    return out.dump();
}

}  // namespace llm_npc
