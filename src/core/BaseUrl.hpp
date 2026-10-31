#pragma once

#include <string>

namespace llm_npc {

// A cloud base URL split the way httplib::Client wants it: the scheme+host[:port]
// origin, and the leading path prefix to put in front of each endpoint path.
// Shared by every backend that speaks to an https provider.
struct ParsedUrl {
    std::string origin;      // "https://openrouter.ai"
    std::string pathPrefix;  // "/api/v1" (no trailing slash; may be empty)
    bool https = true;
};

// Splits "https://openrouter.ai/api/v1" into {origin, "/api/v1", true}.
// Defaults to https when no scheme is present.
inline ParsedUrl parseBaseUrl(const std::string& url) {
    ParsedUrl out;
    std::string rest = url;
    if (rest.rfind("http://", 0) == 0) {
        out.https = false;
        rest = rest.substr(7);
    } else if (rest.rfind("https://", 0) == 0) {
        out.https = true;
        rest = rest.substr(8);
    }
    const auto slash = rest.find('/');
    const std::string hostPort = slash == std::string::npos ? rest : rest.substr(0, slash);
    out.pathPrefix = slash == std::string::npos ? "" : rest.substr(slash);
    while (!out.pathPrefix.empty() && out.pathPrefix.back() == '/') out.pathPrefix.pop_back();
    out.origin = (out.https ? "https://" : "http://") + hostPort;
    return out;
}

}  // namespace llm_npc
